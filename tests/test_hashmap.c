/**
 * @file test_hashmap.c
 * @brief Unit tests for the essential high-performance hashmap module.
 */

#include "unity.h"
#include "hashmap.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define TEST_CAPACITY_8   (8U)
#define TEST_CAPACITY_16  (16U)
#define TEST_CAPACITY_64  (64U)

static HashMap s_map;
static HashMapEntry s_entries_8[TEST_CAPACITY_8];
static HashMapEntry s_entries_16[TEST_CAPACITY_16];
static HashMapEntry s_entries_64[TEST_CAPACITY_64];

void setUp(void)
{
    (void)memset(&s_map, 0, sizeof(s_map));
    (void)memset(s_entries_8, 0, sizeof(s_entries_8));
    (void)memset(s_entries_16, 0, sizeof(s_entries_16));
    (void)memset(s_entries_64, 0, sizeof(s_entries_64));
}

void tearDown(void)
{
}

void test_hashmap_init_valid(void)
{
    bool ok = hashmap_init(&s_map, s_entries_16, TEST_CAPACITY_16);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_PTR(s_entries_16, s_map.entries);
    TEST_ASSERT_EQUAL_UINT32(TEST_CAPACITY_16, s_map.capacity);
    TEST_ASSERT_EQUAL_UINT32(15U, s_map.mask);
    TEST_ASSERT_EQUAL_UINT32(12U, s_map.max_count); /* 75% of 16 */
    TEST_ASSERT_EQUAL_UINT32(0U, s_map.count);
    TEST_ASSERT_EQUAL_UINT32(16U, hashmap_capacity(&s_map));
    TEST_ASSERT_EQUAL_UINT32(0U, hashmap_size(&s_map));
    TEST_ASSERT_TRUE(hashmap_is_empty(&s_map));

    /* Check all entries initialized to NULL */
    for (size_t i = 0U; i < TEST_CAPACITY_16; i++) {
        TEST_ASSERT_NULL(s_entries_16[i].key);
        TEST_ASSERT_NULL(s_entries_16[i].value);
    }
}

void test_hashmap_init_invalid_params(void)
{
    /* NULL map */
    TEST_ASSERT_FALSE(hashmap_init(NULL, s_entries_16, TEST_CAPACITY_16));

    /* NULL entries buffer */
    TEST_ASSERT_FALSE(hashmap_init(&s_map, NULL, TEST_CAPACITY_16));

    /* Non-power-of-two capacities */
    TEST_ASSERT_FALSE(hashmap_init(&s_map, s_entries_16, 0U));
    TEST_ASSERT_FALSE(hashmap_init(&s_map, s_entries_16, 1U));
    TEST_ASSERT_FALSE(hashmap_init(&s_map, s_entries_16, 3U));
    TEST_ASSERT_FALSE(hashmap_init(&s_map, s_entries_16, 5U));
    TEST_ASSERT_FALSE(hashmap_init(&s_map, s_entries_16, 6U));
    TEST_ASSERT_FALSE(hashmap_init(&s_map, s_entries_16, 7U));
    TEST_ASSERT_FALSE(hashmap_init(&s_map, s_entries_16, 15U));
    TEST_ASSERT_FALSE(hashmap_init(&s_map, s_entries_16, 100U));
}

void test_hashmap_insert_and_get_basic(void)
{
    TEST_ASSERT_TRUE(hashmap_init(&s_map, s_entries_16, TEST_CAPACITY_16));

    const void* key1 = (const void*)0x08001000U;
    void* val1 = (void*)0x1111U;

    const void* key2 = (const void*)0x08002004U;
    void* val2 = (void*)0x2222U;

    TEST_ASSERT_TRUE(hashmap_insert(&s_map, key1, val1));
    TEST_ASSERT_EQUAL_UINT32(1U, hashmap_size(&s_map));
    TEST_ASSERT_FALSE(hashmap_is_empty(&s_map));

    TEST_ASSERT_TRUE(hashmap_insert(&s_map, key2, val2));
    TEST_ASSERT_EQUAL_UINT32(2U, hashmap_size(&s_map));

    void* retrieved_val = NULL;
    TEST_ASSERT_TRUE(hashmap_get(&s_map, key1, &retrieved_val));
    TEST_ASSERT_EQUAL_PTR(val1, retrieved_val);

    retrieved_val = NULL;
    TEST_ASSERT_TRUE(hashmap_get(&s_map, key2, &retrieved_val));
    TEST_ASSERT_EQUAL_PTR(val2, retrieved_val);

    /* Test get with NULL out_value */
    TEST_ASSERT_TRUE(hashmap_get(&s_map, key1, NULL));

    /* Key not found */
    const void* key_missing = (const void*)0x08009999U;
    TEST_ASSERT_FALSE(hashmap_get(&s_map, key_missing, &retrieved_val));
}

