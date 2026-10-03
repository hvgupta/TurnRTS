#ifndef _MOVE
#define _MOVE 1

#pragma once

#include <stdint.h>
#include <iostream>

struct Coord{
    int x;
    int y;

    void print() const {
        std::cout << "(" << x << "," << y << ")";
    }
    bool operator==(const Coord& other) const {
        return x == other.x && y == other.y;
    }
};

namespace std {
    template <>
    struct hash<Coord> {
        std::size_t operator()(const Coord& c) const noexcept {
            std::size_t h1 = std::hash<int>{}(c.x);
            std::size_t h2 = std::hash<int>{}(c.y);
            return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
        }
    };
}

struct MoveRequest {
    uint64_t unit_id;
    float time;
    Coord to;

    void print() const {
        std::cout << "unit (" << unit_id << ") will take "<< time << " to reach ";
        to.print();
        std::cout << "\n";
    }
};

Coord one_step_back(Coord from, Coord to);

#endif