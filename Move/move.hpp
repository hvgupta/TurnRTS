#ifndef _MOVE
#define _MOVE 1

#pragma once

#include <stdint.h>

struct Coord{
    int x;
    int y;
};

struct MoveRequest {
    uint16_t unit_id;
    float time;
    Coord to;
};

#endif