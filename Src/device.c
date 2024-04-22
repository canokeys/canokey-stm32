// SPDX-License-Identifier: Apache-2.0
#include "device-config.h"
#include "main.h"
#include "stm32l4xx_ll_gpio.h"
#include "stm32l4xx_ll_lpuart.h"
#include "stm32l4xx_ll_usart.h"
#include "stm32l4xx_ll_rcc.h"
#include "stm32l4xx_ll_tim.h"
#include <admin.h>
#include <device.h>
#include <usb_device.h>

/* This file overrides functions defined in canokey-core/src/device.c */

const uint32_t UNTOUCHED_MAX_VAL = 40; /* Suitable for 56K pull-down resistor */
const uint32_t CALI_TIMES = 4;
const uint32_t TOUCH_GAP_TIME = 1500; /* Gap period (in ms) between two consecutive touch events */

extern TIM_HandleTypeDef htim6;
extern SPI_HandleTypeDef FM_SPI;
extern UART_HandleTypeDef DBG_UART;

static volatile uint32_t blinking_until;
static uint16_t touch_threshold = 14, measure_touch;
static void (*tim_callback)(void);

void device_delay(int ms) { HAL_Delay(ms); }

uint32_t device_get_tick(void) { return HAL_GetTick(); }

void device_set_timeout(void (*callback)(void), uint16_t timeout) {
  const uint32_t prescaler = 4000;
  uint32_t counting_freq =
      HAL_RCC_GetPCLK1Freq() * (LL_RCC_GetAPB1Prescaler() == LL_RCC_APB1_DIV_1 ? 1 : 2) / prescaler;
  // DBG_MSG("counting_freq=%u\n", counting_freq);
  if (timeout == 0) {
    HAL_TIM_Base_Stop_IT(&htim6);
    return;
  }
  tim_callback = callback;
  htim6.Instance = TIM6;
  htim6.Init.Prescaler = prescaler - 1;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6.Init.Period = counting_freq / 1000 * timeout - 1;
  if (htim6.Init.Period > 65535) {
    ERR_MSG("TIM6 period %u overflow!\n", htim6.Init.Period);
    htim6.Init.Period = 65535;
  }
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

    // charging the capacitor (human body)
    for (int j = 0; j < 100; ++j)
      asm volatile("nop");

    __disable_irq();
    // measure the time of discharging
    LL_GPIO_SetPinMode(TOUCH_GPIO_Port, TOUCH_Pin, GPIO_MODE_INPUT);
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
  uint32_t counter = 0;
  LL_GPIO_SetPinMode(TOUCH_GPIO_Port, TOUCH_Pin, GPIO_MODE_OUTPUT_PP);
  LL_GPIO_SetOutputPin(TOUCH_GPIO_Port, TOUCH_Pin);
  // charging the capacitor (human body)
  for (int i = 0; i < 100; ++i)
    asm volatile("nop");

  __disable_irq();
  // measure the time of discharging
  LL_GPIO_SetPinMode(TOUCH_GPIO_Port, TOUCH_Pin, GPIO_MODE_INPUT);
  while ((LL_GPIO_ReadInputPort(TOUCH_GPIO_Port) & TOUCH_Pin) /*  && counter <= touch_threshold */)
    ++counter;
  __enable_irq();

  if (counter > measure_touch) measure_touch = counter;
  return counter > touch_threshold ? GPIO_PIN_SET : GPIO_PIN_RESET;
}

void led_on(void) { HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET); }

void led_off(void) { HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET); }

void device_periodic_task(void) {
  enum {
    TOUCH_STATE_IDLE,
    TOUCH_STATE_DOWN,
    TOUCH_STATE_ASSERT,
    TOUCH_STATE_DEASSERT,
  };
  static uint32_t event_tick, fsm = TOUCH_STATE_IDLE;
  uint32_t tick = HAL_GetTick();
  switch (fsm)
  {
  case TOUCH_STATE_IDLE:
#ifdef DEBUG_OUTPUT
    // Emulate touch events with UART input
    if (LL_USART_IsActiveFlag_RXNE(DBG_UART.Instance)) {
      int data = LL_USART_ReceiveData8(DBG_UART.Instance);
      DBG_MSG("UART: %x\n", data);
      if ('T' == data || 'L' == data) {
        set_touch_result('T' == data ? TOUCH_SHORT : TOUCH_LONG);
        fsm = TOUCH_STATE_ASSERT;
        event_tick = tick;
      }
    }
#endif
    if(GPIO_Touched()) {
      measure_touch = 0;
      fsm = TOUCH_STATE_DOWN;
      event_tick = tick;
    }
    break;
  case TOUCH_STATE_DOWN:
    if(!GPIO_Touched() || tick - event_tick > 500) {
      if (tick - event_tick > 50) {
        set_touch_result(tick - event_tick > 500 ? TOUCH_LONG : TOUCH_SHORT);
        fsm = TOUCH_STATE_ASSERT;
        event_tick = tick;
      } else
        fsm = TOUCH_STATE_IDLE;
    }
    break;
  case TOUCH_STATE_ASSERT:
    if (tick - event_tick > TOUCH_GAP_TIME) {
      set_touch_result(TOUCH_NO);
      fsm = TOUCH_STATE_DEASSERT;
      DBG_MSG("touch deassert, max value measured: %u\r\n", measure_touch);
    }
    break;
  case TOUCH_STATE_DEASSERT:
    if(!GPIO_Touched()) {
      measure_touch = 0;
      fsm = TOUCH_STATE_IDLE;
    }
    break;
  default:
    break;
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

  EP_TABLE.kbd_hid = ep;
  IFACE_TABLE.kbd_hid = iface;
  EP_SIZE_TABLE.kbd_hid = 8;
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
