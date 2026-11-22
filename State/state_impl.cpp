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

class DependencyManager {
private:
  std::unordered_map<uint64_t, uint64_t> dependency_graph;
  std::unordered_map<uint64_t, MoveRequest> unit_to_move;

public:
  DependencyManager() {}
  void add_dependency(const uint64_t &mover, const MoveRequest &waiter) {
    dependency_graph[mover] = waiter.unit_id;
    unit_to_move[waiter.unit_id] = waiter;
  }
};

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
State::simple_conflict_resolve(std::vector<MoveRequest> mrl) const {
  std::unordered_set<uint64_t> moving_units;
  std::vector<MoveRequest> accepted_requests;
  std::unordered_set<Coord> will_be_occupied;
  DependencyManager dm;

  for (const MoveRequest &mr : mrl) {
    moving_units.insert(mr.unit_id);
  }

  auto order_func = [](const MoveRequest &a, const MoveRequest &b) {
    return a.time < b.time;
  };
  std::make_heap(mrl.begin(), mrl.end(), order_func);

  auto take_one_step_back = [this, order_func, &mrl](const MoveRequest &mr) {
    const Unit &cur_unit = this->unit_map.at(mr.unit_id);
    Coord new_coord = one_step_back(cur_unit.get_coord().value(), mr.to);
    MoveRequest new_mr = cur_unit.build_movement_request(new_coord).value();
    mrl.push_back(new_mr);
    std::push_heap(mrl.begin(), mrl.end(), order_func);
  };

  while (!mrl.empty()) {
    MoveRequest mr = mrl.back();
    mrl.pop_back();

    if (this->coord_inverse_map.contains(mr.to)) {
      uint64_t mover_id = this->coord_inverse_map.at(mr.to);
      if (moving_units.contains(mover_id)) {
        dm.add_dependency(mover_id, mr);
      } else {
        take_one_step_back(mr);
      }
      continue;
    }

    if (!will_be_occupied.contains(mr.to)) {
      accepted_requests.push_back(mr);
      will_be_occupied.insert(mr.to);
    } else {
      take_one_step_back(mr);
    }
  }

  return accepted_requests;
}

std::vector<MoveRequest>
State::simple_conflict_resolve(std::queue<MoveRequest> mrq,
                               std::set<uint64_t> moving_units) const {
  std::unordered_map<Coord, MoveRequest> toCoord_to_accepted_move_map;
  int num_iterations = 0;
  while (num_iterations < kMaxGenerations && !mrq.empty()) {
    MoveRequest mr = mrq.front();
    mrq.pop();

    if (!this->coord_inverse_map.contains(mr.to)) {
    }
    // unordered_map<Coord, MoveRequest>
    // moveBack
    // (time, &moveRequsest)?
    // 2 units can have the same (time, toCoord)
  }
}

const std::vector<Unit> State::get_units() const {
  std::vector<Unit> units;
  for (const auto &p : unit_map) {
    units.push_back(p.second);
  }

  return units;
}

void set_conflict_seed(uint64_t seed) { conflict_rng().seed(seed); }