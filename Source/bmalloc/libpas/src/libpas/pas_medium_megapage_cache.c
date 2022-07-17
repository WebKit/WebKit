/*
 * Copyright (c) 2021 Apple Inc. All rights reserved.
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

#include "pas_medium_megapage_cache.h"

#include "pas_config.h"
#include "pas_heap_config.h"
#include "pas_megapage_cache.h"
#include "pas_segregated_page.h"

void* pas_medium_megapage_cache_try_allocate(
    pas_megapage_cache* cache,
    const pas_page_base_config* config,
    bool should_zero,
    pas_heap* heap,
    pas_physical_memory_transaction* transaction)
{
    pas_megapage_cache_config cache_config;

    cache_config.megapage_size = PAS_MEDIUM_MEGAPAGE_SIZE;
    cache_config.allocation_size = config->page_size;
    cache_config.allocation_alignment =
        pas_alignment_create_traditional(cache_config.allocation_size);
    cache_config.excluded_size = 0;
    cache_config.table_set_by_index = NULL;
    cache_config.table_set_by_index_arg = NULL;
    cache_config.should_zero = should_zero;
    
    return pas_megapage_cache_try_allocate(cache, &cache_config, heap, transaction);
}

#endif /* LIBPAS_ENABLED */
