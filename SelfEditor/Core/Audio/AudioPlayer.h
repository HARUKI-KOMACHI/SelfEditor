#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <xaudio2.h>
#include <string>
#include <vector>
#include <cstdint>

class AudioPlayer
{
public:
    ~AudioPlayer();

    // WAV ファイルを読み込む。成功したら true
    bool load(const std::string& filePath);
    void unload();

    void  play();
    void  pause();
    void  stop();
    void  seekSeconds(float seconds);
    void  setSpeed(float ratio);   // 再生速度設定 (0.25 / 0.5 / 0.75 / 1.0)
    float getSpeed() const { return m_speed; }
    void  setVolume(float vol);    // BGM音量設定 (0.0〜1.0)
    float getVolume() const { return m_volume; }

    float currentTimeSeconds() const;
    float durationSeconds()    const;
    bool  isPlaying() const { return m_playing; }
    bool  isLoaded()  const { return !m_pcmData.empty(); }

    // 波形表示用アクセサ
    const std::vector<uint8_t>& pcmData()     const { return m_pcmData; }
    const WAVEFORMATEX&          waveFormat()  const { return m_fmt; }
    uint32_t                     totalFrames() const { return m_totalFrames; }

private:
    bool initXAudio2();
    bool createSourceVoice();
    void submitBuffer(uint32_t startFrame);

    IXAudio2*               m_xaudio2     = nullptr;
    IXAudio2MasteringVoice* m_master      = nullptr;
    IXAudio2SourceVoice*    m_source      = nullptr;

    std::vector<uint8_t>    m_pcmData;
    WAVEFORMATEX            m_fmt             = {};
    uint32_t                m_totalFrames     = 0;
    float                   m_baseSec         = 0.0f;
    uint64_t                m_samplesAtStart  = 0;
    bool                    m_playing         = false;
    float                   m_speed           = 1.0f;
    float                   m_volume          = 1.0f;
};
