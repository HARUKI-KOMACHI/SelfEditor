#include "TimelineEditor.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include "../Core/Chart/ChartIO.h"
#include "../Core/Timing/Timing.h"

// ---- ヘルパー ----

static const char* wallLabel(Wall w)
{
    switch (w) {
    case Wall::Up:    return "Up";
    case Wall::Left:  return "Lt";
    case Wall::Down:  return "Dn";
    case Wall::Right: return "Rt";
    default:          return "??";
    }
}

static ImU32 eventColor(EventType t)
{
    switch (t) {
    case EventType::Tap:    return IM_COL32(255,  80,  80, 220);
    case EventType::Hold:   return IM_COL32( 80, 160, 255, 220);
    case EventType::Orb:    return IM_COL32(255, 220,  50, 220);
    case EventType::Object: return IM_COL32(180,  80, 255, 220);
    default:                return IM_COL32(255, 255, 255, 220);
    }
}

// 壁背景色: Up / Left / Down / Right
static const ImU32 kWallBg[4] = {
    IM_COL32(35, 65, 35, 255),   // Up    - 緑
    IM_COL32(35, 35, 65, 255),   // Left  - 青
    IM_COL32(65, 35, 35, 255),   // Down  - 赤
    IM_COL32(55, 50, 30, 255),   // Right - 黄
};
static const ImU32 kWallBgAlt[4] = {
    IM_COL32(40, 72, 40, 255),
    IM_COL32(40, 40, 72, 255),
    IM_COL32(72, 40, 40, 255),
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

    // Escape で Hold pending キャンセル
    if (ImGui::IsKeyPressed(ImGuiKey_Escape))
        m_holdPending = false;

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
            if (ChartIO::load(m_filePath, tmp)) { m_chart = tmp; m_statusMsg = "Loaded: " + m_filePath; m_statusOk = true; }
            else                                { m_statusMsg = "Load failed: " + m_filePath;            m_statusOk = false; }
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
    ImGui::SetNextItemWidth(90.0f);
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
    const char* typeLabels[] = { "Tap", "Hold", "Orb", "Object" };
    int typeIdx = static_cast<int>(m_selectedType);
    ImGui::SetNextItemWidth(90.0f);
    if (ImGui::Combo("Type", &typeIdx, typeLabels, 4))
    {
        m_selectedType = static_cast<EventType>(typeIdx);
        m_holdPending  = false;  // タイプ変更でキャンセル
    }

    ImGui::SameLine();

    // オフセット
    ImGui::SetNextItemWidth(90.0f);
    ImGui::InputFloat("Offset(s)", &m_offsetSec, 0.01f, 0.1f, "%.3f");

    ImGui::SameLine();

    // 操作説明
    //ImGui::TextDisabled("  LClick=Place/Toggle  RClick=Delete  Wheel=Scroll  Esc=Cancel");

    // ---- 2行目: 音楽 & トランスポート ----
    static char musicBuf[256] = "";
    ImGui::SetNextItemWidth(220.0f);
    ImGui::InputText("Music", musicBuf, sizeof(musicBuf));
    ImGui::SameLine();
    if (ImGui::Button("Load"))
    {
        m_musicPath = "Assets/music/" + std::string(musicBuf);
        if (m_audio.load(m_musicPath))
        {
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
        float t    = m_audio.currentTimeSeconds() - m_offsetSec;
        float dur  = m_audio.durationSeconds();
        float beat = Timing::secondsToBeat(t, m_chart.bpm);
        ImGui::SameLine();
        ImGui::Text("%.2f / %.2f sec   beat: %.2f", t, dur, beat);
    }

    // Hold pending 表示
    if (m_holdPending)
    {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.3f, 0.6f, 1.0f, 1.0f),
            "  [Hold] 始点設定済み (wall=%s beat=%.2f) - 終点をクリック / Esc でキャンセル",
            wallLabel(m_holdStartWall), m_holdStartBeat);
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

    // 壁の行Y範囲
    auto wallRowY = [&](Wall w) -> float {
        return origin.y + headerHeight + static_cast<int>(w) * 3 * rowHeight;
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
    float clipL = origin.x + labelWidth;
    float clipR = origin.x + labelWidth + tlWidth;

    for (const auto& e : m_chart.events)
    {
        float x = beatToX(e.beat);
        ImU32 col = eventColor(e.type);

        if (e.type == EventType::Hold)
        {
            float x2   = beatToX(e.endBeat);
            float sy = wallRowY(e.wall);
            float ey = wallRowY(e.endWall);

            if (e.wall == e.endWall)
            {
                // 同一壁: 横バー
                float y1 = sy + 2;
                float y2 = sy + 3 * rowHeight - 2;
                if (x2 > clipL && x < clipR)
                {
                    dl->AddRectFilled(ImVec2(std::max(x, clipL), y1),
                                      ImVec2(std::min(x2, clipR), y2), col);
                    dl->AddRect(ImVec2(std::max(x, clipL), y1),
                                ImVec2(std::min(x2, clipR), y2),
                                IM_COL32(200, 220, 255, 200), 3.0f);
                }
            }
            else
            {
                // 壁またぎ: 始点上下・終点上下を結んだ四角形（平行四辺形）
                if (x2 > clipL && x < clipR)
                {
                    ImVec2 p1(x,  sy + 2);                   // 始点・上
                    ImVec2 p2(x,  sy + 3 * rowHeight - 2);   // 始点・下
                    ImVec2 p3(x2, ey + 3 * rowHeight - 2);   // 終点・下
                    ImVec2 p4(x2, ey + 2);                   // 終点・上
                    dl->AddQuadFilled(p1, p2, p3, p4, col);
                    dl->AddQuad(p1, p2, p3, p4, IM_COL32(200, 220, 255, 200), 2.0f);
                }
            }
        }
        else
        {
            if (x < clipL - 8 || x > clipR + 8) continue;

            int   row = static_cast<int>(e.wall) * 3 + e.lane;
            float cy  = origin.y + headerHeight + (row + 0.5f) * rowHeight;
            float r   = rowHeight * 0.32f;

            switch (e.type) {
            case EventType::Tap:
                dl->AddCircleFilled(ImVec2(x, cy), r, col);
                break;
            case EventType::Orb:
                dl->AddCircle(ImVec2(x, cy), r, col, 0, 2.5f);
                break;
            case EventType::Object:
                dl->AddRectFilled(ImVec2(x - r, cy - r), ImVec2(x + r, cy + r), col);
                break;
            default:
                dl->AddCircleFilled(ImVec2(x, cy), r, col);
                break;
            }
        }
    }

    // Hold pending フィードバック（始点マーカー）
    if (m_holdPending)
    {
        float px  = beatToX(m_holdStartBeat);
        float sy  = wallRowY(m_holdStartWall);
        if (px >= clipL && px <= clipR)
        {
            dl->AddLine(ImVec2(px, sy),
                        ImVec2(px, sy + 3 * rowHeight),
                        IM_COL32(80, 160, 255, 180), 2.0f);
            dl->AddText(ImVec2(px + 3, sy + 2), IM_COL32(80, 160, 255, 255), "H>");
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

            if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            {
                if (m_selectedType == EventType::Hold)
                {
                    if (!m_holdPending)
                    {
                        // 1回目: 始点記録
                        m_holdPending   = true;
                        m_holdStartBeat = beat;
                        m_holdStartWall = clickWall;
                    }
                    else
                    {
                        // 2回目: Hold 生成（壁またぎ可）
                        pushUndo();
                        Event e;
                        e.type    = EventType::Hold;
                        e.wall    = m_holdStartWall;
                        e.endWall = clickWall;
                        e.lane    = 1;
                        e.beat    = std::min(m_holdStartBeat, beat);
                        e.endBeat = std::max(m_holdStartBeat, beat);
                        m_chart.events.push_back(e);
                        m_holdPending = false;
                    }
                }
                else
                {
                    m_holdPending = false;
                    pushUndo();
                    toggleEvent(beat, clickWall, clickLane);
                }
            }

            if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
            {
                if (m_holdPending)
                {
                    m_holdPending = false;
                }
                else
                {
                    pushUndo();
                    auto& ev = m_chart.events;
                    ev.erase(std::remove_if(ev.begin(), ev.end(), [&](const Event& e) {
                        if (e.type == EventType::Hold)
                        {
                            // クリックした壁が始点か終点に一致し、beat範囲内なら削除
                            bool wallMatch = (e.wall == clickWall || e.endWall == clickWall);
                            bool beatMatch = (beat >= e.beat - m_snapBeat * 0.5f &&
                                             beat <= e.endBeat + m_snapBeat * 0.5f);
                            return wallMatch && beatMatch;
                        }
                        return fabsf(e.beat - beat) < m_snapBeat * 0.5f
                            && e.wall == clickWall && e.lane == clickLane;
                    }), ev.end());
                }
            }
        }
    }

    // ---- 再生ヘッド ----
    if (m_audio.isLoaded())
    {
        float timeSec      = m_audio.currentTimeSeconds() - m_offsetSec;
        float playheadBeat = Timing::secondsToBeat(timeSec, m_chart.bpm);
        float px           = beatToX(playheadBeat);

        if (px >= clipL && px <= clipR)
        {
            dl->AddLine(ImVec2(px, origin.y + headerHeight),
                        ImVec2(px, origin.y + totalH),
                        IM_COL32(255, 220, 0, 220), 2.0f);
            dl->AddTriangleFilled(
                ImVec2(px - 6, origin.y),
                ImVec2(px + 6, origin.y),
                ImVec2(px,     origin.y + 12),
                IM_COL32(255, 220, 0, 220));
        }

        if (m_audio.isPlaying())
        {
            if (playheadBeat > m_scrollBeat + m_zoomBeats * 0.85f)
                m_scrollBeat = playheadBeat - m_zoomBeats * 0.1f;
            else if (playheadBeat < m_scrollBeat)
                m_scrollBeat = std::max(0.0f, playheadBeat - m_zoomBeats * 0.1f);
        }
    }

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

    auto it = std::find_if(ev.begin(), ev.end(), [&](const Event& e) {
        if (e.type == EventType::Hold) return false;
        return fabsf(e.beat - beat) < m_snapBeat * 0.5f
            && e.wall == wall && e.lane == lane;
    });
    if (it != ev.end()) { ev.erase(it); return; }

    Event e;
    e.beat    = beat;
    e.endBeat = beat;
    e.type    = m_selectedType;
    e.wall    = wall;
    e.endWall = wall;
    e.lane    = lane;
    ev.push_back(e);
}
