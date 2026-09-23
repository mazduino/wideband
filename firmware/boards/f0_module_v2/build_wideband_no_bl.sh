#!/bin/bash

# Debug build: no bootloader, application linked at 0x08000000.
#
# The 6k the bootloader normally occupies goes to the application instead, which
# is what makes room for -Og and no LTO - the regular image does not fit those.
# -O0 does not fit even here (overflows appflash by ~8.5k).
# Flash build/wideband.bin to 0x08000000 over SWD (J2) - an image built this way
# can not be updated over CAN.

set -eo pipefail

cd ../..

rm -rf build/

make -j12 BOARD=f0_module_v2 \
	USE_BOOTLOADER=no \
	USE_LTO=no \
	USE_OPT="-Og -ggdb -fomit-frame-pointer -falign-functions=16 -fsingle-precision-constant -DWB_PROD=1"
