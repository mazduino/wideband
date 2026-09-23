#pragma once

// TS settings
#define TS_SIGNATURE "rusEFI 2024.02.11.wideband_dual"

// This board implements two channels
#define AFR_CHANNELS 2
#define EGT_CHANNELS 2

// Fundamental board constants
#define VCC_VOLTS (3.3f)
#define HALF_VCC (VCC_VOLTS / 2)
#define ADC_MAX_COUNT (4095)
#define ADC_OVERSAMPLE 16

// Algo settings
// TODO: move to settings
#define HEATER_FAST_HEATING_THRESHOLD_T		550

// *******************************
//    Nernst voltage & ESR sense
// *******************************
#define NERNST_INPUT_GAIN (3.15f)
#define NERNST_INPUT_OFFSET (0.247)

// *******************************
//        Battery Sensing
// *******************************
// 100K + 10K divider
#define BATTERY_INPUT_DIVIDER (10.0 / (10.0 + 100.0))

// *******************************
//     Heater low side Sensing
// *******************************
// 100K + 10K divider
#define HEATER_INPUT_DIVIDER (10.0 / (10.0 + 100.0))
#define HEATER_FILTER_ALPHA (0.1f)

// *******************************
//        Vm output Sensing
// *******************************
// 100K + 10K divider
#define VM_INPUT_DIVIDER (10.0 / (10.0 + 100.0))

// *******************************
//    Nernst voltage & ESR sense
// *******************************
#define VM_RESISTOR_VALUE (0)

// *******************************
//          AUX outputs
// *******************************
// OpAmp with 82K + 160K
#define AUXOUT_GAIN         ((82.0 + 160.0) / 160.0)

// *******************************
// With heater constantly on we
// are not able to measure Vbatt
// through Heater-
// *******************************
// TODO: remove!
#define HEATER_MAX_DUTY		(0.85)

// *******************************
//   TunerStudio Primary Port - Routed to BlueTooth
// *******************************
#define TS_PRIMARY_SERIAL_PORT	SD3
#define TS_PRIMARY_BAUDRATE		115200

#if 1
// *******************************
//   TunerStudio Secondary Port - J3 connector (2x2)
// *******************************
#define TS_SECONDARY_SERIAL_PORT	SD1
#define TS_SECONDARY_BAUDRATE		115200
#else
// *******************************
//   Debug serial device
// *******************************
#define DEBUG_SERIAL_PORT			SD1
#define DEBUG_SERIAL_BAUDRATE		115200
#endif

// *******************************
//   MAX31855 spi port
// *******************************
#define EGT_SPI_DRIVER			(&SPID3)

// Serial connection over BT module
// enable BT module setup attempt
#define BT_SERIAL_OVER_JDY33	TRUE
#define BT_BROADCAST_NAME		"RusEFI WBO x2"

// *******************************
//   Indication: per channel status LED + communication LED
// *******************************
#define ADVANCED_INDICATION

// *******************************
//   I2C OLED on I2C1 (PB8 = SCL, PB9 = SDA, remapped in boardInit)
// *******************************
// Needs external 4k7 pull-ups on both lines.
#define OLED_I2C_DRIVER         (&I2CD1)
#define OLED_I2C_BITRATE        (400000)
// 0x3C for most SSD1306 modules, 0x3D if the module ties SA0 high
#define OLED_I2C_ADDRESS        (0x3C)
// Controller type: SSD1306 (default) or SH1106 (132x64 RAM, 2px column offset)
//#define OLED_CONTROLLER_SH1106
// Panel geometry - only 128x64 and 128x32 are supported
#define OLED_WIDTH              (128)
#define OLED_HEIGHT             (64)
// Flip the image 180 degrees if the module is mounted upside down
//#define OLED_ROTATE_180
// Screen refresh period
#define OLED_REFRESH_PERIOD_MS  (200)
// Stoichiometric AFR used to convert lambda for the display
#define OLED_AFR_STOICH         (14.7f)