void test_hashmap_null_key_defensive(void)
{
    TEST_ASSERT_TRUE(hashmap_init(&s_map, s_entries_16, TEST_CAPACITY_16));

    void* dummy = (void*)0x1234U;
    void* out_val = NULL;

    /* Inserting NULL key must be rejected */
    TEST_ASSERT_FALSE(hashmap_insert(&s_map, NULL, dummy));
    TEST_ASSERT_EQUAL_UINT32(0U, hashmap_size(&s_map));

    /* Querying NULL key must be rejected */
    TEST_ASSERT_FALSE(hashmap_get(&s_map, NULL, &out_val));
    TEST_ASSERT_FALSE(hashmap_contains(&s_map, NULL));
    TEST_ASSERT_NULL(hashmap_get_ref(&s_map, NULL));

    /* Removing NULL key must be rejected */
    TEST_ASSERT_FALSE(hashmap_remove(&s_map, NULL, &out_val));
}

void test_hashmap_update_existing_key(void)
{
    TEST_ASSERT_TRUE(hashmap_init(&s_map, s_entries_16, TEST_CAPACITY_16));

    const void* key = (const void*)0x08001000U;
    void* val_initial = (void*)100U;
    void* val_updated = (void*)200U;

    TEST_ASSERT_TRUE(hashmap_insert(&s_map, key, val_initial));
    TEST_ASSERT_EQUAL_UINT32(1U, hashmap_size(&s_map));

    /* Update with new value */
    TEST_ASSERT_TRUE(hashmap_insert(&s_map, key, val_updated));
    TEST_ASSERT_EQUAL_UINT32(1U, hashmap_size(&s_map));

    void* retrieved = NULL;
    TEST_ASSERT_TRUE(hashmap_get(&s_map, key, &retrieved));
    TEST_ASSERT_EQUAL_PTR(val_updated, retrieved);
}

void test_hashmap_get_ref_inplace_mutation(void)
{
    TEST_ASSERT_TRUE(hashmap_init(&s_map, s_entries_16, TEST_CAPACITY_16));

    const void* key = (const void*)0x08004000U;
    uintptr_t counter = 50U;

    TEST_ASSERT_TRUE(hashmap_insert(&s_map, key, (void*)counter));

    /* Obtain direct slot pointer */
    void** slot = hashmap_get_ref(&s_map, key);
    TEST_ASSERT_NOT_NULL(slot);
    TEST_ASSERT_EQUAL_PTR((void*)50U, *slot);

    /* In-place mutation */
    *slot = (void*)((uintptr_t)(*slot) + 25U);

    /* Verify with hashmap_get */
    void* out_val = NULL;
    TEST_ASSERT_TRUE(hashmap_get(&s_map, key, &out_val));
    TEST_ASSERT_EQUAL_PTR((void*)75U, out_val);

    /* Non-existent key */
    TEST_ASSERT_NULL(hashmap_get_ref(&s_map, (const void*)0x08009999U));
}

void test_hashmap_contains(void)
{
    TEST_ASSERT_TRUE(hashmap_init(&s_map, s_entries_16, TEST_CAPACITY_16));

    const void* key = (const void*)0x08001234U;
    TEST_ASSERT_FALSE(hashmap_contains(&s_map, key));

    TEST_ASSERT_TRUE(hashmap_insert(&s_map, key, (void*)1U));
    TEST_ASSERT_TRUE(hashmap_contains(&s_map, key));

    TEST_ASSERT_FALSE(hashmap_contains(&s_map, (const void*)0x08005678U));
}

