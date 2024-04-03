/*
 * Copyright (c) 2020-2022 Apple Inc. All rights reserved.
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

#ifndef PAS_BITFIT_PAGE_CONFIG_H
#define PAS_BITFIT_PAGE_CONFIG_H

#include "pas_allocation_mode.h"
#include "pas_bitfit_max_free.h"
#include "pas_bitfit_page_config_kind.h"
#include "pas_bitfit_page_config_variant.h"
#include "pas_bitvector.h"
#include "pas_fast_path_allocation_result.h"
#include "pas_heap_runtime_config.h"
#include "pas_page_base_config.h"
#include "pas_page_malloc.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

struct pas_bitfit_allocator;
struct pas_bitfit_page;
struct pas_bitfit_page_config;
struct pas_heap_runtime_config;
struct pas_local_allocator;
typedef struct pas_bitfit_allocator pas_bitfit_allocator;
typedef struct pas_bitfit_page pas_bitfit_page;
typedef struct pas_bitfit_page_config pas_bitfit_page_config;
typedef struct pas_heap_runtime_config pas_heap_runtime_config;
typedef struct pas_local_allocator pas_local_allocator;

typedef void* (*pas_bitfit_page_config_page_allocator)(
    pas_segregated_heap*, pas_physical_memory_transaction* transaction);
typedef pas_fast_path_allocation_result (*pas_bitfit_page_config_specialized_allocator_try_allocate)(
    pas_bitfit_allocator* allocator,
    pas_local_allocator* local_allocator,
    size_t size,
    size_t alignment,
    pas_allocation_mode allocation_mode);
typedef void (*pas_bitfit_page_config_specialized_page_deallocate_with_page)(
    pas_bitfit_page* page, uintptr_t begin);
typedef size_t (*pas_bitfit_page_config_specialized_page_get_allocation_size_with_page)(
    pas_bitfit_page* page, uintptr_t begin);
typedef void (*pas_bitfit_page_config_specialized_page_shrink_with_page)(
    pas_bitfit_page* page, uintptr_t begin, size_t new_size);

#define PAS_MAX_BITFIT_OBJECT_SIZE(payload_size, min_align_shift) \
    PAS_MIN_CONST(PAS_MAX_OBJECT_SIZE((payload_size)), \
                  PAS_BITFIT_MAX_FREE_MAX_VALID * (1u << (min_align_shift)))

struct pas_bitfit_page_config {
    pas_page_base_config base;

    pas_bitfit_page_config_variant variant;
    pas_bitfit_page_config_kind kind;

    /* What's the first byte at which the object payload could start relative to the boundary? */
    uintptr_t page_object_payload_offset;

    /* How many bytes are provisioned for objects past that offset? */
    size_t page_object_payload_size;

    /* This is the allocator used to create pages. */
    pas_bitfit_page_config_page_allocator page_allocator;

    pas_bitfit_page_config_specialized_allocator_try_allocate specialized_allocator_try_allocate;
    pas_bitfit_page_config_specialized_page_deallocate_with_page specialized_page_deallocate_with_page;
    pas_bitfit_page_config_specialized_page_get_allocation_size_with_page specialized_page_get_allocation_size_with_page;
    pas_bitfit_page_config_specialized_page_shrink_with_page specialized_page_shrink_with_page;
};

PAS_API extern bool pas_small_bitfit_page_config_variant_is_enabled_override;
PAS_API extern bool pas_medium_bitfit_page_config_variant_is_enabled_override;
PAS_API extern bool pas_marge_bitfit_page_config_variant_is_enabled_override;

/* We may want this to be non-zero if we use certain kinds of bitvector search tricks. */
#define PAS_BITFIT_PAGE_CONFIG_ALLOC_BIT_BYTE_PADDING 0

#define PAS_BITFIT_PAGE_CONFIG_NUM_ALLOC_BIT_BYTES_FOR_NUM_BITS(num_alloc_bits) \
    (PAS_BITVECTOR_NUM_BYTES64(num_alloc_bits) * 2 + PAS_BITFIT_PAGE_CONFIG_ALLOC_BIT_BYTE_PADDING)

#define PAS_BITFIT_PAGE_CONFIG_BYTE_OFFSET_FOR_OBJECT_BITS_FOR_NUM_BITS(num_alloc_bits) \
    (PAS_BITVECTOR_NUM_BYTES64(num_alloc_bits) + PAS_BITFIT_PAGE_CONFIG_ALLOC_BIT_BYTE_PADDING)

#define PAS_BITFIT_PAGE_CONFIG_NUM_ALLOC_BITS(page_size, min_align_shift) \
    ((page_size) >> (min_align_shift))

#define PAS_BITFIT_PAGE_CONFIG_NUM_ALLOC_BIT_BYTES(page_size, min_align_shift) \
    PAS_BITFIT_PAGE_CONFIG_NUM_ALLOC_BIT_BYTES_FOR_NUM_BITS( \
        PAS_BITFIT_PAGE_CONFIG_NUM_ALLOC_BITS((page_size), (min_align_shift)))

