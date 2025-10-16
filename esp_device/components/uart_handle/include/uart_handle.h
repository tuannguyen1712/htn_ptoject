#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/timers.h"
#include "esp_timer.h"
#include <time.h>
#include <sys/time.h>

#define TXD2 32             // connect mcu
#define RXD2 33

#define BUF_SIZE (1024)
#define RD_BUF_SIZE (BUF_SIZE)

#define UART_EVENT_BIT      BIT0
 
// static void uart2_event_task(void *pvParameters);
void uart2_init();
void uart_handle_event();
void uart_GetData(int16_t *tempx10, int16_t *humix10, uint16_t *co2, uint16_t *mode, uint16_t *state);
void uart_SendData(uint16_t mode, uint16_t state);