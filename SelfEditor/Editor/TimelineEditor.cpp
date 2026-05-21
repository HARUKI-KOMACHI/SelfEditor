#include "TimelineEditor.h"
#include <imgui.h>
#include <algorithm>
#include "algorithm"
#include <cmath>
#include "../Core/Chart/ChartIO.h"
#include "../Core/Timing/Timing.h"

// ---- ヘルパー ----

static const char* wallLabel(Wall w)
{
    switch (w) {
    case Wall::Up:    return "Up";
    case Wall::Down:  return "Dn";
    case Wall::Left:  return "Lt";
    case Wall::Right: return "Rt";
    default:          return "??";
    }
}

static ImU32 eventColor(EventType t)
{
    switch (t) {
    case EventType::Enemy:    return IM_COL32(255,  80,  80, 220);
    case EventType::Obstacle: return IM_COL32(255, 160,  30, 220);
    case EventType::Gravity:  return IM_COL32(180,  80, 255, 220);
    case EventType::Jump:     return IM_COL32( 60, 220, 100, 220);
    default:                  return IM_COL32(255, 255, 255, 220);
    }
}

static const ImU32 kWallBg[4] = {
    IM_COL32(35, 65, 35, 255),   // Up    - 緑
    IM_COL32(65, 35, 35, 255),   // Down  - 赤
    IM_COL32(35, 35, 65, 255),   // Left  - 青
    IM_COL32(55, 50, 30, 255),   // Right - 黄
};
static const ImU32 kWallBgAlt[4] = {
    IM_COL32(40, 72, 40, 255),
    IM_COL32(72, 40, 40, 255),
    IM_COL32(40, 40, 72, 255),
    IM_COL32(62, 57, 35, 255),
};

// ---- render ----

void TimelineEditor::render()
{
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);
    ImGui::Begin("##editor", nullptr,
        ImGuiWindowFlags_NoTitleBar        | ImGuiWindowFlags_NoResize   |
        ImGuiWindowFlags_NoMove            | ImGuiWindowFlags_MenuBar    |
        ImGuiWindowFlags_NoBringToFrontOnFocus                           |
        ImGuiWindowFlags_NoScrollbar       | ImGuiWindowFlags_NoScrollWithMouse);

    // Undo / Redo
    if (ImGui::GetIO().KeyCtrl)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_Z) && !m_undoStack.empty())
        {
            m_redoStack.push_back(m_chart.events);
            m_chart.events = m_undoStack.back();
            m_undoStack.pop_back();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Y) && !m_redoStack.empty())
        {
            m_undoStack.push_back(m_chart.events);
            m_chart.events = m_redoStack.back();
            m_redoStack.pop_back();
        }
    }

    renderMenuBar();
    renderControls();
    renderTimeline();

    if (!m_statusMsg.empty())
    {
        if (m_statusOk)
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "%s", m_statusMsg.c_str());
        else
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", m_statusMsg.c_str());
    }

    ImGui::End();
}

void TimelineEditor::renderMenuBar()
{
    if (!ImGui::BeginMenuBar()) return;

    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem("New"))
        {
            m_chart     = Chart();
            m_statusMsg = "New chart.";
            m_statusOk  = true;
        }
        if (ImGui::MenuItem("Open"))
        {
            Chart tmp;
            if (ChartIO::load(m_filePath, tmp))
            {
                m_chart = tmp;
                m_statusMsg = "Loaded: " + m_filePath;
                m_statusOk  = true;

                if (!m_chart.musicPath.empty())
                {
                    snprintf(m_musicBuf, sizeof(m_musicBuf), "%s", m_chart.musicPath.c_str());
                    m_musicPath = "Assets/music/" + m_chart.musicPath;
                    if (!m_audio.load(m_musicPath))
                        m_statusMsg += "  (music load failed)";
                }
            }
            else { m_statusMsg = "Load failed: " + m_filePath; m_statusOk = false; }
        }
        if (ImGui::MenuItem("Save"))
        {
            if (ChartIO::save(m_chart, m_filePath)) { m_statusMsg = "Saved: " + m_filePath; m_statusOk = true; }
            else                                    { m_statusMsg = "Save failed: " + m_filePath; m_statusOk = false; }
        }
        ImGui::EndMenu();
    }

    ImGui::EndMenuBar();
}

