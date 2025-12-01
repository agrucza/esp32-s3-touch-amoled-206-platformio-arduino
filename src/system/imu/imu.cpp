#include "imu.hpp"

volatile bool IMU::motion_detected = false;

void IRAM_ATTR IMU::motionISR() {
    motion_detected = true;
}

bool IMU::setBus(TwoWire &bus) {
    i2c = &bus;
    interrupt_pin = IMU_INT2;
    
    // Read chip ID
    uint8_t whoami = 0;
    if (!readRegister(REG_WHO_AM_I, &whoami)) {
        if (logger != nullptr) logger->failure("IMU", "QMI8658 not found");
        return false;
    }
    
    if (logger != nullptr) logger->info("IMU", (String("Chip ID: 0x") + String(whoami, HEX)).c_str());
    
    // Software reset
    writeRegister(REG_RESET, 0xB0);
    delay(10);
    
    // Configure CTRL1: Set serial interface, address auto increment, and INT pins
    // Bit 6: address auto increment (1)
    // Bit 3-2: INT pin config (11 = push-pull, active high)
    if (!writeRegister(REG_CTRL1, 0x4C)) {
        if (logger != nullptr) logger->failure("IMU", "Failed to configure CTRL1");
        return false;
    }
    
    // Configure accelerometer: 8g range, 128Hz ODR
    // CTRL2: [7:4] = accel range (0011 = 8g), [3:0] = ODR (0110 = 128Hz)
    if (!writeRegister(REG_CTRL2, 0x36)) {
        if (logger != nullptr) logger->failure("IMU", "Failed to configure accelerometer");
        return false;
    }
    
    // Configure gyroscope: 1024dps range, 128Hz ODR
    // CTRL3: [7:4] = gyro range (0110 = 1024dps), [3:0] = ODR (0110 = 128Hz)
    if (!writeRegister(REG_CTRL3, 0x66)) {
        if (logger != nullptr) logger->failure("IMU", "Failed to configure gyroscope");
        return false;
    }
    
    // Enable accelerometer and gyroscope with syncSmpl
    // CTRL7: [7] = syncSmpl (1 = level mode INT2), [1] = enable gyro, [0] = enable accel
    if (!writeRegister(REG_CTRL7, 0x83)) {  // 0x83 = syncSmpl + gyro + accel
        if (logger != nullptr) logger->failure("IMU", "Failed to enable sensors");
        return false;
    }
    
    delay(50);
    
    // Setup INT2 for data ready interrupt (not INT1!)
    interrupt_pin = IMU_INT2;
    pinMode(interrupt_pin, INPUT);
    attachInterrupt(digitalPinToInterrupt(interrupt_pin), motionISR, RISING);
    
    // Read back CTRL7 for verification
    uint8_t ctrl7_read = 0;
    if (readRegister(REG_CTRL7, &ctrl7_read)) {
        if (logger != nullptr) logger->info("IMU", (String("CTRL7 readback: 0x") + String(ctrl7_read, HEX)).c_str());
    }
    
    if (logger != nullptr) logger->info("IMU", (String("Data Ready interrupt (INT2) on GPIO") + String(interrupt_pin)).c_str());
    
    if (logger != nullptr) logger->success("IMU", "QMI8658 initialized");
    initialized = true;
    return true;
}

bool IMU::writeRegister(uint8_t reg, uint8_t value) {
    if (!i2c) return false;
    
    i2c->beginTransmission(ADDR_QMI8658);
    i2c->write(reg);
    i2c->write(value);
    return (i2c->endTransmission() == 0);
}

bool IMU::readRegister(uint8_t reg, uint8_t* value) {
    return readRegisters(reg, value, 1);
}

bool IMU::readRegisters(uint8_t reg, uint8_t* buffer, size_t len) {
    if (!i2c) return false;
    
    i2c->beginTransmission(ADDR_QMI8658);
    i2c->write(reg);
    if (i2c->endTransmission(false) != 0) return false;
    
    if (i2c->requestFrom(ADDR_QMI8658, len) != len) return false;
    
    for (size_t i = 0; i < len; i++) {
        buffer[i] = i2c->read();
    }
    
    return true;
}

bool IMU::readAccel(AccelData& data) {
    if (!initialized) return false;
    
    uint8_t raw[6];
    if (!readRegisters(REG_AX_L, raw, 6)) return false;
    
    // Combine bytes (little endian)
    int16_t ax = (int16_t)(raw[1] << 8 | raw[0]);
    int16_t ay = (int16_t)(raw[3] << 8 | raw[2]);
    int16_t az = (int16_t)(raw[5] << 8 | raw[4]);
    
    // Convert to g (8g range, 16-bit)
    const float scale = 8.0f / 32768.0f;
    data.x = ax * scale;
    data.y = ay * scale;
    data.z = az * scale;
    
    return true;
}

bool IMU::readGyro(GyroData& data) {
    if (!initialized) return false;
    
    uint8_t raw[6];
    if (!readRegisters(REG_GX_L, raw, 6)) return false;
    
    // Combine bytes (little endian)
    int16_t gx = (int16_t)(raw[1] << 8 | raw[0]);
    int16_t gy = (int16_t)(raw[3] << 8 | raw[2]);
    int16_t gz = (int16_t)(raw[5] << 8 | raw[4]);
    
    // Convert to dps (1024dps range, 16-bit)
    const float scale = 1024.0f / 32768.0f;
    data.x = gx * scale;
    data.y = gy * scale;
    data.z = gz * scale;
    
    return true;
}

