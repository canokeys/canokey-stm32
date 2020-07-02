/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "device-config.h"
#include "device-stm32.h"
#include "git-rev.h"
#include "lfs_init.h"
#include <admin.h>
#include <ccid.h>
#include <ctap.h>
#include <device.h>
#include <nfc.h>
#include <oath.h>
#include <openpgp.h>
#include <piv.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define VENDOR_NFC_SET 0x01
#define VENDOR_NFC_GET 0x02
#define VENDOR_RDP 0x55
#define VENDOR_DFU 0x22
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
RNG_HandleTypeDef hrng;

SPI_HandleTypeDef hspi1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
extern uint32_t _stack_boundary;
uint32_t device_loop_enable;
static uint8_t variant;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static void MX_GPIO_Init(void);
static void MX_RNG_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// override the function defined in rand.c
uint32_t random32(void) {
  uint32_t v;
  while (HAL_RNG_GenerateRandomNumber(&hrng, &v) != HAL_OK)
    ;
  return v;
}

int SetupMPU(void) {
  if (MPU->TYPE == 0) return -1;
  int nRegion = MPU->TYPE >> 8 & 0xFF;
  MPU_Region_InitTypeDef configs[] = {
      {
          .Enable = MPU_REGION_ENABLE,
          .BaseAddress = SRAM1_BASE,
          .Size = MPU_REGION_SIZE_64KB,
          .SubRegionDisable = 0,
          .TypeExtField = MPU_TEX_LEVEL0,
          .AccessPermission = MPU_REGION_FULL_ACCESS,
          .DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE,
          .IsShareable = MPU_ACCESS_SHAREABLE,
          .IsCacheable = MPU_ACCESS_CACHEABLE,
          .IsBufferable = MPU_ACCESS_NOT_BUFFERABLE,
      },
      {
          .Enable = MPU_REGION_ENABLE,
          .BaseAddress = ((uint32_t)&_stack_boundary) - 0x100,
          .Size = MPU_REGION_SIZE_256B, // stack overflow protection
          .SubRegionDisable = 0,
          .TypeExtField = MPU_TEX_LEVEL0,
          .AccessPermission = MPU_REGION_NO_ACCESS,
          .DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE,
          .IsShareable = MPU_ACCESS_SHAREABLE,
          .IsCacheable = MPU_ACCESS_CACHEABLE,
          .IsBufferable = MPU_ACCESS_NOT_BUFFERABLE,
      },
      {
          .Enable = MPU_REGION_ENABLE,
          .BaseAddress = FLASH_BASE,
          .Size = MPU_REGION_SIZE_256KB,
          .SubRegionDisable = 0,
          .TypeExtField = MPU_TEX_LEVEL0,
          .AccessPermission = MPU_REGION_FULL_ACCESS,
          .DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE,
          .IsShareable = MPU_ACCESS_NOT_SHAREABLE,
          .IsCacheable = MPU_ACCESS_CACHEABLE,
          .IsBufferable = MPU_ACCESS_NOT_BUFFERABLE,
      },
      {
          .Enable = MPU_REGION_ENABLE,
          .BaseAddress = PERIPH_BASE,
          .Size = MPU_REGION_SIZE_512MB,
          .SubRegionDisable = 0,
          .TypeExtField = MPU_TEX_LEVEL0,
          .AccessPermission = MPU_REGION_FULL_ACCESS,
          .DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE,
          .IsShareable = MPU_ACCESS_SHAREABLE,
          .IsCacheable = MPU_ACCESS_NOT_CACHEABLE,
          .IsBufferable = MPU_ACCESS_BUFFERABLE,
      },
      {
          .Enable = MPU_REGION_ENABLE,
          .BaseAddress = 0x1FFF0000, // Option Bytes, OTP Area, System Memory
          .Size = MPU_REGION_SIZE_64KB,
          .SubRegionDisable = 0,
          .TypeExtField = MPU_TEX_LEVEL0,
          .AccessPermission = MPU_REGION_PRIV_RO,
          .DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE,
          .IsShareable = MPU_ACCESS_NOT_SHAREABLE,
          .IsCacheable = MPU_ACCESS_CACHEABLE,
          .IsBufferable = MPU_ACCESS_NOT_BUFFERABLE,
      },
  };
  HAL_MPU_Disable();
  for (int i = 0; i < nRegion; i++) {
    if (i < sizeof(configs) / sizeof(configs[0])) {
      configs[i].Number = i;
      HAL_MPU_ConfigRegion(configs + i);
    } else {
      MPU->RNR = i;
      MPU->RBAR = 0;
      MPU->RASR = 0;
    }
  }
  SCB->SHCSR |= SCB_SHCSR_MEMFAULTENA_Msk;
  SCB->SHCSR |= SCB_SHCSR_BUSFAULTENA_Msk;
  NVIC_SetPriority(MemoryManagement_IRQn, -1);
  NVIC_SetPriority(BusFault_IRQn, -1);

  HAL_MPU_Enable(0);
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

// override functions defined in admin.c

int admin_vendor_hw_variant(const CAPDU *capdu, RAPDU *rapdu) {
  UNUSED(capdu);

  const char *s;
  static const char *const hw_variant_str[] = {
      [CANOKEY_STM32L4_EARLY_ES] = "Canokey ES",
      [CANOKEY_STM32L4_USBA_NFC_R3] = "Canokey NFC-A",
      [CANOKEY_STM32L4_USBA_NANO_R2] = "Canokey Nano-A",
      [CANOKEY_STM32L4_USBC_NFC_R1] = "Canokey NFC-C",
  };

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

// Initial system clock profile, 40MHz currently
void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the CPU, AHB and APB busses clocks  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 20;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV30; // SAI unused
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();

  /** Initializes the CPU, AHB and APB busses clocks  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK) Error_Handler();
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART2 | RCC_PERIPHCLK_RNG;
  PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
  PeriphClkInit.RngClockSelection = RCC_RNGCLKSOURCE_PLL;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) Error_Handler();
}

// High-pref system clock profile
void SystemClock_Config_80M(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
  RCC_CRSInitTypeDef RCC_CRSInitStruct = {0};

  /** Changes SYSCLKSource to MSI */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK) Error_Handler();

  /** Initializes the CPU, AHB and APB busses clocks */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48 | RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 40;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV8;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();

  /** Initializes the CPU, AHB and APB busses clocks */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK) Error_Handler();
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART2 | RCC_PERIPHCLK_USB | RCC_PERIPHCLK_RNG;
  PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_HSI48;
  PeriphClkInit.RngClockSelection = RCC_RNGCLKSOURCE_HSI48;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) Error_Handler();

  __HAL_RCC_CRS_CLK_ENABLE();
  /** Configures CRS */
  RCC_CRSInitStruct.Prescaler = RCC_CRS_SYNC_DIV1;
  RCC_CRSInitStruct.Source = RCC_CRS_SYNC_SOURCE_USB;
  RCC_CRSInitStruct.Polarity = RCC_CRS_SYNC_POLARITY_RISING;
  RCC_CRSInitStruct.ReloadValue = __HAL_RCC_CRS_RELOADVALUE_CALCULATE(48000000, 1000);
  RCC_CRSInitStruct.ErrorLimitValue = 34;
  RCC_CRSInitStruct.HSI48CalibrationValue = 32;

  HAL_RCCEx_CRSConfig(&RCC_CRSInitStruct);

  if (HAL_SPI_DeInit(&hspi1) != HAL_OK) Error_Handler();
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;
  if (HAL_SPI_Init(&hspi1) != HAL_OK) Error_Handler();
}

