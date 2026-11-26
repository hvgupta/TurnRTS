#ifndef _STATE
#define _STATE 1

#pragma once

#include <queue>
#include <stdint.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "unit.hpp"

class State {
  std::unordered_map<uint64_t, Unit> unit_map;
  std::unordered_map<Coord, uint64_t> coord_inverse_map;

  class Turn {
  private:
    std::vector<MoveRequest> mrl;

    std::unordered_map<uint64_t, uint64_t> dependency_graph;
    std::unordered_map<uint64_t, MoveRequest> unit_to_move_map;

    std::unordered_map<Coord, MoveRequest> will_be_occupied;
    std::unordered_set<uint64_t> accepted_units;

    void mrl_push(const MoveRequest &mr);

    void non_dependency_and_circular_eval(
        const State &s, std::unordered_set<uint64_t> moving_units);
    void manage_conflict(const State &s, MoveRequest &mr, bool same_time);
    void release_non_circular_dependency(const State &s);
    void release_circular_dependency();

    void resolve_simple_cycle(const std::vector<uint64_t> &simple_cycle_units);

  public:
    Turn(std::vector<MoveRequest> &&_mrl);
    void add_dependency(const uint64_t &mover, const MoveRequest &waiter);
    std::vector<MoveRequest> evaluate(const State &s);
    void take_one_step_back(const State &s, const MoveRequest &mr);
  };

public:
  void add_unit(Unit &&unit);
  void add_unit(uint16_t range, uint8_t speed, Coord cur_pos);

  const std::vector<Unit> get_units() const;

  std::vector<MoveRequest> evaluate(std::vector<MoveRequest> mrl) const;
};

void set_conflict_seed(uint64_t seed);
#endif