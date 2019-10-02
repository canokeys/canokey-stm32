#include "main.h"

void device_delay(int ms) { HAL_Delay(ms); }

uint32_t device_get_tick(void) { return HAL_GetTick(); }
