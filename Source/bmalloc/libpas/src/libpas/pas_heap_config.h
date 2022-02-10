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

#ifndef PAS_HEAP_CONFIG_H
#define PAS_HEAP_CONFIG_H

#include "pas_aligned_allocation_result.h"
#include "pas_alignment.h"
#include "pas_allocation_result.h"
#include "pas_bitfit_page_config.h"
#include "pas_bitvector.h"
#include "pas_compact_atomic_bitfit_heap_ptr.h"
#include "pas_deallocation_mode.h"
#include "pas_deallocator.h"
#include "pas_fast_megapage_table.h"
#include "pas_heap_config_kind.h"
#include "pas_heap_ref.h"
#include "pas_heap_ref_kind.h"
#include "pas_mmap_capability.h"
#include "pas_segregated_heap_lookup_kind.h"
#include "pas_segregated_page_config.h"
#include "pas_size_lookup_mode.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

struct pas_heap;
struct pas_heap_config;
struct pas_heap_runtime_config;
struct pas_heap_type;
struct pas_large_heap;
struct pas_segregated_shared_page_directory;
struct pas_stream;
typedef struct pas_heap pas_heap;
typedef struct pas_heap_config pas_heap_config;
typedef struct pas_heap_runtime_config pas_heap_runtime_config;
typedef struct pas_heap_type pas_heap_type;
typedef struct pas_large_heap pas_large_heap;
typedef struct pas_segregated_shared_page_directory pas_segregated_shared_page_directory;
typedef struct pas_stream pas_stream;

typedef void (*pas_heap_config_activate_callback)(void);

typedef size_t (*pas_heap_config_get_type_size)(const pas_heap_type*);
typedef size_t (*pas_heap_config_get_type_alignment)(const pas_heap_type*);
typedef void (*pas_heap_config_dump_type)(const pas_heap_type*, pas_stream* stream);
typedef pas_fast_megapage_kind (*pas_heap_config_fast_megapage_kind_func)(uintptr_t begin);
typedef pas_page_base* (*pas_heap_config_page_header_func)(uintptr_t begin);
typedef pas_aligned_allocation_result (*pas_heap_config_aligned_allocator)(
    size_t size,
    pas_alignment alignment,
    pas_large_heap* large_heap,
    pas_heap_config* config);
typedef void* (*pas_heap_config_prepare_to_enumerate)(pas_enumerator* enumerator);
typedef bool (*pas_heap_config_for_each_shared_page_directory)(
    pas_segregated_heap* heap,
    bool (*callback)(pas_segregated_shared_page_directory* directory, void* arg),
    void* arg);
typedef bool (*pas_heap_config_for_each_shared_page_directory_remote)(
    pas_enumerator* enumerator,
    pas_segregated_heap* heap,
    bool (*callback)(pas_enumerator* enumerator,
                     pas_segregated_shared_page_directory* directory,
                     void* arg),
    void* arg);
typedef void (*pas_heap_config_dump_shared_page_directory_arg)(
    pas_stream* stream, pas_segregated_shared_page_directory* directory);

typedef pas_allocation_result
(*pas_heap_config_specialized_local_allocator_try_allocate_small_segregated_slow)(
    pas_local_allocator* allocator,
    pas_allocator_counts* counts,
    pas_allocation_result_filter result_filter);
typedef pas_allocation_result
(*pas_heap_config_specialized_local_allocator_try_allocate_medium_segregated_with_free_bits)(
    pas_local_allocator* allocator);
typedef pas_allocation_result (*pas_heap_config_specialized_local_allocator_try_allocate_inline_cases)(
    pas_local_allocator* allocator);
typedef pas_allocation_result (*pas_heap_config_specialized_local_allocator_try_allocate_slow)(
    pas_local_allocator* allocator,
    size_t size,
    size_t alignment,
    pas_allocator_counts* counts,
    pas_allocation_result_filter result_filter);
typedef pas_allocation_result (*pas_heap_config_specialized_try_allocate_common_impl_slow)(
    pas_heap_ref* heap_ref,
    pas_heap_ref_kind heap_ref_kind,
    size_t size,
    size_t alignment,
    pas_heap_runtime_config* runtime_config,
    pas_allocator_counts* allocator_counts,
    pas_size_lookup_mode size_lookup_mode);
typedef bool (*pas_heap_config_specialized_try_deallocate_not_small_exclusive_segregated)(
    pas_thread_local_cache* thread_local_cache,
    uintptr_t begin,
    pas_deallocation_mode deallocation_mode,
    pas_fast_megapage_kind megapage_kind);

struct pas_heap_config {
    /* This always self-points. It's useful for going from a config to a config_ptr. */
    pas_heap_config* config_ptr;

    pas_heap_config_kind kind;

    /* Called when the heap config is made the active one. This can only be called once. */
    pas_heap_config_activate_callback activate_callback;
    
    /* Tells you the size size for a heap type. */
    pas_heap_config_get_type_size get_type_size;
    
