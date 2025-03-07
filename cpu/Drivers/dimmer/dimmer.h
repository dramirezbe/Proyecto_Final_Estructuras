#ifndef DIMMER_H
#define DIMMER_H

#include "main.h"

// Inicializa variables internas del módulo dimmer (si se desea)
void Dimmer_Init(void);

// Funciones que deben llamarse desde los callbacks HAL
void Dimmer_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim);
void Dimmer_GPIO_EXTI_Callback(void);
void Dimmer_UART_RxCpltCallback(UART_HandleTypeDef *huart, uint8_t rx_data);

#endif /* DIMMER_H */
