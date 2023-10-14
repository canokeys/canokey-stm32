/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32l4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "common.h"
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */
#define DBG_UART huart2
#define FM_SPI hspi1
/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
extern const char *fw_git_version;
void SystemClock_CustomConfig(bool nfc_low_power, bool pll_reconfig);
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define FM_IRQN_Pin GPIO_PIN_0
#define FM_IRQN_GPIO_Port GPIOA
#define FM_IRQN_EXTI_IRQn EXTI0_IRQn
#define FM_SSN_Pin GPIO_PIN_4
#define FM_SSN_GPIO_Port GPIOA
#define LED_Pin GPIO_PIN_3
#define LED_GPIO_Port GPIOB
#define TOUCH_Pin GPIO_PIN_3
#define TOUCH_GPIO_Port GPIOH
/* USER CODE BEGIN Private defines */
#define HW_CFG_Port GPIOB
#define HW_CFG0_Pin GPIO_PIN_4
#define HW_CFG1_Pin GPIO_PIN_5
#define HW_CFG2_Pin GPIO_PIN_6
#define HW_CFG3_Pin GPIO_PIN_7
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
