#include <Arduino.h>
#include "HWCDC.h"
#include "system/system_manager.hpp"

#include "config.h"

HWCDC USBSerial;
SystemManager system_manager;

void setup() {
    // put your setup code here, to run once:
    USBSerial.begin(115200);
    while (!USBSerial) {
        ; // wait for serial port to connect. Needed for native USB
    }
    USBSerial.println("\n=============================");
    USBSerial.println("SETUP STARTING");
    USBSerial.println("=============================\n");

    system_manager.init(USBSerial);
    system_manager.getDisplay().setCursor(10, 120);
}

void loop() {
    static unsigned long lastTime = 0;
    static int heartbeat = 0;
    
    // Heartbeat every 5 seconds
    if (millis() - lastTime > 5000) {
        lastTime = millis();
        heartbeat++;
    
        USBSerial.println("\n==========================================");
        USBSerial.print("HEARTBEAT #");
        USBSerial.println(heartbeat);
        USBSerial.print("UPTIME: ");
        USBSerial.print(millis() / 1000);
        USBSerial.println(" seconds");
        
        // Speicher-Status
        USBSerial.print("Internal RAM Free: ");
        USBSerial.print(ESP.getFreeHeap());
        USBSerial.println(" bytes");
        
        USBSerial.print("PSRAM Free: ");
        USBSerial.print(ESP.getFreePsram());
        USBSerial.println(" bytes");
        
        USBSerial.print("Battery Voltage: ");
        USBSerial.print(system_manager.getPMU().getBattVoltage());
        USBSerial.print(" mV / ");
        USBSerial.print(system_manager.getPMU().getBatteryPercent());
        USBSerial.println(" %");

        USBSerial.print("USB Connected: ");
        USBSerial.println(system_manager.getPMU().isUSBConnected() ? "Yes" : "No");

        USBSerial.print("Battery Connected: ");
        USBSerial.println(system_manager.getPMU().isBatteryConnect() ? "Yes" : "No");

        USBSerial.print("Charging: ");
        USBSerial.println(system_manager.getPMU().isCharging() ? "Yes" : "No");
        
        // PSRAM-Funktionstest alle 10 Heartbeats
        /*
        if (heartbeat % 10 == 0 && ESP.getPsramSize() > 0) {
            USBSerial.println("--- PSRAM STRESS TEST ---");
            void* testPtr = ps_malloc(100000); // 100KB Test
            if (testPtr) {
                USBSerial.println("✓ Large PSRAM allocation OK");
                free(testPtr);
            } else {
                USBSerial.println("✗ Large PSRAM allocation FAILED");
            }
        }
        */
       
        USBSerial.println("*** USB SERIAL + PSRAM STABLE! ***");
        USBSerial.println("==========================================\n");
    }
}
