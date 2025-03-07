/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include "ring_buffer.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include "keypad.h"
#include "DHT11.h"
#include "dimmer.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;

UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;

/* USER CODE BEGIN PV */
volatile uint8_t press_count = 0;         // Contador de pulsaciones
volatile uint32_t first_press_time = 0;     // Tiempo de la primera pulsación

uint32_t door_tick = 0;
uint32_t door_status = 0;
uint8_t door_prev_status = 0;               // Guarda el estado previo para la lógica del botón

uint8_t pc_rx_data[BUFFER_CAPACITY];
ring_buffer_t pc_rx_buffer;

uint8_t keypad_rx_data[BUFFER_CAPACITY];
ring_buffer_t keypad_rx_buffer;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM1_Init(void);
static void MX_TIM2_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_I2C1_Init(void);
/* USER CODE BEGIN PFP */
void OLED_Write(char *text);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/*-----------------------------------INTERRUPTIONS---------------------------------*/
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if(htim->Instance == TIM2)
  {
    Dimmer_TIM_PeriodElapsedCallback(htim);
  }
  
}

/* Callback de la interrupción externa (EXTI) para el cruce por cero */
uint32_t key_pressed_tick = 0;
uint16_t column_pressed = 0;
uint32_t debounce_tick = 0;
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if(GPIO_Pin == ZERO_DETECT_Pin)
  {
    Dimmer_GPIO_EXTI_Callback();
  }
  uint32_t current_time = HAL_GetTick();

  if (GPIO_Pin == BUTTON_Pin)
  {
      // Debounce para B1: 100 ms
      static uint32_t b1_debounce_tick = 0;
      if ((current_time - b1_debounce_tick) < 100)
          return;
      b1_debounce_tick = current_time;
      
      // Procesamiento del botón B1
      if (press_count == 0)
      {
          first_press_time = current_time;
      }
      press_count++;
  }
  
  if(GPIO_Pin == COLUMN_1_Pin || GPIO_Pin == COLUMN_2_Pin || GPIO_Pin == COLUMN_3_Pin || GPIO_Pin == COLUMN_4_Pin) {
  
    // Debounce para el keypad: 200 ms
    static uint32_t keypad_debounce_tick = 0;
    if ((current_time - keypad_debounce_tick) < 200)
        return;
    keypad_debounce_tick = current_time;
    
    key_pressed_tick = current_time;
    column_pressed = GPIO_Pin;
  }
}

uint8_t rx_data;
uint8_t wifi_data;
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
  if (huart->Instance == USART2) {
    ring_buffer_write(&pc_rx_buffer, rx_data);
    show_rb(&pc_rx_buffer, &huart2);
    HAL_UART_Receive_IT(&huart2, &rx_data, 1);
  }
  else if (huart->Instance == USART3) {
    Dimmer_UART_RxCpltCallback(huart, wifi_data);
    
    HAL_UART_Transmit(&huart3, (uint8_t *)&wifi_data, 1, 1000);
    HAL_UART_Receive_IT(&huart3, (uint8_t *)&wifi_data, 1);
    //HAL_UART_Receive_IT(&huart2, (uint8_t *)&wifi_data, 1);
  }
}

/*------------------------------------MACROS------------------------------------*/

uint32_t heartbit_tick = 0;
void heartbit(void) {
  if (HAL_GetTick() - heartbit_tick >= 500) {
      HAL_GPIO_TogglePin(HEARTBIT_GPIO_Port, HEARTBIT_Pin);
      ssd1306_UpdateScreen();
      
      heartbit_tick = HAL_GetTick();
  }
}
void OLED_Write(char *text) {
  ssd1306_Fill(Black);
  ssd1306_SetCursor(17, 17);
  ssd1306_WriteString(text, Font_11x18, White);
}

/*-------------------------------------------------------FUNCTIONS----------------------------------*/

