#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <queue>
#include <random>
#include <set>
#include <unordered_map>
#include <vector>

#include "move.hpp"
#include "state.hpp"
#include "unit.hpp"

using Starts = std::map<uint64_t, Coord>; // logical unit id -> start tile

Coord C(int x, int y) { return Coord{x, y}; }

// MoveRequest is {unit_id, time, to}.
MoveRequest R(uint64_t logical_id, Coord to, float time) {
  return MoveRequest{logical_id, time, to};
}

struct Built {
  State state;
  std::map<uint64_t, uint64_t> to_real, to_logical;
};

Built build(const Starts &starts) {
  Built b;
  for (const auto &[logical, c] : starts) {
    Unit u(/*range=*/1, /*speed=*/1, c);
    b.to_real[logical] = u.get_id();
    b.to_logical[u.get_id()] = logical;
    b.state.add_unit(
        std::move(u)); // TODO adapt: however State registers a unit
  }
  return b;
}

// ------------------------------------------------------------------- helpers
std::vector<MoveRequest> resolve(const Starts &starts,
                                 const std::vector<MoveRequest> &reqs) {
  Built b = build(starts);
  auto out = b.state.simple_conflict_resolve(reqs);
  for (auto &o : out)
    o.unit_id = b.to_logical.at(o.unit_id);
  return out;
}

// Final tile of every unit; units with no output request stay put.
std::map<uint64_t, Coord> finals(const Starts &starts,
                                 const std::vector<MoveRequest> &out) {
  std::map<uint64_t, Coord> f(starts.begin(), starts.end());
  for (const auto &o : out)
    f.at(o.unit_id) = o.to;
  return f;
}

// Invariants that must hold for ANY input.
void check_invariants(const Starts &starts, const std::vector<MoveRequest> &in,
                      const std::vector<MoveRequest> &out) {
  // 1. Every requesting unit appears exactly once (nobody dropped/duplicated).
  std::map<uint64_t, int> seen;
  for (const auto &o : out)
    seen[o.unit_id]++;
  for (const auto &r : in)
    ASSERT_EQ(seen[r.unit_id], 1) << "unit " << r.unit_id;
  ASSERT_EQ(out.size(), in.size());

  // 2. One unit per tile, counting units that didn't request anything.
  auto f = finals(starts, out);
  std::unordered_map<Coord, uint64_t> tile;
  for (const auto &[id, c] : f) {
    auto [it, inserted] = tile.emplace(c, id);
    ASSERT_TRUE(inserted) << "units " << it->second << " and " << id
                          << " share a tile";
  }

  // 3. No swaps / pass-through.
  for (const auto &[u, fu] : f)
    for (const auto &[v, fv] : f)
      if (u < v && fu == starts.at(v) && fv == starts.at(u) &&
          !(starts.at(u) == starts.at(v)))
        FAIL() << "units " << u << " and " << v << " swapped";
}

// ------------------------------------------------------------ basic scenarios
TEST(ConflictResolve, SingleFreeMoveIsAccepted) {
  Starts s{{1, C(0, 0)}};
  auto in = std::vector{R(1, C(1, 0), 1)};
  auto out = resolve(s, in);
  check_invariants(s, in, out);
  EXPECT_EQ(finals(s, out).at(1), C(1, 0));
}

TEST(ConflictResolve, EarlierArrivalWinsFreeTile) {
  Starts s{{1, C(0, 0)}, {2, C(2, 0)}};
  auto in = std::vector{R(1, C(1, 0), 1), R(2, C(1, 0), 2)};
  for (uint64_t seed = 0; seed < 50; ++seed) {
    set_conflict_seed(seed);
    auto out = resolve(s, in);
    check_invariants(s, in, out);
    auto f = finals(s, out);
    EXPECT_EQ(f.at(1), C(1, 0));
    EXPECT_EQ(f.at(2), C(2, 0)); // late unit steps back
  }
}

TEST(ConflictResolve, MoveIntoStationaryUnitIsRejected) {
  Starts s{{1, C(0, 0)}, {2, C(1, 0)}}; // unit 2 doesn't request a move
  auto in = std::vector{R(1, C(1, 0), 1)};
  auto out = resolve(s, in);
  check_invariants(s, in, out);
  EXPECT_EQ(finals(s, out).at(1), C(0, 0));
}

