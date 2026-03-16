/**
 * @file bmp280.c
 * @brief Treiber-Implementierung für den Bosch BMP280 Sensor.
 * @author tfe
 * @date March 16, 2026
 * * Dieses Modul kapselt die I2C-Kommunikation und die mathematische Kompensation
 * der Rohdaten des BMP280. Die Implementierung basiert auf den Referenzformeln
 * des Bosch Datenblatts (Kapitel 3.11).
 *
 * WICHTIG: Der Hersteller weisst darauf hin die API für die umrechnung zu nutzen!
 *
 */

#include "bmp280.h"
#include <stdio.h>



/** * @brief Interne Struktur zur Speicherung der werksseitigen Trimm-Parameter.
 * Diese Werte sind individuell für jeden Sensor und werden einmalig beim Booten geladen.
 */
BMP280_CalibData calib;


/** * @brief Globaler Zwischenwert für die Temperaturkompensation.
 * Wird in BMP280_Compensate_T berechnet und zwingend für BMP280_Compensate_P benötigt.
 * Kapitel 3.11.3
 */
int32_t t_fine;

/**
 * @brief Versetzt den BMP280 vom Sleep-Mode in den Normal-Mode.
 * * Konfiguriert das Register 0xF4 (ctrl_meas):
 * - Temperatur-Oversampling: x1
 * - Druck-Oversampling: x1
 * - Mode: Normal (kontinuierliche Messung)
 * * @param hi2c Pointer auf den initialisierten I2C-Handle des STM32.
 */
void BMP280_Init(I2C_HandleTypeDef *hi2c) {
	/** Konfiguration des 'ctrl_meas' Registers (0xF4):
	 * Wir senden den Wert 0x27 (Binär: 001 001 11).
	 * - Bits 7,6,5 (001): Temperatur-Oversampling x1
	 * - Bits 4,3,2 (001): Luftdruck-Oversampling x1
	 * - Bits 1,0   (11) : Power-Modus "Normal" (Sensor misst dauerhaft selbstständig)
	 * Siehe BMP280 Datenblatt Kapitel 3.6.2 für die Bit-Belegung.
	 */
	uint8_t init_config = 0x27;

	/* * HAL_I2C_Mem_Write schreibt den Konfigurationswert direkt in den Sensor:
	 * - hi2c1: Die I2C-Schnittstelle unseres STM32 -> Wurde durch die IDE generiert
	 * - BMP280_I2C_ADDR: Die Adresse des Sensors (0xEC)
	 * - BMP280_REG_CTRL_MEAS: Das Ziel-Register im Sensor (0xF4)
	 * - 1: Die Größe der Register-Adresse (1 Byte)
	 * - &init_config: Zeiger auf unseren Einstellungs-Wert (0x27)
	 * - 1: Wir senden nur 1 Byte an Daten
	 * - HAL_MAX_DELAY: Timeout-Schutz, falls der I2C-Bus blockiert ist!
	 *
	 * Quelle: https://sourcevu.sysprogs.com/stm32/HAL/symbols/HAL_I2C_Mem_Write
	 */

	HAL_I2C_Mem_Write(hi2c, BMP280_I2C_ADDR, BMP280_REG_CTRL_MEAS, 1,
			&init_config, 1, HAL_MAX_DELAY);
}


/**
 * @brief Liest alle 24 Bytes der Kalibrierungs-Parameter aus dem Sensor.
 * * Die Parameter sind ab Adresse 0x88 als LSB/MSB Paare gespeichert.
 * Sie werden hier zu vorzeichenbehafteten oder vorzeichenlosen 16-Bit Werten kombiniert.
 * * @param hi2c Pointer auf den I2C-Handle.
 */
void BMP280_ReadCalibration(I2C_HandleTypeDef *hi2c) {
	uint8_t data[24] = { 0 }; // Füllen mit 0

	// Wir lesen 24 Bytes ab Register 0x88
	// Siehe Datenblatt Kapitel 3.11.1
	HAL_I2C_Mem_Read(hi2c, BMP280_I2C_ADDR, 0x88, 1, data, 24, HAL_MAX_DELAY);

	/* * Jedes Parameter-Paar besteht aus zwei 8-Bit Werten.
	 * Wir fügen sie hier zu 16-Bit Werten (int16_t oder uint16_t) zusammen.
	 * data[1] ist das MSB (höherwertig), data[0] das LSB (niederwertig).
	 */
	calib.dig_T1 = (data[1] << 8) | data[0];
	calib.dig_T2 = (data[3] << 8) | data[2];
	calib.dig_T3 = (data[5] << 8) | data[4];
	calib.dig_P1 = (data[7] << 8) | data[6];
	calib.dig_P2 = (data[9] << 8) | data[8];
	calib.dig_P3 = (data[11] << 8) | data[10];
	calib.dig_P4 = (data[13] << 8) | data[12];
	calib.dig_P5 = (data[15] << 8) | data[14];
	calib.dig_P6 = (data[17] << 8) | data[16];
	calib.dig_P7 = (data[19] << 8) | data[18];
	calib.dig_P8 = (data[21] << 8) | data[20];
	calib.dig_P9 = (data[23] << 8) | data[22];
}

