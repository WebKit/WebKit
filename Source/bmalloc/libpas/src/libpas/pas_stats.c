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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_stats.h"

#if PAS_ENABLE_STATS

#if !PAS_OS(DARWIN) && !PAS_OS(LINUX)
#error "Cannot set PAS_ENABLE_STATS on a non-POSIX system"
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "pas_allocation_mode.h"
#include "pas_heap_lock.h"
#include "pas_immortal_heap.h"
#include "pas_utils.h"

static void pas_stats_default_sink_output(pas_stats_sink* sink, const char* json_output)
{
    PAS_UNUSED_PARAM(sink);
    printf("%s\n", json_output);
    fflush(stdout);
}

static void pas_stats_file_sink_output(pas_stats_sink* sink, const char* json_output)
{
    FILE* file = (FILE*)sink->context;
    fprintf(file, "%s\n", json_output);
    fflush(file);
}

#define INIT_FIELD(field_name, field_type, field_dumper) \
    . field_name = { \
        .base = { \
            .name = #field_name, \
            .dumper = field_dumper, \
            .buffer = { \
                .ptr = NULL, \
                .length = 0, \
            }, \
            .enabled = true, \
        } \
    },
pas_stats_data g_pas_stats_data = {
    .log_counter = (PAS_STATS_LOG_INTERVAL - 1),
    .start_time_ns = 0,
    .pid = 0,
    .log_lock = PAS_LOCK_INITIALIZER,
    .sink = {
        .output_func = pas_stats_default_sink_output,
        .context = NULL,
    },
    .per_stat_data = {
        PAS_STATS_FOR_EACH_COUNTER(INIT_FIELD)
    },
};
#undef INIT_FIELD

static uint64_t current_time_ns(void)
{
#if PAS_OS(DARWIN) || PAS_OS(LINUX)
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ((uint64_t)ts.tv_sec * 1000000000ULL) + ts.tv_nsec;
#else
    return 0;
#endif // PAS_OS(DARWIN) || PAS_OS(LINUX)
}

char* pas_stats_ensure_print_buffer(pas_stats_print_buffer* buf, size_t desired_len)
{
    // FIXME: find a way to use the utility heap without introducing re-entrancy concerns
    if (buf->ptr) {
        if (desired_len <= buf->length)
            return buf->ptr;
        free(buf->ptr);
    }
    buf->length = desired_len * 2;
    buf->ptr = calloc(1, buf->length);
    return buf->ptr;
}

static void pas_stats_process_stat_enablements(void)
{
    pas_lock_assert_held(&g_pas_stats_data.log_lock);

#define DISABLE_STAT(stat_name, type, dumper) g_pas_stats_data.per_stat_data. stat_name .base.enabled = false;
    PAS_STATS_FOR_EACH_COUNTER(DISABLE_STAT);
#undef DISABLE_STAT

    const char* env_name = "PAS_STATS_ENABLE";
    const char* env = getenv(env_name);
    if (!env || !*env)
        return;

    // Special case: if the setting is just '1' then enable all stats
    // This is fine because the stat names have to be valid identifiers,
    // and thus cannot start with a number.
    if (env[0] == '1') {
#define ENABLE_STAT(stat_name, type, dumper) g_pas_stats_data.per_stat_data. stat_name .base.enabled = true;
        PAS_STATS_FOR_EACH_COUNTER(ENABLE_STAT);
#undef ENABLE_STAT
        return;
    }

    // We don't use strdup+strtok because we don't want to allocate memory if we don't have to,
    // so instead we non-destructively scan the env-string in-place
    const char* p = env;
    for (;;) {
        // token = [p, comma_or_nul)
        const char* q = p;
        while (*q && *q != ',')
            ++q;

        // trim left/right whitespace without copying
        const char* s = p;
        while (s < q && isspace((unsigned char)*s))
            ++s;
        const char* e = q;
        while (e > s && isspace((unsigned char)e[-1]))
            --e;

        size_t len = (size_t)(e - s);

        // Brute-force match against each known stat name
        if (len) {
            int matched = 0;
            #define TRY_ENABLE(stat_name, type, dumper) \
                if (!matched) { \
                    const char* nm = #stat_name; \
                    if ((nm[len] == '\0') && !memcmp(s, nm, len)) { \
                        g_pas_stats_data.per_stat_data. stat_name .base.enabled = true; \
                        matched = 1; \
                    } \
                }

            PAS_STATS_FOR_EACH_COUNTER(TRY_ENABLE)
            #undef TRY_ENABLE
            if (!matched) {
                int carat_padding = (strlen(env_name) + 1) + (int)(p - env) + 1;
                fprintf(stderr, "error: unknown stat name in %s\n%s=%s\n%*c here\n",
                    env_name, env_name, env, carat_padding, '^');
            }
            PAS_ASSERT(matched);
        }

        // now at comma_or_nul
        if (*q == ',')
            p = q + 1;
        else
            break;
    }
}

