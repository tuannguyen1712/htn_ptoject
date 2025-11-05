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

void Misting_Control(void);
void Send_Data(void);
void Handle_Command(void);
void Measure_And_Control(void);
float Calculate_Suitable_Humidity(float temperature_c);

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);
void Clear_Uart_RxBuffer();

void delay_us(uint32_t us);
void Update_Screen();

void Air_Monitor_Main();

#endif /* INC_APP_H_ */