#define PAS_BITFIT_PAGE_CONFIG_BYTE_OFFSET_FOR_OBJECT_BITS(page_size, min_align_shift) \
    PAS_BITFIT_PAGE_CONFIG_BYTE_OFFSET_FOR_OBJECT_BITS_FOR_NUM_BITS( \
        PAS_BITFIT_PAGE_CONFIG_NUM_ALLOC_BITS((page_size), (min_align_shift)))

#define PAS_BITFIT_PAGE_CONFIG_SPECIALIZATIONS(lower_case_page_config_name) \
    .specialized_allocator_try_allocate = \
        lower_case_page_config_name ## _specialized_allocator_try_allocate, \
    .specialized_page_deallocate_with_page = \
        lower_case_page_config_name ## _specialized_page_deallocate_with_page, \
    .specialized_page_get_allocation_size_with_page = \
        lower_case_page_config_name ## _specialized_page_get_allocation_size_with_page, \
    .specialized_page_shrink_with_page = \
        lower_case_page_config_name ## _specialized_page_shrink_with_page

#define PAS_BITFIT_PAGE_CONFIG_SPECIALIZATION_DECLARATIONS(lower_case_page_config_name) \
    PAS_API pas_fast_path_allocation_result \
    lower_case_page_config_name ## _specialized_allocator_try_allocate( \
        pas_bitfit_allocator* allocator, \
        pas_local_allocator* local_allocator, \
        size_t size, \
        size_t alignment, \
        pas_allocation_mode allocation_mode); \
    PAS_API void lower_case_page_config_name ## _specialized_page_deallocate_with_page( \
        pas_bitfit_page* page, uintptr_t begin); \
    PAS_API size_t lower_case_page_config_name ## _specialized_page_get_allocation_size_with_page( \
        pas_bitfit_page* page, uintptr_t begin); \
    PAS_API void lower_case_page_config_name ## _specialized_page_shrink_with_page( \
        pas_bitfit_page* page, uintptr_t begin, size_t new_size)

static inline bool pas_bitfit_page_config_is_enabled(pas_bitfit_page_config config,
                                                     pas_heap_runtime_config* runtime_config)
{
    if (!config.base.is_enabled)
        return false;
    /* Doing this check here is not super necessary, but it's sort of nice for cases where we have a heap
       that sometimes uses bitfit exclusively or sometimes uses segregated exclusively and that's selected
       by selecting or mutating runtime_configs. This is_enabled function is only called as part of the math
       that sets up size classes, at least for now, so the implications of not doing this check are rather
       tiny. */
    if (!runtime_config->max_bitfit_object_size)
        return false;
    switch (config.variant) {
    case pas_small_bitfit_page_config_variant:
        return pas_small_bitfit_page_config_variant_is_enabled_override;
    case pas_medium_bitfit_page_config_variant:
        return pas_medium_bitfit_page_config_variant_is_enabled_override;
    case pas_marge_bitfit_page_config_variant:
        return pas_marge_bitfit_page_config_variant_is_enabled_override;
    }
    PAS_ASSERT(!"Should not be reached");
    return false;
}

static PAS_ALWAYS_INLINE uintptr_t
pas_bitfit_page_config_object_payload_end_offset_from_boundary(pas_bitfit_page_config config)
{
    return config.page_object_payload_offset + config.page_object_payload_size;
}

static PAS_ALWAYS_INLINE size_t pas_bitfit_page_config_num_alloc_bits(pas_bitfit_page_config config)
{
    return PAS_BITFIT_PAGE_CONFIG_NUM_ALLOC_BITS(config.base.page_size,
                                                 config.base.min_align_shift);
}

static PAS_ALWAYS_INLINE size_t pas_bitfit_page_config_num_alloc_words(pas_bitfit_page_config config)
{
    return PAS_BITVECTOR_NUM_WORDS(pas_bitfit_page_config_num_alloc_bits(config));
}

static PAS_ALWAYS_INLINE size_t
pas_bitfit_page_config_num_alloc_words64(pas_bitfit_page_config config)
{
    return PAS_BITVECTOR_NUM_WORDS64(pas_bitfit_page_config_num_alloc_bits(config));
}

static PAS_ALWAYS_INLINE size_t
pas_bitfit_page_config_num_alloc_bit_bytes(pas_bitfit_page_config config)
{
    return PAS_BITFIT_PAGE_CONFIG_NUM_ALLOC_BIT_BYTES(config.base.page_size,
                                                      config.base.min_align_shift);
}

static PAS_ALWAYS_INLINE size_t
pas_bitfit_page_config_byte_offset_for_object_bits(pas_bitfit_page_config config)
{
    return PAS_BITFIT_PAGE_CONFIG_BYTE_OFFSET_FOR_OBJECT_BITS(config.base.page_size,
                                                              config.base.min_align_shift);
}

PAS_END_EXTERN_C;

#endif /* PAS_BITFIT_PAGE_CONFIG_H */

