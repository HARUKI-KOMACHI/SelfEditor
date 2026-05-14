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

    float currentTimeSeconds() const;
    float durationSeconds()    const;
    bool  isPlaying() const { return m_playing; }
    bool  isLoaded()  const { return !m_pcmData.empty(); }

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
};
