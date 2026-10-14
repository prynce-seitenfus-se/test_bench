#include "unity.h"
#include "profiler.h"

void setUp(void)
{
}

void tearDown(void)
{
    profiler_stop();
}

static void test_configured_backend_is_pulled_from_archive(void)
{
    uint8_t device = 0U;
    /* A weak fallback would return success and silently skip this backend. */
    TEST_ASSERT_FALSE(profiler_init(1000000U, &device));
    TEST_ASSERT_TRUE(profiler_process());
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_configured_backend_is_pulled_from_archive);
    return UNITY_END();
}
