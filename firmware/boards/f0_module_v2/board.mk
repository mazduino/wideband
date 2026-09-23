USE_BOOTLOADER = yes

# Main thread only runs the LED loop - 1k of process stack is far more than it
# needs, and the RAM is needed for the thread working areas.
USE_PROCESS_STACKSIZE = 0x200

MCU = cortex-m0

include $(CHIBIOS)/os/common/startup/ARMCMx/compilers/GCC/mk/startup_stm32f0xx.mk
include $(CHIBIOS)/os/hal/ports/STM32/STM32F0xx/platform.mk
include $(CHIBIOS)/os/common/ports/ARMv6-M/compilers/GCC/mk/port.mk