bool IMU::readTemperature(float& temp) {
    if (!initialized) return false;
    
    uint8_t raw[2];
    if (!readRegisters(REG_TEMP_L, raw, 2)) return false;
    
    int16_t temp_raw = (int16_t)(raw[1] << 8 | raw[0]);
    
    // Convert to celsius (datasheet formula)
    temp = temp_raw / 256.0f;
    
    return true;
}

bool IMU::checkWristTilt() {
    if (!initialized) return false;
    
    // Rate limiting: only check every 50ms
    static unsigned long last_check = 0;
    unsigned long now = millis();
    if (now - last_check < 50) return false;
    last_check = now;
    
    AccelData accel;
    GyroData gyro;
    
    if (!readAccel(accel) || !readGyro(gyro)) return false;
    
    // Simple state machine: remember rotation, wait for target position
    static enum { IDLE, TRIGGERED } state = IDLE;
    static unsigned long last_rotation_time = 0;
    static unsigned long state_time = 0;
    
    // Target position: WATCH_UP (X > 0.2, Z < -0.2)
    bool watch_up = (accel.x > 0.20f && accel.z < -0.20f);
    bool strong_rotation = (abs(gyro.x) > 40.0f || abs(gyro.y) > 40.0f || abs(gyro.z) > 40.0f);
    
    // Remember when we last saw rotation
    if (strong_rotation) {
        last_rotation_time = now;
    }
    
    switch (state) {
        case IDLE:
            // If we see watch_up position AND had rotation in last 1.5 seconds = gesture!
            if (watch_up && (now - last_rotation_time < 1500)) {
                state = TRIGGERED;
                state_time = now;
                if (logger != nullptr) logger->info("IMU_TILT", "✓ Wrist raise gesture!");
                return true;
            }
            break;
            
        case TRIGGERED:
            // Cooldown: wait 1s, then back to IDLE when NOT in watch_up position
            if (now - state_time > 1000 && !watch_up) {
                state = IDLE;
            }
            break;
    }
    
    return false;
}

bool IMU::checkWristTiltDown() {
    if (!initialized) return false;
    
    // Rate limiting: only check every 50ms
    static unsigned long last_check = 0;
    unsigned long now = millis();
    if (now - last_check < 50) return false;
    last_check = now;
    
    AccelData accel;
    GyroData gyro;
    
    if (!readAccel(accel) || !readGyro(gyro)) return false;
    
    // Simple state machine: remember rotation, wait for target position
    static enum { IDLE, TRIGGERED } state = IDLE;
    static unsigned long last_rotation_time = 0;
    static unsigned long state_time = 0;
    
    // Target positions
    bool watch_up = (accel.x > 0.20f && accel.z < -0.20f);
    bool arm_down_standing = (accel.y < -0.35f);
    bool arm_down_sitting = (accel.y > 0.10f && accel.z < -0.40f);
    bool arm_down = arm_down_standing || arm_down_sitting;
    bool strong_rotation = (abs(gyro.x) > 40.0f || abs(gyro.y) > 40.0f || abs(gyro.z) > 40.0f);
    
    // Remember when we last saw rotation
    if (strong_rotation) {
        last_rotation_time = now;
    }
    
    switch (state) {
        case IDLE:
            // If we see arm_down position AND had rotation in last 1.5 seconds = gesture!
            if (arm_down && (now - last_rotation_time < 1500)) {
                state = TRIGGERED;
                state_time = now;
                if (logger != nullptr) logger->info("IMU_TILT", "✓ Wrist lowered - sleep!");
                return true;
            }
            break;
            
        case TRIGGERED:
            // Cooldown: wait 1s, then back to IDLE when NOT in arm_down position
            if (now - state_time > 1000 && !arm_down) {
                state = IDLE;
            }
            break;
    }
    
    return false;
}

bool IMU::checkDataReadyStatus() {
    if (!initialized) return false;
    
    uint8_t status0 = 0;
    if (!readRegister(REG_STATUS0, &status0)) return false;
    
    // Reading STATUS0 clears INT2 in syncSmpl mode
    // Bit 0: Accel data ready, Bit 1: Gyro data ready
    return (status0 & 0x03) == 0x03;  // Both accel and gyro ready
}

bool IMU::checkMotion() {
    if (!initialized) return false;
    
    // Rate limiting: only check every 100ms
    static unsigned long last_check = 0;
    unsigned long now = millis();
    if (now - last_check < 100) return false;
    last_check = now;
    
    AccelData accel;
    if (!readAccel(accel)) return false;
    
    // Calculate magnitude of acceleration vector
    float magnitude = sqrt(accel.x * accel.x + accel.y * accel.y + accel.z * accel.z);
    
    // Initialize on first run
    if (last_accel_magnitude == 0.0f) {
        last_accel_magnitude = magnitude;
        return false;
    }
    
    // Check if change in magnitude exceeds threshold
    float delta = abs(magnitude - last_accel_magnitude);
    last_accel_magnitude = magnitude;
    
    // Motion detected
    if (delta > motion_threshold) {
        // Debounce: only report motion once per 2 seconds
        if (now - last_motion_time > 2000) {
            last_motion_time = now;
            return true;
        }
    }
    
    return false;
}
