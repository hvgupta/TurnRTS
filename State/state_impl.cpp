#include "state.hpp"

#include <set>
#include <algorithm>
#include <unordered_set>

void State::push_step_back_request(std::queue<MoveRequest> &mrq,
                                   const uint64_t &unit_id,
                                   const Coord &to) const {
  const Unit &cur_unit = unit_map.at(unit_id);
  std::optional<Coord> cur_pos = cur_unit.get_coord();
  if (!cur_pos.has_value()) {
    return;
  }
  Coord new_pos = one_step_back(cur_pos.value(), to);
  mrq.push(cur_unit.build_movement_request(new_pos).value());
}

void State::add_unit(Unit &&unit) {
  if (unit.isDestroyed()) {
    return;
  }
  coord_inverse_map[unit.get_coord().value()] = unit.get_id();
  unit_map.insert({unit.get_id(), std::move(unit)});
}

void State::add_unit(uint16_t range, uint8_t speed, Coord cur_pos) {
  add_unit(Unit(range, speed, cur_pos));
}

std::vector<MoveRequest>
State::simple_conflict_resolve(std::vector<MoveRequest> mrl) const {
  std::set<uint64_t> moving_units;
  std::queue<MoveRequest> mrq;
  for (const MoveRequest &mr : mrl) {
    moving_units.insert(mr.unit_id);
    mrq.push(mr);
  }
  return simple_conflict_resolve(mrq, moving_units);
}

std::vector<MoveRequest>
State::simple_conflict_resolve(std::queue<MoveRequest> initial_mrq,
                               std::set<uint64_t> initial_moving_units) const {
  // Tracks the finalized, stabilized cell layout across generations
  std::unordered_map<Coord, std::vector<MoveRequest>> stabilized_tracker;

  std::queue<MoveRequest> mrq = initial_mrq;
  std::set<uint64_t> moving_units = initial_moving_units;

  bool state_changed = true;

  // MACRO LOOP: Resolves cascading fallbacks and new dynamic conflicts
  // generationally
  while (state_changed) {
    state_changed = false;

    // Clear intermediate tracking layers for this pass
    std::unordered_map<Coord, std::vector<MoveRequest>> coord_tracker;
    std::unordered_map<uint64_t, uint64_t> dependency_graph;
    std::unordered_map<uint64_t, MoveRequest> cached_requests;

    // Collects fallback requests to process in the NEXT generation
    std::queue<MoveRequest> next_pass_mrq;

    run_one_generation(mrq, moving_units, cached_requests, coord_tracker,
                       dependency_graph, next_pass_mrq, state_changed);

    // Pass down the generated fallback queue for the next generational loop
    // pass
    mrq = std::move(next_pass_mrq);

    // Stash stabilized grid results from this pass
    stabilized_tracker = std::move(coord_tracker);
  }

  // ====================================================================
  // PHASE 4: FLATTEN STABILIZED POSITION DATA
  // ====================================================================
  std::vector<MoveRequest> ans;
  for (const auto &c : stabilized_tracker) {
    ans.insert(ans.end(), c.second.begin(), c.second.end());
  }

  return ans;
}