uint8_t detect_usb(void) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = GPIO_PIN_12; // USB_DP
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_12) == 0) return 1;

  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  return 0;
}

static void config_usb_mode(void) {
  DBG_MSG("Init USB\n");
  SystemClock_Config_80M();
  MX_USART2_UART_Init(); // re-config the baudrate counter
  usb_device_init();
  // enable the device_periodic_task, which controls LED and Touch sensing
  device_loop_enable = 1;
}
/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {
  /* USER CODE BEGIN 1 */
  uint8_t in_nfc_mode;

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  // We first initialize GPIO & SPI to detect NFC field
  MX_GPIO_Init();
  MX_SPI1_Init();
  // Then initialize other peripherals
  MX_RNG_Init();
  MX_USART2_UART_Init();
  SetupMPU();
  /* USER CODE BEGIN 2 */
  variant = stm32_hw_variant_probe();
  in_nfc_mode = 1; // boot in NFC mode by default
  nfc_init();
  set_nfc_state(in_nfc_mode);

  DBG_MSG("Init FS\n");
  littlefs_init();

  DBG_MSG("Init applets\n");
  admin_install(); // initialize admin applet first to load config
  openpgp_install(0);
  piv_install(0);
  oath_install(0);
  ctap_install(0);

  DBG_MSG("Main Loop, HW %u\n", (unsigned int)variant);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  for (uint32_t i = 0;;) {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    if (in_nfc_mode) {
      nfc_loop();
      if (detect_usb()) { // USB plug-in
        config_usb_mode();
        in_nfc_mode = 0;
        set_nfc_state(in_nfc_mode);
      }
    } else {
      if ((i & ((1 << 23) - 1)) == 0) {
        DBG_MSG("Touch calibrating...\n");
        GPIO_Touch_Calibrate();
      }
      device_loop(1);
      ++i;
    }
  }
  /* USER CODE END 3 */
}

/**
 * @brief RNG Initialization Function
 * @param None
 * @retval None
 */
static void MX_RNG_Init(void) {

  /* USER CODE BEGIN RNG_Init 0 */

  /* USER CODE END RNG_Init 0 */

  /* USER CODE BEGIN RNG_Init 1 */

  /* USER CODE END RNG_Init 1 */
  hrng.Instance = RNG;
  if (HAL_RNG_Init(&hrng) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN RNG_Init 2 */

  /* USER CODE END RNG_Init 2 */
}

/**
 * @brief SPI1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_SPI1_Init(void) {

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_HIGH;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */
}

/**
 * @brief USART2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART2_UART_Init(void) {

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */
}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(FM_SSN_GPIO_Port, FM_SSN_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : FM_IRQN_Pin */
  GPIO_InitStruct.Pin = FM_IRQN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(FM_IRQN_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : FM_SSN_Pin */
  GPIO_InitStruct.Pin = FM_SSN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
  HAL_GPIO_Init(FM_SSN_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LED_Pin */
  GPIO_InitStruct.Pin = LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : TOUCH_Pin */
  GPIO_InitStruct.Pin = TOUCH_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(TOUCH_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI0_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);
}

/* USER CODE BEGIN 4 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
  if (GPIO_Pin == FM_IRQN_Pin) {
    nfc_handler();
  }
}
/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  ERR_MSG("in");
  /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(char *file, uint32_t line) {
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
