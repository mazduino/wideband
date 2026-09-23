Firmware for the rusEFI-Wideband v2.0 F0 module (STM32F042K6Tx), a hardware revision of the
original https://github.com/mck1117/wideband/tree/master/board_module F0 board.

Unlike the original f0_module, this board has three separate Nernst ESR injection drivers,
one per supported sensor type (same pattern as f1_rev2/f1_rev3):

| Sensor  | Pin | AC injection resistor |
|---------|-----|------------------------|
| LSU4.9  | PB7 | 22k                    |
| LSU4.2  | PB3 | 6.8k                   |
| LSU ADV | PB4 | 47k                    |

Sensor type is stored in `Configuration::sensorType` and can be set over CAN with
`WB_MSG_SET_SENS_TYPE` (see `for_rusefi/wideband_can.h` and `can.cpp`). Changing sensor type
triggers a reboot so the ESR driver pin modes (set up once at boot) are reconfigured correctly.
