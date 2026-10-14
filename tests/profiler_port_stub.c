#include "profiler_port.h"
#include "profiler_port_stub.h"

#include <stddef.h>

static uint32_t s_test_timestamp;
static bool s_auto_ticks = true;
static const void* s_context_id = NULL;
static uint32_t s_mask;
static uint32_t s_critical_start;
static uint32_t s_max_critical_ticks;
static uint32_t s_unlock_count;
static void (*s_unlock_callback)(void);

void profiler_port_init(void)
{
    s_test_timestamp = 0U;
    s_auto_ticks = true;
    s_context_id = NULL;
    s_mask = 0U;
    s_max_critical_ticks = 0U;
    s_unlock_count = 0U;
    s_unlock_callback = NULL;
}

uint32_t profiler_port_ticks(void)
{
    if (s_auto_ticks) {
        s_test_timestamp++;
    }
    return s_test_timestamp;
}

uint32_t profiler_port_enter_critical(void)
{
    uint32_t previous = s_mask;
    if (previous == 0U) {
        s_critical_start = s_test_timestamp;
    }
    s_mask = 1U;
    return previous;
}

void profiler_port_exit_critical(uint32_t state)
{
    s_mask = state;
    if (state == 0U) {
        uint32_t ticks = s_test_timestamp - s_critical_start;
        if (ticks > s_max_critical_ticks) {
            s_max_critical_ticks = ticks;
        }
        s_unlock_count++;
        if (s_unlock_callback != NULL) {
            void (*callback)(void) = s_unlock_callback;
            s_unlock_callback = NULL;
            callback();
        }
    }
}

const void* profiler_port_context_id(void)
{
    return s_context_id;
}

void profiler_port_stub_set_auto_ticks(bool enabled)
{
    s_auto_ticks = enabled;
}

void profiler_port_stub_set_ticks(uint32_t ticks)
{
    s_test_timestamp = ticks;
}

void profiler_port_stub_set_context(const void* id)
{
    s_context_id = id;
}

void profiler_port_stub_on_unlock(void (*callback)(void))
{
    s_unlock_callback = callback;
}

uint32_t profiler_port_stub_max_critical_ticks(void)
{
    return s_max_critical_ticks;
}

uint32_t profiler_port_stub_unlock_count(void)
{
    return s_unlock_count;
}