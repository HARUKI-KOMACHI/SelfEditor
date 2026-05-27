#include "SePlayer.h"
#include "AudioLoader.h"

// ---- SePlayer ----

SePlayer::~SePlayer()
{
    unloadAll();
    if (m_master)  { m_master->DestroyVoice();  m_master  = nullptr; }
    if (m_xaudio2) { m_xaudio2->Release();      m_xaudio2 = nullptr; }
}

bool SePlayer::init()
{
    if (m_xaudio2) return true;
    if (FAILED(XAudio2Create(&m_xaudio2, 0))) return false;
    if (FAILED(m_xaudio2->CreateMasteringVoice(&m_master)))
    {
        m_xaudio2->Release();
        m_xaudio2 = nullptr;
        return false;
    }
    return true;
}

bool SePlayer::loadSe(const std::string& name, const std::string& filePath, float volume)
{
    if (!m_xaudio2) return false;

    SeEntry entry;
    entry.name   = name;
    entry.volume = volume;

    if (!loadAudioFile(filePath, entry.fmt, entry.pcmData)) return false;

    for (int i = 0; i < kVoicePoolSize; ++i)
    {
        if (FAILED(m_xaudio2->CreateSourceVoice(&entry.voices[i], &entry.fmt)))
            return false;
        entry.voices[i]->SetVolume(volume);
    }

    m_entries.push_back(std::move(entry));
    return true;
}

void SePlayer::play(const std::string& name)
{
    for (auto& e : m_entries)
    {
        if (e.name != name) continue;
        IXAudio2SourceVoice* v = e.voices[e.nextVoice];
        if (!v) return;

        v->Stop();
        v->FlushSourceBuffers();

        XAUDIO2_BUFFER buf = {};
        buf.Flags      = XAUDIO2_END_OF_STREAM;
        buf.AudioBytes = (UINT32)e.pcmData.size();
        buf.pAudioData = e.pcmData.data();
        v->SubmitSourceBuffer(&buf);
        v->Start();

        e.nextVoice = (e.nextVoice + 1) % kVoicePoolSize;
        return;
    }
}

void SePlayer::unloadAll()
{
    for (auto& e : m_entries)
        for (int i = 0; i < kVoicePoolSize; ++i)
            if (e.voices[i]) { e.voices[i]->DestroyVoice(); e.voices[i] = nullptr; }
    m_entries.clear();
}
