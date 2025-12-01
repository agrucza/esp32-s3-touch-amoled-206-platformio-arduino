#pragma once
#include <Arduino.h>
#include <Wire.h>

#include "config.h"
#include "../../logger/logger.hpp"

class IMU {
private:
    static constexpr uint8_t ADDR_QMI8658 = 0x6B;
    static constexpr uint8_t CHIP_ID = 0x05;
    
    TwoWire* i2c = nullptr;
    Logger* logger = nullptr;
    bool initialized = false;
    uint8_t interrupt_pin = 21;
    static volatile bool motion_detected;
    static void IRAM_ATTR motionISR();
    
    // Software motion detection
    float last_accel_magnitude = 0.0f;
    float motion_threshold = 0.15f;  // g threshold for motion (walking ~0.2g, running ~0.5g)
    unsigned long last_motion_time = 0;
    
    // QMI8658 Register addresses
    enum Registers : uint8_t {
        REG_WHO_AM_I = 0x00,
        REG_REVISION_ID = 0x01,
        REG_CTRL1 = 0x02,
        REG_CTRL2 = 0x03,
        REG_CTRL3 = 0x04,
        REG_CTRL4 = 0x05,
        REG_CTRL5 = 0x06,
        REG_CTRL6 = 0x07,
        REG_CTRL7 = 0x08,
        REG_CTRL8 = 0x09,
        REG_CTRL9 = 0x0A,
        REG_CAL1_L = 0x0B,
        REG_CAL1_H = 0x0C,
        REG_CAL2_L = 0x0D,
        REG_CAL2_H = 0x0E,
        REG_CAL3_L = 0x0F,
        REG_CAL3_H = 0x10,
        REG_CAL4_L = 0x11,
        REG_CAL4_H = 0x12,
        REG_FIFO_WTM_TH = 0x13,
        REG_FIFO_CTRL = 0x14,
        REG_FIFO_SMPL_CNT = 0x15,
        REG_FIFO_STATUS = 0x16,
        REG_FIFO_DATA = 0x17,
        REG_I2CM_STATUS = 0x2C,
        REG_STATUSINT = 0x2D,
        REG_STATUS0 = 0x2E,
        REG_STATUS1 = 0x2F,
        REG_TIMESTAMP_L = 0x30,
        REG_TEMP_L = 0x33,
        REG_TEMP_H = 0x34,
        REG_AX_L = 0x35,
        REG_AX_H = 0x36,
        REG_AY_L = 0x37,
        REG_AY_H = 0x38,
        REG_AZ_L = 0x39,
        REG_AZ_H = 0x3A,
        REG_GX_L = 0x3B,
        REG_GX_H = 0x3C,
        REG_GY_L = 0x3D,
        REG_GY_H = 0x3E,
        REG_GZ_L = 0x3F,
        REG_GZ_H = 0x40,
        REG_dQW_L = 0x49,
        REG_dQW_H = 0x4A,
        REG_dQX_L = 0x4B,
        REG_dQX_H = 0x4C,
        REG_dQY_L = 0x4D,
        REG_dQY_H = 0x4E,
        REG_dQZ_L = 0x4F,
        REG_dQZ_H = 0x50,
        REG_dVX_L = 0x51,
        REG_dVX_H = 0x52,
        REG_dVY_L = 0x53,
        REG_dVY_H = 0x54,
        REG_dVZ_L = 0x55,
        REG_dVZ_H = 0x56,
        REG_RESET = 0x60,
        REG_STEP_CNT_LOW = 0x07,   // Step counter low byte
        REG_STEP_CNT_MID = 0x08,   // Step counter mid byte  
        REG_STEP_CNT_HIGH = 0x09,  // Step counter high byte
    };
    
    enum MotionInterruptMode : uint8_t {
        MOTION_ANY = 0,         // Any motion
        MOTION_NO = 1,          // No motion
        MOTION_SIGNIFICANT = 2  // Significant motion
    };
    
    bool writeRegister(uint8_t reg, uint8_t value);
    bool readRegister(uint8_t reg, uint8_t* value);
    bool readRegisters(uint8_t reg, uint8_t* buffer, size_t len);

public:
    struct AccelData {
        float x;  // g
        float y;  // g
        float z;  // g
    };
    
    struct GyroData {
        float x;  // dps (degrees per second)
        float y;  // dps
        float z;  // dps
    };
    
    IMU(Logger* logger) : logger(logger) {}
    
    bool setBus(TwoWire& bus);
    bool isInitialized() const { return initialized; }
    bool readAccel(AccelData& data);
    bool readGyro(GyroData& data);
    bool readTemperature(float& temp);
    
    // Data ready interrupt
    bool isDataReady() { return motion_detected; }
    void clearDataReadyFlag() { motion_detected = false; }
    bool checkDataReadyStatus();  // Poll STATUS0 register instead of interrupt
    
    // Software motion detection
    bool checkMotion();  // Returns true if significant motion detected
    bool checkWristTilt();  // Returns true if wrist raise/tilt gesture detected
    bool checkWristTiltDown();  // Returns true if arm lowered (watch down)
    void setMotionThreshold(float threshold_g) { motion_threshold = threshold_g; }
    float getMotionThreshold() const { return motion_threshold; }
};
