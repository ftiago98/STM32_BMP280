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
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/** * @name BMP280 Sensor Configuration
 * @{
 */
#define BMP280_I2C_ADDR       0xEC       // Adresse des Sensors (0x76 << 1). Siehe Datasheet Kapitel 5.2
#define BMP280_REG_CTRL_MEAS  0xF4       // Register zum Initalisieren der Messung (Siehe 4.3.4 Register 0xF4 "ctrl_meas")
#define BMP280_REG_DATA_START 0xF7       // Startadresse der Rohdaten (Siehe 4.36 Register 0xF7 -> 0xF9 "press")
/** @} */

/** * @name Storage Configuration
 * @{
 */
#define FLASH_STORAGE_ADDR    0x0801F800 // Die letzte Seite im Flash (Siehe reference manual RM0490)
/** @} */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

COM_InitTypeDef BspCOMInit;

I2C_HandleTypeDef hi2c1;

RTC_HandleTypeDef hrtc;

/* USER CODE BEGIN PV */

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

/**
 * @brief Initialisiert den BMP280 Sensor mit Standardeinstellungen.
 * * Diese Funktion Initialisiert den Sensor - ohne diese Initialsierung würde der Sensor im "Sleep Mode" bleiben
 */

void BMP280_SimpleInit(void) {
	/** Konfiguration des 'ctrl_meas' Registers (0xF4):
	     * Wir senden den Wert 0x27 (Binär: 001 001 11).
	     * - Bits 7,6,5 (001): Temperatur-Oversampling x1
	     * - Bits 4,3,2 (001): Luftdruck-Oversampling x1
	     * - Bits 1,0   (11) : Power-Modus "Normal" (Sensor misst dauerhaft selbstständig)
	     * Siehe BMP280 Datenblatt Kapitel 3.6.2 für die Bit-Belegung.
	*/
    uint8_t init_config = 0x27;

    /* * HAL_I2C_Mem_Write schreibt den Konfigurationswert direkt in den Sensor:
         * - &hi2c1: Die I2C-Schnittstelle unseres STM32 -> Wurde durch die IDE generiert
         * - BMP280_I2C_ADDR: Die Adresse des Sensors (0xEC)
         * - BMP280_REG_CTRL_MEAS: Das Ziel-Register im Sensor (0xF4)
         * - 1: Die Größe der Register-Adresse (1 Byte)
         * - &init_config: Zeiger auf unseren Einstellungs-Wert (0x27)
         * - 1: Wir senden nur 1 Byte an Daten
         * - HAL_MAX_DELAY: Timeout-Schutz, falls der I2C-Bus blockiert ist!
         *
         * Quelle: https://sourcevu.sysprogs.com/stm32/HAL/symbols/HAL_I2C_Mem_Write
     */

    HAL_I2C_Mem_Write(&hi2c1, BMP280_I2C_ADDR, BMP280_REG_CTRL_MEAS, 1, &init_config, 1, HAL_MAX_DELAY);
}


/**
 * @brief Liest die Rohdaten für Druck und Temperatur in einem burst aus.
 * @param raw_temp  Pointer auf die Variable, in die die Temperatur gespeichert wird.
 * @param raw_press Pointer auf die Variable, in die der Luftdruck gespeichert wird.
 * Es wird mit jeweils mit einem * (Pointer) gearbeitet, somit kann die Funktion mehr als eine Variable zurückgeben
 * Der Pointer Referenziert auf die Adressen der Variablen und schreibt diese dann dort direkt rein
 */

void BMP280_ReadRawData(uint32_t *raw_temp, uint32_t *raw_press) {
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
	uint8_t data[6];

	/* 2. Den "Burst Read" ausführen.
	 * -> Fang bei Register 0xF7 an und gib mir die nächsten 6 Bytes.
	 * Siehe Datenblatt Kapitel 3.9 "Data readout".
	 *
	 * Quelle: https://sourcevu.sysprogs.com/stm32/HAL/symbols/HAL_I2C_Mem_Read
	 */

    HAL_I2C_Mem_Read(&hi2c1, BMP280_I2C_ADDR, BMP280_REG_DATA_START, 1, data, 6, HAL_MAX_DELAY);

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

    *raw_press = (data[0] << 12) | (data[1] << 4) | (data[2] >> 4);
    *raw_temp  = (data[3] << 12) | (data[4] << 4) | (data[5] >> 4);

}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
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

  // Wir erstellen zwei Platzhalter für die speicherung der Rohdaten
  uint32_t raw_temperature = 0;
  uint32_t raw_pressure = 0;

  // Wir Initalisieren den BMP280 Sensor
  BMP280_SimpleInit();

  /* USER CODE END 2 */

  /* Initialize leds */
  BSP_LED_Init(LED_GREEN);
  BSP_LED_Init(LED_BLUE);

  /* Initialize USER push-button, will be used to trigger an interrupt each time it's pressed.*/
  BSP_PB_Init(BUTTON_USER, BUTTON_MODE_EXTI);

  /* Initialize COM1 port (115200, 8 bits (7-bit data + 1 stop bit), no parity */
  BspCOMInit.BaudRate   = 115200;
  BspCOMInit.WordLength = COM_WORDLENGTH_8B;
  BspCOMInit.StopBits   = COM_STOPBITS_1;
  BspCOMInit.Parity     = COM_PARITY_NONE;
  BspCOMInit.HwFlowCtl  = COM_HWCONTROL_NONE;
  if (BSP_COM_Init(COM1, &BspCOMInit) != BSP_ERROR_NONE)
  {
    Error_Handler();
  }

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

	// Rohdaten abfragen
	BMP280_ReadRawData(&raw_temperature, &raw_pressure);

	// 100 Millisekunden warten
	HAL_Delay(100);

	//Werte über den Virtuellen Com Port ausegben (Test)
	printf("T=%ld.%02ld C | P=%lu.%02lu hPa\r\n", raw_temperature, raw_pressure);

  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_FLASH_SET_LATENCY(FLASH_LATENCY_0);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV4;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0*/

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
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
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
static void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef sDate = {0};

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
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
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
  if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }
  sDate.WeekDay = RTC_WEEKDAY_MONDAY;
  sDate.Month = RTC_MONTH_JANUARY;
  sDate.Date = 0x1;
  sDate.Year = 0x0;

  if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BCD) != HAL_OK)
  {
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
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
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
