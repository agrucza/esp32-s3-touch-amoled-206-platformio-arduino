#include "rtc.hpp"

bool RTC::setBus(TwoWire &bus) {
    i2c = &bus;
    
    // Test communication by reading control register
    uint8_t ctrl1 = 0;
    if (!readRegister(REG_CONTROL_1, &ctrl1)) {
        if (logger) logger->failure("RTC", "PCF85063 not found");
        return false;
    }
    
    // Enable RTC, disable 12h mode (use 24h)
    if (!writeRegister(REG_CONTROL_1, 0x00)) {
        if (logger) logger->failure("RTC", "Failed to configure PCF85063");
        return false;
    }
    
    // Setup interrupt pin
    pinMode(interrupt_pin, INPUT_PULLUP);
    attachInterruptArg(digitalPinToInterrupt(interrupt_pin), RTC::isrArg, this, FALLING);
    
    if (logger) logger->success("RTC", "PCF85063 initialized");
    initialized = true;
    return true;
}

void IRAM_ATTR RTC::isrArg(void* arg) {
    RTC* self = static_cast<RTC*>(arg);
    if (self) {
        // Check which interrupt fired by reading CONTROL_2
        // We'll set flags and check them in main loop
        self->alarm_triggered = true;  // Could be alarm, timer, or minute
        self->timer_triggered = true;
        self->minute_triggered = true;
    }
}

bool RTC::writeRegister(uint8_t reg, uint8_t value) {
    if (!i2c) return false;
    
    i2c->beginTransmission(ADDR_PCF85063);
    i2c->write(reg);
    i2c->write(value);
    return (i2c->endTransmission() == 0);
}

bool RTC::readRegister(uint8_t reg, uint8_t* value) {
    return readRegisters(reg, value, 1);
}

bool RTC::readRegisters(uint8_t reg, uint8_t* buffer, size_t len) {
    if (!i2c) return false;
    
    i2c->beginTransmission(ADDR_PCF85063);
    i2c->write(reg);
    if (i2c->endTransmission(false) != 0) return false;
    
    if (i2c->requestFrom(ADDR_PCF85063, len) != len) return false;
    
    for (size_t i = 0; i < len; i++) {
        buffer[i] = i2c->read();
    }
    
    return true;
}

bool RTC::setDateTime(const DateTime& dt) {
    if (!initialized) return false;
    
    uint8_t data[7];
    data[0] = decToBcd(dt.second) & 0x7F;  // Clear OS flag
    data[1] = decToBcd(dt.minute);
    data[2] = decToBcd(dt.hour);
    data[3] = decToBcd(dt.day);
    data[4] = dt.weekday & 0x07;
    data[5] = decToBcd(dt.month);
    data[6] = decToBcd(dt.year - 2000);
    
    // Write all time/date registers at once
    i2c->beginTransmission(ADDR_PCF85063);
    i2c->write(REG_SECONDS);
    for (int i = 0; i < 7; i++) {
        i2c->write(data[i]);
    }
    
    bool success = (i2c->endTransmission() == 0);
    
    if (logger) {
        if (success) {
            logger->success("RTC", "Date/Time set");
        } else {
            logger->failure("RTC", "Failed to set Date/Time");
        }
    }
    
    return success;
}

bool RTC::getDateTime(DateTime& dt) {
    if (!initialized) return false;
    
    uint8_t data[7];
    if (!readRegisters(REG_SECONDS, data, 7)) return false;
    
    dt.second = bcdToDec(data[0] & 0x7F);
    dt.minute = bcdToDec(data[1] & 0x7F);
    dt.hour = bcdToDec(data[2] & 0x3F);
    dt.day = bcdToDec(data[3] & 0x3F);
    dt.weekday = data[4] & 0x07;
    dt.month = bcdToDec(data[5] & 0x1F);
    dt.year = bcdToDec(data[6]) + 2000;
    
    return true;
}

bool RTC::setTime(uint8_t hour, uint8_t minute, uint8_t second) {
    DateTime dt;
    if (!getDateTime(dt)) return false;
    
    dt.hour = hour;
    dt.minute = minute;
    dt.second = second;
    
    return setDateTime(dt);
}

bool RTC::setDate(uint16_t year, uint8_t month, uint8_t day) {
    DateTime dt;
    if (!getDateTime(dt)) return false;
    
    dt.year = year;
    dt.month = month;
    dt.day = day;
    
    return setDateTime(dt);
}

bool RTC::setAlarm(uint8_t hour, uint8_t minute, uint8_t second, uint8_t day) {
    if (!initialized) return false;
    
    // 0x80 = alarm disabled for that field
    uint8_t alarm_second = (second == 0xFF) ? 0x80 : (decToBcd(second) & 0x7F);
    uint8_t alarm_minute = (minute == 0xFF) ? 0x80 : (decToBcd(minute) & 0x7F);
    uint8_t alarm_hour = (hour == 0xFF) ? 0x80 : (decToBcd(hour) & 0x3F);
    uint8_t alarm_day = (day == 0xFF) ? 0x80 : (decToBcd(day) & 0x3F);
    uint8_t alarm_weekday = 0x80; // Disable weekday alarm
    
    // Write alarm registers
    if (!writeRegister(REG_SECOND_ALARM, alarm_second)) return false;
    if (!writeRegister(REG_MINUTE_ALARM, alarm_minute)) return false;
    if (!writeRegister(REG_HOUR_ALARM, alarm_hour)) return false;
    if (!writeRegister(REG_DAY_ALARM, alarm_day)) return false;
    if (!writeRegister(REG_WEEKDAY_ALARM, alarm_weekday)) return false;
    
    // Enable alarm interrupt in CONTROL_2 (bit 7 = AIE)
    uint8_t ctrl2 = 0;
    if (!readRegister(REG_CONTROL_2, &ctrl2)) return false;
    ctrl2 |= 0x80; // Set AIE bit
    if (!writeRegister(REG_CONTROL_2, ctrl2)) return false;
    
    if (logger) {
        logger->success("RTC", (String("Alarm set: ") + String(hour) + ":" + String(minute)).c_str());
    }
    
    return true;
}

