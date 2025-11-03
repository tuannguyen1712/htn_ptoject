/**
 ********************************************************************************
 * @file    Wrappers_I2c.h
 * @author  tuann
 * @date    Jan 26, 2025
 * @brief
 ********************************************************************************
 */

#ifndef WRAPPERS_I2C_H_
#define WRAPPERS_I2C_H_

#ifdef __cplusplus
extern "C" {
#endif

/************************************
 * INCLUDES
 ************************************/
#include "stm32f4xx_hal.h"

/************************************
 * EXPORTED VARIABLES
 ************************************/
extern I2C_HandleTypeDef hi2c1;

/************************************
 * MACROS AND DEFINES
 ************************************/
#define WRAPPERS_I2C_DEFAULT_TIMEOUT        100
#define WRAPPER_I2C_MAXIMUM_TIMEOUT         0xFFFFFFFFU

#define Wrappers_I2c_Master_Transmit(DevAddr, pData, Size) \
        HAL_I2C_Master_Transmit(&hi2c1, DevAddr << 1, pData,  Size, WRAPPERS_I2C_DEFAULT_TIMEOUT)

#define Wrappers_I2c_Master_Reveive(DevAddr, pData, Size) \
        HAL_I2C_Master_Receive(&hi2c1, DevAddr << 1, pData, Size, WRAPPERS_I2C_DEFAULT_TIMEOUT)

#define Wrappers_I2c_Mem_Write(DevAddr, MemAddr, MemAddrSize, pData, Size) \
        HAL_I2C_Mem_Write(&hi2c1, DevAddr << 1, MemAddr, MemAddrSize, pData, Size, WRAPPER_I2C_MAXIMUM_TIMEOUT)

/************************************
 * TYPEDEFS
 ************************************/


/************************************
 * GLOBAL FUNCTION PROTOTYPES
 ************************************/


#ifdef __cplusplus
}
#endif

#endif /* WRAPPERS_I2C_H_ */
