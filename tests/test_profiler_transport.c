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

#define TEST_MAP_CAPACITY     (16U)
#define TEST_METRICS_CAPACITY (12U)
#define TEST_STACK_DEPTH      (8U)
#define TEST_TX_CAPACITY      (256U)
#define TEST_RX_CAPACITY      (8U)

static HashMapEntry s_map_entries[TEST_MAP_CAPACITY];
static ProfilerMetric s_metrics[TEST_METRICS_CAPACITY];
static ProfilerStackFrame s_stack_frames[TEST_STACK_DEPTH];
static uint8_t s_tx_buffer[TEST_TX_CAPACITY];
static uint8_t s_rx_buffer[TEST_RX_CAPACITY];
static ProfilerTransportConfig s_transport_config;

void setUp(void)
{
    (void)memset(s_map_entries, 0, sizeof(s_map_entries));
    (void)memset(s_metrics, 0, sizeof(s_metrics));
    (void)memset(s_stack_frames, 0, sizeof(s_stack_frames));
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
    ProfilerConfig config = {
        .frequency = 1000000U,
        .map_entries = s_map_entries,
        .map_capacity = TEST_MAP_CAPACITY,
        .metrics = s_metrics,
        .metrics_capacity = TEST_METRICS_CAPACITY,
        .stack_frames = s_stack_frames,
        .stack_depth = TEST_STACK_DEPTH
    };

    return profiler_init(&config);
}

static void test_transport_init_validation(void)
{
    TEST_ASSERT_NULL(profiler_transport_stream());
    TEST_ASSERT_EQUAL(PROFILER_TRANSPORT_STATUS_INVALID_PARAM,
                      profiler_transport_init(NULL));

    s_transport_config.tx_timeout_ms = 100U;
    TEST_ASSERT_EQUAL(PROFILER_TRANSPORT_STATUS_INVALID_PARAM,
                      profiler_transport_init(&s_transport_config));

    s_transport_config.tx_timeout_ms = 0U;
    TEST_ASSERT_EQUAL(PROFILER_TRANSPORT_STATUS_OK,
                      profiler_transport_init(&s_transport_config));
    TEST_ASSERT_NOT_NULL(profiler_transport_stream());
}

static void test_memory_receive_and_overrun(void)
{
    static const uint8_t frame[] = "prof-dump\r\n";
    uint8_t received[TEST_RX_CAPACITY];
    size_t length = 0U;
    uint8_t events;

    TEST_ASSERT_EQUAL(PROFILER_TRANSPORT_STATUS_OK,
                      profiler_transport_init(&s_transport_config));
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
    ProfilerBinHeader header;
    size_t expected_size;
    const Stream* stream;
    static const uint8_t function_address = 0U;

    TEST_ASSERT_EQUAL(PROFILER_TRANSPORT_STATUS_OK,
                      profiler_transport_init(&s_transport_config));
    TEST_ASSERT_TRUE(initialize_profiler());
    profiler_start();
    __cyg_profile_func_enter((void*)&function_address, NULL);
    __cyg_profile_func_exit((void*)&function_address, NULL);

    stream = profiler_transport_stream();
    TEST_ASSERT_NOT_NULL(stream);
    TEST_ASSERT_TRUE(profiler_dump(stream));
    TEST_ASSERT_EQUAL(PROFILER_TRANSPORT_STATUS_OK, profiler_transport_status());

    expected_size = sizeof(ProfilerBinHeader) + sizeof(ProfilerMetric) + sizeof(uint32_t);
    TEST_ASSERT_EQUAL_UINT32(expected_size, profiler_transport_memory_output_size());
    (void)memcpy(&header, s_tx_buffer, sizeof(header));
    TEST_ASSERT_EQUAL_HEX32(PROFILER_BIN_MAGIC, header.magic);
    TEST_ASSERT_EQUAL_UINT16(PROFILER_BIN_VERSION, header.version);
    TEST_ASSERT_EQUAL_UINT16(1U, header.record_count);
    TEST_ASSERT_EQUAL_UINT32(1000000U, header.frequency);
}

static void test_memory_write_overflow_is_reported(void)
{
    uint8_t small_tx[4];
    static const uint8_t data[8] = { 0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U };
    size_t written;

    s_transport_config.tx_buffer = small_tx;
    s_transport_config.tx_buffer_size = sizeof(small_tx);
    TEST_ASSERT_EQUAL(PROFILER_TRANSPORT_STATUS_OK,
                      profiler_transport_init(&s_transport_config));

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
    RUN_TEST(test_memory_write_overflow_is_reported);
    return UNITY_END();
}
