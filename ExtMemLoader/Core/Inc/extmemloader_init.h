/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    extmemloader_init.h
  * @author  MCD Application Team
  * @brief   Header file of Loader_Src.c
  *
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
#ifndef EXTMEMLOADER_INIT_H
#define EXTMEMLOADER_INIT_H

/* Includes ------------------------------------------------------------------*/
#include "stm32h7rsxx_hal.h"

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

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

uint32_t extmemloader_Init(void);
void Error_Handler(void);

/* Private defines -----------------------------------------------------------*/

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#define LCD_SPI_CS_Pin GPIO_PIN_5
#define LCD_SPI_CS_GPIO_Port GPIOB
#define TP_IRQ_Pin GPIO_PIN_15
#define TP_IRQ_GPIO_Port GPIOE
#define LCD_SPI_MOSI_Pin GPIO_PIN_9
#define LCD_SPI_MOSI_GPIO_Port GPIOF
#define LCD_SPI_SCK_Pin GPIO_PIN_10
#define LCD_SPI_SCK_GPIO_Port GPIOF
#define TP_RST_Pin GPIO_PIN_12
#define TP_RST_GPIO_Port GPION
#define LED_RED_Pin GPIO_PIN_1
#define LED_RED_GPIO_Port GPIOO
#define LED_BLUE_Pin GPIO_PIN_5
#define LED_BLUE_GPIO_Port GPIOO
#endif /* EXTMEMLOADER_INIT_H */
