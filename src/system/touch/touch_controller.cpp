#include "touch_controller.hpp"

#include <Arduino.h>

bool TouchController::setBus(TwoWire &bus) {
    i2c = &bus;
    
    return init();
}

bool TouchController::init() {
    if (!i2c) {
        if (this->logger) this->logger->failure("TOUCH", "I2C bus not set");
        initialized = false;
        return false;
    }

    pinMode(reset_pin, OUTPUT);
    digitalWrite(reset_pin, HIGH);
    delay(1);
    digitalWrite(reset_pin, LOW);
    delay(20);
    digitalWrite(reset_pin, HIGH);
    delay(50);

    // Initialize device power mode via direct I2C
    // Retry a few times if the bus reports errors.
    const int init_retries = 3;
    bool init_ok = false;
    for (int attempt = 0; attempt < init_retries; ++attempt) {
        i2c->beginTransmission(i2c_addr);
        i2c->write(TouchController::REG_POWER_MODE);
        i2c->write(0B00000001);
        int err = i2c->endTransmission();
        if (err == 0) { init_ok = true; break; }
        if (this->logger) this->logger->debug("TOUCH", (String("init write failed, attempt=") + attempt + String(" err=") + err + " retrying").c_str());
        delay(10 + attempt * 20);
    }
    if (!init_ok) {
        if (this->logger) this->logger->failure("TOUCH", "initialization write failed");
    } else {
        delay(20);
    }

    // Enable gesture mode
    bool gesture_ok = false;
    for (int attempt = 0; attempt < init_retries; ++attempt) {
        i2c->beginTransmission(i2c_addr);
        i2c->write(TouchController::REG_GESTURE_MODE);
        i2c->write(0x01); // enable gesture
        int err = i2c->endTransmission();
        if (err == 0) { gesture_ok = true; break; }
        delay(10 + attempt * 20);
    }
    if (gesture_ok) {
        if (this->logger) this->logger->info("TOUCH", "Gesture mode enabled");
        // Verify gesture register is readable
        uint8_t g_test = 0;
        if (safeReadRegisters(TouchController::REG_GESTURE_ID, &g_test, 1)) {
            if (this->logger) this->logger->debug("TOUCH", (String("Gesture register reads: 0x") + String(g_test, HEX)).c_str());
        } else {
            if (this->logger) this->logger->warn("TOUCH", "Cannot read gesture register");
        }
    } else {
        if (this->logger) this->logger->warn("TOUCH", "Gesture mode enable failed");
    }

    uint8_t dev_id = 0;
    if (safeReadRegisters(TouchController::REG_DEVICE_ID, &dev_id, 1)) {
        if(dev_id != DEV_ID) {
            if (this->logger) this->logger->warn("TOUCH", (String("Unexpected FT3x68 device id: 0x0") + String(dev_id, HEX) + String(" (") + dev_id + ")").c_str());
            initialized = false;
            return false;
        }
        logger->info("TOUCH", (String("FT3x68 device id: 0x0") + String(dev_id, HEX) + String(" (") + dev_id + ")").c_str());
    } else {
        logger->warn("TOUCH", "Could not read device id after init");
    }

    pinMode(interrupt_pin, INPUT_PULLUP);
    attachInterruptArg(digitalPinToInterrupt(interrupt_pin), TouchController::isrArg, this, FALLING);

    if (this->logger) this->logger->debug("TOUCH", "Interrupt handler attached");
    if (this->logger) this->logger->success("TOUCH", "Touch controller initialized");

    initialized = true;
    touch_event = false;
    
    return true;
}

void IRAM_ATTR TouchController::isrArg(void* arg) {
    TouchController* self = static_cast<TouchController*>(arg);
    if (self) {
        self->touch_event = true;
    }
}

