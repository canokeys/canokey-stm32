#include "main.h"
#include <device.h>

void device_delay(int ms) { HAL_Delay(ms); }

uint32_t device_get_tick(void) { return HAL_GetTick(); }

uint8_t is_nfc(void) { return 0; }

// ARM Cortex-M Programming Guide to Memory Barrier Instructions,	Application Note 321

int device_spinlock_lock(volatile uint32_t *lock, uint32_t blocking) {
  // Note: __LDREXW and __STREXW are CMSIS functions
  int status = 0;
  do {
    while (__LDREXW(lock) != 0)
      if (!blocking)
        return -1;
      else {
      } // Wait until
    // lock is free
    status = __STREXW(1, lock); // Try to set
    // lock
  } while (status != 0); // retry until lock successfully
  __DMB();               // Do not start any other memory access
  // until memory barrier is completed
  return 0;
}

void device_spinlock_unlock(volatile uint32_t *lock) {
  // Note: __LDREXW and __STREXW are CMSIS functions
  __DMB(); // Ensure memory operations completed before
  // releasing lock
  *lock = 0;
}
