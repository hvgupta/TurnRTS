#ifndef	_UNIT
#define _UNIT 1

#pragma GCC system_header

#include <stdint.h>
#include <move.hpp>
#include <result.hpp>
#include <string>
#include <time.h>
#include <optional>

class Unit
{
private:
    uint16_t id;
    uint16_t range;
    uint8_t speed;
    Coord cur_pos;

public:
    Unit(uint16_t range, uint8_t speed, Coord cur_pos) : id((uint16_t)time(NULL)), range(range), speed(speed), cur_pos(cur_pos) {}

    Result<MoveRequest, std::string> make_movement(Coord to);
};

#endif