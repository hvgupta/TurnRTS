#ifndef _STATE
#define _STATE 1

#pragma once

#include <queue>
#include <set>
#include <stdint.h>
#include <unordered_map>
#include <vector>

#include "unit.hpp"

class State {
  std::unordered_map<uint64_t, Unit> unit_map;
  std::unordered_map<Coord, uint64_t> coord_inverse_map;

  void push_step_back_request(std::queue<MoveRequest> &mrq,
                              const uint64_t &unit_id, const Coord &to) const;

  std::vector<MoveRequest>
  simple_conflict_resolve(std::queue<MoveRequest> mrq,
                          std::set<uint64_t> moving_units) const;

  bool
  run_one_generation(std::queue<MoveRequest> &mrq,
                     std::set<uint64_t> &moving_units,
                     std::unordered_map<uint64_t, MoveRequest> &cached_requests,
                     std::unordered_map<Coord, MoveRequest> &coord_tracker,
                     std::unordered_map<uint64_t, uint64_t> &dependency_graph,
                     std::queue<MoveRequest> &next_pass_mrq) const;

public:
  void add_unit(Unit &&unit);
  void add_unit(uint16_t range, uint8_t speed, Coord cur_pos);

  const std::vector<Unit> get_units() const;

  std::vector<MoveRequest>
  simple_conflict_resolve(std::vector<MoveRequest> mrl) const;
};

void set_conflict_seed(uint64_t seed);
#endif