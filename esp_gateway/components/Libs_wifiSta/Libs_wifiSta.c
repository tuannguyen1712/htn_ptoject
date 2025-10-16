/**
 ********************************************************************************
 * @file    ${file_name}
 * @author  ${user}
 * @date    ${date}
 * @brief   
 ********************************************************************************
 */

/************************************
 * INCLUDES
 ************************************/
#include <string.h>
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "freertos/FreeRTOS.h" 
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "Libs_wifiSta.h"  

/************************************
 * EXTERN VARIABLES
 ************************************/

/************************************
 * PRIVATE MACROS AND DEFINES
 ************************************/

/************************************
 * PRIVATE TYPEDEFS
 ************************************/

/************************************
 * STATIC VARIABLES
 ************************************/
static const char* TAG = "WIFI STA";
static uint32_t s_retry_num = 0;
static esp_event_handler_instance_t g_instance_any_id;
static esp_event_handler_instance_t g_instance_got_ip;
static esp_netif_t *ptr;
static uint8_t sta_connectState = WIFI_CONNECT_FAILED;       // = 0 when connect fail
esp_netif_ip_info_t sta_ipInfo;

/************************************
 * GLOBAL VARIABLES
 ************************************/

/************************************
 * STATIC FUNCTION PROTOTYPES
 ************************************/
static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data);

/************************************
 * STATIC FUNCTIONS
 ************************************/
static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) 
    {
        esp_wifi_connect();
        ESP_LOGI(TAG, "start connect to access point");
        
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        
        if (s_retry_num < MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI("WIFI STATION", "retry to connect to the AP");
        } 
        else 
        {
            /* Reset retry counter */
            s_retry_num = 0;
            
            /* Update connection state */
            sta_connectState = WIFI_CONNECT_FAILED;
            
            /* Set event bit at the end of connection */
            xEventGroupSetBits(xEventGroup, WIFI_EVENT_BIT);  
            ESP_LOGI("WIFI STATION", "Can't connect to the AP point");
        }
        ESP_LOGI("WIFI STATION","connect to the AP fail");
    } 
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) 
    {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI("WIFI STATION", "got ip:" IPSTR, IP2STR(&event->ip_info.gw));
        
        /* Reset retry counter */
        s_retry_num = 0;
        
        /* Update connection state */
        sta_connectState = WIFI_CONNECT_SUCCESS;

        /* Get access point IPv4 address */
        esp_netif_get_ip_info(ptr, &sta_ipInfo);

        /* Set event bit at the end of connection */
        xEventGroupSetBits(xEventGroup, WIFI_EVENT_BIT);                
    }
}

/************************************
 * GLOBAL FUNCTIONS
 ************************************/
void Libs_staInit(char *s, char *p)
{
    wifi_config_t wifi_config = {0};
    strcpy((char*) wifi_config.sta.ssid, s);
    strcpy((char*) wifi_config.sta.password, p);
    ESP_LOGI("WIFI STATION", "wifi_init_sta finished, ssid:%s, pass:%s. ssid len:%d, pass len:%d",
                (char*) wifi_config.sta.ssid, (char*) wifi_config.sta.password,
                strlen((char*) wifi_config.sta.ssid), strlen((char*) wifi_config.sta.password));
    wifi_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void Libs_staDeinit() {
    esp_wifi_stop();
    esp_wifi_disconnect();
    esp_event_loop_delete_default();
    esp_wifi_set_mode(WIFI_MODE_NULL);
    esp_netif_destroy_default_wifi(ptr);
    esp_netif_deinit();

    ESP_LOGI("WIFI STA", "Deinit station mode");
}

void Libs_lwipInit() 
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ptr = esp_netif_create_default_wifi_sta();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    g_instance_any_id = instance_any_id;                                                    
    g_instance_got_ip = instance_got_ip;
}

/* Wifi string type: w:<ssid>\tp:<pass>
*/
void Libs_staGetWifiInfo(uint8_t *info, uint8_t *ssid, uint8_t *password)
{
    uint8_t sfl = 1;
    int k = 0;
    for (int i = 2; i < strlen((char*) info); i++) {
        if (sfl) {
            if (info[i] != '\t') {
                ssid[k] = info[i];
                k++;
            }

            else {
                sfl = 0;
                ssid[k] = 0;
                k = 0;
            }
        }
        else {
            strcpy((char*) password, (char*) (info + i + 2));
            break;
        }
    }
    ESP_LOGI(TAG, "ssid: .%s. pass: .%s.\n", ssid, password);
}

void Libs_staSetWifiInfo(uint8_t *info, uint8_t *ssid, uint8_t *password) 
{
    sprintf((char*) info, "w:%s\tp:%s", ssid, password);
}

uint8_t Libs_staCheckConnect(void) 
{
    return sta_connectState == WIFI_CONNECT_SUCCESS;
}

void Libs_staGetApIpAddress(char *ip)
{
    sprintf(ip, IPSTR, IP2STR(&sta_ipInfo.gw));
}
