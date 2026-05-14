#include "AudioPlayer.h"
#include <fstream>
#include <cstring>
#include <algorithm>
#include <cctype>

#define MINIMP3_IMPLEMENTATION
#include "minimp3_ex.h"

// ---- 簡易 WAV ローダー ----

struct ChunkHeader { char id[4]; uint32_t size; };

static bool loadWav(const std::string& path, WAVEFORMATEX& outFmt, std::vector<uint8_t>& outPcm)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    ChunkHeader riff; char wave[4];
    f.read((char*)&riff, 8);
    f.read(wave, 4);
    if (strncmp(riff.id, "RIFF", 4) != 0 || strncmp(wave, "WAVE", 4) != 0)
        return false;

    bool hasFmt = false;

    while (f)
    {
        ChunkHeader chunk;
        f.read((char*)&chunk, 8);
        if (f.gcount() < 8) break;

        if (strncmp(chunk.id, "fmt ", 4) == 0)
        {
            uint16_t audioFmt, channels, blockAlign, bitsPerSample;
            uint32_t sampleRate, byteRate;
            f.read((char*)&audioFmt,      2);
            f.read((char*)&channels,      2);
            f.read((char*)&sampleRate,    4);
            f.read((char*)&byteRate,      4);
            f.read((char*)&blockAlign,    2);
            f.read((char*)&bitsPerSample, 2);
            if (chunk.size > 16) f.seekg(chunk.size - 16, std::ios::cur);

            outFmt.wFormatTag      = WAVE_FORMAT_PCM;
            outFmt.nChannels       = channels;
            outFmt.nSamplesPerSec  = sampleRate;
            outFmt.wBitsPerSample  = bitsPerSample;
            outFmt.nBlockAlign     = blockAlign;
            outFmt.nAvgBytesPerSec = byteRate;
            outFmt.cbSize          = 0;
            hasFmt = true;
        }
        else if (strncmp(chunk.id, "data", 4) == 0)
        {
            outPcm.resize(chunk.size);
            f.read((char*)outPcm.data(), chunk.size);
        }
        else
        {
            // 未知チャンクはスキップ (2バイト境界アライン)
            f.seekg((chunk.size + 1) & ~1u, std::ios::cur);
        }
    }

    return hasFmt && !outPcm.empty();
}

// ---- MP3 ローダー ----

static bool loadMp3(const std::string& path, WAVEFORMATEX& outFmt, std::vector<uint8_t>& outPcm)
{
    mp3dec_ex_t dec;
    if (mp3dec_ex_open(&dec, path.c_str(), MP3D_SEEK_TO_SAMPLE) != 0)
        return false;

    if (dec.info.channels == 0 || dec.info.hz == 0 || dec.samples == 0)
    {
        mp3dec_ex_close(&dec);
        return false;
    }

    outFmt.wFormatTag      = WAVE_FORMAT_PCM;
    outFmt.nChannels       = (WORD)dec.info.channels;
    outFmt.nSamplesPerSec  = (DWORD)dec.info.hz;
    outFmt.wBitsPerSample  = 16;
    outFmt.nBlockAlign     = outFmt.nChannels * 2;
    outFmt.nAvgBytesPerSec = outFmt.nSamplesPerSec * outFmt.nBlockAlign;
    outFmt.cbSize          = 0;

    outPcm.resize(dec.samples * sizeof(mp3d_sample_t));
    size_t decoded = mp3dec_ex_read(&dec, (mp3d_sample_t*)outPcm.data(), dec.samples);
    mp3dec_ex_close(&dec);

    if (decoded == 0) return false;
    outPcm.resize(decoded * sizeof(mp3d_sample_t));
    return true;
}

// ---- AudioPlayer ----

AudioPlayer::~AudioPlayer()
{
    unload();
    if (m_master)  { m_master->DestroyVoice();  m_master  = nullptr; }
    if (m_xaudio2) { m_xaudio2->Release();      m_xaudio2 = nullptr; }
}

bool AudioPlayer::initXAudio2()
{
    if (m_xaudio2) return true;
    if (FAILED(XAudio2Create(&m_xaudio2, 0))) return false;
    if (FAILED(m_xaudio2->CreateMasteringVoice(&m_master))) return false;
    return true;
}

