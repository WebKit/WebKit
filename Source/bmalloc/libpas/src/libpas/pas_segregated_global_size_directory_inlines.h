/*
 * Copyright (c) 2019-2020 Apple Inc. All rights reserved.
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

#ifndef PAS_SEGREGATED_GLOBAL_SIZE_DIRECTORY_INLINES_H
#define PAS_SEGREGATED_GLOBAL_SIZE_DIRECTORY_INLINES_H

#include "pas_baseline_allocator_result.h"
#include "pas_baseline_allocator_table.h"
#include "pas_count_lookup_mode.h"
#include "pas_local_allocator.h"
#include "pas_segregated_heap.h"
#include "pas_segregated_global_size_directory.h"
#include "pas_thread_local_cache.h"

PAS_BEGIN_EXTERN_C;

PAS_API pas_baseline_allocator_result
pas_segregated_global_size_directory_get_allocator_from_tlc(
    pas_segregated_global_size_directory* directory,
    size_t count,
    pas_count_lookup_mode count_lookup_mode,
    pas_heap_config* config,
    unsigned* cached_index);

PAS_API pas_baseline_allocator*
pas_segregated_global_size_directory_select_allocator_slow(
    pas_segregated_global_size_directory* directory);

static PAS_ALWAYS_INLINE pas_baseline_allocator_result
pas_segregated_global_size_directory_select_allocator(
    pas_segregated_global_size_directory* directory,
    size_t count,
    pas_count_lookup_mode count_lookup_mode,
    pas_heap_config* config,
    unsigned* cached_index)
{
    if (pas_segregated_global_size_directory_has_tlc_allocator(directory)
        && (pas_thread_local_cache_try_get() || pas_thread_local_cache_can_set())) {
        return pas_segregated_global_size_directory_get_allocator_from_tlc(
            directory, count, count_lookup_mode, config, cached_index);
    }

    for (;;) {
        unsigned index;
        pas_baseline_allocator* allocator;

        index = directory->baseline_allocator_index;
        if (index >= PAS_NUM_BASELINE_ALLOCATORS)
            allocator = pas_segregated_global_size_directory_select_allocator_slow(directory);
        else {
            allocator = pas_baseline_allocator_table + index;
            
            pas_lock_lock(&allocator->lock);
            
            if (directory->baseline_allocator_index != index) {
                pas_lock_unlock(&allocator->lock);
                continue;
            }
        }

        PAS_ASSERT(pas_segregated_view_get_global_size_directory(allocator->u.allocator.view) == directory);

        return pas_baseline_allocator_result_create_success(
            &allocator->u.allocator, &allocator->lock);
    }
}

static PAS_ALWAYS_INLINE size_t
pas_segregated_global_size_directory_local_allocator_size_for_null_config(void)
{
    return PAS_LOCAL_ALLOCATOR_SIZE(0);
}

static PAS_ALWAYS_INLINE size_t pas_segregated_global_size_directory_local_allocator_size_for_config(
    pas_segregated_page_config config)
{
    PAS_ASSERT(config.base.is_enabled);
    return PAS_LOCAL_ALLOCATOR_SIZE(config.num_alloc_bits);
}

static inline pas_allocator_index
pas_segregated_global_size_directory_num_allocator_indices_for_allocator_size(size_t size)
{
    pas_allocator_index result;

    result = (pas_allocator_index)(size / 8);
    PAS_TESTING_ASSERT((size_t)result * 8 == size);
    
    return result;
}

static PAS_ALWAYS_INLINE pas_allocator_index
pas_segregated_global_size_directory_num_allocator_indices_for_config(
    pas_segregated_page_config config)
{
    static const bool verbose = false;
    
    size_t size;

    PAS_ASSERT(config.base.is_enabled);
    
    size = pas_segregated_global_size_directory_local_allocator_size_for_config(config);

    if (verbose) {
        pas_log(
            "kind = %s, size = %zu.\n",
            pas_segregated_page_config_kind_get_string(config.kind),
            size);
    }

    return pas_segregated_global_size_directory_num_allocator_indices_for_allocator_size(size);
}

PAS_END_EXTERN_C;

#endif /* PAS_SEGREGATED_GLOBAL_SIZE_DIRECTORY_INLINES_H */

