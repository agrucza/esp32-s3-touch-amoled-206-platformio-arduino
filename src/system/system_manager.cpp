#include "system_manager.hpp"

bool SystemManager::init(HWCDC &USBSerial) {
    // Zeiger auf die übergebenen Objekte setzen
    this->usbSerial = &USBSerial;
    
    USBSerial.println("# SystemManager initialization starting...");
    
    // I2C Bus initialisieren
    Wire.begin(I2C_SDA, I2C_SCL, 100000); // TODO: try 400kHz, if supported
    this->i2c = &Wire;
    
    // PMU initialisieren
    if (!pmu.init(USBSerial, Wire)) {
        USBSerial.println("✗ SystemManager init failed: PMU initialization failed");
        return false;
    }

    if (!display.init(USBSerial)) {
        USBSerial.println("✗ SystemManager init failed: Display initialization failed");
        return false;
    }
    
    USBSerial.println("✓ SystemManager initialization complete");
    return true;
}
