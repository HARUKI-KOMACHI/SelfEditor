#include "AudioPlayer.h"
#include "AudioLoader.h"
#include <algorithm>

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

    if (!loadAudioFile(filePath, fmt, pcm)) return false;

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
    m_source->SetFrequencyRatio(m_speed);
    m_source->SetVolume(m_volume);
    m_source->Start();
    m_playing = true;
}

void AudioPlayer::setSpeed(float ratio)
{
    m_speed = ratio;
    if (m_source) m_source->SetFrequencyRatio(ratio);
}

void AudioPlayer::setVolume(float vol)
{
    m_volume = vol;
    if (m_source) m_source->SetVolume(vol);
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