TEST(ConflictResolve, UnitRequestingItsOwnTileStaysAndBlocksOthers) {
  Starts s{{1, C(0, 0)}, {2, C(1, 0)}};
  auto in = std::vector{R(1, C(1, 0), 1), R(2, C(1, 0), 1)}; // 2 stays
  auto out = resolve(s, in);
  check_invariants(s, in, out);
  EXPECT_EQ(finals(s, out).at(1), C(0, 0));
  EXPECT_EQ(finals(s, out).at(2), C(1, 0));
}

// -------------------------------------------------------------------- trains
TEST(ConflictResolve, TrainFollowsHeadIntoFreeTile) {
  Starts s{{1, C(0, 0)}, {2, C(1, 0)}, {3, C(2, 0)}};
  auto in = std::vector{R(1, C(1, 0), 1), R(2, C(2, 0), 1), R(3, C(3, 0), 1)};
  auto out = resolve(s, in);
  check_invariants(s, in, out);
  auto f = finals(s, out);
  EXPECT_EQ(f.at(1), C(1, 0));
  EXPECT_EQ(f.at(2), C(2, 0));
  EXPECT_EQ(f.at(3), C(3, 0));
}

TEST(ConflictResolve, BlockedHeadEvictsWholeTrain) {
  Starts s{{1, C(0, 0)}, {2, C(1, 0)}, {3, C(2, 0)}, {4, C(3, 0)}};
  // unit 4 is stationary and has no request
  auto in = std::vector{R(1, C(1, 0), 1), R(2, C(2, 0), 1), R(3, C(3, 0), 1)};
  auto out = resolve(s, in);
  check_invariants(s, in, out);
  auto f = finals(s, out);
  EXPECT_EQ(f.at(1), C(0, 0));
  EXPECT_EQ(f.at(2), C(1, 0));
  EXPECT_EQ(f.at(3), C(2, 0));
}

// -------------------------------------------------------------------- cycles
TEST(ConflictResolve, FourCycleRotates) {
  Starts s{{1, C(0, 0)}, {2, C(1, 0)}, {3, C(1, 1)}, {4, C(0, 1)}};
  auto in = std::vector{R(1, C(1, 0), 1), R(2, C(1, 1), 1), R(3, C(0, 1), 1),
                        R(4, C(0, 0), 1)};
  auto out = resolve(s, in);
  check_invariants(s, in, out);
  auto f = finals(s, out);
  EXPECT_EQ(f.at(1), C(1, 0));
  EXPECT_EQ(f.at(2), C(1, 1));
  EXPECT_EQ(f.at(3), C(0, 1));
  EXPECT_EQ(f.at(4), C(0, 0));
}

TEST(ConflictResolve, SwapIsRejected) {
  Starts s{{1, C(0, 0)}, {2, C(1, 0)}};
  auto in = std::vector{R(1, C(1, 0), 1), R(2, C(0, 0), 1)};
  auto out = resolve(s, in);
  check_invariants(s, in, out);
  EXPECT_EQ(finals(s, out).at(1), C(0, 0));
  EXPECT_EQ(finals(s, out).at(2), C(1, 0));
}

TEST(ConflictResolve, TailOnCycleIsNotDroppedAndDoesNotEnter) {
  Starts s{
      {1, C(0, 0)}, {2, C(1, 0)}, {3, C(1, 1)}, {4, C(0, 1)}, {5, C(-1, 0)}};
  auto in = std::vector{R(1, C(1, 0), 1), R(2, C(1, 1), 1), R(3, C(0, 1), 1),
                        R(4, C(0, 0), 1), R(5, C(0, 0), 1)};
  for (uint64_t seed = 0; seed < 20; ++seed) {
    set_conflict_seed(seed);
    auto out = resolve(s, in);
    check_invariants(s, in, out);
    EXPECT_EQ(finals(s, out).at(4), C(0, 0)); // cycle member wins its tile
    EXPECT_EQ(finals(s, out).at(5), C(-1, 0));
  }
}

// ------------------------------------------------------------------ fairness
// Units 1 and 2 both want the tile unit 3 is vacating; 3 moves to a free tile.
static std::map<uint64_t, int> vacated_tile_wins(int trials) {
  Starts s{{1, C(0, 0)}, {2, C(1, 1)}, {3, C(1, 0)}};
  auto in = std::vector{R(1, C(1, 0), 1), R(2, C(1, 0), 1), R(3, C(2, 0), 1)};
  std::map<uint64_t, int> wins;
  for (int i = 0; i < trials; ++i) {
    set_conflict_seed(i);
    auto out = resolve(s, in);
    check_invariants(s, in, out);
    auto f = finals(s, out);
    EXPECT_EQ(f.at(3), C(2, 0));
    for (uint64_t u : {1, 2})
      if (f.at(u) == C(1, 0))
        wins[u]++;
  }
  return wins;
}

