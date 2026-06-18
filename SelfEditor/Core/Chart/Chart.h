#pragma once
#include <string>
#include <vector>
#include "../Event/Event.h"

struct Chart
{
    std::string        musicname;
    std::string        musicauthor;
    std::string        scoreauthor;
    float              difficulty = 0.0f;
    std::string        thumbnail;
    float              bpm = 120.0f;
    std::string        musicPath;
    std::vector<Event> events;
};
