#include "system_manager.hpp"

SystemManager::SystemManager(Logger* logger)
    : logger(logger), pmu(logger), display(logger), touchController(logger), fsManager(logger), rtc(logger), imu(logger)
{
    logger->header("SystemManager Initialization");

    // init power button
    pinMode(BTN_BOOT, INPUT_PULLUP);
    
    // Initialize I2C bus (100kHz Standard Mode)
    logger->info("I2C", "Initializing bus at 100kHz...");
    Wire.begin(I2C_SDA, I2C_SCL, 100000);
    this->i2c = &Wire;
    
    Serial.println("I2C scanner start");
    for (uint8_t addr = 1; addr < 127; ++addr) {
        i2c->beginTransmission(addr);
        int r = i2c->endTransmission();
        if (r == 0) {
        Serial.printf("Found I2C device at 0x%02X\n", addr);
        } // ignore other errors here
    }
    Serial.println("I2C scanner done");

    logger->success("I2C", "Bus initialized at 400kHz");

    // Initialize PMU
    logger->info("PMU", "Initializing AXP2101...");
    if (!pmu.setBus(*i2c)) {
        logger->failure("PMU", "AXP2101 initialization failed");
        logger->footer();
        return;
    }

    // Initialize Display
    logger->info("DISPLAY", "Initializing CO5300 AMOLED...");
    if (!display.isInitialized()) {
        logger->failure("DISPLAY", "CO5300 initialization failed");
        logger->footer();
        return;
    }

    // Initialize Touch
    logger->info("TOUCH", "Initializing Touch Controller...");
    if (!touchController.setBus(*i2c)) {
        logger->failure("TOUCH", "Touch Controller initialization failed");
        logger->footer();
        return;
    }

    // Initialize RTC
    logger->info("RTC", "Initializing PCF85063...");
    if (!rtc.setBus(*i2c)) {
        logger->failure("RTC", "PCF85063 initialization failed");
        logger->footer();
        return;
    }
    
    // Set initial time for testing (2025-12-01 14:30:00)
    RTC::DateTime testTime;
    testTime.year = 2025;
    testTime.month = 12;
    testTime.day = 1;
    testTime.hour = 14;
    testTime.minute = 30;
    testTime.second = 0;
    testTime.weekday = 0; // Sunday
    if (rtc.setDateTime(testTime)) {
        logger->info("RTC", "Test time set: 2025-12-01 14:30:00");
    }

    // Initialize IMU
    logger->info("IMU", "Initializing QMI8658...");
    if (!imu.setBus(*i2c)) {
        logger->failure("IMU", "QMI8658 initialization failed");
        logger->footer();
        return;
    }

    // Initialize File System
    logger->info("LittleFS", "Initializing LittleFS...");
    if (!fsManager.isInitialized()) {
        logger->failure("LittleFS", "Failed to initialize LittleFS");
        logger->footer();
        return;
    }
    
    logger->success("SYSTEM", "All components initialized successfully");
    logger->footer();
    
    this->initialized = true;
    return;
}

void SystemManager::update() {
    static unsigned long lastTime = 0;
    unsigned long current_time = millis();
    unsigned long idle_time = current_time - last_activity_time;
    
    // Simple button check
    if (buttonPressed(BTN_BOOT)) {
        this->sleep();
        return;
    }

    touchController.handleInterrupt();
    
    // Check IMU for wrist gestures (has its own rate limiting)
    // Check for wrist tilt UP to wake display
    if (imu.checkWristTilt()) {
        if (sleeping) {
            logger->info("IMU", "⌚ Wrist raise - waking display!");
            display.powerOn();
            sleeping = false;
        }

        last_activity_time = millis();
    }
    
    // Check for wrist tilt DOWN to sleep
    if (imu.checkWristTiltDown()) {
        if (!sleeping) {
            logger->info("IMU", "⌚ Wrist lowered - entering sleep");
            sleep();
        }
    }
    
    // Check RTC alarm
    if (rtc.isAlarmTriggered()) {
        logger->info("RTC", "⏰ ALARM TRIGGERED!");
        rtc.clearAlarmFlag();
        rtc.clearAlarm();
    }
    
    // Auto Sleep Logic
    if (idle_time > LIGHT_SLEEP_TIMEOUT && !sleeping) {
        logger->info("SYSTEM", "Entering light sleep (inactive >30s)");
        sleep();
    }
    
    // Heartbeat every 5 seconds
    if (millis() - lastTime > 5000) {
        lastTime = millis();
        this->logger->debug("SYSTEM", "Heartbeat log");
        this->logHeartbeat();
    }
}

