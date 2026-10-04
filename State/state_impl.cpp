#include "state.hpp"

#include <algorithm>
#include <random>
#include <set>
#include <unordered_set>

// Debug logging is compiled out unless CONFLICT_DEBUG is defined.
#ifdef CONFLICT_DEBUG
#define CR_LOG(x) (std::cout << x)
#else
#define CR_LOG(x) ((void)0)
#endif

namespace {
constexpr int kMaxGenerations = 1000; // safety cap against endless bouncing

std::mt19937_64 &conflict_rng() {
  static std::mt19937_64 rng{std::random_device{}()};
  return rng;
}
} // namespace

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

// Each generation re-evaluates ONE request per unit. Accepted requests are
// carried into the next generation, evicted ones are replaced by their
// step-back request, so the queue always holds exactly one request per unit.
// The loop ends when a generation changes nothing.
std::vector<MoveRequest>
State::simple_conflict_resolve(std::queue<MoveRequest> initial_mrq,
                               std::set<uint64_t> initial_moving_units) const {
  std::queue<MoveRequest> mrq = std::move(initial_mrq);
  std::set<uint64_t> moving_units = std::move(initial_moving_units);

  for (int gen = 0; gen < kMaxGenerations; ++gen) {
    std::unordered_map<Coord, MoveRequest> coord_tracker;
    std::unordered_map<uint64_t, uint64_t>
        dependency_graph; // waiter -> blocker
    std::unordered_map<uint64_t, MoveRequest> cached_requests;
    std::queue<MoveRequest> next_pass_mrq;
    bool state_changed = false;

    run_one_generation(mrq, moving_units, cached_requests, coord_tracker,
                       dependency_graph, next_pass_mrq, state_changed);

    if (!state_changed) {
      std::vector<MoveRequest> ans;
      for (const auto &c : coord_tracker) {
        ans.push_back(c.second);
      }
      return ans;
    }

    // Carry accepted requests forward so they are re-validated next pass.
    for (const auto &c : coord_tracker) {
      next_pass_mrq.push(c.second);
    }
    mrq = std::move(next_pass_mrq);
  }

  throw std::runtime_error("simple_conflict_resolve: did not converge");
}

