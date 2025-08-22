#include "memory_tests.h"
#include "ma.h"
#include <assert.h>
#include <errno.h>
#include <malloc.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// This file must be compiled with -std=gnu17 and -fPIC,
// and linked with -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc
// -Wl,--wrap=reallocarray -Wl,--wrap=free -Wl,--wrap=strdup -Wl,--wrap=strndup.

// Declare real memory allocation functions.
void *__real_malloc(size_t size) __attribute__((weak));
void *__real_calloc(size_t nmemb, size_t size) __attribute__((weak));
void *__real_realloc(void *ptr, size_t size) __attribute__((weak));
void *__real_reallocarray(void *ptr, size_t nmemb, size_t size) __attribute__((weak));
char *__real_strdup(const char *s) __attribute__((weak));
char *__real_strndup(const char *s, size_t size) __attribute__((weak));
void __real_free(void *ptr) __attribute__((weak));

// Global memory test statistics.
static memory_test_stats_t stats;

// Accessor for test statistics.
memory_test_stats_t *get_memory_test_stats(void) {
    return &stats;
}

// Check if allocation should fail.
static bool should_fail(void) {
    return ++stats.alloc_calls == stats.fail_at_call;
}

// Allow allocation failure only if increasing memory size.
static bool can_fail(void const *old_ptr, size_t new_size) {
    if (old_ptr == NULL) return true;
    return new_size > malloc_usable_size((void *)old_ptr);
}

// Macro to simulate unreliable memory allocation.
#define UNRELIABLE_ALLOC(ptr, size, fun, name)                         \
    do {                                                              \
        stats.total_calls++;                                          \
        if (ptr != NULL && size == 0) {                               \
            stats.free_count++;                                       \
            return fun;                                               \
        }                                                             \
        void *p = can_fail(ptr, size) && should_fail() ? NULL : (fun);\
        if (p) {                                                      \
            stats.alloc_count += ptr != p;                            \
            stats.free_count += ptr != p && ptr != NULL;              \
        } else {                                                      \
            errno = ENOMEM;                                           \
            stats.failed_function = (char *)name;                     \
        }                                                             \
        return p;                                                     \
    } while (0)

// Wrapped memory allocation functions.
void *__wrap_malloc(size_t size) {
    UNRELIABLE_ALLOC(NULL, size, __real_malloc(size), "malloc");
}

void *__wrap_calloc(size_t nmemb, size_t size) {
    UNRELIABLE_ALLOC(NULL, nmemb * size, __real_calloc(nmemb, size), "calloc");
}

void *__wrap_realloc(void *ptr, size_t size) {
    UNRELIABLE_ALLOC(ptr, size, __real_realloc(ptr, size), "realloc");
}

void *__wrap_reallocarray(void *ptr, size_t nmemb, size_t size) {
    UNRELIABLE_ALLOC(ptr, nmemb * size, __real_reallocarray(ptr, nmemb, size), "reallocarray");
}

char *__wrap_strdup(const char *s) {
    UNRELIABLE_ALLOC(NULL, 0, __real_strdup(s), "strdup");
}

char *__wrap_strndup(const char *s, size_t size) {
    UNRELIABLE_ALLOC(NULL, 0, __real_strndup(s, size), "strndup");
}

void __wrap_free(void *ptr) {
    stats.total_calls++;
    __real_free(ptr);
    if (ptr) stats.free_count++;
}

// Sample transition function for testing.
static void test_transition(uint64_t *next_state, const uint64_t *input, const uint64_t *state, size_t n, size_t s) {
    for (size_t i = 0; i < (s + 63) / 64; i++) {
        next_state[i] = input[i % ((n + 63) / 64)] ^ state[i];
    }
}

// Sample output function for testing.
static void test_output(uint64_t *output, const uint64_t *state, size_t m, size_t s) {
    for (size_t i = 0; i < (m + 63) / 64; i++) {
        output[i] = state[i % ((s + 63) / 64)];
    }
}

// Test memory management with Moore machine operations.
void memory_tests_run(void) {
    memory_test_stats_t *stats = get_memory_test_stats();
    stats->total_calls = 0;
    stats->alloc_calls = 0;
    stats->fail_at_call = 0;
    stats->alloc_count = 0;
    stats->free_count = 0;
    stats->failed_function = NULL;

    // Test 1: Create and delete a simple Moore machine.
    uint64_t initial_state[] = {0};
    moore_t *m1 = ma_create_full(2, 2, 2, test_transition, test_output, initial_state);
    assert(m1 != NULL);
    ma_delete(m1);
    assert(stats->alloc_count == stats->free_count);

    // Test 2: Create a simple machine and simulate memory failure.
    stats->fail_at_call = 1; // Fail on first allocation.
    moore_t *m2 = ma_create_simple(3, 4, test_transition);
    assert(m2 == NULL);
    assert(errno == ENOMEM);
    stats->fail_at_call = 0; // Reset for next tests.

    // Test 3: Connect two machines and test memory allocation.
    moore_t *m3 = ma_create_full(4, 2, 3, test_transition, test_output, initial_state);
    moore_t *m4 = ma_create_full(2, 4, 3, test_transition, test_output, initial_state);
    assert(m3 != NULL && m4 != NULL);
    assert(ma_connect(m3, 0, m4, 0, 2) == 0);
    ma_delete(m3);
    ma_delete(m4);
    assert(stats->alloc_count == stats->free_count);

    // Test 4: Step multiple machines with memory allocation failure.
    moore_t *m5 = ma_create_simple(2, 2, test_transition);
    moore_t *m6 = ma_create_simple(2, 2, test_transition);
    moore_t *machines[] = {m5, m6};
    stats->fail_at_call = 1; // Fail during step.
    assert(ma_step(machines, 2) == -1);
    assert(errno == ENOMEM);
    ma_delete(m5);
    ma_delete(m6);
    assert(stats->alloc_count == stats->free_count);

    // Verify memory statistics.
    assert(stats->total_calls >= 10);
    assert(stats->alloc_count >= 4);
    assert(stats->alloc_count == stats->free_count);
}
