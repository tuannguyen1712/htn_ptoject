/**
 ********************************************************************************
 * @file    Libs_I2c.h
 * @author  tuann
 * @date    Jan 26, 2025
 * @brief
 ********************************************************************************
 */

#ifndef LIBS_I2C_LIBS_I2C_H_
#define LIBS_I2C_LIBS_I2C_H_

#ifdef __cplusplus
extern "C" {
#endif

/************************************
 * INCLUDES
 ************************************/
#include "Wrappers_I2c.h"

/************************************
 * MACROS AND DEFINES
 ************************************/
#define Libs_I2c_Master_Transmit(DevAddr, pData, Size) \
        Wrappers_I2c_Master_Transmit(DevAddr, pData, Size)

#define Libs_I2c_Master_Reveive(DevAddr, pData, Size) \
        Wrappers_I2c_Master_Reveive(DevAddr, pData, Size)

#define Libs_I2c_Mem_Write(DevAddr, MemAddr, MemAddrSize, pData, Size) \
        Wrappers_I2c_Mem_Write(DevAddr, MemAddr, MemAddrSize, pData, Size)

/************************************
 * TYPEDEFS
 ************************************/

/************************************
 * EXPORTED VARIABLES
 ************************************/

/************************************
 * GLOBAL FUNCTION PROTOTYPES
 ************************************/


#ifdef __cplusplus
}
#endif

#endif /* LIBS_I2C_LIBS_I2C_H_ */
