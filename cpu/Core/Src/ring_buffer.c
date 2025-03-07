/**
 * @file ring_buffer.c
 * @brief Circular buffer (ring buffer) implementation with overwrite capability
 * @author dramirezbe
 * @date Feb 11, 2025
 * 
 * This implementation overwrites the oldest data when writing to a full buffer.
 * Suitable for embedded systems using STM32 HAL.
 */

#include "ring_buffer.h"
#include <stdint.h>

/**
 * @brief Initialize ring buffer structure
 * @param rb        Pointer to ring buffer instance
 * @param mem_add   Memory address for buffer storage
 * @param capacity  Maximum number of elements the buffer can hold
 */
void ring_buffer_init(ring_buffer_t *rb, uint8_t *mem_add, uint8_t capacity) {
    rb->buffer   = mem_add;
    rb->capacity = capacity;
    rb->head     = 0;
    rb->tail     = 0;
    rb->is_full  = 0;
}

/**
 * @brief Reset buffer to empty state
 * @param rb Pointer to ring buffer instance
 */
void ring_buffer_reset(ring_buffer_t *rb) {
    rb->head    = 0;
    rb->tail    = 0;
    rb->is_full = 0;
}

/**
 * @brief Calculate current number of elements in buffer
 * @param rb Pointer to ring buffer instance
 * @return Number of elements stored in buffer
 */
uint8_t ring_buffer_size(ring_buffer_t *rb) {
    if (rb->is_full) {
        return rb->capacity;
    }
    
    return (rb->head >= rb->tail) ? (rb->head - rb->tail) 
                                : (rb->capacity - rb->tail + rb->head);
}

/**
 * @brief Check if buffer is full
 * @param rb Pointer to ring buffer instance
 * @return 1 if full, 0 otherwise
 */
uint8_t ring_buffer_is_full(ring_buffer_t *rb) {
    return rb->is_full;
}

/**
 * @brief Check if buffer is empty
 * @param rb Pointer to ring buffer instance
 * @return 1 if empty, 0 otherwise
 */
uint8_t ring_buffer_is_empty(ring_buffer_t *rb) {
    return (!rb->is_full && (rb->head == rb->tail));
}

/**
 * @brief Write data to buffer (overwrites oldest data if full)
 * @param rb    Pointer to ring buffer instance
 * @param data  Byte to write to buffer
 * 
 * @note When buffer is full, writing new data will:
 *       1. Overwrite the oldest data
 *       2. Advance both head and tail pointers
 *       3. Maintain full state
 */
void ring_buffer_write(ring_buffer_t *rb, uint8_t data) {
    rb->buffer[rb->head] = data;
    rb->head = (rb->head + 1) % rb->capacity;

    if (rb->is_full) {
        rb->tail = (rb->tail + 1) % rb->capacity;
    }

    rb->is_full = (rb->head == rb->tail);
}

/**
 * @brief Read data from buffer
 * @param rb    Pointer to ring buffer instance
 * @param byte  Pointer to store read byte
 * @return 1 if read successful, 0 if buffer empty
 * 
 * @note Reading from buffer will:
 *       1. Clear full status if buffer was full
 *       2. Advance tail pointer
 *       3. Preserve data until overwritten
 */
uint8_t ring_buffer_read(ring_buffer_t *rb, uint8_t *byte) {
    if (ring_buffer_is_empty(rb)) {
        return 0;
    }

    *byte = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1) % rb->capacity;
    rb->is_full = 0;
    return 1;
}


/**
 * @brief Muestra los elementos actuales del ring buffer por UART.
 * @param rb        Puntero a la instancia del ring buffer.
 * @param huart     Puntero al manejador de UART para la transmisión.
 */
void show_rb(ring_buffer_t *rb, UART_HandleTypeDef *huart) {
    uint8_t size = ring_buffer_size(rb); // Obtener el número de elementos en el buffer

    // Transmitir los elementos por UART
    HAL_UART_Transmit(huart, (uint8_t *)"rb:", 3, 1000); // Encabezado

    // Mostrar los elementos del ring buffer
    for (uint8_t i = 0; i < size; i++) {
        uint8_t index = (rb->tail + i) % rb->capacity; // Calcular el índice circular
        HAL_UART_Transmit(huart, &rb->buffer[index], 1, 1000); // Transmitir cada byte
    }

    HAL_UART_Transmit(huart, (uint8_t *)"\r\n", 2, 1000); // Nueva línea
}


/**
 * @brief Busca una cadena en el ring buffer, y si se encuentra, lo resetea.
 * 
 * @param str Cadena de caracteres a buscar (debe estar terminada en NULL).
 * @param rb  Puntero a la instancia del ring buffer.
 * @return uint8_t Retorna 1 si se encontró la cadena y se borró el buffer, 0 si no se encontró.
 */
uint8_t check_string_in_rb(const char *str, ring_buffer_t *rb) {
    uint8_t rb_size = ring_buffer_size(rb);
    size_t str_len = strlen(str);

    // Si el string está vacío o el buffer tiene menos datos que la longitud del string, no hay coincidencia.
    if (str_len == 0 || rb_size < str_len) {
        return 0;
    }

    // Recorrer el buffer considerando la posición de tail y el comportamiento circular
    for (uint8_t i = 0; i <= rb_size - str_len; i++) {
        uint8_t match = 1;
        for (size_t j = 0; j < str_len; j++) {
            uint8_t index = (rb->tail + i + j) % rb->capacity;
            if (rb->buffer[index] != (uint8_t)str[j]) {
                match = 0;
                break;
            }
        }
        if (match) {
            // Se encontró la cadena, se borra el ring buffer
            ring_buffer_reset(rb);
            return 1;
        }
    }
    return 0;
}