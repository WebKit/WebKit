/*
 * Copyright (c) 2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

/*
 * === Overview ===
 * This is a rudimentary stat-counter collection system, intended primarily for
 * tracking libpas-internal statistics & performance data.
 *
 * By default, it is disabled -- `#define PAS_ENABLE_STATS 1` will enable it at
 * compile time.
 * Even when compiled in, each individual metric is by default disabled at runtime.
 * To enable one at runtime you must pass a comma-separated
 * list of stat names to the PAS_STATS_ENABLE environment variable
 * e.g. `PAS_STATS_ENABLE=malloc_info_bytes,malloc_info_allocations`
 * or, to enable all stat-counters: `PAS_STATS_ENABLE=1`
 *
 * === Logging Stats ===
 * Stats are logged as textual json dumps at periodic intervals,
 * effectively producing a .jsonl file in the output. See below for a schema.
 * Only stats which are enabled are logged.
 *
 * Rough json schema (prettified):
 * ```
 * {
 *   "pid": <INT>,
 *   "time_ns": <INT>,
 *   "per_stat_data": {
 *     "<STATNAME_1>": {
 *        ...
 *     },
 *     "<STATNAME_2>": {
 *        ...
 *     },
 *     ...
 *     "<STATNAME_N>": {
 *        ...
 *     }
 *   }
 * }
 * ```
 *
 * Stats are logged to a 'sink'.
 * By default, this sink is stdout, but you can set
 * `PAS_STATS_LOG_FILE=<filename>` to instead log to a file.
 *
 * === Adding New Stats ===
 *
 * To add a new stat counter, you need to do two things:
 * 1) Add a new entry in PAS_STATS_FOR_EACH_COUNTER, including
 *   - the name of the stat (e.g. `mmap_count`)
 *   - a struct (e.g. `struct mmap_count_data`) which will be used
 *     to store the stat counter data accumulated at runtime
 *   - a function to dump that struct to json (e.g. mmap_count_dump_to_json)
 * 2) Define a new macro PAS_RECORD_STATE_<name> which takes a pointer to the
 *    data struct registered above, other arguments as necessary, and then
 *    actually accumulates into that struct.
 * Then go add PAS_RECORD_STAT(<name>, arg1, ..., argN); wherever in the
 * codebase to capture into your new stat-counter.
 *
 * === Stat Logging Design Notes ===
 *
 * Since not all environments in which libpas runs have a clean 'exit' hook
 * (e.g. Safari tends to call terminate() in order to ensure it exits quickly)
 * this system instead logs all counters periodically, based on the total number
 * of stat-count-events which have taken place across all threads:
 * PAS_STATS_LOG_INTERVAL controls the rate at which this happens.
 * The reason for not doing this in the scavenger is that the scavenger runs
 * infrequently enough that relying on it leads to significant under-reporting
 * of stat counter values.
 *
 * It would be relatively easy to add a subsidiary time-based interval check:
 * just make sure that current_time_ns() is only called inside
 * pas_stats_do_accounting_before_recording_stat_slow_path(), i.e. underneath
 * the check for `new_count == PAS_STATS_LOG_INTERVAL`.
 *
 * Note that PAS_STATS_LOG_INTERVAL is fixed and does not depend on how many /
 * which counters are actually enabled. If you add a counter that is not hit
 * very often, and enable only that counter, you may not see that counterget
 * logged during runtime. To get around this you can either A) temporarily
 * change PAS_STATS_LOG_INTERVAL and recompile, or B) enable some other stats
 * that are accumulated more frequently.
 *
 * === Implementation Notes ===
 *
 * In order to make the stat-collection-sites inlineable in libpas' C codebase,
 * we use a C-style macro-for-each to enumerate the stats that are available for
 * collection.
 * To register a new stat-counter, you must do three things:
 *   1. Add a new line to PAS_STATS_FOR_EACH_COUNTER with
 *     - the name of your stat
 *     - the struct to be used to carry its data: the first member of this
 *       struct should be of type `pas_stats_stat_base` and named 'base'.
 *     - a function which serializes your struct to json: this struct should not
 *       allocate memory of its own, but instead prefer calling
 *       `pas_stats_ensure_print_buffer` with whatever size of memory is
 *       necessary.
 *   2. Define a new PAS_RECORD_STAT_<statname> macro with the desired arguments
 *     - this should call the function which actually modifies your stat-struct;
 *       this function must handle its own synchronization.
 *   3. Add calls to PAS_RECORD_STAT(<statname>, <args...>) to the relevant
 *      points inside of libpas.
 * The fact that we need #2 is somewhat undesirable (in principle we should just
 * generate that table automatically from PAS_STATS_FOR_EACH_COUNTER) but I
 * couldn't find a way to do so ergonomically.
 *
 * Since this framework is intended for use inside of an allocator, it is
 * intended to have low overhead (both for enabled and disabled counters) and to
 * have minimal use of heap-allocated memory -- however, there is room for
 * improvement on both counts.
 *   - Re.: heap-allocated memory, on the logging path we currently do rely on
 *     heap allocations to make it easier for people to add new counters, as
 *     using a fixed-size static allocation per counter would mean every counter
 *     would need to pre-compute the theoretical maximum size of its json
 *     payload.
 *     Normally the utility heap would be a good fit for this use-case, but to
 *     avoid reentrancy we do not use libpas to allocate this memory -- even by
 *     going through the system heap. Instead, we call `malloc` directly. These
 *     buffers are cached so it shouldn't happen often but it would be better to
 *     be able to remove that dependency.
 *   - Re.: performance, the current design is not bad but does introduce a lot
 *     of atomic traffic and cross-core contention. Ideally, we would instead
 *     have a per-thread 'local stat counter cache' which we would then
 *     periodically accumulate into a global stat-counter object. Individual
 *     stat counters would have to be aware since they need to implement their
 *     own accumulate functions.
 *     Even better than thread-local would be if we had something like Linux'
 *     rseqs, as we could then store this data per-CPU and avoid any migration
 *     whatsoever.
 *     In both cases though, we would risk under-counting statistics unless we
 *     implemented an analog of what pas-TLCs do where they iterate over other
 *     threads' TLCs and collect data out. Doing so generically across all kinds
 *     of stat counters seems like a challenge.
 */

