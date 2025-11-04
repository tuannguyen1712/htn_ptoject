// main.c
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"

#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"

#include "esp_sntp.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "esp_netif_sntp.h"
#include "lwip/ip_addr.h"
#include "esp_sntp.h"

#include "cJSON.h"

//====================== USER CONFIG ======================//

// Firebase Realtime Database base URL 
#define FIREBASE_BASE_URL       "https://htn-project-8381a-default-rtdb.asia-southeast1.firebasedatabase.app"

// Node 
#define FIREBASE_PATH_DATA       "/sensor/data"
#define FIREBASE_PATH_CONTROL    "/sensor/control"

// Token
#define FIREBASE_AUTH_TOKEN     "KlMYO3uxZRE5AvnbolhaRvf5U2wIUUVPeWB10XLG"  
//========================================================//

static const char *TAG = "APP";

static EventGroupHandle_t s_wifi_event_group;
static const int WIFI_CONNECTED_BIT = BIT0;
time_t now = 0; 
struct tm timeinfo = {0};

// -------------------- Wi-Fi --------------------
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "WiFi disconnected, retry...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wicfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wicfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_config_t sta_cfg = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {.capable = true, .required = false},
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Waiting for WiFi...");
    ESP_LOGI(TAG, "WiFi connected");
}

// -------------------- SNTP (TLS needs valid time) --------------------
static void sntp_sync_time_test(void)
{
    ESP_LOGI(TAG, "Init SNTP...");
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "time1.google.com");
    esp_sntp_init();

    // đợi time hợp lệ
    int retry = 0;
    const int retry_count = 15;
    while (esp_netif_sntp_sync_wait(2000 / portTICK_PERIOD_MS) == ESP_ERR_TIMEOUT && ++retry < retry_count) {
        ESP_LOGI("SNTP", "Waiting for system time to be set... (%d/%d)", retry, retry_count);
    }
    if (retry < retry_count) 
    {
        ESP_LOGI("SNTP", "Set time success");  
        time(&now);
        localtime_r(&now, &timeinfo);
    }
    else 
        ESP_LOGW(TAG, "SNTP time not set yet (continue anyway)");
}

// -------------------- HTTP helper --------------------
typedef struct {
    char *buf;        // dynamic buffer
    size_t len;       // used length
} http_resp_buf_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    http_resp_buf_t *acc = (http_resp_buf_t *)evt->user_data;

    switch (evt->event_id) {
    case HTTP_EVENT_ON_CONNECTED:
        //ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        //ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        //ESP_LOGD(TAG, "HDR: %.*s: %.*s", evt->header_key_len, evt->header_key, evt->header_value_len, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        if (acc) {
            // append data
            size_t new_len = acc->len + evt->data_len;
            char *new_buf = realloc(acc->buf, new_len + 1);
            if (!new_buf) {
                ESP_LOGE(TAG, "realloc failed");
                return ESP_FAIL;
            }
            acc->buf = new_buf;
            memcpy(acc->buf + acc->len, evt->data, evt->data_len);
            acc->len = new_len;
            acc->buf[acc->len] = '\0';
        }
        break;
    case HTTP_EVENT_ON_FINISH:
        //ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        //ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    case HTTP_EVENT_ERROR:
        ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
        break;
    default:
        break;
    }
    return ESP_OK;
}

static void build_url(char *out, size_t out_sz, const char *path_no_json)
{
    // tạo URL: BASE + PATH + .json [ + ?auth=TOKEN nếu có]
    if (FIREBASE_AUTH_TOKEN && strlen(FIREBASE_AUTH_TOKEN) > 0) {
        snprintf(out, out_sz, "%s%s.json?auth=%s", FIREBASE_BASE_URL, path_no_json, FIREBASE_AUTH_TOKEN);
    } else {
        snprintf(out, out_sz, "%s%s.json", FIREBASE_BASE_URL, path_no_json);
    }
}

