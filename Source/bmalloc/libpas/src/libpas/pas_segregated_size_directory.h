/*
 * Copyright (c) 2018-2022 Apple Inc. All rights reserved.
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

#ifndef PAS_SEGREGATED_SIZE_DIRECTORY_H
#define PAS_SEGREGATED_SIZE_DIRECTORY_H

#include "pas_allocator_index.h"
#include "pas_bitfit_size_class.h"
#include "pas_compact_atomic_page_sharing_pool_ptr.h"
#include "pas_compact_atomic_segregated_size_directory_ptr.h"
#include "pas_compact_atomic_thread_local_cache_layout_node.h"
#include "pas_compact_tagged_page_granule_use_count_ptr.h"
#include "pas_compact_tagged_unsigned_ptr.h"
#include "pas_config.h"
#include "pas_heap_config.h"
#include "pas_page_granule_use_count.h"
#include "pas_segregated_directory.h"
#include "pas_segregated_size_directory_creation_mode.h"

PAS_BEGIN_EXTERN_C;

struct pas_extended_segregated_size_directory_data;
struct pas_segregated_size_directory;
struct pas_segregated_size_directory_data;
typedef struct pas_extended_segregated_size_directory_data pas_extended_segregated_size_directory_data;
typedef struct pas_segregated_size_directory_data pas_segregated_size_directory_data;
typedef struct pas_segregated_size_directory pas_segregated_size_directory;
typedef struct pas_segregated_size_directory_data pas_segregated_size_directory_data;

PAS_DEFINE_COMPACT_ATOMIC_PTR(pas_segregated_size_directory_data,
                              pas_segregated_size_directory_data_ptr);

#define PAS_SEGREGATED_SIZE_DIRECTORY_ALIGNMENT_SHIFT_BITS 5

struct pas_segregated_size_directory {
    pas_segregated_directory base;
    
    pas_segregated_heap* heap;

    unsigned object_size : 27;
    unsigned alignment_shift : PAS_SEGREGATED_SIZE_DIRECTORY_ALIGNMENT_SHIFT_BITS;

    /* This holds two fields that need to be CASable, so we don't want use a bitfield:
       
       - baseline_allocator_index
             => This has a funny encoding (let N = PAS_NUM_BASELINE_ALLOCATORS):
                
                index in [0, N)     => attached to index
                index in [N, 2N)    => attaching to index - N
                2N                  => detached
                
                In the detached state, the only way to change this is to CAS it.
                
                In the attached and attaching states, the corresponding index's lock protects this field.
    
       - min_index
             => This is the first index at which this directory is installed in its heap. It's used to enable
                rematerialization of the size class lookup tables. This field can be all-ones (UINT_MAX chopped
                to whatever size the field has) to indicate that the directory is not currently participating in
                size lookup. */
    unsigned encoded_stuff;

    pas_allocator_index view_cache_index; /* This could be in the size_directory_data but we put it here
                                             because it takes less space to put it here. */

    pas_allocator_index allocator_index; /* This could be in the size_directory_data but we put it here
                                            because it takes less space to put it here. */
    
    pas_segregated_size_directory_data_ptr data;

    /* The owning segregated heap holds these in a singly linked list. */
    pas_compact_atomic_segregated_size_directory_ptr next_for_heap;
};

struct pas_segregated_size_directory_data {
    unsigned offset_from_page_boundary_to_first_object; /* Cached to make refill fast. */
    unsigned offset_from_page_boundary_to_end_of_last_object; /* Cached to make refill fast. */
    
    uint8_t full_num_non_empty_words;

    pas_compact_tagged_unsigned_ptr full_alloc_bits; /* Precomputed alloc bits in the case that a page is empty. */
};

struct pas_extended_segregated_size_directory_data {
    pas_segregated_size_directory_data base;
    pas_compact_tagged_page_granule_use_count_ptr full_use_counts;
};

#define PAS_SEGREGATED_SIZE_DIRECTORY_BASELINE_ALLOCATOR_INDEX_BITS 7u
#define PAS_SEGREGATED_SIZE_DIRECTORY_MIN_INDEX_BITS                25u

#if PAS_COMPILER(CLANG)
_Static_assert(PAS_SEGREGATED_SIZE_DIRECTORY_BASELINE_ALLOCATOR_INDEX_BITS
               + PAS_SEGREGATED_SIZE_DIRECTORY_MIN_INDEX_BITS
               == 32u,
               "encoded_stuff field should use exactly 32 bits");
#endif

#define PAS_SEGREAGTED_SIZE_DIRECTORY_BASELINE_ALLOCATOR_INDEX_SHIFT 0u
#define PAS_SEGREGATED_SIZE_DIRECTORY_BASELINE_ALLOCATOR_INDEX_MASK \
    ((1u << PAS_SEGREGATED_SIZE_DIRECTORY_BASELINE_ALLOCATOR_INDEX_BITS) - 1u)