bool AudioPlayer::createSourceVoice()
{
    if (m_source) { m_source->DestroyVoice(); m_source = nullptr; }
    return SUCCEEDED(m_xaudio2->CreateSourceVoice(&m_source, &m_fmt));
}

void AudioPlayer::submitBuffer(uint32_t startFrame)
{
    if (!m_source || m_pcmData.empty()) return;
    m_source->Stop();
    m_source->FlushSourceBuffers();

    // バッファ送信前の SamplesPlayed をベースラインとして記録
    XAUDIO2_VOICE_STATE vs = {};
    m_source->GetState(&vs);
    m_samplesAtStart = vs.SamplesPlayed;

    uint32_t byteOffset = startFrame * m_fmt.nBlockAlign;
    if (byteOffset >= (uint32_t)m_pcmData.size()) return;

    XAUDIO2_BUFFER buf = {};
    buf.Flags      = XAUDIO2_END_OF_STREAM;
    buf.AudioBytes = (UINT32)(m_pcmData.size() - byteOffset);
    buf.pAudioData = m_pcmData.data() + byteOffset;
    m_source->SubmitSourceBuffer(&buf);
}

bool AudioPlayer::load(const std::string& filePath)
{
    unload();
    if (!initXAudio2()) return false;

    WAVEFORMATEX fmt = {};
    std::vector<uint8_t> pcm;

    // 拡張子で WAV / MP3 を判定
    std::string ext;
    auto dot = filePath.rfind('.');
    if (dot != std::string::npos)
    {
        ext = filePath.substr(dot);
        for (auto& c : ext) c = (char)tolower((unsigned char)c);
    }

    bool ok = (ext == ".mp3") ? loadMp3(filePath, fmt, pcm)
                              : loadWav(filePath, fmt, pcm);
    if (!ok) return false;

    m_fmt         = fmt;
    m_pcmData     = std::move(pcm);
    m_totalFrames = (uint32_t)(m_pcmData.size() / m_fmt.nBlockAlign);
    m_baseSec     = 0.0f;
    m_playing     = false;

    return createSourceVoice();
}

void AudioPlayer::unload()
{
    if (m_source)
    {
        m_source->Stop();
        m_source->FlushSourceBuffers();
        m_source->DestroyVoice();
        m_source = nullptr;
    }
    m_pcmData.clear();
    m_totalFrames = 0;
    m_baseSec     = 0.0f;
    m_playing     = false;
}

void AudioPlayer::play()
{
    if (!isLoaded() || m_playing) return;
    uint32_t startFrame = (uint32_t)(m_baseSec * m_fmt.nSamplesPerSec);
    startFrame = std::min(startFrame, m_totalFrames);
    submitBuffer(startFrame);
    m_source->Start();
    m_playing = true;
}

void AudioPlayer::pause()
{
    if (!m_playing || !m_source) return;
    m_baseSec = currentTimeSeconds();
    m_source->Stop();
    m_source->FlushSourceBuffers();
    m_playing = false;
}

void AudioPlayer::stop()
{
    if (!m_source) return;
    m_source->Stop();
    m_source->FlushSourceBuffers();
    m_baseSec = 0.0f;
    m_playing = false;
}

void AudioPlayer::seekSeconds(float seconds)
{
    if (!isLoaded()) return;
    float clamped   = std::max(0.0f, std::min(seconds, durationSeconds()));
    bool wasPlaying = m_playing;
    if (wasPlaying) { m_source->Stop(); m_source->FlushSourceBuffers(); m_playing = false; }
    m_baseSec = clamped;
    if (wasPlaying) play();
}

float AudioPlayer::currentTimeSeconds() const
{
    if (!m_source || !m_playing) return m_baseSec;
    XAUDIO2_VOICE_STATE state = {};
    m_source->GetState(&state);
    uint64_t elapsed = state.SamplesPlayed - m_samplesAtStart;
    return m_baseSec + (float)elapsed / (float)m_fmt.nSamplesPerSec;
}

float AudioPlayer::durationSeconds() const
{
    if (!isLoaded() || m_fmt.nSamplesPerSec == 0) return 0.0f;
    return (float)m_totalFrames / (float)m_fmt.nSamplesPerSec;
}