void State::run_one_generation(
    std::queue<MoveRequest> &mrq, std::set<uint64_t> &moving_units,
    std::unordered_map<uint64_t, MoveRequest> &cached_requests,
    std::unordered_map<Coord, MoveRequest> &coord_tracker,
    std::unordered_map<uint64_t, uint64_t> &dependency_graph,
    std::queue<MoveRequest> &next_pass_mrq, bool &state_changed) const {

  // ---------------------------------------------------------------- phase 1
  // Classify every request.
  std::unordered_map<Coord, int> tie_count;
  while (!mrq.empty()) {
    const MoveRequest mr = mrq.front();
    mrq.pop();

    // Target tile is occupied in the original grid.
    // auto occ = coord_inverse_map.find(mr.to);
    if (coord_inverse_map.contains(mr.to)) {
      const uint64_t blocker_id = coord_inverse_map.at(mr.to);

      if (blocker_id == mr.unit_id) {
        // Stationary: unit stays where it is.
        moving_units.erase(blocker_id);
        cached_requests.erase(blocker_id);
        dependency_graph.erase(blocker_id);
        coord_tracker[mr.to] = mr;
      } else if (moving_units.contains(blocker_id)) {
        // Might vacate the tile; decide in phase 2.
        dependency_graph[mr.unit_id] = blocker_id;
        cached_requests[mr.unit_id] = mr;
      } else {
        // Blocker is stationary: cannot enter.
        push_step_back_request(next_pass_mrq, mr.unit_id, mr.to);
        state_changed = true;
      }
      continue;
    }

    // Free tile: earliest arrival wins, ties broken by lowest unit id.
    // coord_tracker[to] therefore holds at most one request for free tiles.
    if (!coord_tracker.contains(mr.to)) {
      coord_tracker[mr.to] = mr;
      continue;
    }

    const MoveRequest cur = coord_tracker.at(mr.to);
    bool mr_wins;
    if (mr.time < cur.time) {
      mr_wins = true;
      tie_count[mr.to] = 1; // new earliest time: restart the lottery
    } else if (mr.time == cur.time) {
      const int n = ++tie_count[mr.to];
      mr_wins = std::uniform_int_distribution<int>(1, n)(conflict_rng()) == 1;
    } else {
      mr_wins = false;
    }

    CR_LOG(mr.unit_id << (mr_wins ? " displaces " : " loses to ") << cur.unit_id
                      << "\n");

    if (mr_wins) {
      push_step_back_request(next_pass_mrq, cur.unit_id, cur.to);
      coord_tracker[mr.to] = {mr};
    } else {
      push_step_back_request(next_pass_mrq, mr.unit_id, mr.to);
    }
    state_changed = true;
  }

  // ---------------------------------------------------------------- phase 2
  // Resolve requests that target a tile currently occupied by a moving unit.
  // Blockers are evaluated before the units waiting on them.

  // Units that hold an accepted request right now.
  std::unordered_set<uint64_t> accepted;
  for (const auto &c : coord_tracker) {
    accepted.insert(c.second.unit_id);
  }

  // Reverse graph: blocker -> units waiting on it.
  std::unordered_map<uint64_t, std::vector<uint64_t>> waiters;
  for (const auto &[uid, _] : cached_requests) {
    waiters[dependency_graph.at(uid)].push_back(uid);
  }

  std::unordered_set<uint64_t> handled; // accepted or evicted in phase 2
  std::queue<uint64_t> ready;

  // Accept a cached request and release the units waiting behind it.
  auto accept = [&](uint64_t uid) {
    const MoveRequest &mr = cached_requests.at(uid);
    coord_tracker[mr.to] = mr;
    accepted.insert(uid);
    handled.insert(uid);
    if (!waiters.contains(uid)) {
      return;
    }
    for (const uint64_t &wuid : waiters.at(uid)) {
      ready.push(wuid);
    }
  };

  // Evict a cached request. Everything queued behind it is blocked too,
  // because the evicted unit still occupies its tile.
  auto evict_cascade = [&](uint64_t root) {
    std::vector<uint64_t> stack{root};
    while (!stack.empty()) {
      uint64_t uid = stack.back();
      stack.pop_back();
      if (!handled.insert(uid).second) {
        continue;
      }
      const MoveRequest &mr = cached_requests.at(uid);
      push_step_back_request(next_pass_mrq, mr.unit_id, mr.to);
      state_changed = true;
      if (!waiters.contains(uid)){
        continue;
      }
      for (const uint64_t &x: waiters.at(uid)){
        stack.push_back(x);
      }
    }
  };

  // Evaluate ready units: blocker must have an accepted move AND the target
  // tile must not have been claimed by someone else.
  auto drain = [&]() {
    while (!ready.empty()) {
      uint64_t uid = ready.front();
      ready.pop();
      if (handled.count(uid))
        continue;
      const MoveRequest &mr = cached_requests.at(uid);
      const uint64_t blocker = dependency_graph.at(uid);
      const bool blocker_left = accepted.count(blocker) > 0;
      if (blocker_left && coord_tracker.find(mr.to) == coord_tracker.end()) {
        accept(uid);
      } else {
        evict_cascade(uid);
      }
    }
  };

  // Heads: blocker is not itself a pending cached request, so its fate is
  // already decided (accepted, stationary, or evicted in phase 1).
  for (const auto &[uid, _] : cached_requests) {
    if (!cached_requests.count(dependency_graph.at(uid))) {
      ready.push(uid);
    }
  }
  drain();

  // ---------------------------------------------------------------- phase 3
  // Whatever is still unhandled is a cycle or a tail hanging off a cycle.
  for (const auto &[start, _] : cached_requests) {
    if (handled.count(start))
      continue;

    // Walk the single out-edge chain until it repeats.
    std::unordered_map<uint64_t, size_t> pos;
    std::vector<uint64_t> path;
    uint64_t cur = start;
    while (!handled.count(cur) && cached_requests.count(cur) &&
           !pos.count(cur)) {
      pos[cur] = path.size();
      path.push_back(cur);
      cur = dependency_graph.at(cur);
    }

    if (pos.count(cur) && !handled.count(cur)) {
      std::vector<uint64_t> cycle(path.begin() + pos[cur], path.end());

      // A 2-cycle is a swap (units pass through each other): disallow.
      bool ok = cycle.size() > 2;
      for (uint64_t id : cycle) {
        if (coord_tracker.count(cached_requests.at(id).to))
          ok = false;
      }

      if (ok) {
        CR_LOG("Synchronous loop, shifting " << cycle.size() << " units\n");
        for (uint64_t id : cycle)
          accept(id);
        drain(); // releases tails hanging off the cycle
      } else {
        evict_cascade(cycle.front()); // also evicts all tails
      }
    }

    // Safety net: nothing should remain unhandled, but never drop a unit.
    if (!handled.count(start)) {
      evict_cascade(start);
    }
  }
}

const std::vector<Unit> &State::get_units() const {
  std::vector<Unit> units;
  for (const auto &p : unit_map) {
    units.push_back(p.second);
  }

  return units;
}