#include "fake_hal.h"

#include <string.h>

#define FAKE_HAL_CAPTURE_CAPACITY (1024U)

void HAL_UART_TxCpltCallback(UART_HandleTypeDef* uart);
void HAL_UART_ErrorCallback(UART_HandleTypeDef* uart);
void HAL_UART_RxCpltCallback(UART_HandleTypeDef* uart);
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef* uart, uint16_t size);

static DMA_HandleTypeDef s_tx_dma;
static DMA_HandleTypeDef s_rx_dma;
static uint8_t* s_rx_buffer;
static uint8_t* s_rx_it_buffer;
static uint16_t s_rx_capacity;
static uint16_t s_rx_it_capacity;
static uint8_t s_tx_capture[FAKE_HAL_CAPTURE_CAPACITY];
static size_t s_tx_length = 0U;
static size_t s_abort_count = 0U;
static bool s_tx_auto_complete = true;
static HAL_StatusTypeDef s_tx_status = HAL_OK;
static HAL_StatusTypeDef s_rx_status = HAL_OK;
static HAL_UART_RxEventTypeTypeDef s_rx_event = HAL_UART_RXEVENT_IDLE;

UART_HandleTypeDef huart2 = {
    &s_tx_dma,
    &s_rx_dma
};

void fake_hal_reset(void)
{
    s_rx_buffer = NULL;
    s_rx_it_buffer = NULL;
    s_rx_capacity = 0U;
    s_rx_it_capacity = 0U;
    s_tx_length = 0U;
    s_abort_count = 0U;
    s_tx_auto_complete = true;
    s_tx_status = HAL_OK;
    s_rx_status = HAL_OK;
    s_rx_event = HAL_UART_RXEVENT_IDLE;
    (void)memset(s_tx_capture, 0, sizeof(s_tx_capture));
}

void fake_hal_set_tx_auto_complete(bool enabled)
{
    s_tx_auto_complete = enabled;
}

void fake_hal_set_tx_status(HAL_StatusTypeDef status)
{
    s_tx_status = status;
}

void fake_hal_set_rx_status(HAL_StatusTypeDef status)
{
    s_rx_status = status;
}

void fake_hal_receive(const uint8_t* data, size_t length, HAL_UART_RxEventTypeTypeDef event)
{
    size_t copied = (length < (size_t)s_rx_capacity) ? length : (size_t)s_rx_capacity;

    if ((s_rx_buffer == NULL) || ((data == NULL) && (copied > 0U))) {
        return;
    }
    if (copied > 0U) {
        (void)memcpy(s_rx_buffer, data, copied);
    }

    s_rx_event = event;
    HAL_UARTEx_RxEventCallback(&huart2, (uint16_t)length);
}

void fake_hal_receive_interrupt(const uint8_t* data, size_t length)
{
    size_t index;

    if ((data == NULL) && (length > 0U)) {
        return;
    }

    for (index = 0U; index < length; index++) {
        if ((s_rx_it_buffer == NULL) || (s_rx_it_capacity == 0U)) {
            return;
        }

        *s_rx_it_buffer = data[index];
        s_rx_it_buffer = NULL;
        s_rx_it_capacity = 0U;
        HAL_UART_RxCpltCallback(&huart2);
    }
}

void fake_hal_error(UART_HandleTypeDef* uart)
{
    HAL_UART_ErrorCallback(uart);
}

size_t fake_hal_transmitted_length(void)
{
    return s_tx_length;
}

size_t fake_hal_copy_transmitted(uint8_t* destination, size_t capacity)
{
    size_t copied = (capacity < s_tx_length) ? capacity : s_tx_length;

    if ((destination != NULL) && (copied > 0U)) {
        (void)memcpy(destination, s_tx_capture, copied);
    }

    return copied;
}

size_t fake_hal_abort_count(void)
{
    return s_abort_count;
}

void fake_hal_disable_dma_interrupt(DMA_HandleTypeDef* dma, uint32_t interrupt)
{
    (void)dma;
    (void)interrupt;
}

HAL_StatusTypeDef HAL_UART_Transmit_DMA(UART_HandleTypeDef* uart,
                                        uint8_t* data,
                                        uint16_t length)
{
    if ((uart == NULL) || (data == NULL) || (s_tx_status != HAL_OK)) {
        return HAL_ERROR;
    }

    if (((size_t)length <= (sizeof(s_tx_capture) - s_tx_length)) && s_tx_auto_complete) {
        (void)memcpy(&s_tx_capture[s_tx_length], data, (size_t)length);
        s_tx_length += (size_t)length;
        HAL_UART_TxCpltCallback(uart);
    }

    return HAL_OK;
}

HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef* uart,
                                    uint8_t* data,
                                    uint16_t length,
                                    uint32_t timeout)
{
    (void)timeout;

    if ((uart == NULL) || (data == NULL)) {
        return HAL_ERROR;
    }
    if (s_tx_status != HAL_OK) {
        return s_tx_status;
    }

    if ((size_t)length > (sizeof(s_tx_capture) - s_tx_length)) {
        return HAL_ERROR;
    }

    (void)memcpy(&s_tx_capture[s_tx_length], data, (size_t)length);
    s_tx_length += (size_t)length;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_UART_AbortTransmit(UART_HandleTypeDef* uart)
{
    if (uart == NULL) {
        return HAL_ERROR;
    }

    s_abort_count++;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_UARTEx_ReceiveToIdle_DMA(UART_HandleTypeDef* uart,
                                               uint8_t* data,
                                               uint16_t length)
{
    if ((uart == NULL) || (data == NULL) || (length == 0U) || (s_rx_status != HAL_OK)) {
        return HAL_ERROR;
    }

    s_rx_buffer = data;
    s_rx_capacity = length;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef* uart,
                                      uint8_t* data,
                                      uint16_t length)
{
    if ((uart == NULL) || (data == NULL) || (length == 0U) || (s_rx_status != HAL_OK)) {
        return HAL_ERROR;
    }

    s_rx_it_buffer = data;
    s_rx_it_capacity = length;
    return HAL_OK;
}

HAL_UART_RxEventTypeTypeDef HAL_UARTEx_GetRxEventType(const UART_HandleTypeDef* uart)
{
    (void)uart;
    return s_rx_event;
}
