#include "main.h"
#include "stm32l4xx_ll_gpio.h"
#include "stm32l4xx_ll_tim.h"
#include <admin.h>
#include <device.h>
#include <usb_device.h>

/* This file overrides functions defined in canokey-core/src/device.c */

const uint32_t UNTOUCHED_MAX_VAL = 10; /* Suitable for 56K pull-down resistor */
const uint32_t CALI_TIMES = 4;
TIM_HandleTypeDef htim6;

static volatile uint32_t blinking_until;
static uint16_t touch_threshold = 5, measure_touch;
static uint8_t has_rf;
static void (*tim_callback)(void);

void device_delay(int ms) { HAL_Delay(ms); }

uint32_t device_get_tick(void) { return HAL_GetTick(); }

void device_set_timeout(void (*callback)(void), uint16_t timeout) {
  if (timeout == 0) HAL_TIM_Base_Stop_IT(&htim6);
  tim_callback = callback;
  htim6.Instance = TIM6;
  htim6.Init.Prescaler = 7999;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  if (is_nfc())
    htim6.Init.Period = 2 * timeout - 1;
  else
    htim6.Init.Period = 9 * timeout - 1;
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim6) != HAL_OK) Error_Handler();
  LL_TIM_ClearFlag_UPDATE(htim6.Instance);
  LL_TIM_SetCounter(htim6.Instance, 0);
  HAL_TIM_Base_Start_IT(&htim6);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
  if (htim == &htim6) {
    HAL_TIM_Base_Stop_IT(&htim6);
    tim_callback();
  }
}

void set_nfc(uint8_t val) { has_rf = val; }

uint8_t is_nfc(void) { return has_rf; }

void GPIO_Touch_Calibrate(void) {
  uint32_t sum = 0;
  for (int i = 0; i < CALI_TIMES; ++i) {
    LL_GPIO_SetPinMode(TOUCH_GPIO_Port, TOUCH_Pin, GPIO_MODE_OUTPUT_PP);
    LL_GPIO_SetOutputPin(TOUCH_GPIO_Port, TOUCH_Pin);

    for (int j = 0; j < 100; ++j)
      asm volatile("nop");
    LL_GPIO_SetPinMode(TOUCH_GPIO_Port, TOUCH_Pin, GPIO_MODE_INPUT);
    __disable_irq();
    while ((LL_GPIO_ReadInputPort(TOUCH_GPIO_Port) & TOUCH_Pin) && sum < UNTOUCHED_MAX_VAL * CALI_TIMES)
      ++sum;
    __enable_irq();
    // DBG_MSG("val %u\n", sum);
  }
  if (sum == UNTOUCHED_MAX_VAL * CALI_TIMES) {
    DBG_MSG("max limit exceeded, discard...\n");
    return;
  }

  touch_threshold = sum / CALI_TIMES * 2;
  DBG_MSG("touch_threshold %u\n", touch_threshold);
}

static GPIO_PinState GPIO_Touched(void) {
  LL_GPIO_SetPinMode(TOUCH_GPIO_Port, TOUCH_Pin, GPIO_MODE_OUTPUT_PP);
  LL_GPIO_SetOutputPin(TOUCH_GPIO_Port, TOUCH_Pin);
  for (int i = 0; i < 100; ++i)
    asm volatile("nop");
  uint32_t counter = 0;
  LL_GPIO_SetPinMode(TOUCH_GPIO_Port, TOUCH_Pin, GPIO_MODE_INPUT);
  __disable_irq();
  while ((LL_GPIO_ReadInputPort(TOUCH_GPIO_Port) & TOUCH_Pin) /*  && counter <= touch_threshold */)
    ++counter;
  __enable_irq();
  if (counter > measure_touch) measure_touch = counter;
  return counter > touch_threshold ? GPIO_PIN_SET : GPIO_PIN_RESET;
}

void device_periodic_task(void) {
  static uint32_t testcnt = 0, deassert_at = ~0u;
  if (HAL_GetTick() > blinking_until) {
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, cfg_is_led_normally_on());
  } else {
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, ((testcnt >> 9) & 1));
  }
  if (GPIO_Touched()) {
    set_touch_result(TOUCH_SHORT);
    deassert_at = HAL_GetTick() + 2000;
  } else if (HAL_GetTick() > deassert_at) {
    DBG_MSG("De-assert %u\r\n", measure_touch);
    measure_touch = 0;
    set_touch_result(TOUCH_NO);
    deassert_at = ~0u;
  }
  testcnt++;
}

void device_start_blinking(uint8_t sec) {
  if (!sec) {
    blinking_until = ~0u;
    DBG_MSG("Start blinking\n");
  } else {
    blinking_until = HAL_GetTick() + sec * 1000u;
    DBG_MSG("Start blinking until %u\n", blinking_until);
  }
};

void device_stop_blinking(void) { blinking_until = 0; }

/* Override the function defined in usb_device.c */
void usb_resources_alloc(void) {
  uint8_t iface = 0;
  uint8_t ep = 1;

  memset(&IFACE_TABLE, 0xFF, sizeof(IFACE_TABLE));
  memset(&EP_TABLE, 0xFF, sizeof(EP_TABLE));

  EP_TABLE.ctap_hid = ep++;
  IFACE_TABLE.ctap_hid = iface++;
  EP_SIZE_TABLE.ctap_hid = 64;

  IFACE_TABLE.webusb = iface++;

  EP_TABLE.ccid = ep++;
  IFACE_TABLE.ccid = iface++;
  EP_SIZE_TABLE.ccid = 64;

  if (cfg_is_kbd_interface_enable() && ep <= EP_ADDR_MSK) {
    DBG_MSG("Keyboard interface enabled, Iface %u\n", iface);
    EP_TABLE.kbd_hid = ep;
    IFACE_TABLE.kbd_hid = iface;
    IFACE_TABLE.kbd_hid = 8;
  }
}

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
