/*
 * Copyright (c) 2019-2021 Apple Inc. All rights reserved.
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

#ifndef PAS_STATUS_REPORTER_H
#define PAS_STATUS_REPORTER_H

#include "pas_heap.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

struct pas_bitfit_directory;
struct pas_large_heap;
struct pas_segregated_heap;
struct pas_segregated_shared_page_directory;
struct pas_segregated_size_directory;
struct pas_stream;
typedef struct pas_bitfit_directory pas_bitfit_directory;
typedef struct pas_large_heap pas_large_heap;
typedef struct pas_segregated_heap pas_segregated_heap;
typedef struct pas_segregated_shared_page_directory pas_segregated_shared_page_directory;
typedef struct pas_segregated_size_directory pas_segregated_size_directory;
typedef struct pas_stream pas_stream;

PAS_API extern unsigned pas_status_reporter_enabled;
PAS_API extern unsigned pas_status_reporter_period_in_microseconds;

PAS_API void pas_status_reporter_dump_bitfit_directory(
    pas_stream* stream, pas_bitfit_directory* directory);
PAS_API void pas_status_reporter_dump_segregated_size_directory(
    pas_stream* stream, pas_segregated_size_directory* directory);
PAS_API void pas_status_reporter_dump_segregated_shared_page_directory(
    pas_stream* stream, pas_segregated_shared_page_directory* directory);
PAS_API void pas_status_reporter_dump_large_heap(pas_stream* stream, pas_large_heap* heap);
PAS_API void pas_status_reporter_dump_large_map(pas_stream* stream);
PAS_API void pas_status_reporter_dump_heap_table(pas_stream* stream);
PAS_API void pas_status_reporter_dump_immortal_heap(pas_stream* stream);
PAS_API void pas_status_reporter_dump_compact_large_utility_free_heap(pas_stream* stream);
PAS_API void pas_status_reporter_dump_large_utility_free_heap(pas_stream* stream);
PAS_API void pas_status_reporter_dump_compact_bootstrap_free_heap(pas_stream* stream);
PAS_API void pas_status_reporter_dump_bootstrap_free_heap(pas_stream* stream);

PAS_API void pas_status_reporter_dump_bitfit_heap(pas_stream* stream, pas_bitfit_heap* heap);
PAS_API void pas_status_reporter_dump_segregated_heap(pas_stream* stream, pas_segregated_heap* heap);
PAS_API void pas_status_reporter_dump_heap(pas_stream* stream, pas_heap* heap);
PAS_API void pas_status_reporter_dump_all_heaps(pas_stream* stream);
PAS_API void pas_status_reporter_dump_all_shared_page_directories(pas_stream* stream);
PAS_API void pas_status_reporter_dump_all_heaps_non_utility_summaries(pas_stream* stream);
PAS_API void pas_status_reporter_dump_large_sharing_pool(pas_stream* stream);
PAS_API void pas_status_reporter_dump_utility_heap(pas_stream* stream);
PAS_API void pas_status_reporter_dump_total_fragmentation(pas_stream* stream);
PAS_API void pas_status_reporter_dump_view_stats(pas_stream* stream);
PAS_API void pas_status_reporter_dump_tier_up_rates(pas_stream* stream);
PAS_API void pas_status_reporter_dump_baseline_allocators(pas_stream* stream);
PAS_API void pas_status_reporter_dump_thread_local_caches(pas_stream* stream);
PAS_API void pas_status_reporter_dump_configuration(pas_stream* stream);
PAS_API void pas_status_reporter_dump_physical_page_sharing_pool(pas_stream* stream);
PAS_API void pas_status_reporter_dump_expendable_memories(pas_stream* stream);
PAS_API void pas_status_reporter_dump_everything(pas_stream* stream);

PAS_API void pas_status_reporter_start_if_necessary(void);

PAS_END_EXTERN_C;

#endif /* PAS_STATUS_REPORTER_H */

