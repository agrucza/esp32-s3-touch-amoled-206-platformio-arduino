#include "system_manager.hpp"

SystemManager::SystemManager(Logger* logger)
    : logger(logger), pmu(logger), display(logger), touchController(logger), fsManager(logger) // Explicitly initialize pmu
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
    
    // Simple button check
    if (buttonPressed(BTN_BOOT)) {
        this->sleep();
    }

    touchController.handleInterrupt();
    
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
    esp_sleep_enable_timer_wakeup(5000000); // Wakeup after 5 seconds (microseconds)
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
        logger->footer();
    }
}
