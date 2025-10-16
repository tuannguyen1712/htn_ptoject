/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct {
	uint8_t hum;
	uint8_t hum0;
	uint8_t tem;
	uint8_t tem0;
	uint8_t check_sum;
} dht22;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define UART_TIMEOUT		2				// 2 ms
#define DHT_TIMEOUT			2
#define INPUT 				0
#define OUTPUT				1
#define DURATION			30 * 1000
#define SECTOR_USE			41
#define RETRY				5

#define DHT_ERROR_VAL		(int8_t) -1

#define DEVICE_STATE_ON		1
#define DEVICE_STATE_OFF 	0
#define DEVICE_MODE_AUTO	1
#define DEVICE_MODE_MANUAL	0

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
// UART
uint8_t uart_buf[100];
uint8_t tx[1024];
uint8_t uart_buf_cnt = 0;
uint8_t uart_chr;
uint8_t uart_last_rcv = 0;

// dht22
dht22 dht;
float tem = 0;
float hum = 0;
uint32_t time_out = 0;
uint8_t dht_err = 0;

// misting control
uint8_t device_threshold = 50;
uint8_t device_state = 0;
uint8_t device_mode = 0;
uint8_t device_done = 0;

// button
uint8_t button_tick = 0;

// mq135 sensor
uint32_t adc_val = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM2_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */
void gpio_set_mode(uint8_t mode);
void delay_us(uint32_t us);
void init_dht22();
void dht22_GetValue(dht22 *dht);

void getADC_value(void);
void misting_control(void);
void Response(void);
void Handle_Command(void);
void clear_uart_buf();

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
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

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_TIM3_Init();
  MX_TIM2_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
	HAL_UART_Receive_IT(&huart1, &uart_chr, sizeof(uart_chr));
	HAL_TIM_Base_Start(&htim2);
	init_dht22();
	HAL_Delay(1000);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
		Handle_Command();
    dht22_GetValue(&dht);
		misting_control();
		HAL_Delay(1000);
		Response();
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 15;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 4294967295;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 799;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 19999;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PC15 */
  GPIO_InitStruct.Pin = GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PA3 */
  GPIO_InitStruct.Pin = GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PB9 */
  GPIO_InitStruct.Pin = GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 7, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	UNUSED(GPIO_Pin);
	if (GPIO_Pin == GPIO_PIN_15) {
		if (HAL_GetTick() - button_tick >= 300) {
			device_done = 1;
			device_mode = DEVICE_MODE_MANUAL;
			device_state ^= 1;
			button_tick = HAL_GetTick();
		}
	}
}

void Response() {
	dht22_GetValue(&dht);
//	getADC_value();
	if (tem != -1) {
		sprintf((char*) tx, "tem:%.1f hum:%.1f mq2:%lu mod:%u state:%u",
				tem, hum, adc_val, device_mode, device_state);
		HAL_UART_Transmit(&huart1, tx, strlen((char*) tx), 200);

	}
}

void misting_control(void) {
	if ((int) hum != DHT_ERROR_VAL && device_mode == DEVICE_MODE_AUTO) {
		if ((int) hum <= device_threshold) {
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, DEVICE_STATE_ON);
			device_state = DEVICE_STATE_ON;
		} else {
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, DEVICE_STATE_OFF);
			device_state = DEVICE_STATE_ON;
		}
	} else if (device_done) {
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, device_state);
	}
}

void getADC_value() {
	HAL_ADC_Start(&hadc1);
	HAL_ADC_PollForConversion(&hadc1, 1000);
	adc_val = HAL_ADC_GetValue(&hadc1);
	HAL_ADC_Stop(&hadc1);
}

void gpio_set_mode(uint8_t mode) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };
	if (mode) {						// output
		GPIO_InitStruct.Pin = GPIO_PIN_9;
		GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
		HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
	} else {							// input
		GPIO_InitStruct.Pin = GPIO_PIN_9;
		GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
	}
}

void delay_us(uint32_t us) {
	__HAL_TIM_SetCounter(&htim2, 0);
	while (__HAL_TIM_GetCounter(&htim2) < us)
		;
}