static void pas_stats_setup_logging(void)
{
    pas_lock_assert_held(&g_pas_stats_data.log_lock);

    // The default sink 'just works'; additional setup is only needed if
    // the user wants to log stats to a file.
    const char* env = getenv("PAS_STATS_LOG_FILE");
    if (!env || !*env)
        return;

    FILE* file = fopen(env, "w");
    PAS_ASSERT(file);

    g_pas_stats_data.sink.context = (void*)file;
    g_pas_stats_data.sink.output_func = pas_stats_file_sink_output;
    g_pas_stats_data.pid = getpid();
    g_pas_stats_data.start_time_ns = current_time_ns();
}

// This setup is only called the first time statistics are actually logged.
// We use it to handle enablement via a sort of hack: all stats are enabled to
// begin with, but the first one to accrue a counter will check envvars and
// disable all the ones which shouldn't actually be enabled. This may lead to
// some unecessary atomic writes at the beginning, but it avoids placing an
// extra pthread_once check inline with every statistic, both enabled and
// disabled.
static void pas_stats_setup(void)
{
    pas_stats_process_stat_enablements();
    pas_stats_setup_logging();
}

static void pas_stats_setup_if_necessary(void)
{
    static pthread_once_t once_control = PTHREAD_ONCE_INIT;
    pthread_once(&once_control, pas_stats_setup);
}

/*
 * Stats are logged as json; the rough schema is
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
 */
