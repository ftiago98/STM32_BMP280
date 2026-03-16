/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <stdlib.h>
#include "storage.h"
#include "bmp280.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/**
 * @brief Statemachine
 * Wir nutzen diese States für die weiteren Abfragen
 */
typedef enum {
	STATE_INITIALISATION,
	STATE_MAIN_MENU,
	STATE_MEASURING,
	STATE_WRITE_FLASH,
	STATE_FLASH_FULL,
	STATE_DELETE_FLASH,
	STATE_LOGGER_STOPPED,
	STATE_PRINT_DATA
} SystemState_t;

volatile SystemState_t currentState = STATE_INITIALISATION;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

COM_InitTypeDef BspCOMInit;

I2C_HandleTypeDef hi2c1;

RTC_HandleTypeDef hrtc;

uint32_t logginZyklus = 5000;

/* USER CODE BEGIN PV */

/*
 * Wir nutzen dies für die Eingabe des Users.
 */
extern UART_HandleTypeDef hcom_uart[];
uint8_t uart_rx_byte;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_RTC_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {

	/* USER CODE BEGIN 1 */

	/* USER CODE END 1 */

	HAL_Init();

	/* USER CODE BEGIN Init */

	/* USER CODE END Init */

	/* Configure the system clock */
	SystemClock_Config();

	/* USER CODE BEGIN SysInit */

	/* USER CODE END SysInit */

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	MX_I2C1_Init();
	MX_RTC_Init();
	/* USER CODE BEGIN 2 */

	/*
	 * Speichert die Eingabe des Users.
	 */
	uint8_t empfangenes_RX;

	/* USER CODE END 2 */

	/* Initialize leds */
	BSP_LED_Init(LED_GREEN);
	BSP_LED_Init(LED_BLUE);

	/* Initialize USER push-button, will be used to trigger an interrupt each time it's pressed.*/
	BSP_PB_Init(BUTTON_USER, BUTTON_MODE_EXTI);
	/* Initialize COM1 port (115200, 8 bits (7-bit data + 1 stop bit), no parity */
	BspCOMInit.BaudRate = 115200;
	BspCOMInit.WordLength = COM_WORDLENGTH_8B;
	BspCOMInit.StopBits = COM_STOPBITS_1;
	BspCOMInit.Parity = COM_PARITY_NONE;
	BspCOMInit.HwFlowCtl = COM_HWCONTROL_NONE;
	if (BSP_COM_Init(COM1, &BspCOMInit) != BSP_ERROR_NONE) {
		Error_Handler();
	}

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1) {
		/* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */

		/*
		 * 1. Wir erstellen einen SensorDatensatz welches das Format für die spätere speicherung im Flash-Speicher vorgibt.
		 */
		SensorDatensatz_t datensatz;

		/*
		 * Wir starten mit der Switch-Case Abfrage
		 */
		switch (currentState) {

		/*
		 * STATE_INITIALISATION
		 * Dieser State wird direkt nach der Deklaration gesetzt.
		 * Solange kein Sensor gefunden werden kann oder die benötigten Kalibrierungsdaten nicht ausgelesen werden können
		 * wird in diesem State gewartet und gesucht.
		 * Sollte der BMP280 nicht gefunden werden, wurde eine HAL_Delay vorgesehen damit die Konsole nicht gespammt wird
		 */
		case STATE_INITIALISATION:
			printf("\r\n--- Der BMP280 Sensor wird gesucht... ---\r\n");

			if (HAL_I2C_IsDeviceReady(&hi2c1, BMP280_I2C_ADDR, 3, 100)
					== HAL_OK) {
				printf("--- Der BMP280 Sensor wurde gefunden.---\r\n");
				BMP280_Init(&hi2c1);
				BMP280_ReadCalibration(&hi2c1);

				// Wir springen direkt ins Menü!
				currentState = STATE_MAIN_MENU;

			} else {
				printf(
						"FEHLER: BMP280 antwortet nicht! Pruefe die Kabel...\r\n");
				HAL_Delay(2000); // 2 Sekunden warten, dann probiert die Schleife es erneut
			}
			break;

		/*
		 * Der STATE_MAIN_MENU ist die Bedienkonsole des User
		 * Hier können Messwerte ausgelesen, gelöscht und gestartet werden!
		*/
		case STATE_MAIN_MENU:
			// Optisches Feedback: LEDs zurücksetzen, falls wir aus einem Fehler/Voll-State kommen
			BSP_LED_Off(LED_BLUE);
			BSP_LED_Off(LED_GREEN);

			printf("\r\n");
			printf(
					"======================================================\r\n");
			printf("              BMP280 DATENLOGGER MENUE\r\n");
			printf(
					"======================================================\r\n");
			printf(" [ 1 ] - Messung STARTEN\r\n");
			printf(" [ A ] - Gespeicherte Daten AUSGEBEN\r\n");
			printf(" [ X ] - Flash-Speicher LOESCHEN\r\n");
			printf(
					"======================================================\r\n");
			printf("Bitte waehlen: ");

			// Wir warten solange bis der User eine Taste drückt
			HAL_UART_Receive(&hcom_uart[COM1], &empfangenes_RX, 1,
			HAL_MAX_DELAY);

			// Auswertung der Eingabe
			if (empfangenes_RX == '1') {
				printf(
						"\r\n>>> LOGGING GESTARTET! (Druecke 'P' fuer Pause) <<<\r\n");
				currentState = STATE_MEASURING;
			} else if (empfangenes_RX == 'a' || empfangenes_RX == 'A') {
				EEPROM_PrintAllData();
				// Danach fängt die Schleife wieder oben im Menü an!
			} else if (empfangenes_RX == 'x' || empfangenes_RX == 'X') {
				currentState = STATE_DELETE_FLASH;
			} else {
				printf(
						"\r\n[!] Ungueltige Eingabe. Bitte versuche es erneut.\r\n");
			}
			break;

		/*
		* Hier wir die Messung gestartet.
		*/
		case STATE_MEASURING:
			if (HAL_UART_Receive(&hcom_uart[COM1], &empfangenes_RX, 1, 1)
					== HAL_OK) {
				if (empfangenes_RX == 'p' || empfangenes_RX == 'P') {
					printf("\r\n>>> LOGGING PAUSIERT <<<\r\n");
					currentState = STATE_MAIN_MENU;
					break;
				}
			}
			// -----------------------------

			// 1. Zeit und Datum auslesen
			RTC_TimeTypeDef sTime = { 0 };
			RTC_DateTypeDef sDate = { 0 };
			HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
			HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

			// 2. Rohdaten abfragen und umrechnen
			uint32_t raw_T, raw_P;
			BMP280_ReadRawData(&hi2c1, &raw_T, &raw_P);
			int32_t temp = BMP280_Compensate_T(raw_T);
			uint32_t press = BMP280_Compensate_P(raw_P);

			// 3. Konsolenausgabe
			printf(
					"[%02d.%02d.20%02d - %02d:%02d:%02d] Temp: %ld.%02ld C | Press: %ld.%02ld hPa\r\n",
					sDate.Date, sDate.Month, sDate.Year, sTime.Hours,
					sTime.Minutes, sTime.Seconds, temp / 100, abs(temp % 100),
					press / 25600, (press % 25600) / 256);

			// 4. Datenstruktur für die spätere Speicherung füllen
			datensatz.stunden = sTime.Hours;
			datensatz.minuten = sTime.Minutes;
			datensatz.sekunden = sTime.Seconds;
			datensatz.tag = sDate.Date;
			datensatz.monat = sDate.Month;
			datensatz.jahr = sDate.Year;
			datensatz.padding = 0;
			datensatz.temperatur = temp;
			datensatz.luftdruck = press;

			// Jetzt wechseln wir in den Speicher-State
			currentState = STATE_WRITE_FLASH;
			break;

		case STATE_WRITE_FLASH:
			// Wir versuchen, den Datensatz zu speichern
			uint8_t write_status = EEPROM_WriteSensorLog(&datensatz);

			if (write_status == 1) {
				currentState = STATE_FLASH_FULL;
			} else {
				// Speichern war erfolgreich. Kurzes Blinken der blauen LED als Indikator!
				BSP_LED_Toggle(LED_BLUE);

				HAL_Delay(logginZyklus); // 1 Sekunde warten bis zur nächsten Messung
				currentState = STATE_MEASURING;
			}
			break;

		case STATE_FLASH_FULL:
			printf("\r\n[!] SPEICHER IST VOLL! Aufzeichnung gestoppt.\r\n");

			// LEDs dauerhaft einschalten als Warnung
			BSP_LED_On(LED_BLUE);
			BSP_LED_On(LED_GREEN);

			// WICHTIG: Zurück ins Menü! Nur so kann der User 'a' zum Auslesen oder 'x' zum Löschen drücken.
			currentState = STATE_MAIN_MENU;
			break;

		case STATE_DELETE_FLASH:
			// Feedback für den User:
			printf("\r\n>>> LOESCHE FLASH SPEICHER... BITTE WARTEN! <<<\r\n");

			// Beide LEDs an als "Busy"-Signal
			BSP_LED_On(LED_BLUE);
			BSP_LED_On(LED_GREEN);

			// Den eigentlichen Hardware-Löschbefehl ausführen
			EEPROM_ErasePage();

			printf(">>> SPEICHER ERFOLGREICH GELOESCHT. <<<\r\n");

			// Kurz warten, damit der User die Nachricht lesen kann
			HAL_Delay(2000);


			currentState = STATE_MAIN_MENU;
			break;
		}
		/* USER CODE END 3 */
	}
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
	RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
	RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };

	__HAL_FLASH_SET_LATENCY(FLASH_LATENCY_0);

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI
			| RCC_OSCILLATORTYPE_LSI;
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;
	RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV4;
	RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
	RCC_OscInitStruct.LSIState = RCC_LSI_ON;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
			| RCC_CLOCKTYPE_PCLK1;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
	RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK) {
		Error_Handler();
	}
}

