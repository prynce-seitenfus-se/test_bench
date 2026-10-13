#include "unity.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "profiler.h"
#include "profiler_port.h"
#include "profiler_transport.h"

bool profiler_transport_memory_inject_rx(const uint8_t* data, size_t length);
size_t profiler_transport_memory_output_size(void);

#define TEST_TX_CAPACITY      (256U)
#define TEST_RX_CAPACITY      (8U)

static uint8_t s_tx_buffer[TEST_TX_CAPACITY];
static uint8_t s_rx_buffer[TEST_RX_CAPACITY];
static ProfilerTransportConfig s_transport_config;

void setUp(void)
{
    (void)memset(s_tx_buffer, 0, sizeof(s_tx_buffer));
    (void)memset(s_rx_buffer, 0, sizeof(s_rx_buffer));

    s_transport_config.device = NULL;
    s_transport_config.tx_buffer = s_tx_buffer;
    s_transport_config.tx_buffer_size = sizeof(s_tx_buffer);
    s_transport_config.rx_buffer = s_rx_buffer;
    s_transport_config.rx_buffer_size = sizeof(s_rx_buffer);
    s_transport_config.tx_timeout_ms = 0U;
    s_transport_config.tick_ms = NULL;
    s_transport_config.yield = NULL;
}

void tearDown(void)
{
    profiler_stop();
}

static bool initialize_profiler(void)
{
    return profiler_init(1000000U);
}

static void test_transport_init_validation(void)
{
    TEST_ASSERT_EQUAL(PROFILER_TRANSPORT_STATUS_INVALID_PARAM,
                      profiler_transport_init_custom(NULL));

    s_transport_config.tx_timeout_ms = 100U;
    TEST_ASSERT_EQUAL(PROFILER_TRANSPORT_STATUS_INVALID_PARAM,
                      profiler_transport_init_custom(&s_transport_config));

    s_transport_config.tx_timeout_ms = 0U;
    TEST_ASSERT_EQUAL(PROFILER_TRANSPORT_STATUS_OK,
                      profiler_transport_init_custom(&s_transport_config));
    TEST_ASSERT_NOT_NULL(profiler_transport_stream());

    /* Test convenient one-line default init */
    TEST_ASSERT_EQUAL(PROFILER_TRANSPORT_STATUS_OK, profiler_transport_init(NULL));
    TEST_ASSERT_NOT_NULL(profiler_transport_stream());
}

static void test_memory_receive_and_overrun(void)
{
    static const uint8_t frame[] = "prof-dump\r\n";
    uint8_t received[TEST_RX_CAPACITY];
    size_t length = 0U;
    uint8_t events;

    TEST_ASSERT_EQUAL(PROFILER_TRANSPORT_STATUS_OK,
                      profiler_transport_init_custom(&s_transport_config));
    TEST_ASSERT_EQUAL(PROFILER_TRANSPORT_STATUS_OK, profiler_transport_listen());
    TEST_ASSERT_TRUE(profiler_transport_memory_inject_rx(frame, sizeof(frame) - 1U));

    events = profiler_transport_poll(&length);
    TEST_ASSERT_EQUAL_UINT8(PROFILER_TRANSPORT_EVENT_FRAME | PROFILER_TRANSPORT_EVENT_OVERRUN,
                            events);
    TEST_ASSERT_EQUAL_UINT32(TEST_RX_CAPACITY, length);
    TEST_ASSERT_EQUAL(PROFILER_TRANSPORT_STATUS_IO_ERROR, profiler_transport_status());

    TEST_ASSERT_EQUAL_UINT32(TEST_RX_CAPACITY,
                             stream_read(profiler_transport_stream(),
                                         received,
                                         sizeof(received)));
    TEST_ASSERT_EQUAL_MEMORY(frame, received, sizeof(received));

    TEST_ASSERT_EQUAL(PROFILER_TRANSPORT_STATUS_OK, profiler_transport_listen());
    events = profiler_transport_poll(NULL);
    TEST_ASSERT_EQUAL_UINT8(PROFILER_TRANSPORT_EVENT_NONE, events);
}

static void test_profiler_dump_uses_memory_transport(void)
{
    const Stream* stream;
    static const uint8_t function_address = 0U;

    TEST_ASSERT_EQUAL(PROFILER_TRANSPORT_STATUS_OK,
                      profiler_transport_init_custom(&s_transport_config));
    TEST_ASSERT_TRUE(initialize_profiler());
    profiler_start();
    __cyg_profile_func_enter((void*)&function_address, NULL);
    __cyg_profile_func_exit((void*)&function_address, NULL);

    stream = profiler_transport_stream();
    TEST_ASSERT_NOT_NULL(stream);
    TEST_ASSERT_TRUE(profiler_dump(stream, PROFILER_DUMP_BIN));
    TEST_ASSERT_EQUAL(PROFILER_TRANSPORT_STATUS_OK, profiler_transport_status());

    /* Header (28) + 1 record (24) + CRC32 (4) = 56 bytes */
    size_t expected_size = 28U + 24U + 4U;
    TEST_ASSERT_EQUAL_UINT32(expected_size, profiler_transport_memory_output_size());
    TEST_ASSERT_EQUAL_HEX8('P', s_tx_buffer[0]);
    TEST_ASSERT_EQUAL_HEX8('R', s_tx_buffer[1]);
    TEST_ASSERT_EQUAL_HEX8('O', s_tx_buffer[2]);
    TEST_ASSERT_EQUAL_HEX8('F', s_tx_buffer[3]);
}

static void test_transport_process_autonomous_dispatch(void)
{
    static const uint8_t cmd_live[] = "live\r\n";
    TEST_ASSERT_EQUAL(PROFILER_TRANSPORT_STATUS_OK, profiler_transport_init(NULL));
    TEST_ASSERT_TRUE(initialize_profiler());

    /* Inject command */
    TEST_ASSERT_TRUE(profiler_transport_memory_inject_rx(cmd_live, sizeof(cmd_live) - 1U));

    /* Process autonomously */
    TEST_ASSERT_TRUE(profiler_transport_process());

    /* Check that memory output contains the TOP header */
    TEST_ASSERT_GREATER_THAN(0U, profiler_transport_memory_output_size());
}

static void test_memory_write_overflow_is_reported(void)
{
    uint8_t small_tx[4];
    static const uint8_t data[8] = { 0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U };
    size_t written;

    s_transport_config.tx_buffer = small_tx;
    s_transport_config.tx_buffer_size = sizeof(small_tx);
    TEST_ASSERT_EQUAL(PROFILER_TRANSPORT_STATUS_OK,
                      profiler_transport_init_custom(&s_transport_config));

    written = stream_write(profiler_transport_stream(), data, sizeof(data));
    TEST_ASSERT_EQUAL_UINT32(sizeof(small_tx), written);
    TEST_ASSERT_EQUAL(PROFILER_TRANSPORT_STATUS_IO_ERROR, profiler_transport_status());
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_transport_init_validation);
    RUN_TEST(test_memory_receive_and_overrun);
    RUN_TEST(test_profiler_dump_uses_memory_transport);
    RUN_TEST(test_transport_process_autonomous_dispatch);
    RUN_TEST(test_memory_write_overflow_is_reported);
    return UNITY_END();
}