static void pas_stats_log_all_enabled_stats(void)
{
    pas_lock_assert_held(&g_pas_stats_data.log_lock);

    uint64_t log_time = current_time_ns() - g_pas_stats_data.start_time_ns;

#define COUNT_ONE(name, type, dumper_sym) + 1
    const unsigned pas_stats_count = 0 + PAS_STATS_FOR_EACH_COUNTER(COUNT_ONE);
#undef COUNT_ONE
    const char* names[pas_stats_count];
    char* values[pas_stats_count];
    size_t value_lens[pas_stats_count];
    size_t n_used = 0;

    // Collect enabled stats and call their dumpers
#define COLLECT_ONE(stat_name, type, dumper_sym) \
    do { \
        pas_stats_stat_base* base = &g_pas_stats_data.per_stat_data. stat_name .base; \
        if (base->dumper && base->enabled) { \
            char* json = base->dumper((void*)&g_pas_stats_data.per_stat_data. stat_name ); \
            if (json) { \
                names[n_used] = base->name; \
                values[n_used] = json; \
                value_lens[n_used] = strlen(json); \
                n_used++; \
            } \
        } \
    } while (0);

    PAS_STATS_FOR_EACH_COUNTER(COLLECT_ONE)
#undef COLLECT_ONE

    static const char* pid_header = "{\"pid\": ";
    static const char* timing_header = ", \"time_ns\": ";
    static const char* per_stat_header = ", \"per_stat_data\": {";
    static const char* footer = "}}\n";

    size_t total_len = 0;
    total_len += strlen(pid_header);
    total_len += PAS_STATS_UINT64_MAX_STRING_LEN;
    total_len += strlen(timing_header);
    total_len += PAS_STATS_UINT64_MAX_STRING_LEN;
    total_len += strlen(per_stat_header);
    total_len += strlen(footer);

    for (size_t i = 0; i < n_used; i++) {
        const char* name = names[i];
        size_t name_len = strlen(name);
        if (!i)
            total_len += strlen("\"") + name_len + strlen("\": ");
        else
            total_len += strlen(", \"") + name_len + strlen("\": ");
        total_len += value_lens[i];  /* value is a full JSON object string (with its own { }) */
    }

    char* out = pas_stats_ensure_print_buffer(&g_pas_stats_data.buffer, total_len + 1);
    PAS_ASSERT(out);

    /* Concatenate into the buffer */
    char* cursor = out;

    memcpy(cursor, pid_header, strlen(pid_header));
    cursor += strlen(pid_header);
    // +1 to account for null terminator, which we subsequently overwrite
    snprintf(cursor, PAS_STATS_UINT64_MAX_STRING_LEN + 1, "%*llu", PAS_STATS_UINT64_MAX_STRING_LEN, g_pas_stats_data.pid);
    cursor += PAS_STATS_UINT64_MAX_STRING_LEN;

    memcpy(cursor, timing_header, strlen(timing_header));
    cursor += strlen(timing_header);
    // +1 to account for null terminator, which we subsequently overwrite
    snprintf(cursor, PAS_STATS_UINT64_MAX_STRING_LEN + 1, "%*llu", PAS_STATS_UINT64_MAX_STRING_LEN, log_time);
    cursor += PAS_STATS_UINT64_MAX_STRING_LEN;

    memcpy(cursor, per_stat_header, strlen(per_stat_header));
    cursor += strlen(per_stat_header);

    for (size_t i = 0; i < n_used; i++) {
        const char* name = names[i];
        size_t name_len = strlen(name);

        const char* prefix = !i ? " \"" : ", \"";
        size_t prefix_len = strlen(prefix);
        memcpy(cursor, prefix, prefix_len);
        cursor += prefix_len;

        memcpy(cursor, name, name_len);
        cursor += name_len;

        const char* after_name = "\": ";
        size_t after_name_len = strlen(after_name);
        memcpy(cursor, after_name, after_name_len);
        cursor += after_name_len;

        memcpy(cursor, values[i], value_lens[i]);
        cursor += value_lens[i];
    }

    memcpy(cursor, footer, strlen(footer));
    cursor += strlen(footer);

    *cursor = '\0'; /* null-terminate */

    g_pas_stats_data.sink.output_func(&g_pas_stats_data.sink, out);
}

void pas_stats_do_accounting_before_recording_stat_slow_path(void)
{
    pas_lock_lock(&g_pas_stats_data.log_lock);

    pas_stats_setup_if_necessary();
    // By ensuring that the write of 0 to g_pas_stats_data.log_counter is only visible iff
    // the writes made by pas_stats_setup are, we ensure that no other code can enter
    // pas_stats_do_accounting_before_recording_stat_slow_path (except in the unlikely case
    // of a uint64_t overflow) without being aware of the proper logging configuration.
    pas_store_store_fence();
    pas_atomic_store_uint64(&g_pas_stats_data.log_counter, 0);

    // But wait until after resetting the log_counter so that we don't
    // add further, unnecessary space between log events
    pas_stats_log_all_enabled_stats();

    pas_lock_unlock(&g_pas_stats_data.log_lock);
}

static size_t pas_stats_malloc_info_bucket_idx_from_size(size_t size)
{
    size_t bucket_base;
    size_t surplus_size;

    if (size < PAS_STATS_MALLOC_INFO_HIGH_GRANULARITY_MAX_SIZE)
        return size >> PAS_STATS_MALLOC_INFO_HIGH_GRANULARITY_SHIFT;
    bucket_base = PAS_STATS_MALLOC_INFO_HIGH_GRANULARITY_BUCKETS;

    if (size < PAS_STATS_MALLOC_INFO_MEDIUM_GRANULARITY_MAX_SIZE) {
        surplus_size = size - PAS_STATS_MALLOC_INFO_HIGH_GRANULARITY_MAX_SIZE;
        return bucket_base + (surplus_size >> PAS_STATS_MALLOC_INFO_MEDIUM_GRANULARITY_SHIFT);
    }
    bucket_base += PAS_STATS_MALLOC_INFO_MEDIUM_GRANULARITY_BUCKETS;

    if (size < PAS_STATS_MALLOC_INFO_LOW_GRANULARITY_MAX_SIZE) {
        surplus_size = size - PAS_STATS_MALLOC_INFO_MEDIUM_GRANULARITY_MAX_SIZE;
        return bucket_base + (surplus_size >> PAS_STATS_MALLOC_INFO_LOW_GRANULARITY_SHIFT);
    }
    bucket_base += PAS_STATS_MALLOC_INFO_LOW_GRANULARITY_BUCKETS;

    return bucket_base;
}

