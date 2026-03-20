#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "driver/i2s.h"
#include "config.h"
#include "logger/logger.hpp"

// ES7210 I2C address (AD0=AD1=GND on Waveshare board)
#define ES7210_I2C_ADDR     0x40

// Same I2S port as speaker — already installed in TX+RX duplex mode
#define MIC_I2S_PORT        I2S_NUM_0

class Mic {
private:
    Logger* logger = nullptr;
    bool initialized = false;

    esp_err_t writeReg(uint8_t reg, uint8_t val);

public:
    Mic(Logger* logger) : logger(logger) {}

    bool begin();
    bool isInitialized() const { return initialized; }

    // Read raw PCM samples (16-bit signed stereo, same rate as speaker)
    // Returns bytes actually read, or 0 on error.
    size_t read(int16_t* buf, size_t samples, uint32_t timeoutMs = 100);
};
