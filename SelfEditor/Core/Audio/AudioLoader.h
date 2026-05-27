#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <xaudio2.h>
#include <string>
#include <vector>
#include <cstdint>

// WAV または MP3 ファイルを PCM に展開する。失敗時は false を返す。
bool loadAudioFile(const std::string& path, WAVEFORMATEX& outFmt, std::vector<uint8_t>& outPcm);
