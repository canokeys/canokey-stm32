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
#include "stm32l4xx_ll_spi.h"
#include "stm32l4xx_ll_usart.h"
#include "device-config.h"
#include "device-stm32.h"
#include "lfs_init.h"
#include <apdu.h>
#include <applets.h>
#include <ccid.h>
#include <device.h>
#include <nfc.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
RNG_HandleTypeDef hrng;

SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim6;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
extern uint32_t _stack_boundary;
uint8_t device_loop_enable;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_RNG_Init(void);
static void MX_SPI1_Init(void);
// static void MX_TIM6_Init(void);
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

// Initial system clock profile. 
// In NFC (low-power) mode: SYSCLK is 32MHz currently
// In USB mode: SYSCLK is 80MHz
void SystemClock_CustomConfig(bool nfc_low_power, bool pll_reconfig) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
  uint32_t flash_wait;

  if (pll_reconfig) {
    /** Switch SYSCLK to MSI before PLL reconfig */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK) Error_Handler();
  }

  /** Initializes the CPU, AHB and APB busses clocks  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48 | RCC_OSCILLATORTYPE_MSI;
  if (nfc_low_power) {
    flash_wait = FLASH_LATENCY_1; // according to Ref Manual 3.3.3
    RCC_OscInitStruct.HSI48State = RCC_HSI48_OFF;
    RCC_OscInitStruct.PLL.PLLN = 16;
    RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2; // to RNG, =SYSCLK
  } else {
    flash_wait = FLASH_LATENCY_4;
    RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
    RCC_OscInitStruct.PLL.PLLN = 40;
    RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV4; // unused
  }
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV30; // SAI unused
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();

  /** Initializes the CPU, AHB and APB busses clocks  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  if (nfc_low_power) {
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV8; // PCLK1 affects TIM6, LPUART1
  } else {
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1; // PCLK1 affects TIM6, LPUART1, USB SRAM, CRS
  }
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV4; // PCLK2 affects SPI1
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, flash_wait) != HAL_OK) Error_Handler();

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_LPUART1 | RCC_PERIPHCLK_RNG;
  if (nfc_low_power) {
    PeriphClkInit.RngClockSelection = RCC_RNGCLKSOURCE_PLL;
  } else {
    PeriphClkInit.PeriphClockSelection |= RCC_PERIPHCLK_USB;
    PeriphClkInit.RngClockSelection = RCC_USBCLKSOURCE_HSI48;
  }
  PeriphClkInit.Lpuart1ClockSelection = RCC_LPUART1CLKSOURCE_PCLK1;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_HSI48;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) Error_Handler();

  if (!nfc_low_power) {
    RCC_CRSInitTypeDef RCC_CRSInitStruct = {0};
    __HAL_RCC_CRS_CLK_ENABLE();
    /** Configures CRS */
    RCC_CRSInitStruct.Prescaler = RCC_CRS_SYNC_DIV1;
    RCC_CRSInitStruct.Source = RCC_CRS_SYNC_SOURCE_USB;
    RCC_CRSInitStruct.Polarity = RCC_CRS_SYNC_POLARITY_RISING;
    RCC_CRSInitStruct.ReloadValue = __HAL_RCC_CRS_RELOADVALUE_CALCULATE(48000000, 1000);
    RCC_CRSInitStruct.ErrorLimitValue = 34;
    RCC_CRSInitStruct.HSI48CalibrationValue = 32;

    HAL_RCCEx_CRSConfig(&RCC_CRSInitStruct);
  }
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
  SystemClock_CustomConfig(false, true);
  // reconfig peripheral clock dividers
  LL_SPI_SetBaudRatePrescaler(hspi1.Instance, LL_SPI_BAUDRATEPRESCALER_DIV8);
  MX_USART2_UART_Init();

  /* Enable USB power on Pwrctrl CR2 register. */
  HAL_PWREx_EnableVddUSB();
  /* Peripheral clock enable */
  __HAL_RCC_USB_CLK_ENABLE();
  /* Peripheral interrupt init */
  HAL_NVIC_SetPriority(USB_IRQn, 1, 0);
  usb_device_init();
  // enable the device_periodic_task, which controls LED and Touch sensing
  device_loop_enable = 1;
}

int check_is_nfc_en(void) {
  uint32_t *flash_loc = (uint32_t*) 0x1FFF7808U;
  uint32_t val = *flash_loc; //FLASH->PCROP1SR;
  DBG_MSG("%x\n", val);
  return val == 0xFFFFFFFFU || // ST production default value
          val == 0xFFFF802a; // magic written by admin_vendor_nfc_enable()
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
  SystemClock_CustomConfig(true, false);

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  // We first initialize GPIO & SPI to detect NFC field
  MX_GPIO_Init();
  MX_SPI1_Init();
  // Then initialize other peripherals
  MX_RNG_Init();
  MX_USART2_UART_Init();
  SetupMPU(); // comment out this line during on-chip debugging
  /* USER CODE BEGIN 2 */
  in_nfc_mode = check_is_nfc_en(); // boot in NFC mode by default
  nfc_init();
  set_nfc_state(in_nfc_mode);

  DBG_MSG("Init FS\n");
  littlefs_init();

  DBG_MSG("Init applets\n");
  applets_install();
  init_apdu_buffer();

  if (!in_nfc_mode) {
    while (!detect_usb());
    config_usb_mode();
  }
  DBG_MSG("Main Loop\n");
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
        set_nfc_state(in_nfc_mode); // comment out this line to emulate NFC mode with USB connection
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
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

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
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
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
