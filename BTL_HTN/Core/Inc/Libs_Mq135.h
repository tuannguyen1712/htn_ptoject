#ifndef MQ135_H_
#define MQ135_H_

#include <stdio.h>
#include <math.h>
#include "stdlib.h"
#include "stm32f4xx_hal.h"

#define RL 		10				// kOhms
#define Vin 	3.3
#define Res 	4096			// Resolution
#define ATMOCO2 397.13

//Correction Values
#define CorrA 	-0.000002469136
#define CorrB 	0.00048148148
#define CorrC 	0.0274074074
#define CorrD 	1.37530864197
#define CorrE 	0.0019230769
#define R0 		76.63			//	Average value of R0 (kOhms)

float MQ135_getVoltage(uint32_t ADC_val);
float MQ135_getCorrectionFactor(float tem, float hum);
float MQ135_getResistance(uint32_t ADC_val);
float MQ135_getCorrectResistance(float tem, float hum, uint32_t ADC_val);
float MQ135_getCorrectPPM(float a, float b, float tem, float hum, uint32_t ADC_val);
float MQ135_getCorrectCO2(float tem, float hum, uint32_t ADC_val);

#endif /* MQ135_H_ */
