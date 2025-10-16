/**
 ********************************************************************************
 * @file    Libs_softAP.h
 * @author  TUANNM
 * @date    ${date}
 * @brief   
 ********************************************************************************
 */

#ifndef ___LIBS_SOFTAP_H__
#define ___LIBS_SOFTAP_H__

#ifdef __cplusplus
extern "C" {
#endif

/************************************
 * INCLUDES
 ************************************/
#include "Configuration.h"

/************************************
 * MACROS AND DEFINES
 ************************************/
#define AP_ESP_WIFI_SSID      "ESP32"
#define AP_ESP_WIFI_PASS      "12345678"
#define AP_ESP_WIFI_CHANNEL   2
#define AP_MAX_STA_CONN       3

/************************************
 * TYPEDEFS
 ************************************/

/************************************
 * EXPORTED VARIABLES
 ************************************/

/************************************
 * GLOBAL FUNCTION PROTOTYPES
 ************************************/
void Libs_softApInit(void);
void Libs_softApDeinit(void);


#ifdef __cplusplus
}
#endif

#endif 
