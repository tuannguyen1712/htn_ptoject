#include <stdio.h>
#include <Libs_wifiSta.h>
#include <Libs_BLEGattClient.h>
#include <Libs_FireBase.h>
#include <Libs_softAP.h>
#include <Libs_nvsHandle.h>
#include <Libs_httpConfig.h>
#include <Configuration.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include "Configuration.h"

void WebServer_Handler();
esp_err_t WebServer_getNvsData(uint8_t *ssid, uint8_t *pass, uint16_t *port);
void WebServer_setNvsData(uint8_t *ssid, uint8_t *pass, uint16_t port);
void GetControlDataFromFirebase();

uint16_t mode, state, temp, humi, co2, hum_thres;
uint16_t new_webapp_mode, new_webapp_sampling_interval, new_webapp_threshold;
uint16_t current_sampling_interval;
uint16_t curent_mode;
uint16_t current_threshold;
uint8_t time_buf[32], date_path[32], time_node[32];

void app_main(void)
{
    xEventGroup = xEventGroupCreate();
    // init NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    xTaskCreate(&WebServer_Handler, "Wifi Config Task", 8096, NULL, 2, NULL);
    
    xEventGroupWaitBits(xEventGroup, WIFI_EVENT_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
    xEventGroupWaitBits(xEventGroup, FIREBASE_EVENT_BIT, pdTRUE, pdTRUE, portMAX_DELAY);

    // init BLE GATT client
    Libs_BLEGattClient_init();
    Libs_FireBaseInit();
    /* Control data monitoring task */
    xTaskCreate(&GetControlDataFromFirebase, "Get Control Data From Firebase", 8096, NULL, 2, NULL);
    
    for(;;)
    {   
        xEventGroupWaitBits(xEventGroup, DATA_COMMING_EVENT_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
        Libs_BLEGattClientGetValue(0, &temp, &humi, &co2, &mode, &state, &hum_thres);
        ESP_LOGI(TAG, "Sensor: temp=%d.%dC, humi=%d.%d%%, co2=%dppm, mode=%d, state=%d", temp/10, temp%10, humi/10, humi%10, co2, mode, state);
        Libs_FireBaseGetTime(time_buf, date_path, time_node);
        firebase_put_node_data("ESP32_ABC123", temp/10.0, humi/10.0, co2, mode, state, hum_thres, time_buf, date_path, time_node);
    }
}

void UpdateManualControlToFirebase() {
    firebase_put_control_data("ESP32_ABC123", mode, hum_thres, current_sampling_interval, last_data_comming_timestamp);
    curent_mode = mode;
    current_threshold = hum_thres;
}

void ApplyControlFromWebapp() {
    current_sampling_interval = new_webapp_sampling_interval;
    curent_mode = new_webapp_mode;
    current_threshold = new_webapp_threshold;
    
    /* Set BLE data */
    gateway_set_sampling_interval(0, new_webapp_sampling_interval);
    gateway_set_mode(0, new_webapp_mode);
    gateway_set_humth(0, new_webapp_threshold);
    ESP_LOGI(TAG, "Send to node from Firebase: interval=%d, mode=%d, threshold=%d", new_webapp_sampling_interval, new_webapp_mode, new_webapp_threshold);
}

void GetControlDataFromFirebase() {
    time_t firebase_control_timestamp;

    /* Wait sync event */
    ESP_LOGI(TAG, "Waiting initial SYNC_EVENT_BIT from nodes...");
    EventBits_t bits = xEventGroupWaitBits(xEventGroup, SYNC_EVENT_BIT | DATA_COMMING_EVENT_BIT, pdTRUE, pdFALSE, portMAX_DELAY);

    firebase_get_and_parse("ESP32_ABC123", &new_webapp_sampling_interval, &new_webapp_mode, &new_webapp_threshold, &firebase_control_timestamp);
    current_sampling_interval = new_webapp_sampling_interval;
    curent_mode = new_webapp_mode;
    current_threshold = new_webapp_threshold;
    if (bits & SYNC_EVENT_BIT) {
        /* Get data and send control from firebase */
        ApplyControlFromWebapp();
        ESP_LOGI(TAG, "Sync data is sent to gatt server...");
    }

    for(;;)
    {   
        EventBits_t uxBits = xEventGroupWaitBits(xEventGroup, SYNC_EVENT_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(5000));
        firebase_get_and_parse("ESP32_ABC123", &new_webapp_sampling_interval, &new_webapp_mode, &new_webapp_threshold, &firebase_control_timestamp);
        ESP_LOGI(TAG, "Control from Firebase: state=%d, mode=%d humt=%d", new_webapp_sampling_interval, new_webapp_mode, new_webapp_threshold);

        uint8_t is_new_control_from_webapp = 0;
        uint8_t is_new_control_from_physical_button = 0;
        
        if ((uxBits & SYNC_EVENT_BIT) != 0) {
            ApplyControlFromWebapp();
        }

        if ((new_webapp_sampling_interval != current_sampling_interval) || 
            (new_webapp_mode != curent_mode) ||
            (new_webapp_threshold != current_threshold)) {
                is_new_control_from_webapp = 1;
                ESP_LOGI(TAG, "New control from web app %lld", firebase_control_timestamp);
        }

        if ((mode != curent_mode) || (hum_thres != current_threshold)) {
            is_new_control_from_physical_button = 1;
            ESP_LOGI(TAG, "New control from buttons %lld", last_data_comming_timestamp);
        }

        if ((is_new_control_from_webapp == 1) && (is_new_control_from_physical_button == 0)) {
            ApplyControlFromWebapp();
        } else if ((is_new_control_from_webapp == 0) && (is_new_control_from_physical_button == 1)) {
            UpdateManualControlToFirebase();
        } else if ((is_new_control_from_webapp == 1) && (is_new_control_from_physical_button == 1)) {
            if (last_data_comming_timestamp > firebase_control_timestamp) {
                UpdateManualControlToFirebase();
                ESP_LOGI(TAG, "Control from button is after from webapp");
            } else {
                ApplyControlFromWebapp();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void WebServer_Handler()
{
    esp_err_t err;
    // uint8_t wifi_data[512];
    uint8_t ssid_ptr[256];
    uint8_t pass_ptr[256];
    uint8_t ssid_len;
    uint8_t pass_len;
    
    ESP_LOGI(TAG, "Web server handler task started");
    err = WebServer_getNvsData(ssid_ptr, pass_ptr, &udpPort);

    if (ESP_OK == err) 
    {
        /* Must call station init function imidiately after lwip init function */
        Libs_lwipInit();
        Libs_staInit((char*) ssid_ptr, (char*) pass_ptr);

        ESP_LOGI(TAG, "Read data from nvs flash: %s %s %d", ssid_ptr, pass_ptr, udpPort);

        /* Wait connection finished, keep WIFI event bit value */
        xEventGroupWaitBits(xEventGroup, WIFI_EVENT_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
        
        /* Connect successful */
        if (Libs_staCheckConnect()) {
            ESP_LOGI(TAG, "Connect to AP successful");
        }
        else
        {
            /* Stop sta mode to switch */
            Libs_staDeinit();
        }
    }
    xEventGroupSetBits(xEventGroup, WIFI_EVENT_BIT);
    ESP_LOGI(TAG, "connection state: %d", Libs_staCheckConnect());

    while (true)
    {
        /* Wait connection finished */
        xEventGroupWaitBits(xEventGroup, WIFI_EVENT_BIT, pdTRUE, pdTRUE, portMAX_DELAY);

        switch (Libs_staCheckConnect())
        {
        case WIFI_CONNECT_SUCCESS:
            xEventGroupSetBits(xEventGroup, FIREBASE_EVENT_BIT);
            break;
        case WIFI_CONNECT_FAILED:
            /* clear udp and mqtt event bit to stop another task */
            xEventGroupClearBits(xEventGroup, FIREBASE_EVENT_BIT);
            
            /* clear ssid and password buffer */
            memset(ssid_ptr, 0, sizeof(ssid_ptr));
            memset(pass_ptr, 0, sizeof(pass_ptr));    

            /* Enter access point mode */
            Libs_softApInit();
            /* Start web server */
            Libs_httpServerStart();
            ESP_LOGI(TAG, "Start webserver for connection configuration");

            /* Waiting for receive wifi configuration from http client */
            xEventGroupWaitBits(xEventGroup, HTTP_EVENT_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
            
            /* Get infomation from webserver */
            Libs_httpServerGetWifiCredentials(ssid_ptr, pass_ptr, &ssid_len, &pass_len);
            
            vTaskDelay(pdMS_TO_TICKS(10));
            
            /* Switch to Sta mode */
            Libs_softApDeinit();

            Libs_lwipInit();
            Libs_staInit((char*) ssid_ptr, (char*) pass_ptr);
            
            /* Wait connection finished */
            xEventGroupWaitBits(xEventGroup, WIFI_EVENT_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
            
            /* Connect successful */
            if (Libs_staCheckConnect()) {
                Libs_staGetApIpAddress(hostIpAddr);
                ESP_LOGI(TAG, "Connect to AP successful");
                /*Stop http server */
                Libs_httpServerStop();   
                /* Save new connection data to nvs flash */
                WebServer_setNvsData(ssid_ptr, pass_ptr, udpPort);
            }
            else
            {
                /* Stop sta mode to switch ap mode */
                Libs_staDeinit();
            }
            xEventGroupSetBits(xEventGroup, WIFI_EVENT_BIT);
        default:
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    vTaskDelete(NULL);
}

esp_err_t WebServer_getNvsData(uint8_t *ssid, uint8_t *pass, uint16_t *port)
{
    esp_err_t ret;
    if ( ESP_OK != Libs_nvsGetData(NVS_WIFI_NAMESPACE, NVS_WIFI_SSID, NVS_STRING, ssid) \
         || ESP_OK != Libs_nvsGetData(NVS_WIFI_NAMESPACE, NVS_WIFI_PASS, NVS_STRING, pass) \
            || ESP_OK != Libs_nvsGetData(NVS_WIFI_NAMESPACE, NVS_WIFI_PORT, NVS_UINT16, port)) 
    {
        ESP_LOGI(TAG, "Read data from nvs flash failed");
        ret = ESP_FAIL;
    }
    else
    {
        if (strlen((char*) ssid) == 0 || strlen((char*) pass) == 0)
        {
            ret = ESP_FAIL;
        }
        else
        { 
            ret = ESP_OK;
        }
    }

    return ret;
}

void WebServer_setNvsData(uint8_t *ssid, uint8_t *pass, uint16_t port)
{
    Libs_nvsSetData(NVS_WIFI_NAMESPACE, NVS_WIFI_SSID, NVS_STRING, ssid);
    Libs_nvsSetData(NVS_WIFI_NAMESPACE, NVS_WIFI_PASS, NVS_STRING, pass);   
    Libs_nvsSetData(NVS_WIFI_NAMESPACE, NVS_WIFI_PORT, NVS_UINT16, &port);
}