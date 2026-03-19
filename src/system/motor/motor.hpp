#pragma once
#include <Arduino.h>
#include "config.h"
#include "../../logger/logger.hpp"

class Motor {
private:
    Logger* logger = nullptr;
    bool initialized = false;

public:
    Motor(Logger* logger) : logger(logger) {}

    void begin() {
        pinMode(MOTOR_PIN, OUTPUT);
        digitalWrite(MOTOR_PIN, LOW);
        initialized = true;
        logger->success("MOTOR", (String("Vibration motor ready on GPIO") + String(MOTOR_PIN)).c_str());
    }

    bool isInitialized() const { return initialized; }

    void on()  { if (initialized) digitalWrite(MOTOR_PIN, HIGH); }
    void off() { if (initialized) digitalWrite(MOTOR_PIN, LOW);  }

    // Single blocking pulse
    void pulse(uint16_t ms) {
        if (!initialized) return;
        logger->debug("MOTOR", (String("GPIO") + String(MOTOR_PIN) + String(" HIGH for ") + String(ms) + String("ms")).c_str());
        on();
        delay(ms);
        off();
        logger->debug("MOTOR", "GPIO LOW");
    }

    // Short confirmation buzz — 200ms for reliable spin-up
    void buzz()   { pulse(200); }

    // Double-tap pattern (e.g. notification)
    void doubleBuzz() {
        pulse(200);
        delay(100);
        pulse(200);
    }

    // Long alert pulse
    void alert() { pulse(500); }
};
