/*
 * app.h
 *
 *  Created on: Oct 26, 2025
 *      Author: Truong
 */

#ifndef INC_APP_H_
#define INC_APP_H_

#include "Libs_I2c.h"
#include "Libs_Ssd1306.h"
#include "Libs_Mq135.h"

typedef struct {
	uint8_t hum;
	uint8_t hum0;
	uint8_t tem;
	uint8_t tem0;
	uint8_t check_sum;
} dht22;

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
void Air_Monitor_Main();

void Update_Screen();

#endif /* INC_APP_H_ */
