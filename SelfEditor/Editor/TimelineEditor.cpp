#include "TimelineEditor.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <nlohmann/json.hpp>
#include "../Core/Chart/ChartIO.h"
#include "../Core/Timing/Timing.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#pragma comment(lib, "comdlg32.lib")

using json = nlohmann::json;

static constexpr const char* kEditorSettingsPath = "Assets/editor_settings.json";

// ---- ヘルパー ----

// ファイル選択ダイアログ（絶対パスを返す）。OFN_NOCHANGEDIR でカレントディレクトリの変更を防ぐ
// （music/ などの相対パス読み込みに依存しているため）
static bool browseOpenFile(const char* filter, std::string& outPath, const char* initialDir = nullptr)
{
    char buf[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = GetActiveWindow();
    ofn.lpstrFilter = filter;
    ofn.lpstrFile   = buf;
    ofn.nMaxFile    = MAX_PATH;
    ofn.lpstrInitialDir = initialDir;
    ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameA(&ofn)) return false;
    outPath = buf;
    return true;
}

static std::string extractFileName(const std::string& path)
{
    size_t pos = path.find_last_of("\\/");
    return (pos == std::string::npos) ? path : path.substr(pos + 1);
}

// 入力欄の文字列から実際の読み込み/保存パスを組み立てる。
// ファイル名のみ（区切り文字を含まない）なら json/ を自動付加、
// 絶対パス/相対パスらしき文字列（: \ / を含む）ならそのまま使う。
static std::string resolveJsonFilePath(const std::string& buf)
{
    if (buf.find(':') != std::string::npos ||
        buf.find('\\') != std::string::npos ||
        buf.find('/') != std::string::npos)
        return buf;
    return "json/" + buf;
}

// ダイアログで選んだ絶対パスが json/ フォルダ内なら「ファイル名のみ」、
// それ以外なら「絶対パスそのまま」を File 欄の表示用文字列として返す。
static std::string makeFileDisplayPath(const std::string& absPath)
{
    char jsonDirAbs[MAX_PATH] = {};
    GetFullPathNameA("json", MAX_PATH, jsonDirAbs, nullptr);

    size_t pos = absPath.find_last_of("\\/");
    std::string dir = (pos == std::string::npos) ? "" : absPath.substr(0, pos);

    if (_stricmp(dir.c_str(), jsonDirAbs) == 0)
        return extractFileName(absPath);
    return absPath;
}

