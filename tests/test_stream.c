/**
 * @file test_stream.c
 * @brief Unit tests for the stream transport layer module.
 */

#include "unity.h"
#include "stream.h"
#include <string.h>

/* Mock context tracking state */
typedef struct MockDevice {
    uint8_t  tx_buffer[64];
    size_t   tx_count;
    uint8_t  rx_buffer[64];
    size_t   rx_count;
    size_t   rx_offset;
    bool     flush_called;
    void*    last_context_seen;
} MockDevice;

static MockDevice s_mock_device;

static size_t mock_write(void* context, const uint8_t* buffer, size_t size)
{
    MockDevice* dev = (MockDevice*)context;
    if (dev == NULL) {
        return 0U;
    }
    dev->last_context_seen = context;

    size_t available = sizeof(dev->tx_buffer) - dev->tx_count;
    size_t to_write = (size < available) ? size : available;

    (void)memcpy(&dev->tx_buffer[dev->tx_count], buffer, to_write);
    dev->tx_count += to_write;
    return to_write;
}

static size_t mock_read(void* context, uint8_t* buffer, size_t size)
{
    MockDevice* dev = (MockDevice*)context;
    if (dev == NULL) {
        return 0U;
    }
    dev->last_context_seen = context;

    size_t available = dev->rx_count - dev->rx_offset;
    size_t to_read = (size < available) ? size : available;

    (void)memcpy(buffer, &dev->rx_buffer[dev->rx_offset], to_read);
    dev->rx_offset += to_read;
    return to_read;
}

static void mock_flush(void* context)
{
    MockDevice* dev = (MockDevice*)context;
    if (dev != NULL) {
        dev->last_context_seen = context;
        dev->flush_called = true;
    }
}

/* Static context-less mock */
static size_t s_null_ctx_write_count = 0U;
static size_t mock_write_no_ctx(void* context, const uint8_t* buffer, size_t size)
{
    (void)context;
    (void)buffer;
    s_null_ctx_write_count += size;
    return size;
}

void setUp(void)
{
    (void)memset(&s_mock_device, 0, sizeof(s_mock_device));
    s_null_ctx_write_count = 0U;
}

void tearDown(void)
{
}

void test_stream_init_value_construction(void)
{
    Stream stream = stream_init(&s_mock_device, mock_write, mock_read, mock_flush);

    TEST_ASSERT_EQUAL_PTR(&s_mock_device, stream.context);
    TEST_ASSERT_EQUAL_PTR(mock_write, stream.write);
    TEST_ASSERT_EQUAL_PTR(mock_read, stream.read);
    TEST_ASSERT_EQUAL_PTR(mock_flush, stream.flush);
}

void test_stream_init_with_null_callbacks_and_null_context(void)
{
    Stream stream_tx_only = stream_init(NULL, mock_write, NULL, NULL);

    TEST_ASSERT_NULL(stream_tx_only.context);
    TEST_ASSERT_EQUAL_PTR(mock_write, stream_tx_only.write);
    TEST_ASSERT_NULL(stream_tx_only.read);
    TEST_ASSERT_NULL(stream_tx_only.flush);
}

void test_stream_write_success(void)
{
    Stream stream = stream_init(&s_mock_device, mock_write, mock_read, mock_flush);
    const uint8_t test_data[] = { 0x10U, 0x20U, 0x30U, 0x40U };

    size_t written = stream_write(&stream, test_data, sizeof(test_data));

    TEST_ASSERT_EQUAL_UINT32(4U, (uint32_t)written);
    TEST_ASSERT_EQUAL_UINT32(4U, (uint32_t)s_mock_device.tx_count);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(test_data, s_mock_device.tx_buffer, 4U);
    TEST_ASSERT_EQUAL_PTR(&s_mock_device, s_mock_device.last_context_seen);
}

