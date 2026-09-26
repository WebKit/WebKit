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

#include "tagged_bmalloc_heap.h"

#if PAS_ENABLE_BMALLOC

#include "tagged_bmalloc_heap_config.h"
#include "tagged_bmalloc_heap_innards.h"
#include "bmalloc_heap_config.h"
#include "pas_ensure_heap_forced_into_reserved_memory.h"
#include "pas_get_allocation_size.h"
#include "pas_get_heap.h"
#include "pas_get_page_base.h"
#include "pas_heap_lock.h"
#include "pas_large_map.h"
#include "pas_probabilistic_guard_malloc_allocator.h"
#include "pas_try_allocate_intrinsic.h"

PAS_BEGIN_EXTERN_C;

const bmalloc_type tagged_bmalloc_common_primitive_type = BMALLOC_TYPE_INITIALIZER(1, 1, "Tagged Common Primitive");

pas_intrinsic_heap_support tagged_bmalloc_common_primitive_heap_support =
    PAS_INTRINSIC_HEAP_SUPPORT_INITIALIZER;

pas_heap tagged_bmalloc_common_primitive_heap =
    PAS_INTRINSIC_HEAP_INITIALIZER(
        &tagged_bmalloc_common_primitive_heap,
        &tagged_bmalloc_common_primitive_type,
        tagged_bmalloc_common_primitive_heap_support,
        TAGGED_BMALLOC_HEAP_CONFIG,
        &tagged_bmalloc_intrinsic_runtime_config.base);

pas_allocator_counts tagged_bmalloc_allocator_counts;

size_t tagged_bmalloc_heap_ref_get_type_size(pas_heap_ref* heap_ref)
{
    return TAGGED_BMALLOC_HEAP_CONFIG.get_type_size(heap_ref->type);
}

size_t tagged_bmalloc_get_allocation_size(void* ptr)
{
    return pas_get_allocation_size(ptr, TAGGED_BMALLOC_HEAP_CONFIG);
}

pas_heap* tagged_bmalloc_get_heap(void* ptr)
{
    return pas_get_heap(ptr, TAGGED_BMALLOC_HEAP_CONFIG);
}

bool tagged_bmalloc_owns_object(void* ptr)
{
    pas_large_map_entry entry;
    uintptr_t begin;
    bool result;

    if (!ptr)
        return false;

    /* Segregated and bitfit objects resolve out of the per-config megapage table and page
       header tables, both of which are lock-free. This is the common case. */
    if (pas_get_page_base(ptr, TAGGED_BMALLOC_HEAP_CONFIG))
        return true;
    if (pas_get_page_base(ptr, BMALLOC_HEAP_CONFIG))
        return false;

    /* Otherwise it is a large object, or not ours at all. Unlike the page tables, the large
       map is global, so it is the only thing that can tell the two configs apart here -- and
       reading it means taking the heap lock. */
    begin = (uintptr_t)ptr;

    pas_heap_lock_lock();

    if (pas_probabilistic_guard_malloc_check_exists(begin))
        entry = pas_probabilistic_guard_malloc_return_as_large_map_entry(begin);
    else
        entry = pas_large_map_find(begin);

    result = !pas_large_map_entry_is_empty(entry)
        && pas_heap_for_large_heap(entry.heap)->config_kind == pas_heap_config_kind_tagged_bmalloc;

    pas_heap_lock_unlock();

    return result;
}

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_BMALLOC */

#endif /* LIBPAS_ENABLED */
