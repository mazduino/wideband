# GPIO mapping: `f0_module_v2` vs `f1_rev2` vs `f1_dual_rev1`

Reference for respinning the F0 wideband module onto an STM32F103.

`f1_rev2` is the single-channel F103 board; `f1_dual_rev1` is the dual-channel
one and is the target for the new board (STM32F103RCT6). Section 6 covers it in
full - sections 2 to 5 compare the F0 module against the single-channel `f1_rev2`
so the one-to-one signal migration stays readable.

Every entry below was read out of the board files, not from the schematic:

- pin direction / alternate function -> `boards/<board>/board.h`
- logical pin names -> `boards/<board>/io/io_pins.h`
- ADC chain and what each sample feeds -> `boards/<board>/port.cpp`
- divider constants and channel count -> `boards/<board>/wideband_board_config.h`

Where the board files contradict each other, that is called out in
[Discrepancies](#discrepancies) rather than silently resolved.

## 1. MCU

|                     | `f0_module_v2`        | `f1_rev2`                  | `f1_dual_rev1`             |
| ------------------- | --------------------- | -------------------------- | -------------------------- |
| Part                | STM32F042K6T6         | STM32F103CBT6              | **STM32F103RCT6**          |
| Package             | LQFP-32 7x7 P0.8      | LQFP-48                    | LQFP-64                    |
| Density             | -                     | medium (no DAC)            | high (**has DAC**)         |
| Core / clock        | Cortex-M0 @ 48 MHz    | Cortex-M3 @ 64 MHz         | Cortex-M3 @ 64 MHz         |
| Flash / RAM         | 32 K / 6 K            | 128 K / 20 K               | 256 K / 48 K               |
| App flash budget    | 25 600 B (6 K boot + 1 K config) | 122 880 B (8 K config), 112 640 B with OpenBLT | 245 760 B (8 K OpenBLT + 8 K config) |
| Measured app size   | 21 272 B (83 %)       | 39 252 B (32 %)            | 43 912 B (18 %) / 48 724 B with OLED |
| Measured RAM free   | 8 B                   | 11 088 B                   | 36 712 B / 34 768 B with OLED |
| AFR channels        | 1                     | 1                          | **2**                      |
| EGT                 | no                    | no                         | 2 (MAX3185x on SPI1)       |
| Supply voltage sense| **none**              | PB0                        | per channel: PB0 / PC5, plus 12 V on PC0 |
| Pump (Ip) drive     | PWM + RC              | PWM + RC                   | **real DAC** (PA4 / PA5)   |
| TunerStudio / UART  | **not configured**    | yes, one port              | yes, two ports (one to BT) |
| Bootloader          | custom, 6 K           | OpenBLT, 8 K               | OpenBLT, 8 K               |

## 2. Function to pin

| Function                    | `f0_module_v2`          | `f1_rev2`               | Note |
| --------------------------- | ----------------------- | ----------------------- | ---- |
| **Analog in**               |                         |                         |      |
| Nernst voltage (Un)         | PA0 / ADC_IN0           | PA7 / ADC12_IN7         | f1 calls it `Un_3x_sense` |
| Nernst, second tap          | -                       | PA5 / ADC12_IN5         | declared, **not sampled** on f1 |
| Pump current (Ip)           | PA2 / ADC_IN2           | PA6 / ADC12_IN6         | |
| Virtual ground (Vm)         | PA3 / ADC_IN3           | PA0 / ADC12_IN0         | f1 samples it but uses the `HALF_VCC` constant instead |
| Vm, declared but unused     | PA1                     | -                       | see Discrepancies |
| Battery / supply sense      | -                       | PB0 / ADC12_IN8         | needs `BATTERY_INPUT_DIVIDER` |
| Heater low-side sense       | -                       | PB1 / ADC12_IN9         | sampled, **discarded** - bad RC filter |
| **Analog / PWM out**        |                         |                         |      |
| Pump DAC (Ip drive)         | PA6, TIM3_CH1 (AF1)     | PA1, TIM2_CH2           | `PUMP_DAC_PWM_DEVICE` |
| Heater PWM                  | PA7, TIM1_CH1N (AF2)    | PB6, TIM4_CH1           | `HEATER_PWM_DEVICE` |
| `AUXOUT_DAC_PWM_CHANNEL_0`  | -                       | PB14, TIM1_CH2N         | complementary, active low |
| `AUXOUT_DAC_PWM_CHANNEL_1`  | -                       | PB15, TIM1_CH3N         | complementary, active low; board.h labels these PWMout2 / PWMout1 - the numbering is inverted relative to the firmware channel index |
| **Nernst ESR drive**        |                         |                         |      |
| LSU 4.9 ESR                 | PB7                     | PB11                    | high-Z until sensor type is known |
| LSU 4.2 ESR                 | PB3                     | PB12                    | |
| LSU ADV ESR                 | PB4                     | PB10                    | |
| LSU 4.9 bias                | -                       | PB2                     | |
| **Digital**                 |                         |                         |      |
| Blue LED                    | PB5                     | PB13                    | |
| Green LED                   | PB6                     | PA8                     | |
| CAN RX                      | PA11 (AF4)              | PA11                    | |
| CAN TX                      | PA12 (AF4)              | PA12                    | |
| UART TX                     | PA9 (AF1)               | PA9                     | f0: ROM bootloader only |
| UART RX                     | PA10 (AF0, see below)   | PA10                    | f0: ROM bootloader only |
| Config strap 0              | PB1 (`ID_SEL1`)         | PC13 (`config0`)        | tri-state: low / float / high |
| Config strap 1              | PA8 (`ID_SEL2`)         | PC14 (`config1`)        | |
| SWDIO / SWCLK               | PA13 / PA14             | PA13 / PA14             | |
| SWO                         | -                       | PB3                     | |

## 3. ADC chain

The buffer index is what `port.cpp` indexes with `AverageSamples(adcBuffer, N)`,
so this is the table to check when moving a signal to a different pin.

### `f0_module_v2` - 3 channels, `ADC_CHSELR_CHSEL0 | CHSEL2 | CHSEL3`

STM32F0 always scans selected channels in ascending channel order.

| idx | Channel | Pin | Feeds                       |
| --- | ------- | --- | --------------------------- |
| 0   | IN0     | PA0 | `NernstVoltage` (/ `NERNST_INPUT_GAIN`) |
| 1   | IN2     | PA2 | `PumpCurrentVoltage`        |
| 2   | IN3     | PA3 | `VirtualGroundVoltageInt`   |
| -   | -       | -   | `HeaterSupplyVoltage = 0` (hardcoded) |

`ADC_OVERSAMPLE` = 24, `NERNST_INPUT_GAIN` = 2.7, `VM_RESISTOR_VALUE` = 10.

### `f1_rev2` - 5 channels, explicit `SQR3` sequence

The sequence is author-defined, so the index does **not** follow channel order.

| idx | SQ  | Channel | Pin | Feeds                                     |
| --- | --- | ------- | --- | ----------------------------------------- |
| 0   | SQ1 | IN0     | PA0 | (`VirtualGroundVoltageExt`, commented out) |
| 1   | SQ2 | IN6     | PA6 | `PumpCurrentVoltage`                      |
| 2   | SQ3 | IN7     | PA7 | `NernstVoltage` (/ `NERNST_INPUT_GAIN`)   |
| 3   | SQ4 | IN8     | PB0 | `HeaterSupplyVoltage` (/ `BATTERY_INPUT_DIVIDER`) |
| 4   | SQ5 | IN9     | PB1 | unused - see Discrepancies                |

`ADC_OVERSAMPLE` = 16, `NERNST_INPUT_GAIN` = 3.15, `VM_RESISTOR_VALUE` = 0,
`BATTERY_INPUT_DIVIDER` = 10 / 110, `VirtualGroundVoltageInt` = `HALF_VCC`.

## 4. Timers

| Timer | `f0_module_v2`         | `f1_rev2`                |
| ----- | ---------------------- | ------------------------ |
| TIM1  | heater PWM (CH1N, PA7) | AUX out (CH2N/CH3N, PB14/PB15) |
| TIM2  | -                      | pump DAC (CH2, PA1)      |
| TIM3  | pump DAC (CH1, PA6)    | -                        |
| TIM4  | -                      | heater PWM (CH1, PB6)    |

`STM32_PWM_USE_ADVANCED` is `TRUE` on both boards and must stay that way:
`heater_thread.cpp` and `pump_dac.cpp` unconditionally request
`PWM_OUTPUT_ACTIVE_HIGH | PWM_COMPLEMENTARY_OUTPUT_ACTIVE_LOW` on every channel,
which is what lets the f0 heater drive TIM1_CH1N on PA7.

## 5. Free pins

### `f0_module_v2`

`PA4`, `PA5`, `PB0`, `PB2`, plus `PA15` (configured alternate AF0 as "SPI NSS",
but no SPI driver is enabled).

**There is no usable I2C.** On the STM32F042, I2C1 can only reach PB6/PB7,
PA9/PA10, or PF0/PF1 - taken by the green LED, the LSU 4.9 ESR driver, the UART,
and the oscillator respectively. This is why an I2C display cannot be added to
this board without moving other functions.

### `f1_rev2`

The rev2 board.h already reserves a display header: `DISP0`..`DISP6`.

| Label   | Pin  | Free? | Caveat |
| ------- | ---- | ----- | ------ |
| DISP0   | PA15 | yes   | JTDI - needs `AFIO_MAPR_SWJ_CFG_1` |
| DISP1   | PB4  | yes   | JNTRST - needs `AFIO_MAPR_SWJ_CFG_1` |
| DISP2   | PB5  | yes   | |
| DISP4   | PB7  | yes   | I2C1 SDA (default mapping) |
| DISP5   | PB8  | yes   | **I2C1 SCL (remapped)** |
| DISP6   | PB9  | yes   | **I2C1 SDA (remapped)** |
| -       | PA2, PA3, PA4 | yes | test points TP5 / TP3 / TP4, analog in |
| -       | PC15 | yes   | |

So `PB8` + `PB9` give a free I2C1 bus, exactly the pair used by
`f1_dual_rev1_oled`. Two things are needed, neither of which rev2 has today:

1. `boardInit()` in `boards/f1_rev2/board.c` is **empty** - it must add
   `AFIO->MAPR |= AFIO_MAPR_I2C1_REMAP`.
2. `VAL_GPIOBCRH` is `0xAA244488` (PB8/PB9 = input). For I2C both nibbles must
   become `F` (alternate function open drain, 50 MHz) -> `0xAA2444FF`.

External 4k7 pull-ups to 3.3 V are required; the internal ones do not work in
alternate-function mode.

## 6. Dual-channel target: `f1_dual_rev1` (STM32F103RCT6)

This is the board the new design is based on. Unlike `f0_module_v2` and
`f1_rev2`, its `board.h` and `port.cpp` agree with each other everywhere that
matters - the ADC sequence, the buffer indices and the pin labels all line up.

Channel naming: **L** = channel 0, **R** = channel 1.

### 6.1 Pin map

| Pin | Function | Notes |
| --- | -------- | ----- |
| **Port A** | | |
| PA0  | `R_Ip_sense` | analog, ADC12_IN0 |
| PA1  | `R_Un_3x_sense` | analog, ADC12_IN1 - gained Nernst, R |
| PA2  | `R_Un_sense` | analog, ADC12_IN2 - ungained fallback, R |
| PA3  | `L_Un_sense` | analog, ADC12_IN3 - ungained fallback, L (board.h mislabels this, see 6.5) |
| PA4  | `R_Ip_dac` | **DAC1_OUT1** - pump drive, R |
| PA5  | `L_Ip_dac` | **DAC1_OUT2** - pump drive, L |
| PA6  | `R_AUX_ADC` | analog, ADC12_IN6 |
| PA7  | `L_AUX_ADC` | analog, ADC12_IN7 |
| PA8  | `LED_BLUE` | output |
| PA9  | `UART_TX` | USART1, TS secondary port (SD1) |
| PA10 | `UART_RX` | USART1 |
| PA11 | `CAN_RX` | |
| PA12 | `CAN_TX` | |
| PA13 | `SWDIO` | |
| PA14 | `SWCLK` | |
| PA15 | `BT_EN` | output - needs `SWJ_CFG` (JTDI), handled in `boardInit()` |
| **Port B** | | |
| PB0  | `R_Heater_sense` | analog, ADC12_IN8 |
| PB1  | `R_OUT_sense` | analog |
| PB2  | `Nernst_4.9_bias` | output |
| PB3  | `SPI1_SCK` | remapped SPI1, EGT |
| PB4  | `SPI1_MISO` | remapped SPI1, EGT |
| PB5  | `SPI1_MOSI` | remapped SPI1, EGT |
| PB6  | `R_heater_pwm` | TIM4_CH1, `HEATER_PWM_CHANNEL_1` |
| PB7  | `L_heater_pwm` | TIM4_CH2, `HEATER_PWM_CHANNEL_0` |
| PB8  | **`I2C1_SCL`** | remapped I2C1 - the OLED bus |
| PB9  | **`I2C1_SDA`** | remapped I2C1 - the OLED bus |
| PB10 | `Nernst_ADV_esr_drive` | high-Z until sensor type is known |
| PB11 | `Nernst_4.9_esr_drive` | |
| PB12 | `Nernst_4.2_esr_drive` | high-Z until sensor type is known |
| PB13 | `L_LED_GREEN` | output |
| PB14 | `L_OUT_en` | output |
| PB15 | `R_OUT_en` | output |
| **Port C** | | |
| PC0  | `12V_sense` | analog |
| PC1  | `R_LED_GREEN` | output |
| PC2  | `L_Un_3x_sense` | analog, ADC123_IN12 - gained Nernst, L |
| PC3  | `L_Ip_sense` | analog, ADC123_IN13 |
| PC4  | `L_OUT_sense` | analog |
| PC5  | `L_Heater_sense` | analog, ADC12_IN15 |
| PC6  | `DACout1` | TIM8_CH1, `AUXOUT_DAC_PWM_CHANNEL_0` |
| PC7  | `DACout1` inverted | TIM8_CH2, `..._CHANNEL_0_NC` |
| PC8  | `DACout2` | TIM8_CH3, `AUXOUT_DAC_PWM_CHANNEL_1` |
| PC9  | `DACout2` inverted | TIM8_CH4, `..._CHANNEL_1_NC` |
| PC10 | `BT_UART_TX` | remapped USART3, TS primary port (SD3) |
| PC11 | `BT_UART_RX` | remapped USART3 (board.h types this as "PA11") |
| PC12 | `config0` | strap - see 6.5 |
| PC13 | `config0` | strap - see 6.5 |
| PC14 | `SPI_CS1` | EGT chip select 1 |
| PC15 | `SPI_CS0` | EGT chip select 0 |
| **Port D** | | |
| PD0  | `OSC_IN` | |
| PD1  | `OSC_OUT` | |
| PD2  | `SPI_CS2` | third chip select, unused by firmware |

### 6.2 ADC chain - 10 channels

Sequence is author-defined across `SQR3` (SQ1-6) and `SQR2` (SQ7-10). The buffer
index is `SQn - 1`.

| idx | SQ   | Channel | Pin | Signal | Consumed as |
| --- | ---- | ------- | --- | ------ | ----------- |
| 0   | SQ1  | IN0     | PA0 | `R_Ip_sense`      | `ch[1].PumpCurrentVoltage` |
| 1   | SQ2  | IN1     | PA1 | `R_Un_3x_sense`   | `ch[1].NernstVoltage` (gained) |
| 2   | SQ3  | IN13    | PC3 | `L_Ip_sense`      | `ch[0].PumpCurrentVoltage` |
| 3   | SQ4  | IN12    | PC2 | `L_Un_3x_sense`   | `ch[0].NernstVoltage` (gained) |
| 4   | SQ5  | IN6     | PA6 | `R_AUX_ADC`       | unused - marked "move to slow ADC" |
| 5   | SQ6  | IN7     | PA7 | `L_AUX_ADC`       | unused - marked "move to slow ADC" |
| 6   | SQ7  | IN15    | PC5 | `L_Heater_sense`  | `ch[0].HeaterSupplyVoltage`, peak-held and filtered |
| 7   | SQ8  | IN8     | PB0 | `R_Heater_sense`  | `ch[1].HeaterSupplyVoltage`, peak-held and filtered |
| 8   | SQ9  | IN2     | PA2 | `R_Un_sense`      | `ch[1].NernstVoltage` fallback when the gained input clamps |
| 9   | SQ10 | IN3     | PA3 | `L_Un_sense`      | `ch[0].NernstVoltage` fallback when the gained input clamps |

Two things this board does that the others do not:

- **Clamp detection.** Each channel has both a gained (3x) and an ungained
  Nernst input. `AnalogSampleFinish()` reads the gained one first and falls back
  to the ungained pin when `isClamped()` trips, so the amplifier railing does
  not silently corrupt the reading. Budget two ADC pins per channel for this.
- **Heater voltage is peak-held.** `GetMaxSample()` is used instead of
  `AverageSamples()`, gated on the heater PWM pin actually being low, then run
  through `HEATER_FILTER_ALPHA`. This is what `f1_rev2` could not do because of
  its broken RC filter - the rev1 front end got it right.

`VirtualGroundVoltageInt` is the `HALF_VCC` constant: the board generates
3.3 V / 2 internally and does not measure it.

`ADC_OVERSAMPLE` = 16, `NERNST_INPUT_GAIN` = 3.15, `NERNST_INPUT_OFFSET` = 0.247,
`BATTERY_INPUT_DIVIDER` = `HEATER_INPUT_DIVIDER` = 10 / 110, `VM_RESISTOR_VALUE` = 0.

### 6.3 Peripherals

| Peripheral | Use | Pins |
| ---------- | --- | ---- |
| TIM4 CH1/CH2 | heater PWM R / L | PB6 / PB7 |
| TIM8 CH1-CH4 | AUX analog out, complementary pairs | PC6-PC9 |
| DAC1 OUT1/OUT2 | pump current drive R / L | PA4 / PA5 |
| SPI1 (remapped) | 2x MAX3185x EGT | PB3/PB4/PB5, CS on PC14/PC15 |
| USART1 | TS secondary (J3 connector) | PA9 / PA10 |
| USART3 (remapped) | TS primary, to the JDY-33 BT module | PC10 / PC11 |
| CAN1 | | PA11 / PA12 |
| I2C1 (remapped) | OLED | PB8 / PB9 |

`boardInit()` sets `AFIO_MAPR_I2C1_REMAP | AFIO_MAPR_SPI1_REMAP |
AFIO_MAPR_USART3_REMAP_0 | AFIO_MAPR_SWJ_CFG_1` - note the last one frees PA15,
PB3 and PB4 from JTAG, which SPI1 and `BT_EN` both need.

### 6.4 Free pins

Almost nothing is free on LQFP-64: PB1 (`R_OUT_sense`) and PC4 (`L_OUT_sense`)
are wired to analog inputs but never sampled, and PD2 (`SPI_CS2`) is a third
chip select the firmware does not use. If the new board drops EGT, PB3/PB4/PB5
and PC14/PC15/PD2 come free as well.

### 6.5 Discrepancies

Cosmetic, but worth fixing when the board files are copied:

1. **`board.h` labels PA3 as `R_Un_3x_sense`**, duplicating the PA1 entry. The
   ADC comment in `port.cpp` and the index-9 usage both say it is
   **`L_Un_sense`**. `port.cpp` is the one that matches the code.
2. **`board.h` writes "PA11 - BT_UART_RX"** inside the Port C block. It means
   **PC11** - USART3 remapped puts RX on PC11, and PA11 is already CAN_RX.
3. **`board.h` labels PA8 as `LED_GREEN`.** `io_pins.h` says `LED_BLUE` = PA8,
   `LED_GREEN` = PB13 and `LED_R_GREEN` = PC1, which matches the Port B / Port C
   comments (`L_LED_GREEN`, `R_LED_GREEN`). `io_pins.h` is what the code uses.
4. **`ID_SEL1` and `ID_SEL2` are both PC13** in `io_pins.h`, while `board.h`
   documents straps on PC12 and PC13. This is dead code, not a live bug:
   `readSelPin()` is only ever called from the two f0 ports, and the F1 boards
   take their CAN index from the flash `Configuration` instead. Wire PC12/PC13
   as straps anyway if you want the option back.

## Discrepancies (f0 and f1_rev2)

Found while cross-checking `board.h` against `port.cpp`. None of them are
introduced by `f0_module_v2` - the ADC block is byte-identical to the upstream
`f0_module`.

1. **f0: PA1 is wired but never read.** `board.h` documents `PA1 - Vm_sense`
   and sets it to analog mode, but `CHSELR` selects channels 0, 2 and 3 only.
   Channel 1 is never converted.

2. **f0: PA3 is read but configured as a digital input.** `CHSEL3` samples it
   into `VirtualGroundVoltageInt`, while `VAL_GPIOA_MODER` has
   `PIN_MODE_INPUT(GPIOA_PIN3)` instead of `PIN_MODE_ANALOG`. The Schmitt
   trigger stays enabled on that pin. On a new board, make this
   `PIN_MODE_ANALOG` - or move virtual-ground sense to PA1 and fix `CHSELR`.

3. **f0: no supply voltage measurement at all.** `HeaterSupplyVoltage` is
   hardcoded to `0` and there is no `BATTERY_INPUT_DIVIDER`, so the
   `HEATER_SUPPLY_ON_VOLTAGE` / `HEATER_SUPPLY_OFF_VOLTAGE` logic in
   `heater_control.cpp` has nothing to work with. A 100 K / 10 K divider into a
   spare ADC pin is worth adding on the new board.

4. **f1_rev2: heater low-side sense is sampled and thrown away.** From
   `port.cpp`: *"Heater measurement circuit has incorrect RC filter making
   inposible accurate measurement when heater pwm has high duty"* - so PB1 is
   read into index 4 and the code substitutes the PB0 battery reading instead.
   If you copy the rev2 analog front end, **fix that RC filter** and the
   measurement becomes usable.

5. **f1_rev2: PA5 (`Un_sense`) is declared analog and left out of the
   sequence.** The real Nernst input is PA7 (`Un_3x_sense`). Two comments in
   `port.cpp` mark PA5 as "no used".

6. **f1_rev2: `boardInit()` is empty.** No `SWJ_CFG` setting, so PA15, PB3 and
   PB4 remain JTAG pins at reset and `DISP0` / `DISP1` are not usable as GPIO.
   Compare with `f1_dual_rev1/board.c`, which sets `AFIO_MAPR_SWJ_CFG_1`.

## Checklist for the new board

- [ ] Part chosen: **`STM32F103RCT6`** (LQFP-64, 256 K/48 K, high density).
      High density is what provides the DAC used for pump drive on PA4/PA5 -
      a medium-density CBT6 would force the PWM+RC approach instead.
      (If you ever drop to LQFP-48, use CBT6 not C8: the config flash sits at
      `0x0801E000` and needs the full 128 K.)
- [ ] Start from `boards/f1_dual_rev1_oled` - it already builds for this part
      with the OLED enabled (48 724 B flash, 34 768 B RAM free).
- [ ] Avoid GD32F103. `f1_rev3` is forced to `-O0` because of an unresolved GD32
      ADC issue; that alone grows the image from 38 K to 67 K.
- [ ] Route PB8 / PB9 to a 4-pin I2C header (3V3, GND, SCL, SDA) with 4k7
      pull-ups. `boardInit()` already remaps I2C1 there; confirm the pads exist
      on your layout since the lambda-x2 hardware files are not in this repo.
- [ ] Per-channel heater sense: 100 K / 10 K dividers to PB0 (R) and PC5 (L),
      plus 12 V sense on PC0. Copy the rev1 RC filter, not the rev2 one.
- [ ] Two Nernst inputs per channel - gained (3x) and ungained - so the clamp
      fallback in `AnalogSampleFinish()` works: PC2 + PA3 (L), PA1 + PA2 (R).
- [ ] Keep the config straps on PC13 / PC14 - `readSelPin()` needs them to float
      cleanly, so no strong pull-ups on the board.
- [ ] Virtual ground: rev1 generates 3.3 V / 2 internally and the firmware
      assumes `HALF_VCC`. If you measure it instead (like the F0 does),
      `port.cpp` has to change too.
- [ ] Reserve 8 K at the top of flash for `configflash` and 8 K at the bottom for
      OpenBLT.
