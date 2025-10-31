/*
 * Libs_Mq135.h
 *
 * Created: 12/5/2023 9:18:54 AM
 *  Author: tuann
 */

#ifndef MQ135_H_
#define MQ135_H_

#include <stdio.h>
#include <math.h>
#include "stdlib.h"

#define RL 		10				// kOhms
#define Vin 	3.3
#define Res 	4096	// Resolution
#define ATMOCO2 397.13

//Correction Values
#define CorrA 	-0.000002469136
#define CorrB 	0.00048148148
#define CorrC 	0.0274074074
#define CorrD 	1.37530864197
#define CorrE 	0.0019230769
#define R0 		76.63			//	Average value of R0 (kOhms)
/// Helper to calculate Voltage from Input
/// Voltage = input * Vin / (Resolution - 1)

typedef struct {
	uint16_t Acetone;
	uint16_t Alcohol;
	uint16_t CO;
	uint16_t CO2;
	uint16_t NH4;
	uint16_t Toluene;
} Air_param_t;

//struct MQ135 {
//	uint8_t pin;
//};

float MQ135_getVoltage(uint32_t ADC_val);
float MQ135_getCorrectionFactor(float temparature, float humidity);
float MQ135_getResistance(uint32_t ADC_val);
float MQ135_getCorrectResistance(float tem, float hum, uint32_t ADC_val);
// a and b dependent on type of gas
float MQ135_getPPM(float a, float b, uint32_t ADC_val);
float MQ135_getCorrectPPM(float a, float b, float tem, float hum,
		uint32_t ADC_val);
float MQ135_getPPMLinear(float a, float b, uint32_t ADC_val);
float MQ135_getAcetone(uint32_t ADC_val);
float MQ135_getCorrectAcetone(float tem, float hum, uint32_t ADC_val);
float MQ135_getAlcohol(uint32_t ADC_val);
float MQ135_getCorrectAlcohol(float tem, float hum, uint32_t ADC_val);
float MQ135_getCO2(uint32_t ADC_val);
float MQ135_getCorrectCO2(float tem, float hum, uint32_t ADC_val);
float MQ135_getCO(uint32_t ADC_val);
float MQ135_getCorrectCO(float tem, float hum, uint32_t ADC_val);
float MQ135_getNH4(uint32_t ADC_val);
float MQ135_getCorrectNH4(float tem, float hum, uint32_t ADC_val);
float MQ135_getToluene(uint32_t ADC_val);
float MQ135_getCorrectToluene(float tem, float hum, uint32_t ADC_val);
void getAQI_val(Air_param_t *aqi, uint32_t ADC_val);
void getAQI_Correctval(Air_param_t *aqi, int tem, int hum, uint32_t ADC_val);




#endif /* MQ135_H_ */
