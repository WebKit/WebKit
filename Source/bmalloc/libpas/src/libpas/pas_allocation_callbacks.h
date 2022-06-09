/*
 * Copyright (c) 2019 Apple Inc. All rights reserved.
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

#ifndef PAS_ALLOCATION_CALLBACKS_H
#define PAS_ALLOCATION_CALLBACKS_H

#include "pas_allocation_kind.h"
#include "pas_heap_kind.h"
#include "pas_log.h"

PAS_BEGIN_EXTERN_C;

PAS_API extern void (*pas_allocation_callback)(
    void* resulting_base,
    size_t size,
    pas_heap_kind heap_kind,
    const char* name,
    pas_allocation_kind allocation_kind);
PAS_API extern void (*pas_deallocation_callback)(
    void* base,
    size_t size, /* This is zero for non-free heaps like utility. */
    pas_heap_kind heap_kind,
    pas_allocation_kind allocation_kind);

static inline void pas_did_allocate(
    void* resulting_base,
    size_t size,
    pas_heap_kind heap_kind,
    const char* name,
    pas_allocation_kind allocation_kind)
{
    static const bool verbose = false;

    if (verbose) {
        pas_log("Doing pas_did_allocate with size = %zu, heap_kind = %s, name = %s, "
                "allocation_kind = %s.\n",
                size, pas_heap_kind_get_string(heap_kind), name,
                pas_allocation_kind_get_string(allocation_kind));
    }
    
    if (pas_allocation_callback && resulting_base)
        pas_allocation_callback(resulting_base, size, heap_kind, name, allocation_kind);
}

static inline void pas_will_deallocate(
    void* base,
    size_t size, /* This is zero for non-free heaps like utility. */
    pas_heap_kind heap_kind,
    pas_allocation_kind allocation_kind)
{
    if (pas_deallocation_callback && base)
        pas_deallocation_callback(base, size, heap_kind, allocation_kind);
}

PAS_END_EXTERN_C;

#endif /* PAS_ALLOCATION_CALLBACKS_H */

