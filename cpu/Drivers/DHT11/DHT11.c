#include "DHT11.h"

// htim1 se utiliza para generar retardos en microsegundos; su inicialización se realiza en main
extern TIM_HandleTypeDef htim1;

// Función interna para retardos en microsegundos utilizando htim1
static void microDelay(uint16_t delay)
{
  __HAL_TIM_SET_COUNTER(&htim1, 0);
  while (__HAL_TIM_GET_COUNTER(&htim1) < delay);
}

/**
  * @brief  Inicia la comunicación con el sensor DHT11.
  * @retval 1 si el sensor responde correctamente, 0 en caso contrario.
  */
uint8_t DHT11_Start(void)
{
  uint8_t Response = 0;
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  // Configurar el pin como salida
  GPIO_InitStruct.Pin = DHT11_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(DHT11_GPIO_Port, &GPIO_InitStruct);

  // Enviar señal de inicio: poner el pin en bajo durante 20ms
  HAL_GPIO_WritePin(DHT11_GPIO_Port, DHT11_Pin, GPIO_PIN_RESET);
  uint32_t startTick = HAL_GetTick();
  while(HAL_GetTick() - startTick < 20) {} // Espera 20ms

  // Poner el pin en alto durante 30µs
  HAL_GPIO_WritePin(DHT11_GPIO_Port, DHT11_Pin, GPIO_PIN_SET);
  microDelay(30);

  // Configurar el pin como entrada con pull-up
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(DHT11_GPIO_Port, &GPIO_InitStruct);

  microDelay(40);
  if (!HAL_GPIO_ReadPin(DHT11_GPIO_Port, DHT11_Pin))
  {
    microDelay(80);
    if (HAL_GPIO_ReadPin(DHT11_GPIO_Port, DHT11_Pin))
      Response = 1;
  }
  // Espera a que el sensor finalice la respuesta (timeout de 2ms)
  startTick = HAL_GetTick();
  while (HAL_GPIO_ReadPin(DHT11_GPIO_Port, DHT11_Pin) && (HAL_GetTick() - startTick < 2)) {}

  return Response;
}

/**
  * @brief  Lee un byte del sensor DHT11.
  * @retval El byte leído.
  */
uint8_t DHT11_ReadByte(void)
{
  uint8_t i, byte = 0;
  for (i = 0; i < 8; i++)
  {
    // Esperar a que el pin se ponga en alto (timeout de 2ms)
    uint32_t startTick = HAL_GetTick();
    while(!HAL_GPIO_ReadPin(DHT11_GPIO_Port, DHT11_Pin) && (HAL_GetTick() - startTick < 2)) {}

    // Esperar 40µs para muestrear el bit
    microDelay(40);
    if (HAL_GPIO_ReadPin(DHT11_GPIO_Port, DHT11_Pin))
      byte |= (1 << (7 - i));

    // Esperar a que el pin vuelva a bajo (timeout de 2ms)
    startTick = HAL_GetTick();
    while(HAL_GPIO_ReadPin(DHT11_GPIO_Port, DHT11_Pin) && (HAL_GetTick() - startTick < 2)) {}
  }
  return byte;
}

/**
  * @brief  Lee los datos del DHT11 y los almacena en la estructura proporcionada.
  * @param  data: Puntero a una estructura DHT11_Data donde se guardarán los datos.
  * @retval 1 si la lectura es correcta, 0 en caso de error.
  */
uint8_t DHT11_ReadData(DHT11_Data *data)
{
  uint8_t checksum;
  if (DHT11_Start())
  {
    data->humidity_int = DHT11_ReadByte();
    data->humidity_dec = DHT11_ReadByte();
    data->temp_int     = DHT11_ReadByte();
    data->temp_dec     = DHT11_ReadByte();
    checksum           = DHT11_ReadByte();
    
    if (checksum == (data->humidity_int + data->humidity_dec + data->temp_int + data->temp_dec))
      return 1; // Datos válidos
  }
  return 0; // Error en la lectura
}

void DHT11_send_data(UART_HandleTypeDef *huart)
{
    char dht11_data[20];
    DHT11_Data dhtData;
    
    if (DHT11_ReadData(&dhtData))
    {
        // Enviar datos en formato: |tempC|humedad|
        sprintf(dht11_data, "|%d.%d|%d.%d|\r\n", 
                dhtData.temp_int, dhtData.temp_dec, 
                dhtData.humidity_int, dhtData.humidity_dec);
        HAL_UART_Transmit(huart, (uint8_t *)dht11_data, strlen(dht11_data), 1000);
    }
    else
    {
        sprintf(dht11_data, "Error\r\n");
        HAL_UART_Transmit(huart, (uint8_t *)dht11_data, strlen(dht11_data), 1000);
    }
}