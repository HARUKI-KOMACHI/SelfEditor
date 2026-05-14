#include "ChartIO.h"
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// ---- 文字列変換ヘルパー ----

static std::string eventTypeToString(EventType t)
{
    switch (t)
    {
    case EventType::Enemy:    return "Enemy";
    case EventType::Obstacle: return "Obstacle";
    case EventType::Gravity:  return "Gravity";
    case EventType::Jump:     return "Jump";
    default:                  return "Enemy";
    }
}

static EventType stringToEventType(const std::string& s)
{
    if (s == "Obstacle") return EventType::Obstacle;
    if (s == "Gravity")  return EventType::Gravity;
    if (s == "Jump")     return EventType::Jump;
    return EventType::Enemy;
}

static std::string wallToString(Wall w)
{
    switch (w)
    {
    case Wall::Up:    return "Up";
    case Wall::Down:  return "Down";
    case Wall::Left:  return "Left";
    case Wall::Right: return "Right";
    default:          return "Up";
    }
}

static Wall stringToWall(const std::string& s)
{
    if (s == "Down")  return Wall::Down;
    if (s == "Left")  return Wall::Left;
    if (s == "Right") return Wall::Right;
    return Wall::Up;
}

static std::string gravityDirToString(GravityDirection d)
{
    switch (d)
    {
    case GravityDirection::Up:    return "Up";
    case GravityDirection::Down:  return "Down";
    case GravityDirection::Left:  return "Left";
    case GravityDirection::Right: return "Right";
    default:                      return "Down";
    }
}

static GravityDirection stringToGravityDir(const std::string& s)
{
    if (s == "Up")    return GravityDirection::Up;
    if (s == "Left")  return GravityDirection::Left;
    if (s == "Right") return GravityDirection::Right;
    return GravityDirection::Down;
}

// ---- 保存 ----

bool ChartIO::save(const Chart& chart, const std::string& filePath)
{
    json j;
    j["music"] = chart.musicPath;
    j["bpm"]   = chart.bpm;

    json eventsJson = json::array();
    for (const auto& e : chart.events)
    {
        json ej;
        ej["beat"] = e.beat;
        ej["type"] = eventTypeToString(e.type);

        if (e.type == EventType::Gravity)
        {
            ej["direction"] = gravityDirToString(e.gravityDir);
        }
        else
        {
            ej["wall"] = wallToString(e.wall);
            ej["lane"] = e.lane;
        }

        eventsJson.push_back(ej);
    }
    j["events"] = eventsJson;

    std::ofstream file(filePath);
    if (!file.is_open()) return false;

    file << j.dump(2);
    return file.good();
}

// ---- 読み込み ----

bool ChartIO::load(const std::string& filePath, Chart& out)
{
    std::ifstream file(filePath);
    if (!file.is_open()) return false;

    json j;
    try
    {
        file >> j;
    }
    catch (...)
    {
        return false;
    }

    out.musicPath = j.value("music", "");
    out.bpm       = j.value("bpm", 120.0f);
    out.events.clear();

    for (const auto& ej : j.value("events", json::array()))
    {
        Event e;
        e.beat = ej.value("beat", 0.0f);
        e.type = stringToEventType(ej.value("type", "Enemy"));

        if (e.type == EventType::Gravity)
        {
            e.gravityDir = stringToGravityDir(ej.value("direction", "Down"));
        }
        else
        {
            e.wall = stringToWall(ej.value("wall", "Up"));
            e.lane = ej.value("lane", 0);
        }

        out.events.push_back(e);
    }

    return true;
}