void State::run_one_generation(
    std::queue<MoveRequest> &mrq, std::set<uint64_t> &moving_units,
    std::unordered_map<uint64_t, MoveRequest> &cached_requests,
    std::unordered_map<Coord, std::vector<MoveRequest>> &coord_tracker,
    std::unordered_map<uint64_t, uint64_t> &dependency_graph,
    std::queue<MoveRequest> &next_pass_mrq, bool &state_changed) const {
  // ====================================================================
  // PHASE 1: ARBITRATE SPOT CLASHES & MAP INTENDED MOVEMENT GRAPH
  // ====================================================================
  while (!mrq.empty()) {
    const MoveRequest mr = mrq.front();
    mrq.pop();

    // If the target tile contains an entity in the original grid state
    if (coord_inverse_map.find(mr.to) != coord_inverse_map.end()) {
      const uint64_t &unit_id = coord_inverse_map.at(mr.to);

      // Stationary check: Heading to its own current starting location
      if (unit_id == mr.unit_id) {
        moving_units.erase(unit_id);
        cached_requests.erase(unit_id);
        coord_tracker[mr.to].push_back(mr);
        continue;
      }

      // Dependency check: The blocking unit is actively trying to move
      if (moving_units.find(unit_id) != moving_units.end()) {
        dependency_graph[mr.unit_id] = unit_id;
        cached_requests[mr.unit_id] = mr;
        continue;
      }

      // Hard block check: The blocking unit is permanently stationary
      push_step_back_request(next_pass_mrq, mr.unit_id, mr.to);
      state_changed = true;
      continue;
    }

    // Standard spatial-temporal prioritization for unblocked spaces
    if (coord_tracker.find(mr.to) == coord_tracker.end() ||
        coord_tracker[mr.to][0].time == mr.time) {
      coord_tracker[mr.to].push_back(mr);
    } else if (coord_tracker[mr.to][0].time > mr.time) {
      std::cout << mr.unit_id
                << " removes the following due to early arrival\n";
      int n = coord_tracker[mr.to].size();
      for (int i = 0; i < n; i++) {
        const MoveRequest &cur_req = coord_tracker[mr.to][i];
        std::cout << "\t " << cur_req.unit_id << "\n";
        push_step_back_request(next_pass_mrq, cur_req.unit_id, cur_req.to);
      }
      coord_tracker[mr.to] = {mr};
      state_changed = true;
    } else {
      std::cout << "going one step back for late unit " << mr.unit_id << "\n";
      push_step_back_request(next_pass_mrq, mr.unit_id, mr.to);
      state_changed = true;
    }
  }

  // ====================================================================
  // PHASE 2: TOPOLOGICAL TRAVERSAL (The Sequential Train Effect)
  // ====================================================================
  std::unordered_map<uint64_t, int> in_degree;
  for (const auto &pair : dependency_graph) {
    in_degree[pair.second]++;
  }

  // Collect free ends (units with no other moving entities behind them)
  std::queue<uint64_t> leaf_nodes;
  for (const auto &pair : cached_requests) {
    if (in_degree[pair.first] == 0) {
      leaf_nodes.push(pair.first);
    }
  }

  std::unordered_set<uint64_t> resolved_dependents;

  while (!leaf_nodes.empty()) {
    uint64_t uid = leaf_nodes.front();
    leaf_nodes.pop();

    if (cached_requests.find(uid) == cached_requests.end())
      continue;
    const MoveRequest &mr = cached_requests.at(uid);

    // If the blocking vehicle safely vacated the cell, this unit slides forward
    if (coord_tracker.find(mr.to) == coord_tracker.end()) {
      coord_tracker[mr.to].push_back(mr);
      resolved_dependents.insert(uid);

      // De-escalate blocking restrictions backward down the train line
      if (dependency_graph.find(uid) != dependency_graph.end()) {
        uint64_t blocker_id = dependency_graph.at(uid);
        in_degree[blocker_id]--;
        if (in_degree[blocker_id] == 0) {
          leaf_nodes.push(blocker_id);
        }
      }
    } else {
      // Space became locked or blocked permanently. Evict to fallback queue.
      push_step_back_request(next_pass_mrq, mr.unit_id, mr.to);
      state_changed = true;
    }
  }

  // ====================================================================
  // PHASE 3: CLOSED-LOOP CYCLE TRACING (Roundabout Arbitration)
  // ====================================================================
  for (const auto &pair : cached_requests) {
    uint64_t start_uid = pair.first;
    if (resolved_dependents.find(start_uid) != resolved_dependents.end())
      continue;

    // Follow the single out-dependency chain to discover cyclic tracks
    std::unordered_set<uint64_t> visited_in_chain;
    std::vector<uint64_t> current_chain;

    uint64_t current_uid = start_uid;
    bool is_cycle = false;

    while (true) {
      if (resolved_dependents.find(current_uid) != resolved_dependents.end())
        break;
      if (cached_requests.find(current_uid) == cached_requests.end())
        break;

      if (visited_in_chain.find(current_uid) != visited_in_chain.end()) {
        auto it =
            std::find(current_chain.begin(), current_chain.end(), current_uid);
        if (it != current_chain.end()) {
          current_chain.erase(current_chain.begin(), it);
          is_cycle = true;
        }
        break;
      }

      visited_in_chain.insert(current_uid);
      current_chain.push_back(current_uid);

      if (dependency_graph.find(current_uid) != dependency_graph.end()) {
        current_uid = dependency_graph.at(current_uid);
      } else {
        break; // Chain collided into a stationary object or wall
      }
    }

    if (is_cycle) {
      std::cout << "Synchronous loop verified! Executing concurrent shift for "
                   "units: ";
      for (uint64_t cycle_uid : current_chain) {
        std::cout << cycle_uid << " ";
      }
      std::cout << "\n";

      // Allow every component of the circular track to shift forward at once
      for (uint64_t cycle_uid : current_chain) {
        const MoveRequest &cycle_mr = cached_requests.at(cycle_uid);
        coord_tracker[cycle_mr.to].push_back(cycle_mr);
        resolved_dependents.insert(cycle_uid);
      }
      continue;
    }

    // Broken dependency chain or dead-end: Unit must issue a step-back request
    const MoveRequest &mr = pair.second;
    push_step_back_request(next_pass_mrq, mr.unit_id, mr.to);
    state_changed = true;
  }
}


const std::vector<Unit> &State::get_units() const {
  std::vector<Unit> units;
  for (const auto &p: unit_map){
    units.push_back(p.second);
  } 

  return units;
}