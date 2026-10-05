#include "unity.h"

#include <string.h>

#include "fake_hal.h"
#include "profiler_port.h"
#include "profiler_transport.h"
#include "usart.h"

#define TEST_TX_CAPACITY (4U)
#define TEST_RX_CAPACITY (8U)

static uint8_t s_tx_buffer[TEST_TX_CAPACITY];
static uint8_t s_rx_buffer[TEST_RX_CAPACITY];
static uint32_t s_tick;
static ProfilerTransportConfig s_config;

static uint32_t test_tick_ms(void)
{
    uint32_t tick = s_tick;
    s_tick++;
    return tick;
}

static void test_yield_ms(uint32_t milliseconds)
{
    s_tick += milliseconds;
}

static void init_transport(void)
{
    s_config.device = &huart2;
    s_config.tx_buffer = s_tx_buffer;
    s_config.tx_buffer_size = sizeof(s_tx_buffer);
    s_config.rx_buffer = s_rx_buffer;
    s_config.rx_buffer_size = sizeof(s_rx_buffer);
    s_config.tx_timeout_ms = 3U;
    s_config.tick_ms = test_tick_ms;
    s_config.yield = test_yield_ms;
    TEST_ASSERT_EQUAL(PROFILER_TRANSPORT_STATUS_OK,
                      profiler_transport_init(&s_config));
}

void setUp(void)
{
    fake_hal_reset();
    (void)memset(s_tx_buffer, 0, sizeof(s_tx_buffer));
    (void)memset(s_rx_buffer, 0, sizeof(s_rx_buffer));
    s_tick = 0U;
}

void tearDown(void)
{
}

static void test_chunked_transmit_and_partial_flush(void)
{
    static const uint8_t payload[] = { 0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U };
    uint8_t transmitted[sizeof(payload)];
    const Stream* stream;

    init_transport();
    stream = profiler_transport_stream();

    TEST_ASSERT_EQUAL_UINT32(8U, stream_write(stream, payload, 8U));
    TEST_ASSERT_EQUAL_UINT32(8U, fake_hal_transmitted_length());
    TEST_ASSERT_EQUAL_UINT32(2U, stream_write(stream, &payload[8], 2U));
    stream_flush(stream);
    TEST_ASSERT_EQUAL_UINT32(sizeof(payload), fake_hal_transmitted_length());
    TEST_ASSERT_EQUAL_UINT32(sizeof(payload),
                             fake_hal_copy_transmitted(transmitted, sizeof(transmitted)));
    TEST_ASSERT_EQUAL_MEMORY(payload, transmitted, sizeof(payload));
    TEST_ASSERT_EQUAL(PROFILER_TRANSPORT_STATUS_OK, profiler_transport_status());
}

static void test_transmit_start_failure_is_reported(void)
{
    static const uint8_t payload[TEST_TX_CAPACITY] = { 0U, 1U, 2U, 3U };

    init_transport();
    fake_hal_set_tx_status(HAL_ERROR);
    TEST_ASSERT_EQUAL_UINT32(0U,
                             stream_write(profiler_transport_stream(),
                                          payload,
                                          sizeof(payload)));
    TEST_ASSERT_EQUAL(PROFILER_TRANSPORT_STATUS_IO_ERROR, profiler_transport_status());
}

static void test_transmit_timeout_aborts_and_latches_status(void)
{
    static const uint8_t payload[TEST_TX_CAPACITY] = { 0U, 1U, 2U, 3U };

    init_transport();
    fake_hal_set_tx_auto_complete(false);
    TEST_ASSERT_EQUAL_UINT32(0U,
                             stream_write(profiler_transport_stream(),
                                          payload,
                                          sizeof(payload)));
    TEST_ASSERT_EQUAL_UINT32(1U, fake_hal_abort_count());
    TEST_ASSERT_EQUAL(PROFILER_TRANSPORT_STATUS_TIMEOUT, profiler_transport_status());
}

static void test_receive_frame_and_overrun_events(void)
{
    static const uint8_t frame[] = { 'p', 'r', 'o', 'f', '-', 'd', 'u', 'm' };
    uint8_t received[TEST_RX_CAPACITY];
    size_t length = 0U;
    uint8_t events;

    init_transport();
    TEST_ASSERT_EQUAL(PROFILER_TRANSPORT_STATUS_OK, profiler_transport_listen());
    fake_hal_receive(frame, sizeof(frame), HAL_UART_RXEVENT_IDLE);

    events = profiler_transport_poll(&length);
    TEST_ASSERT_EQUAL_UINT8(PROFILER_TRANSPORT_EVENT_FRAME, events);
    TEST_ASSERT_EQUAL_UINT32(sizeof(frame), length);
    TEST_ASSERT_EQUAL_UINT32(sizeof(frame),
                             stream_read(profiler_transport_stream(),
                                         received,
                                         sizeof(received)));
    TEST_ASSERT_EQUAL_MEMORY(frame, received, sizeof(frame));
    TEST_ASSERT_EQUAL(PROFILER_TRANSPORT_STATUS_OK, profiler_transport_listen());

    fake_hal_receive(frame, sizeof(frame), HAL_UART_RXEVENT_TC);
    events = profiler_transport_poll(&length);
    TEST_ASSERT_EQUAL_UINT8(PROFILER_TRANSPORT_EVENT_FRAME, events);
    TEST_ASSERT_EQUAL_UINT32(sizeof(frame), length);

    TEST_ASSERT_EQUAL(PROFILER_TRANSPORT_STATUS_OK, profiler_transport_listen());
    fake_hal_receive(frame, sizeof(frame) + 4U, HAL_UART_RXEVENT_IDLE);
    events = profiler_transport_poll(&length);
    TEST_ASSERT_EQUAL_UINT8(PROFILER_TRANSPORT_EVENT_FRAME | PROFILER_TRANSPORT_EVENT_OVERRUN,
                            events);
    TEST_ASSERT_EQUAL_UINT32(TEST_RX_CAPACITY, length);

    TEST_ASSERT_EQUAL(PROFILER_TRANSPORT_STATUS_OK, profiler_transport_listen());
    fake_hal_receive(frame, sizeof(frame), HAL_UART_RXEVENT_HT);
    events = profiler_transport_poll(&length);
    TEST_ASSERT_EQUAL_UINT8(PROFILER_TRANSPORT_EVENT_NONE, events);
}

static void test_hal_error_event_latches_status(void)
{
    uint8_t events;

    init_transport();
    fake_hal_error(&huart2);
    events = profiler_transport_poll(NULL);

    TEST_ASSERT_EQUAL_UINT8(PROFILER_TRANSPORT_EVENT_ERROR, events);
    TEST_ASSERT_EQUAL(PROFILER_TRANSPORT_STATUS_IO_ERROR, profiler_transport_status());
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_chunked_transmit_and_partial_flush);
    RUN_TEST(test_transmit_start_failure_is_reported);
    RUN_TEST(test_transmit_timeout_aborts_and_latches_status);
    RUN_TEST(test_receive_frame_and_overrun_events);
    RUN_TEST(test_hal_error_event_latches_status);
    return UNITY_END();
}
