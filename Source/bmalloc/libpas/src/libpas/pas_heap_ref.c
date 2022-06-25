/*
 * Copyright (c) 2018-2020 Apple Inc. All rights reserved.
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

#include "pas_heap_ref.h"

#include "pas_config.h"
#include "pas_heap.h"
#include "pas_heap_lock.h"
#include "pas_utils.h"

pas_heap* pas_ensure_heap_slow(pas_heap_ref* heap_ref,
                               pas_heap_ref_kind heap_ref_kind,
                               pas_heap_config* config,
                               pas_heap_runtime_config* runtime_config)
{
    pas_heap* heap;

    PAS_ASSERT(heap_ref_kind != pas_fake_heap_ref_kind);

    pas_heap_lock_lock();
    heap = heap_ref->heap;
    if (!heap) {
        heap = pas_heap_create(heap_ref, heap_ref_kind, config, runtime_config);
        pas_store_store_fence();
        heap_ref->heap = heap;
    }
    pas_heap_lock_unlock();
    return heap;
}

#endif /* LIBPAS_ENABLED */
