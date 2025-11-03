/*
 * Libs_Mq135.cpp
 *
 * Created: 12/5/2023 9:24:09 AM
 *  Author: tuann
 */

#include "Libs_Mq135.h"

float Vs = (float) Vin / (Res - 1);

float MQ135_getVoltage(uint32_t ADC_val) {
	return ADC_val * Vs;
}

float MQ135_getCorrectionFactor(float tem, float hum) {
	return (CorrA * pow(tem, 3) + CorrB * pow(tem, 2) - CorrC * tem + CorrD
			- (CorrE * hum - CorrE * 33));
}

float MQ135_getResistance(uint32_t ADC_val) {
	float vol = MQ135_getVoltage(ADC_val);
	float rs = ((Vin * RL) / vol) - RL;
	if (rs > 0) {
		return rs;
	} else
		return 0;
}

float MQ135_getCorrectResistance(float tem, float hum, uint32_t ADC_val) {
	return MQ135_getResistance(ADC_val) / MQ135_getCorrectionFactor(tem, hum);
}

// a and b dependent on type of gas
float MQ135_getPPM(float a, float b, uint32_t ADC_val) {
	float ratio = MQ135_getResistance(ADC_val) / R0;
	float ppm = a * pow(ratio, b);
	if (ppm > 0)
		return ppm;
	else
		return 0;
}

float MQ135_getCorrectPPM(float a, float b, float tem, float hum,
		uint32_t ADC_val) {
	float ratio = MQ135_getCorrectResistance(tem, hum, ADC_val) / R0;
	float ppm = a * pow(ratio, b);
	return ppm;
}

float MQ135_getPPMLinear(float a, float b, uint32_t ADC_val) {
	float ratio = MQ135_getResistance(ADC_val) / R0;
	float ppm_log = (log10(ratio) - b) / a;
	float ppm = pow(10, ppm_log);
	if (ppm > 0)
		return ppm;
	else
		return 0;
}

float MQ135_getAcetone(uint32_t ADC_val) {
	return MQ135_getPPM(34.688, -3.369, ADC_val);
}

float MQ135_getCorrectAcetone(float tem, float hum, uint32_t ADC_val) {
	return MQ135_getCorrectPPM(34.688, -3.369, tem, hum, ADC_val);
}

float MQ135_getAlcohol(uint32_t ADC_val) {
	return MQ135_getPPM(77.255, -3.18, ADC_val);
}

float MQ135_getCorrectAlcohol(float tem, float hum, uint32_t ADC_val) {
	return MQ135_getCorrectPPM(77.255, -3.18, tem, hum, ADC_val);
}

float MQ135_getCO2(uint32_t ADC_val) {
	return MQ135_getPPM(110.47, -2.862, ADC_val) + ATMOCO2;
}

float MQ135_getCorrectCO2(float tem, float hum, uint32_t ADC_val) {
	return MQ135_getCorrectPPM(110.47, -2.862, tem, hum, ADC_val) + ATMOCO2;
}

float MQ135_getCO(uint32_t ADC_val) {
	return MQ135_getPPM(605.18, -3.937, ADC_val);
}

float MQ135_getCorrectCO(float tem, float hum, uint32_t ADC_val) {
	return MQ135_getCorrectPPM(605.18, -3.937, tem, hum, ADC_val);
}

float MQ135_getNH4(uint32_t ADC_val) {
	return MQ135_getPPM(102.2, -2.473, ADC_val);
}

float MQ135_getCorrectNH4(float tem, float hum, uint32_t ADC_val) {
	return MQ135_getCorrectPPM(102.2, -2.473, tem, hum, ADC_val);
}

float MQ135_getToluene(uint32_t ADC_val) {
	return MQ135_getPPM(44.947, -3.445, ADC_val);
}

float MQ135_getCorrectToluene(float tem, float hum, uint32_t ADC_val) {
	return MQ135_getCorrectPPM(44.947, -3.445, tem, hum, ADC_val);
}

void getAQI_val(Air_param_t *aqi, uint32_t ADC_val) {
	aqi->Acetone = MQ135_getAcetone(ADC_val);
	aqi->Alcohol = MQ135_getAlcohol(ADC_val);
	aqi->CO = MQ135_getCO(ADC_val);
	aqi->CO2 = MQ135_getCO2(ADC_val);
	aqi->NH4 = MQ135_getNH4(ADC_val);
	aqi->Toluene = MQ135_getToluene(ADC_val);
}

void getAQI_Correctval(Air_param_t *aqi, int tem, int hum, uint32_t ADC_val) {
	aqi->Acetone = MQ135_getCorrectAcetone(tem, hum, ADC_val);
	aqi->Alcohol = MQ135_getCorrectAlcohol(tem, hum, ADC_val);
	aqi->CO = MQ135_getCorrectCO(tem, hum, ADC_val);
	aqi->CO2 = MQ135_getCorrectCO2(tem, hum, ADC_val);
	aqi->NH4 = MQ135_getCorrectNH4(tem, hum, ADC_val);
	aqi->Toluene = MQ135_getCorrectToluene(tem, hum, ADC_val);
}

