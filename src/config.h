#pragma once

/*
 * Hardware Pin Definitions
 */

// AMOLED Display Pins (QSPI Interface)
#define LCD_SDIO0       4       // Data 0 (MOSI)
#define LCD_SDIO1       5       // Data 1
#define LCD_SDIO2       6       // Data 2  
#define LCD_SDIO3       7       // Data 3
#define LCD_SCLK        11      // Serial Clock
#define LCD_CS          12      // Chip Select
#define LCD_RESET       8       // Reset Pin
#define LCD_TE          13      // Tear enable (not used)

// Display Properties
#define LCD_WIDTH       410
#define LCD_HEIGHT      502
#define LCD_ORIENTATION 0       // 0=Portrait, 1=Landscape 90°, 2=Portrait 180°, 3=Landscape 270°
#define LCD_COL_OFFSET1 22
#define LCD_ROW_OFFSET1 0
#define LCD_COL_OFFSET2 0
#define LCD_ROW_OFFSET2 0

// I2C bus
#define I2C_SDA         15      // shared I2C bus
#define I2C_SCL         14      // shared I2C bus

// Touch Controller Pins (I2C Interface - FT3168)
#define TOUCH_SDA       I2C_SDA // shared I2C bus
#define TOUCH_SCL       I2C_SCL // shared I2C bus
#define TOUCH_INT       38      // Touch Interrupt
#define TOUCH_RST       9       // Touch Reset

// IMU Pins (I2C Interface - QMI8658)
#define IMU_SDA         I2C_SDA // shared I2C bus
#define IMU_SCL         I2C_SCL // shared I2C bus
#define IMU_INT1        21      // IMU Interrupt 1

// RTC Pins (I2C Interface - PCF85063)
#define RTC_SDA         I2C_SDA // shared I2C bus
#define RTC_SCL         I2C_SCL // shared I2C bus
#define RTC_INT         39      // RTC Interrupt

// Power Management Pins (I2C Interface - AXP2101)
#define PMU_SDA         I2C_SDA // shared I2C bus
#define PMU_SCL         I2C_SCL // shared I2C bus

// Buttons
#define BTN_PWR         10      // Power Button
#define BTN_BOOT        0       // Boot Button (GPIO0)

// SD Card Pins (SPI Interface)
#define SD_MOSI         1       // SD Card MOSI
#define SD_SCK          2       // SD Card Clock
#define SD_MISO         3       // SD Card MISO
#define SD_CS           17      // SD Card Chip Select

// Audio
#define SPEAKER_SDA     I2C_SDA // Shared I2C Bus (for I2S Control)
#define SPEAKER_SCL     I2C_SCL // Shared I2C Bus (for I2S Control)
#define SPEAKER_MCLK    16
#define SPEAKER_SCLK    41
#define MIC_DSIN        40
#define MIC_ASDOUT      42
#define MIC_LRCK        45
#define MIC_PA_CTRL     46

// Motor
#define MOTOR_PIN       18
