/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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

#include "IsoHeap.h"

#include "AllocationCounts.h"

#if BUSE(LIBPAS)

#include "bmalloc_heap_inlines.h"

#include "IsoMallocFallback.h"

namespace bmalloc { namespace api {

void* isoAllocate(pas_heap_ref& heapRef)
{
    // FIXME: libpas should know how to do the fallback thing.
    // https://bugs.webkit.org/show_bug.cgi?id=227177
    
    auto typeSize = pas_simple_type_size(reinterpret_cast<pas_simple_type>(heapRef.type));
    if (IsoMallocFallback::shouldTryToFallBack()) {
        IsoMallocFallback::MallocResult result = IsoMallocFallback::tryMalloc(typeSize);
        if (result.didFallBack) {
            RELEASE_BASSERT(result.ptr);
            BPROFILE_ALLOCATION(NON_JS_CELL, result.ptr, typeSize);
            return result.ptr;
        }
    }

    void* result = bmalloc_iso_allocate_inline(&heapRef);
    BPROFILE_ALLOCATION(NON_JS_CELL, result, typeSize);
    return result;
}

void* isoTryAllocate(pas_heap_ref& heapRef)
{
    auto typeSize = pas_simple_type_size(reinterpret_cast<pas_simple_type>(heapRef.type));
    if (IsoMallocFallback::shouldTryToFallBack()) {
        IsoMallocFallback::MallocResult result = IsoMallocFallback::tryMalloc(typeSize);
        if (result.didFallBack) {
            BPROFILE_TRY_ALLOCATION(NON_JS_CELL, result.ptr, typeSize);
            return result.ptr;
        }
    }

    void* result = bmalloc_try_iso_allocate_inline(&heapRef);
    BPROFILE_TRY_ALLOCATION(NON_JS_CELL, result, typeSize);
    return result;
}

void* isoAllocateCompact(pas_heap_ref& heapRef)
{
    // FIXME: libpas should know how to do the fallback thing.
    // https://bugs.webkit.org/show_bug.cgi?id=227177

    auto typeSize = pas_simple_type_size(reinterpret_cast<pas_simple_type>(heapRef.type));
    if (IsoMallocFallback::shouldTryToFallBack()) {
        IsoMallocFallback::MallocResult result = IsoMallocFallback::tryMalloc(typeSize);
        if (result.didFallBack) {
            RELEASE_BASSERT(result.ptr);
            BPROFILE_ALLOCATION(COMPACTIBLE, result.ptr, typeSize);
            return result.ptr;
        }
    }

    void* result = bmalloc_iso_allocate_inline(&heapRef);
    BPROFILE_ALLOCATION(COMPACTIBLE, result, typeSize);
    return result;
}

void* isoTryAllocateCompact(pas_heap_ref& heapRef)
{
    auto typeSize = pas_simple_type_size(reinterpret_cast<pas_simple_type>(heapRef.type));
    if (IsoMallocFallback::shouldTryToFallBack()) {
        IsoMallocFallback::MallocResult result = IsoMallocFallback::tryMalloc(typeSize);
        if (result.didFallBack) {
            BPROFILE_TRY_ALLOCATION(NON_JS_CELL, result.ptr, typeSize);
            return result.ptr;
        }
    }

    void* result = bmalloc_try_iso_allocate_inline(&heapRef);
    BPROFILE_TRY_ALLOCATION(COMPACTIBLE, result, typeSize);
    return result;
}

void isoDeallocate(void* ptr)
{
    if (IsoMallocFallback::shouldTryToFallBack()
        && IsoMallocFallback::tryFree(ptr))
        return;

    bmalloc_deallocate_inline(ptr);
}

} } // namespace bmalloc::api

#endif // BUSE(LIBPAS)

