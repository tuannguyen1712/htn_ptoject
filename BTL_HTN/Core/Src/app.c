#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "stm32f4xx_hal.h"
#include "Libs_Dht22.h"
#include "app.h"

#define DEFAULT_SAMPLING_INTERVAL_MS	10000
#define UART_TIMEOUT_MS					20
#define UART_RECV_COMMAND_LENGTH		6
#define UART_RECV_LENGTH_MAX 			10
#define UART_TRANS_LENGTH_MAX			256

#define ON_OFF_MANUAL_PIN				GPIO_PIN_10
#define ON_OFF_MANUAL_PORT				GPIOB
#define MANUAL_HUMIDITY_UP_PIN			GPIO_PIN_1
#define MANUAL_HUMIDITY_DOWN_PIN		GPIO_PIN_0		
#define SET_AUTO_MODE_PIN				GPIO_PIN_7	
#define RELAY_PIN						GPIO_PIN_13
#define RELAY_PORT						GPIOC

#define DEVICE_FORCING_STATUS_ON		1
#define DEVICE_FORCING_STATUS_OFF		0
#define MIST_STATE_ON					1
#define MIST_STATE_OFF					0
#define DEVICE_MODE_AUTO				1
#define DEVICE_MODE_MANUAL				0

#define DEVICE_DEFAULT_HUMIDITY_THRESHOLD		55
#define DEVICE_DEFAULT_FORCING_STATUS			DEVICE_FORCING_STATUS_OFF
#define DEVICE_DEFAULT_MODE						DEVICE_MODE_AUTO

#define SYSTEM_NORMAL_TASK_SEQUENCE_INTERVAL_SECOND 			5

#define MIST_ON_MAX_DURATION_MS   		10000
#define MIST_MIN_OFF_INTERVAL_MS   		5000

#define FIREBASE_SYNC_TIMEOUT			30000

#define HUMIDITY_JUMP_THRESHOLD     	0.3f
#define TEMP_JUMP_THRESHOLD     		0.2f
#define CO2_JUMP_THRESHOLD				10

extern UART_HandleTypeDef huart2;
extern ADC_HandleTypeDef hadc1;
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim4;

uint8_t uart_rx_buf[UART_RECV_LENGTH_MAX];
uint8_t uart_tx_buf[UART_TRANS_LENGTH_MAX];
uint8_t uart_rx_buf_cnt = 0;
uint32_t uart_last_rcv_time_ms = 0;
uint8_t uart_chr;

/* Time between two samplings */
uint32_t sampling_interval = DEFAULT_SAMPLING_INTERVAL_MS;
uint32_t last_data_sendtime_ms = 0;

/* Misting control variable */ 
uint8_t forcing_status = DEVICE_DEFAULT_FORCING_STATUS;
uint8_t current_hum_threshold = DEVICE_DEFAULT_HUMIDITY_THRESHOLD;
uint8_t current_state = MIST_STATE_OFF;
uint8_t current_mode = DEVICE_DEFAULT_MODE;

/* Sensor data */
uint32_t co2_ppm;
float tem = 0;
float hum = 0;

/* Mist state control variable */
uint32_t mist_on_start_time = 0;
uint32_t mist_off_time = 0;

/* Sync from firebase completed! */
volatile uint8_t sync_complete = 0;

/* Static function prototype */
static inline void Mq135_GetCo2Ppm(float tem, float hum);
static inline void Misting_Control_ON();
static inline void Misting_Control_OFF();
static inline void Uart_Start_Check_Message_Timer();
static inline void Uart_Reload_Check_Message_Timer();
static inline void Uart_Stop_Check_Message_Timer();

void UART_Log(const char *fmt, ...) {
    char buf[200];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    HAL_UART_Transmit(&huart2, (uint8_t*)buf, strlen(buf), HAL_MAX_DELAY);
}