/**
 * @brief Führt einen Burst-Read der Rohdaten für Temperatur und Druck aus.
 * * Liest 6 Bytes ab Register 0xF7. Die 20-Bit Werte werden aus jeweils
 * drei 8-Bit Registern (MSB, LSB, XLSB) zusammengesetzt.
 * * @param hi2c      Pointer auf den I2C-Handle.
 * @param raw_temp  Output-Pointer für den 20-Bit Temperatur-Rohwert.
 * @param raw_press Output-Pointer für den 20-Bit Druck-Rohwert.
 */
void BMP280_ReadRawData(I2C_HandleTypeDef *hi2c, uint32_t *raw_temp, uint32_t *raw_press) {
	/* 1. Ein Zwischenspeicher (Array) für die 6 Daten-Bytes erstellen.
	 * Der BMP280 liefert pro Messwert 20 Bit. Diese sind auf 3 Register (Bytes) verteilt.
	 * data[0-2] = Luftdruck (MSB, LSB, XLSB)
	 * data[3-5] = Temperatur (MSB, LSB, XLSB)
	 *
	 * bzw.
	 *
	 * data[0] = Luftdruck MSB
	 * data[1] = Luftdruck LSB
	 * data[2] = Luftdruck XLSB
	 * data[3] = Temperatur MSB
	 * data[4] = Temperatur LSB
	 * data[5] = Temperatur XLSB
	 */
	uint8_t data[6] = { 0 }; //Füllen mit 0

	/* 2. Den "Burst Read" ausführen.
	 * -> Fang bei Register 0xF7 an und gib mir die nächsten 6 Bytes.
	 * Siehe Datenblatt Kapitel 3.9 "Data readout".
	 *
	 * Quelle: https://sourcevu.sysprogs.com/stm32/HAL/symbols/HAL_I2C_Mem_Read
	 */

	HAL_I2C_Mem_Read(hi2c, BMP280_I2C_ADDR, BMP280_REG_DATA_START, 1, data, 6,
	HAL_MAX_DELAY);

	/* Wir erhalten ein 20-Bit Antwort, nun müssen wir dies jedoch noch richtig "zusammenbauen"
	 *
	 * Beispiel Luftdruck
	 *
	 * data[0]	->	MSB (Most Significant Byte)			=	Wichtigster Teil, somit ganz nach Links schieben
	 * data[1]	->	LSB (Least Significant Byte)		=	Mittlere Teil weil noch ein XLSB vorhanden ist
	 * data[2]	->	XLSB (Xtra Least Significant Byte)	=	Letzter Teil, ganz rechts
	 *
	 * Das Operationszeichen | fungiert hier zum zusammensetzten der Bits
	 *
	 */

	*raw_press = (((uint32_t) data[0]) << 12) | (((uint32_t) data[1]) << 4)
			| ((data[2] >> 4) & 0x0F);
	*raw_temp = (((uint32_t) data[3]) << 12) | (((uint32_t) data[4]) << 4)
			| ((data[5] >> 4) & 0x0F);
}


/**
 * @brief Berechnet die kompensierte Temperatur in Grad Celsius.
 * * Nutzt die Bosch-Formel zur Kompensation der thermischen Nichtlinearität.
 * Berechnet zusätzlich den globalen Wert @ref t_fine.
 * * @param adc_T 20-Bit Rohwert vom Sensor.
 * @return int32_t Temperatur im Format 'T * 100' (z.B. 2550 = 25.50 °C).
 */
int32_t BMP280_Compensate_T(int32_t adc_T) {
	int32_t var1, var2, T;
	var1 = ((((adc_T >> 3) - ((int32_t) calib.dig_T1 << 1)))
			* ((int32_t) calib.dig_T2)) >> 11;
	var2 = (((((adc_T >> 4) - ((int32_t) calib.dig_T1))
			* ((adc_T >> 4) - ((int32_t) calib.dig_T1))) >> 12)
			* ((int32_t) calib.dig_T3)) >> 14;
	t_fine = var1 + var2;
	T = (t_fine * 5 + 128) >> 8;
	return (uint32_t) T;
}

/**
 * @brief Berechnet den kompensierten Luftdruck in Pascal.
 * * Benötigt den aktuellen @ref t_fine Wert der Temperaturmessung.
 * Nutzt 64-Bit Arithmetik für maximale Präzision ohne Fließkommazahlen.
 * * @param adc_P 20-Bit Rohwert vom Sensor.
 * @return uint32_t Luftdruck im Q24.8 Format (Wert / 256 = hPa).
 */
uint32_t BMP280_Compensate_P(int32_t adc_P) {
	int64_t var1, var2, p;
	var1 = ((int64_t) t_fine) - 128000;
	var2 = var1 * var1 * (int64_t) calib.dig_P6;
	var2 = var2 + ((var1 * (int64_t) calib.dig_P5) << 17);
	var2 = var2 + (((int64_t) calib.dig_P4) << 35);
	var1 = ((var1 * var1 * (int64_t) calib.dig_P3) >> 8)
			+ ((var1 * (int64_t) calib.dig_P2) << 12);
	var1 = (((((int64_t) 1) << 47) + var1)) * ((int64_t) calib.dig_P1) >> 33;
	if (var1 == 0)
		return 0;
	p = 1048576 - adc_P;
	p = (((p << 31) - var2) * 3125) / var1;
	var1 = (((int64_t) calib.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
	var2 = (((int64_t) calib.dig_P8) * p) >> 19;
	p = ((p + var1 + var2) >> 8) + (((int64_t) calib.dig_P7) << 4);
	return (uint32_t) p;
}
