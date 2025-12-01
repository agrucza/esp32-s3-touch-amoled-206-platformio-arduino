#pragma once

#include <Arduino.h>

#include <HWCDC.h>
#include <LittleFS.h>

#include "config.h"
#include "pmu/pmu.hpp"
#include "display/display.hpp"
#include "button/button.hpp"
#include "storage/fs_manager.hpp"
#include "touch/touch_controller.hpp"

class SystemManager {
private:
    bool initialized = false;
    bool sleeping = false;
    Logger* logger = nullptr;
    TwoWire* i2c = nullptr;
    PMU pmu;
    FSManager fsManager;
    Display display;
    TouchController touchController;

    void sleep();
    void wakeup();
    void logHeartbeat();
public:
    SystemManager(Logger* logger);
    bool isInitialized() const { return initialized; }
    PMU& getPMU() { return pmu; }
    Display& getDisplay() { return display; }
    Logger* getLogger() { return logger; }
    TwoWire* getI2C() { return i2c; }
    
    void update();
};