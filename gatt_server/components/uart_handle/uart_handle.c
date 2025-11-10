#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include "uart_handle.h"


static QueueHandle_t uart2_queue;
extern EventGroupHandle_t EventGroupHandle;
uint8_t uart_rcv_done = 0;

uint32_t last_tick = 0;
uint32_t g_sys_tick = 0;

float tem;
float hum;
int adc_val;
int device_mode;
int device_state;
int device_hum_threshold;


static void uart_handle_event() 
{
        uart_event_t event;
    uint8_t* dtmp = (uint8_t*) malloc(RD_BUF_SIZE);
    for (;;) {
        if(xQueueReceive(uart2_queue, (void * )&event, (TickType_t)portMAX_DELAY)) {
            bzero(dtmp, RD_BUF_SIZE);
            ESP_LOGI("UART2", "uart[%d] event:", UART_NUM_2);
            switch(event.type) {
                //Event of UART receving data
                /*We'd better handler data event fast, there would be much more data events than
                other types of events. If we take too much time on data event, the queue might
                be full.*/
                case UART_DATA:
                    uart_read_bytes(UART_NUM_2, dtmp, event.size, portMAX_DELAY);
                    ESP_LOGI("UART2", "[UART DATA]: %d", event.size);
                    ESP_LOGI("UART2", "[UART DATA]: %.*s", event.size, dtmp);
                    if (strncmp((char*)dtmp, "s:", 2) == 0) {
                        // Sync request from MCU
                        ESP_LOGI("UART2", "Sync request received from MCU");
                        xEventGroupSetBits(EventGroupHandle, UART_SYNC_EVENT_BIT);
                    } else {
                        sscanf((char*) dtmp,
                                "tem:%f hum:%f mq2:%u mod:%u state:%u ht:%02u",
                                &tem, &hum, &adc_val, &device_mode, &device_state, &device_hum_threshold);
                        ESP_LOGI("UART2", "tem:%1f hum:%f mq2:%u mod:%u state:%u ht:%02u", tem, hum, adc_val, device_mode, device_state, device_hum_threshold);
                        xEventGroupSetBits(EventGroupHandle, UART_EVENT_BIT);
                    }
                    break;
                //Event of HW FIFO overflow detected
                case UART_FIFO_OVF:
                    ESP_LOGI("UART2", "hw fifo overflow");
                    // If fifo overflow happened, you should consider adding flow control for your application.
                    // The ISR has already reset the rx FIFO,
                    // As an example, we directly flush the rx buffer here in order to read more data.
                    uart_flush_input(UART_NUM_2);
                    xQueueReset(uart2_queue);
                    break;
                //Event of UART ring buffer full
                case UART_BUFFER_FULL:
                    ESP_LOGI("UART2", "ring buffer full");
                    // If buffer full happened, you should consider increasing your buffer size
                    // As an example, we directly flush the rx buffer here in order to read more data.
                    uart_flush_input(UART_NUM_2);
                    xQueueReset(uart2_queue);
                    break;
                //Event of UART RX break detected
                case UART_BREAK:
                    ESP_LOGI("UART2", "uart rx break");
                    break;
                //Event of UART parity check error
                case UART_PARITY_ERR:
                    ESP_LOGI("UART2", "uart parity error");
                    break;
                //Event of UART frame error
                case UART_FRAME_ERR:
                    ESP_LOGI("UART2", "uart frame error");
                    break;
                //Others
                default:
                    ESP_LOGI("UART2", "uart event type: %d", event.type);
                    break;
            }
        }
    }
}

void uart2_init()
{
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    uart_driver_install(UART_NUM_2, BUF_SIZE * 2, BUF_SIZE * 2, 20, &uart2_queue, 0);
    uart_param_config(UART_NUM_2, &uart_config);                    // connect stm32
    uart_set_pin(UART_NUM_2, TXD2, RXD2, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_pattern_queue_reset(UART_NUM_2, 20);
    xTaskCreate(uart_handle_event, "uart_event_task", 2048, NULL, 2, NULL);
}

void uart_GetData(int16_t *tempx10, int16_t *humix10, uint16_t *co2, uint16_t *mode, uint16_t *state, uint16_t *hum_threshold)
{
    *tempx10 = (int16_t) (tem * 10);
    *humix10 = (uint16_t) (hum * 10);
    *co2 = (uint16_t) adc_val;
    *mode = (uint16_t) device_mode;
    *state = (uint16_t) device_state;
    *hum_threshold = (uint16_t) device_hum_threshold;
}

void uart_SendData(uint16_t mode, uint16_t sampling_interval, uint16_t hum_threshold)
{
    uint8_t tx_data[6];
    tx_data[0] = 'c';
    tx_data[1] = ':';
    tx_data[2] = (uint8_t)(mode);
    tx_data[3] = (uint8_t)(sampling_interval >> 8);
    tx_data[4] = (uint8_t)(sampling_interval & 0xFF);
    tx_data[5] = (uint8_t)(hum_threshold);
    uart_write_bytes(UART_NUM_2, (const char*) tx_data, 6);
    ESP_LOGI("UART2", "state = %d", sampling_interval);
    ESP_LOG_BUFFER_HEX("UART2", tx_data, 6);
}