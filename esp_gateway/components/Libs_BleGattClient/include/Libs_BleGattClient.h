// gattc_multi_demo.c  (ESP-IDF v5.1.1, Bluedroid)
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "nvs_flash.h"

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_common_api.h"


/* ==== THÔNG TIN NODE SERVER ==== */
#define TARGET_NAME          "ESP32_Device"      // tên quảng cáo server
#define MAX_PEERS            5                    // Số server kết nối đồng thời

/* ==== UUID khớp với server ==== */
#define SENSOR_SVC_UUID16       0xA000
#define TEMP_CHAR_UUID16        0xA001
#define HUMI_CHAR_UUID16        0xA002
#define CO2_CHAR_UUID16         0xA003
#define DEVICE_MODE             0xA004
#define DEVICE_STATE            0xA005
#define DEVICE_HUM_THRES        0xA006


#define LED_SVC_UUID16          0xB000         
#define LED_CHAR_UUID16         0xB001
#define MODE_CHAR_UUID16        0xB002
#define HUM_THRES_CHAR_UUID16   0xB003
#define CCCD_UUID16             0x2902

/* ==== TRẠNG THÁI/STRUCT CHO MỖI SERVER ==== */
typedef struct {
    bool in_use;
    bool connected;
    esp_bd_addr_t bda;
    esp_ble_addr_type_t bda_type;
    uint16_t conn_id;

    // service handle ranges
    uint16_t sensor_start, sensor_end;
    uint16_t led_start, led_end;

    // char handles
    uint16_t h_temp_val, h_humi_val, h_co2_val, h_state_val, h_mode_val, h_hum_thres_val;
    uint16_t h_temp_cccd, h_humi_cccd, h_co2_cccd, h_state_cccd, h_mode_cccd, h_hum_thres_cccd;
    uint16_t h_led_val, h_led_mode_val, h_led_hum_thres_val;
} peer_t;

typedef struct {
    float temp;
    float humi;
    uint32_t co2;
    uint32_t mode;
    uint32_t state;
    uint32_t hum_thres;
} device_data_t;

/* ==== Global function ==== */
void Libs_BLEGattClient_init(void);
void gateway_set_state(int server_idx, uint16_t state);
void gateway_set_mode(int server_idx, uint8_t on);
void gateway_set_humth(int server_idx, uint8_t on);
void Libs_BLEGattClientGetValue(uint16_t idx, uint16_t *temp, uint16_t *humi, uint16_t *co2, uint16_t *mode, uint16_t *state, uint16_t *hum_thres);