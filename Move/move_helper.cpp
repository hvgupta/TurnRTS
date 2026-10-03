#include "move.hpp"

Coord one_step_back(Coord from, Coord to){
    if (from == to){
        return from;
    }

    int dx = to.x - from.x;
    int dy = to.y - from.y;

    auto step_calculator = [](int dom_da, int dom_axis_val, int other_da, int other_val){
        int step_a = (dom_da > 0) ? 1 : -1;
        int prev_a = dom_axis_val-step_a;

        int num = other_da * (prev_a - dom_axis_val);
        int adjustment = ((num * dom_axis_val > 0) ? 1:-1)*std::abs(dom_da)/2;
        int prev_other = other_val + (num+adjustment)/dom_da;

        return std::pair<int,int>(prev_a, prev_other);
    };

    if (std::abs(dx) >= std::abs(dy)){
        std::pair<int,int> out = step_calculator(dx, to.x, dy, to.y);
        return {out.first, out.second};
    }
    
    std::pair<int,int> out = step_calculator(dy, to.y, dx, to.x);
    return {out.second, out.first};
}