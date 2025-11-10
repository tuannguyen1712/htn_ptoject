/**
 ********************************************************************************
 * @file    Configuration.h
 * @author  TUANNM
 * @date    ${date}
 * @brief   
 ********************************************************************************
 */

#ifndef __CONFIGURATION_H__
#define __CONFIGURATION_H__

#ifdef __cplusplus
extern "C" {
#endif

/************************************
 * INCLUDES
 ************************************/
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include "esp_bit_defs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/mcpwm_prelude.h"

/************************************
 * MACROS AND DEFINES
 ************************************/
#define WIFI_EVENT_BIT                      BIT0
#define HTTP_EVENT_BIT                      BIT1
#define FIREBASE_EVENT_BIT                  BIT2

#define DATA_COMMING_EVENT_BIT              BIT3
#define MODE_CHECK_EVENT_BIT                BIT4
#define SYNC_EVENT_BIT                      BIT5

#define NVS_WIFI_NAMESPACE          (char*) "CAM"
#define NVS_WIFI_SSID               (char*) "CAM_SSID"
#define NVS_WIFI_PASS               (char*) "CAM_PASS"
#define NVS_WIFI_PORT               (char*) "CAM_PORT"    

/************************************
 * TYPEDEFS
 ************************************/

/************************************
 * EXPORTED VARIABLES
 ************************************/
extern EventGroupHandle_t xEventGroup;
extern uint16_t udpPort;
extern char hostIpAddr[20];
extern char macAddr[20];

/************************************
 * GLOBAL FUNCTION PROTOTYPES
 ************************************/


#ifdef __cplusplus
}
#endif

#endif 