bool RTC::clearAlarm() {
    if (!initialized) return false;
    
    // Disable alarm interrupt in CONTROL_2
    uint8_t ctrl2 = 0;
    if (!readRegister(REG_CONTROL_2, &ctrl2)) return false;
    ctrl2 &= ~0x80; // Clear AIE bit
    ctrl2 &= ~0x40; // Clear AF (alarm flag)
    if (!writeRegister(REG_CONTROL_2, ctrl2)) return false;
    
    alarm_triggered = false;
    
    if (logger) logger->info("RTC", "Alarm cleared");
    
    return true;
}

bool RTC::setTimer(uint8_t value, TimerClockFreq freq) {
    if (!initialized) return false;
    
    // PCF85063 uses OFFSET register as timer countdown
    if (!writeRegister(REG_OFFSET, value)) return false;
    
    // Enable timer interrupt in CONTROL_1
    // Bits [2:0] = timer frequency
    uint8_t ctrl1 = freq & 0x03;
    ctrl1 |= 0x04;  // Enable timer
    if (!writeRegister(REG_CONTROL_1, ctrl1)) return false;
    
    // Enable timer interrupt in CONTROL_2 (bit 4 = TIE)
    uint8_t ctrl2 = 0;
    if (!readRegister(REG_CONTROL_2, &ctrl2)) return false;
    ctrl2 |= 0x10;  // Set TIE bit
    if (!writeRegister(REG_CONTROL_2, ctrl2)) return false;
    
    if (logger) {
        const char* freq_str[] = {"4096Hz", "64Hz", "1Hz", "1/60Hz"};
        logger->info("RTC", (String("Timer set: ") + String(value) + " ticks @ " + freq_str[freq]).c_str());
    }
    
    return true;
}

bool RTC::clearTimer() {
    if (!initialized) return false;
    
    // Disable timer in CONTROL_1
    uint8_t ctrl1 = 0;
    if (!readRegister(REG_CONTROL_1, &ctrl1)) return false;
    ctrl1 &= ~0x04;  // Clear timer enable
    if (!writeRegister(REG_CONTROL_1, ctrl1)) return false;
    
    // Disable timer interrupt in CONTROL_2
    uint8_t ctrl2 = 0;
    if (!readRegister(REG_CONTROL_2, &ctrl2)) return false;
    ctrl2 &= ~0x10;  // Clear TIE bit
    ctrl2 &= ~0x08;  // Clear TF (timer flag)
    if (!writeRegister(REG_CONTROL_2, ctrl2)) return false;
    
    timer_triggered = false;
    
    if (logger) logger->info("RTC", "Timer cleared");
    
    return true;
}

bool RTC::enableMinuteInterrupt() {
    if (!initialized) return false;
    
    // Enable minute interrupt in CONTROL_2 (bit 0 = MI)
    uint8_t ctrl2 = 0;
    if (!readRegister(REG_CONTROL_2, &ctrl2)) return false;
    ctrl2 |= 0x01;  // Set MI bit
    if (!writeRegister(REG_CONTROL_2, ctrl2)) return false;
    
    if (logger) logger->info("RTC", "Minute interrupt enabled");
    
    return true;
}

bool RTC::disableMinuteInterrupt() {
    if (!initialized) return false;
    
    // Disable minute interrupt in CONTROL_2
    uint8_t ctrl2 = 0;
    if (!readRegister(REG_CONTROL_2, &ctrl2)) return false;
    ctrl2 &= ~0x01;  // Clear MI bit
    if (!writeRegister(REG_CONTROL_2, ctrl2)) return false;
    
    minute_triggered = false;
    
    if (logger) logger->info("RTC", "Minute interrupt disabled");
    
    return true;
}

bool RTC::setClockOut(ClockOutFreq freq) {
    if (!initialized) return false;
    
    // Set CLKOUT frequency in CONTROL_1 bits [7:5]
    uint8_t ctrl1 = 0;
    if (!readRegister(REG_CONTROL_1, &ctrl1)) return false;
    ctrl1 = (ctrl1 & 0x1F) | ((freq & 0x07) << 5);
    if (!writeRegister(REG_CONTROL_1, ctrl1)) return false;
    
    if (logger) {
        const char* freq_str[] = {"32768Hz", "16384Hz", "8192Hz", "4096Hz", "2048Hz", "1024Hz", "1Hz", "OFF"};
        logger->info("RTC", (String("CLKOUT set to ") + freq_str[freq]).c_str());
    }
    
    return true;
}
