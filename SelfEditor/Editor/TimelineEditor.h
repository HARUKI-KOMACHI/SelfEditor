#pragma once
#include <string>
#include <vector>
#include "../Core/Chart/Chart.h"
#include "../Core/Audio/AudioPlayer.h"

class TimelineEditor
{
public:
    void render();

private:
    void renderMenuBar();
    void renderControls();
    void renderTimeline();

    void toggleEvent(float beat, Wall wall, int lane);
    void pushUndo();

    Chart            m_chart;
    EventType        m_selectedType = EventType::Tap;

    float       m_scrollBeat  = 0.0f;
    float       m_zoomBeats   = 16.0f;
    float       m_snapBeat    = 0.25f;
    float       m_offsetSec   = 0.0f;
    std::string m_filePath    = "chart.json";
    std::string m_statusMsg;
    bool        m_statusOk    = true;

    AudioPlayer m_audio;
    std::string m_musicPath;

    // Hold 2クリック配置用
    bool  m_holdPending   = false;
    float m_holdStartBeat = 0.0f;
    Wall  m_holdStartWall = Wall::Up;

    std::vector<std::vector<Event>> m_undoStack;
    std::vector<std::vector<Event>> m_redoStack;
};
