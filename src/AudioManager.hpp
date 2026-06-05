#pragma once

#define NOMINMAX

#include "../external/miniaudio.h"
#include <string>
#include <unordered_map>
#include <memory>
#include <iostream>

class AudioManager {
public:
    AudioManager();
    ~AudioManager();

    // Prevent copying
    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    bool init();
    void shutdown();

    // Music (background, looping)
    bool playMusic(const std::string& filepath, float volume = 1.0f);
    void stopMusic();
    void pauseMusic();
    void resumeMusic();
    void setMusicVolume(float volume);
    bool isMusicPlaying() const;

    // Sound effects (one-shot)
    bool playSound(const std::string& filepath, float volume = 1.0f);

    // Master volume
    void setMasterVolume(float volume);

private:
    ma_engine engine;
    ma_sound* currentMusic = nullptr;
    bool initialized = false;
    float masterVolume = 1.0f;
};