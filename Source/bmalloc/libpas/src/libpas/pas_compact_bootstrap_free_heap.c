/*
 * Copyright (c) 2018-2021 Apple Inc. All rights reserved.
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

#include "pas_compact_bootstrap_free_heap.h"

#include "pas_compact_unsigned_ptr.h"
#include "pas_config.h"
#include "pas_heap_lock.h"
#include "pas_large_free_heap_config.h"
#include "pas_enumerable_page_malloc.h"
#include "pas_simple_free_heap_helpers.h"

static pas_aligned_allocation_result compact_bootstrap_source_allocate_aligned(
    size_t size,
    pas_alignment alignment,
    void* arg)
{
    PAS_ASSERT(!arg);
    PAS_ASSERT(!alignment.alignment_begin);
    return pas_compact_heap_reservation_try_allocate(size, alignment.alignment);
}

static void initialize_config(pas_large_free_heap_config* config)
{
    config->type_size = 1;
    config->min_alignment = 1;
    config->aligned_allocator = compact_bootstrap_source_allocate_aligned;
    config->aligned_allocator_arg = NULL;
    config->deallocator = NULL;
    config->deallocator_arg = NULL;
}

#define PAS_SIMPLE_FREE_HEAP_NAME pas_compact_bootstrap_free_heap
#define PAS_SIMPLE_FREE_HEAP_ID(suffix) pas_compact_bootstrap_free_heap ## suffix
#include "pas_simple_free_heap_definitions.def"
#undef PAS_SIMPLE_FREE_HEAP_NAME
#undef PAS_SIMPLE_FREE_HEAP_ID

#endif /* LIBPAS_ENABLED */
