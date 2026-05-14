#pragma once

namespace Timing
{
    // beat を秒に変換
    inline float beatToSeconds(float beat, float bpm)
    {
        return (60.0f / bpm) * beat;
    }

    // 秒を beat に変換
    inline float secondsToBeat(float seconds, float bpm)
    {
        return seconds * (bpm / 60.0f);
    }
}