void SystemManager::sleep() {
    logger->info("SYSTEM", "Entering light sleep mode...");

    if(!sleeping) {
        display.powerOff();
        delay(50); // Safely turn off display

        // TODO: sleep peripherals (I2C, PMU, etc.)

        // Wait until button is released (HIGH)
        while (digitalRead(BTN_BOOT) == LOW) {
            delay(10);
        }
    }

    logger->info("SYSTEM", "Button released, preparing for light sleep...");

    sleeping = true;
    esp_sleep_enable_ext0_wakeup((gpio_num_t)BTN_BOOT, 0); // Wakeup on LOW
    esp_sleep_enable_timer_wakeup(1000000); // Wakeup after 1 second (microseconds)
    esp_light_sleep_start();

    // After light sleep: reinitialize display
    logger->info("SYSTEM", "Waking up from light sleep...");
    wakeup();
}

void SystemManager::wakeup() {
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

    if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
        logger->info("SYSTEM", "Woke up by button press");
        display.powerOn();
        sleeping = false;
        last_activity_time = millis();  // Reset idle timer!
    }
}

void SystemManager::logHeartbeat() {
    static unsigned long lastTime = 0;
    static int heartbeat = 0;
    
    // Heartbeat every 5 seconds
    if (millis() - lastTime > 5000) {
        lastTime = millis();
        heartbeat++;

        logger->header((String("SYSTEM HEARTBEAT #") + String(heartbeat)).c_str());
        logger->info("UPTIME", (String(millis() / 1000) + String(" seconds")).c_str());
        
        // Memory Status
        logger->info("MEMORY", (String("Internal RAM Free: ") + String(ESP.getFreeHeap() / 1024) + String(" KB")).c_str());
        logger->info("MEMORY", (String("PSRAM Free: ") + String(ESP.getFreePsram() / 1024) + String(" KB")).c_str());
        logger->info("MEMORY", (String("FLASH Size: ") + String(ESP.getFlashChipSize() / 1024) + String(" KB")).c_str());

        logger->info("BATTERY", (String("Battery Voltage: ") + String(this->getPMU().getBattVoltage()) + String(" mV")).c_str());
        logger->info("BATTERY", (String("Battery Percentage: ") + String(this->getPMU().getBatteryPercent()) + String(" %")).c_str());

        logger->info("BATTERY", (String("USB Connected: ") + String(this->getPMU().isUSBConnected() ? "Yes" : "No")).c_str());
        logger->info("BATTERY", (String("Battery Connected: ") + String(this->getPMU().isBatteryConnect() ? "Yes" : "No")).c_str());
        logger->info("BATTERY", (String("Charging: ") + String(this->getPMU().isCharging() ? "Yes" : "No")).c_str());

        // RTC Status
        if (rtc.isInitialized()) {
            RTC::DateTime dt;
            if (rtc.getDateTime(dt)) {
                char timeStr[32];
                snprintf(timeStr, sizeof(timeStr), "%04d-%02d-%02d %02d:%02d:%02d", 
                         dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
                logger->info("RTC", (String("Current Time: ") + String(timeStr)).c_str());
            } else {
                logger->warn("RTC", "Failed to read time");
            }
        }
        
        // IMU Status
        if (imu.isInitialized()) {
            IMU::AccelData accel;
            IMU::GyroData gyro;
            float temp;
            
            if (imu.readAccel(accel)) {
                logger->info("IMU", (String("Accel: X=") + String(accel.x, 2) + "g Y=" + String(accel.y, 2) + "g Z=" + String(accel.z, 2) + "g").c_str());
            }
            
            if (imu.readGyro(gyro)) {
                logger->info("IMU", (String("Gyro: X=") + String(gyro.x, 1) + "°/s Y=" + String(gyro.y, 1) + "°/s Z=" + String(gyro.z, 1) + "°/s").c_str());
            }
            
            if (imu.readTemperature(temp)) {
                logger->info("IMU", (String("Temperature: ") + String(temp, 1) + "°C").c_str());
            }
        }
        
        logger->footer();
    }
}