void TimelineEditor::renderControls()
{
    // ファイルパス
    static char pathBuf[256] = "chart.json";
    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::InputText("File", pathBuf, sizeof(pathBuf)))
        m_filePath = pathBuf;

    ImGui::SameLine();

    // BPM
    ImGui::SetNextItemWidth(80.0f);
    ImGui::InputFloat("BPM", &m_chart.bpm, 1.0f, 10.0f, "%.1f");
    if (m_chart.bpm < 1.0f) m_chart.bpm = 1.0f;

    ImGui::SameLine();

    // スナップ
    const char* snapLabels[] = { "1 beat", "1/2", "1/4", "1/8" };
    const float snapValues[] = { 1.0f, 0.5f, 0.25f, 0.125f };
    int snapIdx = 2;
    for (int i = 0; i < 4; ++i)
        if (fabsf(m_snapBeat - snapValues[i]) < 0.001f) { snapIdx = i; break; }
    ImGui::SetNextItemWidth(80.0f);
    if (ImGui::Combo("Snap", &snapIdx, snapLabels, 4))
        m_snapBeat = snapValues[snapIdx];

    ImGui::SameLine();

    // ズーム
    ImGui::SetNextItemWidth(120.0f);
    ImGui::SliderFloat("Zoom", &m_zoomBeats, 4.0f, 64.0f, "%.0f beats");

    ImGui::SameLine();

    // イベントタイプ
    const char* typeLabels[] = { "Enemy", "Obstacle", "Gravity", "Jump" };
    int typeIdx = static_cast<int>(m_selectedType);
    ImGui::SetNextItemWidth(90.0f);
    if (ImGui::Combo("Type", &typeIdx, typeLabels, 4))
        m_selectedType = static_cast<EventType>(typeIdx);

    if (m_selectedType == EventType::Gravity)
    {
        ImGui::SameLine();
        const char* dirLabels[] = { "Up", "Down", "Left", "Right" };
        int dirIdx = static_cast<int>(m_selectedDir);
        ImGui::SetNextItemWidth(80.0f);
        if (ImGui::Combo("Dir", &dirIdx, dirLabels, 4))
            m_selectedDir = static_cast<GravityDirection>(dirIdx);
    }

    // 操作説明
    ImGui::SameLine();
    ImGui::TextDisabled("  LClick=Place/Toggle  RClick=Delete  Wheel=Scroll");

    // ---- 2行目: 音楽 & トランスポート ----
    ImGui::SetNextItemWidth(220.0f);
    ImGui::InputText("Music", m_musicBuf, sizeof(m_musicBuf));
    ImGui::SameLine();
    if (ImGui::Button("Load"))
    {
        m_musicPath = "Assets/music/" + std::string(m_musicBuf);
        if (m_audio.load(m_musicPath))
        {
            m_chart.musicPath = std::string(m_musicBuf);
            m_statusMsg = "Loaded: " + m_musicPath;
            m_statusOk  = true;
        }
        else
        {
            m_statusMsg = "Load failed: " + m_musicPath;
            m_statusOk  = false;
        }
    }

    ImGui::SameLine();
    ImGui::Spacing(); ImGui::SameLine();

    // 再生 / 一時停止
    if (m_audio.isPlaying())
    {
        if (ImGui::Button("  ||  ")) m_audio.pause();
    }
    else
    {
        if (ImGui::Button("  >   ")) m_audio.play();
    }
    ImGui::SameLine();
    if (ImGui::Button("  []  ")) m_audio.stop();

    // 現在時刻と beat 表示
    if (m_audio.isLoaded())
    {
        float t    = m_audio.currentTimeSeconds();
        float dur  = m_audio.durationSeconds();
        float beat = Timing::secondsToBeat(t, m_chart.bpm);
        ImGui::SameLine();
        ImGui::Text("%.2f / %.2f sec   beat: %.2f", t, dur, beat);
    }
}

