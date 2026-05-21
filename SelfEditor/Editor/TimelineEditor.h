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
    EventType        m_selectedType = EventType::Enemy;
    GravityDirection m_selectedDir  = GravityDirection::Down;

    float       m_scrollBeat  = 0.0f;
    float       m_zoomBeats   = 16.0f;
    float       m_snapBeat    = 0.25f;
    std::string m_filePath    = "chart.json";
    std::string m_statusMsg;
    bool        m_statusOk    = true;

    AudioPlayer m_audio;
    std::string m_musicPath;
    char        m_musicBuf[256]    = {};
    bool        m_draggingPlayhead = false;

    std::vector<std::vector<Event>> m_undoStack;
    std::vector<std::vector<Event>> m_redoStack;
};
