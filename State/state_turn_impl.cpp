#include "helper_functions.hpp"
#include "state.hpp"

#include <random>
#include <set>
#include <stack>

constexpr int kMaxGenerations = 1000; // safety cap against endless bouncing

std::mt19937_64 &conflict_rng() {
  static std::mt19937_64 rng{std::random_device{}()};
  return rng;
}
void set_conflict_seed(uint64_t seed) { conflict_rng().seed(seed); }

auto order_func = [](const MoveRequest &a, const MoveRequest &b) {
  return a.time < b.time;
};

State::Turn::Turn(std::vector<MoveRequest> &&_mrl) {
  mrl = _mrl;
  std::make_heap(this->mrl.begin(), this->mrl.end(), order_func);
}

void State::Turn::add_dependency(const uint64_t &mover,
                                 const MoveRequest &waiter) {
  dependency_graph[waiter.unit_id] = mover;
  unit_to_move_map[waiter.unit_id] = waiter;
}

void State::Turn::manage_conflict(const State &s, MoveRequest &mr,
                                  bool same_time) {
  if (same_time) {
    std::bernoulli_distribution coin(0.5);
    bool cur_evicted = coin(conflict_rng());
    if (cur_evicted) {
      this->take_one_step_back(s, mr);
    }
  }
  this->take_one_step_back(s, will_be_occupied.at(mr.to));
  will_be_occupied[mr.to] = mr;
  this->accepted_units.insert(mr.unit_id);
}

void State::Turn::take_one_step_back(const State &s, const MoveRequest &mr) {
  this->accepted_units.erase(mr.unit_id);
  const Unit &cur_unit = s.unit_map.at(mr.unit_id);
  Coord new_coord = one_step_back(cur_unit.get_coord().value(), mr.to);
  MoveRequest new_mr = cur_unit.build_movement_request(new_coord).value();
  this->mrl_push(new_mr);
}

void State::Turn::mrl_push(const MoveRequest &mr) {
  this->mrl.push_back(mr);
  std::push_heap(this->mrl.begin(), this->mrl.end(), order_func);
}

std::vector<MoveRequest> State::Turn::evaluate(const State &s) {
  std::unordered_set<uint64_t> moving_units;
  for (const MoveRequest &mr : this->mrl) {
    moving_units.insert(mr.unit_id);
  }

  int num_generations = 0;
  while ((!mrl.empty() || !dependency_graph.empty()) &&
         num_generations++ < kMaxGenerations) {
    non_dependency_and_circular_eval(s, moving_units);
    release_non_circular_dependency(s);
  }
  std::vector<MoveRequest> accepted_requests;
  for (const auto &p : this->will_be_occupied) {
    accepted_requests.push_back(p.second);
  }

  return accepted_requests;
}

void State::Turn::non_dependency_and_circular_eval(
    const State &s, std::unordered_set<uint64_t> moving_units) {
  while (!this->mrl.empty()) {
    MoveRequest mr = this->mrl.back();
    this->mrl.pop_back();

    if (mr.to == s.unit_map.at(mr.unit_id).get_coord().value()) {
      moving_units.erase(mr.unit_id);
      continue;
    }

    if (s.coord_inverse_map.contains(mr.to) &&
        !this->accepted_units.contains(s.coord_inverse_map.at(mr.to))) {
      uint64_t mover_id = s.coord_inverse_map.at(mr.to);
      if (moving_units.contains(mover_id)) {
        this->add_dependency(mover_id, mr);
      } else {
        this->take_one_step_back(s, mr);
      }
      continue;
    }

    if (!this->will_be_occupied.contains(mr.to)) {
      this->will_be_occupied[mr.to] = mr;
      this->accepted_units.insert(mr.unit_id);
    } else {
      if (mr.time <= this->will_be_occupied.at(mr.to).time) {
        this->manage_conflict(s, mr,
                              mr.time == this->will_be_occupied.at(mr.to).time);
        continue;
      }
      this->take_one_step_back(s, mr);
    }
  }
}

void State::Turn::release_non_circular_dependency(const State &s) {
  std::unordered_map<uint64_t, uint64_t> new_dependency_graph;
  std::unordered_map<uint64_t, MoveRequest> new_unit_to_move_map;

  for (const auto &p : dependency_graph) {
    if (dependency_graph.contains(p.second)) {
      new_dependency_graph[p.first] = p.second;
      new_unit_to_move_map[p.first] = this->unit_to_move_map.at(p.first);
      continue;
    }
    if (accepted_units.contains(p.second)) {
      this->mrl_push(this->unit_to_move_map.at(p.first));
    } else {
      this->take_one_step_back(s, this->unit_to_move_map.at(p.first));
    }
    this->unit_to_move_map.erase(p.first);
  }

  this->dependency_graph = new_dependency_graph;
  this->unit_to_move_map = new_unit_to_move_map;
}

void State::Turn::release_circular_dependency() {
  std::unordered_map<uint64_t, std::vector<uint64_t>> inv_dependency_map =
      invert_map(this->dependency_graph);

  std::vector<uint64_t> simple_cycle_units;
  std::vector<std::vector<uint64_t>> complex_cycle_units;

  std::unordered_map<uint64_t, uint64_t> new_dependency_map;
  std::unordered_set<uint64_t> global_seen;

  for (const auto &p : this->dependency_graph) {
    if (global_seen.contains(p.first)) {
      continue;
    }
    std::unordered_set<uint64_t> seen;
    std::unordered_map<uint64_t, uint64_t> local_dependency_map;
    local_dependency_map[p.first] = p.second;
    bool is_complex = false;
    bool is_loop = false;

    std::stack<uint64_t> s;

    s.push(p.first);

    while (!s.empty()) {
      uint64_t cur_uid = s.top();
      s.pop();

      seen.insert(cur_uid);
      uint64_t next_id = this->dependency_graph.at(cur_uid);
      local_dependency_map[cur_uid] = next_id;
      if (!seen.contains(next_id) && !accepted_units.contains(next_id)) {
        s.push(next_id);
      }
      if (seen.contains(next_id)) {
        is_loop = true;
      }

      for (const uint64_t &prev_uid : inv_dependency_map.at(cur_uid)) {
        if (seen.contains(prev_uid)) {
          continue;
        }
        s.push(prev_uid);
      }

      is_complex |= (inv_dependency_map.at(cur_uid).size() > 1);
    }

    global_seen.insert(seen.begin(), seen.end());
    if (is_loop && !is_complex) {
      simple_cycle_units.insert(simple_cycle_units.end(), seen.begin(),
                                seen.end());
      continue;
    } else if (is_loop) {
      complex_cycle_units.push_back(
          std::vector<uint64_t>(seen.begin(), seen.end()));
    }
    new_dependency_map.insert(local_dependency_map.begin(),
                              local_dependency_map.end());
  }
  this->resolve_simple_cycle(simple_cycle_units);
  this->dependency_graph = new_dependency_map;
}

void State::Turn::resolve_simple_cycle(
    const std::vector<uint64_t> &simple_cycle_units) {
  for (const uint64_t &uid : simple_cycle_units) {
    MoveRequest mr = this->unit_to_move_map[uid];
    this->unit_to_move_map.erase(uid);
    this->will_be_occupied[mr.to] = mr;
    this->accepted_units.insert(mr.unit_id);
  }
}