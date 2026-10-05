#include "unity.h"

#include <string.h>

#include "fake_hal.h"
#include "profiler_transport.h"

#define TEST_TX_CAPACITY (4U)
#define TEST_RX_CAPACITY (16U)

static uint8_t s_tx_buffer[TEST_TX_CAPACITY];
static uint8_t s_rx_buffer[TEST_RX_CAPACITY];
static ProfilerTransportConfig s_config;

void HAL_UART_TxCpltCallback(UART_HandleTypeDef* uart)
{
    (void)uart;
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef* uart, uint16_t size)
{
    (void)uart;
    (void)size;
}

void setUp(void)
{
    fake_hal_reset();
    (void)memset(s_tx_buffer, 0, sizeof(s_tx_buffer));
    (void)memset(s_rx_buffer, 0, sizeof(s_rx_buffer));

    s_config.device = &huart2;
    s_config.tx_buffer = s_tx_buffer;
    s_config.tx_buffer_size = sizeof(s_tx_buffer);
    s_config.rx_buffer = s_rx_buffer;
    s_config.rx_buffer_size = sizeof(s_rx_buffer);
    s_config.tx_timeout_ms = 1000U;
    s_config.tick_ms = NULL;
    s_config.yield = NULL;
}

void tearDown(void)
{
}

static void test_uart_it_init_validation(void)
{
    TEST_ASSERT_EQUAL(PROFILER_TRANSPORT_STATUS_INVALID_PARAM,
                      profiler_transport_init(NULL));
    s_config.device = NULL;
    TEST_ASSERT_EQUAL(PROFILER_TRANSPORT_STATUS_INVALID_PARAM,
                      profiler_transport_init(&s_config));
    s_config.device = &huart2;
    s_config.tx_timeout_ms = 0U;
    TEST_ASSERT_EQUAL(PROFILER_TRANSPORT_STATUS_INVALID_PARAM,
                      profiler_transport_init(&s_config));
}

static void test_uart_it_receive_frame(void)
{
    static const uint8_t command[] = "prof-dump\r\n";
    uint8_t received[TEST_RX_CAPACITY];
    size_t length = 0U;

    TEST_ASSERT_EQUAL(PROFILER_TRANSPORT_STATUS_OK,
                      profiler_transport_init(&s_config));
    TEST_ASSERT_EQUAL(PROFILER_TRANSPORT_STATUS_OK, profiler_transport_listen());
    fake_hal_receive_interrupt(command, sizeof(command) - 1U);

    TEST_ASSERT_EQUAL_UINT8(PROFILER_TRANSPORT_EVENT_FRAME,
                            profiler_transport_poll(&length));
    TEST_ASSERT_EQUAL_UINT32(sizeof(command) - 1U, length);
    TEST_ASSERT_EQUAL_UINT32(sizeof(command) - 1U,
                             stream_read(profiler_transport_stream(),
                                         received,
                                         sizeof(received)));
    TEST_ASSERT_EQUAL_MEMORY(command, received, sizeof(command) - 1U);
}

static void test_uart_it_receive_overrun(void)
{
    static const uint8_t bytes[] = "abcde";
    size_t length = 0U;

    s_config.rx_buffer_size = 4U;
    TEST_ASSERT_EQUAL(PROFILER_TRANSPORT_STATUS_OK,
                      profiler_transport_init(&s_config));
    TEST_ASSERT_EQUAL(PROFILER_TRANSPORT_STATUS_OK, profiler_transport_listen());
    fake_hal_receive_interrupt(bytes, sizeof(bytes) - 1U);

    TEST_ASSERT_EQUAL_UINT8(PROFILER_TRANSPORT_EVENT_FRAME |
                            PROFILER_TRANSPORT_EVENT_OVERRUN,
                            profiler_transport_poll(&length));
    TEST_ASSERT_EQUAL_UINT32(s_config.rx_buffer_size, length);
}

static void test_uart_it_transmit_chunks_and_latches_timeout(void)
{
    static const uint8_t data[] = "abcdefghij";
    uint8_t captured[sizeof(data) - 1U];

    TEST_ASSERT_EQUAL(PROFILER_TRANSPORT_STATUS_OK,
                      profiler_transport_init(&s_config));
    TEST_ASSERT_EQUAL_UINT32(sizeof(data) - 1U,
                             stream_write(profiler_transport_stream(),
                                          data,
                                          sizeof(data) - 1U));
    stream_flush(profiler_transport_stream());
    TEST_ASSERT_EQUAL_UINT32(sizeof(data) - 1U, fake_hal_transmitted_length());
    TEST_ASSERT_EQUAL_UINT32(sizeof(data) - 1U,
                             fake_hal_copy_transmitted(captured, sizeof(captured)));
    TEST_ASSERT_EQUAL_MEMORY(data, captured, sizeof(captured));

    fake_hal_reset();
    fake_hal_set_tx_status(HAL_TIMEOUT);
    TEST_ASSERT_EQUAL(PROFILER_TRANSPORT_STATUS_OK,
                      profiler_transport_init(&s_config));
    TEST_ASSERT_EQUAL_UINT32(0U, stream_write(profiler_transport_stream(), data, TEST_TX_CAPACITY));
    TEST_ASSERT_EQUAL(PROFILER_TRANSPORT_STATUS_TIMEOUT, profiler_transport_status());
}

static void test_uart_it_receive_start_failure(void)
{
    fake_hal_set_rx_status(HAL_ERROR);
    TEST_ASSERT_EQUAL(PROFILER_TRANSPORT_STATUS_OK,
                      profiler_transport_init(&s_config));
    TEST_ASSERT_EQUAL(PROFILER_TRANSPORT_STATUS_IO_ERROR,
                      profiler_transport_listen());
    TEST_ASSERT_EQUAL_UINT8(PROFILER_TRANSPORT_EVENT_ERROR,
                            profiler_transport_poll(NULL));
    TEST_ASSERT_EQUAL(PROFILER_TRANSPORT_STATUS_IO_ERROR,
                      profiler_transport_status());
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_uart_it_init_validation);
    RUN_TEST(test_uart_it_receive_frame);
    RUN_TEST(test_uart_it_receive_overrun);
    RUN_TEST(test_uart_it_transmit_chunks_and_latches_timeout);
    RUN_TEST(test_uart_it_receive_start_failure);
    return UNITY_END();
}
