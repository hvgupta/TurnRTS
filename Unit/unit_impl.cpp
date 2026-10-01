#include <unit.hpp>
#include <iostream>
#include <random>

uint64_t generateRandomIntKey() {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;
    return dis(gen);
}

const uint64_t&Unit::get_id() const {
    return id;
}

Unit::Unit(uint16_t range, uint8_t speed, Coord cur_pos): id(generateRandomIntKey()), range(range), speed(speed), cur_pos(cur_pos){}

std::expected<MoveRequest, std::string> Unit::build_movement_request(Coord to) const{
    uint16_t dist_req = std::abs(cur_pos.x - to.x) + std::abs(cur_pos.y - to.y);
    if (dist_req > range){
        return std::unexpected("The movement is more than the range of the unit");
    }

    MoveRequest move_req{id, (float)dist_req/speed, to};
    return move_req;
}

void Unit::print() const {
    std::cout << "------Unit: " << id << " ---------\n";
    std::cout << "range: " << range << "\n";
    std::cout << "speed: " << (int)speed << "\n";
    std::cout << "current postion -> x: " << cur_pos.x << " y: " << cur_pos.y << "\n";
}