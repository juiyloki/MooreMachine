#ifndef MEMORY_TESTS_H
#define MEMORY_TESTS_H

#include <stdint.h>

// Structure to store memory operation statistics.
typedef struct {
    volatile unsigned total_calls;    // Total calls to memory functions.
    volatile unsigned alloc_calls;    // Number of allocation attempts.
    volatile unsigned fail_at_call;   // Call number at which allocation fails.
    volatile unsigned alloc_count;    // Successful allocations.
    volatile unsigned free_count;     // Successful deallocations.
    volatile char *failed_function;   // Name of the function that failed.
} memory_test_stats_t;

// Get access to memory test statistics.
memory_test_stats_t *get_memory_test_stats(void);

// Test the memory management behavior of the library.
void memory_tests_run(void);

#endif
