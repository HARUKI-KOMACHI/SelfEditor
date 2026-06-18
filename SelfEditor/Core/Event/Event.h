#pragma once
#include "EventType.h"

struct Event
{
    float     beat    = 0.0f;
    float     endBeat = 0.0f;  // Hold のみ使用

    EventType type    = EventType::Enemy;

    Wall      wall    = Wall::Up;
    Wall      endWall = Wall::Up;  // Rainbow のみ使用（壁またぎ対応）
    int       lane    = 0;         // 0-2 (wall 内のレーン番号)、Rainbow は常に 1
    int       endLane = 0;         // Hold のみ使用（レーンまたぎ対応）
};
