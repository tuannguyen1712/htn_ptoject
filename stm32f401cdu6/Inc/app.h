/*
 * app.h
 *
 *  Created on: Oct 26, 2025
 *      Author: Truong
 */

#ifndef INC_APP_H_
#define INC_APP_H_

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

#endif /* INC_APP_H_ */
