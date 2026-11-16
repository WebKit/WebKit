/*
 * Copyright (c) 2019 Apple Inc. All rights reserved.
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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_reserved_memory_provider.h"

static pas_aligned_allocation_result null_aligned_allocator(size_t size,
                                                            pas_alignment alignment,
                                                            void* arg)
{
    PAS_UNUSED_PARAM(size);
    PAS_UNUSED_PARAM(alignment);
    PAS_UNUSED_PARAM(arg);
    return pas_aligned_allocation_result_create_empty();
}

static void initialize_config(pas_large_free_heap_config* config)
{
    config->type_size = 1;
    config->min_alignment = 1;
    config->aligned_allocator = null_aligned_allocator;
    config->aligned_allocator_arg = NULL;
    config->deallocator = NULL;
    config->deallocator_arg = NULL;
}

void pas_reserved_memory_provider_construct(
    pas_reserved_memory_provider* provider,
    uintptr_t begin,
    uintptr_t end)
{
    pas_large_free_heap_config config;

    initialize_config(&config);
    
    pas_simple_large_free_heap_construct(&provider->free_heap);

    pas_simple_large_free_heap_deallocate(
        &provider->free_heap, begin, end, pas_zero_mode_is_all_zero, &config);
}

pas_allocation_result pas_reserved_memory_provider_try_allocate(
    size_t size,
    pas_alignment alignment,
    const char* name,
    pas_heap* heap,
    pas_physical_memory_transaction* transaction,
    void* arg)
{
    pas_reserved_memory_provider* provider;
    pas_large_free_heap_config config;

    PAS_UNUSED_PARAM(name);
    PAS_UNUSED_PARAM(heap);
    PAS_UNUSED_PARAM(transaction);

    provider = arg;

    initialize_config(&config);

    return pas_simple_large_free_heap_try_allocate(
        &provider->free_heap, size, alignment, &config);
}

#endif /* LIBPAS_ENABLED */
