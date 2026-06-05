#define NOMINMAX

#include "AudioManager.hpp"

#include <algorithm>

AudioManager::AudioManager() {}

AudioManager::~AudioManager() {
    shutdown();
}

bool AudioManager::init() {
    if (initialized) return true;

    ma_result result = ma_engine_init(nullptr, &engine);
    if (result != MA_SUCCESS) {
        std::cerr << "Failed to initialize audio engine!" << std::endl;
        return false;
    }

    initialized = true;
    std::cout << "Audio engine initialized successfully" << std::endl;
    return true;
}

void AudioManager::shutdown() {
    if (!initialized) return;

    stopMusic();
    ma_engine_uninit(&engine);
    initialized = false;
}

bool AudioManager::playMusic(const std::string& filepath, float volume) {
    if (!initialized) return false;

    // Stop current music if playing
    stopMusic();

    // Create new music sound
    currentMusic = new ma_sound();

    ma_result result = ma_sound_init_from_file(
        &engine,
        filepath.c_str(),
        MA_SOUND_FLAG_STREAM,  // Stream from disk (good for music)
        nullptr,
        nullptr,
        currentMusic
    );

    if (result != MA_SUCCESS) {
        std::cerr << "Failed to load music: " << filepath << std::endl;
        delete currentMusic;
        currentMusic = nullptr;
        return false;
    }

    ma_sound_set_looping(currentMusic, MA_TRUE);
    ma_sound_set_volume(currentMusic, volume * masterVolume);
    ma_sound_start(currentMusic);

    std::cout << "Playing music: " << filepath << std::endl;
    return true;
}

void AudioManager::stopMusic() {
    if (currentMusic) {
        ma_sound_stop(currentMusic);
        ma_sound_uninit(currentMusic);
        delete currentMusic;
        currentMusic = nullptr;
    }
}

void AudioManager::pauseMusic() {
    if (currentMusic) {
        ma_sound_stop(currentMusic);
    }
}

void AudioManager::resumeMusic() {
    if (currentMusic) {
        ma_sound_start(currentMusic);
    }
}

void AudioManager::setMusicVolume(float volume) {
    if (currentMusic) {
        ma_sound_set_volume(currentMusic, volume * masterVolume);
    }
}

bool AudioManager::isMusicPlaying() const {
    if (currentMusic) {
        return ma_sound_is_playing(currentMusic);
    }
    return false;
}

bool AudioManager::playSound(const std::string& filepath, float volume) {
    if (!initialized) return false;

    // Fire and forget - miniaudio handles cleanup
    ma_result result = ma_engine_play_sound(&engine, filepath.c_str(), nullptr);

    if (result != MA_SUCCESS) {
        std::cerr << "Failed to play sound: " << filepath << std::endl;
        return false;
    }

    return true;
}

void AudioManager::setMasterVolume(float volume) {
    masterVolume = volume;
    ma_engine_set_volume(&engine, volume);
}