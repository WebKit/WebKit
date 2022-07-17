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

#ifndef PAS_HEAP_RUNTIME_CONFIG_H
#define PAS_HEAP_RUNTIME_CONFIG_H

#include "pas_page_sharing_mode.h"
#include "pas_segregated_heap_lookup_kind.h"
#include "pas_segregated_page_config_variant.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

struct pas_heap_runtime_config;
struct pas_segregated_page_config;
typedef struct pas_heap_runtime_config pas_heap_runtime_config;
typedef struct pas_segregated_page_config pas_segregated_page_config;

typedef size_t (*pas_heap_runtime_config_view_cache_capacity_for_object_size_callback)(
    size_t object_size, const pas_segregated_page_config* page_config);

struct pas_heap_runtime_config {
    pas_segregated_heap_lookup_kind lookup_kind : 8;
    pas_page_sharing_mode sharing_mode : 8;

    bool statically_allocated : 1;
    bool is_part_of_heap : 1;
    
    unsigned directory_size_bound_for_partial_views;
    unsigned directory_size_bound_for_baseline_allocators;
    unsigned directory_size_bound_for_no_view_cache;

    /* It's OK to set these to UINT_MAX, in which case the maximum is decided by the heap_config. */
    unsigned max_segregated_object_size;
    unsigned max_bitfit_object_size;

    pas_heap_runtime_config_view_cache_capacity_for_object_size_callback view_cache_capacity_for_object_size;
};

PAS_API uint8_t pas_heap_runtime_config_view_cache_capacity_for_object_size(
    pas_heap_runtime_config* config,
    size_t object_size,
    const pas_segregated_page_config* page_config);

PAS_API size_t pas_heap_runtime_config_zero_view_cache_capacity(
    size_t object_size,
    const pas_segregated_page_config* page_config);

PAS_API size_t pas_heap_runtime_config_aggressive_view_cache_capacity(
    size_t object_size,
    const pas_segregated_page_config* page_config);

PAS_END_EXTERN_C;

#endif /* PAS_HEAP_RUNTIME_CONFIG_H */

