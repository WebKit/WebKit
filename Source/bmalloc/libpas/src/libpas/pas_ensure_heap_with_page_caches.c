/*
 * Copyright (c) 2020 Apple Inc. All rights reserved.
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

#include "pas_ensure_heap_with_page_caches.h"

#include "pas_basic_heap_runtime_config.h"
#include "pas_heap_lock.h"
#include "pas_immortal_heap.h"

pas_heap* pas_ensure_heap_with_page_caches(
    pas_heap_ref* heap_ref,
    pas_heap_ref_kind heap_ref_kind,
    pas_heap_config* config,
    pas_heap_runtime_config* template_runtime_config,
    pas_basic_heap_page_caches* page_caches)
{
    pas_basic_heap_runtime_config* runtime_config;

    pas_heap_lock_lock();

    runtime_config = pas_immortal_heap_allocate(
        sizeof(pas_basic_heap_runtime_config),
        "pas_basic_heap_runtime_config",
        pas_object_allocation);

    pas_heap_lock_unlock();

    runtime_config->base = *template_runtime_config;
    runtime_config->page_caches = page_caches;

    PAS_ASSERT(!heap_ref->heap);
    PAS_ASSERT(heap_ref->allocator_index == UINT_MAX);

    return pas_ensure_heap(heap_ref, heap_ref_kind, config, &runtime_config->base);
}

#endif /* LIBPAS_ENABLED */
