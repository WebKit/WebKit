/*
 * Copyright (c) 2021-2022 Apple Inc. All rights reserved.
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

#include "jit_heap.h"

#if PAS_ENABLE_JIT

#include "jit_heap_config.h"
#include "pas_deallocate.h"
#include "pas_get_allocation_size.h"
#include "pas_try_allocate_intrinsic.h"
#include "pas_try_shrink.h"

PAS_BEGIN_EXTERN_C;

pas_heap jit_common_primitive_heap = PAS_INTRINSIC_HEAP_INITIALIZER(
    &jit_common_primitive_heap,
    NULL,
    jit_common_primitive_heap_support,
    JIT_HEAP_CONFIG,
    &jit_heap_runtime_config);

pas_intrinsic_heap_support jit_common_primitive_heap_support = PAS_INTRINSIC_HEAP_SUPPORT_INITIALIZER;

pas_allocator_counts jit_allocator_counts;

void jit_heap_add_fresh_memory(pas_range range)
{
    static const bool verbose = false;

    if (verbose)
        pas_log("JIT heap at %p...%p\n", (void*)range.begin, (void*)range.end);
    
    pas_heap_lock_lock();
    jit_heap_config_add_fresh_memory(range);
    pas_heap_lock_unlock();
}

PAS_CREATE_TRY_ALLOCATE_INTRINSIC(
    jit_try_allocate_common_primitive_impl,
    JIT_HEAP_CONFIG,
    &jit_heap_runtime_config,
    &jit_allocator_counts,
    pas_allocation_result_identity,
    &jit_common_primitive_heap,
    &jit_common_primitive_heap_support,
    pas_intrinsic_heap_is_not_designated);

void* jit_heap_try_allocate(size_t size)
{
    static const bool verbose = false;
    void* result;
    if (verbose)
        pas_log("going to allocate in jit\n");
    result = (void*)jit_try_allocate_common_primitive_impl(size, 1, pas_compact_allocation_mode).begin;
    if (verbose)
        pas_log("done allocating in jit, returning %p\n", result);
    return result;
}

void jit_heap_shrink(void* object, size_t new_size)
{
    /* NOTE: the shrink call will fail (return false) for segregated allocations, and that's fine because we
       only use segregated allocations for smaller sizes (so the amount of potential memory savings from
       shrinking is small). */
    pas_try_shrink(object, new_size, JIT_HEAP_CONFIG);
}

size_t jit_heap_get_size(void* object)
{
    return pas_get_allocation_size(object, JIT_HEAP_CONFIG);
}

void jit_heap_deallocate(void* object)
{
    pas_deallocate(object, JIT_HEAP_CONFIG);
}

PAS_END_EXTERN_C;

#endif /* PAS_ENABLE_JIT */

#endif /* LIBPAS_ENABLED */
