#ifndef PROFILER_UART_TEST_FAKE_HAL_H
#define PROFILER_UART_TEST_FAKE_HAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "stm32g0xx_hal_uart_ex.h"

extern UART_HandleTypeDef huart2;

void fake_hal_reset(void);
void fake_hal_set_tx_auto_complete(bool enabled);
void fake_hal_set_tx_status(HAL_StatusTypeDef status);
void fake_hal_set_rx_status(HAL_StatusTypeDef status);
void fake_hal_receive(const uint8_t* data, size_t length, HAL_UART_RxEventTypeTypeDef event);
void fake_hal_receive_interrupt(const uint8_t* data, size_t length);
void fake_hal_error(UART_HandleTypeDef* uart);
size_t fake_hal_transmitted_length(void);
size_t fake_hal_copy_transmitted(uint8_t* destination, size_t capacity);
size_t fake_hal_abort_count(void);

#endif /* PROFILER_UART_TEST_FAKE_HAL_H */
