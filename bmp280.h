/*
 * BMP280.h
 *
 *  Created on: Mar 16, 2026
 *      Author: tfe
 */


#ifndef BMP280_H_
#define BMP280_H_

#include "main.h"

/** * @name BMP280 Sensor Configuration
 * @{
 */
#define BMP280_I2C_ADDR       0xEE       // Adresse des Sensors
#define BMP280_REG_CTRL_MEAS  0xF4       // Register zum Initalisieren der Messung (Siehe 4.3.4 Register 0xF4 "ctrl_meas")
#define BMP280_REG_DATA_START 0xF7       // Startadresse der Rohdaten (Siehe 4.36 Register 0xF7 -> 0xF9 "press")
/** @} */

typedef struct {
	uint16_t dig_T1;
	int16_t dig_T2;
	int16_t dig_T3;
	uint16_t dig_P1;
	int16_t dig_P2;
	int16_t dig_P3;
	int16_t dig_P4;
	int16_t dig_P5;
	int16_t dig_P6;
	int16_t dig_P7;
	int16_t dig_P8;
	int16_t dig_P9;
} BMP280_CalibData;

void BMP280_Init(I2C_HandleTypeDef *hi2c);
void BMP280_ReadCalibration(I2C_HandleTypeDef *hi2c);
void BMP280_ReadRawData(I2C_HandleTypeDef *hi2c, uint32_t *raw_temp, uint32_t *raw_press);
int32_t BMP280_Compensate_T(int32_t adc_T);
uint32_t BMP280_Compensate_P(int32_t adc_P);

#endif /* BMP280_H_ */
