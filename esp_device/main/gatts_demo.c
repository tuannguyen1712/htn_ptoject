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

#include "uart_handle.h"


#define BLE_RECEIVE_EVENT_BIT       BIT1

static const char *TAG = "GATT_DEMO";

EventGroupHandle_t EventGroupHandle;

/* ---------------- Device / GAP config ---------------- */
#define DEVICE_NAME            "ESP32_Device"

/* Adv service UUID list (two services: 0xA000 and 0xB000) */
static uint8_t adv_service_uuid128[32] = {
    // Sensor Service UUID = 0xA000
    0xfb,0x34,0x9b,0x5f,0x80,0x00,0x00,0x80,
    0x00,0x10,0x00,0x00,0x00,0xA0,0x00,0x00,
    // LED Service UUID = 0xB000
    0xfb,0x34,0x9b,0x5f,0x80,0x00,0x00,0x80,
    0x00,0x10,0x00,0x00,0x00,0xB0,0x00,0x00,
};

static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = true,
    .min_interval = 0x20,     // 20ms
    .max_interval = 0x40,     // 40ms
    .appearance = 0x00,
    .manufacturer_len = 0,
    .p_manufacturer_data = NULL,
    .service_uuid_len = sizeof(adv_service_uuid128),
    .p_service_uuid = adv_service_uuid128,
    .flag = ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT,
};

static esp_ble_adv_params_t adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

/* ---------------- Profile definitions ---------------- */
#define PROFILE_NUM       2
#define PROFILE_A_APP_ID  0   // Sensor
#define PROFILE_B_APP_ID  1   // LED

typedef struct {
    esp_gatts_cb_t     gatts_cb;
    uint16_t           gatts_if;
    uint16_t           app_id;
    uint16_t           conn_id;
    uint16_t           service_handle;  // optional, if you start service directly
    esp_gatt_srvc_id_t service_id;
    uint16_t          *handle_table;    // points to attr handles array
} gatts_profile_inst_t;

/* ---------------- UUIDs & Attribute Tables ---------------- */
// Common UUIDs
static const uint16_t UUID_PRIMARY_SERVICE        = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t UUID_CHAR_DECLARE           = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t UUID_CCCD                   = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;

// Properties
static const uint8_t  PROP_READ_NOTIFY            = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;
static const uint8_t  PROP_READ_WRITE             = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE;

/* --- Sensor Service --- */
#define SENSOR_SERVICE_UUID   0xA000
#define TEMP_CHAR_UUID        0xA001
#define HUMI_CHAR_UUID        0xA002
#define CO2_CHAR_UUID         0xA003
#define MODE_CHAR_UUID        0xA004
#define STATE_CHAR_UUID       0xA005

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

    SENSOR_IDX_NB   // total number of indices
};
static uint16_t sensor_handle_table[SENSOR_IDX_NB];

// Initial values (fixed-point)
static int16_t  init_temp_x10 = 250;   // 25.0 C
static int16_t  init_humi_x10 = 500;   // 50.0 %RH
static uint16_t init_co2_ppm  = 450;   // 450 ppm
static uint16_t init_mode     = 0;     // mode maunual
static uint16_t init_state    = 0;     // state off
static uint16_t cccd_init     = 0x0000;