void TimelineEditor::renderTimeline()
{
    const float labelWidth   = 52.0f;
    const float rowHeight    = 26.0f;
    const float headerHeight = 22.0f;
    const int   numLanes     = 12;

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetStyle().ItemSpacing.y);
    ImVec2 origin    = ImGui::GetCursorScreenPos();
    float  tlWidth   = ImGui::GetContentRegionAvail().x - labelWidth;
    float  totalH    = headerHeight + rowHeight * numLanes;

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // 背景
    dl->AddRectFilled(origin,
        ImVec2(origin.x + labelWidth + tlWidth, origin.y + totalH),
        IM_COL32(25, 25, 25, 255));

    // beat -> pixel
    auto beatToX = [&](float beat) -> float {
        return origin.x + labelWidth + (beat - m_scrollBeat) / m_zoomBeats * tlWidth;
    };

    // ---- 行背景 & ラベル ----
    for (int wi = 0; wi < 4; ++wi)
    {
        for (int li = 0; li < 3; ++li)
        {
            int   row  = wi * 3 + li;
            float rowY = origin.y + headerHeight + row * rowHeight;

            dl->AddRectFilled(
                ImVec2(origin.x, rowY),
                ImVec2(origin.x + labelWidth + tlWidth, rowY + rowHeight),
                (li % 2 == 0) ? kWallBg[wi] : kWallBgAlt[wi]);

            char lbl[12];
            snprintf(lbl, sizeof(lbl), "%s-%d", wallLabel(static_cast<Wall>(wi)), li);
            dl->AddText(ImVec2(origin.x + 4, rowY + 5), IM_COL32(200, 200, 200, 255), lbl);
        }
    }

    // ---- グリッド線 ----
    float beatStep  = m_snapBeat;
    int   firstTick = (int)floorf(m_scrollBeat / beatStep);
    float lastBeat  = m_scrollBeat + m_zoomBeats + beatStep;

    for (int ti = firstTick; ti * beatStep <= lastBeat; ++ti)
    {
        float beat = ti * beatStep;
        float x    = beatToX(beat);
        if (x < origin.x + labelWidth || x > origin.x + labelWidth + tlWidth) continue;

        bool isMeasure = (fabsf(fmodf(beat, 4.0f)) < 0.001f);
        bool isBeat    = (fabsf(fmodf(beat, 1.0f)) < 0.001f);

        ImU32 col = isMeasure ? IM_COL32(210, 210, 210, 130)
                  : isBeat    ? IM_COL32(160, 160, 160,  80)
                              : IM_COL32(100, 100, 100,  45);
        float thickness = isMeasure ? 1.5f : 1.0f;

        dl->AddLine(ImVec2(x, origin.y),
                    ImVec2(x, origin.y + totalH), col, thickness);

        // ヘッダーに小節番号を表示 (4beat ごと)
        if (isMeasure)
        {
            char buf[16];
            snprintf(buf, sizeof(buf), "%d", (int)(beat / 4) + 1);
            dl->AddText(ImVec2(x + 3, origin.y + 3), IM_COL32(210, 210, 210, 220), buf);
        }
    }

    // 行区切り線
    for (int row = 0; row <= numLanes; ++row)
    {
        float y = origin.y + headerHeight + row * rowHeight;
        bool isWallBorder = (row % 3 == 0);
        dl->AddLine(ImVec2(origin.x, y),
                    ImVec2(origin.x + labelWidth + tlWidth, y),
                    isWallBorder ? IM_COL32(180, 180, 180, 80) : IM_COL32(80, 80, 80, 60));
    }

    // ---- イベント描画 ----
    for (const auto& e : m_chart.events)
    {
        float x = beatToX(e.beat);
        if (x < origin.x + labelWidth - 8 || x > origin.x + labelWidth + tlWidth + 8) continue;

        ImU32 col = eventColor(e.type);

        if (e.type == EventType::Gravity)
        {
            float yTop = origin.y + headerHeight;
            float yBot = yTop + rowHeight * numLanes;
            dl->AddLine(ImVec2(x, yTop), ImVec2(x, yBot), col, 2.0f);
            dl->AddCircleFilled(ImVec2(x, yTop + 8), 5.0f, col);
        }
        else
        {
            int   row = static_cast<int>(e.wall) * 3 + e.lane;
            float cy  = origin.y + headerHeight + (row + 0.5f) * rowHeight;
            dl->AddCircleFilled(ImVec2(x, cy), rowHeight * 0.32f, col);
        }
    }

    // ---- シークバードラッグ ----
    if (m_audio.isLoaded())
    {
        float phBeat = Timing::secondsToBeat(m_audio.currentTimeSeconds(), m_chart.bpm);
        float phX    = beatToX(phBeat);
        ImVec2 mp    = ImGui::GetIO().MousePos;

        bool inTimeline = mp.x >= origin.x + labelWidth && mp.x <= origin.x + labelWidth + tlWidth
                       && mp.y >= origin.y              && mp.y <= origin.y + totalH;

        if (!m_draggingPlayhead && inTimeline && fabsf(mp.x - phX) <= 8.0f
            && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            m_draggingPlayhead = true;
        }

        if (m_draggingPlayhead)
        {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                float relX    = std::max(0.0f, std::min(mp.x - (origin.x + labelWidth), tlWidth));
                float beat    = m_scrollBeat + (relX / tlWidth) * m_zoomBeats;
                float seekSec = Timing::beatToSeconds(std::max(0.0f, beat), m_chart.bpm);
                m_audio.seekSeconds(std::min(seekSec, m_audio.durationSeconds()));

                // エッジスクロール
                const float edgeZone    = 60.0f;
                const float scrollSpeed = m_zoomBeats * 2.0f;
                float dt      = ImGui::GetIO().DeltaTime;
                float tlLeft  = origin.x + labelWidth;
                float tlRight = tlLeft + tlWidth;

                if (mp.x < tlLeft + edgeZone)
                {
                    float t = 1.0f - (mp.x - tlLeft) / edgeZone;
                    m_scrollBeat = std::max(0.0f, m_scrollBeat - scrollSpeed * t * dt);
                }
                else if (mp.x > tlRight - edgeZone)
                {
                    float t = (mp.x - (tlRight - edgeZone)) / edgeZone;
                    m_scrollBeat += scrollSpeed * t * dt;
                }
            }
            else
            {
                m_draggingPlayhead = false;
            }
        }
    }

    // ---- マウス操作 ----
    ImGui::SetCursorScreenPos(ImVec2(origin.x + labelWidth, origin.y + headerHeight));
    ImGui::InvisibleButton("##tl", ImVec2(tlWidth, rowHeight * numLanes));

    if (ImGui::IsItemHovered())
    {
        // ホイール -> スクロール
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f)
            m_scrollBeat = std::max(0.0f, m_scrollBeat - wheel * m_snapBeat * 4.0f);

        ImVec2 mp   = ImGui::GetIO().MousePos;
        float  relX = mp.x - (origin.x + labelWidth);
        float  relY = mp.y - (origin.y + headerHeight);

        float beat = m_scrollBeat + (relX / tlWidth) * m_zoomBeats;
        beat = roundf(beat / m_snapBeat) * m_snapBeat;
        if (beat < 0.0f) beat = 0.0f;

        int row = (int)(relY / rowHeight);
        if (row >= 0 && row < numLanes)
        {
            Wall clickWall = static_cast<Wall>(row / 3);
            int  clickLane = row % 3;

            if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !m_draggingPlayhead)
            {
                pushUndo();
                toggleEvent(beat, clickWall, clickLane);
            }

            if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
            {
                pushUndo();
                auto& ev = m_chart.events;
                ev.erase(std::remove_if(ev.begin(), ev.end(), [&](const Event& e) {
                    if (e.type == EventType::Gravity)
                        return fabsf(e.beat - beat) < m_snapBeat * 0.5f;
                    return fabsf(e.beat - beat) < m_snapBeat * 0.5f
                        && e.wall == clickWall && e.lane == clickLane;
                }), ev.end());
            }
        }
    }

    // ---- 再生ヘッド ----
    if (m_audio.isLoaded())
    {
        float timeSec      = m_audio.currentTimeSeconds();
        float playheadBeat = Timing::secondsToBeat(timeSec, m_chart.bpm);
        float px           = beatToX(playheadBeat);

        if (px >= origin.x + labelWidth && px <= origin.x + labelWidth + tlWidth)
        {
            // 縦線
            dl->AddLine(ImVec2(px, origin.y + headerHeight),
                        ImVec2(px, origin.y + totalH),
                        IM_COL32(255, 220, 0, 220), 2.0f);
            // 上部の三角マーカー
            dl->AddTriangleFilled(
                ImVec2(px - 6, origin.y),
                ImVec2(px + 6, origin.y),
                ImVec2(px,     origin.y + 12),
                IM_COL32(255, 220, 0, 220));
        }

        // 再生中: 再生ヘッドが画面端に近づいたら自動スクロール
        if (m_audio.isPlaying())
        {
            if (playheadBeat > m_scrollBeat + m_zoomBeats * 0.85f)
                m_scrollBeat = playheadBeat - m_zoomBeats * 0.1f;
            else if (playheadBeat < m_scrollBeat)
                m_scrollBeat = std::max(0.0f, playheadBeat - m_zoomBeats * 0.1f);
        }
    }

    // カーソルを canvas の下に移動
    ImGui::Dummy(ImVec2(labelWidth + tlWidth, totalH));
}