TEST(ConflictResolve, SameTimeContentionForVacatedTileIsFair) {
  const int n = 2000;
  auto wins = vacated_tile_wins(n);
  EXPECT_EQ(wins[1] + wins[2], n); // exactly one winner every time
  EXPECT_NEAR(wins[1] / double(n), 0.5, 0.05);
}

TEST(ConflictResolve, SameTimeContentionForFreeTileIsFairForThree) {
  Starts s{{1, C(0, 0)}, {2, C(2, 0)}, {3, C(1, 1)}};
  auto in = std::vector{R(1, C(1, 0), 1), R(2, C(1, 0), 1), R(3, C(1, 0), 1)};
  const int n = 3000;
  std::map<uint64_t, int> wins;
  for (int i = 0; i < n; ++i) {
    set_conflict_seed(i);
    auto out = resolve(s, in);
    check_invariants(s, in, out);
    for (const auto &[id, c] : finals(s, out))
      if (c == C(1, 0))
        wins[id]++;
  }
  for (uint64_t u : {1, 2, 3})
    EXPECT_NEAR(wins[u] / double(n), 1.0 / 3, 0.05) << "unit " << u;
}

TEST(ConflictResolve, EarlierTimeBeatsLaterForVacatedTile) {
  Starts s{{1, C(0, 0)}, {2, C(1, 1)}, {3, C(1, 0)}};
  auto in = std::vector{R(1, C(1, 0), 2), R(2, C(1, 0), 1), R(3, C(2, 0), 1)};
  for (uint64_t seed = 0; seed < 50; ++seed) {
    set_conflict_seed(seed);
    auto out = resolve(s, in);
    check_invariants(s, in, out);
    EXPECT_EQ(finals(s, out).at(2), C(1, 0));
  }
}

TEST(ConflictResolve, SameSeedGivesSameResult) {
  Starts s{{1, C(0, 0)}, {2, C(2, 0)}, {3, C(1, 1)}};
  auto in = std::vector{R(1, C(1, 0), 1), R(2, C(1, 0), 1), R(3, C(1, 0), 1)};
  set_conflict_seed(42);
  auto a = finals(s, resolve(s, in));
  set_conflict_seed(42);
  auto b = finals(s, resolve(s, in));
  EXPECT_EQ(a, b);
}

// ---------------------------------------------------------------------- fuzz
// Random boards: whatever happens, the invariants must hold and it must
// terminate (the generation cap throws if it doesn't converge).
TEST(ConflictResolve, FuzzInvariants) {
  std::mt19937 rng(12345);
  const int W = 6, H = 6;

  for (int iter = 0; iter < 5000; ++iter) {
    std::vector<Coord> cells;
    for (int x = 0; x < W; ++x)
      for (int y = 0; y < H; ++y)
        cells.push_back(C(x, y));
    std::shuffle(cells.begin(), cells.end(), rng);

    const int n = 1 + rng() % 20;
    Starts s;
    for (int i = 0; i < n; ++i)
      s[i + 1] = cells[i];

    std::vector<MoveRequest> in;
    for (int i = 1; i <= n; ++i) {
      if (rng() % 4 == 0)
        continue; // this unit has no request
      static const int dx[] = {0, 1, -1, 0, 0}, dy[] = {0, 0, 0, 1, -1};
      int d = rng() % 5;
      int x = std::clamp(s[i].x + dx[d], 0, W - 1); // TODO adapt Coord fields
      int y = std::clamp(s[i].y + dy[d], 0, H - 1);
      in.push_back(R(i, C(x, y), 1 + rng() % 3));
    }

    set_conflict_seed(iter);
    std::vector<MoveRequest> out;
    ASSERT_NO_THROW(out = resolve(s, in)) << "iteration " << iter;
    check_invariants(s, in, out);
    if (::testing::Test::HasFailure()) {
      ADD_FAILURE() << "failing iteration: " << iter;
      return;
    }
  }
}

// ---------------------------------------------------------------- regression
TEST(State, GetUnitsReturnsAllUnitsByValue) {
  Starts s{{1, C(0, 0)}, {2, C(1, 0)}};
  Built b = build(s);
  auto units = b.state.get_units(); // was a dangling reference before
  EXPECT_EQ(units.size(), 2u);
}