    /* Tells you the size size for a heap type. */
    pas_heap_config_get_type_alignment get_type_alignment;

    /* Tells the type to dump information about itself into a stream. */
    pas_heap_config_dump_type dump_type;
    
    /* Alignment requirement of large objects. */
    size_t large_alignment;
    
    /* Separate configurations for each of the page configs. You can use these very flexibly.
       The only requirement is that they are in order of increasing minimum object size shift.
       It's actually OK if it's just non-decreasing, but then the "first" one will be picked
       for computing the heap's minimum object size and the "last" one will be picked for
       computing the heap's maximum object size. It's OK to have disabled holes (like
       use_small_pages = false, use_medium_pages = true). If you only want one page_config,
       you can pick which one you use as your only one. However, some things will fast-path
       the small_segregated_config. Therefore, it's best to always have that one. */
    pas_segregated_page_config small_segregated_config;
    pas_segregated_page_config medium_segregated_config;

    pas_bitfit_page_config small_bitfit_config;
    pas_bitfit_page_config medium_bitfit_config;
    pas_bitfit_page_config marge_bitfit_config;
    
    /* The size upper bound for the "small" lookup table. */
    size_t small_lookup_size_upper_bound;

    /* This abstracts the megapage table lookup. It doesn't have to actually use megapages and it's
       OK for this to return pas_not_a_megapage_kind all the time. */
    pas_heap_config_fast_megapage_kind_func fast_megapage_kind_func;

    bool small_segregated_is_in_megapage;
    bool small_bitfit_is_in_megapage;

    /* This abstracts the page header table lookup. It doesn't have to actually use a page header
       table and it's OK for this to always return NULL. */
    pas_heap_config_page_header_func page_header_func;

    /* Things passed down to the large free heap. The arg that the large free heap will pass is
       the pas_large_heap instance. */
    pas_heap_config_aligned_allocator aligned_allocator;
    bool aligned_allocator_talks_to_sharing_pool;
    pas_deallocator deallocator;

    /* Tells if it's OK to call mmap on memory managed by this heap. */
    pas_mmap_capability mmap_capability;

    /* This points to things that are necessary for enumeration that are specific to the heap config. */
    void* root_data;

    /* The enumerator uses this to prepare itself for enumerating the active heap config. */
    pas_heap_config_prepare_to_enumerate prepare_to_enumerate;

    /* Gives you a way to iterate over the shared page directories that a heap uses. Note that multiple
       heaps may have the same shared page directories. */
    pas_heap_config_for_each_shared_page_directory for_each_shared_page_directory;
    pas_heap_config_for_each_shared_page_directory_remote for_each_shared_page_directory_remote;
    pas_heap_config_dump_shared_page_directory_arg dump_shared_page_directory_arg;

    pas_heap_config_specialized_local_allocator_try_allocate_small_segregated_slow specialized_local_allocator_try_allocate_small_segregated_slow;
    pas_heap_config_specialized_local_allocator_try_allocate_medium_segregated_with_free_bits specialized_local_allocator_try_allocate_medium_segregated_with_free_bits;
    pas_heap_config_specialized_local_allocator_try_allocate_inline_cases specialized_local_allocator_try_allocate_inline_cases;
    pas_heap_config_specialized_local_allocator_try_allocate_slow specialized_local_allocator_try_allocate_slow;
    pas_heap_config_specialized_try_allocate_common_impl_slow specialized_try_allocate_common_impl_slow;
    pas_heap_config_specialized_try_deallocate_not_small_exclusive_segregated specialized_try_deallocate_not_small_exclusive_segregated;
};

#define PAS_HEAP_CONFIG_SPECIALIZATIONS(lower_case_heap_config_name) \
    .specialized_local_allocator_try_allocate_small_segregated_slow = \
        lower_case_heap_config_name ## _specialized_local_allocator_try_allocate_small_segregated_slow, \
    .specialized_local_allocator_try_allocate_medium_segregated_with_free_bits = \
        lower_case_heap_config_name ## _specialized_local_allocator_try_allocate_medium_segregated_with_free_bits, \
    .specialized_local_allocator_try_allocate_inline_cases = \
        lower_case_heap_config_name ## _specialized_local_allocator_try_allocate_inline_cases, \
    .specialized_local_allocator_try_allocate_slow = \
        lower_case_heap_config_name ## _specialized_local_allocator_try_allocate_slow, \
    .specialized_try_allocate_common_impl_slow = \
        lower_case_heap_config_name ## _specialized_try_allocate_common_impl_slow, \
    .specialized_try_deallocate_not_small_exclusive_segregated = \
        lower_case_heap_config_name ## _specialized_try_deallocate_not_small_exclusive_segregated