void test_hashmap_max_load_factor_enforcement(void)
{
    /* Capacity 8 -> max_count = (8 * 3) / 4 = 6 */
    TEST_ASSERT_TRUE(hashmap_init(&s_map, s_entries_8, TEST_CAPACITY_8));
    TEST_ASSERT_EQUAL_UINT32(6U, s_map.max_count);

    for (uintptr_t i = 1U; i <= 6U; i++) {
        const void* key = (const void*)(0x08000000U + (i * 0x100U));
        TEST_ASSERT_TRUE(hashmap_insert(&s_map, key, (void*)i));
    }
    TEST_ASSERT_EQUAL_UINT32(6U, hashmap_size(&s_map));

    /* 7th insertion exceeds 75% limit and must be rejected */
    const void* overflow_key = (const void*)0x08000700U;
    TEST_ASSERT_FALSE(hashmap_insert(&s_map, overflow_key, (void*)7U));
    TEST_ASSERT_EQUAL_UINT32(6U, hashmap_size(&s_map));

    /* However, updating an existing key when at max capacity must still succeed */
    const void* existing_key = (const void*)0x08000100U;
    TEST_ASSERT_TRUE(hashmap_insert(&s_map, existing_key, (void*)999U));
    TEST_ASSERT_EQUAL_UINT32(6U, hashmap_size(&s_map));

    void* check_val = NULL;
    TEST_ASSERT_TRUE(hashmap_get(&s_map, existing_key, &check_val));
    TEST_ASSERT_EQUAL_PTR((void*)999U, check_val);
}

void test_hashmap_remove_basic_and_reinsert(void)
{
    TEST_ASSERT_TRUE(hashmap_init(&s_map, s_entries_8, TEST_CAPACITY_8));

    const void* k1 = (const void*)0x1000U;
    const void* k2 = (const void*)0x2000U;

    TEST_ASSERT_TRUE(hashmap_insert(&s_map, k1, (void*)10U));
    TEST_ASSERT_TRUE(hashmap_insert(&s_map, k2, (void*)20U));
    TEST_ASSERT_EQUAL_UINT32(2U, hashmap_size(&s_map));

    void* removed = NULL;
    TEST_ASSERT_TRUE(hashmap_remove(&s_map, k1, &removed));
    TEST_ASSERT_EQUAL_PTR((void*)10U, removed);
    TEST_ASSERT_EQUAL_UINT32(1U, hashmap_size(&s_map));
    TEST_ASSERT_FALSE(hashmap_contains(&s_map, k1));
    TEST_ASSERT_TRUE(hashmap_contains(&s_map, k2));

    /* Removing non-existent key */
    TEST_ASSERT_FALSE(hashmap_remove(&s_map, (const void*)0x9999U, &removed));

    /* Test remove with NULL out_removed_value */
    TEST_ASSERT_TRUE(hashmap_remove(&s_map, k2, NULL));
    TEST_ASSERT_EQUAL_UINT32(0U, hashmap_size(&s_map));
    TEST_ASSERT_TRUE(hashmap_is_empty(&s_map));
}

void test_hashmap_collision_chain_and_algorithm_r(void)
{
    /* Use capacity 16 */
    TEST_ASSERT_TRUE(hashmap_init(&s_map, s_entries_16, TEST_CAPACITY_16));

    /* Insert 10 items to form multi-element collision clusters */
    const void* keys[10];
    for (size_t i = 0U; i < 10U; i++) {
        keys[i] = (const void*)(uintptr_t)(0x08001000U + (i * 16U));
        TEST_ASSERT_TRUE(hashmap_insert(&s_map, keys[i], (void*)(uintptr_t)(i + 1U)));
    }
    TEST_ASSERT_EQUAL_UINT32(10U, hashmap_size(&s_map));

    /* Verify all 10 are readable */
    for (size_t i = 0U; i < 10U; i++) {
        void* v = NULL;
        TEST_ASSERT_TRUE(hashmap_get(&s_map, keys[i], &v));
        TEST_ASSERT_EQUAL_PTR((void*)(uintptr_t)(i + 1U), v);
    }

    /* Remove middle element (index 4) */
    void* removed_val = NULL;
    TEST_ASSERT_TRUE(hashmap_remove(&s_map, keys[4], &removed_val));
    TEST_ASSERT_EQUAL_PTR((void*)5U, removed_val);
    TEST_ASSERT_EQUAL_UINT32(9U, hashmap_size(&s_map));

    /* Invariant check: All other 9 elements must still be found */
    for (size_t i = 0U; i < 10U; i++) {
        if (i == 4U) {
            TEST_ASSERT_FALSE(hashmap_contains(&s_map, keys[i]));
        } else {
            void* v = NULL;
            TEST_ASSERT_TRUE(hashmap_get(&s_map, keys[i], &v));
            TEST_ASSERT_EQUAL_PTR((void*)(uintptr_t)(i + 1U), v);
        }
    }

    /* Remove first element (index 0) */
    TEST_ASSERT_TRUE(hashmap_remove(&s_map, keys[0], &removed_val));
    TEST_ASSERT_EQUAL_PTR((void*)1U, removed_val);

    /* Verify remaining 8 elements are still found */
    for (size_t i = 1U; i < 10U; i++) {
        if (i == 4U) {
            TEST_ASSERT_FALSE(hashmap_contains(&s_map, keys[i]));
        } else {
            void* v = NULL;
            TEST_ASSERT_TRUE(hashmap_get(&s_map, keys[i], &v));
            TEST_ASSERT_EQUAL_PTR((void*)(uintptr_t)(i + 1U), v);
        }
    }

    /* Remove last element (index 9) */
    TEST_ASSERT_TRUE(hashmap_remove(&s_map, keys[9], &removed_val));
    TEST_ASSERT_EQUAL_PTR((void*)10U, removed_val);

    /* Verify remaining 7 elements are still found */
    for (size_t i = 1U; i < 9U; i++) {
        if (i == 4U) {
            TEST_ASSERT_FALSE(hashmap_contains(&s_map, keys[i]));
        } else {
            TEST_ASSERT_TRUE(hashmap_contains(&s_map, keys[i]));
        }
    }
}

