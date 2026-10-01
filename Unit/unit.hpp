#ifndef	_UNIT
#define _UNIT 1

#pragma once

#include <stdint.h>
#include <string>
#include <time.h>
#include <expected>

#include "move.hpp"

class Unit
{
private:
    uint16_t id;
    uint16_t range;
    uint8_t speed;
    Coord cur_pos;

public:
    Unit(uint16_t range, uint8_t speed, Coord cur_pos) : id((uint16_t)time(NULL)), range(range), speed(speed), cur_pos(cur_pos) {}
    void print() const;
    std::expected<MoveRequest, std::string> make_movement(Coord to);
};

#endif