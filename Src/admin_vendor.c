// SPDX-License-Identifier: Apache-2.0

#include "main.h"
#include "device-config.h"
#include "device-stm32.h"
#include "git-rev.h"
#include <usb_device.h>
#include <device.h>
#include <admin.h>

#define VENDOR_NFC_SET 0x01
#define VENDOR_NFC_GET 0x02
#define VENDOR_STACK_TEST 0x03
#define VENDOR_RDP 0x55
#define VENDOR_DFU 0x22

void EnableRDP(uint32_t level);
void EnterDFUBootloader(void);

// override functions defined in admin.c

int admin_vendor_hw_variant(const CAPDU *capdu, RAPDU *rapdu) {
  UNUSED(capdu);

  const char *s;
  static const char *const hw_variant_str[] = {
      [CANOKEY_STM32L4_EARLY_ES] = "CanoKey ES (STM32)",
      [CANOKEY_STM32L4_USBA_NFC_R3] = "CanoKey NFC-A (STM32)",
      [CANOKEY_STM32L4_USBA_NANO_R2] = "CanoKey Nano-A (STM32)",
      [CANOKEY_STM32L4_USBC_NFC_R1] = "CanoKey NFC-C (STM32)",
  };

  uint8_t variant = stm32_hw_variant_probe();
  if (variant >= sizeof(hw_variant_str) / sizeof(const char *) || !hw_variant_str[variant])
    s = "Error";
  else
    s = hw_variant_str[variant];

  size_t len = strlen(s);
  memcpy(RDATA, s, len);
  LL = len;
  if (LL > LE) LL = LE;

  return 0;
}

int admin_vendor_version(const CAPDU *capdu, RAPDU *rapdu) {
  UNUSED(capdu);

  size_t len = strlen(GIT_REV);
  memcpy(RDATA, GIT_REV, len);
  memcpy(RDATA + len, "-O", 2);
  LL = len + 2;
  if (LL > LE) LL = LE;

  return 0;
}

int admin_vendor_nfc_enable(const CAPDU *capdu, RAPDU *rapdu) {
  if (P1 != 0x00 && P1 != 0x01) EXCEPT(SW_WRONG_P1P2);
  if (P2 != 0x00) EXCEPT(SW_WRONG_P1P2);
  if (LC != 0x00) EXCEPT(SW_WRONG_LENGTH);

  uint32_t magic = P1 * 0x50 + 0x100;
  FLASH_OBProgramInitTypeDef cfg = {
    .OptionType = OPTIONBYTE_PCROP,
    .PCROPConfig = FLASH_BANK_1,
    .PCROPStartAddr = FLASH_BASE + magic, // Reuse this option word as NFC switch
    .PCROPEndAddr = FLASH_BASE + 0xF, // Fixed value
  };
  // DBG_MSG("Unlock OB\n");
  HAL_FLASH_Unlock();
  HAL_FLASH_OB_Unlock();
  int ret = HAL_FLASHEx_OBProgram(&cfg);
  HAL_FLASH_OB_Lock();
  HAL_FLASH_Lock();
  HAL_FLASHEx_OBGetConfig(&cfg);
  // uint32_t *flash_loc = (uint32_t*) 0x1FFF7808U;
  // DBG_MSG("HAL_FLASHEx_OBGetConfig: %d %x %x %x\n",
  //   ret, cfg.PCROPStartAddr, cfg.PCROPEndAddr, *flash_loc);
  // DBG_MSG("value= %x %x\n", *(uint32_t*)FLASH_BASE, *(uint32_t*)cfg.PCROPStartAddr);

  if (ret != HAL_OK) return -1;

  return 0;
}


extern uint32_t _stack_boundary;

