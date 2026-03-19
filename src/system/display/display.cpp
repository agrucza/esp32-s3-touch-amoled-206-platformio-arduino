#include "display.hpp"
#include <cstdarg>

Display::Display(Logger* logger) : gfx(nullptr), initialized(false) {
    this->logger = logger;
}

bool Display::init() {
    logger->debug("DISPLAY", "Starting CO5300 AMOLED initialization...");

    // Initialize QSPI bus
    logger->debug("DISPLAY", "Creating QSPI bus...");
    qspi_bus = new Arduino_ESP32QSPI(LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);

    if (!qspi_bus) {
        logger->failure("DISPLAY", "Failed to create QSPI bus");
        return false;
    }

    logger->success("DISPLAY", "QSPI bus created successfully");

    // Initialize display driver
    logger->debug("DISPLAY", "Creating CO5300 driver instance...");

    gfx = new Arduino_CO5300(
        qspi_bus, LCD_RESET, LCD_ORIENTATION,
        LCD_WIDTH, LCD_HEIGHT,
        LCD_COL_OFFSET1, LCD_ROW_OFFSET1,
        LCD_COL_OFFSET2, LCD_ROW_OFFSET2
    );

    if (!gfx) {
        logger->failure("DISPLAY", "Failed to create CO5300 display driver");
        delete qspi_bus;
        qspi_bus = nullptr;
        return false;
    }
    logger->success("DISPLAY", "CO5300 driver instance created");

    logger->debug("DISPLAY", "Starting display hardware...");
    gfx->begin();

    logger->debug("DISPLAY", "Setting rotation...");
    gfx->setRotation(0);

    logger->debug("DISPLAY", "Filling screen with red for initial test...");
    gfx->fillScreen(RGB565_RED);

    logger->success("DISPLAY", "Display ready");
    initialized = true;
    return true;
}

Display::~Display() {
    powerOff();
}

void Display::powerOn() {
    logger->debug("DISPLAY", "Powering on display...");
    gfx->displayOn();
}

void Display::powerOff() {
    logger->debug("DISPLAY", "Powering off display and freeing resources...");
    gfx->displayOff();
}

void Display::clear(uint16_t color) {
    if (initialized && gfx) {
        gfx->fillScreen(color);
    }
}

void Display::fillScreen(uint16_t color) {
    if (initialized && gfx) {
        gfx->fillScreen(color);
    }
}

void Display::setCursor(int16_t x, int16_t y) {
    if (initialized && gfx) {
        gfx->setCursor(x, y);
    }
}

void Display::setTextColor(uint16_t color) {
    if (initialized && gfx) {
        gfx->setTextColor(color);
    }
}

void Display::setTextSize(float size) {
    if (initialized && gfx) {
        gfx->setTextSize(size);
    }
}

void Display::print(const char* text) {
    if (initialized && gfx) {
        gfx->print(text);
    }
}

void Display::println(const char* text) {
    if (initialized && gfx) {
        gfx->println(text);
    }
}

void Display::printf(const char* format, ...) {
    if (initialized && gfx) {
        va_list args;
        va_start(args, format);
        
        // Create formatted string buffer
        char buffer[256];
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        
        // Print the formatted string
        gfx->print(buffer);
    }
}

void Display::drawPixel(int16_t x, int16_t y, uint16_t color) {
    if (initialized && gfx) {
        gfx->drawPixel(x, y, color);
    }
}

void Display::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    if (initialized && gfx) {
        gfx->drawLine(x0, y0, x1, y1, color);
    }
}

void Display::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (initialized && gfx) {
        gfx->drawRect(x, y, w, h, color);
    }
}

void Display::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (initialized && gfx) {
        gfx->fillRect(x, y, w, h, color);
    }
}

void Display::drawCircle(int16_t x, int16_t y, int16_t r, uint16_t color) {
    if (initialized && gfx) {
        gfx->drawCircle(x, y, r, color);
    }
}

void Display::fillCircle(int16_t x, int16_t y, int16_t r, uint16_t color) {
    if (initialized && gfx) {
        gfx->fillCircle(x, y, r, color);
    }
}

uint16_t Display::getWidth() {
    return initialized && gfx ? gfx->width() : 0;
}

uint16_t Display::getHeight() {
    return initialized && gfx ? gfx->height() : 0;
}

void Display::setBrightness(uint8_t brightness) {
    if (initialized && gfx) {
        gfx->setBrightness(brightness);
    }
}

void Display::startWrite() {
    if (initialized && gfx) {
        gfx->startWrite();
    }
}

void Display::endWrite() {
    if (initialized && gfx) {
        gfx->endWrite();
    }
}

void Display::clearScreen(uint16_t color) {
    if (initialized && gfx) {
        gfx->fillScreen(color);
    }
}

void Display::drawText(int16_t x, int16_t y, const char* text, uint16_t color, uint8_t size) {
    if (initialized && gfx) {
        gfx->setCursor(x, y);
        gfx->setTextColor(color);
        gfx->setTextSize(size);
        gfx->print(text);
    }
}