/* Main function */
void Air_Monitor_Main() {
	HAL_UART_Receive_IT(&huart2, &uart_chr, sizeof(uart_chr));
	HAL_TIM_Base_Start(&htim2);
	Dht22_Init();
	Clear_Uart_RxBuffer();
	/* Print init ssd1306 screen */
	HAL_Delay(100);
	Libs_Ssd1306_Init();
	Libs_Ssd1306_Fill(SSD1306_WHITE);
	Libs_Ssd1306_UpdateScreen();
	HAL_Delay(1000);
	
	Send_Firebase_Sync_Request();
	HAL_Delay(300);
	/* Wait until data sync from firebase, if timeout, use default value */
	uint32_t sync_time = HAL_GetTick();
	while (sync_complete == 0) {
		if (HAL_GetTick() - sync_time > FIREBASE_SYNC_TIMEOUT) {
			break;
		}
		HAL_Delay(2000);
		Send_Firebase_Sync_Request();
	}

	HAL_TIM_Base_Start_IT(&htim4);
	while (1) {
		Update_Screen();
	}
}

static inline void Mq135_GetCo2Ppm(float tem, float hum) {
	HAL_ADC_Start(&hadc1);
	HAL_ADC_PollForConversion(&hadc1, 1000);
	uint32_t adc_val = HAL_ADC_GetValue(&hadc1);
	HAL_ADC_Stop(&hadc1);
	co2_ppm = (uint32_t) MQ135_getCorrectCO2(tem, hum, adc_val);
}

static inline void Misting_Control_ON(void) {
    uint32_t now = HAL_GetTick();

	/* Maximum time on exceeded */
    if ((current_state == MIST_STATE_ON) && (now - mist_on_start_time >= MIST_ON_MAX_DURATION_MS)) {
        HAL_GPIO_WritePin(RELAY_PORT, RELAY_PIN, GPIO_PIN_RESET);
        current_state = MIST_STATE_OFF;
        mist_off_time = now;
        return;
    }

    /* Minimum off time is reached */
    if ((current_state == MIST_STATE_OFF) && (now - mist_off_time >= MIST_MIN_OFF_INTERVAL_MS)) {
        HAL_GPIO_WritePin(RELAY_PORT, RELAY_PIN, GPIO_PIN_SET);
        current_state = MIST_STATE_ON;
        mist_on_start_time = now;
    }
}

static inline void Misting_Control_OFF(void) {
	HAL_GPIO_WritePin(RELAY_PORT, RELAY_PIN, GPIO_PIN_RESET);
	current_state = MIST_STATE_OFF;
	mist_off_time = HAL_GetTick();
}

static inline void Uart_Start_Check_Message_Timer() {
	__HAL_TIM_CLEAR_FLAG(&htim3, TIM_FLAG_UPDATE);
    __HAL_TIM_SET_COUNTER(&htim3, 0);
    HAL_TIM_Base_Start_IT(&htim3);
}

static inline void Uart_Reload_Check_Message_Timer() {
	__HAL_TIM_SET_COUNTER(&htim3, 0);
}

static inline void Uart_Stop_Check_Message_Timer() {
    HAL_TIM_Base_Stop_IT(&htim3);
    __HAL_TIM_SET_COUNTER(&htim3, 0);
}

