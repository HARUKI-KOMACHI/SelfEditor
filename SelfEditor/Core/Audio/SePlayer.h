#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <xaudio2.h>
#include <string>
#include <vector>
#include <cstdint>

class SePlayer
{
public:
    ~SePlayer();

    bool init();
    // filePath: WAV ファイルパス
    // volume  : 音量係数 (0.0〜1.0) — SE ファイルの録音レベルに合わせて調整する
    bool loadSe(const std::string& name, const std::string& filePath, float volume = 1.0f);
    void play(const std::string& name);
    void playLoop(const std::string& name);
    void stopLoop(const std::string& name);
    void unloadAll();

private:
    static constexpr int kVoicePoolSize = 4;

    struct SeEntry {
        std::string          name;
        std::vector<uint8_t> pcmData;
        WAVEFORMATEX         fmt        = {};
        float                volume     = 1.0f;  // ← SEごとの音量 (0.0〜1.0)
        IXAudio2SourceVoice* voices[kVoicePoolSize] = {};
        int                  nextVoice  = 0;
        IXAudio2SourceVoice* loopVoice  = nullptr;  // ループ再生専用ボイス
    };

    IXAudio2*               m_xaudio2 = nullptr;
    IXAudio2MasteringVoice* m_master  = nullptr;
    std::vector<SeEntry>    m_entries;
};
