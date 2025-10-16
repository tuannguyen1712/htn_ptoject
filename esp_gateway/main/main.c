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

void WebServer_Handler();
esp_err_t WebServer_getNvsData(uint8_t *ssid, uint8_t *pass, uint16_t *port);
void WebServer_setNvsData(uint8_t *ssid, uint8_t *pass, uint16_t port);

uint16_t mode, state, temp, humi, co2;
uint16_t ctrl_mode, ctrl_state;

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

    /* Must call station init function imidiately after lwip init function */
    // Libs_lwipInit();
    // Libs_staInit((char*) "IP12PRX", (char*) "toikhongbiet");

    xEventGroupWaitBits(xEventGroup, WIFI_EVENT_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
    xEventGroupWaitBits(xEventGroup, FIREBASE_EVENT_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
    // // init BLE GATT client
    
    // vTaskDelay(10000 / portTICK_PERIOD_MS);
    Libs_BLEGattClient_init();
    Libs_FireBaseInit();

    for(;;)
    {
        
        Libs_BLEGattClientGetValue(0, &temp, &humi, &co2, &mode, &state);
        ESP_LOGI(TAG, "Sensor: temp=%d.%dC, humi=%d.%d%%, co2=%dppm, mode=%d, state=%d", temp/10, temp%10, humi/10, humi%10, co2, mode, state);
        firebase_put_example("ESP32_ABC123", temp/10.0, humi/10.0, co2, mode, state);
        // Ví dụ GET + parse
        firebase_get_and_parse("ESP32_ABC123", &ctrl_state, &ctrl_mode);
        if (ctrl_state != state || ctrl_mode != mode) { 
            gateway_set_state(0, ctrl_state);
            gateway_set_mode(0, ctrl_mode);
            ESP_LOGI(TAG, "Parsed from Firebase: state=%d, mode=%d", ctrl_state, ctrl_mode);
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
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