// SPDX-License-Identifier: Apache-2.0
#include "device-config.h"
#include "main.h"
#include "stm32l4xx_ll_gpio.h"
#include "stm32l4xx_ll_tim.h"
#include <admin.h>
#include <device.h>
#include <usb_device.h>

/* This file overrides functions defined in canokey-core/src/device.c */

const uint32_t UNTOUCHED_MAX_VAL = 40; /* Suitable for 56K pull-down resistor */
const uint32_t CALI_TIMES = 4;

TIM_HandleTypeDef htim6;
extern SPI_HandleTypeDef FM_SPI;

static volatile uint32_t blinking_until;
static uint16_t touch_threshold = 14, measure_touch;
static void (*tim_callback)(void);

void device_delay(int ms) { HAL_Delay(ms); }

uint32_t device_get_tick(void) { return HAL_GetTick(); }

void device_set_timeout(void (*callback)(void), uint16_t timeout) {
  if (timeout == 0) {
    HAL_TIM_Base_Stop_IT(&htim6);
    return;
  }
  tim_callback = callback;
  htim6.Instance = TIM6;
  htim6.Init.Prescaler = 7999;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  if (is_nfc()) // TODO：calc the period by Sysclk
    htim6.Init.Period = 2 * timeout - 1;
  else
    htim6.Init.Period = 10 * timeout - 1;
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
    DBG_MSG("max limit exceeded, discarded. touch_threshold %u\n", touch_threshold);
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

void led_on(void) { HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET); }

void led_off(void) { HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET); }

void device_periodic_task(void) {
  static uint32_t deassert_at = ~0u;
  uint32_t tick = HAL_GetTick();
  if (GPIO_Touched()) {
    set_touch_result(TOUCH_SHORT);
    deassert_at = tick + 2000;
  } else if (tick > deassert_at) {
    DBG_MSG("De-assert %u\r\n", measure_touch);
    measure_touch = 0;
    set_touch_result(TOUCH_NO);
    deassert_at = ~0u;
  }
  device_update_led();
}

void fm_nss_low(void) { HAL_GPIO_WritePin(FM_SSN_GPIO_Port, FM_SSN_Pin, GPIO_PIN_RESET); }

void fm_nss_high(void) { HAL_GPIO_WritePin(FM_SSN_GPIO_Port, FM_SSN_Pin, GPIO_PIN_SET); }

void fm_transmit(uint8_t *buf, uint8_t len) { HAL_SPI_Transmit(&FM_SPI, buf, len, 1000); }

void fm_receive(uint8_t *buf, uint8_t len) { HAL_SPI_Receive(&FM_SPI, buf, len, 1000); }

static bool hwcfg_set_and_probe(uint16_t out, uint16_t in) {
  HAL_GPIO_WritePin(HW_CFG_Port, out, 0);
  for (int i = 0; i < 10; ++i)
    asm volatile("nop");
  bool conn = HAL_GPIO_ReadPin(HW_CFG_Port, in) == 0;
  HAL_GPIO_WritePin(HW_CFG_Port, out, 1);
  return conn;
}

uint8_t stm32_hw_variant_probe(void) {
#define N_CFGPIN 4
  uint16_t cfg_pins[N_CFGPIN] = {HW_CFG0_Pin, HW_CFG1_Pin, HW_CFG2_Pin, HW_CFG3_Pin};
  uint8_t result = CANOKEY_STM32L4_EARLY_ES;
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  for (int i = 0; i < N_CFGPIN; i++) {
    GPIO_InitStruct.Pin = cfg_pins[i];
    HAL_GPIO_Init(HW_CFG_Port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(HW_CFG_Port, cfg_pins[i], 1);
  }

  if (hwcfg_set_and_probe(cfg_pins[0], cfg_pins[1]))
    result = CANOKEY_STM32L4_USBA_NANO_R2;
  else if (hwcfg_set_and_probe(cfg_pins[2], cfg_pins[3]))
    result = CANOKEY_STM32L4_USBA_NFC_R3;
  else if (hwcfg_set_and_probe(cfg_pins[1], cfg_pins[3]))
    result = CANOKEY_STM32L4_USBC_NFC_R1;

  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  for (int i = 0; i < N_CFGPIN; i++) {
    GPIO_InitStruct.Pin = cfg_pins[i];
    HAL_GPIO_Init(HW_CFG_Port, &GPIO_InitStruct);
  }
  return result;
}

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
    EP_SIZE_TABLE.kbd_hid = 8;
  }
}

int device_atomic_compare_and_swap(volatile uint32_t *var, uint32_t expect, uint32_t update) {
  int status = 0;
  do {
    if (__LDREXW(var) != expect) return -1;
    status = __STREXW(update, var); // Try to set
  } while (status != 0);            // retry until updated
  __DMB();                          // Do not start any other memory access
  return 0;
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
