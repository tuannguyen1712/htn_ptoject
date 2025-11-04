// gatt_server.c
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"

#include "esp_gatts_api.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_common_api.h"

#include "Libs_GattServer.h"
#include "uart_handle.h"

#include <time.h>
#include <sys/time.h>
#include "esp_attr.h"
#include "esp_sleep.h"
#include "esp_netif_sntp.h"
#include "lwip/ip_addr.h"
#include "esp_sntp.h"

EventGroupHandle_t EventGroupHandle;

#define NOTIFY_TIME_INTERVAL_MS    500

#define BLE_RECEIVE_EVENT_BIT       BIT1
static const char *TAG = "GATTS_SERVER";

void notify_task(void *arg);
void SendData_task(void *arg);

/* ---------------- app_main ---------------- */
void app_main(void)
{
    esp_err_t ret;
    EventGroupHandle = xEventGroupCreate();
    // NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // GATT Server init
    Libs_GattServerInit();

    // UART init
    uart2_init();

    // Start notify task
    xTaskCreate(notify_task, "notify_task", 4096, NULL, 5, NULL);
    xTaskCreate(SendData_task, "SendData_task", 2048, NULL, 2, NULL);
}

void notify_task(void *arg)
{
    int16_t init_temp_x10;
    int16_t init_humi_x10;
    uint16_t init_co2_ppm;
    uint16_t init_mode;
    uint16_t init_state;
    uint16_t init_hum_threshold;
    for(;;) 
    {
        xEventGroupWaitBits(EventGroupHandle, UART_EVENT_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
        uart_GetData(&init_temp_x10, &init_humi_x10, &init_co2_ppm, &init_mode, &init_state, &init_hum_threshold);
        Libs_GattServerNotify(init_temp_x10, init_humi_x10, init_co2_ppm, init_mode, init_state, init_hum_threshold);
    }
}


void SendData_task(void *arg) 
{
    uint16_t led_mode;
    uint16_t led_state;
    uint16_t led_hum_threshold;
    for(;;) {
        xEventGroupWaitBits(EventGroupHandle, BLE_RECEIVE_EVENT_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
        ESP_LOGI(TAG, "BLE receive event");
        Libs_GattServerGetData(&led_state, &led_mode, &led_hum_threshold);
        uart_SendData(led_mode, led_state, led_hum_threshold);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}