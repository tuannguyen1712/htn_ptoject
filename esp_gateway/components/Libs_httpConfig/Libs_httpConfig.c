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
#include <inttypes.h>
#include <string.h>
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/param.h>
#include "esp_netif.h"
#include "esp_tls_crypto.h"
#include <esp_http_server.h>
#include "cJSON.h"

#include "Libs_httpConfig.h"

/************************************
 * EXTERN VARIABLES
 ************************************/
extern const uint8_t webserver_start[]              asm("_binary_web_server_html_start");
extern const uint8_t webserver_end[]                asm("_binary_web_server_html_end");
extern const uint8_t favicon_ico_start[]			asm("_binary_favicon_ico_start");
extern const uint8_t favicon_ico_end[]				asm("_binary_favicon_ico_end");

/************************************
 * PRIVATE MACROS AND DEFINES
 ************************************/

/************************************
 * PRIVATE TYPEDEFS
 ************************************/

/************************************
 * STATIC VARIABLES
 ************************************/
static httpd_handle_t http_server_handle = NULL;
static const char *TAG = "HTTP_SERVER";
static uint8_t *http_config_ssid;
static uint8_t *http_config_password;
static uint8_t http_config_ssid_len;
static uint8_t http_config_pass_len;
static uint16_t portInfo;

/************************************
 * GLOBAL VARIABLES
 ************************************/

/************************************
 * STATIC FUNCTION PROTOTYPES
 ************************************/
static esp_err_t http_web_server_html_handler(httpd_req_t *req);
static esp_err_t http_server_favicon_ico_handler(httpd_req_t *req);
static esp_err_t echo_post_handler(httpd_req_t *req);
static httpd_handle_t http_server_configure(void);

/************************************
 * STATIC FUNCTIONS
 ************************************/
static esp_err_t http_web_server_html_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "index.html requested");

	httpd_resp_set_type(req, "text/html");
	httpd_resp_send(req, (const char *)webserver_start, webserver_end - webserver_start);

	return ESP_OK;
}

static esp_err_t http_server_favicon_ico_handler(httpd_req_t *req)
{
	ESP_LOGI(TAG, "favicon.ico requested");

	httpd_resp_set_type(req, "image/x-icon");
	httpd_resp_send(req, (const char *)favicon_ico_start, favicon_ico_end - favicon_ico_start);

	return ESP_OK;
}

static esp_err_t echo_post_handler(httpd_req_t *req)
{
	char content[100]; 
	httpd_req_recv(req, content, sizeof(content));

	cJSON *json = cJSON_Parse(content); 
	if (json == NULL) 
	{ 
		printf("Error parsing JSON\n"); 
		return 1; 
	} 

	cJSON *ssid = cJSON_GetObjectItemCaseSensitive(json, "ssid"); 
	if (cJSON_IsString(ssid) && (ssid->valuestring != NULL)) 
	{ 
		// ESP_LOGI(TAG, "ssid %d %s ", strlen(ssid->valuestring), ssid->valuestring);
		http_config_ssid_len = strlen(ssid->valuestring);
		http_config_ssid = (uint8_t*) malloc(http_config_ssid_len + 1);
		if (http_config_ssid)
		{
			strcpy((char*) http_config_ssid, ssid->valuestring);
			ESP_LOGI(TAG, "sssid: %s", http_config_ssid);
		}
	} 
	
	cJSON *password = cJSON_GetObjectItemCaseSensitive(json, "password"); 
	if (cJSON_IsString(password) && (password->valuestring != NULL)) 
	{
		// ESP_LOGI(TAG, "ssid %d %s ", strlen(password->valuestring), password->valuestring); 
		http_config_pass_len = strlen(password->valuestring);
		http_config_password = (uint8_t*) malloc(http_config_pass_len + 1);
		if (http_config_password)
		{
			strcpy((char*) http_config_password, password->valuestring);
			ESP_LOGI(TAG, "password: %s", http_config_password);
		}
	} 

	cJSON *port = cJSON_GetObjectItemCaseSensitive(json, "port"); 
	if (cJSON_IsString(port) && (port->valuestring != NULL)) 
	{
		// ESP_LOGI(TAG, "ssid %d %s ", strlen(password->valuestring), password->valuestring); 
		portInfo = atoi(port->valuestring);
		ESP_LOGI(TAG, "port: %u", portInfo);
	} 

	ESP_LOGI(TAG, "Receive data:%s %s", ssid->valuestring,password->valuestring);
	
	cJSON_Delete(json);

	xEventGroupSetBits(xEventGroup, HTTP_EVENT_BIT);

	return ESP_OK;
}

static httpd_handle_t http_server_configure(void)
{
	// Generate the default configuration
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();

	config.recv_wait_timeout = 10;
	config.send_wait_timeout = 10;
	config.task_priority = 3;

	ESP_LOGI(TAG,
			"http_server_configure: Starting server on port: '%d' with task priority: '%d'",
			config.server_port,
			config.task_priority);

	// Start the httpd server
	if (httpd_start(&http_server_handle, &config) == ESP_OK)
	{
		ESP_LOGI(TAG, "http_server_configure: Registering URI handlers");

		httpd_uri_t index_html = {
				.uri = "/",
				.method = HTTP_GET,
				.handler = http_web_server_html_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &index_html);

		httpd_uri_t favicon_ico = {
				.uri = "/favicon.ico",
				.method = HTTP_GET,
				.handler = http_server_favicon_ico_handler,
				.user_ctx = NULL
		};
		httpd_register_uri_handler(http_server_handle, &favicon_ico);

		httpd_uri_t wifi_config = {
			.uri       = "/wifi-config",
			.method    = HTTP_POST,
			.handler   = echo_post_handler,
			.user_ctx  = "wifi_config"
		};
		httpd_register_uri_handler(http_server_handle, &wifi_config);

		return http_server_handle;
	}

	return NULL;
}

/************************************
 * GLOBAL FUNCTIONS
 ************************************/
void Libs_httpServerStart(void)
{
	if (http_server_handle == NULL)
	{
		http_server_handle = http_server_configure();
	}
}

void Libs_httpServerStop(void)
{
	if (http_server_handle != NULL)
	{
		httpd_stop(http_server_handle);
	}
	if (http_config_ssid) {
		free(http_config_ssid);
	}
	if (http_config_password) {
		free(http_config_password);
	}
}

void Libs_httpServerGetWifiCredentials(uint8_t *ssid_ptr, uint8_t *pass_ptr, 
                          uint8_t *ssid_len, uint8_t *pass_len)
{
	strncpy((char*) ssid_ptr, (char*) http_config_ssid, http_config_ssid_len);
	strncpy((char*) pass_ptr, (char*) http_config_password, http_config_pass_len);
	*ssid_len = http_config_ssid_len;
	*pass_len = http_config_pass_len;
}
uint32_t Libs_httpServerGetPort()
{
	return portInfo;
}
