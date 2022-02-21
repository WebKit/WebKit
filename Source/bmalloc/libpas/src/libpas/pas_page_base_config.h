/*
 * Copyright (c) 2020-2021 Apple Inc. All rights reserved.
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

#ifndef PAS_PAGE_BASE_CONFIG_H
#define PAS_PAGE_BASE_CONFIG_H

#include "pas_lock.h"
#include "pas_page_config_kind.h"
#include "pas_page_kind.h"
#include "pas_page_granule_use_count.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

struct pas_bitfit_page_config;
struct pas_enumerator;
struct pas_heap_config;
struct pas_page_base;
struct pas_page_base_config;
struct pas_physical_memory_transaction;
struct pas_segregated_heap;
struct pas_segregated_page_config;
typedef struct pas_bitfit_page_config pas_bitfit_page_config;
typedef struct pas_enumerator pas_enumerator;
typedef struct pas_heap_config pas_heap_config;
typedef struct pas_page_base pas_page_base;
typedef struct pas_page_base_config pas_page_base_config;
typedef struct pas_physical_memory_transaction pas_physical_memory_transaction;
typedef struct pas_segregated_heap pas_segregated_heap;
typedef struct pas_segregated_page_config pas_segregated_page_config;

typedef pas_page_base* (*pas_page_base_config_page_header_for_boundary)(void* boundary);
typedef void* (*pas_page_base_config_boundary_for_page_header)(pas_page_base* page);
typedef pas_page_base* (*pas_page_base_config_page_header_for_boundary_remote)(
    pas_enumerator* enumerator, void* boundary);
typedef pas_page_base* (*pas_page_base_config_create_page_header)(
    void* boundary, pas_page_kind kind, pas_lock_hold_mode heap_lock_hold_mode);
typedef void (*pas_page_base_config_destroy_page_header)(
    pas_page_base* page_base, pas_lock_hold_mode heap_lock_hold_mode);

struct pas_page_base_config {
    /* Are we using this page config? */
    bool is_enabled;

    /* This points to the owning heap config. Currently there is always an owning heap config. */
    pas_heap_config* heap_config_ptr;
    
    /* This always self-points. It's useful for going from a page_config to a page_config_ptr. */
    pas_page_base_config* page_config_ptr;

    /* What page_kind to put in pages allocated by this config. This happens to tell if the config
       is a segregated or a bitfit config. */
    pas_page_config_kind page_config_kind;

    /* Smallest small object size and the minimum alignment. */
    uint8_t min_align_shift;

    /* Page size. This is the amount of bytes that we allocate to create one. */
    size_t page_size;

    /* The commit granule size. */
    size_t granule_size;

    /* Hard cut-off for object sizes for this variant. For segregated page configs, it's recommended
       that this is something that divides cleanly into page_object_payload_size. For bitfit page
       configs, this must be strictly smaller than PAS_BITFIT_MAX_FREE_UNPROCESSED * min_align. */
    size_t max_object_size;
    
    /* How do we go from the page boundary to the page header and back again? */
    pas_page_base_config_page_header_for_boundary page_header_for_boundary;
    pas_page_base_config_boundary_for_page_header boundary_for_page_header;

    /* Need to also be able to get the page header remotely during enumeration. */
    pas_page_base_config_page_header_for_boundary_remote page_header_for_boundary_remote;
    
    /* Some configurations need to be able to allocate/free the page header. The allocation would
       happen after page allocation or commit, and the deallocation would happen right before or
       right after page decommit. */
    pas_page_base_config_create_page_header create_page_header;
    pas_page_base_config_destroy_page_header destroy_page_header;
};

static PAS_ALWAYS_INLINE size_t pas_page_base_config_min_align(pas_page_base_config config)
{
    return (size_t)1 << (size_t)config.min_align_shift;
}

#define PAS_PAGE_BASE_CONFIG_NUM_GRANULE_BYTES(num_granules) \
    ((num_granules) == 1 ? 0 : (num_granules) * sizeof(pas_page_granule_use_count))

static PAS_ALWAYS_INLINE size_t
pas_page_base_config_num_granule_bytes(pas_page_base_config config)
{
    return PAS_PAGE_BASE_CONFIG_NUM_GRANULE_BYTES(config.page_size / config.granule_size);
}

static PAS_ALWAYS_INLINE bool pas_page_base_config_is_segregated(pas_page_base_config config)
{
    return config.page_config_kind == pas_page_config_kind_segregated;
}

static PAS_ALWAYS_INLINE bool pas_page_base_config_is_bitfit(pas_page_base_config config)
{
    return config.page_config_kind == pas_page_config_kind_bitfit;
}

static inline pas_segregated_page_config*
pas_page_base_config_get_segregated(pas_page_base_config* config)
{
    PAS_ASSERT(pas_page_base_config_is_segregated(*config));
    return (pas_segregated_page_config*)config;
}

static inline pas_bitfit_page_config*
pas_page_base_config_get_bitfit(pas_page_base_config* config)
{
    PAS_ASSERT(pas_page_base_config_is_bitfit(*config));
    return (pas_bitfit_page_config*)config;
}

PAS_API const char* pas_page_base_config_get_kind_string(pas_page_base_config* config);

PAS_END_EXTERN_C;

#endif /* PAS_PAGE_BASE_CONFIG_H */

