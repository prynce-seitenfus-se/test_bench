#include "profiler_port.h"
#include "profiler_port_stub.h"

#include <stddef.h>

static uint32_t s_test_timestamp;
static bool s_auto_ticks = true;
static const void* s_context_id = NULL;

void profiler_port_init(void)
{
    s_test_timestamp = 0U;
    s_auto_ticks = true;
    s_context_id = NULL;
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
    return 0U;
}

void profiler_port_exit_critical(uint32_t state)
{
    (void)state;
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