#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include "config.h"
#include "logger/logger.hpp"

class SDCard {
private:
    Logger* logger = nullptr;
    bool initialized = false;

    static const char* cardTypeStr(uint8_t type) {
        switch (type) {
            case CARD_MMC:  return "MMC";
            case CARD_SD:   return "SDSC";
            case CARD_SDHC: return "SDHC";
            default:        return "Unknown";
        }
    }

public:
    SDCard(Logger* logger) : logger(logger) {}

    // Uses SD_MMC in 1-bit SDIO mode (D1/D2 are NC on this board).
    // Pins: CLK=SD_SCK, CMD=SD_MOSI, D0=SD_MISO, D3=SD_CS
    bool begin();

    bool isInitialized() const { return initialized; }

    uint64_t totalMB() const { return SD.totalBytes() / (1024ULL * 1024ULL); }
    uint64_t usedMB()  const { return SD.usedBytes()  / (1024ULL * 1024ULL); }
    uint64_t freeMB()  const { return totalMB() - usedMB(); }

    bool exists(const char* path)  { return SD.exists(path); }
    bool mkdir(const char* path)   { return SD.mkdir(path); }
    bool remove(const char* path)  { return SD.remove(path); }

    bool writeFile(const char* path, const String& data);
    bool appendFile(const char* path, const String& data);
    String readFile(const char* path);
    void listDir(const char* path, uint8_t levels = 1);
};
