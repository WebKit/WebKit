/*
 * Copyright (c) 2026 Apple Inc. All rights reserved.
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

#include "js_marked_block_heap.h"

#if PAS_ENABLE_BMALLOC

#include "bmalloc_heap_ref.h"
#include "bmalloc_type.h"
#include "js_heap_config.h"
#include "js_heap_innards.h"
#include "js_heap_ref.h"
#include "pas_ensure_heap_forced_into_reserved_memory.h"
#include "pas_heap.h"
#include "pas_heap_lock.h"
#include "pas_large_heap.h"
#include "pas_large_sharing_pool.h"
#include "pas_physical_memory_transaction.h"
#include "pas_primitive_heap_ref.h"

PAS_BEGIN_EXTERN_C;

static const bmalloc_type js_marked_block_heap_type = BMALLOC_TYPE_INITIALIZER(
    JS_MARKED_BLOCK_HEAP_BLOCK_SIZE,
    JS_MARKED_BLOCK_HEAP_BLOCK_SIZE,
    "JS MarkedBlock Heap");

pas_primitive_heap_ref js_marked_block_heap_ref =
    JS_PRIMITIVE_HEAP_REF_INITIALIZER(
        &js_marked_block_heap_type, pas_bmalloc_heap_ref_kind_compact);

void js_marked_block_heap_force_into_reserved_memory(uintptr_t begin, uintptr_t end)
{
    pas_ensure_heap_forced_into_reserved_memory(
        &js_marked_block_heap_ref.base, pas_primitive_heap_ref_kind,
        &js_heap_config, &js_primitive_runtime_config.base, begin, end);
}

/* Return the backing pas_large_heap. The heap must already have been brought
   up by js_marked_block_heap_force_into_reserved_memory. */
static pas_large_heap* get_large_heap(void)
{
    pas_heap* heap = js_marked_block_heap_ref.base.heap;
    PAS_ASSERT(heap);
    return &heap->large_heap;
}

void* js_marked_block_heap_try_allocate(void)
{
    pas_allocation_result result = pas_allocation_result_create_failure();

    /* When pas_large_sharing_pool is disabled (the common low-memory case)
       pas_large_sharing_pool_allocate_and_commit is a no-op, so no commit
       transaction is needed. Take the simple, single-shot path. Otherwise
       fall back to the standard construct/begin/end retry loop so the
       sharing pool can arrange for the virtual-range-common-lock to be
       acquired without nesting under the heap lock. */
    if (!pas_large_sharing_pool_enabled) {
        pas_heap_lock_lock();
        result = pas_large_heap_try_allocate_user_allocation(
            get_large_heap(),
            JS_MARKED_BLOCK_HEAP_BLOCK_SIZE,
            JS_MARKED_BLOCK_HEAP_BLOCK_SIZE,
            pas_always_compact_allocation_mode,
            &js_heap_config, NULL);
        pas_heap_lock_unlock();
    } else {
        pas_physical_memory_transaction transaction;
        pas_physical_memory_transaction_construct(&transaction);
        do {
            pas_physical_memory_transaction_begin(&transaction);
            pas_heap_lock_lock();

            result = pas_large_heap_try_allocate_user_allocation(
                get_large_heap(),
                JS_MARKED_BLOCK_HEAP_BLOCK_SIZE,
                JS_MARKED_BLOCK_HEAP_BLOCK_SIZE,
                pas_always_compact_allocation_mode,
                &js_heap_config, &transaction);

            pas_heap_lock_unlock();
        } while (!pas_physical_memory_transaction_end(&transaction));
    }

    if (!result.did_succeed)
        return NULL;
    return (void*)result.begin;
}

void js_marked_block_heap_deallocate(void* ptr)
{
    PAS_ASSERT(ptr);
    pas_heap_lock_lock();
    pas_large_heap_try_deallocate((uintptr_t)ptr, &js_heap_config);
    pas_heap_lock_unlock();
}

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_BMALLOC */

#endif /* LIBPAS_ENABLED */
