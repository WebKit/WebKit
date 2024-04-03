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

#ifndef PAS_SEGREGATED_HEAP_CONFIG_H
#define PAS_SEGREGATED_HEAP_CONFIG_H

#include "pas_allocation_mode.h"
#include "pas_allocation_result.h"
#include "pas_bitvector.h"
#include "pas_config.h"
#include "pas_heap_runtime_config.h"
#include "pas_local_allocator_refill_mode.h"
#include "pas_lock.h"
#include "pas_page_base_config.h"
#include "pas_page_granule_use_count.h"
#include "pas_page_sharing_mode.h"
#include "pas_segregated_deallocation_logging_mode.h"
#include "pas_segregated_page_config_kind.h"
#include "pas_segregated_page_config_variant.h"
#include "pas_segregated_page_role.h"
#include "pas_segregated_view.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

#ifndef pas_heap
#define pas_heap __pas_heap
#endif

struct pas_allocator_counts;
struct pas_heap;
struct pas_heap_config;
struct pas_local_allocator;
struct pas_heap_runtime_config;
struct pas_page_sharing_pool;
struct pas_physical_memory_transaction;
struct pas_segregated_size_directory;
struct pas_segregated_heap;
struct pas_segregated_page;
struct pas_segregated_page_config;
struct pas_segregated_partial_view;
struct pas_segregated_shared_page_directory;
struct pas_thread_local_cache;
typedef struct pas_allocator_counts pas_allocator_counts;
typedef struct pas_heap pas_heap;
typedef struct pas_heap_config pas_heap_config;
typedef struct pas_local_allocator pas_local_allocator;
typedef struct pas_heap_runtime_config pas_heap_runtime_config;
typedef struct pas_page_sharing_pool pas_page_sharing_pool;
typedef struct pas_physical_memory_transaction pas_physical_memory_transaction;
typedef struct pas_segregated_size_directory pas_segregated_size_directory;
typedef struct pas_segregated_heap pas_segregated_heap;
typedef struct pas_segregated_page pas_segregated_page;
typedef struct pas_segregated_page_config pas_segregated_page_config;
typedef struct pas_segregated_partial_view pas_segregated_partial_view;
typedef struct pas_segregated_shared_page_directory pas_segregated_shared_page_directory;
typedef struct pas_thread_local_cache pas_thread_local_cache;

typedef void* (*pas_segregated_page_config_page_allocator)(
    pas_segregated_heap*, pas_physical_memory_transaction* transaction, pas_segregated_page_role role);
typedef pas_segregated_shared_page_directory*
(*pas_segregated_page_config_shared_page_directory_selector)(
    pas_segregated_heap* heap, pas_segregated_size_directory* directory);
typedef void (*pas_segregated_page_config_dealloc_func)(pas_thread_local_cache* thread_local_cache,
                                                        uintptr_t begin);

typedef pas_allocation_result
(*pas_segregated_page_config_specialized_local_allocator_try_allocate_in_primordial_partial_view)(
    pas_local_allocator* allocator, pas_allocation_mode allocation_mode);
typedef bool
(*pas_segregated_page_config_specialized_local_allocator_start_allocating_in_primordial_partial_view)(
    pas_local_allocator* allocator, pas_segregated_partial_view* partial,
    pas_segregated_size_directory* size_directory);
typedef bool (*pas_segregated_page_config_specialized_local_allocator_refill)(
    pas_local_allocator* allocator,
    pas_allocator_counts* counts);
typedef void (*pas_segregated_page_config_specialized_local_allocator_return_memory_to_page)(
    pas_local_allocator* allocator,
    pas_segregated_view view,
    pas_segregated_page* page,
    pas_segregated_size_directory* directory,
    pas_lock_hold_mode heap_lock_hold_mode);

struct pas_segregated_page_config {
    /* Lots of interesting properties come from this. */
    pas_page_base_config base;
    
    /* Tells whether we are using this as the small or medium page config in the heap config. */
    pas_segregated_page_config_variant variant;
    
