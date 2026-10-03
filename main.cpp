#include <iostream>
#include <vector>
#include <queue>
#include <unordered_map>
#include <random>

#include "unit.hpp"
#include "move.hpp"

std::unordered_map<uint64_t, Unit> get_units()
{
    Unit unit1(4, 2, {0, 0});
    Unit unit2(5, 6, {0, 1});
    Unit unit3(1, 2, {1, 1});
    Unit unit4(3, 1, {5, 5});
    return {{unit1.get_id(), unit1}, {unit2.get_id(), unit2}, {unit3.get_id(), unit3}, {unit4.get_id(), unit4}};
}

std::vector<Coord> units_destination()
{
    return {{6, 7}, {2, 3}, {1, 0}, {1, 0}};
}

std::vector<MoveRequest> simple_conflict_resolve(
    const std::unordered_map<uint64_t, Unit> &unit_map,
    std::queue<MoveRequest> mrq)
{
    std::unordered_map<Coord, std::vector<MoveRequest>> coord_tracker;

    auto tracker = [&mrq, &unit_map](const uint64_t &unit_id, const Coord &to)
    {
        const Unit &cur_unit = unit_map.at(unit_id);
        std::optional<Coord> cur_pos = cur_unit.get_coord();
        if (!cur_pos.has_value())
        {
            return;
        }
        Coord new_pos = one_step_back(cur_pos.value(), to);
        mrq.push(cur_unit.build_movement_request(new_pos).value());
    };

    while (!mrq.empty())
    {
        const MoveRequest mr = mrq.front();
        mrq.pop();

        if (coord_tracker.find(mr.to) == coord_tracker.end() || coord_tracker[mr.to][0].time == mr.time)
        {
            coord_tracker[mr.to].push_back(mr);
        }
        else if (coord_tracker[mr.to][0].time > mr.time)
        {
            std::cout << mr.unit_id << " removes the following\n"; 
            int n = coord_tracker[mr.to].size();
            for (int i = 0; i < n; i++)
            {
                const MoveRequest &cur_req = coord_tracker[mr.to][i];
                std::cout << "\t " << cur_req.unit_id << "\n";
                tracker(cur_req.unit_id, cur_req.to);
            }
            coord_tracker[mr.to] = {mr};
        }
        else
        {
            std::cout << "going one step back for " << mr.unit_id << "\n";
            tracker(mr.unit_id, mr.to);
        }
    }

    std::vector<MoveRequest> ans;
    for (const auto &c : coord_tracker)
    {
        ans.insert(ans.end(), c.second.begin(), c.second.end());
    }

    return ans;
}

int main()
{
    std::unordered_map<uint64_t, Unit> units = get_units();
    std::vector<Coord> dests = units_destination();
    std::vector<MoveRequest> mrl;
    int i = 0;
    for (const std::pair<const uint64_t, Unit> &p : units)
    {
        p.second.print();
        std::cout << "moves to ";
        dests[i].print();
        std::cout << "\n";
        std::expected<MoveRequest, std::string> mr = p.second.build_movement_request(dests[i++]);
        if (!mr.has_value())
        {
            std::cout << "some error has occured: " << mr.error() << "\n";
            continue;
        }
        mrl.push_back(mr.value());
    }
    std::cout << "\n\n";

    std::vector<MoveRequest> mr_out = simple_conflict_resolve(units, std::queue<MoveRequest>(mrl.begin(), mrl.end()));
    for (const auto &mro : mr_out)
    {
        mro.print();
        std::cout << "\n";
    }
}