#pragma once

// Fundamental board constants
#define VCC_VOLTS (3.3f)
#define HALF_VCC (VCC_VOLTS / 2)
#define ADC_MAX_COUNT (4095)
#define ADC_OVERSAMPLE 24

// *******************************
//    Nernst voltage & ESR sense
// *******************************
#define NERNST_INPUT_GAIN (2.7f)

#define AFR_CHANNELS 1

// *******************************
//    Nernst voltage & ESR sense
// *******************************
#define VM_RESISTOR_VALUE (10)

// TEMPORARY DIAGNOSTIC: freeze (instead of resetting) on CPU fault, and stash
// fault info in g_faultDiag[] (see main.cpp) so it can be read via SWD memory
// read while frozen. Remove once the boot-crash issue is root-caused.
#define FAULT_DIAGNOSTIC_FREEZE
