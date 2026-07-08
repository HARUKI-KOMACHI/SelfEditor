#include "ChartIO.h"
#include <fstream>
#include <nlohmann/json.hpp>

using json  = nlohmann::json;
using ojson = nlohmann::ordered_json;

// ---- 文字列変換ヘルパー ----

static std::string eventTypeToString(EventType t)
{
    switch (t)
    {
    case EventType::Enemy:   return "Enemy";
    case EventType::Hold:    return "Hold";
    case EventType::Orb:     return "Orb";
    case EventType::Barrier: return "Barrier";
    case EventType::Rainbow: return "Rainbow";
    default:                 return "Tap";
    }
}

static EventType stringToEventType(const std::string& s)
{
    if (s == "Hold")    return EventType::Hold;
    if (s == "Orb")     return EventType::Orb;
    if (s == "Barrier") return EventType::Barrier;
    if (s == "Rainbow") return EventType::Rainbow;
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
    ojson j;
    j["musicname"]   = chart.musicname;
    j["musicauthor"] = chart.musicauthor;
    j["scoreauthor"] = chart.scoreauthor;
    j["difficulty"]  = chart.difficulty;
    j["thumbnail"]   = chart.thumbnail;
    j["bpm"]         = chart.bpm;
    j["music"]       = chart.musicPath;

    ojson eventsJson = ojson::array();
    for (const auto& e : chart.events)
    {
        ojson ej;
        ej["beat"] = e.beat;
        ej["type"] = eventTypeToString(e.type);
        ej["wall"] = wallToString(e.wall);
        ej["lane"] = e.lane;

        if (e.type == EventType::Hold)
        {
            ej["endBeat"] = e.endBeat;
            ej["endLane"] = e.endLane;
        }
        if (e.type == EventType::Rainbow)
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

    out.musicname   = j.value("musicname",   "");
    out.musicauthor = j.value("musicauthor", "");
    out.scoreauthor = j.value("scoreauthor", "");
    out.difficulty  = j.value("difficulty",  0.0f);
    out.thumbnail   = j.value("thumbnail",   "");
    out.bpm         = j.value("bpm",         120.0f);
    out.musicPath   = j.value("music",       "");
    out.events.clear();

    for (const auto& ej : j.value("events", json::array()))
    {
        Event e;
        e.beat = ej.value("beat", 0.0f);
        e.type = stringToEventType(ej.value("type", "Enemy"));
        e.wall = stringToWall(ej.value("wall", "Up"));
        e.lane = ej.value("lane", 0);

        if (e.type == EventType::Hold)
        {
            e.endBeat = ej.value("endBeat", e.beat);
            e.endLane = ej.value("endLane", e.lane);
            e.endWall = e.wall;
        }
        else if (e.type == EventType::Rainbow)
        {
            e.endBeat = ej.value("endBeat", e.beat);
            e.endWall = stringToWall(ej.value("endWall", wallToString(e.wall)));
            e.endLane = 1;
        }
        else
        {
            e.endBeat = e.beat;
            e.endWall = e.wall;
            e.endLane = e.lane;
        }

        out.events.push_back(e);
    }

    return true;
}