void init_dht22() {
	gpio_set_mode(OUTPUT);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, 1);
}

void dht22_GetValue(dht22 *dht) {
	uint8_t bytes[5];

	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, 0);
	delay_us(1000);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, 1);
	delay_us(20);

	gpio_set_mode(INPUT);

	delay_us(120);

	for (int j = 0; j < 5; j++) {
		uint8_t result = 0;
		for (int i = 0; i < 8; i++) { //for each bit in each byte (8 total)
			time_out = HAL_GetTick();				// wait input become high
			while (!HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_9)) {
				if (HAL_GetTick() - time_out >= DHT_TIMEOUT) {
					// if (isDebug) {
					// 	sprintf((char*) tx, "Err3");
					// 	HAL_UART_Transmit(&huart1, tx, strlen((char*) tx), 200);
					// }
					tem = -1;
					hum = -1;
					init_dht22();
					return;
				}
			}
			delay_us(30);
			if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_9))// if input still high after 30us -> bit 1
				//result |= (1 << (7-i));
				result = (result << 1) | 0x01;
			else
				// else bit 0
				result = result << 1;
			time_out = HAL_GetTick();
			while (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_9))	// wait dht22 transmit bit 1 complete
			{
				if (HAL_GetTick() - time_out >= DHT_TIMEOUT) {
					// if (isDebug) {
					// 	sprintf((char*) tx, "Err3");
					// 	HAL_UART_Transmit(&huart1, tx, strlen((char*) tx), 200);
					// }
					tem = -1;
					hum = -1;
					init_dht22();
					return;
				}
			}
		}
		bytes[j] = result;
	}

	init_dht22();

	dht->hum = bytes[0];
	dht->hum0 = bytes[1];
	dht->tem = bytes[2];
	dht->tem0 = bytes[3];
	dht->check_sum = bytes[4];

	uint16_t check = (uint16_t) bytes[0] + (uint16_t) bytes[1]
			+ (uint16_t) bytes[2] + (uint16_t) bytes[3];
	if ((check % 256) != bytes[4]) {
		// if (isDebug) {
		// 	sprintf((char*) tx, "Err4");
		// 	HAL_UART_Transmit(&huart1, tx, strlen((char*) tx), 200);
		// }
		tem = -1;
		hum = -1;
		init_dht22();
		return;							// incorrect checksum
	}

	uint16_t t = ((uint16_t) dht->tem << 8) | ((uint16_t) dht->tem0);
	uint16_t h = ((uint16_t) dht->hum << 8) | ((uint16_t) dht->hum0);

	tem = (float) t / 10;
	hum = (float) h / 10;
}

void Handle_Command() {
	if (HAL_GetTick() - uart_last_rcv >= (uint32_t) 20
			&& strlen((char*) uart_buf) >= 3) {
		if (strlen((char*) uart_buf) == 4
				&& strncmp((char*) uart_buf, "c:", 2) == 0) {
			int temp = atoi((char*) uart_buf + 2);
			device_mode = temp / 10;
			device_state = atoi((char*) uart_buf + 3);
			clear_uart_buf();
			Response();
			device_done = 1;
//			sprintf((char*) tx, "%d %d\n", device_mode, device_state);
//			HAL_UART_Transmit(&huart1, tx, strlen((char*) tx), 200);
		}
	} else {
		clear_uart_buf();
//			sprintf((char*) tx, "Invalid Command!\n");
//			HAL_UART_Transmit(&huart1, tx, strlen((char*) tx), 200);
	}
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
	if (huart->Instance == huart1.Instance) {
		if (uart_buf_cnt < sizeof(uart_buf)) {
			uart_buf[uart_buf_cnt] = uart_chr;
		}
		uart_last_rcv = HAL_GetTick();
		uart_buf_cnt++;
		HAL_UART_Receive_IT(&huart1, &uart_chr, sizeof(uart_chr));
	}
}
void clear_uart_buf() {
	memset(uart_buf, 0, strlen((char*) uart_buf));
	uart_buf_cnt = 0;
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
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
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