/**
 * @brief I2C1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_I2C1_Init(void) {

	/* USER CODE BEGIN I2C1_Init 0 */

	/* USER CODE END I2C1_Init 0 */

	/* USER CODE BEGIN I2C1_Init 1 */

	/* USER CODE END I2C1_Init 1 */
	hi2c1.Instance = I2C1;
	hi2c1.Init.Timing = 0x00402D41;
	hi2c1.Init.OwnAddress1 = 0;
	hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
	hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
	hi2c1.Init.OwnAddress2 = 0;
	hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
	hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
	hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
	if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
		Error_Handler();
	}

	/** Configure Analogue filter
	 */
	if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE)
			!= HAL_OK) {
		Error_Handler();
	}

	/** Configure Digital filter
	 */
	if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN I2C1_Init 2 */

	/* USER CODE END I2C1_Init 2 */

}

/**
 * @brief RTC Initialization Function
 * @param None
 * @retval None
 */
static void MX_RTC_Init(void) {

	/* USER CODE BEGIN RTC_Init 0 */

	/* USER CODE END RTC_Init 0 */

	RTC_TimeTypeDef sTime = { 0 };
	RTC_DateTypeDef sDate = { 0 };

	/* USER CODE BEGIN RTC_Init 1 */

	/* USER CODE END RTC_Init 1 */

	/** Initialize RTC Only
	 */
	hrtc.Instance = RTC;
	hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
	hrtc.Init.AsynchPrediv = 127;
	hrtc.Init.SynchPrediv = 255;
	hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
	hrtc.Init.OutPutRemap = RTC_OUTPUT_REMAP_NONE;
	hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
	hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
	hrtc.Init.OutPutPullUp = RTC_OUTPUT_PULLUP_NONE;
	if (HAL_RTC_Init(&hrtc) != HAL_OK) {
		Error_Handler();
	}

	/* USER CODE BEGIN Check_RTC_BKUP */

	/* USER CODE END Check_RTC_BKUP */

	/** Initialize RTC and set the Time and Date
	 */
	sTime.Hours = 0x0;
	sTime.Minutes = 0x0;
	sTime.Seconds = 0x0;
	sTime.SubSeconds = 0x0;
	sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
	sTime.StoreOperation = RTC_STOREOPERATION_RESET;
	if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BCD) != HAL_OK) {
		Error_Handler();
	}
	sDate.WeekDay = RTC_WEEKDAY_MONDAY;
	sDate.Month = RTC_MONTH_JANUARY;
	sDate.Date = 0x1;
	sDate.Year = 0x0;

	if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BCD) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN RTC_Init 2 */

	/* USER CODE END RTC_Init 2 */

}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
	/* USER CODE BEGIN MX_GPIO_Init_1 */

	/* USER CODE END MX_GPIO_Init_1 */

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOB_CLK_ENABLE();

	/* USER CODE BEGIN MX_GPIO_Init_2 */

	/* USER CODE END MX_GPIO_Init_2 */
}
/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1) {
	}
	/* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
