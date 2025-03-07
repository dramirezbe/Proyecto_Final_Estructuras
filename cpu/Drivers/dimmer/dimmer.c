#include "dimmer.h"

// Variables internas del módulo (estáticas para encapsulación)
static volatile uint32_t i = 0;         // Contador del timer (cada 100 µs)
static volatile uint8_t cruce_cero = 0;   // Flag que se activa al detectar el cruce por cero
static uint32_t dim = 0;                // Valor que controla el disparo del triac (0 a 83)

/**
  * @brief  Inicializa las variables internas del módulo.
  * @note   Se puede llamar al inicio desde main().
  */
void Dimmer_Init(void)
{
  i = 0;
  cruce_cero = 0;
  dim = 0;
}

/**
  * @brief  Función a llamar desde HAL_TIM_PeriodElapsedCallback.
  * @param  htim: manejador del timer.
  */
void Dimmer_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if(htim->Instance == TIM2)
  {
    if (i >= dim)
    {
      // Dispara el triac (configurado en un pin de salida)
      HAL_GPIO_WritePin(TRIAC_PULSE_GPIO_Port, TRIAC_PULSE_Pin, GPIO_PIN_SET);
      i = 0;
      cruce_cero = 0;
    }
    else
    {
      i++;
    }
  }
}

/**
  * @brief  Función a llamar desde HAL_GPIO_EXTI_Callback.
  * @param  GPIO_Pin: Pin que generó la interrupción.
  */
void Dimmer_GPIO_EXTI_Callback()
{
    // Al detectar el rising edge (con configuración pull-up) se reinicia el contador
    // y se apaga el triac (se pone en LOW) para iniciar el retardo.
    cruce_cero = 1;
    i = 0;
    HAL_GPIO_WritePin(TRIAC_PULSE_GPIO_Port, TRIAC_PULSE_Pin, GPIO_PIN_RESET);
}

/**
  * @brief  Función a llamar desde HAL_UART_RxCpltCallback.
  * @param  huart: manejador del UART.
  *
  * Se espera que se reciba un dígito (carácter '0' a '9'). Se convierte ese dígito
  * en un valor entre 0 y 83, invertido (es decir, '0' => 83 y '9' => 0).
  */
void Dimmer_UART_RxCpltCallback(UART_HandleTypeDef *huart, uint8_t rx_data)
{
    // Se verifica que el byte recibido sea un dígito (0-9)
    if (rx_data >= '0' && rx_data <= '9')
    {
      uint8_t digit = rx_data - '0';  // Convierte ASCII a número (0-9)
      // Inversión y escalado: mapear 0->83 y 9->0
      // Alternativamente: dim = 83 - ((digit * 83) / 9);
      dim = 83 - ((digit * 83) / 9);
      
      char msgBuffer[50];
      sprintf(msgBuffer, "Nuevo valor de dim: %lu\r\n", dim);
      HAL_UART_Transmit(huart, (uint8_t *)msgBuffer, strlen(msgBuffer), 100);
    }
    // Rehabilita la recepción para el siguiente byte
    
}


