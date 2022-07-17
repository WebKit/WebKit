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

#ifndef PAS_HEAP_REF_H
#define PAS_HEAP_REF_H

#include "pas_config.h"
#include "pas_heap_ref_kind.h"
#include "pas_segregated_heap_lookup_kind.h"
#include "pas_utils.h"

#include "pas_heap_ref_prefix.h"

PAS_BEGIN_EXTERN_C;

/* You can use a pas_heap_ref for different kinds of heap configurations. Each of those heap
   configurations will have a distinct set of entrypoints for allocation, deallocation, and
   introspection. For example, thingy_deallocate will deallocate with the thingy_heap_config,
   while pxi_deallocate will deallocate with the pxi_heap_config. */

#define pas_heap __pas_heap
#define pas_heap_ref __pas_heap_ref
#define pas_heap_type __pas_heap_type

struct pas_heap_config;
struct pas_heap_runtime_config;
typedef struct pas_heap_config pas_heap_config;
typedef struct pas_heap_runtime_config pas_heap_runtime_config;

PAS_API pas_heap* pas_ensure_heap_slow(pas_heap_ref* heap_ref,
                                       pas_heap_ref_kind heap_ref_kind,
                                       const pas_heap_config* config,
                                       pas_heap_runtime_config* runtime_config);

static inline pas_heap* pas_ensure_heap(pas_heap_ref* heap_ref,
                                        pas_heap_ref_kind heap_ref_kind,
                                        const pas_heap_config* config,
                                        pas_heap_runtime_config* runtime_config)
{
    pas_heap* heap = heap_ref->heap;
    if (PAS_LIKELY(heap))
        return heap;
    return pas_ensure_heap_slow(heap_ref, heap_ref_kind, config, runtime_config);
}

PAS_END_EXTERN_C;

#endif /* PAS_HEAP_REF_H */