#define PAS_SEGREGATED_SIZE_DIRECTORY_MIN_INDEX_SHIFT \
    PAS_SEGREGATED_SIZE_DIRECTORY_BASELINE_ALLOCATOR_INDEX_BITS
#define PAS_SEGREGATED_SIZE_DIRECTORY_MIN_INDEX_MASK \
    ((1u << PAS_SEGREGATED_SIZE_DIRECTORY_MIN_INDEX_BITS) - 1u)

static inline unsigned pas_segregated_size_directory_decode_baseline_allocator_index(unsigned encoded_stuff)
{
    return (encoded_stuff >> PAS_SEGREAGTED_SIZE_DIRECTORY_BASELINE_ALLOCATOR_INDEX_SHIFT)
        & PAS_SEGREGATED_SIZE_DIRECTORY_BASELINE_ALLOCATOR_INDEX_MASK;
}

static inline unsigned pas_segregated_size_directory_decode_min_index(unsigned encoded_stuff)
{
    unsigned result;

    result = (encoded_stuff >> PAS_SEGREGATED_SIZE_DIRECTORY_MIN_INDEX_SHIFT)
        & PAS_SEGREGATED_SIZE_DIRECTORY_MIN_INDEX_MASK;

    if (result == PAS_SEGREGATED_SIZE_DIRECTORY_MIN_INDEX_MASK)
        return UINT_MAX;

    return result;
}

static inline unsigned pas_segregated_size_directory_encode_stuff(unsigned baseline_allocator_index,
                                                                  unsigned min_index)
{
    unsigned result;

    result =
        (baseline_allocator_index << PAS_SEGREAGTED_SIZE_DIRECTORY_BASELINE_ALLOCATOR_INDEX_SHIFT) |
        ((min_index & PAS_SEGREGATED_SIZE_DIRECTORY_MIN_INDEX_MASK)
         << PAS_SEGREGATED_SIZE_DIRECTORY_MIN_INDEX_SHIFT);

    PAS_ASSERT(
        pas_segregated_size_directory_decode_baseline_allocator_index(result) == baseline_allocator_index);
    PAS_ASSERT(pas_segregated_size_directory_decode_min_index(result) == min_index);

    return result;
}

static inline pas_segregated_view
pas_segregated_size_directory_as_view(pas_segregated_size_directory* directory)
{
    return pas_segregated_view_create(directory, pas_segregated_size_directory_view_kind);
}

PAS_API pas_segregated_size_directory* pas_segregated_size_directory_create(
    pas_segregated_heap* heap,
    unsigned object_size,
    unsigned alignment,
    const pas_heap_config* heap_config,
    const pas_segregated_page_config* page_config, /* Pass NULL to create a bitfit size directory. */
    pas_segregated_size_directory_creation_mode creation_mode);

PAS_API void pas_segregated_size_directory_finish_creation(pas_segregated_size_directory* directory);

static inline size_t
pas_segregated_size_directory_alignment(pas_segregated_size_directory* directory)
{
    return (size_t)1 << (size_t)directory->alignment_shift;
}

static inline unsigned pas_segregated_size_directory_baseline_allocator_index(
    pas_segregated_size_directory* directory)
{
    return pas_segregated_size_directory_decode_baseline_allocator_index(directory->encoded_stuff);
}

static inline unsigned pas_segregated_size_directory_min_index(pas_segregated_size_directory* directory)
{
    return pas_segregated_size_directory_decode_min_index(directory->encoded_stuff);
}

static inline void pas_segregated_size_directory_set_baseline_allocator_index(
    pas_segregated_size_directory* directory, unsigned new_baseline_allocator_index)
{
    for (;;) {
        unsigned old_stuff;
        unsigned new_stuff;

        old_stuff = directory->encoded_stuff;
        new_stuff = pas_segregated_size_directory_encode_stuff(
            new_baseline_allocator_index,
            pas_segregated_size_directory_decode_min_index(old_stuff));

        if (pas_compare_and_swap_uint32_weak(&directory->encoded_stuff, old_stuff, new_stuff))
            return;
    }
}

static inline bool pas_segregated_size_directory_compare_and_swap_baseline_allocator_index_weak(
    pas_segregated_size_directory* directory,
    unsigned expected_baseline_allocator_index,
    unsigned new_baseline_allocator_index)
{
    /* We have an early return here, so use this to make sure that the compiler doesn't get any funny ideas. */
    pas_compiler_fence();
    
    for (;;) {
        unsigned old_stuff;
        unsigned new_stuff;

        old_stuff = directory->encoded_stuff;
        if (pas_segregated_size_directory_decode_baseline_allocator_index(old_stuff)
            != expected_baseline_allocator_index) {
            pas_compiler_fence(); /* Put the compiler in its place. */
            return false;
        }
        
        new_stuff = pas_segregated_size_directory_encode_stuff(
            new_baseline_allocator_index,
            pas_segregated_size_directory_decode_min_index(old_stuff));

        if (pas_compare_and_swap_uint32_weak(&directory->encoded_stuff, old_stuff, new_stuff))
            return true;
    }
}

