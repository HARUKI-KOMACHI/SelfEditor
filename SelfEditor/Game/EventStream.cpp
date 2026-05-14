#include "EventStream.h"
#include <algorithm>
#include "../Core/Timing/Timing.h"

void EventStream::load(const Chart& chart)
{
    m_bpm    = chart.bpm;
    m_events = chart.events;

    // beat 昇順にソート
    std::sort(m_events.begin(), m_events.end(),
        [](const Event& a, const Event& b) { return a.beat < b.beat; });

    reset();
}

void EventStream::reset()
{
    m_currentBeat = 0.0f;
    m_currentTime = 0.0f;
    m_nextIndex   = 0;
}

std::vector<Event> EventStream::update(float deltaTime)
{
    m_currentTime += deltaTime;
    m_currentBeat  = Timing::secondsToBeat(m_currentTime, m_bpm);

    std::vector<Event> fired;

    while (m_nextIndex < m_events.size() &&
           m_events[m_nextIndex].beat <= m_currentBeat)
    {
        fired.push_back(m_events[m_nextIndex]);
        ++m_nextIndex;
    }

    return fired;
}
