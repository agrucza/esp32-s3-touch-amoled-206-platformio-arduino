#pragma once

#include <Arduino.h>

#include <HWCDC.h>
#include <Wire.h>

#include "config.h"
#include "pmu/pmu.hpp"
#include "display/display.hpp"

class SystemManager {
private:
    HWCDC* usbSerial;
    TwoWire* i2c = nullptr;
    PMU pmu;
    Display display;
    //TouchController touch;
public:
    SystemManager() : usbSerial(nullptr), i2c(nullptr) {}
    bool init(HWCDC &USBSerial);
    void update();
    PMU& getPMU() { return pmu; }
    Display& getDisplay() { return display; }
    HWCDC* getUSBSerial() { return usbSerial; }
    TwoWire* getI2C() { return i2c; }
};