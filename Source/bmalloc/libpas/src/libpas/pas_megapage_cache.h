/*
 * Copyright (c) 2019-2021 Apple Inc. All rights reserved.
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

#ifndef PAS_MEGAPAGE_CACHE_H
#define PAS_MEGAPAGE_CACHE_H

#include "pas_bootstrap_heap_page_provider.h"
#include "pas_simple_large_free_heap.h"
#include "pas_utils.h"

PAS_BEGIN_EXTERN_C;

struct pas_megapage_cache;
struct pas_megapage_cache_config;
typedef struct pas_megapage_cache pas_megapage_cache;
typedef struct pas_megapage_cache_config pas_megapage_cache_config;

typedef void (*pas_megapage_cache_table_set_by_index)(size_t index, void* arg);

struct pas_megapage_cache {
    pas_simple_large_free_heap free_heap;
    pas_heap_page_provider provider;
    void* provider_arg;
};

struct pas_megapage_cache_config {
    size_t megapage_size;
    size_t allocation_size;
    pas_alignment allocation_alignment;
    size_t excluded_size; /* FIXME: Remove this, we don't use it anymore. It used to be used for putting
                             medium page headers at the start of the megapage. That was before we had the
                             page header table. */
    pas_megapage_cache_table_set_by_index table_set_by_index;
    void* table_set_by_index_arg;
    bool should_zero;
};

#define PAS_MEGAPAGE_CACHE_INITIALIZER { \
        .free_heap = PAS_SIMPLE_LARGE_FREE_HEAP_INITIALIZER, \
        .provider = pas_bootstrap_heap_page_provider, \
        .provider_arg = NULL \
    }

PAS_API void pas_megapage_cache_construct(pas_megapage_cache* cache,
                                          pas_heap_page_provider provider,
                                          void* provider_arg);

PAS_API void* pas_megapage_cache_try_allocate(pas_megapage_cache* cache,
                                              pas_megapage_cache_config* cache_config,
                                              pas_heap* heap,
                                              pas_physical_memory_transaction* transaction);

PAS_END_EXTERN_C;

#endif /* PAS_MEGAPAGE_CACHE_H */