    /* Each unique small heap config gets a unique kind so that we can can use the
       pas_segregated_page_config_kind as an efficient representation of pas_heap_config. But it's
       bad style to switch on this to detect something about a heap config that isn't
       represented in the config's fields. */
    pas_segregated_page_config_kind kind;
    
    /* The handicap when evaluating wasteage. For eample, handicap == 1 means no handicap and
       handicap > 1 means having a handicap.  It's also OK to use it inverted - handicap < 0 means
       being preferred.
    
       We use this to bias page selection to picking small pages. Small pages exhibit lower
       internal fragmentation because they are smaller. That means that free bytes have a higher
       probability of being turned into decommitted bytes. We just pretend that this is a tax
       on the wasteage factor of a page.
    
       Note that you can also use this to force always using small pages whenever possible - just
       set the small handicap to 0 and the medium handicap to 1. On the other hand if you set the
       small handicap to 1 and the medium handicap to 0 then we will _always_ select the medium
       heap (which you almost certainly don't want).
    
       Note also that infinite handicaps don't work because wasteage == infinity is reserved to
       mean that a page config is unusable for some size. */
    double wasteage_handicap;

    /* Sharing granule size expressed as a shift. The sweet spot is PAS_BITVECTOR_WORD_SHIFT if
       you want perf. */
    uint8_t sharing_shift;
    
    /* Number of bits needed for alloc bits. This ends up impacting the size of the page header,
       so this value needs to be set in a way that is compatible with page_size, payload_size,
       and where you place the header. */
    size_t num_alloc_bits;

    /* What's the first byte at which the object payload could start relative to the boundary? */
    uintptr_t shared_payload_offset;
    uintptr_t exclusive_payload_offset;

    /* How many bytes are provisioned for objects past that offset? */
    size_t shared_payload_size;
    size_t exclusive_payload_size;

    pas_segregated_deallocation_logging_mode shared_logging_mode;
    pas_segregated_deallocation_logging_mode exclusive_logging_mode;

    /* Tells whether we should use a reversed current word. Only valid for the small segregated
       variant. */
    bool use_reversed_current_word;

    /* Set to true if we check deallocations. */
    bool check_deallocation;

    /* Tells if we enable the empty word eligibility optimization for pages of this kind. That
       optimization will make it so that a page does not appear as eligible until at least one word
       of bits goes clear. */
    bool enable_empty_word_eligibility_optimization_for_shared;
    bool enable_empty_word_eligibility_optimization_for_exclusive;

    /* Tells if we use the view cache for this size class. */
    bool enable_view_cache;

    /* This is the allocator used to create pages. */
    pas_segregated_page_config_page_allocator page_allocator;

    pas_segregated_page_config_shared_page_directory_selector shared_page_directory_selector;

    /* These two get filled in with the PAS_SEGREGATED_PAGE_CONFIG_SPECIALIZATIONS() macro. */
    pas_segregated_page_config_specialized_local_allocator_try_allocate_in_primordial_partial_view specialized_local_allocator_try_allocate_in_primordial_partial_view;
    pas_segregated_page_config_specialized_local_allocator_start_allocating_in_primordial_partial_view specialized_local_allocator_start_allocating_in_primordial_partial_view;
    pas_segregated_page_config_specialized_local_allocator_refill specialized_local_allocator_refill;
    pas_segregated_page_config_specialized_local_allocator_return_memory_to_page specialized_local_allocator_return_memory_to_page;
};

PAS_API extern bool pas_segregated_page_config_do_validate;
PAS_API extern bool pas_small_segregated_page_config_variant_is_enabled_override;
PAS_API extern bool pas_medium_segregated_page_config_variant_is_enabled_override;

#define PAS_SEGREGATED_PAGE_CONFIG_TLC_SPECIALIZATIONS(lower_case_page_config_name) \
    .specialized_local_allocator_try_allocate_in_primordial_partial_view = \
        lower_case_page_config_name ## _specialized_local_allocator_try_allocate_in_primordial_partial_view, \
    .specialized_local_allocator_start_allocating_in_primordial_partial_view = \
        lower_case_page_config_name ## _specialized_local_allocator_start_allocating_in_primordial_partial_view, \
    .specialized_local_allocator_refill = \
        lower_case_page_config_name ## _specialized_local_allocator_refill, \
    .specialized_local_allocator_return_memory_to_page = \
        lower_case_page_config_name ## _specialized_local_allocator_return_memory_to_page

