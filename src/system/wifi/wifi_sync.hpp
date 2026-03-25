#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include "logger/logger.hpp"
#include "../rtc/rtc.hpp"

#define WIFI_CREDENTIALS_FILE   "/wifi.txt"
#define NTP_SERVER              "pool.ntp.org"
#define NTP_TIMEOUT_MS          10000
#define WIFI_CONNECT_TIMEOUT_MS 15000

class WiFiSync {
private:
    Logger* logger = nullptr;

    bool loadCredentials(String& ssid, String& password);
    bool waitForConnect(uint32_t timeoutMs);
    bool waitForNTP(struct tm& timeinfo, uint32_t timeoutMs);

public:
    WiFiSync(Logger* logger) : logger(logger) {}

    // Connect to WiFi, sync NTP → RTC, then disconnect. Returns true if RTC was updated.
    bool syncTime(RTC& rtc);
};
