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

#include "pas_enumerate_unaccounted_pages_as_meta.h"

#include "pas_enumerator.h"
#include "pas_ptr_hash_set.h"
#include "pas_ptr_min_heap.h"
#include "pas_root.h"

bool pas_enumerate_unaccounted_pages_as_meta(pas_enumerator* enumerator)
{
    size_t index;
    pas_ptr_min_heap ptr_heap;
    void* span_begin;
    void* span_end;
    void* page;

    if (!enumerator->record_meta)
        return true;

    pas_ptr_min_heap_construct(&ptr_heap);

    for (index = pas_ptr_hash_set_entry_index_end(enumerator->unaccounted_pages); index--;) {
        page = *pas_ptr_hash_set_entry_at_index(enumerator->unaccounted_pages, index);

        if (pas_ptr_hash_set_entry_is_empty_or_deleted(page))
            continue;

        PAS_ASSERT_WITH_DETAIL(page);

        pas_ptr_min_heap_add(&ptr_heap, page, &enumerator->allocation_config);
    }

    span_begin = NULL;
    span_end = NULL;
    while ((page = pas_ptr_min_heap_take_min(&ptr_heap))) {
        if (span_end != page) {
            PAS_ASSERT_WITH_DETAIL(page > span_end);
            pas_enumerator_record(
                enumerator,
                span_begin,
                (uintptr_t)span_end - (uintptr_t)span_begin,
                pas_enumerator_meta_record);
            span_begin = page;
        }
        span_end = (char*)page + enumerator->root->page_malloc_alignment;
    }
    pas_enumerator_record(
        enumerator,
        span_begin,
        (uintptr_t)span_end - (uintptr_t)span_begin,
        pas_enumerator_meta_record);

    return true;
}

#endif /* LIBPAS_ENABLED */
