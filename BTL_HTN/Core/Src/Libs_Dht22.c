/*
 * Libs_Dht22.c
 *
 *  Created on: Nov 2, 2025
 *      Author: Truong
 */

#include "Libs_Dht22.h"

#define DHT22_PINMODE_INPUT 				0
#define DHT22_PINMODE_OUTPUT				1

dht22 Dht22_Instance;

static inline void Dht22_Gpio_Setmode(uint8_t mode) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };
	if (mode) {	/* output mode */
		GPIO_InitStruct.Pin = DHT22_PIN;
		GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
		HAL_GPIO_Init(DHT22_PORT, &GPIO_InitStruct);
	} else { /* input mode */
		GPIO_InitStruct.Pin = DHT22_PIN;
		GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		HAL_GPIO_Init(DHT22_PORT, &GPIO_InitStruct);
	}
}

void Dht22_Init() {
	Dht22_Gpio_Setmode(DHT22_PINMODE_OUTPUT);
	HAL_GPIO_WritePin(DHT22_PORT, DHT22_PIN, 1);
}

void Dht22_GetValue(float *tem, float *hum) {
	uint8_t bytes[5];

	HAL_GPIO_WritePin(DHT22_PORT, DHT22_PIN, 0);
	delay_us(1000);
	HAL_GPIO_WritePin(DHT22_PORT, DHT22_PIN, 1);
	delay_us(20);

	Dht22_Gpio_Setmode(DHT22_PINMODE_INPUT);

	delay_us(120);

	for (int j = 0; j < 5; j++) {
		uint8_t result = 0;
		for (int i = 0; i < 8; i++) {                       /* for each bit in each byte (8 total) */
			uint32_t time_out = HAL_GetTick();				/* wait input become high */ 
			while (!HAL_GPIO_ReadPin(DHT22_PORT, DHT22_PIN)) {
				if (HAL_GetTick() - time_out >= DHT_TIMEOUT) {
					Dht22_Init();
					return;
				}
			}
			delay_us(30);
			if (HAL_GPIO_ReadPin(DHT22_PORT, DHT22_PIN)) {
				// result |= (1 << (7-i));
				result = (result << 1) | 0x01;              /* if input still high after 30us -> bit 1 */
            } else {
				// else bit 0
				result = result << 1;
            }
			time_out = HAL_GetTick();
			while (HAL_GPIO_ReadPin(DHT22_PORT, DHT22_PIN)) {    /* wait Dht22 to transmit bit 1 complete */
				if (HAL_GetTick() - time_out >= DHT_TIMEOUT) {
					Dht22_Init();
					return;
				}
			}
		}
		bytes[j] = result;
	}

	Dht22_Init();

	Dht22_Instance.hum = bytes[0];
	Dht22_Instance.hum0 = bytes[1];
	Dht22_Instance.tem = bytes[2];
	Dht22_Instance.tem0 = bytes[3];
	Dht22_Instance.check_sum = bytes[4];

	uint16_t check = (uint16_t) bytes[0] + (uint16_t) bytes[1] + (uint16_t) bytes[2] + (uint16_t) bytes[3];
	if ((check % 256) != bytes[4]) {
		Dht22_Init();
		return;
	}

	uint16_t t = ((uint16_t) Dht22_Instance.tem << 8) | ((uint16_t) Dht22_Instance.tem0);
	uint16_t h = ((uint16_t) Dht22_Instance.hum << 8) | ((uint16_t) Dht22_Instance.hum0);

	*tem = (float) t / 10;
	*hum = (float) h / 10;
}
