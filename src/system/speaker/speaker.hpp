#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "driver/i2s.h"
#include "config.h"
#include "logger/logger.hpp"

extern "C" {
    #include "es8311.h"
}

#define SPEAKER_SAMPLE_RATE     16000
#define SPEAKER_VOLUME_DEFAULT  80
#define SPEAKER_I2S_PORT        I2S_NUM_0

class Speaker {
private:
    Logger* logger = nullptr;
    bool initialized = false;
    es8311_handle_t es_handle = nullptr;

public:
    Speaker(Logger* logger) : logger(logger) {}

    bool begin();

    bool isInitialized() const { return initialized; }

    // Play raw PCM audio (16-bit signed, stereo, SPEAKER_SAMPLE_RATE Hz)
    size_t play(const uint8_t* data, size_t len);

    // Generate and play a square-wave tone (freq in Hz, duration in ms)
    void beep(uint16_t freq = 1000, uint16_t durationMs = 200);

    void setVolume(int volume);
    void mute(bool enable);
};