#define PAS_SEGREGATED_PAGE_CONFIG_TLC_SPECIALIZATION_DECLARATIONS(lower_case_page_config_name) \
    PAS_API pas_allocation_result \
    lower_case_page_config_name ## _specialized_local_allocator_try_allocate_in_primordial_partial_view( \
        pas_local_allocator* allocator, pas_allocation_mode allocation_mode); \
    PAS_API bool lower_case_page_config_name ## _specialized_local_allocator_start_allocating_in_primordial_partial_view( \
        pas_local_allocator* allocator, \
        pas_segregated_partial_view* partial, \
        pas_segregated_size_directory* size_directory); \
    PAS_API bool lower_case_page_config_name ## _specialized_local_allocator_refill( \
        pas_local_allocator* allocator, \
        pas_allocator_counts* counts); \
    PAS_API void \
    lower_case_page_config_name ## _specialized_local_allocator_return_memory_to_page( \
        pas_local_allocator* allocator, \
        pas_segregated_view view, \
        pas_segregated_page* page, \
        pas_segregated_size_directory* directory, \
        pas_lock_hold_mode heap_lock_hold_mode)

#define PAS_SEGREGATED_PAGE_CONFIG_SPECIALIZATIONS(lower_case_page_config_name) \
    PAS_SEGREGATED_PAGE_CONFIG_TLC_SPECIALIZATIONS(lower_case_page_config_name)

#define PAS_SEGREGATED_PAGE_CONFIG_SPECIALIZATION_DECLARATIONS(lower_case_page_config_name) \
    PAS_SEGREGATED_PAGE_CONFIG_TLC_SPECIALIZATION_DECLARATIONS(lower_case_page_config_name)

#define PAS_SEGREGATED_PAGE_CONFIG_GOOD_MAX_OBJECT_SIZE(object_payload_size, min_num_objects) \
    ((object_payload_size) / (min_num_objects))

static inline bool pas_segregated_page_config_is_enabled(pas_segregated_page_config config,
                                                         pas_heap_runtime_config* runtime_config)
{
    if (!config.base.is_enabled)
        return false;
    /* Doing this check here is not super necessary, but it's sort of nice for cases where we have a heap
       that sometimes uses bitfit exclusively or sometimes uses segregated exclusively and that's selected
       by selecting or mutating runtime_configs. This is_enabled function is only called as part of the math
       that sets up size classes, at least for now, so the implications of not doing this check are rather
       tiny. */
    if (!runtime_config->max_segregated_object_size)
        return false;
    switch (config.variant) {
    case pas_small_segregated_page_config_variant:
        return pas_small_segregated_page_config_variant_is_enabled_override;
    case pas_medium_segregated_page_config_variant:
        return pas_medium_segregated_page_config_variant_is_enabled_override;
    }
    PAS_ASSERT(!"Should not be reached");
    return false;
}

static PAS_ALWAYS_INLINE size_t
pas_segregated_page_config_min_align(pas_segregated_page_config config)
{
    return pas_page_base_config_min_align(config.base);
}

static PAS_ALWAYS_INLINE uintptr_t
pas_segregated_page_config_payload_offset_for_role(pas_segregated_page_config config,
                                                   pas_segregated_page_role role)
{
    switch (role) {
    case pas_segregated_page_shared_role:
        return config.shared_payload_offset;
    case pas_segregated_page_exclusive_role:
        return config.exclusive_payload_offset;
    }
    PAS_ASSERT(!"Should not be reached");
    return 0;
}

static PAS_ALWAYS_INLINE size_t
pas_segregated_page_config_payload_size_for_role(pas_segregated_page_config config,
                                                 pas_segregated_page_role role)
{
    switch (role) {
    case pas_segregated_page_shared_role:
        return config.shared_payload_size;
    case pas_segregated_page_exclusive_role:
        return config.exclusive_payload_size;
    }
    PAS_ASSERT(!"Should not be reached");
    return 0;
}

