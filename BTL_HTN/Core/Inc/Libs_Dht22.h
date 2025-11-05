/*
 * Libs_Dht22.h
 *
 *  Created on: Nov 2, 2025
 *      Author: Truong
 */

#ifndef INC_LIBS_DHT22_H_
#define INC_LIBS_DHT22_H_

#include "stm32f4xx_hal.h"

#define DHT22_PIN			GPIO_PIN_2
#define DHT22_PORT			GPIOB
#define DHT_TIMEOUT			2
#define DHT_ERROR_VAL		(int8_t) -1

typedef struct {
	uint8_t hum;
	uint8_t hum0;
	uint8_t tem;
	uint8_t tem0;
	uint8_t check_sum;
} dht22;

extern void delay_us(uint32_t us);

void Dht22_Init();

void Dht22_GetValue(float *tem, float *hum);

#endif /* INC_LIBS_DHT22_H_ */
