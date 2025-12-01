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
#include "rtc/rtc.hpp"
#include "imu/imu.hpp"

class SystemManager {
private:
    bool initialized = false;
    bool sleeping = false;
    unsigned long last_activity_time = 0;
    static constexpr unsigned long LIGHT_SLEEP_TIMEOUT = 30000;  // 30 seconds
    
    Logger* logger = nullptr;
    TwoWire* i2c = nullptr;
    PMU pmu;
    FSManager fsManager;
    Display display;
    TouchController touchController;
    RTC rtc;
    IMU imu;

    void sleep();
    void wakeup();
    void logHeartbeat();
public:
    SystemManager(Logger* logger);
    bool isInitialized() const { return initialized; }
    PMU& getPMU() { return pmu; }
    Display& getDisplay() { return display; }
    RTC& getRTC() { return rtc; }
    IMU& getIMU() { return imu; }
    Logger* getLogger() { return logger; }
    TwoWire* getI2C() { return i2c; }
    
    void update();
};