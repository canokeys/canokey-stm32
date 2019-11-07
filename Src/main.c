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
#include "stm32l4xx_ll_gpio.h"
#include <admin.h>
#include <ccid.h>
#include <ctap.h>
#include <device.h>
#include <oath.h>
#include <openpgp.h>
#include <piv.h>
#include "lfs_init.h"
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
static uint16_t touch_threshold = 5, measure_touch;
const uint32_t UNTOUCHED_MAX_VAL = 10; /* Suitable for 56K pull-down resistor */
const uint32_t CALI_TIMES = 4;
static volatile uint32_t blinking_until;
extern uint32_t _stack_boundary;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_RNG_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM6_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void MX_USB_DEVICE_Init(){}

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
  if (sum == UNTOUCHED_MAX_VAL * CALI_TIMES){
    DBG_MSG("max limit exceeded, discard...\n");
    return;
  }

  touch_threshold = sum / CALI_TIMES * 2;
  DBG_MSG("touch_threshold %u\n", touch_threshold);
}

GPIO_PinState GPIO_Touched(void) {
  LL_GPIO_SetPinMode(TOUCH_GPIO_Port, TOUCH_Pin, GPIO_MODE_OUTPUT_PP);
  LL_GPIO_SetOutputPin(TOUCH_GPIO_Port, TOUCH_Pin);
  for (int i = 0; i < 100; ++i)
    asm volatile("nop");
  uint32_t counter = 0;
  LL_GPIO_SetPinMode(TOUCH_GPIO_Port, TOUCH_Pin, GPIO_MODE_INPUT);
  __disable_irq();
  while ((LL_GPIO_ReadInputPort(TOUCH_GPIO_Port) & TOUCH_Pin)/*  && counter <= touch_threshold */)
    ++counter;
  __enable_irq();
  if (counter > measure_touch) measure_touch = counter;
  return counter > touch_threshold ? GPIO_PIN_SET : GPIO_PIN_RESET;
}
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if(htim == &htim6) {
    static uint32_t testcnt = 0, deassert_at = ~0u;
    if (testcnt % 150 == 0) {
      CCID_TimeExtensionLoop();
    }
    if (HAL_GetTick() > blinking_until) {
      HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
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
// override the function defined in rand.c
uint32_t random32(void) {
  uint32_t v;
  while (HAL_RNG_GenerateRandomNumber(&hrng, &v) != HAL_OK);
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

__attribute__((always_inline)) unsigned svc_try_lock(lock_addr) {
  register unsigned r0 asm("r0") = lock_addr;
  __asm volatile("SVC #1" : "=r"(r0) : "r"(r0));
  return r0; // value returned from SVC
}
uint32_t test_lock;
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */
  

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  SetupMPU();
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_RNG_Init();
  MX_SPI1_Init();
  MX_USART2_UART_Init();
  MX_USB_DEVICE_Init();
  MX_TIM6_Init();
  /* USER CODE BEGIN 2 */
  DBG_MSG("test_lock=%d\n", test_lock);
  DBG_MSG("svc_try_lock()=%d\n", svc_try_lock(&test_lock));
  DBG_MSG("test_lock=%d\n", test_lock);
  DBG_MSG("svc_try_lock()=%d\n", svc_try_lock(&test_lock));

  DBG_MSG("Init FS\n");
  littlefs_init();
  DBG_MSG("Init applets\r\n");
  openpgp_install(0);
  piv_install(0);
  oath_install(0);
  ctap_install(0);
  admin_install();
  DBG_MSG("Init USB\r\n");
  usb_device_init();
  DBG_MSG("Main Loop\r\n");
  HAL_TIM_Base_Start_IT(&htim6);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  for (uint32_t i = 0; ;i++)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    if((i & ((1<<23)-1)) == 0) {
      DBG_MSG("Touch calibrating...\r\n");
      GPIO_Touch_Calibrate();
    }
    device_loop();
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
  RCC_CRSInitTypeDef RCC_CRSInitStruct = {0};

  /** Initializes the CPU, AHB and APB busses clocks 
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48|RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 40;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB busses clocks 
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART2|RCC_PERIPHCLK_USB
                              |RCC_PERIPHCLK_RNG;
  PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_HSI48;
  PeriphClkInit.RngClockSelection = RCC_RNGCLKSOURCE_HSI48;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure the main internal regulator output voltage 
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }
  /** Enable the SYSCFG APB clock 
  */
  __HAL_RCC_CRS_CLK_ENABLE();
  /** Configures CRS 
  */
  RCC_CRSInitStruct.Prescaler = RCC_CRS_SYNC_DIV1;
  RCC_CRSInitStruct.Source = RCC_CRS_SYNC_SOURCE_USB;
  RCC_CRSInitStruct.Polarity = RCC_CRS_SYNC_POLARITY_RISING;
  RCC_CRSInitStruct.ReloadValue = __HAL_RCC_CRS_RELOADVALUE_CALCULATE(48000000,1000);
  RCC_CRSInitStruct.ErrorLimitValue = 34;
  RCC_CRSInitStruct.HSI48CalibrationValue = 32;

  HAL_RCCEx_CRSConfig(&RCC_CRSInitStruct);
}

/**
  * @brief RNG Initialization Function
  * @param None
  * @retval None
  */
static void MX_RNG_Init(void)
{

  /* USER CODE BEGIN RNG_Init 0 */

  /* USER CODE END RNG_Init 0 */

  /* USER CODE BEGIN RNG_Init 1 */

  /* USER CODE END RNG_Init 1 */
  hrng.Instance = RNG;
  if (HAL_RNG_Init(&hrng) != HAL_OK)
  {
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
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief TIM6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM6_Init(void)
{

  /* USER CODE BEGIN TIM6_Init 0 */

  /* USER CODE END TIM6_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM6_Init 1 */

  /* USER CODE END TIM6_Init 1 */
  htim6.Instance = TIM6;
  htim6.Init.Prescaler = 8000;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6.Init.Period = 9;
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM6_Init 2 */

  /* USER CODE END TIM6_Init 2 */

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
  huart2.Init.Mode = UART_MODE_TX;
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
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
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
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
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

}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  ERR_MSG("in");
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(char *file, uint32_t line)
{ 
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