static const char* seNameForType(EventType t)
{
    switch (t) {
    case EventType::Enemy:   return "Enemy";
    case EventType::Hold:    return "Hold";
    case EventType::Orb:     return "Orb";
    case EventType::Barrier: return "Barrier";
    case EventType::Rainbow: return "Rainbow";
    default:                 return "Enemy";
    }
}

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
    case EventType::Enemy:   return IM_COL32(255,  80,  80, 220);
    case EventType::Hold:    return IM_COL32( 80, 160, 255, 220);
    case EventType::Orb:     return IM_COL32(255, 220,  50, 220);
    case EventType::Barrier: return IM_COL32(180,  80, 255, 220);
    case EventType::Rainbow: return IM_COL32( 50, 255, 180, 220);
    default:                 return IM_COL32(255, 255, 255, 220);
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

    // SE 初期化（初回のみ）
    if (!m_seInitialized)
    {
        m_seInitialized = true;
        m_seVolumes = {
            {"Enemy",   1.0f},
            {"Hold",    1.0f},
            {"Orb",     1.0f},
            {"Barrier", 1.0f},
            {"Rainbow", 1.0f},
        };
        loadEditorSettings();
        if (m_sePlayer.init())
        {
            auto vol = [&](const std::string& n) {
                for (auto& sv : m_seVolumes) if (sv.name == n) return sv.volume;
                return 1.0f;
            };
            m_sePlayer.loadSe("Enemy",   "Assets/SE/EnemySE.wav",   vol("Enemy"));
            m_sePlayer.loadSe("Hold",    "Assets/SE/HoldSE.wav",    vol("Hold"));
            m_sePlayer.loadSe("Orb",     "Assets/SE/OrbSE.wav",     vol("Orb"));
            m_sePlayer.loadSe("Barrier", "Assets/SE/BarrierSE.wav", vol("Barrier"));
            m_sePlayer.loadSe("Rainbow", "Assets/SE/HoldSE.wav", vol("Rainbow"));
        }
    }

    // Undo / Redo / Copy / Paste
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

        // Ctrl+C: 選択範囲のノーツをコピー
        if (ImGui::IsKeyPressed(ImGuiKey_C) && m_selectionActive)
        {
            m_clipboard.clear();
            for (const auto& e : m_chart.events)
            {
                if (e.type == EventType::Hold || e.type == EventType::Rainbow)
                {
                    // Hold/Rainbow は範囲に完全に収まる場合のみコピー
                    if (e.beat >= m_selectStart && e.endBeat <= m_selectEnd)
                        m_clipboard.push_back(e);
                }
                else
                {
                    if (e.beat >= m_selectStart && e.beat <= m_selectEnd)
                        m_clipboard.push_back(e);
                }
            }
            if (!m_clipboard.empty())
            {
                m_statusMsg = std::to_string(m_clipboard.size()) + " note(s) copied.";
                m_statusOk  = true;
            }
        }

        // Ctrl+V: カーソル位置にペースト
        if (ImGui::IsKeyPressed(ImGuiKey_V) && !m_clipboard.empty())
        {
            // カーソルがタイムライン上にあればその位置、なければ選択開始位置
            float pasteBase = (m_hoverBeat >= 0.0f) ? m_hoverBeat : m_selectStart;
            float offset = pasteBase - m_selectStart;
            pushUndo();
            for (auto e : m_clipboard)
            {
                e.beat    += offset;
                e.endBeat += offset;
                if (e.beat >= 0.0f)
                    m_chart.events.push_back(e);
            }
            m_statusMsg = std::to_string(m_clipboard.size()) + " note(s) pasted.";
            m_statusOk  = true;
        }
    }

    // Space: 再生/一時停止トグル
    if (ImGui::IsKeyPressed(ImGuiKey_Space))
    {
        if (m_audio.isPlaying()) m_audio.pause();
        else                     m_audio.play();
    }
    // Enter: 停止して先頭へ戻る
    if (ImGui::IsKeyPressed(ImGuiKey_Enter))
        m_audio.stop();

    // M: 現在位置をマーカーに記録 / Shift+M: マーカー位置へジャンプ
    if (ImGui::IsKeyPressed(ImGuiKey_M))
    {
        if (ImGui::GetIO().KeyShift)
        {
            // Shift+M: マーカーをカーソル位置のbeatへ移動
            if (m_hoverBeat >= 0.0f)
                m_markerBeat = m_hoverBeat;
        }
        else if (m_audio.isLoaded())
        {
            float t = m_audio.currentTimeSeconds() - m_offsetSec;
            m_markerBeat = Timing::secondsToBeat(t, m_chart.bpm);
        }
    }

    // Escape で Hold pending・選択をキャンセル
    if (ImGui::IsKeyPressed(ImGuiKey_Escape))
    {
        m_holdPending    = false;
        m_selectionActive = false;
        m_selectDragging  = false;
    }

    // 終端自動停止
    if (m_audio.isPlaying() &&
        m_audio.currentTimeSeconds() >= m_audio.durationSeconds())
    {
        m_audio.stop();
        if (m_holdLoopActive) { m_sePlayer.stopLoop("Hold"); m_holdLoopActive = false; }
        m_lastPlayheadBeat = -1.0f;
    }

    // SE トリガー処理
    if (m_audio.isLoaded())
    {
        float t    = m_audio.currentTimeSeconds() - m_offsetSec;
        float beat = Timing::secondsToBeat(t, m_chart.bpm);

        if (m_audio.isPlaying() && m_lastPlayheadBeat >= 0.0f)
        {
            float delta = beat - m_lastPlayheadBeat;
            if (delta > 0.0f && delta < kSeekThresholdBeats)
            {
                for (const auto& e : m_chart.events)
                    if (e.beat > m_lastPlayheadBeat && e.beat <= beat)
                        if (e.type != EventType::Hold && e.type != EventType::Rainbow)  // Hold/Rainbow はループで管理
                            m_sePlayer.play(seNameForType(e.type));
            }
        }

        // Hold / Rainbow ループ SE 管理
        bool holdActive = false;
        if (m_audio.isPlaying())
        {
            for (const auto& e : m_chart.events)
            {
                if (e.type == EventType::Hold || e.type == EventType::Rainbow)
                {
                    float s  = std::min(e.beat, e.endBeat);
                    float en = std::max(e.beat, e.endBeat);
                    if (beat >= s && beat <= en) { holdActive = true; break; }
                }
            }
        }
        if (holdActive && !m_holdLoopActive)
        {
            m_sePlayer.playLoop("Hold");
            m_holdLoopActive = true;
        }
        else if (!holdActive && m_holdLoopActive)
        {
            m_sePlayer.stopLoop("Hold");
            m_holdLoopActive = false;
        }

        m_lastPlayheadBeat = beat;
    }

    renderMenuBar();
    renderControls();
    renderTimeline();
    //renderExplain();

    //ImGui::TextDisabled("  LClick=Place/Toggle  RClick=Delete  Wheel=Scroll  Esc=Cancel  Ctrl+Drag=Select  Ctrl+C=Copy  Ctrl+V=Paste@cursor  Header=Seek  M=SetMarker  Shift+M=MoveMarker@cursor  ->M=SeekToMarker");

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
            m_chart = Chart();
            m_offsetSec = m_chart.offset;
            m_statusMsg = "New chart.";
            m_statusOk = true;
        }
        if (ImGui::MenuItem("Open"))
        {
            std::string picked;
            if (browseOpenFile("Chart JSON (*.json)\0*.json\0All Files (*.*)\0*.*\0\0", picked, "json"))
            {
                std::string display = makeFileDisplayPath(picked);
                snprintf(m_filePathBuf, sizeof(m_filePathBuf), "%s", display.c_str());
                m_filePath = resolveJsonFilePath(display);

                Chart tmp;
                if (ChartIO::load(m_filePath, tmp))
                {
                    m_chart = tmp;
                    m_offsetSec = m_chart.offset;
                    m_statusMsg = "Loaded: " + m_filePath;
                    m_statusOk = true;

                    // メタデータバッファに反映
                    snprintf(m_musicnameBuf,   sizeof(m_musicnameBuf),   "%s", m_chart.musicname.c_str());
                    snprintf(m_musicauthorBuf, sizeof(m_musicauthorBuf), "%s", m_chart.musicauthor.c_str());
                    snprintf(m_scoreauthorBuf, sizeof(m_scoreauthorBuf), "%s", m_chart.scoreauthor.c_str());
                    snprintf(m_thumbnailBuf,   sizeof(m_thumbnailBuf),   "%s", m_chart.thumbnail.c_str());

                    if (!m_chart.musicPath.empty())
                    {
                        snprintf(m_musicBuf, sizeof(m_musicBuf), "%s", m_chart.musicPath.c_str());
                        m_musicPath = "music/" + m_chart.musicPath;
                        if (!m_audio.load(m_musicPath))
                            m_statusMsg += "  (music load failed)";
                        else
                            buildWaveform();
                    }
                }
                else { m_statusMsg = "Load failed: " + m_filePath; m_statusOk = false; }
            }
        }
        if (ImGui::MenuItem("Save"))
        {
            m_chart.offset = m_offsetSec;
            if (ChartIO::save(m_chart, m_filePath)) { m_statusMsg = "Saved: " + m_filePath; m_statusOk = true; }
            else { m_statusMsg = "Save failed: " + m_filePath; m_statusOk = false; }
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("SE"))
    {
        for (auto& sv : m_seVolumes)
        {
            ImGui::SetNextItemWidth(160.0f);
            if (ImGui::SliderFloat(sv.name.c_str(), &sv.volume, 0.0f, 1.0f, "%.2f"))
            {
                m_sePlayer.setVolume(sv.name, sv.volume);
                saveEditorSettings();
            }
        }
        ImGui::EndMenu();
    }

    ImGui::EndMenuBar();
}