// Returns the minimum size for the bucket
static size_t pas_stats_malloc_info_size_from_bucket_idx(size_t bucket_idx)
{
    const size_t high_granularity_bucket_bound = PAS_STATS_MALLOC_INFO_HIGH_GRANULARITY_BUCKETS;
    const size_t medium_granularity_bucket_bound = high_granularity_bucket_bound
        + PAS_STATS_MALLOC_INFO_MEDIUM_GRANULARITY_BUCKETS;
    const size_t low_granularity_bucket_bound = medium_granularity_bucket_bound
        + PAS_STATS_MALLOC_INFO_LOW_GRANULARITY_BUCKETS;

    if (bucket_idx < high_granularity_bucket_bound)
        return bucket_idx * PAS_STATS_MALLOC_INFO_HIGH_GRANULARITY;
    if (bucket_idx < medium_granularity_bucket_bound)
        return (bucket_idx - high_granularity_bucket_bound) * PAS_STATS_MALLOC_INFO_MEDIUM_GRANULARITY;
    if (bucket_idx < low_granularity_bucket_bound)
        return (bucket_idx - medium_granularity_bucket_bound) * PAS_STATS_MALLOC_INFO_LOW_GRANULARITY;
    return PAS_STATS_MALLOC_INFO_LOW_GRANULARITY_MAX_SIZE;
}

void pas_stats_malloc_info_record(pas_stats_malloc_info_data* data, pas_stats_heap_type heap_type, size_t size, size_t count)
{
    PAS_TESTING_ASSERT(data);
    PAS_TESTING_ASSERT(heap_type < pas_stats_heap_type_count);

    size_t size_bucket_idx = pas_stats_malloc_info_bucket_idx_from_size(size);

    pas_atomic_fetch_add_uint64_relaxed(&data->total_count, count);
    pas_atomic_fetch_add_uint64_relaxed(&data->count_by_heap_type[heap_type], count);
    pas_atomic_fetch_add_uint64_relaxed(&data->count_by_size[size_bucket_idx], count);
}

static const char* pas_stats_heap_type_to_string(pas_stats_heap_type heap_type)
{
    switch (heap_type) {
    case pas_stats_heap_type_segregated:
        return "segregated";
    case pas_stats_heap_type_bitfit:
        return "bitfit";
    case pas_stats_heap_type_large:
        return "large";
    case pas_stats_heap_type_unknown:
    default:
        return "unknown";
    }
}

