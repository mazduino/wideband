#pragma once

// Starts the I2C OLED display thread. Safe to call even if no display is
// attached - the thread keeps retrying until one answers on the bus.
void InitOled();
