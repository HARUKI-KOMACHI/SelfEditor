#pragma once
#include <string>
#include <vector>
#include "../Event/Event.h"

struct Chart
{
    std::string        musicPath;
    float              bpm = 120.0f;
    std::vector<Event> events;
};
