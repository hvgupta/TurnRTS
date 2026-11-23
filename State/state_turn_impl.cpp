#include "state.hpp"
#include <random>
#include <set>

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
    if (cur_evicted){
      this->take_one_step_back(s, mr);
    }
  }
  this->take_one_step_back(s, will_be_occupied.at(mr.to));
  will_be_occupied[mr.to] = mr;
}

void State::Turn::take_one_step_back(const State &s, const MoveRequest &mr) {
  const Unit &cur_unit = s.unit_map.at(mr.unit_id);
  Coord new_coord = one_step_back(cur_unit.get_coord().value(), mr.to);
  MoveRequest new_mr = cur_unit.build_movement_request(new_coord).value();
  this->mrl_push(new_mr);
}

void State::Turn::mrl_push(const MoveRequest &mr) {
  this->mrl.push_back(mr);
  std::push_heap(this->mrl.begin(), this->mrl.end(), order_func);
}

void State::Turn::evaluate(const State &s) {
  for (const MoveRequest &mr : this->mrl) {
    this->moving_units.insert(mr.unit_id);
  }

  int num_generations = 0;
  while ((!mrl.empty() || !dependency_graph.empty()) &&
         num_generations++ < kMaxGenerations) {
    non_dependency_and_circular_eval(s);
    release_non_circular_dependency(s);
  }

  for (const auto &p : will_be_occupied) {
    accepted_requests.push_back(p.second);
  }
}

void State::Turn::non_dependency_and_circular_eval(const State &s) {
  while (!this->mrl.empty()) {
    MoveRequest mr = this->mrl.back();
    this->mrl.pop_back();

    if (mr.to == s.unit_map.at(mr.unit_id).get_coord().value()) {
      this->moving_units.erase(mr.unit_id);
      continue;
    }

    if (s.coord_inverse_map.contains(mr.to)) {
      uint64_t mover_id = s.coord_inverse_map.at(mr.to);
      if (this->moving_units.contains(mover_id)) {
        this->add_dependency(mover_id, mr);
      } else {
        this->take_one_step_back(s, mr);
      }
      continue;
    }

    if (!will_be_occupied.contains(mr.to)) {
      will_be_occupied[mr.to] = mr;
    } else {
      if (mr.time <= will_be_occupied.at(mr.to).time) {
        manage_conflict(s, mr, mr.time == will_be_occupied.at(mr.to).time);
        continue;
      }
      this->take_one_step_back(s, mr);
    }
  }
}

void State::Turn::release_non_circular_dependency(const State &s) {
  std::unordered_map<uint64_t, uint64_t> new_dependency_graph;
  for (const auto &p : dependency_graph) {
    if (dependency_graph.contains(p.second)) {
      new_dependency_graph[p.first] = p.second;
      continue;
    }
    if (moving_units.contains(p.second)) {
      this->mrl_push(this->unit_to_move_map.at(p.first));
    } else {
      this->take_one_step_back(s, this->unit_to_move_map.at(p.first));
    }
  }

  dependency_graph = new_dependency_graph;
}