char* pas_stats_malloc_info_dump_to_json(void* stat_v)
{
    /*
     * Rough schema:
     * ```
     * {
     *   "total_count": <NUM>,
     *   "count_by_heap_type": {
     *     "segregated": <NUM>,
     *     "bitfit": <NUM>,
     *     "large": <NUM>,
     *     "unknown": <NUM>,
     *   },
     *   "count_by_size": {
     *     "0": <NUM>,
     *     "16": <NUM>,
     *     "32": <NUM>,
     *     ...
     *     "16773120": <NUM>,
     *     "16777216": <NUM>
     *   }
     * }
     * ```
     */

    pas_stats_malloc_info_data* stat = (pas_stats_malloc_info_data*)stat_v;
    
    // Each "count_by_size" entry looks like
    // "<NUM>":<NUM>,
    // ergo 2*sizeof(<NUM>) + strlen("":,)
    const size_t json_bytes_per_bucket = (2 * PAS_STATS_UINT64_MAX_STRING_LEN) + 4;
    // 1024 is a conservative overestimate of the rest: total_count, heap_type
    // If this turns out to be wrong, then we will catch this with PAS_ASSERTs later
    size_t bufsize = 1024 + (json_bytes_per_bucket * PAS_STATS_MALLOC_INFO_BUCKET_COUNT_PER_STAT);
    char* buf = pas_stats_ensure_print_buffer(&stat->base.buffer, bufsize);

    char* p = buf;
    int n = snprintf(p, bufsize,
        "{ \"name\": \"%s\", \"total_count\": %llu, \"count_by_heap_type\": {",
        stat->base.name, (unsigned long long)stat->total_count);
    PAS_ASSERT(n >= 0);
    p += n;
    bufsize -= n;

    for (int i = 0; i < pas_stats_heap_type_count; i++) {
        n = snprintf(p, bufsize, "%s\"%s\":%llu",
            i ? ", " : "",
            pas_stats_heap_type_to_string((pas_stats_heap_type)i),
            (unsigned long long)stat->count_by_heap_type[i]);
        PAS_ASSERT(n >= 0);
        p += n;
        bufsize -= n;
    }

    n = snprintf(p, bufsize, "}, \"count_by_size\": {");
    PAS_ASSERT(n >= 0);
    p += n;
    bufsize -= n;

    for (size_t i = 0; i < PAS_STATS_MALLOC_INFO_BUCKET_COUNT_PER_STAT; i++) {
        n = snprintf(p, bufsize, "%s\"%llu\":%llu",
            i ? "," : "",
            (unsigned long long)pas_stats_malloc_info_size_from_bucket_idx(i),
            (unsigned long long)stat->count_by_size[i]);
        PAS_ASSERT(n >= 0);
        p += n;
        bufsize -= n;
    }

    n = snprintf(p, bufsize, "} }");
    PAS_ASSERT(n >= 0);

    return buf;
}

void pas_stats_page_alloc_counts_record(pas_stats_page_alloc_counts_data* data, size_t size, bool may_contain_small_or_medium, bool mapped_with_mte)
{
    PAS_TESTING_ASSERT(data);

    pas_atomic_fetch_add_uint64_relaxed(&data->total_bytes_mapped, size);
    if (mapped_with_mte)
        pas_atomic_fetch_add_uint64_relaxed(&data->bytes_mapped_mte_tagged, size);
    if (may_contain_small_or_medium)
        pas_atomic_fetch_add_uint64_relaxed(&data->bytes_mapped_may_contain_small_or_medium, size);
}


char* pas_stats_page_alloc_counts_dump_to_json(void* stat_v)
{
    /*
     * Rough schema:
     * ```
     * {
     *   "total_bytes_mapped": <NUM>,
     *   "bytes_mapped_mte_tagged": <NUM>,
     *   "bytes_mapped_may_contain_small_or_medium": <NUM>
     * }
     * ```
     */

    pas_stats_page_alloc_counts_data* stat = (pas_stats_page_alloc_counts_data*)stat_v;

    // 128 is a conservative overestimate of the identifier names + `":,` chars
    // 1024 is a conservative overestimate of `{"name": "page_alloc_counts"}`
    size_t bufsize = 1024 + (128 + PAS_STATS_UINT64_MAX_STRING_LEN) * 3;
    char* buf = pas_stats_ensure_print_buffer(&stat->base.buffer, bufsize);

    int n = snprintf(buf, bufsize,
        "{ \"name\": \"%s\", "
        "\"total_bytes_mapped\": %llu, "
        "\"bytes_mapped_mte_tagged\": %llu, "
        "\"bytes_mapped_may_contain_small_or_medium\": %llu "
        "}",
        stat->base.name,
        stat->total_bytes_mapped,
        stat->bytes_mapped_mte_tagged,
        stat->bytes_mapped_may_contain_small_or_medium);
    PAS_ASSERT(n >= 0);

    return buf;
}

#endif /* PAS_ENABLE_STATS */
#endif /* LIBPAS_ENABLED */
