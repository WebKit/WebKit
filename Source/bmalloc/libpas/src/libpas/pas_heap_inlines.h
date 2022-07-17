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

#ifndef PAS_HEAP_INLINES_H
#define PAS_HEAP_INLINES_H

#include "pas_allocator_counts.h"
#include "pas_config.h"
#include "pas_heap.h"
#include "pas_log.h"
#include "pas_segregated_heap_inlines.h"

PAS_BEGIN_EXTERN_C;

PAS_API pas_segregated_size_directory*
pas_heap_ensure_size_directory_for_size_slow(
    pas_heap* heap,
    size_t size,
    size_t alignment,
    pas_size_lookup_mode force_size_lookup,
    const pas_heap_config* config,
    unsigned* cached_index);

static PAS_ALWAYS_INLINE pas_segregated_size_directory*
pas_heap_ensure_size_directory_for_size(
    pas_heap* heap,
    size_t size,
    size_t alignment,
    pas_size_lookup_mode force_size_lookup,
    pas_heap_config config,
    unsigned* cached_index,
    pas_allocator_counts* counts)
{
    static const bool verbose = false;

    pas_segregated_size_directory* result;

    PAS_UNUSED_PARAM(counts);
    
    if (verbose) {
        pas_log("%p: getting directory with size = %zu, alignment = %zu.\n",
                heap, size, alignment);
    }
    
    result = pas_segregated_heap_size_directory_for_size(
        &heap->segregated_heap, size, config, cached_index);
    if (result && pas_segregated_size_directory_alignment(result) >= alignment)
        return result;

#if PAS_ENABLE_TESTING
    counts->slow_paths++;
#endif /* PAS_ENABLE_TESTING */
    
    return pas_heap_ensure_size_directory_for_size_slow(
        heap, size, alignment, force_size_lookup, config.config_ptr, cached_index);
}

PAS_END_EXTERN_C;

#endif /* PAS_HEAP_INLINES_H */