#include "pas_config.h"

#include "pas_allocation_mode.h"
#include "pas_lock.h"
#include "pas_internal_config.h"
#include "pas_utils.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

PAS_BEGIN_EXTERN_C;

/**********************************/
/* General stat-counter machinery */
/**********************************/

// Each stat-counter must provide a json dumper to serialize the stat counter
// data to json, which can then be composed with other stats and logged to a
// log-sink.
typedef char* (*pas_stats_json_dump_function)(void* stat);

/* Statistics sink for output */
typedef struct pas_stats_sink pas_stats_sink;
typedef void (*pas_stats_sink_output_func)(pas_stats_sink* sink, const char* json_output);

// ptr and length are always owned by pas_stats_ensure_print_buffer, and the
// underlying storage should be considered an implementation detail.
typedef struct pas_stats_print_buffer {
    char* ptr;
    size_t length;
} pas_stats_print_buffer;
char* pas_stats_ensure_print_buffer(pas_stats_print_buffer*, size_t desired_len);

// All stat-counter structs must have a field of this type named 'base' as
// their first member
typedef struct {
    const char* name;
    pas_stats_json_dump_function dumper;
    pas_stats_print_buffer buffer;
    bool enabled;
} pas_stats_stat_base;

/******************************************************************/
/* Implementations for the individual stat-counter instantiations */
/******************************************************************/