void test_hashmap_clear(void)
{
    TEST_ASSERT_TRUE(hashmap_init(&s_map, s_entries_16, TEST_CAPACITY_16));

    for (size_t i = 1U; i <= 5U; i++) {
        (void)hashmap_insert(&s_map, (const void*)(uintptr_t)(i * 0x100U), (void*)i);
    }
    TEST_ASSERT_EQUAL_UINT32(5U, hashmap_size(&s_map));

    hashmap_clear(&s_map);
    TEST_ASSERT_EQUAL_UINT32(0U, hashmap_size(&s_map));
    TEST_ASSERT_TRUE(hashmap_is_empty(&s_map));

    /* Slots should be reset to NULL */
    for (size_t i = 1U; i <= 5U; i++) {
        TEST_ASSERT_FALSE(hashmap_contains(&s_map, (const void*)(uintptr_t)(i * 0x100U)));
    }

    /* Re-insertion after clear should work normally */
    TEST_ASSERT_TRUE(hashmap_insert(&s_map, (const void*)0x1234U, (void*)42U));
    TEST_ASSERT_EQUAL_UINT32(1U, hashmap_size(&s_map));
}

void test_hashmap_iterator(void)
{
    TEST_ASSERT_TRUE(hashmap_init(&s_map, s_entries_16, TEST_CAPACITY_16));

    /* Empty map iteration */
    HashMapIter iter = hashmap_iter(&s_map);
    const void* k = NULL;
    void* v = NULL;
    TEST_ASSERT_FALSE(hashmap_iter_next(&iter, &k, &v));

    /* Insert 4 items */
    const void* inserted_keys[4] = {
        (const void*)0x08001000U,
        (const void*)0x08002000U,
        (const void*)0x08003000U,
        (const void*)0x08004000U
    };
    for (size_t i = 0U; i < 4U; i++) {
        TEST_ASSERT_TRUE(hashmap_insert(&s_map, inserted_keys[i], (void*)(uintptr_t)(i + 10U)));
    }

    /* Iterate and count */
    iter = hashmap_iter(&s_map);
    size_t count = 0U;
    bool seen[4] = { false, false, false, false };

    while (hashmap_iter_next(&iter, &k, &v)) {
        count++;
        TEST_ASSERT_NOT_NULL(k);
        TEST_ASSERT_NOT_NULL(v);

        /* Match against inserted set */
        bool matched = false;
        for (size_t j = 0U; j < 4U; j++) {
            if (inserted_keys[j] == k) {
                TEST_ASSERT_EQUAL_PTR((void*)(uintptr_t)(j + 10U), v);
                seen[j] = true;
                matched = true;
                break;
            }
        }
        TEST_ASSERT_TRUE(matched);
    }

    TEST_ASSERT_EQUAL_UINT32(4U, count);
    for (size_t j = 0U; j < 4U; j++) {
        TEST_ASSERT_TRUE(seen[j]);
    }

    /* Test iter with NULL destination pointers */
    iter = hashmap_iter(&s_map);
    TEST_ASSERT_TRUE(hashmap_iter_next(&iter, NULL, NULL));
}

