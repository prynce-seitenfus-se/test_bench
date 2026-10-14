#include "profiler_transport.h"

NO_INST ProfilerTransportStatus profiler_transport_init(void* device)
{
    (void)device;
    return PROFILER_TRANSPORT_STATUS_IO_ERROR;
}

NO_INST bool profiler_transport_process(void)
{
    return true;
}
