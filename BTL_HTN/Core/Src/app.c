#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "stm32f4xx_hal.h"
#include "app.h"

extern UART_HandleTypeDef huart2;
extern ADC_HandleTypeDef hadc1;
extern TIM_HandleTypeDef htim2;

#define DEFAULT_SAMPLING_INTERVAL_MS	10000

#define DHT22_PIN			GPIO_PIN_2
#define DHT22_PORT			GPIOB
#define TOUTCH_SENSOR_PIN	GPIO_PIN_10
#define TOUTCH_SENSOR_PORT	GPIOB
#define RELAY_PIN			GPIO_PIN_13
#define RELAY_PORT			GPIOC

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
uint32_t co2_ppm;

uint32_t sampling_interval = DEFAULT_SAMPLING_INTERVAL_MS;

static inline void Mq135_GetCo2Ppm(float tem, float hum) {
	getADC_value();
	co2_ppm = (uint32_t) MQ135_getCorrectCO2(tem, hum, adc_val);
}

void Air_Monitor_Main() {
	HAL_UART_Receive_IT(&huart2, &uart_chr, sizeof(uart_chr));
	HAL_TIM_Base_Start(&htim2);
	init_dht22();
	clear_uart_buf();
	/* Print init ssd1306 screen */
	HAL_Delay(100);
	Libs_Ssd1306_Init();
	Libs_Ssd1306_Fill(SSD1306_WHITE);
	Libs_Ssd1306_UpdateScreen();
	HAL_Delay(1000);

	while (1) {
		Handle_Command();
		dht22_GetValue(&dht);
		Mq135_GetCo2Ppm(tem, hum);
		misting_control();
		Update_Screen();
		HAL_Delay(sampling_interval);
		Response();
	}
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	UNUSED(GPIO_Pin);
	if (GPIO_Pin == TOUTCH_SENSOR_PIN) {
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

	if (tem != -1) {
		sprintf((char*) tx,
				"tem:%.1f hum:%.1f mq2:%lu mod:%u state:%u ht:%02u\n", tem,
				hum,
				sampling_interval, device_mode, device_state, device_threshold);
		HAL_UART_Transmit(&huart2, tx, strlen((char*) tx), 200);

	}
}

void misting_control(void) {
	if ((int) hum != DHT_ERROR_VAL && device_mode == DEVICE_MODE_AUTO) {
		if ((int) hum <= device_threshold) {
			HAL_GPIO_WritePin(RELAY_PORT, RELAY_PIN, DEVICE_STATE_ON);
			device_state = DEVICE_STATE_ON;
		} else {
			HAL_GPIO_WritePin(RELAY_PORT, RELAY_PIN, DEVICE_STATE_OFF);
			device_state = DEVICE_STATE_OFF;
		}
	} else if (device_done && device_mode == DEVICE_MODE_MANUAL) {
		HAL_GPIO_WritePin(RELAY_PORT, RELAY_PIN, device_state);
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
	if (mode) {							// output
		GPIO_InitStruct.Pin = DHT22_PIN;
		GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
		HAL_GPIO_Init(DHT22_PORT, &GPIO_InitStruct);
	} else {							// input
		GPIO_InitStruct.Pin = DHT22_PIN;
		GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		HAL_GPIO_Init(DHT22_PORT, &GPIO_InitStruct);
	}
}

void delay_us(uint32_t us) {
	__HAL_TIM_SetCounter(&htim2, 0);
	while (__HAL_TIM_GetCounter(&htim2) < us);
}

void init_dht22() {
	gpio_set_mode(OUTPUT);
	HAL_GPIO_WritePin(DHT22_PORT, DHT22_PIN, 1);
}

void dht22_GetValue(dht22 *dht) {
	uint8_t bytes[5];

	HAL_GPIO_WritePin(DHT22_PORT, DHT22_PIN, 0);
	delay_us(1000);
	HAL_GPIO_WritePin(DHT22_PORT, DHT22_PIN, 1);
	delay_us(20);

	gpio_set_mode(INPUT);

	delay_us(120);

	for (int j = 0; j < 5; j++) {
		uint8_t result = 0;
		for (int i = 0; i < 8; i++) { //for each bit in each byte (8 total)
			time_out = HAL_GetTick();				// wait input become high
			while (!HAL_GPIO_ReadPin(DHT22_PORT, DHT22_PIN)) {
				if (HAL_GetTick() - time_out >= DHT_TIMEOUT) {
					init_dht22();
					return;
				}
			}
			delay_us(30);
			if (HAL_GPIO_ReadPin(DHT22_PORT, DHT22_PIN))	// if input still high after 30us -> bit 1
				// result |= (1 << (7-i));
				result = (result << 1) | 0x01;
			else
				// else bit 0
				result = result << 1;
			time_out = HAL_GetTick();
			while (HAL_GPIO_ReadPin(GPIOB, DHT22_PIN)) // wait dht22 transmit bit 1 complete
			{
				if (HAL_GetTick() - time_out >= DHT_TIMEOUT) {
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
		init_dht22();
		return;
	}

	uint16_t t = ((uint16_t) dht->tem << 8) | ((uint16_t) dht->tem0);
	uint16_t h = ((uint16_t) dht->hum << 8) | ((uint16_t) dht->hum0);

	tem = (float) t / 10;
	hum = (float) h / 10;
}

void Handle_Command() {
	if (HAL_GetTick() - uart_last_rcv >= 20) {
		if (uart_buf_cnt == 6 && strncmp((char*) uart_buf, "c:", 2) == 0) {
			device_mode = uart_buf[2];
			sampling_interval = (uint32_t)((((uint32_t)uart_buf[3]) << 8) | (uint32_t)uart_buf[4]) * 1000;
			device_threshold = uart_buf[5];
			clear_uart_buf();
			Response();
			device_done = 1;
		}
		else {
			clear_uart_buf();
		}
	} else {
		clear_uart_buf();
	}
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
	if (huart->Instance == huart2.Instance) {
		if (uart_buf_cnt < sizeof(uart_buf)) {
			uart_buf[uart_buf_cnt] = uart_chr;
		}
		uart_last_rcv = HAL_GetTick();
		uart_buf_cnt++;
		HAL_UART_Receive_IT(&huart2, &uart_chr, sizeof(uart_chr));
	}
}

void clear_uart_buf() {
	memset(uart_buf, 0, strlen((char*) uart_buf));
	uart_buf_cnt = 0;
}

void Update_Screen() {
	char line[24];

	Libs_Ssd1306_Fill(SSD1306_BLACK);

	sprintf(line, "tem:%2.1f hum:%3.1f", tem, hum);
	Libs_Ssd1306_SetCursor(2, 0);
	Libs_Ssd1306_WriteString(line, Font_6x8, SSD1306_WHITE);

	sprintf((char*) line, "co2:%4ld ppm", co2_ppm);
	Libs_Ssd1306_SetCursor(2, 9);
	Libs_Ssd1306_WriteString(line, Font_6x8, SSD1306_WHITE);

	sprintf((char*) line, "mode:%s", device_mode ? "auto" : "manual");
	Libs_Ssd1306_SetCursor(2, 18);
	Libs_Ssd1306_WriteString(line, Font_6x8, SSD1306_WHITE);

	sprintf((char*) line, "state:%s", device_state ? "on" : "off");
	Libs_Ssd1306_SetCursor(2, 27);
	Libs_Ssd1306_WriteString(line, Font_6x8, SSD1306_WHITE);

	sprintf((char*) line, "hum th:%d %%", device_threshold);
	Libs_Ssd1306_SetCursor(2, 36);
	Libs_Ssd1306_WriteString(line, Font_6x8, SSD1306_WHITE);

	Libs_Ssd1306_UpdateScreen();
}
