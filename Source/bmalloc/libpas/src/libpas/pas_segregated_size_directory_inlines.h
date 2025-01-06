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
#include "pas_size_lookup_mode.h"
#include "pas_local_allocator.h"
#include "pas_segregated_directory_inlines.h"
#include "pas_segregated_heap.h"
#include "pas_segregated_size_directory.h"
#include "pas_thread_local_cache.h"

PAS_BEGIN_EXTERN_C;

PAS_API pas_baseline_allocator_result
pas_segregated_size_directory_get_allocator_from_tlc(
    pas_segregated_size_directory* directory,
    size_t size,
    pas_size_lookup_mode size_lookup_mode,
    const pas_heap_config* config,
    unsigned* cached_index);

PAS_API pas_baseline_allocator*
pas_segregated_size_directory_select_allocator_slow(
    pas_segregated_size_directory* directory);

static PAS_ALWAYS_INLINE pas_baseline_allocator_result
pas_segregated_size_directory_select_allocator(
    pas_segregated_size_directory* directory,
    size_t size,
    pas_size_lookup_mode size_lookup_mode,
    const pas_heap_config* config,
    unsigned* cached_index)
{
    if (pas_segregated_size_directory_has_tlc_allocator(directory)
        && (pas_thread_local_cache_try_get() || pas_thread_local_cache_can_set())) {
        return pas_segregated_size_directory_get_allocator_from_tlc(
            directory, size, size_lookup_mode, config, cached_index);
    }

    for (;;) {
        unsigned index;
        pas_baseline_allocator* allocator;

        index = pas_segregated_size_directory_baseline_allocator_index(directory);
        if (index >= PAS_NUM_BASELINE_ALLOCATORS)
            allocator = pas_segregated_size_directory_select_allocator_slow(directory);
        else {
            allocator = pas_baseline_allocator_table + index;
            
            pas_lock_lock(&allocator->lock);
            
            if (pas_segregated_size_directory_baseline_allocator_index(directory) != index) {
                pas_lock_unlock(&allocator->lock);
                continue;
            }
        }

        PAS_ASSERT(pas_segregated_view_get_size_directory(allocator->u.allocator.view) == directory);

        return pas_baseline_allocator_result_create_success(
            &allocator->u.allocator, &allocator->lock);
    }
}

static PAS_ALWAYS_INLINE size_t
pas_segregated_size_directory_local_allocator_size_for_null_config(void)
{
    return PAS_LOCAL_ALLOCATOR_SIZE(0);
}

static PAS_ALWAYS_INLINE size_t pas_segregated_size_directory_local_allocator_size_for_config(
    pas_segregated_page_config config)
{
    PAS_ASSERT(config.base.is_enabled);
    return PAS_LOCAL_ALLOCATOR_SIZE(config.num_alloc_bits);
}

static inline pas_allocator_index
pas_segregated_size_directory_num_allocator_indices_for_allocator_size(size_t size)
{
    pas_allocator_index result;

    result = (pas_allocator_index)(size / 8);
    PAS_TESTING_ASSERT((size_t)result * 8 == size);
    
    return result;
}

static PAS_ALWAYS_INLINE pas_allocator_index
pas_segregated_size_directory_num_allocator_indices_for_config(
    pas_segregated_page_config config)
{
    static const bool verbose = PAS_SHOULD_LOG(PAS_LOG_SEGREGATED_HEAPS);
    
    size_t size;

    PAS_ASSERT(config.base.is_enabled);
    
    size = pas_segregated_size_directory_local_allocator_size_for_config(config);

    if (verbose) {
        pas_log(
            "kind = %s, size = %zu.\n",
            pas_segregated_page_config_kind_get_string(config.kind),
            size);
    }

    return pas_segregated_size_directory_num_allocator_indices_for_allocator_size(size);
}

static PAS_ALWAYS_INLINE unsigned
pas_segregated_size_directory_take_first_eligible_impl_should_consider_view_parallel(
    pas_segregated_directory_bitvector_segment segment,
    pas_segregated_directory_iterate_config* config)
{
    PAS_UNUSED_PARAM(config);
    
    return segment.eligible_bits;
}

static PAS_ALWAYS_INLINE pas_segregated_view
pas_segregated_size_directory_take_first_eligible_impl(
    pas_segregated_directory* directory,
    pas_segregated_directory_iterate_config* config,
    pas_segregated_view (*create_new_view_callback)(
        pas_segregated_directory_iterate_config* config))
{
    static const bool verbose = PAS_SHOULD_LOG(PAS_LOG_SEGREGATED_HEAPS);
    
    bool did_find_something;
    const pas_segregated_page_config* page_config_ptr;
    pas_segregated_page_config page_config;
    pas_segregated_view view;

    page_config_ptr = pas_segregated_page_config_kind_get_config(directory->page_config_kind);
    page_config = *page_config_ptr;

    view = NULL;
    
    if (verbose)
        pas_log("%p: At start of take_first_eligible_impl.\n", directory);
    
    for (;;) {
        if (verbose)
            pas_log("%p: At top of take_first_eligible_impl loop.\n", directory);
        
        config->directory = directory;
        config->should_consider_view_parallel =
            pas_segregated_size_directory_take_first_eligible_impl_should_consider_view_parallel;
        config->consider_view = NULL;
        config->arg = NULL;

        did_find_something = pas_segregated_directory_iterate_forward_to_take_first_eligible(config);

        if (did_find_something) {
            view = pas_segregated_directory_get(directory, config->index);
            
            if (verbose) {
                pas_log("At index %zu: Taking existing view %p.\n",
                        (size_t)config->index,
                        pas_segregated_view_get_ptr(view));
            }
            
            PAS_TESTING_ASSERT(config->segment.eligible_bits & config->bit_reference.mask);

            if (PAS_SEGREGATED_DIRECTORY_BIT_REFERENCE_SET(
                    directory, config->bit_reference, eligible, false)) {
                if (verbose)
                    pas_log("Did take first eligible.\n");

                break;
            }
        } else {
            size_t new_size;
            
            pas_heap_lock_lock_conditionally(
                pas_segregated_page_config_heap_lock_hold_mode(page_config));
            
            new_size = pas_segregated_directory_size(directory);
            PAS_ASSERT(new_size >= config->index);
            if (new_size == config->index) {
                view = create_new_view_callback(config);
                pas_segregated_directory_append(directory, config->index, view);
                pas_heap_lock_unlock_conditionally(
                    pas_segregated_page_config_heap_lock_hold_mode(page_config));
                break;
            }
            
            pas_heap_lock_unlock_conditionally(
                pas_segregated_page_config_heap_lock_hold_mode(page_config));
        }
    }
    
    if (verbose) {
        pas_log("Taking eligible view %p (%s).\n",
                pas_segregated_view_get_ptr(view),
                pas_segregated_view_kind_get_string(pas_segregated_view_get_kind(view)));
    }

    PAS_TESTING_ASSERT(!PAS_SEGREGATED_DIRECTORY_GET_BIT(
                           directory, pas_segregated_view_get_index(view), eligible));
    
    return view;
}

PAS_END_EXTERN_C;

#endif /* PAS_SEGREGATED_GLOBAL_SIZE_DIRECTORY_INLINES_H */

