/*
 * storage.h
 *
 *  Created on: Mar 16, 2026
 *      Author: tfe
 */

#ifndef STORAGE_H_
#define STORAGE_H_

#include "main.h"

/* --- DEFINES --- */
#define FLASH_STORAGE_ADDR    0x0801F800

/**
 * @brief Datenstruktur für das EEPROM (16 Bytes Gesamtgröße).
 * * Die Struktur ist so optimiert, dass sie exakt in zwei
 * 64-Bit (Double-Word) Flash-Schreibvorgänge passt.
 * * Erstes Double-Word: Enthält Datum und Uhrzeit (6 Bytes) + Padding (2 Bytes),
 * damit das 8-Byte-Format eingehalten wird.
 * Zweites Double-Word: Enthält exakt die Messwerte (Temperatur 4 Bytes + Luftdruck 4 Bytes).
 */
typedef struct {
	uint8_t stunden;     // 1 Byte
	uint8_t minuten;     // 1 Byte
	uint8_t sekunden;    // 1 Byte
	uint8_t tag;         // 1 Byte (z.B. 15)
	uint8_t monat;       // 1 Byte (z.B. 3)
	uint8_t jahr;        // 1 Byte (z.B. 26 für 2026)
	uint16_t padding;     // 2 Bytes Füllbytes

	int32_t temperatur;  // 4 Bytes (Kompensierte Temperatur)
	uint32_t luftdruck;   // 4 Bytes (Kompensierter Luftdruck)
} SensorDatensatz_t;


/* --- FUNKTIONS-PROTOTYPEN --- */
uint8_t EEPROM_WriteSensorLog(SensorDatensatz_t *daten);
void EEPROM_ErasePage(void);
void EEPROM_PrintAllData(void);

#endif /* STORAGE_H_ */
