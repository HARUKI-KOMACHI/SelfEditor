#pragma once
#include <string>
#include <vector>
#include "../Core/Chart/Chart.h"
#include "../Core/Audio/AudioPlayer.h"
#include "../Core/Audio/SePlayer.h"

class TimelineEditor
{
public:
    void render();

private:
    void renderMenuBar();
    void renderControls();
    void renderTimeline();
    void buildWaveform();

    void toggleEvent(float beat, Wall wall, int lane);
    void pushUndo();

    Chart            m_chart;
    EventType        m_selectedType = EventType::Enemy;

    float       m_scrollBeat  = 0.0f;
    float       m_zoomBeats   = 16.0f;
    float       m_snapBeat    = 0.25f;
    float       m_offsetSec   = 0.0f;
    std::string m_filePath    = "chart.json";
    std::string m_statusMsg;
    bool        m_statusOk    = true;

    AudioPlayer m_audio;
    std::string m_musicPath;
    char        m_musicBuf[256]    = {};
    bool        m_draggingPlayhead = false;

    // スロー再生速度 (0=0.25x, 1=0.5x, 2=0.75x, 3=1.0x)
    int   m_speedIdx  = 3;
    float m_bgmVolume = 1.0f;

    // 波形表示
    struct WaveformPeak { float minVal; float maxVal; };
    std::vector<WaveformPeak> m_waveformPeaks;
    static constexpr int kWaveChunkFrames = 512;

    // SE 再生
    // 1フレームで進む beat 量がこれを超えたらシークとみなして SE をスキップする
    static constexpr float kSeekThresholdBeats = 4.0f;
    SePlayer m_sePlayer;
    bool     m_seInitialized   = false;
    float    m_lastPlayheadBeat = -1.0f;

    // シーク位置マーカー (-1.0 = 未設定)
    float m_markerBeat = -1.0f;

    // Hold 2クリック配置用
    bool  m_holdPending   = false;
    float m_holdStartBeat = 0.0f;
    Wall  m_holdStartWall = Wall::Up;

    std::vector<std::vector<Event>> m_undoStack;
    std::vector<std::vector<Event>> m_redoStack;
};
