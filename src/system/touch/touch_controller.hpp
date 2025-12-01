#pragma once
#include <Arduino.h>
#include <Wire.h>

#include "config.h"
#include "../../logger/logger.hpp"

class TouchController {
private:
    static constexpr uint8_t ADDR_FT3168 = 0x38;
    static constexpr uint8_t DEV_ID = 3;

    uint8_t i2c_addr = ADDR_FT3168; // default I2C address
    uint8_t interrupt_pin = TOUCH_INT;
    uint8_t reset_pin = TOUCH_RST;
    TwoWire* i2c = nullptr;
    Logger* logger = nullptr;
    bool initialized = false;
    volatile bool touch_event = false;
    
    // Software gesture detection
    bool touch_active = false;
    bool long_press_fired = false;
    uint16_t touch_start_x = 0;
    uint16_t touch_start_y = 0;
    uint16_t touch_last_x = 0;
    uint16_t touch_last_y = 0;
    uint32_t touch_start_time = 0;

    bool init();
    static void IRAM_ATTR isrArg(void* arg);
    bool safeReadRegisters(uint8_t reg, uint8_t *buf, size_t len, int retries = 3);
public:
    // Constructor: optionally specify I2C address for different FT3x68 variants
    TouchController(Logger* logger) { this->logger = logger; };
    bool setBus(TwoWire &bus);

    void handleInterrupt();

    bool available();
    bool readTouch(uint16_t &x, uint16_t &y);

    // FT3x68 register map (use these instead of global macros)
    enum Registers : uint8_t {
        REG_GESTURE_ID = 0xD3,
        REG_FINGER_NUM = 0x02,
        REG_X1_POSH = 0x03,
        REG_X1_POSL = 0x04,
        REG_Y1_POSH = 0x05,
        REG_Y1_POSL = 0x06,
        REG_X2_POSH = 0x09,
        REG_X2_POSL = 0x0A,
        REG_Y2_POSH = 0x0B,
        REG_Y2_POSL = 0x0C,
        REG_GESTURE_MODE = 0xD0,
        REG_POWER_MODE = 0xA5,
        REG_PROXIMITY_MODE = 0xB0,
        REG_DEVICE_ID = 0xA0,
    };
};