#pragma once
#include "EventType.h"

struct Event
{
    float            beat       = 0.0f;
    EventType        type       = EventType::Enemy;

    // Enemy / Obstacle / Jump で使用
    Wall             wall       = Wall::Up;
    int              lane       = 0;          // 0-2 (wall 内のレーン番号)

    // Gravity イベントのみ使用
    GravityDirection gravityDir = GravityDirection::Down;
};
