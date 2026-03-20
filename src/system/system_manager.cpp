#include "system_manager.hpp"

SystemManager::SystemManager(Logger* logger)
    : logger(logger), pmu(logger), display(logger), touchController(logger), fsManager(logger), rtc(logger), imu(logger), motor(logger), sdCard(logger), speaker(logger), mic(logger)
{
    logger->header("SystemManager Initialization");

    // init power button
    pinMode(BTN_BOOT, INPUT_PULLUP);
    
    // Initialize I2C bus (100kHz Standard Mode)
    logger->info("I2C", "Initializing bus at 100kHz...");
    Wire.begin(I2C_SDA, I2C_SCL, 100000);
    this->i2c = &Wire;

    logger->debug("I2C", "Scanning bus...");
    for (uint8_t addr = 1; addr < 127; ++addr) {
        i2c->beginTransmission(addr);
        if (i2c->endTransmission() == 0) {
            char buf[32];
            snprintf(buf, sizeof(buf), "Found device at 0x%02X", addr);
            logger->debug("I2C", buf);
        }
    }

    logger->success("I2C", "Bus initialized at 100kHz");

    // Initialize PMU
    logger->info("PMU", "Initializing AXP2101...");
    if (!pmu.setBus(*i2c)) {
        logger->failure("PMU", "AXP2101 initialization failed");
        logger->footer();
        return;
    }

    // Initialize Display (must come after PMU to ensure power rails are up)
    logger->info("DISPLAY", "Initializing CO5300 AMOLED...");
    if (!display.init()) {
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
    
    // Initialize SD Card (optional — system continues if no card present)
    sdCard.begin();

    // Initialize Speaker (optional — system continues if not present)
    speaker.begin();

    // Initialize Motor
    motor.begin();
    motor.doubleBuzz();  // Startup confirmation — two pulses = system ready

    // Speaker startup beep (1kHz, 200ms)
    if (speaker.isInitialized()) {
        speaker.setVolume(70);
        speaker.beep(1000, 200);
    }

    // Initialize Microphone (ES7210) — shares I2S port with speaker
    mic.begin();

    logger->success("SYSTEM", "All components initialized successfully");
    logger->footer();
    
    this->initialized = true;
    return;
}

void SystemManager::update() {
    static unsigned long lastTime = 0;
    unsigned long current_time = millis();
    unsigned long idle_time = current_time - last_activity_time;
    
    // Button check: short press = sleep, long press (>1s) = mic loopback test
    if (digitalRead(BTN_BOOT) == LOW) {
        unsigned long pressStart = millis();
        while (digitalRead(BTN_BOOT) == LOW) delay(10);
        if (millis() - pressStart > 1000) {
            // Long press: loopback mic → speaker for 3 seconds
            logger->info("MIC", "Loopback test: speak now (3s)...");
            static int16_t buf[512];
            // Log first batch of samples for diagnosis
            size_t got = mic.read(buf, 512, 200);
            if (got > 0) {
                int32_t sum = 0;
                int16_t peak = 0;
                for (size_t i = 0; i < got; i++) {
                    sum += abs(buf[i]);
                    if (abs(buf[i]) > abs(peak)) peak = buf[i];
                }
                char dbg[64];
                snprintf(dbg, sizeof(dbg), "samples=%u peak=%d avg=%d",
                         (unsigned)got, (int)peak, (int)(sum / got));
                logger->info("MIC", dbg);
                speaker.play((const uint8_t*)buf, got * sizeof(int16_t));
            }
            unsigned long end = millis() + 2800;
            while (millis() < end) {
                got = mic.read(buf, 512, 50);
                if (got > 0) speaker.play((const uint8_t*)buf, got * sizeof(int16_t));
            }
            logger->info("MIC", "Loopback done");
        } else {
            this->sleep();
        }
        return;
    }

    touchController.handleInterrupt();
    
    // Check IMU for wrist gestures (has its own rate limiting)
    // Check for wrist tilt UP to wake display
    if (imu.checkWristTilt()) {
        if (sleeping) {
            logger->info("IMU", "⌚ Wrist raise - waking display!");
            touchController.wake();
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

    // General motion resets the idle timer (prevents sleep during active use)
    if (imu.checkMotion()) {
        last_activity_time = millis();
    }

    // Check RTC alarm
    if (rtc.isAlarmTriggered()) {
        logger->info("RTC", "⏰ ALARM TRIGGERED!");
        rtc.clearAlarmFlag();
        rtc.clearAlarm();
    }

    // Check RTC countdown timer
    if (rtc.isTimerTriggered()) {
        logger->info("RTC", "⏱ TIMER TRIGGERED!");
        rtc.clearTimerFlag();
    }

    // Check RTC minute tick
    if (rtc.isMinuteTriggered()) {
        logger->info("RTC", "🕐 MINUTE TICK");
        rtc.clearMinuteFlag();
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
        delay(50);
        touchController.sleep();

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
        touchController.wake();
        display.powerOn();
        sleeping = false;
        last_activity_time = millis();
    }
}

void SystemManager::logHeartbeat() {
    static int heartbeat = 0;
    heartbeat++;

    logger->header((String("SYSTEM HEARTBEAT #") + String(heartbeat)).c_str());
    logger->info("UPTIME", (String(millis() / 1000) + String(" seconds")).c_str());
    
    // Memory Status
    logger->info("MEMORY", (String("Internal RAM Free: ") + String(ESP.getFreeHeap() / 1024) + String(" KB")).c_str());
    logger->info("MEMORY", (String("PSRAM Free: ") + String(ESP.getFreePsram() / 1024) + String(" KB")).c_str());
    logger->info("MEMORY", (String("FLASH Size: ") + String(ESP.getFlashChipSize() / 1024) + String(" KB")).c_str());

    // SD Card Status
    if (sdCard.isInitialized()) {
        logger->info("SD", (String("SD: ") + String(sdCard.usedMB()) + String("MB used / ") + String(sdCard.totalMB()) + String("MB total")).c_str());
    }

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
        // INT1 fire count — expect ~560 per 5s interval at 112Hz if interrupt is working
        logger->info("IMU", (String("INT1 fires this interval: ") + String(imu.getIsrCount())).c_str());
        imu.resetIsrCount();
    }
    
    motor.buzz();
        
    logger->footer();
}