#define PAS_HEAP_CONFIG_SPECIALIZATION_DECLARATIONS(lower_case_heap_config_name) \
    PAS_API pas_allocation_result \
    lower_case_heap_config_name ## _specialized_local_allocator_try_allocate_small_segregated_slow( \
        pas_local_allocator* allocator, pas_allocator_counts* count, \
        pas_allocation_result_filter result_filter); \
    PAS_API pas_allocation_result \
    lower_case_heap_config_name ## _specialized_local_allocator_try_allocate_medium_segregated_with_free_bits( \
        pas_local_allocator* allocator); \
    PAS_API pas_allocation_result \
    lower_case_heap_config_name ## _specialized_local_allocator_try_allocate_inline_cases( \
        pas_local_allocator* allocator); \
    PAS_API pas_allocation_result \
    lower_case_heap_config_name ## _specialized_local_allocator_try_allocate_slow( \
        pas_local_allocator* allocator, \
        size_t size, \
        size_t alignment, \
        pas_allocator_counts* counts, \
        pas_allocation_result_filter result_filter); \
    PAS_API pas_allocation_result \
    lower_case_heap_config_name ## _specialized_try_allocate_common_impl_slow( \
        pas_heap_ref* heap_ref, \
        pas_heap_ref_kind heap_ref_kind, \
        size_t size, \
        size_t alignment, \
        pas_heap_runtime_config* runtime_config, \
        pas_allocator_counts* allocator_counts, \
        pas_size_lookup_mode size_lookup_mode); \
    PAS_API bool lower_case_heap_config_name ## _specialized_try_deallocate_not_small_exclusive_segregated( \
        pas_thread_local_cache* thread_local_cache, \
        uintptr_t begin, \
        pas_deallocation_mode deallocation_mode, \
        pas_fast_megapage_kind megapage_kind)

static PAS_ALWAYS_INLINE pas_segregated_page_config*
pas_heap_config_segregated_page_config_ptr_for_variant(
    pas_heap_config* config,
    pas_segregated_page_config_variant variant)
{
    switch (variant) {
    case pas_small_segregated_page_config_variant:
        return &config->small_segregated_config;
    case pas_medium_segregated_page_config_variant:
        return &config->medium_segregated_config;
    }
    PAS_ASSERT(!"Should not reach here");
    return NULL;
}

static PAS_ALWAYS_INLINE pas_bitfit_page_config*
pas_heap_config_bitfit_page_config_ptr_for_variant(
    pas_heap_config* config,
    pas_bitfit_page_config_variant variant)
{
    switch (variant) {
    case pas_small_bitfit_page_config_variant:
        return &config->small_bitfit_config;
    case pas_medium_bitfit_page_config_variant:
        return &config->medium_bitfit_config;
    case pas_marge_bitfit_page_config_variant:
        return &config->marge_bitfit_config;
    }
    PAS_ASSERT(!"Should not reach here");
    return NULL;
}

static PAS_ALWAYS_INLINE pas_segregated_page_config
pas_heap_config_segregated_page_config_for_variant(
    pas_heap_config config,
    pas_segregated_page_config_variant variant)
{
    switch (variant) {
    case pas_small_segregated_page_config_variant:
        return config.small_segregated_config;
    case pas_medium_segregated_page_config_variant:
        return config.medium_segregated_config;
    }
    PAS_ASSERT(!"Should not reach here");
    return config.small_segregated_config;
}

static PAS_ALWAYS_INLINE pas_bitfit_page_config
pas_heap_config_bitfit_page_config_for_variant(
    pas_heap_config config,
    pas_bitfit_page_config_variant variant)
{
    switch (variant) {
    case pas_small_bitfit_page_config_variant:
        return config.small_bitfit_config;
    case pas_medium_bitfit_page_config_variant:
        return config.medium_bitfit_config;
    case pas_marge_bitfit_page_config_variant:
        return config.marge_bitfit_config;
    }
    PAS_ASSERT(!"Should not reach here");
    return config.small_bitfit_config;
}

static PAS_ALWAYS_INLINE size_t pas_heap_config_segregated_heap_min_align_shift(pas_heap_config config)
{
    size_t result;
    result = SIZE_MAX;
    if (config.small_bitfit_config.base.is_enabled)
        result = PAS_MIN(result, config.small_bitfit_config.base.min_align_shift);
    if (config.small_segregated_config.base.is_enabled)
        result = PAS_MIN(result, config.small_segregated_config.base.min_align_shift);
    PAS_ASSERT(result != SIZE_MAX);
    return result;
}

static PAS_ALWAYS_INLINE size_t pas_heap_config_segregated_heap_min_align(pas_heap_config config)
{
    return (size_t)1 << pas_heap_config_segregated_heap_min_align_shift(config);
}

/* Returns true if we were the first to active it. Must hold the heap lock to call this. This is
   permissive of recursive initialization: in that case, it will just pretend that the config is
   already initialized. */
bool pas_heap_config_activate(pas_heap_config* config);

/* NOTE: always pass pas_heap_config by value to inline functions, using constants if possible.
   Pass pas_heap_config by pointer to out-of-line functions, using the provided globals if
   possible.  See thingy_heap_config.h and pas_fast_heap_config.h for some valid
   configurations.*/

PAS_END_EXTERN_C;

#endif /* PAS_HEAP_CONFIG_H */