typedef enum {
    pas_stats_heap_type_segregated = 0,
    pas_stats_heap_type_bitfit = 1,
    pas_stats_heap_type_large = 2,
    pas_stats_heap_type_unknown = 3,

    pas_stats_heap_type_count
} pas_stats_heap_type;

/******************************************************/
/***** Size-bucket logic (used by multiple stats) *****/
/******************************************************/

/* 
 * The goal here is to track some stat (# allocs, # bytes) across varying
 * allocation-sizes. Rather than using a hashmap, we use a fixed number of
 * buckets: this works well enough since libpas itself only allocates objects
 * as belonging to a given size-class.
 * Obviously extending this to 'all possible allocation sizes' would have
 * diminishing returns, so all objects above
 * PAS_STATS_MALLOC_INFO_LOW_GRANULARITY_MAX_SIZE just get lumped into a single
 * bucket.
 *
 * Instead of using a constant bucket width for all allocations, we vary
 * them such that smaller allocations get logged with a higher granularity.
 * Since the libpas size-category boundaries differ from pas_heap to pas_heap, we
 * don't attempt to use those -- instead the numbers below are intended to be a
 * conservative superset, such that we get at least as much granularity as the
 * actual pas size-class for a given allocation size.
 *
 * To justify this scheme: the naive, non-tiered approach would involve
 * MAX_SIZE / BUCKET_SIZE =
 * (1 << PAS_VA_BASED_ZERO_MEMORY_SHIFT) / (1 << MIN_ALIGN_SHIFT) =
 * ((1 << 24) / (1 << 4)) * 8B = 8MiB of storage per malloc_info stat counter,
 * which memory-wise is perhaps excusable since that price is only paid when
 * we're building with PAS_ENABLE_STATS, but that takes a copious amount of time
 * to log which significantly distorts the stat logging functionality.
 *
 * The actual sizes are technically arbitrary, but they are chosen to dominate
 * the typical size-category boundaries used by the bmalloc_heap &c.
 */
// Just above the typical maximum small-object size, ~1600KB
#define PAS_STATS_MALLOC_INFO_HIGH_GRANULARITY_MAX_SIZE   (1ull << 11)
// 32K -- almost always the small-medium boundary
#define PAS_STATS_MALLOC_INFO_MEDIUM_GRANULARITY_MAX_SIZE (1ull << 15)
// Arbitrary maximum size, tuned to where we see allocation-counts drop off
#define PAS_STATS_MALLOC_INFO_LOW_GRANULARITY_MAX_SIZE    (1ull << (PAS_VA_BASED_ZERO_MEMORY_SHIFT + 2))

#define PAS_STATS_MALLOC_INFO_HIGH_GRANULARITY_SHIFT      PAS_MIN_ALIGN_SHIFT
#define PAS_STATS_MALLOC_INFO_HIGH_GRANULARITY            (1ull << PAS_STATS_MALLOC_INFO_HIGH_GRANULARITY_SHIFT)
#define PAS_STATS_MALLOC_INFO_HIGH_GRANULARITY_BUCKETS    (PAS_STATS_MALLOC_INFO_HIGH_GRANULARITY_MAX_SIZE / PAS_STATS_MALLOC_INFO_HIGH_GRANULARITY)
#define PAS_STATS_MALLOC_INFO_MEDIUM_GRANULARITY_SHIFT    PAS_MIN_MEDIUM_ALIGN_SHIFT
#define PAS_STATS_MALLOC_INFO_MEDIUM_GRANULARITY          (1ull << PAS_STATS_MALLOC_INFO_MEDIUM_GRANULARITY_SHIFT)
#define PAS_STATS_MALLOC_INFO_MEDIUM_GRANULARITY_BUCKETS  (PAS_STATS_MALLOC_INFO_MEDIUM_GRANULARITY_MAX_SIZE / PAS_STATS_MALLOC_INFO_MEDIUM_GRANULARITY)
#define PAS_STATS_MALLOC_INFO_LOW_GRANULARITY_SHIFT       PAS_MIN_MARGE_ALIGN_SHIFT
#define PAS_STATS_MALLOC_INFO_LOW_GRANULARITY             (1ull << PAS_STATS_MALLOC_INFO_LOW_GRANULARITY_SHIFT)
#define PAS_STATS_MALLOC_INFO_LOW_GRANULARITY_BUCKETS     (PAS_STATS_MALLOC_INFO_LOW_GRANULARITY_MAX_SIZE / PAS_STATS_MALLOC_INFO_LOW_GRANULARITY)
// FIXME: handle larger sizes in a sane way -- powers of two?
#define PAS_STATS_MALLOC_INFO_OVERSIZE_BUCKETS            (1ull)