static const esp_gatts_attr_db_t gatt_db_sensor[SENSOR_IDX_NB] = {
    // Service
    [SENSOR_IDX_SVC] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&UUID_PRIMARY_SERVICE, ESP_GATT_PERM_READ,
         sizeof(uint16_t), sizeof(uint16_t), (uint8_t*)&(uint16_t){SENSOR_SERVICE_UUID}}
    },

    // Temperature declaration
    [SENSOR_IDX_CHAR_TEMP] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&UUID_CHAR_DECLARE, ESP_GATT_PERM_READ,
         sizeof(uint8_t), sizeof(uint8_t), (uint8_t*)&PROP_READ_NOTIFY}
    },
    // Temperature value
    [SENSOR_IDX_CHAR_TEMP_VAL] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&(uint16_t){TEMP_CHAR_UUID}, ESP_GATT_PERM_READ,
         sizeof(init_temp_x10), sizeof(init_temp_x10), (uint8_t*)&init_temp_x10}
    },
    // Temperature CCCD
    [SENSOR_IDX_CHAR_TEMP_CCCD] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&UUID_CCCD, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
         sizeof(uint16_t), sizeof(uint16_t), (uint8_t*)&cccd_init}
    },

    // Humidity declaration
    [SENSOR_IDX_CHAR_HUMI] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&UUID_CHAR_DECLARE, ESP_GATT_PERM_READ,
         sizeof(uint8_t), sizeof(uint8_t), (uint8_t*)&PROP_READ_NOTIFY}
    },
    // Humidity value
    [SENSOR_IDX_CHAR_HUMI_VAL] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&(uint16_t){HUMI_CHAR_UUID}, ESP_GATT_PERM_READ,
         sizeof(init_humi_x10), sizeof(init_humi_x10), (uint8_t*)&init_humi_x10}
    },
    // Humidity CCCD
    [SENSOR_IDX_CHAR_HUMI_CCCD] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&UUID_CCCD, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
         sizeof(uint16_t), sizeof(uint16_t), (uint8_t*)&cccd_init}
    },

    // CO2 declaration
    [SENSOR_IDX_CHAR_CO2] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&UUID_CHAR_DECLARE, ESP_GATT_PERM_READ,
         sizeof(uint8_t), sizeof(uint8_t), (uint8_t*)&PROP_READ_NOTIFY}
    },
    // CO2 value
    [SENSOR_IDX_CHAR_CO2_VAL] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&(uint16_t){CO2_CHAR_UUID}, ESP_GATT_PERM_READ,
         sizeof(init_co2_ppm), sizeof(init_co2_ppm), (uint8_t*)&init_co2_ppm}
    },
    // CO2 CCCD
    [SENSOR_IDX_CHAR_CO2_CCCD] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&UUID_CCCD, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
         sizeof(uint16_t), sizeof(uint16_t), (uint8_t*)&cccd_init}
    },

    // device mode declaration
    [SENSOR_IDX_CHAR_MODE] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&UUID_CHAR_DECLARE, ESP_GATT_PERM_READ,
         sizeof(uint8_t), sizeof(uint8_t), (uint8_t*)&PROP_READ_NOTIFY}
    },
    // mode 
    [SENSOR_IDX_CHAR_MODE_VAL] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&(uint16_t){MODE_CHAR_UUID}, ESP_GATT_PERM_READ,
         sizeof(init_mode), sizeof(init_mode), (uint8_t*)&init_mode}
    },
    // mode CCCD
    [SENSOR_IDX_CHAR_MODE_CCCD] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&UUID_CCCD, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
         sizeof(uint16_t), sizeof(uint16_t), (uint8_t*)&cccd_init}
    },

    // device state declaration
    [SENSOR_IDX_CHAR_STATE] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&UUID_CHAR_DECLARE, ESP_GATT_PERM_READ,
         sizeof(uint8_t), sizeof(uint8_t), (uint8_t*)&PROP_READ_NOTIFY}
    },
    // state 
    [SENSOR_IDX_CHAR_STATE_VAL] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&(uint16_t){STATE_CHAR_UUID}, ESP_GATT_PERM_READ,
         sizeof(init_state), sizeof(init_state), (uint8_t*)&init_state}
    },
    // state CCCD
    [SENSOR_IDX_CHAR_STATE_CCCD] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&UUID_CCCD, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
         sizeof(uint16_t), sizeof(uint16_t), (uint8_t*)&cccd_init}
    },
};

/* --- LED Service --- */
#define LED_SERVICE_UUID   0xB000
#define LED_STATE_UUID     0xB001
#define LED_MODE_UUID      0xB002

enum {
    LED_IDX_SVC = 0,
    
    LED_IDX_CHAR_STATE,
    LED_IDX_CHAR_STATE_VAL,

    LED_IDX_CHAR_MODE,
    LED_IDX_CHAR_MODE_VAL,

    LED_IDX_NB
};
static uint16_t led_handle_table[LED_IDX_NB];
static uint8_t  led_state = 0;
static uint8_t  led_mode  = 0;

static const esp_gatts_attr_db_t gatt_db_led[LED_IDX_NB] = {
    [LED_IDX_SVC] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&UUID_PRIMARY_SERVICE, ESP_GATT_PERM_READ,
         sizeof(uint16_t), sizeof(uint16_t), (uint8_t*)&(uint16_t){LED_SERVICE_UUID}}
    },
    [LED_IDX_CHAR_STATE] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&UUID_CHAR_DECLARE, ESP_GATT_PERM_READ,
         sizeof(uint8_t), sizeof(uint8_t), (uint8_t*)&PROP_READ_WRITE}
    },
    [LED_IDX_CHAR_STATE_VAL] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&(uint16_t){LED_STATE_UUID}, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
         sizeof(led_state), sizeof(led_state), &led_state}
    },
    [LED_IDX_CHAR_MODE] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&UUID_CHAR_DECLARE, ESP_GATT_PERM_READ,
         sizeof(uint8_t), sizeof(uint8_t), (uint8_t*)&PROP_READ_WRITE}
    },
    [LED_IDX_CHAR_MODE_VAL] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&(uint16_t){LED_MODE_UUID}, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
         sizeof(led_mode), sizeof(led_mode), &led_mode}
    },
};

