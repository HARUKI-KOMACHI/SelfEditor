#include "ChartIO.h"
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// ---- 文字列変換ヘルパー ----

static std::string eventTypeToString(EventType t)
{
    switch (t)
    {
    case EventType::Enemy:    return "Tap";
    case EventType::Hold:   return "Hold";
    case EventType::Orb:    return "Orb";
    case EventType::Barrier: return "Barrier";
    default:                return "Tap";
    }
}

static EventType stringToEventType(const std::string& s)
{
    if (s == "Hold")   return EventType::Hold;
    if (s == "Orb")    return EventType::Orb;
    if (s == "Barrier") return EventType::Barrier;
    return EventType::Enemy;
}

static std::string wallToString(Wall w)
{
    switch (w)
    {
    case Wall::Up:    return "Up";
    case Wall::Left:  return "Left";
    case Wall::Down:  return "Down";
    case Wall::Right: return "Right";
    default:          return "Up";
    }
}

static Wall stringToWall(const std::string& s)
{
    if (s == "Left")  return Wall::Left;
    if (s == "Down")  return Wall::Down;
    if (s == "Right") return Wall::Right;
    return Wall::Up;
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
        ej["wall"] = wallToString(e.wall);
        ej["lane"] = e.lane;

        if (e.type == EventType::Hold)
        {
            ej["endBeat"] = e.endBeat;
            ej["endWall"] = wallToString(e.endWall);
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
        e.type = stringToEventType(ej.value("type", "Tap"));
        e.wall = stringToWall(ej.value("wall", "Up"));
        e.lane = ej.value("lane", 0);

        if (e.type == EventType::Hold)
        {
            e.endBeat = ej.value("endBeat", e.beat);
            e.endWall = stringToWall(ej.value("endWall", wallToString(e.wall)));
        }
        else
        {
            e.endBeat = e.beat;
            e.endWall = e.wall;
        }

        out.events.push_back(e);
    }

    return true;
}
