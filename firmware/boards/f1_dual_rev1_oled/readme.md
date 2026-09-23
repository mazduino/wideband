Firmware for https://github.com/rusefi/rusefi-hardware/tree/main/lambda-x2/export/rev1
with an I2C OLED (SSD1306 / SH1106, 128x64) attached to I2C1 on PB8 (SCL) / PB9 (SDA).

Same hardware as f1_dual_rev1 - the only difference is that PB8/PB9 are driven as
I2C1 alternate-function open-drain instead of being left unused, and the display
thread is compiled in (ENABLE_OLED in board.mk).

The bus needs external pull-ups (4k7 to 3.3V) on both lines; the STM32 internal
pull-ups are not usable in alternate-function mode.
