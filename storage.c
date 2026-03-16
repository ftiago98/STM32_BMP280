/**
 * @file storage.c
 * @brief Implementierung der Flash-basierten Datenspeicherung.
 * @author tfe
 * @date March 16, 2026
 *
 * Dieses Modul speichert die Daten im Flash-Speicher des STM32.
 * Die Daten werden in 16-Byte Blöcken (entspricht der Grösse von SensorDatensatz_t)
 * gespeichert. Es nutzt das HAL-Flash-Interface für Page-Erase und
 * Double-Word-Programming (64-Bit).
 */

#include "storage.h"
#include <stdio.h>
#include <stdlib.h>


/**
 * @brief Sucht den nächsten freien Platz im Flash und speichert einen Datensatz.
 * * Die Funktion scannt die Flash-Seite in 16-Byte Schritten. Ein freier Platz
 * wird durch den Wert 0xFFFFFFFFFFFFFFFF (gelöschter Flash-Zustand) identifiziert.
 * Da der STM32C0 Flash nur in 64-Bit Einheiten (Double-Word) programmiert werden kann,
 * wird die 16-Byte Struktur in zwei Schreibvorgängen abgelegt.
 * * @param daten Pointer auf die SensorDatensatz_t Struktur, die gespeichert werden soll.
 * @return uint8_t 0 bei Erfolg, 1 wenn der Speicher (die Seite) voll ist.
 */
uint8_t EEPROM_WriteSensorLog(SensorDatensatz_t *daten) {
	uint32_t current_addr = FLASH_STORAGE_ADDR;
	uint32_t end_addr = FLASH_STORAGE_ADDR + 2048; // Ende der 2KB Seite

	/* 1. Lineare Suche nach dem nächsten freien 16-Byte Slot */
	while (current_addr < end_addr) {
		/* Wir prüfen die ersten 8 Bytes des Slots auf den Leer-Zustand */
		uint64_t speicher_inhalt = *(__IO uint64_t*) current_addr;

		if (speicher_inhalt == 0xFFFFFFFFFFFFFFFF) {
			/* Freier Slot gefunden: Schreibvorgang vorbereiten */
			HAL_FLASH_Unlock();

			/* Die Struktur wird für den HAL-Befehl als 64-Bit Array interpretiert */
			uint64_t *daten_zum_speichern = (uint64_t*) daten;

			/* Erstes Double-Word schreiben (Stunden bis Jahr + Padding) */
			HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, current_addr,
					daten_zum_speichern[0]);

			/* Zweites Double-Word schreiben (Temperatur und Luftdruck) */
			HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, current_addr + 8,
					daten_zum_speichern[1]);

			HAL_FLASH_Lock();
			return 0; // Erfolg
		}
		current_addr += 16; //Platz bereits belegt, springe um 16 bits.
	}

	/* Seite ist komplett belegt */
	return 1; // Speicher voll
}

/**
 * @brief Löscht die dedizierte Flash-Seite (Page 63) komplett.
 * * Ein Erase setzt alle Bits der Seite zurück auf 1 (0xFF). Dies ist zwingend
 * erforderlich, bevor ein bereits beschriebener Bereich neu programmiert werden kann.
 */
void EEPROM_ErasePage(void) {
	HAL_FLASH_Unlock();

	FLASH_EraseInitTypeDef EraseInitStruct;
	uint32_t PageError = 0;

	/* Konfiguration für das Löschen einer einzelnen Seite */
	EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
	EraseInitStruct.Page = 63; // Unsere Speicherseite
	EraseInitStruct.NbPages = 1;

	/* Ausführung des Löschvorgangs durch die HAL-Extension */
	HAL_FLASHEx_Erase(&EraseInitStruct, &PageError);

	HAL_FLASH_Lock();
}

/**
 * @brief Liest alle validen Datensätze aus dem Flash und gibt sie via UART aus.
 * * Die Funktion iteriert über die Speicherseite, bis entweder das Ende der Seite
 * erreicht ist oder ein leerer Slot (0xFF...) gefunden wird, was das Ende der
 * Aufzeichnung markiert.
 */
void EEPROM_PrintAllData(void) {
	uint32_t current_addr = FLASH_STORAGE_ADDR;
	uint32_t end_addr = FLASH_STORAGE_ADDR + 2048;
	uint32_t eintraege = 0;

	printf("\r\n======================================================\r\n");
	printf("                GESPEICHERTE DATEN\r\n");
	printf("======================================================\r\n");

	// Wir gehen den Speicher in 16-Byte-Schritten durch (Grösse unseres Datensatzes)
	while (current_addr < end_addr) {

		// 1. Prüfen, ob der Speicherplatz leer ist (0xFFFFFFFFFFFFFFFF)
		uint64_t
		speicher_inhalt = *(__IO uint64_t*) current_addr;

		if (speicher_inhalt == 0xFFFFFFFFFFFFFFFF) {
			// Wir haben das Ende der aufgezeichneten Daten erreicht!
			break;
		}

		// 2. Den rohen Speicherbereich direkt in unsere Struktur umwandeln (Pointer-Magie!)
		SensorDatensatz_t *daten = (SensorDatensatz_t*) current_addr;

		// 3. Daten formatiert ausgeben (exakt wie im Mess-State)
		printf(
				"[%02d.%02d.20%02d - %02d:%02d:%02d] Temp: %ld.%02ld C | Press: %ld.%02ld hPa\r\n",
				daten->tag, daten->monat, daten->jahr, daten->stunden,
				daten->minuten, daten->sekunden, daten->temperatur / 100,
				abs(daten->temperatur % 100), daten->luftdruck / 25600,
				(daten->luftdruck % 25600) / 256);

		// 4. Zum nächsten Datensatz springen (+ 16 Bytes)
		current_addr += 16;
		eintraege++;
	}

	printf("======================================================\r\n");
	printf("-> %lu Datensaetze im Flash gefunden.\r\n", eintraege);
	printf("======================================================\r\n\n");
}