static int stack_test(const CAPDU *capdu, RAPDU *rapdu) {
  uint8_t ref;
  uint8_t *cur = &ref;
  uint8_t *bot = (uint8_t*)&_stack_boundary;
  DBG_MSG("bottom = %p\n", bot);
  if (P2 == 0x55) {
    memset(bot, 0xcc, cur - bot - 0x20);
  } else if (P2 == 0x66) {
    for (uint8_t *p = bot; p < cur; p++)
      if (*p != 0xcc) {
        DBG_MSG("address %p is 0x%x\n", p, (int)*p);
        *(uintptr_t*)RDATA = (uintptr_t)p;
        LL = sizeof(uintptr_t);
        return 0;
      }
    EXCEPT(SW_CHECKING_ERROR);
  }
  return 0;
} 

int admin_vendor_specific(const CAPDU *capdu, RAPDU *rapdu) {
  uint16_t addr;

  switch (P1) {
  case VENDOR_NFC_SET:
#define NFC_SET_MAX_LEN 18
    if (LC <= 2 || LC > NFC_SET_MAX_LEN) EXCEPT(SW_WRONG_LENGTH);

    addr = (DATA[0] << 8) | DATA[1];
    if (addr < 0x000C || addr > 0x03CF) EXCEPT(SW_WRONG_DATA);

    fm_write_eeprom(addr, DATA + 2, LC - 2);
    if (P2 == 1) { // verification enabled
      uint8_t *readback_buf = DATA + NFC_SET_MAX_LEN;
      device_delay(10);
      fm_read_eeprom(addr, readback_buf, LC - 2);
      if (memcmp(DATA + 2, readback_buf, LC - 2)) EXCEPT(SW_CHECKING_ERROR);
    }
    break;

  case VENDOR_NFC_GET:
    if (LC != 2) EXCEPT(SW_WRONG_LENGTH);

    addr = (DATA[0] << 8) | DATA[1];
    if (addr > 0x03CF) EXCEPT(SW_WRONG_DATA);

    fm_read_eeprom(addr, RDATA, LE);
    LL = LE;
    break;

  case VENDOR_STACK_TEST:
    return stack_test(capdu, rapdu);
    break;

  case VENDOR_RDP:
    DBG_MSG("Enable RDP level %d\n", (int)P2);
    if (P2 == 1)
      EnableRDP(OB_RDP_LEVEL_1);
    else if (P2 == 2)
      EnableRDP(OB_RDP_LEVEL_2);
    else
      EXCEPT(SW_WRONG_P1P2);
    break;

  case VENDOR_DFU:
    if (P2 != VENDOR_DFU) EXCEPT(SW_WRONG_P1P2);
    DBG_MSG("Entering DFU\n");
    EnterDFUBootloader();
    ERR_MSG("Failed to enter DFU\n");
    for (;;)
      ;

  default:
    EXCEPT(SW_WRONG_P1P2);
  }

  return 0;
}


void EnableRDP(uint32_t level) {
  FLASH_OBProgramInitTypeDef ob;
  HAL_FLASHEx_OBGetConfig(&ob);
  ob.RDPLevel = level;
  ob.OptionType = OPTIONBYTE_RDP;
  HAL_FLASH_Unlock();
  HAL_FLASH_OB_Unlock();
  HAL_StatusTypeDef ret = HAL_FLASHEx_OBProgram(&ob);
  if (ret != HAL_OK) {
    ERR_MSG("HAL_FLASHEx_OBProgram failed\n");
  }
  HAL_FLASH_Lock();
}

void EnterDFUBootloader() {
  typedef void (*pFunction)(void);
  pFunction JumpToApplication;
  HAL_RCC_DeInit();
  for (int i = 0; i < CRS_IRQn; i++) {
    HAL_NVIC_DisableIRQ(i);
  }
  SysTick->CTRL = 0;
  SysTick->LOAD = 0;
  SysTick->VAL = 0;
  __disable_irq();
  usb_device_deinit();
  HAL_MPU_Disable();
  __DSB();
  __HAL_SYSCFG_REMAPMEMORY_SYSTEMFLASH();
  __DSB();
  __ISB();
  JumpToApplication =
      (void (*)(void))(*((uint32_t *)(0x1FFF0000 + 4))); /* Initialize user application's Stack Pointer */
  __set_MSP(*(__IO uint32_t *)0x1FFF0000);
  JumpToApplication();
}