/* ---------------- Profile table ---------------- */
static gatts_profile_inst_t gl_profile_tab[PROFILE_NUM] = {
    [PROFILE_A_APP_ID] = { // Sensor
        .gatts_cb = NULL,  // set later
        .gatts_if = ESP_GATT_IF_NONE,
        .handle_table = sensor_handle_table,
    },
    [PROFILE_B_APP_ID] = { // LED
        .gatts_cb = NULL,  // set later
        .gatts_if = ESP_GATT_IF_NONE,
        .handle_table = led_handle_table,
    },
};

/* ---------------- Notify controls ---------------- */
static bool conn_a_valid = false;
static uint16_t conn_a_id = 0;

static bool notify_temp_en  = false;
static bool notify_humi_en  = false;
static bool notify_co2_en   = false;
static bool notify_mode_en  = false;
static bool notify_state_en = false;

/* ---------------- Forward decls ---------------- */
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
static void gatts_profile_a_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
static void gatts_profile_b_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);


// send data task
static void notify_task(void *arg) {
    while (1) {
        uart_handle_event();
        if (conn_a_valid) {
            // Fake sensor updates, HERE
            // init_temp_x10 += 1;  // +0.1C
            // init_humi_x10  = 500 + (init_temp_x10 % 20); // 50.0% ±
            // init_co2_ppm   = 420 + (init_temp_x10 % 50); // 420~470 ppm
            // init_mode      = 1;        
            // init_state     = 0;
            
            uart_GetData(&init_temp_x10, &init_humi_x10, &init_co2_ppm, &init_mode, &init_state);
            ESP_LOGI(TAG, "Notify data: T=%d H=%d CO2=%d mode=%d state=%d", init_temp_x10, init_humi_x10, init_co2_ppm, init_mode, init_state);

            if (notify_temp_en) {
                esp_ble_gatts_send_indicate(gl_profile_tab[PROFILE_A_APP_ID].gatts_if,
                                            conn_a_id,
                                            sensor_handle_table[SENSOR_IDX_CHAR_TEMP_VAL],
                                            sizeof(init_temp_x10), (uint8_t*)&init_temp_x10, false);
            }
            if (notify_humi_en) {
                esp_ble_gatts_send_indicate(gl_profile_tab[PROFILE_A_APP_ID].gatts_if,
                                            conn_a_id,
                                            sensor_handle_table[SENSOR_IDX_CHAR_HUMI_VAL],
                                            sizeof(init_humi_x10), (uint8_t*)&init_humi_x10, false);
            }
            if (notify_co2_en) {
                esp_ble_gatts_send_indicate(gl_profile_tab[PROFILE_A_APP_ID].gatts_if,
                                            conn_a_id,
                                            sensor_handle_table[SENSOR_IDX_CHAR_CO2_VAL],
                                            sizeof(init_co2_ppm), (uint8_t*)&init_co2_ppm, false);
            }

            if (notify_mode_en) {
                esp_ble_gatts_send_indicate(gl_profile_tab[PROFILE_A_APP_ID].gatts_if,
                                            conn_a_id,
                                            sensor_handle_table[SENSOR_IDX_CHAR_MODE_VAL],
                                            sizeof(init_mode), (uint8_t*)&init_mode, false);
            }

            if (notify_state_en) {
                esp_ble_gatts_send_indicate(gl_profile_tab[PROFILE_A_APP_ID].gatts_if,
                                            conn_a_id,
                                            sensor_handle_table[SENSOR_IDX_CHAR_STATE_VAL],
                                            sizeof(init_state), (uint8_t*)&init_state, false);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* ---------------- GAP handler ---------------- */
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        ESP_LOGI(TAG, "Adv data set. Start advertising");
        esp_ble_gap_start_advertising(&adv_params);
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "Adv start failed: %d", param->adv_start_cmpl.status);
        }
        break;
    default:
        break;
    }
}

