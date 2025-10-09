#pragma once

#include <Arduino.h>
#include <HWCDC.h>
#include <Arduino_GFX_Library.h>
#include "config.h"

class Display {
private:
    HWCDC* usbSerial = nullptr;
    Arduino_ESP32QSPI *qspi_bus = nullptr;
    Arduino_CO5300 *gfx = nullptr;
    bool initialized = false;

public:
    Display();
    ~Display();
    
    bool init(HWCDC &usbSerial);
    void clear(uint16_t color = 0x0000);
    void fillScreen(uint16_t color);
    
    // Text methods
    void setCursor(int16_t x, int16_t y);
    void setTextColor(uint16_t color);
    void setTextSize(float size);
    void print(const char* text);
    void println(const char* text);
    void printf(const char* format, ...);
    
    // Drawing methods
    void drawPixel(int16_t x, int16_t y, uint16_t color);
    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    void drawCircle(int16_t x, int16_t y, int16_t r, uint16_t color);
    void fillCircle(int16_t x, int16_t y, int16_t r, uint16_t color);
    
    // Display properties
    uint16_t getWidth();
    uint16_t getHeight();
    
    // Advanced features
    void setBrightness(uint8_t brightness);
    void startWrite();
    void endWrite();
    
    // Status
    bool isInitialized() { return initialized; }

    // Direct access to Arduino_CO5300 object if needed
    Arduino_CO5300* getDisplay() { return gfx; }
};