static inline void pas_segregated_size_directory_set_min_index(pas_segregated_size_directory* directory,
                                                                   unsigned new_min_index)
{
    for (;;) {
        unsigned old_stuff;
        unsigned new_stuff;

        old_stuff = directory->encoded_stuff;
        new_stuff = pas_segregated_size_directory_encode_stuff(
            pas_segregated_size_directory_decode_baseline_allocator_index(old_stuff),
            new_min_index);

        if (pas_compare_and_swap_uint32_weak(&directory->encoded_stuff, old_stuff, new_stuff))
            return;
    }
}

static inline bool pas_segregated_size_directory_is_bitfit(pas_segregated_size_directory* directory)
{
    return directory->base.page_config_kind == pas_segregated_page_config_kind_null;
}

static inline pas_bitfit_size_class* pas_segregated_size_directory_get_bitfit_size_class(
    pas_segregated_size_directory* directory)
{
    uintptr_t result;
    PAS_TESTING_ASSERT(pas_segregated_size_directory_is_bitfit(directory));
    result = (uintptr_t)(directory + 1);
    PAS_TESTING_ASSERT(pas_is_aligned(result, PAS_ALIGNOF(pas_bitfit_size_class)));
    return (pas_bitfit_size_class*)result;
}

static inline bool pas_segregated_size_directory_did_try_to_create_view_cache(
    pas_segregated_size_directory* directory)
{
    return pas_segregated_directory_get_misc_bit(&directory->base);
}

static inline bool pas_segregated_size_directory_set_did_try_to_create_view_cache(
    pas_segregated_size_directory* directory,
    bool value)
{
    return pas_segregated_directory_set_misc_bit(&directory->base, value);
}

PAS_API pas_segregated_size_directory_data* pas_segregated_size_directory_ensure_data(
    pas_segregated_size_directory* directory,
    pas_lock_hold_mode heap_lock_hold_mode);

PAS_API pas_extended_segregated_size_directory_data*
pas_segregated_size_directory_get_extended_data(
    pas_segregated_size_directory* directory);

/* Call with heap lock held. */
PAS_API void pas_segregated_size_directory_create_tlc_allocator(
    pas_segregated_size_directory* directory);

static inline bool pas_segregated_size_directory_has_tlc_allocator(
    pas_segregated_size_directory* directory)
{
    return directory->allocator_index;
}

static inline pas_allocator_index pas_segregated_size_directory_get_tlc_allocator_index(
    pas_segregated_size_directory* directory)
{
    return directory->allocator_index;
}

/* Call with heap lock held. */
PAS_API void pas_segregated_size_directory_create_tlc_view_cache(
    pas_segregated_size_directory* directory);

/* Call with heap lock held. */
PAS_API void pas_segregated_size_directory_enable_exclusive_views(
    pas_segregated_size_directory* directory);

static inline bool pas_segregated_size_directory_are_exclusive_views_enabled(
    pas_segregated_size_directory* directory)
{
    pas_segregated_size_directory_data* data;
    data = pas_segregated_size_directory_data_ptr_load(&directory->data);
    return data && data->full_num_non_empty_words;
}

PAS_API pas_segregated_view pas_segregated_size_directory_take_first_eligible(
    pas_segregated_size_directory* directory);

PAS_API pas_page_sharing_pool_take_result
pas_segregated_size_directory_take_last_empty(
    pas_segregated_size_directory* directory,
    pas_deferred_decommit_log* log,
    pas_lock_hold_mode heap_lock_hold_mode);

PAS_API pas_segregated_size_directory* pas_segregated_size_directory_for_object(
    uintptr_t begin,
    const pas_heap_config* config);

/* This assumes that we already have a data. That's a valid assumption if we have exclusives. */
PAS_API pas_heap_summary pas_segregated_size_directory_compute_summary_for_unowned_exclusive(
    pas_segregated_size_directory* directory);

typedef bool (*pas_segregated_size_directory_for_each_live_object_callback)(
    pas_segregated_size_directory* directory,
    pas_segregated_view view,
    uintptr_t begin,
    void* arg);

PAS_API bool pas_segregated_size_directory_for_each_live_object(
    pas_segregated_size_directory* directory,
    pas_segregated_size_directory_for_each_live_object_callback callback,
    void* arg);

PAS_API uint8_t pas_segregated_size_directory_view_cache_capacity(
    pas_segregated_size_directory* directory);

PAS_API size_t pas_segregated_size_directory_local_allocator_size(
    pas_segregated_size_directory* directory);

PAS_API pas_allocator_index pas_segregated_size_directory_num_allocator_indices(
    pas_segregated_size_directory* directory);

PAS_API void pas_segregated_size_directory_dump_reference(
    pas_segregated_size_directory* directory, pas_stream* stream);

PAS_API void pas_segregated_size_directory_dump_for_spectrum(
    pas_stream* stream, void* directory);

PAS_END_EXTERN_C;

#endif /* PAS_SEGREGATED_SIZE_DIRECTORY_H */