void Measure_And_Control(void) {
	Dht22_GetValue(&tem, &hum);
	Mq135_GetCo2Ppm(tem, hum);
	Misting_Control();
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	static uint32_t on_off_manual_button_tick_ms = 0;
	static uint32_t manual_humidity_up_button_tick_ms = 0;
	static uint32_t manual_humidity_down_button_tick_ms = 0;
	static uint32_t set_auto_mode_button_tick_ms = 0;
	UNUSED(GPIO_Pin);
	if (GPIO_Pin == ON_OFF_MANUAL_PIN) {
		if (HAL_GetTick() - on_off_manual_button_tick_ms >= 200) {
			if (forcing_status == DEVICE_FORCING_STATUS_OFF) {
				forcing_status = DEVICE_FORCING_STATUS_ON;
				/* Turn on misting without any added logic */
				HAL_GPIO_WritePin(RELAY_PORT, RELAY_PIN, GPIO_PIN_SET);
				current_state = MIST_STATE_ON;
			} else {
				forcing_status = DEVICE_FORCING_STATUS_OFF;
				Misting_Control_OFF();
			}
			on_off_manual_button_tick_ms = HAL_GetTick();
		}
	} else if (GPIO_Pin == MANUAL_HUMIDITY_UP_PIN) {
		if (HAL_GetTick() - manual_humidity_up_button_tick_ms >= 200) {
			current_mode = DEVICE_MODE_MANUAL;
			current_hum_threshold += 1;
			current_hum_threshold = current_hum_threshold > 30 ? current_hum_threshold : 30;
			current_hum_threshold = current_hum_threshold < 90 ? current_hum_threshold : 90;
			Misting_Control();
			manual_humidity_up_button_tick_ms = HAL_GetTick();
			Send_Data();
		}
	} else if (GPIO_Pin == MANUAL_HUMIDITY_DOWN_PIN) {
		if (HAL_GetTick() - manual_humidity_down_button_tick_ms >= 200) {
			current_mode = DEVICE_MODE_MANUAL;
			current_hum_threshold -= 1;
			current_hum_threshold = current_hum_threshold > 30 ? current_hum_threshold : 30;
			current_hum_threshold = current_hum_threshold < 90 ? current_hum_threshold : 90;
			Misting_Control();
			manual_humidity_down_button_tick_ms = HAL_GetTick();
			Send_Data();
		}	
	} else if (GPIO_Pin == SET_AUTO_MODE_PIN) {
		if (HAL_GetTick() - set_auto_mode_button_tick_ms >= 200) {
			current_mode = DEVICE_MODE_AUTO;
			Misting_Control();
			set_auto_mode_button_tick_ms = HAL_GetTick();
			Send_Data();
		}
	}
}

void Send_Firebase_Sync_Request() {
	uint8_t tx_data[2] = "s:";
	HAL_UART_Transmit(&huart2, tx_data, 2, 200);
}

void Send_Data() {
	if (tem != -1) {
		sprintf((char*) uart_tx_buf, "tem:%.1f hum:%.1f mq2:%lu mod:%u state:%u ht:%02u\n", 
				tem, hum, co2_ppm, current_mode, current_state, current_hum_threshold);
		HAL_UART_Transmit(&huart2, uart_tx_buf, strlen((char*) uart_tx_buf), 200);
	}
}

void Misting_Control(void) {
	if (forcing_status == DEVICE_FORCING_STATUS_OFF) {
		if (current_mode == DEVICE_MODE_AUTO) {
			current_hum_threshold = Calculate_Suitable_Humidity(tem);
		}
		if ((int) hum != DHT_ERROR_VAL) {
			if (hum < current_hum_threshold) {
				Misting_Control_ON();
			} else {
				Misting_Control_OFF();
			}
		}
	}
}

float Calculate_Suitable_Humidity(float temperature_c) {
	const float T_min = 20.0f;
    const float T_max = 32.0f;
    const float RH_min = 40.0f;
    const float RH_max = 60.0f;
	// linear interpolation
	// slope = (RH_min - RH_max) / (T_max - T_min)
	const static float slope = (RH_min - RH_max) / (T_max - T_min);

    if (temperature_c <= T_min) {
        return RH_max;
    }
    if (temperature_c >= T_max) {
        return RH_min;
    }

    float rh = RH_max + slope * (temperature_c - T_min);

    return rh;
}

