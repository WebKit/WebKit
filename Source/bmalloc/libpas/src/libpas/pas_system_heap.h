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

#ifndef PAS_SYSTEM_HEAP_H
#define PAS_SYSTEM_HEAP_H

#include "pas_allocation_mode.h"
#include "pas_allocation_result.h"
#include "pas_heap_config_kind.h"
#include "pas_log.h"
#include "pas_utils.h"

#if PAS_BMALLOC
// FIXME: Find a way to declare bmalloc's symbol visibility without having to
// import a bmalloc header.
#include "BExport.h"
#endif

PAS_BEGIN_EXTERN_C;

/* Bmalloc has a SystemHeap singleton that can be used to divert bmalloc calls to system malloc.
   When libpas is used in bmalloc, we use this to glue libpas into that mechanism. */

#if PAS_BMALLOC


/* The implementations are provided by bmalloc. */
BEXPORT extern bool pas_system_heap_is_enabled(pas_heap_config_kind);
BEXPORT extern bool pas_system_heap_should_supplant_bmalloc(pas_heap_config_kind);
BEXPORT extern void* pas_system_heap_malloc(size_t);
BEXPORT extern void* pas_system_heap_memalign(size_t alignment, size_t);
BEXPORT extern void* pas_system_heap_realloc(void* ptr, size_t);
BEXPORT extern void* pas_system_heap_malloc_compact(size_t);
BEXPORT extern void* pas_system_heap_memalign_compact(size_t alignment, size_t);
BEXPORT extern void* pas_system_heap_realloc_compact(void* ptr, size_t);
BEXPORT extern void pas_system_heap_free(void* ptr);

#else /* PAS_BMALLOC -> so !PAS_BMALLOC */

static inline bool pas_system_heap_is_enabled(pas_heap_config_kind kind)
{
    PAS_UNUSED_PARAM(kind);
    return false;
}

static inline bool pas_system_heap_should_supplant_bmalloc(pas_heap_config_kind kind)
{
    PAS_UNUSED_PARAM(kind);
    return false;
}

static inline void* pas_system_heap_malloc(size_t size)
{
    PAS_UNUSED_PARAM(size);
    PAS_ASSERT_NOT_REACHED();
    return NULL;
}

static inline void* pas_system_heap_memalign(size_t alignment, size_t size)
{
    PAS_UNUSED_PARAM(alignment);
    PAS_UNUSED_PARAM(size);
    PAS_ASSERT_NOT_REACHED();
    return NULL;
}

static inline void* pas_system_heap_realloc(void* ptr, size_t size)
{
    PAS_UNUSED_PARAM(ptr);
    PAS_UNUSED_PARAM(size);
    PAS_ASSERT_NOT_REACHED();
    return NULL;
}

static inline void* pas_system_heap_malloc_compact(size_t size)
{
    PAS_UNUSED_PARAM(size);
    PAS_ASSERT_NOT_REACHED();
    return NULL;
}

static inline void* pas_system_heap_memalign_compact(size_t alignment, size_t size)
{
    PAS_UNUSED_PARAM(alignment);
    PAS_UNUSED_PARAM(size);
    PAS_ASSERT_NOT_REACHED();
    return NULL;
}

static inline void* pas_system_heap_realloc_compact(void* ptr, size_t size)
{
    PAS_UNUSED_PARAM(ptr);
    PAS_UNUSED_PARAM(size);
    PAS_ASSERT_NOT_REACHED();
    return NULL;
}

static inline void pas_system_heap_free(void* ptr)
{
    PAS_UNUSED_PARAM(ptr);
    PAS_ASSERT_NOT_REACHED();
}

#endif /* PAS_BMALLOC -> so end of !PAS_BMALLOC */

static inline pas_allocation_result pas_system_heap_allocate(size_t size, size_t alignment, pas_allocation_mode allocation_mode)
{
    static const bool verbose = false;

    pas_allocation_result result;
    void* raw_result;

    if (alignment > sizeof(void*)) {
        if (verbose)
            pas_log("Going down debug memalign path.\n");
        raw_result = allocation_mode == pas_non_compact_allocation_mode
            ? pas_system_heap_memalign(alignment, size)
            : pas_system_heap_memalign_compact(alignment, size);
    } else {
        if (verbose)
            pas_log("Going down debug malloc path.\n");
        raw_result = allocation_mode == pas_non_compact_allocation_mode
            ? pas_system_heap_malloc(size)
            : pas_system_heap_malloc_compact(size);
    }

    if (verbose)
        pas_log("raw_result = %p\n", raw_result);

    result.did_succeed = !!raw_result;
    result.begin = (uintptr_t)raw_result;
    result.zero_mode = pas_zero_mode_may_have_non_zero;

    return result;
}

PAS_END_EXTERN_C;

#endif /* PAS_SYSTEM_HEAP_H */