/**
 * @brief Máquina de estados para el control de la puerta.
 * Se guarda en door_prev_status el estado previo.
 * 0 = cerrado, 1 = abierto, 2 = abierto temp, 3 = inactivo
 */
void state_machine(void) {
  switch (door_status) {
    case 0: // Cerrado
      door_prev_status = 0;
      HAL_GPIO_WritePin(DOOR_GPIO_Port, DOOR_Pin, GPIO_PIN_RESET);
      HAL_UART_Transmit(&huart2, (uint8_t *)"Closed", 7, 1000);
      HAL_UART_Transmit(&huart2, (uint8_t *)"\r\n", 2, 1000);
      OLED_Write("Closed");
      door_status = 3;
      break;
    case 1: // Abierto
      door_prev_status = 1;
      HAL_GPIO_WritePin(DOOR_GPIO_Port, DOOR_Pin, GPIO_PIN_SET);
      HAL_UART_Transmit(&huart2, (uint8_t *)"Open", 4, 1000);
      HAL_UART_Transmit(&huart2, (uint8_t *)"\r\n", 2, 1000);
      OLED_Write("Open");
      door_status = 3;
      break;
    case 2: // Abierto temporal
      door_prev_status = 2;
      HAL_GPIO_WritePin(DOOR_GPIO_Port, DOOR_Pin, GPIO_PIN_SET);
      if (door_tick == 0) {
        door_tick = HAL_GetTick();
        HAL_UART_Transmit(&huart2, (uint8_t *)"Open", 4, 1000);
        HAL_UART_Transmit(&huart2, (uint8_t *)"\r\n", 2, 1000);
        OLED_Write("Open");
      }
      // Después de 5 segundos se cierra la puerta
      if (HAL_GetTick() - door_tick >= 5000) {
        HAL_GPIO_WritePin(DOOR_GPIO_Port, DOOR_Pin, GPIO_PIN_RESET);
        HAL_UART_Transmit(&huart2, (uint8_t *)"Closed", 7, 1000);
        HAL_UART_Transmit(&huart2, (uint8_t *)"\r\n", 2, 1000);
        OLED_Write("Closed");
        door_status = 3;
        door_tick = 0;
      }
      break;
    case 3: // Inactivo
      // No se realiza ninguna acción.
      break;
    default:
      break;
  }
  wifi_data = 14;
}

