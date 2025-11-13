#include "Libs_Mq135.h"
#include "stm32f4xx_hal.h"

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

float MQ135_getCorrectPPM(float a, float b, float tem, float hum,
		uint32_t ADC_val) {
	float ratio = MQ135_getCorrectResistance(tem, hum, ADC_val) / R0;
	float ppm = a * pow(ratio, b);
	return ppm;
}

float MQ135_getCorrectCO2(float tem, float hum, uint32_t ADC_val) {
	return MQ135_getCorrectPPM(110.47, -2.862, tem, hum, ADC_val) + ATMOCO2;
}
