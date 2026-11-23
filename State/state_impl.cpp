#include "state.hpp"


std::vector<MoveRequest> State::Turn::get_accepted_requests() const {
  return this->accepted_requests;
}

const std::vector<Unit> State::get_units() const {
  std::vector<Unit> units;
  for (const auto &p : unit_map) {
    units.push_back(p.second);
  }

  return units;
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