#ifndef DHT11_H
#define DHT11_H

#include "main.h"  // Asegúrate de que este archivo defina DHT11_Pin y DHT11_GPIO_Port

// Estructura para almacenar los datos leídos del sensor
typedef struct {
    uint8_t humidity_int;
    uint8_t humidity_dec;
    uint8_t temp_int;
    uint8_t temp_dec;
} DHT11_Data;

// Prototipos de funciones públicas
uint8_t DHT11_Start(void);
uint8_t DHT11_ReadByte(void);
uint8_t DHT11_ReadData(DHT11_Data *data);
void DHT11_send_data(UART_HandleTypeDef *huart);

#endif // DHT11_H
