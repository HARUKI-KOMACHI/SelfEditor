#include "AudioLoader.h"
#include <fstream>
#include <cstring>
#include <algorithm>
#include <cctype>

#define MINIMP3_IMPLEMENTATION
#include "minimp3_ex.h"

// ---- WAV ローダー ----

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
            // chunk.size >= 16 を確認してからスキップ（不正ファイルのアンダーフロー防止）
            if (chunk.size >= 16) f.seekg(chunk.size - 16, std::ios::cur);

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
            // 実際に読めたバイト数に切り詰める（ファイル末端切れ対応）
            outPcm.resize((size_t)f.gcount());
            // WAV 仕様: 全チャンクは 2 バイト境界アライン
            if (chunk.size & 1) f.seekg(1, std::ios::cur);
        }
        else
        {
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

// ---- 公開インターフェース ----

bool loadAudioFile(const std::string& path, WAVEFORMATEX& outFmt, std::vector<uint8_t>& outPcm)
{
    std::string ext;
    auto dot = path.rfind('.');
    if (dot != std::string::npos)
    {
        ext = path.substr(dot);
        for (auto& c : ext) c = (char)tolower((unsigned char)c);
    }
    return (ext == ".mp3") ? loadMp3(path, outFmt, outPcm)
                           : loadWav(path, outFmt, outPcm);
}