void handle_door_status(void) {
  if(check_string_in_rb(PASSWORD, &pc_rx_buffer) ||
    check_string_in_rb(PASSWORD, &keypad_rx_buffer) ||
    wifi_data == 12) {
    door_status = 2;
  }
  if(check_string_in_rb(OPEN_COMMAND, &pc_rx_buffer) ||
    check_string_in_rb(OPEN_COMMAND, &keypad_rx_buffer) ||
    wifi_data == 11) {
    door_status = 1;
  }
  if(check_string_in_rb(CLOSE_COMMAND, &pc_rx_buffer) ||
    check_string_in_rb(CLOSE_COMMAND, &keypad_rx_buffer) ||
    wifi_data == 10) {
    door_status = 0;
  }

  // Manejo de pulsaciones del botón usando EXTI y GetTick (sin delay)
  if (press_count > 0)
  {
    if (HAL_GetTick() - first_press_time > 500)
    {
      if (press_count == 2)
      {
        door_status = 1;  // Doble pulsación: se abre la puerta (estado 1)
      }
      else if (press_count == 1)
      {
        // Pulsación única: si la puerta estaba previamente abierta (door_prev_status == 1) se cierra,
        // en caso contrario entra en modo "abierto temp" (estado 2)
        door_status = (door_prev_status == 1) ? 0 : 2;
      }
      press_count = 0; // Reinicia el contador para la siguiente serie
    }
  }

    

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
  keypad_init();
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM1_Init();
  MX_TIM2_Init();
  MX_USART2_UART_Init();
  MX_USART3_UART_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */
  HAL_TIM_Base_Start_IT(&htim1);
  HAL_TIM_Base_Start_IT(&htim2);

  HAL_UART_Receive_IT(&huart2, (uint8_t *)&rx_data, 1);
  HAL_UART_Receive_IT(&huart3, (uint8_t *)&wifi_data, 1);

  char *msg_uart2 = "---Hello USART2---\r\n";
  HAL_UART_Transmit(&huart2, (uint8_t *)msg_uart2, strlen(msg_uart2), 1000);
  char *msg_uart3 = "---Hello USART3---\r\n";
  HAL_UART_Transmit(&huart3, (uint8_t *)msg_uart3, strlen(msg_uart3), 1000);

  ring_buffer_init(&pc_rx_buffer, pc_rx_data, sizeof(pc_rx_data));
  ring_buffer_init(&keypad_rx_buffer, keypad_rx_data, sizeof(keypad_rx_data));

  ssd1306_Init();
  OLED_Write(FW_VERSION);

  HAL_UART_Transmit(&huart2, (uint8_t *)FW_VERSION, strlen(FW_VERSION), 1000);
  HAL_UART_Transmit(&huart3, (uint8_t *)FW_VERSION, strlen(FW_VERSION), 1000);
  HAL_UART_Transmit(&huart2, (uint8_t *)"\n\r", 4, 1000);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  static uint32_t dht_tick = 0;
  while (1)
  {
    heartbit();

    if (HAL_GetTick() >= 10000)
    {
        if (HAL_GetTick() - dht_tick >= 5000)
        {
            dht_tick = HAL_GetTick();
            DHT11_send_data(&huart3);
        }
    }
    if (column_pressed != 0 && (key_pressed_tick + 5) < HAL_GetTick() ) {
      uint8_t key = keypad_scan(column_pressed);

      ring_buffer_write(&keypad_rx_buffer, key);
      //HAL_UART_Transmit(&huart2, (uint8_t *)&key, 1, 1000);
      show_rb(&keypad_rx_buffer, &huart2);
      HAL_UART_Receive_IT(&huart2, &rx_data, 1);

      column_pressed = 0;
    }

    handle_door_status();
    
    state_machine();
    
    
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  /* USER CODE END 3 */
  }
}
/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 9;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
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

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x00C08CCB;
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
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 71;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 65535;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 71;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 99;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, DOOR_Pin|HEARTBIT_Pin|ROW_1_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, TRIAC_PULSE_Pin|ROW_2_Pin|ROW_4_Pin|ROW_3_Pin
                          |DHT11_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : BUTTON_Pin */
  GPIO_InitStruct.Pin = BUTTON_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BUTTON_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : DOOR_Pin HEARTBIT_Pin */
  GPIO_InitStruct.Pin = DOOR_Pin|HEARTBIT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : TRIAC_PULSE_Pin */
  GPIO_InitStruct.Pin = TRIAC_PULSE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
  HAL_GPIO_Init(TRIAC_PULSE_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : ZERO_DETECT_Pin */
  GPIO_InitStruct.Pin = ZERO_DETECT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(ZERO_DETECT_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : COLUMN_1_Pin */
  GPIO_InitStruct.Pin = COLUMN_1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(COLUMN_1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : COLUMN_4_Pin */
  GPIO_InitStruct.Pin = COLUMN_4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(COLUMN_4_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : COLUMN_2_Pin COLUMN_3_Pin */
  GPIO_InitStruct.Pin = COLUMN_2_Pin|COLUMN_3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : ROW_1_Pin */
  GPIO_InitStruct.Pin = ROW_1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(ROW_1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : ROW_2_Pin ROW_4_Pin ROW_3_Pin */
  GPIO_InitStruct.Pin = ROW_2_Pin|ROW_4_Pin|ROW_3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : DHT11_Pin */
  GPIO_InitStruct.Pin = DHT11_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(DHT11_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI2_IRQn);

  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

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

#ifdef  USE_FULL_ASSERT
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