// -------------------- Firebase: POST (tạo key tự sinh) --------------------
static esp_err_t firebase_post_example(const char *device_id, float temp, float hum, int co2_ppm, int mode, int state, int hum_thres, uint8_t *time_stamp)
{
    char path[256];
    snprintf(path, sizeof(path), "%s/%s", FIREBASE_PATH_DATA, device_id);

    char url[256];
    build_url(url, sizeof(url), path);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "Temp", temp);
    cJSON_AddNumberToObject(root, "Humi", hum);
    cJSON_AddNumberToObject(root, "CO2", co2_ppm);
    cJSON_AddNumberToObject(root, "Mode", mode);   // 0: auto, 1: manual
    cJSON_AddNumberToObject(root, "State", state); // 0: off, 1: on
    cJSON_AddNumberToObject(root, "HumThres", hum_thres);
    cJSON_AddStringToObject(root, "Timestamp", (const char*)time_stamp);
    char *post_data = cJSON_PrintUnformatted(root);

    http_resp_buf_t acc = {.buf = NULL, .len = 0};

    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .event_handler = http_event_handler,
        .user_data = &acc,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 10000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    ESP_LOGI(TAG, "HTTP POST %s", url);
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "POST status=%d, resp=%s", code, acc.buf ? acc.buf : "(null)");
    } else {
        ESP_LOGE(TAG, "POST failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    cJSON_Delete(root);
    free(post_data);
    free(acc.buf);
    return err;
}

// -------------------- Firebase: PUT (ghi đè vào key cố định) --------------------
// ví dụ đặt key theo device_id
esp_err_t firebase_put_example(const char *device_id, float temp, float hum, int co2_ppm, int mode, int state, 
                                int hum_thres, uint8_t *time_stamp, uint8_t *time_path)
{
    char path[256];
    snprintf(path, sizeof(path), "%s/%s/%s", FIREBASE_PATH_DATA, device_id, (const char*)time_path);

    char url[256];
    build_url(url, sizeof(url), path);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "Temp", temp);
    cJSON_AddNumberToObject(root, "Humi", hum);
    cJSON_AddNumberToObject(root, "CO2", co2_ppm);
    cJSON_AddNumberToObject(root, "Mode", mode);   // 0: auto, 1: manual
    cJSON_AddNumberToObject(root, "State", state); // 0: off, 1: on
    cJSON_AddNumberToObject(root, "HumThres", hum_thres);
    cJSON_AddStringToObject(root, "Timestamp", (const char*)time_stamp);
    char *put_data = cJSON_PrintUnformatted(root);

    http_resp_buf_t acc = {.buf = NULL, .len = 0};

    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_PUT,
        .event_handler = http_event_handler,
        .user_data = &acc,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 10000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, put_data, strlen(put_data));

    ESP_LOGI(TAG, "HTTP PUT %s", url);
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "PUT status=%d, resp=%s", code, acc.buf ? acc.buf : "(null)");
    } else {
        ESP_LOGE(TAG, "PUT failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    cJSON_Delete(root);
    free(put_data);
    free(acc.buf);
    return err;
}

// -------------------- Firebase: GET + parse JSON --------------------
esp_err_t firebase_get_and_parse(const char *device_id, uint16_t *device_state, uint16_t *device_mode, uint16_t *device_hum_thres)
{
    char url[256];
    build_url(url, sizeof(url), FIREBASE_PATH_CONTROL);

    http_resp_buf_t acc = {.buf = NULL, .len = 0};

    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .event_handler = http_event_handler,
        .user_data = &acc,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 10000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);

    ESP_LOGI(TAG, "HTTP GET %s", url);
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "GET status=%d", code);
        ESP_LOGI(TAG, "GET body=%s", acc.buf ? acc.buf : "(null)");

        if (acc.buf) {
            // Parse: tùy cấu trúc (POST → map key tự sinh; PUT → object cố định)
            cJSON *root = cJSON_Parse(acc.buf);
            if (root) {
                if (cJSON_IsObject(root)) {
                    // Duyệt các field
                    cJSON *it = NULL;
                    cJSON_ArrayForEach(it, root) {
                        if (cJSON_IsObject(it)) {
                            cJSON *mode = cJSON_GetObjectItem(it, "mode");
                            cJSON *state = cJSON_GetObjectItem(it, "state");
                            cJSON *hum_t = cJSON_GetObjectItem(it, "humi_thres");
                            if (mode) ESP_LOGI(TAG, "Item '%s' -> mode=%g", it->string, mode->valuedouble);
                            if (state) ESP_LOGI(TAG, "Item '%s' -> state=%g", it->string, state->valuedouble);
                            if (hum_t) ESP_LOGI(TAG, "Item '%s' -> hum_t=%g", it->string, hum_t->valuedouble);
                            *device_state = state ? state->valuedouble : 0;
                            *device_mode  = mode  ? mode->valuedouble  : 0;
                            *device_hum_thres  = hum_t  ? hum_t->valuedouble  : 0;
                        } else {
                            // nếu GET path là key cố định (PUT), root chính là object
                            cJSON *mode = cJSON_GetObjectItem(root, "mode");
                            cJSON *state = cJSON_GetObjectItem(root, "state");
                            cJSON *hum_t = cJSON_GetObjectItem(it, "humi_thres");
                            if (mode) ESP_LOGI(TAG, "mode=%g", mode->valuedouble);
                            if (state) ESP_LOGI(TAG, "state=%g", state->valuedouble);
                            if (hum_t) ESP_LOGI(TAG, "hum_t=%g", hum_t->valuedouble);
                            *device_state = state ? state->valuedouble : 0;
                            *device_mode  = mode  ? mode->valuedouble  : 0;
                            *device_hum_thres  = hum_t  ? hum_t->valuedouble  : 0;
                            break;
                        }
                    }
                }
                cJSON_Delete(root);
            } else {
                ESP_LOGW(TAG, "JSON parse failed");
            }
        }
    } else {
        ESP_LOGE(TAG, "GET failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    free(acc.buf);
    return err;
}

void Libs_FireBaseInit()
{
    sntp_sync_time_test();
}

/* time format yyyy:mm:dd:hh:mm:ss */
void Libs_FireBaseGetTime(uint8_t *time_buf, uint8_t *time_path) 
{
    time(&now);
    setenv("TZ", "CST-7", 1);
    tzset();
    localtime_r(&now, &timeinfo);
    timeinfo.tm_year += 1900;
    timeinfo.tm_mon += 1;
    sprintf((char *)time_buf, "%04d:%02d:%02d:%02d:%02d:%02d", timeinfo.tm_year, timeinfo.tm_mon, timeinfo.tm_mday,
                                                timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    sprintf((char *)time_path, "%04d%02d%02d%02d%02d%02d", timeinfo.tm_year, timeinfo.tm_mon, timeinfo.tm_mday,
                                                timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    ESP_LOGI("SNTP", "Current time in Vietnam: %d:%02d:%02d %02d:%02d:%02d", timeinfo.tm_year, timeinfo.tm_mon, timeinfo.tm_mday,
                                                timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
}