/* ---------------- Profile A: Sensor ---------------- */
static void gatts_profile_a_event_handler(esp_gatts_cb_event_t event,
                                          esp_gatt_if_t gatts_if,
                                          esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
    case ESP_GATTS_REG_EVT:
        ESP_LOGI(TAG, "A REG app_id=%d, gatts_if=%d", param->reg.app_id, gatts_if);
        gl_profile_tab[PROFILE_A_APP_ID].gatts_if = gatts_if;
        esp_ble_gatts_create_attr_tab(gatt_db_sensor, gatts_if, SENSOR_IDX_NB, PROFILE_A_APP_ID);
        break;

    case ESP_GATTS_CREAT_ATTR_TAB_EVT:
        if (param->add_attr_tab.status == ESP_GATT_OK &&
            param->add_attr_tab.num_handle == SENSOR_IDX_NB &&
            param->add_attr_tab.svc_uuid.uuid.uuid16 == SENSOR_SERVICE_UUID) {
            memcpy(sensor_handle_table, param->add_attr_tab.handles, sizeof(sensor_handle_table));
            esp_ble_gatts_start_service(sensor_handle_table[SENSOR_IDX_SVC]);
            ESP_LOGI(TAG, "Sensor service started");
        } else {
            ESP_LOGE(TAG, "Sensor attr table failed, status=0x%x", param->add_attr_tab.status);
        }
        break;

    case ESP_GATTS_CONNECT_EVT:
        conn_a_valid = true;
        conn_a_id = param->connect.conn_id;
        gl_profile_tab[PROFILE_A_APP_ID].conn_id = conn_a_id;
        ESP_LOGI(TAG, "A CONNECT conn_id=%d", conn_a_id);
        break;

    case ESP_GATTS_DISCONNECT_EVT:
        ESP_LOGI(TAG, "A DISCONNECT");
        conn_a_valid = false;
        notify_temp_en = notify_humi_en = notify_co2_en = notify_mode_en = notify_state_en = false;
        esp_ble_gap_start_advertising(&adv_params);
        break;

    case ESP_GATTS_WRITE_EVT: {
        // Check CCCD writes for 5 characteristics
        uint16_t handle = param->write.handle;
        bool enable = (param->write.len >= 2) && (param->write.value[0] & 0x01);

        if (handle == sensor_handle_table[SENSOR_IDX_CHAR_TEMP_CCCD]) {
            notify_temp_en = enable;
            ESP_LOGI(TAG, "TEMP notify %s", enable ? "EN" : "DIS");
        } else if (handle == sensor_handle_table[SENSOR_IDX_CHAR_HUMI_CCCD]) {
            notify_humi_en = enable;
            ESP_LOGI(TAG, "HUMI notify %s", enable ? "EN" : "DIS");
        } else if (handle == sensor_handle_table[SENSOR_IDX_CHAR_CO2_CCCD]) {
            notify_co2_en = enable;
            ESP_LOGI(TAG, "CO2 notify %s", enable ? "EN" : "DIS");
        }
        else if (handle == sensor_handle_table[SENSOR_IDX_CHAR_MODE_CCCD]) {
            notify_mode_en = enable;
            ESP_LOGI(TAG, "Device mode notify %s", enable ? "EN" : "DIS");
        }
        else if (handle == sensor_handle_table[SENSOR_IDX_CHAR_STATE_CCCD]) {
            notify_state_en = enable;
            ESP_LOGI(TAG, "Device state notify %s", enable ? "EN" : "DIS");
        }
        
        break;
    }

    default:
        break;
    }
}

