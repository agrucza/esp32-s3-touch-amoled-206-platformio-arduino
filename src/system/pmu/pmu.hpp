#pragma once
#include "config.h"

#include "XPowersAXP2101.tpp"

class PMU {
private:
    XPowersAXP2101 pmu;
    uint8_t pmuAddress = 0x34;
    bool initialized = false;
public:
    bool init(HWCDC &serial, TwoWire &wire);
    bool isBatteryConnect();
    bool isCharging();
    bool isUSBConnected();
    uint8_t getBatteryPercent();
    uint16_t getBattVoltage();
};