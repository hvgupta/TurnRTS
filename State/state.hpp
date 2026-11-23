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
    std::vector<MoveRequest> accepted_requests;
    
    std::unordered_set<uint64_t> moving_units;
    std::unordered_map<uint64_t, uint64_t> dependency_graph;
    std::unordered_map<uint64_t, MoveRequest> unit_to_move_map;
    std::unordered_map<Coord, MoveRequest> will_be_occupied;

    void mrl_push(const MoveRequest &mr);

    void non_dependency_and_circular_eval(const State &s);
    void manage_conflict(const State &s, MoveRequest &mr, bool same_time);
    void release_non_circular_dependency(const State &s);
    void release_circular_dependency(const State &s);

  public:
    Turn(std::vector<MoveRequest> &&_mrl);
    void add_dependency(const uint64_t &mover, const MoveRequest &waiter);
    void evaluate(const State &s);
    void take_one_step_back(const State &s, const MoveRequest &mr);
    std::vector<MoveRequest> get_accepted_requests() const;
  };

public:
  void add_unit(Unit &&unit);
  void add_unit(uint16_t range, uint8_t speed, Coord cur_pos);

  const std::vector<Unit> get_units() const;

  std::vector<MoveRequest> evaluate(std::vector<MoveRequest> mrl) const;
};

void set_conflict_seed(uint64_t seed);
#endif