void test_stream_write_null_and_boundary_checks(void)
{
    Stream stream = stream_init(&s_mock_device, mock_write, mock_read, mock_flush);
    Stream stream_no_write = stream_init(&s_mock_device, NULL, mock_read, mock_flush);
    const uint8_t test_data[] = { 0x01U };

    /* NULL stream */
    TEST_ASSERT_EQUAL_UINT32(0U, (uint32_t)stream_write(NULL, test_data, 1U));

    /* NULL write callback */
    TEST_ASSERT_EQUAL_UINT32(0U, (uint32_t)stream_write(&stream_no_write, test_data, 1U));

    /* NULL buffer */
    TEST_ASSERT_EQUAL_UINT32(0U, (uint32_t)stream_write(&stream, NULL, 1U));

    /* Zero size */
    TEST_ASSERT_EQUAL_UINT32(0U, (uint32_t)stream_write(&stream, test_data, 0U));

    /* Ensure mock was untouched */
    TEST_ASSERT_EQUAL_UINT32(0U, (uint32_t)s_mock_device.tx_count);
}

void test_stream_write_with_null_context(void)
{
    Stream stream = stream_init(NULL, mock_write_no_ctx, NULL, NULL);
    const uint8_t test_data[] = { 0xAAU, 0xBBU, 0xCCU };

    size_t written = stream_write(&stream, test_data, sizeof(test_data));

    TEST_ASSERT_EQUAL_UINT32(3U, (uint32_t)written);
    TEST_ASSERT_EQUAL_UINT32(3U, (uint32_t)s_null_ctx_write_count);
}

void test_stream_read_success(void)
{
    Stream stream = stream_init(&s_mock_device, mock_write, mock_read, mock_flush);
    const uint8_t preload_data[] = { 0x05U, 0x06U, 0x07U, 0x08U };
    (void)memcpy(s_mock_device.rx_buffer, preload_data, sizeof(preload_data));
    s_mock_device.rx_count = sizeof(preload_data);

    uint8_t dest_buffer[8] = { 0U };
    size_t read_bytes = stream_read(&stream, dest_buffer, sizeof(dest_buffer));

    TEST_ASSERT_EQUAL_UINT32(4U, (uint32_t)read_bytes);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(preload_data, dest_buffer, 4U);
    TEST_ASSERT_EQUAL_PTR(&s_mock_device, s_mock_device.last_context_seen);
}

void test_stream_read_null_and_boundary_checks(void)
{
    Stream stream = stream_init(&s_mock_device, mock_write, mock_read, mock_flush);
    Stream stream_no_read = stream_init(&s_mock_device, mock_write, NULL, mock_flush);
    uint8_t dest_buffer[4] = { 0U };

    /* NULL stream */
    TEST_ASSERT_EQUAL_UINT32(0U, (uint32_t)stream_read(NULL, dest_buffer, sizeof(dest_buffer)));

    /* NULL read callback */
    TEST_ASSERT_EQUAL_UINT32(0U, (uint32_t)stream_read(&stream_no_read, dest_buffer, sizeof(dest_buffer)));

    /* NULL destination buffer */
    TEST_ASSERT_EQUAL_UINT32(0U, (uint32_t)stream_read(&stream, NULL, sizeof(dest_buffer)));

    /* Zero size */
    TEST_ASSERT_EQUAL_UINT32(0U, (uint32_t)stream_read(&stream, dest_buffer, 0U));
}

void test_stream_flush_behavior(void)
{
    Stream stream = stream_init(&s_mock_device, mock_write, mock_read, mock_flush);
    Stream stream_no_flush = stream_init(&s_mock_device, mock_write, mock_read, NULL);

    /* Valid flush */
    TEST_ASSERT_FALSE(s_mock_device.flush_called);
    stream_flush(&stream);
    TEST_ASSERT_TRUE(s_mock_device.flush_called);

    /* NULL stream flush gracefully no-ops */
    stream_flush(NULL);

    /* Stream with NULL flush callback gracefully no-ops */
    s_mock_device.flush_called = false;
    stream_flush(&stream_no_flush);
    TEST_ASSERT_FALSE(s_mock_device.flush_called);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_stream_init_value_construction);
    RUN_TEST(test_stream_init_with_null_callbacks_and_null_context);
    RUN_TEST(test_stream_write_success);
    RUN_TEST(test_stream_write_null_and_boundary_checks);
    RUN_TEST(test_stream_write_with_null_context);
    RUN_TEST(test_stream_read_success);
    RUN_TEST(test_stream_read_null_and_boundary_checks);
    RUN_TEST(test_stream_flush_behavior);
    return UNITY_END();
}
