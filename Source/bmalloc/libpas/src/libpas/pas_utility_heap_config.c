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

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_utility_heap_config.h"

#include "pas_compact_bootstrap_free_heap.h"
#include "pas_heap_config_utils_inlines.h"
#include "pas_utility_heap.h"

pas_segregated_shared_page_directory pas_utility_heap_shared_page_directory =
    PAS_SEGREGATED_SHARED_PAGE_DIRECTORY_INITIALIZER(
        PAS_UTILITY_HEAP_CONFIG.small_segregated_config,
        pas_share_pages);

pas_heap_config pas_utility_heap_config = PAS_UTILITY_HEAP_CONFIG;

PAS_SEGREGATED_PAGE_CONFIG_SPECIALIZATION_DEFINITIONS(
    pas_utility_heap_page_config, PAS_UTILITY_HEAP_CONFIG.small_segregated_config);
PAS_HEAP_CONFIG_SPECIALIZATION_DEFINITIONS(
    pas_utility_heap_config, PAS_UTILITY_HEAP_CONFIG);

void* pas_utility_heap_allocate_page(pas_segregated_heap* heap)
{
    PAS_UNUSED_PARAM(heap);
    return (void*)pas_compact_bootstrap_free_heap_try_allocate_with_alignment(
        PAS_SMALL_PAGE_DEFAULT_SIZE,
        pas_alignment_create_traditional(PAS_SMALL_PAGE_DEFAULT_SIZE),
        "pas_utility_heap/page",
        pas_delegate_allocation).begin;
}

bool pas_utility_heap_config_for_each_shared_page_directory(
    pas_segregated_heap* heap,
    bool (*callback)(pas_segregated_shared_page_directory* directory,
                     void* arg),
    void* arg)
{
    PAS_ASSERT(heap == &pas_utility_segregated_heap);
    return callback(&pas_utility_heap_shared_page_directory, arg);
}

#endif /* LIBPAS_ENABLED */
