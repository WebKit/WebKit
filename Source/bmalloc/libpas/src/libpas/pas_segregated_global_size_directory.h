/*
 * Copyright (c) 2018-2020 Apple Inc. All rights reserved.
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

#ifndef PAS_SEGREGATED_GLOBAL_SIZE_DIRECTORY_H
#define PAS_SEGREGATED_GLOBAL_SIZE_DIRECTORY_H

#include "pas_allocator_index.h"
#include "pas_compact_atomic_bitfit_global_size_class_ptr.h"
#include "pas_compact_atomic_page_sharing_pool_ptr.h"
#include "pas_compact_atomic_segregated_global_size_directory_ptr.h"
#include "pas_compact_atomic_thread_local_cache_layout_node.h"
#include "pas_compact_tagged_page_granule_use_count_ptr.h"
#include "pas_compact_tagged_unsigned_ptr.h"
#include "pas_config.h"
#include "pas_heap_config.h"
#include "pas_page_granule_use_count.h"
#include "pas_segregated_size_directory.h"

PAS_BEGIN_EXTERN_C;

struct pas_extended_segregated_global_size_directory_data;
struct pas_segregated_global_size_directory;
struct pas_segregated_global_size_directory_data;
typedef struct pas_extended_segregated_global_size_directory_data pas_extended_segregated_global_size_directory_data;
typedef struct pas_segregated_global_size_directory_data pas_segregated_global_size_directory_data;
typedef struct pas_segregated_global_size_directory pas_segregated_global_size_directory;
typedef struct pas_segregated_global_size_directory_data pas_segregated_global_size_directory_data;

PAS_DEFINE_COMPACT_ATOMIC_PTR(pas_segregated_global_size_directory_data,
                              pas_segregated_global_size_directory_data_ptr);

PAS_API extern bool pas_segregated_global_size_directory_can_explode;
PAS_API extern bool pas_segregated_global_size_directory_force_explode;

struct PAS_ALIGNED(sizeof(pas_versioned_field)) pas_segregated_global_size_directory {
    pas_segregated_directory base;
    
    pas_segregated_heap* heap;

    unsigned object_size : 27;
    unsigned alignment_shift : 5;

    /* This has a funny encoding (let N = PAS_NUM_BASELINE_ALLOCATORS):
       
       index in [0, N)     => attached to index
       index in [N, 2N)    => attaching to index - N
       2N                  => detached
       
       In the detached state, the only way to change this is to CAS it.
       
       In the attached and attaching states, the corresponding index's lock protects this field. */
    uint16_t baseline_allocator_index;
    
    pas_segregated_global_size_directory_data_ptr data;
    pas_compact_atomic_bitfit_global_size_class_ptr bitfit_size_class;

    /* The owning segregated heap holds these in a singly linked list. */
    pas_compact_atomic_segregated_global_size_directory_ptr next_for_heap;
};

struct pas_segregated_global_size_directory_data {
    unsigned offset_from_page_boundary_to_first_object; /* Cached to make refill fast. */
    unsigned offset_from_page_boundary_to_end_of_last_object; /* Cached to make refill fast. */
    
    pas_allocator_index allocator_index;
    
    uint8_t full_num_non_empty_words;

    pas_compact_tagged_unsigned_ptr full_alloc_bits; /* Precomputed alloc bits in the case that a page is empty. */
    pas_compact_atomic_page_sharing_pool_ptr bias_sharing_pool;
    pas_compact_atomic_thread_local_cache_layout_node next_for_layout;
};

struct pas_extended_segregated_global_size_directory_data {
    pas_segregated_global_size_directory_data base;
    pas_compact_tagged_page_granule_use_count_ptr full_use_counts;
};

static inline pas_segregated_view
pas_segregated_global_size_directory_as_view(pas_segregated_global_size_directory* directory)
{
    return pas_segregated_view_create(directory, pas_segregated_global_size_directory_view_kind);
}

static inline size_t
pas_segregated_global_size_directory_alignment(pas_segregated_global_size_directory* directory)
{
    return (size_t)1 << (size_t)directory->alignment_shift;
}

PAS_API pas_segregated_global_size_directory* pas_segregated_global_size_directory_create(
    pas_segregated_heap* heap,
    unsigned object_size,
    unsigned alignment,
    pas_heap_config* heap_config,
    pas_segregated_page_config* page_config);

static inline bool pas_segregated_global_size_directory_contention_did_trigger_explosion(
    pas_segregated_global_size_directory* directory)
{
    return pas_segregated_directory_get_misc_bit(&directory->base);
}