#define PAS_STATS_MALLOC_INFO_BUCKET_COUNT_PER_STAT (\
    PAS_STATS_MALLOC_INFO_HIGH_GRANULARITY_BUCKETS + \
    PAS_STATS_MALLOC_INFO_MEDIUM_GRANULARITY_BUCKETS + \
    PAS_STATS_MALLOC_INFO_LOW_GRANULARITY_BUCKETS + \
    PAS_STATS_MALLOC_INFO_OVERSIZE_BUCKETS)

/****************************************/
/***** malloc-info stat definitions *****/
/****************************************/

typedef struct {
    pas_stats_stat_base base;
    uint64_t total_count;
    uint64_t count_by_heap_type[pas_stats_heap_type_count];
    uint64_t count_by_size[PAS_STATS_MALLOC_INFO_BUCKET_COUNT_PER_STAT];
} pas_stats_malloc_info_data;

typedef struct {
    pas_stats_stat_base base;
    uint64_t total_bytes_mapped;
    uint64_t bytes_mapped_mte_tagged;
    uint64_t bytes_mapped_may_contain_small_or_medium;
} pas_stats_page_alloc_counts_data;

#define PAS_STATS_UINT64_MAX_STRING_LEN 20

#if PAS_ENABLE_STATS

PAS_API char* pas_stats_malloc_info_dump_to_json(void* stat_v);
PAS_API void pas_stats_malloc_info_record(pas_stats_malloc_info_data* data, pas_stats_heap_type heap_type, size_t size, size_t count);

PAS_API char* pas_stats_page_alloc_counts_dump_to_json(void* stat_v);
PAS_API void pas_stats_page_alloc_counts_record(pas_stats_page_alloc_counts_data* data, size_t size, bool may_contain_small_or_medium, bool mapped_with_mte);

// Arguments:
//   - stat-name
//   - struct used to store stat data
//   - function used to dump that struct to json
#define PAS_STATS_FOR_EACH_COUNTER(OP) \
    OP(page_alloc_counts, pas_stats_page_alloc_counts_data, pas_stats_page_alloc_counts_dump_to_json) \
    OP(malloc_info_bytes, pas_stats_malloc_info_data, pas_stats_malloc_info_dump_to_json) \
    OP(malloc_info_allocations, pas_stats_malloc_info_data, pas_stats_malloc_info_dump_to_json) \
    OP(malloc_info_mte_tagged_bytes, pas_stats_malloc_info_data, pas_stats_malloc_info_dump_to_json)


// FIXME: in principle it should be possible to automatically generate this via PAS_STATS_FOR_EACH_COUNTER, somehow
#define PAS_RECORD_STAT_page_alloc_counts(data, size, may_contain_small_or_medium, mapped_with_mte) \
    pas_stats_page_alloc_counts_record(data, size, may_contain_small_or_medium, mapped_with_mte)
#define PAS_RECORD_STAT_malloc_info_bytes(data, heap_type, size) \
    pas_stats_malloc_info_record(data, heap_type, size, size)
#define PAS_RECORD_STAT_malloc_info_allocations(data, heap_type, size) \
    pas_stats_malloc_info_record(data, heap_type, size, 1)
