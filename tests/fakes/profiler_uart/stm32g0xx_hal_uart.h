#ifndef PROFILER_UART_TEST_HAL_UART_H
#define PROFILER_UART_TEST_HAL_UART_H

#include <stdint.h>

typedef struct DMA_HandleTypeDef {
    uint32_t reserved;
} DMA_HandleTypeDef;

typedef struct UART_HandleTypeDef {
    DMA_HandleTypeDef* hdmatx;
    DMA_HandleTypeDef* hdmarx;
} UART_HandleTypeDef;

typedef enum {
    HAL_OK = 0,
    HAL_ERROR,
    HAL_BUSY,
    HAL_TIMEOUT
} HAL_StatusTypeDef;

typedef enum {
    HAL_UART_RXEVENT_TC = 0U,
    HAL_UART_RXEVENT_HT = 1U,
    HAL_UART_RXEVENT_IDLE = 2U
} HAL_UART_RxEventTypeTypeDef;

#define DMA_IT_HT (0x01U)

void fake_hal_disable_dma_interrupt(DMA_HandleTypeDef* dma, uint32_t interrupt);
HAL_StatusTypeDef HAL_UART_Transmit_DMA(UART_HandleTypeDef* uart,
                                        uint8_t* data,
                                        uint16_t length);
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef* uart,
                                    uint8_t* data,
                                    uint16_t length,
                                    uint32_t timeout);
HAL_StatusTypeDef HAL_UART_AbortTransmit(UART_HandleTypeDef* uart);
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef* uart,
                                      uint8_t* data,
                                      uint16_t length);

#define __HAL_DMA_DISABLE_IT(dma, interrupt) \
    fake_hal_disable_dma_interrupt((dma), (interrupt))

#endif /* PROFILER_UART_TEST_HAL_UART_H */