static inline bool pas_segregated_global_size_directory_set_contention_did_trigger_explosion(
    pas_segregated_global_size_directory* directory,
    bool value)
{
    return pas_segregated_directory_set_misc_bit(&directory->base, value);
}

PAS_API pas_segregated_global_size_directory_data* pas_segregated_global_size_directory_ensure_data(
    pas_segregated_global_size_directory* directory,
    pas_lock_hold_mode heap_lock_hold_mode);

PAS_API pas_extended_segregated_global_size_directory_data*
pas_segregated_global_size_directory_get_extended_data(
    pas_segregated_global_size_directory* directory);

/* Call with heap lock held. */
PAS_API void pas_segregated_global_size_directory_create_tlc_allocator(
    pas_segregated_global_size_directory* directory);

static inline bool pas_segregated_global_size_directory_has_tlc_allocator(
    pas_segregated_global_size_directory* directory)
{
    pas_segregated_global_size_directory_data* data;
    data = pas_segregated_global_size_directory_data_ptr_load(&directory->data);
    return data && data->allocator_index != (pas_allocator_index)UINT_MAX;
}

/* Call with heap lock held. */
PAS_API void pas_segregated_global_size_directory_enable_exclusive_views(
    pas_segregated_global_size_directory* directory);

static inline bool pas_segregated_global_size_directory_are_exclusive_views_enabled(
    pas_segregated_global_size_directory* directory)
{
    pas_segregated_global_size_directory_data* data;
    data = pas_segregated_global_size_directory_data_ptr_load(&directory->data);
    return data && data->full_num_non_empty_words;
}

PAS_API void pas_segregated_global_size_directory_explode(
    pas_segregated_global_size_directory* directory,
    pas_lock_hold_mode heap_lock_hold_mode);

PAS_API pas_segregated_view pas_segregated_global_size_directory_take_first_eligible_direct(
    pas_segregated_global_size_directory* directory);

PAS_API pas_segregated_view pas_segregated_global_size_directory_take_first_eligible(
    pas_segregated_global_size_directory* directory,
    pas_magazine* magazine);

PAS_API pas_page_sharing_pool_take_result
pas_segregated_global_size_directory_take_last_empty(
    pas_segregated_global_size_directory* directory,
    pas_deferred_decommit_log* log,
    pas_lock_hold_mode heap_lock_hold_mode);

PAS_API pas_segregated_global_size_directory* pas_segregated_global_size_directory_for_object(
    uintptr_t begin,
    pas_heap_config* config);

/* This assumes that we already have a data. That's a valid assumption if we have exclusives. */
PAS_API pas_heap_summary pas_segregated_global_size_directory_compute_summary_for_unowned_exclusive(
    pas_segregated_global_size_directory* directory);

/* We use a page sharing pool as our collection of biasing directories.
 
   Cool fact: page sharing pool iteration functions work for NULL. They'll behave as if you had an
   empty pool. So, no need to NULL-check if using the return value of this function to iterate the
   biasing directories. */
static inline pas_page_sharing_pool* pas_segregated_global_size_directory_bias_sharing_pool(
    pas_segregated_global_size_directory* directory)
{
    pas_segregated_global_size_directory_data* data;
    data = pas_segregated_global_size_directory_data_ptr_load(&directory->data);
    if (!data)
        return NULL;
    return pas_compact_atomic_page_sharing_pool_ptr_load(&data->bias_sharing_pool);
}

typedef bool (*pas_segregated_global_size_directory_for_each_live_object_callback)(
    pas_segregated_global_size_directory* directory,
    pas_segregated_view view,
    uintptr_t begin,
    void* arg);

PAS_API bool pas_segregated_global_size_directory_for_each_live_object(
    pas_segregated_global_size_directory* directory,
    pas_segregated_global_size_directory_for_each_live_object_callback callback,
    void* arg);

PAS_API size_t pas_segregated_global_size_directory_local_allocator_size(
    pas_segregated_global_size_directory* directory);

PAS_API pas_allocator_index pas_segregated_global_size_directory_num_allocator_indices(
    pas_segregated_global_size_directory* directory);

PAS_API void pas_segregated_global_size_directory_dump_reference(
    pas_segregated_global_size_directory* directory, pas_stream* stream);

PAS_API void pas_segregated_global_size_directory_dump_for_spectrum(
    pas_stream* stream, void* directory);

PAS_END_EXTERN_C;

#endif /* PAS_SEGREGATED_GLOBAL_SIZE_DIRECTORY_H */