void delay_us(uint32_t us) {
	__HAL_TIM_SetCounter(&htim2, 0);
	while (__HAL_TIM_GetCounter(&htim2) < us);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
	if (huart->Instance == huart2.Instance) {
		if (uart_rx_buf_cnt < sizeof(uart_rx_buf)) {
			uart_rx_buf[uart_rx_buf_cnt] = uart_chr;
			if (uart_rx_buf_cnt == 0) {
				Uart_Start_Check_Message_Timer();	
			} else {
				Uart_Reload_Check_Message_Timer();
			}
		}
		uart_rx_buf_cnt++;
		HAL_UART_Receive_IT(&huart2, &uart_chr, sizeof(uart_chr));
	}
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
	if (htim->Instance == TIM3) {
		Uart_Stop_Check_Message_Timer();
		Handle_Command();
	} else if (htim->Instance == TIM4) {
		uint32_t cur_tick = HAL_GetTick();
		float old_hum = hum;
		float old_tem = tem;
		uint32_t old_co2_ppm = co2_ppm;
		Measure_And_Control();
		if ((cur_tick - last_data_sendtime_ms >= (sampling_interval - 100)) ||
			(old_hum - hum >= HUMIDITY_JUMP_THRESHOLD) ||
			(old_hum - hum <= -1.0*HUMIDITY_JUMP_THRESHOLD) ||
			(old_tem - tem >= TEMP_JUMP_THRESHOLD) ||
			(old_tem - tem <= -1.0*TEMP_JUMP_THRESHOLD) ||
			(old_co2_ppm - co2_ppm >= CO2_JUMP_THRESHOLD) ||
			(old_co2_ppm - co2_ppm <= -1.0*CO2_JUMP_THRESHOLD)) {
			last_data_sendtime_ms = HAL_GetTick();
			Send_Data();
		}
	}
}

void Handle_Command() {
	if (uart_rx_buf_cnt == UART_RECV_COMMAND_LENGTH && strncmp((char*) uart_rx_buf, "c:", 2) == 0) {
		current_mode = uart_rx_buf[2];
		sampling_interval = (uint32_t)((((uint32_t)uart_rx_buf[3]) << 8) | (uint32_t)uart_rx_buf[4]) * 1000;
		if (current_mode == DEVICE_MODE_MANUAL) {
			current_hum_threshold = uart_rx_buf[5];
		}
		if (sync_complete == 0) {
			sync_complete = 1;
		}
		Measure_And_Control();
	}
	Clear_Uart_RxBuffer();
}

void Clear_Uart_RxBuffer() {
	uart_rx_buf_cnt = 0;
}

void Update_Screen() {
	char line[24];

	Libs_Ssd1306_Fill(SSD1306_BLACK);

	sprintf(line, "tem:%2.1f hum:%3.1f", tem, hum);
	Libs_Ssd1306_SetCursor(2, 18);
	Libs_Ssd1306_WriteString(line, Font_6x8, SSD1306_WHITE);

	sprintf((char*) line, "co2:%4ld ppm", co2_ppm);
	Libs_Ssd1306_SetCursor(2, 27);
	Libs_Ssd1306_WriteString(line, Font_6x8, SSD1306_WHITE);

	sprintf((char*) line, "mode:%s", current_mode ? "auto" : "manual");
	Libs_Ssd1306_SetCursor(2, 36);
	Libs_Ssd1306_WriteString(line, Font_6x8, SSD1306_WHITE);

	sprintf((char*) line, "state:%s%s", current_state ? "on" : "off", forcing_status ? "(!forcing)" : "");
	Libs_Ssd1306_SetCursor(2, 45);
	Libs_Ssd1306_WriteString(line, Font_6x8, SSD1306_WHITE);

	sprintf((char*) line, "hum th:%d %%", current_hum_threshold);
	Libs_Ssd1306_SetCursor(2, 54);
	Libs_Ssd1306_WriteString(line, Font_6x8, SSD1306_WHITE);

	Libs_Ssd1306_UpdateScreen();
}
