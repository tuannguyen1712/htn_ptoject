/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
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
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stdio.h"
#include "stdint.h"
#include "string.h"
#include "stdlib.h"
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define RELAY_Pin GPIO_PIN_13
#define RELAY_GPIO_Port GPIOC
#define SET_AUTO_MODE_Pin GPIO_PIN_7
#define SET_AUTO_MODE_GPIO_Port GPIOA
#define SET_AUTO_MODE_EXTI_IRQn EXTI9_5_IRQn
#define MANUAL_DOWN_Pin GPIO_PIN_0
#define MANUAL_DOWN_GPIO_Port GPIOB
#define MANUAL_DOWN_EXTI_IRQn EXTI0_IRQn
#define MANUAL_UP_Pin GPIO_PIN_1
#define MANUAL_UP_GPIO_Port GPIOB
#define MANUAL_UP_EXTI_IRQn EXTI1_IRQn
#define DHT_Pin GPIO_PIN_2
#define DHT_GPIO_Port GPIOB
#define ON_OFF_MANUAL_Pin GPIO_PIN_10
#define ON_OFF_MANUAL_GPIO_Port GPIOB
#define ON_OFF_MANUAL_EXTI_IRQn EXTI15_10_IRQn

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
