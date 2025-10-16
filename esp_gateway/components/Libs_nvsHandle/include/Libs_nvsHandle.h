/**
 ********************************************************************************
 * @file    Libs_nvsHandle.h
 * @author  TUANNM
 * @date    ${date}
 * @brief   
 ********************************************************************************
 */

#ifndef __LIBS_NVSHANDLE_H__
#define __LIBS_NVSHANDLE_H__

#ifdef __cplusplus
extern "C" {
#endif

/************************************
 * INCLUDES
 ************************************/
#include <stdio.h>
#include "Configuration.h"

/************************************
 * MACROS AND DEFINES
 ************************************/
#define NVS_STR_MAXIMUM_SIZE        256

/************************************
 * TYPEDEFS
 ************************************/
typedef enum {
    NVS_STRING             = 0,
    NVS_UINT8              = 1,   
    NVS_UINT16             = 2,
    NVS_UINT32             = 3
    /* TBD */
}NVS_DATA;

/************************************
 * EXPORTED VARIABLES
 ************************************/

/************************************
 * GLOBAL FUNCTION PROTOTYPES
 ************************************/
void Libs_nvsInit(void);
esp_err_t Libs_nvsGetData(char* nameSpace, char *nvsKey, NVS_DATA nvsDataType, void *data);
esp_err_t Libs_nvsSetData(char* nameSpace, char *nvsKey, NVS_DATA nvsDataType, void *data);


#ifdef __cplusplus
}
#endif

#endif 