void test_hashmap_null_container_handling(void)
{
    void* v = NULL;
    TEST_ASSERT_EQUAL_UINT32(0U, hashmap_size(NULL));
    TEST_ASSERT_EQUAL_UINT32(0U, hashmap_capacity(NULL));
    TEST_ASSERT_TRUE(hashmap_is_empty(NULL));
    TEST_ASSERT_FALSE(hashmap_insert(NULL, (const void*)0x1234U, (void*)1U));
    TEST_ASSERT_FALSE(hashmap_get(NULL, (const void*)0x1234U, &v));
    TEST_ASSERT_NULL(hashmap_get_ref(NULL, (const void*)0x1234U));
    TEST_ASSERT_FALSE(hashmap_contains(NULL, (const void*)0x1234U));
    TEST_ASSERT_FALSE(hashmap_remove(NULL, (const void*)0x1234U, &v));

    /* Clear on NULL should not crash */
    hashmap_clear(NULL);

    /* Iter on NULL */
    HashMapIter null_iter = hashmap_iter(NULL);
    const void* k = NULL;
    TEST_ASSERT_FALSE(hashmap_iter_next(&null_iter, &k, &v));
    TEST_ASSERT_FALSE(hashmap_iter_next(NULL, &k, &v));
}

void test_hashmap_stress_pseudo_random(void)
{
    /* Capacity 64 -> max_count = 48 */
    TEST_ASSERT_TRUE(hashmap_init(&s_map, s_entries_64, TEST_CAPACITY_64));

    /* Simple deterministic LCG random generator */
    uint32_t state = 123456789U;
    #define LCG_RAND() (state = (state * 1664525U + 1013904223U))

    const size_t pool_size = 40U; /* Well within 48 limit */
    const void* key_pool[40];
    bool present[40];

    for (size_t i = 0U; i < pool_size; i++) {
        /* Generate distinct non-null 4-byte aligned pointers */
        uint32_t addr = 0x08000000U + (uint32_t)((i + 1U) * 64U);
        key_pool[i] = (const void*)(uintptr_t)addr;
        present[i] = false;
    }

    /* 1000 randomized operations */
    for (size_t op = 0U; op < 1000U; op++) {
        size_t idx = (size_t)(LCG_RAND() % pool_size);
        uint32_t action = LCG_RAND() % 3U;

        if (action == 0U) {
            /* Insert */
            if (!present[idx]) {
                TEST_ASSERT_TRUE(hashmap_insert(&s_map, key_pool[idx], (void*)(uintptr_t)(idx + 100U)));
                present[idx] = true;
            }
        } else if (action == 1U) {
            /* Remove */
            if (present[idx]) {
                void* rem = NULL;
                TEST_ASSERT_TRUE(hashmap_remove(&s_map, key_pool[idx], &rem));
                TEST_ASSERT_EQUAL_PTR((void*)(uintptr_t)(idx + 100U), rem);
                present[idx] = false;
            }
        } else {
            /* Lookup */
            if (present[idx]) {
                void* v = NULL;
                TEST_ASSERT_TRUE(hashmap_get(&s_map, key_pool[idx], &v));
                TEST_ASSERT_EQUAL_PTR((void*)(uintptr_t)(idx + 100U), v);
            } else {
                TEST_ASSERT_FALSE(hashmap_contains(&s_map, key_pool[idx]));
            }
        }
    }

    /* Final validation: verify all currently present elements match expectations */
    size_t expected_count = 0U;
    for (size_t i = 0U; i < pool_size; i++) {
        if (present[i]) {
            expected_count++;
            void* v = NULL;
            TEST_ASSERT_TRUE(hashmap_get(&s_map, key_pool[i], &v));
            TEST_ASSERT_EQUAL_PTR((void*)(uintptr_t)(i + 100U), v);
        } else {
            TEST_ASSERT_FALSE(hashmap_contains(&s_map, key_pool[i]));
        }
    }
    TEST_ASSERT_EQUAL_UINT32(expected_count, hashmap_size(&s_map));
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_hashmap_init_valid);
    RUN_TEST(test_hashmap_init_invalid_params);
    RUN_TEST(test_hashmap_insert_and_get_basic);
    RUN_TEST(test_hashmap_null_key_defensive);
    RUN_TEST(test_hashmap_update_existing_key);
    RUN_TEST(test_hashmap_get_ref_inplace_mutation);
    RUN_TEST(test_hashmap_contains);
    RUN_TEST(test_hashmap_max_load_factor_enforcement);
    RUN_TEST(test_hashmap_remove_basic_and_reinsert);
    RUN_TEST(test_hashmap_collision_chain_and_algorithm_r);
    RUN_TEST(test_hashmap_clear);
    RUN_TEST(test_hashmap_iterator);
    RUN_TEST(test_hashmap_null_container_handling);
    RUN_TEST(test_hashmap_stress_pseudo_random);

    return UNITY_END();
}
