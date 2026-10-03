#include <iostream>
#include <queue>
#include <random>
#include <unordered_map>
#include <vector>

#include "move.hpp"
#include "state.hpp"
#include "unit.hpp"

void add_units(State &state) {
  state.add_unit(Unit(4, 2, {0, 0}));
  state.add_unit(Unit(5, 6, {0, 1}));
  state.add_unit(Unit(1, 2, {1, 1}));
  state.add_unit(Unit(3, 1, {5, 5}));
}

std::vector<Coord> units_destination() {
  return {{6, 7}, {2, 3}, {1, 0}, {1, 0}};
}

int main() {
  State state;
  add_units(state);
  std::vector<Coord> dests = units_destination();
  std::vector<MoveRequest> mrl;
  int i = 0;
  for (const auto &p : state.get_units()) {
    p.print();
    std::cout << "moves to ";
    dests[i].print();
    std::cout << "\n";
    std::expected<MoveRequest, std::string> mr =
        p.build_movement_request(dests[i++]);
    if (!mr.has_value()) {
      std::cout << "some error has occured: " << mr.error() << "\n";
      continue;
    }
    mrl.push_back(mr.value());
  }
  std::cout << "\n\n";

  std::vector<MoveRequest> mr_out = state.simple_conflict_resolve(mrl);
  for (const auto &mro : mr_out) {
    mro.print();
    std::cout << "\n";
  }
}