#define PAS_RECORD_STAT_malloc_info_mte_tagged_bytes(data, heap_type, size) \
    pas_stats_malloc_info_record(data, heap_type, size, size)

#endif /* PAS_ENABLE_STATS */

#define PAS_RECORD_STAT_MALLOC(heap_type, size) do { \
        PAS_RECORD_STAT(malloc_info_bytes, heap_type, size); \
        PAS_RECORD_STAT_WITHOUT_LOGGING(malloc_info_allocations, heap_type, size); \
    } while (0)

/*******************************************/
/* Back to general stat-counter machinery  */
/*******************************************/

#if PAS_ENABLE_STATS
PAS_API void pas_stats_do_accounting_before_recording_stat(void);

// It's OK for the .enabled check here to be non-atomic: we know that all
// .enabled bits will start initialized to true, and will be set to false at most once.
// If a thread fails to observe that write-to-0, the consequence is that it will
// make a few unnecessary atomic writes to some stat-counters, but those stat
// counters will never actually be used since pas_stats_log_all_enabled_stats
// does use an atomic check for whether .enabled is set.
#define PAS_RECORD_STAT(name, ...) do { \
        if (g_pas_stats_data.per_stat_data. name .base.enabled) { \
            pas_stats_do_accounting_before_recording_stat(); \
            PAS_RECORD_STAT_ ## name(&(g_pas_stats_data.per_stat_data. name ), __VA_ARGS__); \
        } \
    } while (0)
// This version does not call any setup/logging functions so as to reduce
// performance overhead in the case that the caller doesn't need them.
// Since this version does not call any setup/logging functions, it should only
// be called if you are A) ok with this stat not being logged unless other stats
// trigger a logging pass on their own, or B) sure that this stat will only be
// incremented after at least one other stat has called
// pas_stats_do_accounting_before_recording_stat().
#define PAS_RECORD_STAT_WITHOUT_LOGGING(name, ...) do { \
        if (g_pas_stats_data.per_stat_data. name .base.enabled) { \
            PAS_TESTING_ASSERT(g_pas_stats_data.start_time_ns); \
            PAS_RECORD_STAT_ ## name(&(g_pas_stats_data.per_stat_data. name ), __VA_ARGS__); \
        } \
    } while (0)

#define DECLARE_FIELD(name, type, dumper) type name;
typedef struct {
    PAS_STATS_FOR_EACH_COUNTER(DECLARE_FIELD)
} pas_stats_per_stat_data;
#undef DECLARE_FIELD

struct pas_stats_sink {
    pas_stats_sink_output_func output_func;
    void* context;
};

typedef struct {
    uint64_t log_counter;
    uint64_t start_time_ns;
    uint64_t pid;

    pas_stats_print_buffer buffer;

    pas_stats_sink sink;
    pas_lock log_lock; // Guards everything except per_stat_data
    pas_stats_per_stat_data per_stat_data;
} pas_stats_data;

PAS_API extern pas_stats_data g_pas_stats_data;

#define PAS_STATS_LOG_INTERVAL (1 << 16)

PAS_API void pas_stats_do_accounting_before_recording_stat_slow_path(void);
PAS_ALWAYS_INLINE void pas_stats_do_accounting_before_recording_stat(void)
{
    uint64_t new_count = pas_atomic_fetch_add_uint64_relaxed(&g_pas_stats_data.log_counter, 1);
    if (PAS_UNLIKELY(new_count == PAS_STATS_LOG_INTERVAL))
        pas_stats_do_accounting_before_recording_stat_slow_path();
}

#else /* !PAS_ENABLE_STATS */
#define PAS_RECORD_STAT(name, ...) do { PAS_UNUSED_V(__VA_ARGS__); } while (0)
#define PAS_RECORD_STAT_WITHOUT_LOGGING(name, ...) do { PAS_UNUSED_V(__VA_ARGS__); } while (0)
#endif /* PAS_ENABLE_STATS */

PAS_END_EXTERN_C;
