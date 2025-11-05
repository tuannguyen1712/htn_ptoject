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


/* ---------------- Device / GAP config ---------------- */
#define DEVICE_NAME            "ESP32_Device"

#define BLE_RECEIVE_EVENT_BIT       BIT1

/* ---------------- Profile definitions ---------------- */
#define PROFILE_NUM       2
#define PROFILE_A_APP_ID  0   // Sensor
#define PROFILE_B_APP_ID  1   // LED


/* --- Sensor Service --- */
#define SENSOR_SERVICE_UUID     0xA000
#define TEMP_CHAR_UUID          0xA001
#define HUMI_CHAR_UUID          0xA002
#define CO2_CHAR_UUID           0xA003
#define MODE_CHAR_UUID          0xA004
#define STATE_CHAR_UUID         0xA005
#define HUM_THRESHOLD_CHAR_UUID 0xA006

enum {
    SENSOR_IDX_SVC = 0,

    SENSOR_IDX_CHAR_TEMP,
    SENSOR_IDX_CHAR_TEMP_VAL,
    SENSOR_IDX_CHAR_TEMP_CCCD,

    SENSOR_IDX_CHAR_HUMI,
    SENSOR_IDX_CHAR_HUMI_VAL,
    SENSOR_IDX_CHAR_HUMI_CCCD,

    SENSOR_IDX_CHAR_CO2,
    SENSOR_IDX_CHAR_CO2_VAL,
    SENSOR_IDX_CHAR_CO2_CCCD,

    SENSOR_IDX_CHAR_MODE,
    SENSOR_IDX_CHAR_MODE_VAL,
    SENSOR_IDX_CHAR_MODE_CCCD,

    SENSOR_IDX_CHAR_STATE,
    SENSOR_IDX_CHAR_STATE_VAL,
    SENSOR_IDX_CHAR_STATE_CCCD,

    SENSOR_IDX_CHAR_HUM_THRESHOLD,
    SENSOR_IDX_CHAR_HUM_THRESHOLD_VAL,
    SENSOR_IDX_CHAR_HUM_THRESHOLD_CCCD,

    SENSOR_IDX_NB   // total number of indices
};


/* --- LED Service --- */
#define LED_SERVICE_UUID                0xB000
#define LED_SAMPLING_INTERVAL_UUID      0xB001
#define LED_MODE_UUID                   0xB002
#define LED_HUM_UUID                    0xB003

enum {
    LED_IDX_SVC = 0,
    
    LED_IDX_CHAR_SAMPLING_INTERVAL,
    LED_IDX_CHAR_SAMPLING_INTERVAL_VAL,

    LED_IDX_CHAR_MODE,
    LED_IDX_CHAR_MODE_VAL,

    LED_IDX_CHAR_HUM,
    LED_IDX_CHAR_HUM_VAL,

    LED_IDX_NB
};

typedef struct {
    esp_gatts_cb_t     gatts_cb;
    uint16_t           gatts_if;
    uint16_t           app_id;
    uint16_t           conn_id;
    uint16_t           service_handle;  // optional, if you start service directly
    esp_gatt_srvc_id_t service_id;
    uint16_t          *handle_table;    // points to attr handles array
} gatts_profile_inst_t;

void Libs_GattServerNotify(uint16_t init_temp_x10, uint16_t init_humi_x10, 
                            uint16_t init_co2_ppm, uint16_t init_mode, 
                            uint16_t init_state, uint16_t init_hum_threshold);
void Libs_GattServerInit(void);
void Libs_GattServerGetData(uint16_t *sampling_interval, uint16_t *mode, uint16_t *hum_threshold);