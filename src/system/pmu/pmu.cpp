#include "pmu.hpp"

bool PMU::init(HWCDC &serial, TwoWire &wire) {
    serial.println("# Initializing PMU (AXP2101)...");
    if(!this->pmu.begin(wire, this->pmuAddress, PMU_SDA, PMU_SCL)) {
        serial.printf("✗ AXP2101 not found at address 0x%02X\n", pmuAddress);
        initialized = false;
        return false;
    }
    serial.printf("✓ AXP2101 found at address 0x%02X\n", pmuAddress);
    initialized = true;
    return true;
}

bool PMU::isBatteryConnect() {
    return this->initialized ? this->pmu.isBatteryConnect() : false;
}

bool PMU::isCharging() {
    return this->initialized ? this->pmu.isCharging() : false;
}

bool PMU::isUSBConnected() {
    return this->initialized ? this->pmu.isVbusIn() : false;
}

uint8_t PMU::getBatteryPercent() {
    return this->initialized ? this->pmu.getBatteryPercent() : 0;
}

uint16_t PMU::getBattVoltage() {
    return this->initialized ? this->pmu.getBattVoltage() : 0;
}