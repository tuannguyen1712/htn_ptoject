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
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "nvs.h"
#include "string.h"
#include "esp_err.h"

#include "Libs_nvsHandle.h"

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
static const char *TAG = "NVS_HANDLE";

/************************************
 * GLOBAL VARIABLES
 ************************************/

/************************************
 * STATIC FUNCTION PROTOTYPES
 ************************************/

/************************************
 * STATIC FUNCTIONS
 ************************************/

/************************************
 * GLOBAL FUNCTIONS
 ************************************/
void Libs_nvsInit() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );
}

esp_err_t Libs_nvsGetData(char* nameSpace, char *nvsKey, NVS_DATA nvsDataType, void *data) 
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(nameSpace, NVS_READWRITE, &nvs_handle);
    ESP_LOGI(TAG, "Open nvs flash: %d", err);
    switch (nvsDataType)
    {
    case NVS_STRING:
        size_t len;
        /* Get length of string first */
        err = nvs_get_str(nvs_handle, nvsKey, NULL, &len);
        if (len > NVS_STR_MAXIMUM_SIZE || len == 0) 
        {
            len = 0;
            (*(char*)data) = '\0';  
            err = ESP_OK;
        }
        else 
        {
            /* Get string data with len */
            err = nvs_get_str(nvs_handle, nvsKey, (char*) data, &len);
        }
        break;
    case NVS_UINT8:
        err = nvs_get_u8(nvs_handle, nvsKey, (uint8_t*) data);
        break;
    case NVS_UINT16:
        err = nvs_get_u16(nvs_handle, nvsKey, (uint16_t*) data);
        break;
    case NVS_UINT32:
        err = nvs_get_u32(nvs_handle, nvsKey, (uint32_t*) data);
        break;
    default:
        break;
    }

    nvs_close(nvs_handle);
    return err;
}

esp_err_t Libs_nvsSetData(char* nameSpace, char *nvsKey, NVS_DATA nvsDataType, void *data)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(nameSpace, NVS_READWRITE, &nvs_handle);
    switch (nvsDataType)
    {
    case NVS_STRING:
        err = nvs_set_str(nvs_handle, nvsKey, (char*) data);
        break;
    case NVS_UINT8:
        err = nvs_set_u8(nvs_handle, nvsKey, (uint8_t) (*(uint8_t*) data));
        break;
    case NVS_UINT16:
        err = nvs_set_u16(nvs_handle, nvsKey, (uint16_t) (*(uint16_t*) data));
        break;
    case NVS_UINT32:
        err = nvs_set_u32(nvs_handle, nvsKey, (uint32_t) (*(uint32_t*) data));
        break;
    default:
        break;
    }

    nvs_close(nvs_handle);
    return err;
}