void TouchController::handleInterrupt() {
    // called from non-ISR context (e.g. SystemManager::update())
    if (!touch_event) return;
    touch_event = false; // clear early

    // Read finger count first
    uint8_t fingers = 0;
    if (!safeReadRegisters(TouchController::REG_FINGER_NUM, &fingers, 1)) {
        return; // failed to read finger count
    }

    if (fingers == 0) {
        // Finger released - detect swipe gesture (if no long press was fired)
        if (touch_active) {
            touch_active = false;
            long_press_fired = false; // reset for next touch
            
            int16_t dx = touch_last_x - touch_start_x;
            int16_t dy = touch_last_y - touch_start_y;
            uint32_t duration = millis() - touch_start_time;
            
            int16_t abs_dx = dx < 0 ? -dx : dx;
            int16_t abs_dy = dy < 0 ? -dy : dy;
            
            // Swipe detection: minimum distance (50px) and max duration (800ms)
            if (duration < 800 && (abs_dx > 50 || abs_dy > 50)) {
                String gesture = "";
                
                // Determine edge zones (assume ~480x480 display, adjust if needed)
                const uint16_t edge_threshold = 100; // pixels from edge
                bool from_top = (touch_start_y < edge_threshold);
                bool from_bottom = (touch_start_y > 380);
                bool from_left = (touch_start_x < edge_threshold);
                bool from_right = (touch_start_x > 380);
                
                // Build gesture name from start position + direction
                if (from_top && from_left) gesture = "TopLeft";
                else if (from_top && from_right) gesture = "TopRight";
                else if (from_bottom && from_left) gesture = "BottomLeft";
                else if (from_bottom && from_right) gesture = "BottomRight";
                else if (from_top) gesture = "Top";
                else if (from_bottom) gesture = "Bottom";
                else if (from_left) gesture = "Left";
                else if (from_right) gesture = "Right";
                
                // Add swipe direction
                if (abs_dx > abs_dy) {
                    gesture += (dx > 0) ? " Swipe Right" : " Swipe Left";
                } else {
                    gesture += (dy > 0) ? " Swipe Down" : " Swipe Up";
                }
                
                if (logger) {
                    logger->info("TOUCH", (String("Gesture: ") + gesture).c_str());
                }
            }
        }
        return;
    }

    // Active touch - read coordinates and track for gesture detection
    uint16_t x, y;
    if (readTouch(x, y)) {
        if (!touch_active) {
            // New touch started
            touch_active = true;
            touch_start_x = x;
            touch_start_y = y;
            touch_start_time = millis();
            long_press_fired = false;
        }
        touch_last_x = x;
        touch_last_y = y;
        
        // Check for long press while finger is still down
        if (!long_press_fired && touch_active) {
            uint32_t duration = millis() - touch_start_time;
            if (duration > 500) {
                int16_t dx = x - touch_start_x;
                int16_t dy = y - touch_start_y;
                int16_t abs_dx = dx < 0 ? -dx : dx;
                int16_t abs_dy = dy < 0 ? -dy : dy;
                
                // Fire long press if minimal movement (<20px)
                if (abs_dx < 20 && abs_dy < 20) {
                    long_press_fired = true;
                    if (logger) {
                        logger->info("TOUCH", "Gesture: Long Press");
                    }
                }
            }
        }
    }
}

bool TouchController::safeReadRegisters(uint8_t reg, uint8_t* buf, size_t len, int retries) {
    if (!i2c) return false;

    for (int i=0;i<retries;i++) {
        i2c->beginTransmission(i2c_addr);
        i2c->write(reg);
        if (i2c->endTransmission(false) != 0) { delay(10 + i*10); continue; }
        delayMicroseconds(500);
        size_t got = i2c->requestFrom(static_cast<uint8_t>(i2c_addr), static_cast<size_t>(len));
        if (got < len) { delay(10 + i*10); continue; }
        for (size_t j=0;j<len;j++) buf[j] = i2c->read();
        return true;
    }
    return false;
}

bool TouchController::available() {
    // Check IRQ flag first (set by ISR) to avoid unnecessary bus traffic
    if (touch_event) return true;

    // fallback: poll a status register
    uint8_t fingers = 0;
    if (!safeReadRegisters(TouchController::REG_FINGER_NUM, &fingers, 1)) return false;

    return (fingers != 0);
}

bool TouchController::readTouch(uint16_t &x, uint16_t &y) {
    uint8_t data[4];

    if (!safeReadRegisters(TouchController::REG_X1_POSH, data, 4)) return false;
    // FT3x68: high byte contains flags in upper nibble, data in lower 4 bits
    uint16_t xh = ((uint16_t)(data[0] & 0x0F) << 8) | (uint16_t)data[1];
    uint16_t yh = ((uint16_t)(data[2] & 0x0F) << 8) | (uint16_t)data[3];

    // Basic sanity clamps (display is smaller than 2048 typically)
    if (xh > 4095) xh = 0;
    if (yh > 4095) yh = 0;

    x = xh;
    y = yh;

    // clear IRQ flag on successful read
    touch_event = false;

    return true;
}