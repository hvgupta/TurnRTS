#include <unit.hpp>
#include <iostream>

std::expected<MoveRequest, std::string> Unit::make_movement(Coord to){
    uint16_t dist_req = std::abs(cur_pos.x - to.x) + std::abs(cur_pos.y - to.y);
    if (dist_req > range){
        return std::unexpected("The movement is more than the range of the unit");
    }

    MoveRequest move_req{id, (float)dist_req/speed, to};
    return move_req;
}

void Unit::print() const {
    std::cout << "======Unit: " << id << " ==========\n";
    std::cout << "range: " << range << "\n";
    std::cout << "speed: " << speed << "\n";
    std::cout << "current postion -> x: " << cur_pos.x << " y: " << cur_pos.y << "\n";
}