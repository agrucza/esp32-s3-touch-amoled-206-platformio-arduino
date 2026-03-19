#include "pmu.hpp"

bool PMU::setBus(TwoWire &wire) {
    logger->debug("PMU", "Starting AXP2101 initialization...");
    if (!pmu.begin(wire, pmuAddress, PMU_SDA, PMU_SCL)) {
        logger->failure("PMU", "AXP2101 not found");
        initialized = false;
        return false;
    }

    // Configure and enable LDO power rails from schematic:
    // ALDO1 → VL1_3.3V: touch controller (FT3168), IMU, RTC
    pmu.setALDO1Voltage(3300);
    pmu.enableALDO1();

    // ALDO2 → VL2_3.3V: general 3.3V peripherals
    pmu.setALDO2Voltage(3300);
    pmu.enableALDO2();

    // ALDO3 → VCC3V: general 3.3V LDO
    pmu.setALDO3Voltage(3300);
    pmu.enableALDO3();

    // ALDO4 → VL3_1.8V: CO5300 AMOLED VDDIO
    pmu.setALDO4Voltage(1800);
    pmu.enableALDO4();

    // BLDO1 → VL_1.2V: CO5300 AMOLED VCORE
    pmu.setBLDO1Voltage(1200);
    pmu.enableBLDO1();

    // BLDO2 → VL_2.8V: CO5300 AMOLED AVDD
    pmu.setBLDO2Voltage(2800);
    pmu.enableBLDO2();

    // Allow all rails to stabilize before peripherals are initialized
    delay(20);

    logger->success("PMU", "AXP2101 initialized, all LDO rails enabled");
    initialized = true;
    return true;
}
