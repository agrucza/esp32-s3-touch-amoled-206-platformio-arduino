#include "mic.hpp"

esp_err_t Mic::writeReg(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(ES7210_I2C_ADDR);
    Wire.write(reg);
    Wire.write(val);
    return Wire.endTransmission() == 0 ? ESP_OK : ESP_FAIL;
}

bool Mic::begin() {
    logger->info("MIC", "Initializing ES7210...");

    // Probe I2C addresses 0x40–0x43
    uint8_t found = 0xFF;
    for (uint8_t addr = 0x40; addr <= 0x43; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            found = addr;
            char buf[32];
            snprintf(buf, sizeof(buf), "Found ES7210 at 0x%02X", addr);
            logger->info("MIC", buf);
            break;
        }
    }
    if (found == 0xFF) {
        logger->failure("MIC", "ES7210 not found on I2C bus (0x40-0x43)");
        return false;
    }

    // Software reset
    writeReg(0x00, 0xFF);
    delay(10);
    writeReg(0x00, 0x32);

    // Timing control
    writeReg(0x09, 0x30);
    writeReg(0x0A, 0x30);

    // HPF enable for ADC1/2 and ADC3/4
    writeReg(0x23, 0x2A);
    writeReg(0x22, 0x0A);
    writeReg(0x21, 0x2A);
    writeReg(0x20, 0x0A);

    // Serial port: standard I2S, 16-bit
    writeReg(0x11, 0x60);
    // TDM disabled (stereo I2S)
    writeReg(0x12, 0x00);

    // Analog: power on, VMID
    writeReg(0x40, 0xC3);

    // MIC1/2 bias = 2.87V; MIC3/4 bias off (MIC3/4 = AEC speaker reference, not needed)
    writeReg(0x41, 0x70);
    writeReg(0x42, 0x00);  // MIC3/4 bias disabled

    // MIC1/2 gain = 30dB; MIC3/4 gain = 0 (powered down)
    writeReg(0x43, 0x1A);
    writeReg(0x44, 0x1A);
    writeReg(0x45, 0x00);
    writeReg(0x46, 0x00);

    // Power on MIC1/2 only; power down MIC3/4 (AEC path — causes loopback interference)
    writeReg(0x47, 0x08);
    writeReg(0x48, 0x08);
    writeReg(0x49, 0xFF);  // MIC3 power down
    writeReg(0x4A, 0xFF);  // MIC4 power down

    // OSR = 32
    writeReg(0x07, 0x20);

    // ADC clock: dll on, doubler off, adc_div=8
    // MCLK=4096000 (256×16000), ADCCLK=4096000/8=512000, FS=512000/32=16000
    // reg02 = (adc_div-1) | (dll_enable<<7) = 0x07 | 0x80 = 0x87
    writeReg(0x02, 0x87);

    // LRCK divider: MCLK/LRCK - 1 = 256-1 = 255 = 0x00FF
    writeReg(0x04, 0x00);
    writeReg(0x05, 0xFF);

    // DLL power
    writeReg(0x06, 0x04);

    // Enable MIC1/2 bias + ADC + PGA; power down MIC3/4 entirely
    writeReg(0x4B, 0x0F);
    writeReg(0x4C, 0xFF);  // MIC3/4 all power down

    // ADC digital volume: 0xBF = 0dB
    writeReg(0x1B, 0xBF);
    writeReg(0x1C, 0xBF);
    writeReg(0x1D, 0xBF);
    writeReg(0x1E, 0xBF);

    // Enable device
    writeReg(0x00, 0x71);
    delay(10);
    writeReg(0x00, 0x41);

    initialized = true;
    logger->success("MIC", "ES7210 ready");
    return true;
}

size_t Mic::read(int16_t* buf, size_t samples, uint32_t timeoutMs) {
    if (!initialized) return 0;
    size_t bytesRead = 0;
    i2s_read(MIC_I2S_PORT, buf, samples * sizeof(int16_t), &bytesRead, pdMS_TO_TICKS(timeoutMs));
    return bytesRead / sizeof(int16_t);
}
