#include <unit.hpp>

Result<MoveRequest, std::string> Unit::make_movement(Coord to){
    uint16_t dist_req = std::abs(cur_pos.x - to.x) + std::abs(cur_pos.y - to.y);
    if (dist_req > range){
        return Result<MoveRequest, std::string>::from_err("The movement is more then the range of the unit");
    }

    MoveRequest move_req{id, (float)dist_req/speed, to};
    return Result<MoveRequest, std::string>::from_output(move_req);
}