#!/bin/bash

BOARD=f1_dual_rev1_oled \
USE_OPT="-Og -ggdb -fomit-frame-pointer -falign-functions=16 -fsingle-precision-constant" \
	../build_f1_board.sh
