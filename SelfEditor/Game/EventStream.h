#pragma once
#include <cstddef>
#include <vector>
#include "../Core/Chart/Chart.h"

// Chart のイベントを時間に沿って順番に発火させるクラス
class EventStream
{
public:
    // Chart を読み込んでリセット（events は beat 順にソートされる）
    void load(const Chart& chart);

    // リセットして先頭から再生
    void reset();

    // deltaTime 秒進める。その間に発火したイベントを返す
    std::vector<Event> update(float deltaTime);

    float currentBeat() const { return m_currentBeat; }
    float currentTime() const { return m_currentTime; }

    // 全イベントを発火し終えたら true
    bool isFinished() const { return m_nextIndex >= m_events.size(); }

private:
    float              m_bpm         = 120.0f;
    float              m_currentBeat = 0.0f;
    float              m_currentTime = 0.0f;
    std::vector<Event> m_events;   // beat 昇順にソート済み
    size_t             m_nextIndex = 0;
};