void TimelineEditor::renderControls()
{
    // ---- 1行目: メタデータ ----
    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::InputText("MusicName", m_musicnameBuf, sizeof(m_musicnameBuf)))
        m_chart.musicname = m_musicnameBuf;
    ImGui::SameLine();

    ImGui::SetNextItemWidth(130.0f);
    if (ImGui::InputText("Author", m_musicauthorBuf, sizeof(m_musicauthorBuf)))
        m_chart.musicauthor = m_musicauthorBuf;
    ImGui::SameLine();

    ImGui::SetNextItemWidth(100.0f);
    if (ImGui::InputText("ScoreBy", m_scoreauthorBuf, sizeof(m_scoreauthorBuf)))
        m_chart.scoreauthor = m_scoreauthorBuf;
    ImGui::SameLine();

    ImGui::SetNextItemWidth(70.0f);
    ImGui::InputFloat("Diff", &m_chart.difficulty, 0.5f, 1.0f, "%.1f");
    if (m_chart.difficulty < 0.0f) m_chart.difficulty = 0.0f;
    ImGui::SameLine();

    ImGui::SetNextItemWidth(150.0f);
    if (ImGui::InputText("Thumbnail", m_thumbnailBuf, sizeof(m_thumbnailBuf)))
        m_chart.thumbnail = m_thumbnailBuf;
    ImGui::SameLine();
    if (ImGui::Button("...##thumb"))
    {
        std::string picked;
        if (browseOpenFile("Image Files\0*.png;*.jpg;*.jpeg;*.bmp\0All Files (*.*)\0*.*\0\0", picked,"thumbnail"))
        {
            snprintf(m_thumbnailBuf, sizeof(m_thumbnailBuf), "%s", extractFileName(picked).c_str());
            m_chart.thumbnail = m_thumbnailBuf;
        }
    }

    // ---- 2行目: ファイル・BPM・スナップ・ズーム・タイプ・オフセット ----
    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::InputText("File", m_filePathBuf, sizeof(m_filePathBuf)))
        m_filePath = resolveJsonFilePath(m_filePathBuf);

    ImGui::SameLine();

    ImGui::SetNextItemWidth(90.0f);
    ImGui::InputFloat("BPM", &m_chart.bpm, 1.0f, 10.0f, "%.1f");
    if (m_chart.bpm < 1.0f) m_chart.bpm = 1.0f;

    ImGui::SameLine();

    const char* snapLabels[] = { "1/4", "1/8", "1/16", "1/32" };
    const float snapValues[] = { 1.0f, 0.5f, 0.25f, 0.125f };
    int snapIdx = 2;
    for (int i = 0; i < 4; ++i)
        if (fabsf(m_snapBeat - snapValues[i]) < 0.001f) { snapIdx = i; break; }
    ImGui::SetNextItemWidth(80.0f);
    if (ImGui::Combo("Snap", &snapIdx, snapLabels, 4))
        m_snapBeat = snapValues[snapIdx];

    ImGui::SameLine();

    ImGui::SetNextItemWidth(120.0f);
    ImGui::SliderFloat("Zoom", &m_zoomBeats, 4.0f, 64.0f, "%.0f beats");

    ImGui::SameLine();

    const char* typeLabels[] = { "Enemy", "Hold", "Orb", "Barrier", "Rainbow" };
    int typeIdx = static_cast<int>(m_selectedType);
    ImGui::SetNextItemWidth(100.0f);
    if (ImGui::Combo("Type", &typeIdx, typeLabels, 5))
    {
        m_selectedType = static_cast<EventType>(typeIdx);
        m_holdPending  = false;
    }

    ImGui::SameLine();

    ImGui::SetNextItemWidth(90.0f);
    ImGui::InputFloat("Offset(s)", &m_offsetSec, 0.01f, 0.1f, "%.3f");

    // ---- 3行目: 音楽 & トランスポート ----
    ImGui::SetNextItemWidth(220.0f);
    ImGui::InputText("Music", m_musicBuf, sizeof(m_musicBuf));
    ImGui::SameLine();
    if (ImGui::Button("...##music"))
    {
        std::string picked;
        if (browseOpenFile("Audio Files\0*.wav;*.mp3\0All Files (*.*)\0*.*\0\0", picked, "music"))
            snprintf(m_musicBuf, sizeof(m_musicBuf), "%s", extractFileName(picked).c_str());
    }
    ImGui::SameLine();
    if (ImGui::Button("Load"))
    {
        m_musicPath = "music/" + std::string(m_musicBuf);
        if (m_audio.load(m_musicPath))
        {
            m_chart.musicPath = std::string(m_musicBuf);
            buildWaveform();
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

    ImGui::SameLine();
    ImGui::Spacing(); ImGui::SameLine();
    if (ImGui::Button("[M]"))
    {
        if (m_audio.isLoaded())
        {
            float t = m_audio.currentTimeSeconds() - m_offsetSec;
            m_markerBeat = Timing::secondsToBeat(t, m_chart.bpm);
        }
    }
    if (m_markerBeat >= 0.0f)
    {
        ImGui::SameLine();
        if (ImGui::Button("->M"))
        {
            float seekSec = Timing::beatToSeconds(m_markerBeat, m_chart.bpm) + m_offsetSec;
            m_audio.seekSeconds(std::max(0.0f, seekSec));
            m_scrollBeat = std::max(0.0f, m_markerBeat - m_zoomBeats * 0.3f);
        }
        ImGui::SameLine();
        ImGui::TextDisabled("beat %.2f", m_markerBeat);
    }

    ImGui::SameLine();
    const char* speedLabels[] = { "0.25x", "0.5x", "0.75x", "1.0x" };
    const float speedValues[] = { 0.25f, 0.5f, 0.75f, 1.0f };
    ImGui::SetNextItemWidth(70.0f);
    if (ImGui::Combo("Speed", &m_speedIdx, speedLabels, 4))
        m_audio.setSpeed(speedValues[m_speedIdx]);

    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.0f);
    if (ImGui::SliderFloat("Vol", &m_bgmVolume, 0.0f, 1.0f, "%.2f"))
        m_audio.setVolume(m_bgmVolume);

    if (m_audio.isLoaded())
    {
        float t    = m_audio.currentTimeSeconds() - m_offsetSec;
        float dur  = m_audio.durationSeconds();
        float beat = Timing::secondsToBeat(t, m_chart.bpm);
        ImGui::SameLine();
        ImGui::Text("%.2f / %.2f sec   beat: %.2f", t, dur, beat);
    }

    // Hold / Rainbow pending 表示
    if (m_holdPending)
    {
        ImGui::SameLine();
        if (m_selectedType == EventType::Hold)
            ImGui::TextColored(ImVec4(0.3f, 0.6f, 1.0f, 1.0f),
                "  [Hold] 始点: wall=%s lane=%d beat=%.2f - 同一壁をクリック / Esc でキャンセル",
                wallLabel(m_holdStartWall), m_holdStartLane, m_holdStartBeat);
        else
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.7f, 1.0f),
                "  [Rainbow] 始点: wall=%s beat=%.2f - 終点をクリック / Esc でキャンセル",
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

    // ---- マウスカーソルの beat 位置を毎フレーム更新 ----
    {
        ImVec2 mp = ImGui::GetIO().MousePos;
        float tlLeft = origin.x + labelWidth;
        float tlRight = tlLeft + tlWidth;
        if (mp.x >= tlLeft && mp.x <= tlRight
            && mp.y >= origin.y && mp.y <= origin.y + totalH)
        {
            float relX = mp.x - tlLeft;
            float raw  = m_scrollBeat + (relX / tlWidth) * m_zoomBeats;
            m_hoverBeat = roundf(raw / m_snapBeat) * m_snapBeat;
            if (m_hoverBeat < 0.0f) m_hoverBeat = 0.0f;
        }
        else
        {
            m_hoverBeat = -1.0f;
        }
    }

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

    // ---- 波形表示 ----
    if (!m_waveformPeaks.empty())
    {
        float waveXL  = origin.x + labelWidth;
        float waveXR  = origin.x + labelWidth + tlWidth;
        float waveY   = origin.y + headerHeight;
        float waveH   = rowHeight * numLanes;
        float midY    = waveY + waveH * 0.5f;
        float ampH    = waveH * 0.4f;
        float smpRate = (float)m_audio.waveFormat().nSamplesPerSec;
        float audioDur = m_audio.durationSeconds();

        for (float px = waveXL; px < waveXR; px += 1.0f)
        {
            float beat = m_scrollBeat + (px - waveXL) / tlWidth * m_zoomBeats;
            float sec  = Timing::beatToSeconds(beat, m_chart.bpm) + m_offsetSec;
            if (sec < 0.0f || sec > audioDur) continue;

            int peakIdx = (int)((uint32_t)(sec * smpRate) / kWaveChunkFrames);
            if (peakIdx >= (int)m_waveformPeaks.size()) continue;

            const WaveformPeak& p = m_waveformPeaks[peakIdx];
            float y1 = midY + p.minVal * ampH;
            float y2 = midY + p.maxVal * ampH;
            dl->AddLine(ImVec2(px, y1), ImVec2(px, y2), IM_COL32(0, 200, 200, 50));
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
            // Hold: 同一壁・レーンまたぎ可
            float x2  = beatToX(e.endBeat);
            int   wi  = static_cast<int>(e.wall);
            float sy  = origin.y + headerHeight + (wi * 3 + e.lane)    * rowHeight;
            float ey  = origin.y + headerHeight + (wi * 3 + e.endLane) * rowHeight;

            if (x2 > clipL && x < clipR)
            {
                if (e.lane == e.endLane)
                {
                    // 同一レーン: 矩形
                    float y1 = sy + 2;
                    float y2 = sy + rowHeight - 2;
                    dl->AddRectFilled(ImVec2(std::max(x, clipL), y1),
                                      ImVec2(std::min(x2, clipR), y2), col);
                    dl->AddRect(ImVec2(std::max(x, clipL), y1),
                                ImVec2(std::min(x2, clipR), y2),
                                IM_COL32(200, 220, 255, 200), 2.0f);
                }
                else
                {
                    // レーンまたぎ: 平行四辺形（太さ = レーン1行分）
                    ImVec2 p1(x,  sy + 2);
                    ImVec2 p2(x,  sy + rowHeight - 2);
                    ImVec2 p3(x2, ey + rowHeight - 2);
                    ImVec2 p4(x2, ey + 2);
                    dl->AddQuadFilled(p1, p2, p3, p4, col);
                    dl->AddQuad(p1, p2, p3, p4, IM_COL32(200, 220, 255, 200), 2.0f);
                }
            }
        }
        else if (e.type == EventType::Rainbow)
        {
            // Rainbow: 壁全体（3レーン）・壁またぎ平行四辺形
            float x2 = beatToX(e.endBeat);
            float sy  = wallRowY(e.wall);
            float ey  = wallRowY(e.endWall);

            if (e.wall == e.endWall)
            {
                float y1 = sy + 2;
                float y2 = sy + 3 * rowHeight - 2;
                if (x2 > clipL && x < clipR)
                {
                    float lx = std::max(x, clipL);
                    float rx = std::min(x2, clipR);
                    // 虹色グラデーション（上から赤・青・緑）
                    float bH = (y2 - y1) / 3.0f;
                    dl->AddRectFilled(ImVec2(lx, y1),        ImVec2(rx, y1 + bH),     IM_COL32(255,  80,  80, 200));
                    dl->AddRectFilled(ImVec2(lx, y1 + bH),   ImVec2(rx, y1 + 2*bH),   IM_COL32( 80, 160, 255, 200));
                    dl->AddRectFilled(ImVec2(lx, y1 + 2*bH), ImVec2(rx, y2),           IM_COL32( 80, 255, 150, 200));
                    dl->AddRect(ImVec2(lx, y1), ImVec2(rx, y2), IM_COL32(255, 255, 255, 200), 3.0f);
                }
            }
            else
            {
                if (x2 > clipL && x < clipR)
                {
                    ImVec2 p1(x,  sy + 2);
                    ImVec2 p2(x,  sy + 3 * rowHeight - 2);
                    ImVec2 p3(x2, ey + 3 * rowHeight - 2);
                    ImVec2 p4(x2, ey + 2);
                    dl->AddQuadFilled(p1, p2, p3, p4, col);
                    dl->AddQuad(p1, p2, p3, p4, IM_COL32(255, 255, 255, 200), 2.0f);

                    // 回転方向ラベル
                    const char* dirLabel = (e.direction == RotationDir::CW) ? "R" : "L";
                    ImVec2 labelPos((std::max(x, clipL) + std::min(x2, clipR)) * 0.5f,
                                     (sy + ey) * 0.5f + 1.5f * rowHeight - 7.0f);
                    dl->AddText(labelPos, IM_COL32(255, 255, 255, 255), dirLabel);
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
            case EventType::Enemy:
                dl->AddCircleFilled(ImVec2(x, cy), r, col);
                break;
            case EventType::Orb:
                dl->AddCircle(ImVec2(x, cy), r, col, 0, 2.5f);
                break;
            case EventType::Barrier:
                dl->AddRectFilled(ImVec2(x - r, cy - r), ImVec2(x + r, cy + r), col);
                break;
            default:
                dl->AddCircleFilled(ImVec2(x, cy), r, col);
                break;
            }
        }
    }

    // ---- 範囲選択ハイライト ----
    if (m_selectionActive && m_selectStart < m_selectEnd)
    {
        float sx = std::max(beatToX(m_selectStart), clipL);
        float ex = std::min(beatToX(m_selectEnd),   clipR);
        if (ex > sx)
        {
            dl->AddRectFilled(
                ImVec2(sx, origin.y + headerHeight),
                ImVec2(ex, origin.y + totalH),
                IM_COL32(100, 150, 255, 40));
            dl->AddRect(
                ImVec2(sx, origin.y + headerHeight),
                ImVec2(ex, origin.y + totalH),
                IM_COL32(100, 150, 255, 140), 0.0f, 0, 1.5f);
        }
    }

    // ---- シーク位置マーカー ----
    if (m_markerBeat >= 0.0f)
    {
        float mx = beatToX(m_markerBeat);
        if (mx >= clipL && mx <= clipR)
        {
            dl->AddLine(ImVec2(mx, origin.y + headerHeight),
                        ImVec2(mx, origin.y + totalH),
                        IM_COL32(255, 140, 0, 180), 1.5f);
            dl->AddTriangleFilled(
                ImVec2(mx - 5, origin.y),
                ImVec2(mx + 5, origin.y),
                ImVec2(mx,     origin.y + 10),
                IM_COL32(255, 140, 0, 220));

            // 三角形クリックで再生ヘッドをマーカー位置へジャンプ
            ImVec2 mp = ImGui::GetIO().MousePos;
            bool overTriangle = fabsf(mp.x - mx) <= 8.0f
                             && mp.y >= origin.y
                             && mp.y <= origin.y + 14.0f;
            if (overTriangle && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !m_draggingPlayhead)
            {
                float seekSec = Timing::beatToSeconds(m_markerBeat, m_chart.bpm) + m_offsetSec;
                m_audio.seekSeconds(std::max(0.0f, seekSec));
            }
        }
    }

    // Hold / Rainbow pending フィードバック（始点マーカー）
    if (m_holdPending)
    {
        float px = beatToX(m_holdStartBeat);
        if (px >= clipL && px <= clipR)
        {
            if (m_selectedType == EventType::Hold)
            {
                // Hold: 単一レーン行に縦バー
                int   row  = static_cast<int>(m_holdStartWall) * 3 + m_holdStartLane;
                float rowY = origin.y + headerHeight + row * rowHeight;
                dl->AddLine(ImVec2(px, rowY),
                            ImVec2(px, rowY + rowHeight),
                            IM_COL32(80, 160, 255, 180), 2.0f);
                dl->AddText(ImVec2(px + 3, rowY + 2), IM_COL32(80, 160, 255, 255), "H>");
            }
            else
            {
                // Rainbow: 壁全体（3レーン）に縦バー
                float sy = wallRowY(m_holdStartWall);
                dl->AddLine(ImVec2(px, sy),
                            ImVec2(px, sy + 3 * rowHeight),
                            IM_COL32(50, 255, 180, 180), 2.0f);
                dl->AddText(ImVec2(px + 3, sy + 2), IM_COL32(50, 255, 180, 255), "R>");
            }
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

    // ---- ヘッダー行クリックで再生ヘッドシーク ----
    if (m_audio.isLoaded())
    {
        ImGui::SetCursorScreenPos(ImVec2(origin.x + labelWidth, origin.y));
        ImGui::InvisibleButton("##header", ImVec2(tlWidth, headerHeight));
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !m_draggingPlayhead)
        {
            ImVec2 mp = ImGui::GetIO().MousePos;
            float relX = mp.x - (origin.x + labelWidth);
            float beat = m_scrollBeat + (relX / tlWidth) * m_zoomBeats;
            beat = std::max(0.0f, beat);
            float seekSec = Timing::beatToSeconds(beat, m_chart.bpm) + m_offsetSec;
            m_audio.seekSeconds(std::max(0.0f, std::min(seekSec, m_audio.durationSeconds())));
        }
    }

    // ---- 範囲選択ドラッグ追跡 ----
    if (m_selectDragging)
    {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            ImVec2 mp2  = ImGui::GetIO().MousePos;
            float relX2 = mp2.x - (origin.x + labelWidth);
            float curBeat = m_scrollBeat + (relX2 / tlWidth) * m_zoomBeats;
            curBeat = roundf(curBeat / m_snapBeat) * m_snapBeat;
            curBeat = std::max(0.0f, curBeat);
            m_selectStart = std::min(m_selectDragOrigin, curBeat);
            m_selectEnd   = std::max(m_selectDragOrigin, curBeat);
            m_selectionActive = (m_selectStart < m_selectEnd);
        }
        else
        {
            m_selectDragging = false;
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
                if (ImGui::GetIO().KeyCtrl)
                {
                    // Ctrl+クリック: 範囲選択ドラッグ開始
                    m_selectDragging   = true;
                    m_selectDragOrigin = beat;
                    m_selectionActive  = false;
                }
                else if (!m_selectDragging)
                {
                    m_selectionActive = false;  // 選択解除

                    if (m_selectedType == EventType::Hold)
                    {
                        if (!m_holdPending)
                        {
                            // 1回目: 始点記録
                            m_holdPending     = true;
                            m_holdStartBeat   = beat;
                            m_holdStartWall   = clickWall;
                            m_holdStartLane   = clickLane;
                        }
                        else
                        {
                            // 2回目: 同一壁のみ
                            if (clickWall != m_holdStartWall)
                            {
                                m_statusMsg = "Hold は同一壁のみ使用可能です";
                                m_statusOk  = false;
                            }
                            else
                            {
                                pushUndo();
                                Event e;
                                e.type    = EventType::Hold;
                                e.wall    = m_holdStartWall;
                                e.endWall = m_holdStartWall;
                                // beat が早い方を始点にして lane/endLane を対応させる
                                if (m_holdStartBeat <= beat)
                                {
                                    e.beat    = m_holdStartBeat;
                                    e.endBeat = beat;
                                    e.lane    = m_holdStartLane;
                                    e.endLane = clickLane;
                                }
                                else
                                {
                                    e.beat    = beat;
                                    e.endBeat = m_holdStartBeat;
                                    e.lane    = clickLane;
                                    e.endLane = m_holdStartLane;
                                }
                                m_chart.events.push_back(e);
                                m_holdPending = false;
                            }
                        }
                    }
                    else if (m_selectedType == EventType::Rainbow)
                    {
                        if (!m_holdPending)
                        {
                            // 既存 Rainbow ノーツをクリックした場合は回転方向を反転
                            Event* hit = nullptr;
                            for (auto& e : m_chart.events)
                            {
                                if (e.type != EventType::Rainbow) continue;
                                bool wallMatch = (e.wall == clickWall || e.endWall == clickWall);
                                bool beatMatch = (beat >= e.beat - m_snapBeat * 0.5f &&
                                                 beat <= e.endBeat + m_snapBeat * 0.5f);
                                if (wallMatch && beatMatch) { hit = &e; break; }
                            }

                            if (hit)
                            {
                                pushUndo();
                                hit->direction = (hit->direction == RotationDir::CW)
                                    ? RotationDir::CCW : RotationDir::CW;
                            }
                            else
                            {
                                // 1回目: 始点記録
                                m_holdPending   = true;
                                m_holdStartBeat = beat;
                                m_holdStartWall = clickWall;
                            }
                        }
                        else
                        {
                            // 2回目: 壁またぎ可
                            pushUndo();
                            Event e;
                            e.type    = EventType::Rainbow;
                            e.wall    = m_holdStartWall;
                            e.endWall = clickWall;
                            e.lane    = 1;
                            e.beat    = std::min(m_holdStartBeat, beat);
                            e.endBeat = std::max(m_holdStartBeat, beat);

                            // デフォルト方向: CW方向の距離が短い方を採用
                            int startIdx = static_cast<int>(m_holdStartWall);
                            int endIdx   = static_cast<int>(clickWall);
                            int cwSteps  = (endIdx - startIdx + 4) % 4;
                            e.direction  = (cwSteps <= 2) ? RotationDir::CW : RotationDir::CCW;

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
            }

            if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
            {
                m_selectionActive = false;
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
                            // Hold: 同一壁・クリックレーンが [lane, endLane] 範囲内・beat範囲内
                            bool beatMatch = (beat >= e.beat - m_snapBeat * 0.5f &&
                                             beat <= e.endBeat + m_snapBeat * 0.5f);
                            bool laneMatch = (clickLane >= std::min(e.lane, e.endLane) &&
                                             clickLane <= std::max(e.lane, e.endLane));
                            return e.wall == clickWall && laneMatch && beatMatch;
                        }
                        if (e.type == EventType::Rainbow)
                        {
                            // Rainbow: 始点か終点の壁に一致し、beat範囲内なら削除
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

void TimelineEditor::buildWaveform()
{
    m_waveformPeaks.clear();
    if (!m_audio.isLoaded()) return;

    const auto&    pcm   = m_audio.pcmData();
    const auto&    fmt   = m_audio.waveFormat();
    uint32_t       total = m_audio.totalFrames();
    int            ch    = fmt.nChannels;
    const int16_t* s     = reinterpret_cast<const int16_t*>(pcm.data());

    m_waveformPeaks.reserve((total + kWaveChunkFrames - 1) / kWaveChunkFrames);
    for (uint32_t f = 0; f < total; f += (uint32_t)kWaveChunkFrames)
    {
        uint32_t end  = std::min(f + (uint32_t)kWaveChunkFrames, total);
        float    minV =  1.0f;
        float    maxV = -1.0f;
        for (uint32_t i = f; i < end; ++i)
        {
            for (int c = 0; c < ch; ++c)
            {
                float v = s[i * ch + c] / 32768.0f;
                if (v < minV) minV = v;
                if (v > maxV) maxV = v;
            }
        }
        m_waveformPeaks.push_back({ minV, maxV });
    }
}

void TimelineEditor::toggleEvent(float beat, Wall wall, int lane)
{
    auto& ev = m_chart.events;

    auto it = std::find_if(ev.begin(), ev.end(), [&](const Event& e) {
        if (e.type == EventType::Hold || e.type == EventType::Rainbow) return false;
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

void TimelineEditor::loadEditorSettings()
{
    std::ifstream file(kEditorSettingsPath);
    if (!file.is_open()) return;

    json j;
    try { file >> j; } catch (...) { return; }

    if (!j.contains("se_volume")) return;
    const auto& sv = j["se_volume"];
    for (auto& entry : m_seVolumes)
    {
        if (sv.contains(entry.name))
            entry.volume = sv[entry.name].get<float>();
    }
}

void TimelineEditor::saveEditorSettings()
{
    json j;
    for (const auto& entry : m_seVolumes)
        j["se_volume"][entry.name] = entry.volume;

    std::ofstream file(kEditorSettingsPath);
    if (file.is_open())
        file << j.dump(2);
}