void TimelineEditor::pushUndo()
{
    m_undoStack.push_back(m_chart.events);
    m_redoStack.clear();
    if (m_undoStack.size() > 100)
        m_undoStack.erase(m_undoStack.begin());
}

void TimelineEditor::toggleEvent(float beat, Wall wall, int lane)
{
    auto& ev = m_chart.events;

    if (m_selectedType == EventType::Gravity)
    {
        auto it = std::find_if(ev.begin(), ev.end(), [&](const Event& e) {
            return e.type == EventType::Gravity && fabsf(e.beat - beat) < m_snapBeat * 0.5f;
        });
        if (it != ev.end()) { ev.erase(it); return; }

        Event e;
        e.beat       = beat;
        e.type       = EventType::Gravity;
        e.gravityDir = m_selectedDir;
        ev.push_back(e);
        return;
    }

    auto it = std::find_if(ev.begin(), ev.end(), [&](const Event& e) {
        return e.type != EventType::Gravity
            && fabsf(e.beat - beat) < m_snapBeat * 0.5f
            && e.wall == wall && e.lane == lane;
    });
    if (it != ev.end()) { ev.erase(it); return; }

    Event e;
    e.beat = beat;
    e.type = m_selectedType;
    e.wall = wall;
    e.lane = lane;
    ev.push_back(e);
}
