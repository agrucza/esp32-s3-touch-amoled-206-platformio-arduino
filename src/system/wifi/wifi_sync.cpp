#include "wifi_sync.hpp"

bool WiFiSync::loadCredentials(String& ssid, String& password) {
    File f = LittleFS.open(WIFI_CREDENTIALS_FILE, "r");
    if (!f) {
        logger->warn("WIFI", "No " WIFI_CREDENTIALS_FILE " found on LittleFS");
        return false;
    }
    ssid     = f.readStringUntil('\n'); ssid.trim();
    password = f.readStringUntil('\n'); password.trim();
    f.close();

    if (ssid.isEmpty()) {
        logger->warn("WIFI", "SSID empty in " WIFI_CREDENTIALS_FILE);
        return false;
    }
    return true;
}

bool WiFiSync::waitForConnect(uint32_t timeoutMs) {
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > timeoutMs) return false;
        delay(200);
    }
    return true;
}

bool WiFiSync::waitForNTP(struct tm& timeinfo, uint32_t timeoutMs) {
    unsigned long start = millis();
    while (!getLocalTime(&timeinfo)) {
        if (millis() - start > timeoutMs) return false;
        delay(200);
    }
    return true;
}

bool WiFiSync::syncTime(RTC& rtc) {
    String ssid, password;
    if (!loadCredentials(ssid, password)) return false;

    logger->info("WIFI", ("Connecting to: " + ssid).c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());

    if (!waitForConnect(WIFI_CONNECT_TIMEOUT_MS)) {
        logger->failure("WIFI", "Connection timed out");
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        return false;
    }
    logger->success("WIFI", ("Connected, IP: " + WiFi.localIP().toString()).c_str());

    // Sync NTP (UTC: no offset, no DST)
    configTime(0, 0, NTP_SERVER);

    struct tm timeinfo;
    if (!waitForNTP(timeinfo, NTP_TIMEOUT_MS)) {
        logger->failure("WIFI", "NTP sync timed out");
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        return false;
    }

    // Write to RTC
    RTC::DateTime dt;
    dt.year    = timeinfo.tm_year + 1900;
    dt.month   = timeinfo.tm_mon + 1;
    dt.day     = timeinfo.tm_mday;
    dt.hour    = timeinfo.tm_hour;
    dt.minute  = timeinfo.tm_min;
    dt.second  = timeinfo.tm_sec;
    dt.weekday = timeinfo.tm_wday;

    bool ok = rtc.setDateTime(dt);
    if (ok) {
        char buf[48];
        snprintf(buf, sizeof(buf), "RTC set to %04d-%02d-%02d %02d:%02d:%02d UTC",
                 dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
        logger->success("WIFI", buf);
    } else {
        logger->failure("WIFI", "Failed to write time to RTC");
    }

    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    logger->info("WIFI", "WiFi disconnected");
    return ok;
}
