#include "sd_card.hpp"

// HSPI (SPI3) avoids conflict with display QSPI on SPI2/FSPI.
// SPI mode also bypasses UHS-I voltage-switch negotiation that hangs SDXC cards in SD_MMC mode.
static SPIClass sd_spi(HSPI);

bool SDCard::begin() {
    logger->info("SD", "Initializing SD card (SPI mode)...");

    // CS idle-HIGH before SPI init — required by SD card spec
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);

    sd_spi.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    delay(100);  // Power-up settling time

    // Send 80 dummy clock cycles with CS HIGH and MOSI HIGH.
    // SD spec requires this before CMD0 to transition card into SPI mode.
    sd_spi.beginTransaction(SPISettings(400000, MSBFIRST, SPI_MODE0));
    digitalWrite(SD_CS, HIGH);
    for (int i = 0; i < 10; i++) {  // 10 × 8 = 80 clocks
        sd_spi.transfer(0xFF);
    }
    sd_spi.endTransaction();
    delay(10);

    if (!SD.begin(SD_CS, sd_spi, 4000000, "/sd", 5U,true)) {
        logger->warn("SD", "No SD card detected or mount failed");
        return false;
    }

    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        logger->warn("SD", "No SD card attached");
        return false;
    }

    logger->success("SD", (String("SD card mounted: ") + cardTypeStr(cardType) +
                           String(", ") + String(totalMB()) + String("MB total, ") +
                           String(freeMB()) + String("MB free")).c_str());

    initialized = true;
    return true;
}

bool SDCard::writeFile(const char* path, const String& data) {
    File file = SD.open(path, FILE_WRITE);
    if (!file) {
        logger->failure("SD", (String("Failed to open for write: ") + path).c_str());
        return false;
    }
    file.print(data);
    file.close();
    return true;
}

bool SDCard::appendFile(const char* path, const String& data) {
    File file = SD.open(path, FILE_APPEND);
    if (!file) {
        logger->failure("SD", (String("Failed to open for append: ") + path).c_str());
        return false;
    }
    file.print(data);
    file.close();
    return true;
}

String SDCard::readFile(const char* path) {
    File file = SD.open(path);
    if (!file || file.isDirectory()) {
        logger->failure("SD", (String("Failed to open for read: ") + path).c_str());
        return "";
    }
    String out;
    while (file.available()) {
        out += (char)file.read();
    }
    file.close();
    return out;
}

void SDCard::listDir(const char* path, uint8_t levels) {
    File root = SD.open(path);
    if (!root || !root.isDirectory()) {
        logger->warn("SD", (String("Not a directory: ") + path).c_str());
        return;
    }
    File entry = root.openNextFile();
    while (entry) {
        String line = String(path) + String(entry.name());
        if (entry.isDirectory()) {
            line += "/";
            logger->info("SD", line.c_str());
            if (levels > 0) listDir(line.c_str(), levels - 1);
        } else {
            line += String("  (") + String(entry.size()) + String(" bytes)");
            logger->info("SD", line.c_str());
        }
        entry = root.openNextFile();
    }
}
