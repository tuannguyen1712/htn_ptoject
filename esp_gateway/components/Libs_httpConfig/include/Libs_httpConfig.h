/**
 ********************************************************************************
 * @file    Libs_httpConfig.h
 * @author  TUANNM
 * @date    ${date}
 * @brief   
 ********************************************************************************
 */

#ifndef __LIBS_HTTPCONFIG_H__
#define __LIBS_HTTPCONFIG_H__

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

/************************************
 * TYPEDEFS
 ************************************/

/************************************
 * EXPORTED VARIABLES
 ************************************/

/************************************
 * GLOBAL FUNCTION PROTOTYPES
 ************************************/
void Libs_httpServerStart(void);
void Libs_httpServerStop(void);
void Libs_httpServerGetWifiCredentials(uint8_t *ssid_ptr, uint8_t *pass_ptr, \
                          uint8_t *ssid_len, uint8_t *pass_len);
uint32_t Libs_httpServerGetPort();


#ifdef __cplusplus
}
#endif

#endif 
