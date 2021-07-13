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

#include "jit_heap.h"

#if PAS_ENABLE_JIT

#include "jit_heap_config.h"
#include "pas_deallocate.h"
#include "pas_get_allocation_size.h"
#include "pas_try_allocate_intrinsic_primitive.h"
#include "pas_try_shrink.h"

pas_heap jit_common_primitive_heap = PAS_INTRINSIC_PRIMITIVE_HEAP_INITIALIZER(
    &jit_common_primitive_heap,
    NULL,
    jit_common_primitive_heap_support,
    JIT_HEAP_CONFIG,
    &jit_heap_runtime_config);

pas_intrinsic_heap_support jit_common_primitive_heap_support = PAS_INTRINSIC_HEAP_SUPPORT_INITIALIZER;

pas_allocator_counts jit_allocator_counts;

void jit_heap_add_fresh_memory(pas_range range)
{
    pas_heap_lock_lock();
    jit_heap_config_add_fresh_memory(range);
    pas_heap_lock_unlock();
}

PAS_CREATE_TRY_ALLOCATE_INTRINSIC_PRIMITIVE(
    jit_try_allocate_common_primitive_impl,
    JIT_HEAP_CONFIG,
    &jit_heap_runtime_config,
    &jit_allocator_counts,
    pas_intrinsic_allocation_result_identity,
    &jit_common_primitive_heap,
    &jit_common_primitive_heap_support,
    pas_intrinsic_heap_is_not_designated);

void* jit_heap_try_allocate(size_t size)
{
    return jit_try_allocate_common_primitive_impl(size, 1).ptr;
}

void jit_heap_shrink(void* object, size_t new_size)
{
    bool result;
    result = pas_try_shrink(object, new_size, JIT_HEAP_CONFIG);
    PAS_ASSERT(result);
}

size_t jit_heap_get_size(void* object)
{
    return pas_get_allocation_size(object, JIT_HEAP_CONFIG);
}

void jit_heap_deallocate(void* object)
{
    pas_deallocate(object, JIT_HEAP_CONFIG);
}

#endif /* PAS_ENABLE_JIT */

#endif /* LIBPAS_ENABLED */
