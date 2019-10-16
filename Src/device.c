#include "main.h"
#include <device.h>

void device_delay(int ms) { HAL_Delay(ms); }

uint32_t device_get_tick(void) { return HAL_GetTick(); }

uint8_t is_nfc(void) { return 0; }

void device_disable_irq(void) { __disable_irq(); }

void device_enable_irq(void) { __enable_irq(); }

void device_start_blinking(uint8_t sec) {}

void device_stop_blinking(void) {}
