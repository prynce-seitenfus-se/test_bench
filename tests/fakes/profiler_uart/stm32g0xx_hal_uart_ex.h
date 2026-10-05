#ifndef PROFILER_UART_TEST_HAL_UART_EX_H
#define PROFILER_UART_TEST_HAL_UART_EX_H

#include "stm32g0xx_hal_uart.h"

HAL_StatusTypeDef HAL_UARTEx_ReceiveToIdle_DMA(UART_HandleTypeDef* uart,
                                               uint8_t* data,
                                               uint16_t length);
HAL_UART_RxEventTypeTypeDef HAL_UARTEx_GetRxEventType(const UART_HandleTypeDef* uart);

#endif /* PROFILER_UART_TEST_HAL_UART_EX_H */