static PAS_ALWAYS_INLINE uintptr_t
pas_segregated_page_config_payload_end_offset_for_role(pas_segregated_page_config config,
                                                       pas_segregated_page_role role)
{
    return pas_segregated_page_config_payload_offset_for_role(config, role)
        + pas_segregated_page_config_payload_size_for_role(config, role);
}

#define PAS_SEGREGATED_PAGE_CONFIG_NUM_ALLOC_WORDS(num_alloc_bits) \
    PAS_BITVECTOR_NUM_WORDS(num_alloc_bits)

static inline size_t pas_segregated_page_config_num_alloc_words(pas_segregated_page_config config)
{
    return PAS_SEGREGATED_PAGE_CONFIG_NUM_ALLOC_WORDS(config.num_alloc_bits);
}

#define PAS_SEGREGATED_PAGE_CONFIG_NUM_ALLOC_BYTES(num_alloc_bits) \
    PAS_BITVECTOR_NUM_BYTES(num_alloc_bits)

static inline size_t
pas_segregated_page_config_num_alloc_bytes(pas_segregated_page_config config)
{
    return PAS_SEGREGATED_PAGE_CONFIG_NUM_ALLOC_BYTES(config.num_alloc_bits);
}

static PAS_ALWAYS_INLINE bool
pas_segregated_page_config_enable_empty_word_eligibility_optimization_for_role(
    pas_segregated_page_config config,
    pas_segregated_page_role role)
{
    switch (role) {
    case pas_segregated_page_shared_role:
        return config.enable_empty_word_eligibility_optimization_for_shared;
    case pas_segregated_page_exclusive_role:
        return config.enable_empty_word_eligibility_optimization_for_exclusive;
    }
    PAS_ASSERT(!"Should not be reached");
    return pas_segregated_deallocation_no_logging_mode;
}

static PAS_ALWAYS_INLINE pas_segregated_deallocation_logging_mode
pas_segregated_page_config_logging_mode_for_role(pas_segregated_page_config config,
                                                 pas_segregated_page_role role)
{
    switch (role) {
    case pas_segregated_page_shared_role:
        return config.shared_logging_mode;
    case pas_segregated_page_exclusive_role:
        return config.exclusive_logging_mode;
    }
    PAS_ASSERT(!"Should not be reached");
    return pas_segregated_deallocation_no_logging_mode;
}

PAS_API void pas_segregated_page_config_validate(const pas_segregated_page_config*);

static inline pas_segregated_page_config_kind pas_segregated_page_config_get_kind(
    const pas_segregated_page_config* page_config)
{
    return page_config ? page_config->kind : pas_segregated_page_config_kind_null;
}

static PAS_ALWAYS_INLINE bool pas_segregated_page_config_kind_is_utility(
    pas_segregated_page_config_kind config_kind)
{
    return config_kind == pas_segregated_page_config_kind_pas_utility_small;
}

static PAS_ALWAYS_INLINE bool pas_segregated_page_config_is_utility(
    pas_segregated_page_config config)
{
    return pas_segregated_page_config_kind_is_utility(config.kind);
}

static PAS_ALWAYS_INLINE pas_lock_hold_mode
pas_segregated_page_config_kind_heap_lock_hold_mode(pas_segregated_page_config_kind config_kind)
{
    return pas_segregated_page_config_kind_is_utility(config_kind)
        ? pas_lock_is_held
        : pas_lock_is_not_held;
}

static PAS_ALWAYS_INLINE pas_lock_hold_mode
pas_segregated_page_config_heap_lock_hold_mode(pas_segregated_page_config config)
{
    return pas_segregated_page_config_kind_heap_lock_hold_mode(config.kind);
}

/* NOTE: always pass pas_segregated_page_config by value to inline functions, using constants if possible.
   Pass pas_segregated_page_config by pointer to out-of-line functions, using the provided globals if
   possible.  See thingy_heap_config.h and iso_heap_config.h for some valid configurations.*/

PAS_END_EXTERN_C;

#endif /* PAS_SEGREGATED_PAGE_CONFIG_H */
