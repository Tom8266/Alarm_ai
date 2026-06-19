/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.h
  * @brief   This file contains all the function prototypes for
  *          the gpio.c file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#ifndef __GPIO_H__
#define __GPIO_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

#include "main.h"

/* USER CODE END Includes */

/* USER CODE BEGIN Private defines */

#define BUTTON_UP_Pin       GPIO_PIN_0
#define BUTTON_UP_GPIO_Port GPIOA
#define BUTTON_DOWN_Pin     GPIO_PIN_2
#define BUTTON_DOWN_GPIO_Port GPIOA
#define BUTTON_LEFT_Pin     GPIO_PIN_4
#define BUTTON_LEFT_GPIO_Port GPIOA
#define BUTTON_RIGHT_Pin    GPIO_PIN_6
#define BUTTON_RIGHT_GPIO_Port GPIOA
#define ALARM_INT_Pin       GPIO_PIN_10
#define ALARM_INT_GPIO_Port GPIOB

/* USER CODE END Private defines */

void MX_GPIO_Init(void);

/* USER CODE BEGIN Prototypes */

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif
#endif /*__ GPIO_H__ */

