#ifndef	_UNIT
#define _UNIT 1

#pragma once

#include <stdint.h>
#include <string>
#include <time.h>
#include <expected>
#include <optional>

#include "move.hpp"

class Unit
{
private:
    uint64_t _id;
    uint16_t range;
    uint8_t speed;
    Coord cur_pos;
    bool _isDestroyed;

public:
    Unit(uint16_t range, uint8_t speed, Coord cur_pos);
    
    const uint64_t &get_id() const;
    std::optional<Coord> get_coord() const;
    bool isDestroyed() const;

    void print() const;
    std::expected<MoveRequest, std::string> build_movement_request(Coord to) const;
};

#endif