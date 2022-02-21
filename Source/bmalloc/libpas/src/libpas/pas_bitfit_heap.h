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

#ifndef PAS_BITFIT_HEAP_H
#define PAS_BITFIT_HEAP_H

#include "pas_bitfit_directory.h"
#include "pas_bitfit_page_config_variant.h"

PAS_BEGIN_EXTERN_C;

struct pas_bitfit_heap;
struct pas_heap_config;
struct pas_heap_runtime_config;
struct pas_segregated_size_directory;
struct pas_segregated_heap;
typedef struct pas_bitfit_heap pas_bitfit_heap;
typedef struct pas_heap_config pas_heap_config;
typedef struct pas_heap_runtime_config pas_heap_runtime_config;
typedef struct pas_segregated_size_directory pas_segregated_size_directory;
typedef struct pas_segregated_heap pas_segregated_heap;

struct PAS_ALIGNED(sizeof(pas_versioned_field)) pas_bitfit_heap {
    pas_bitfit_directory directories[PAS_NUM_BITFIT_PAGE_CONFIG_VARIANTS];
};

PAS_API pas_bitfit_heap* pas_bitfit_heap_create(pas_segregated_heap* heap,
                                                pas_heap_config* heap_config);

static inline pas_bitfit_directory* pas_bitfit_heap_get_directory(
    pas_bitfit_heap* heap,
    pas_bitfit_page_config_variant variant)
{
    PAS_ASSERT((unsigned)variant <= PAS_NUM_BITFIT_PAGE_CONFIG_VARIANTS);
    return heap->directories + (unsigned)variant;
}

typedef struct {
    unsigned object_size;
    pas_bitfit_page_config_variant variant;
} pas_bitfit_variant_selection;

PAS_API pas_bitfit_variant_selection
pas_bitfit_heap_select_variant(size_t object_size,
                               pas_heap_config* config,
                               pas_heap_runtime_config* runtime_config);

PAS_API void pas_bitfit_heap_construct_and_insert_size_class(pas_bitfit_heap* heap,
                                                             pas_bitfit_size_class* size_class,
                                                             unsigned object_size,
                                                             pas_heap_config* config,
                                                             pas_heap_runtime_config* runtime_config);

PAS_API pas_heap_summary pas_bitfit_heap_compute_summary(pas_bitfit_heap* heap);

typedef bool (*pas_bitfit_heap_for_each_live_object_callback)(
    pas_bitfit_heap* heap,
    pas_bitfit_view* view,
    uintptr_t begin,
    size_t size,
    void* arg);

PAS_API bool pas_bitfit_heap_for_each_live_object(
    pas_bitfit_heap* heap,
    pas_bitfit_heap_for_each_live_object_callback callback,
    void* arg);

PAS_END_EXTERN_C;

#endif /* PAS_BITFIT_HEAP_H */

