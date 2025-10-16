/**
 ********************************************************************************
 * @file    Libs_wifiSta.h
 * @author  TUANNM
 * @date    ${date}
 * @brief   
 ********************************************************************************
 */

#ifndef __LIBS_WIFI_STA_H__
#define __LIBS_WIFI_STA_H__

#ifdef __cplusplus
extern "C" {
#endif

/************************************
 * INCLUDES
 ************************************/
#include "Configuration.h"
#include "FreeRTOS/FreeRTOS.h"
#include "FreeRTOS/event_groups.h"

/************************************
 * MACROS AND DEFINES
 ************************************/
#define WIFI_CONNECT_SUCCESS    1
#define WIFI_CONNECT_FAILED     0
// #define MAXIMUM_RETRY           0xFFFF
#define MAXIMUM_RETRY           0x10

// #define MAXIMUM_RETRY           0x03

/************************************
 * TYPEDEFS
 ************************************/

/************************************
 * EXPORTED VARIABLES
 ************************************/

/************************************
 * GLOBAL FUNCTION PROTOTYPES
 ************************************/
void Libs_staSetEventGroupHandle(EventGroupHandle_t *handle);
void Libs_staInit(char *s, char *p);
void Libs_staDeinit();

void Libs_lwipInit();
void Libs_staGetWifiInfo(uint8_t *info, uint8_t *ssid, uint8_t *password);
void Libs_staSetWifiInfo(uint8_t *info, uint8_t *ssid, uint8_t *password);
uint8_t Libs_staCheckConnect(void);
void Libs_staGetApIpAddress(char *ip);

#ifdef __cplusplus
}
#endif

#endif 