/* ---------------- Profile B: LED ---------------- */
static void gatts_profile_b_event_handler(esp_gatts_cb_event_t event,
                                          esp_gatt_if_t gatts_if,
                                          esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
    case ESP_GATTS_REG_EVT:
        ESP_LOGI(TAG, "B REG app_id=%d, gatts_if=%d", param->reg.app_id, gatts_if);
        gl_profile_tab[PROFILE_B_APP_ID].gatts_if = gatts_if;
        esp_ble_gatts_create_attr_tab(gatt_db_led, gatts_if, LED_IDX_NB, PROFILE_B_APP_ID);
        break;

    case ESP_GATTS_CREAT_ATTR_TAB_EVT:
        if (param->add_attr_tab.status == ESP_GATT_OK &&
            param->add_attr_tab.num_handle == LED_IDX_NB &&
            param->add_attr_tab.svc_uuid.uuid.uuid16 == LED_SERVICE_UUID) {
            memcpy(led_handle_table, param->add_attr_tab.handles, sizeof(led_handle_table));
            esp_ble_gatts_start_service(led_handle_table[LED_IDX_SVC]);
            ESP_LOGI(TAG, "LED handles:");
            ESP_LOGI(TAG, "  SVC        : 0x%04X", led_handle_table[LED_IDX_SVC]);
            ESP_LOGI(TAG, "  CHAR_DECL  : 0x%04X", led_handle_table[LED_IDX_CHAR_STATE]);
            ESP_LOGI(TAG, "  CHAR_VALUE : 0x%04X", led_handle_table[LED_IDX_CHAR_STATE_VAL]);
            ESP_LOGI(TAG, "  CHAR_DECL  : 0x%04X", led_handle_table[LED_IDX_CHAR_MODE]);
            ESP_LOGI(TAG, "  CHAR_VALUE : 0x%04X", led_handle_table[LED_IDX_CHAR_MODE_VAL]);
        } else {
            ESP_LOGE(TAG, "LED attr table failed, status=0x%x", param->add_attr_tab.status);
        }
        break;

    case ESP_GATTS_CONNECT_EVT:
        gl_profile_tab[PROFILE_B_APP_ID].conn_id = param->connect.conn_id;
        break;

    case ESP_GATTS_WRITE_EVT:
        if (param->write.handle == led_handle_table[LED_IDX_CHAR_STATE_VAL] && param->write.len >= 1) {
            led_state = param->write.value[0] ? 1 : 0;
            // TODO: gpio_set_level(GPIO_LED, led_state);
            ESP_LOGI(TAG, "LED STATE -> %d", led_state);
        }
        else if (param->write.handle == led_handle_table[LED_IDX_CHAR_MODE_VAL] && param->write.len >= 1) {
            led_mode = param->write.value[0] ? 1 : 0;
            ESP_LOGI(TAG, "LED MODE -> %d", led_mode);
        }
        xEventGroupSetBits(EventGroupHandle, BLE_RECEIVE_EVENT_BIT);
        break;

    default:
        break;
    }
}

/* ---------------- GATTS dispatcher ---------------- */
static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                esp_ble_gatts_cb_param_t *param)
{
    // If event is register, use returned gatts_if; else dispatch to matching profile(s)
    if (event == ESP_GATTS_REG_EVT) {
        if (param->reg.app_id == PROFILE_A_APP_ID) {
            gatts_profile_a_event_handler(event, gatts_if, param);
        } else if (param->reg.app_id == PROFILE_B_APP_ID) {
            gatts_profile_b_event_handler(event, gatts_if, param);
        }
        return;
    }

    // For other events, dispatch to all profiles that match gatts_if
    if (gatts_if == ESP_GATT_IF_NONE ||
        gatts_if == gl_profile_tab[PROFILE_A_APP_ID].gatts_if) {
        gatts_profile_a_event_handler(event, gl_profile_tab[PROFILE_A_APP_ID].gatts_if, param);
    }
    if (gatts_if == ESP_GATT_IF_NONE ||
        gatts_if == gl_profile_tab[PROFILE_B_APP_ID].gatts_if) {
        gatts_profile_b_event_handler(event, gl_profile_tab[PROFILE_B_APP_ID].gatts_if, param);
    }
}

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

    // BT controller
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT)); // BLE only
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));

    // Bluedroid
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    // GAP/GATTS callbacks
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_event_handler));
    ESP_ERROR_CHECK(esp_ble_gatts_register_callback(gatts_event_handler));

    // Register two apps (profiles)
    ESP_ERROR_CHECK(esp_ble_gatts_app_register(PROFILE_A_APP_ID));
    ESP_ERROR_CHECK(esp_ble_gatts_app_register(PROFILE_B_APP_ID));

    // Set device name & adv data
    ESP_ERROR_CHECK(esp_ble_gap_set_device_name(DEVICE_NAME));
    ESP_ERROR_CHECK(esp_ble_gap_config_adv_data(&adv_data));

    // Optionally increase local MTU
    esp_err_t mtu_ret = esp_ble_gatt_set_local_mtu(185);
    if (mtu_ret) {
        ESP_LOGW(TAG, "set local MTU failed, error code = 0x%x", mtu_ret);
    }

    // Set profile callbacks after forward decls exist
    gl_profile_tab[PROFILE_A_APP_ID].gatts_cb = gatts_profile_a_event_handler;
    gl_profile_tab[PROFILE_B_APP_ID].gatts_cb = gatts_profile_b_event_handler;


    // UART init
    uart2_init();

    // Start notify task
    xTaskCreate(notify_task, "notify_task", 4096, NULL, 5, NULL);
    for(;;) {
        xEventGroupWaitBits(EventGroupHandle, BLE_RECEIVE_EVENT_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
        ESP_LOGI(TAG, "BLE receive event");
        uart_SendData(led_mode, led_state);
    }
}
