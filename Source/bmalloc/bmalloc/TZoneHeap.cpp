/*
 * Copyright (C) 2023-2024 Apple Inc. All rights reserved.
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

#include "BPlatform.h"
#include "TZoneHeap.h"

#if BUSE(TZONE)

#include "TZoneHeapManager.h"
#include "bmalloc.h"
#include "bmalloc_heap_internal.h"
#include "bmalloc_heap_ref.h"

#if !BUSE(LIBPAS)
#error TZONE implementation requires LIBPAS
#endif

namespace bmalloc { namespace api {

#define TO_PAS_HEAPREF(heapRef) std::bit_cast<pas_heap_ref*>(heapRef)

// HeapRef is an opaque alias for pas_heap_ref* in the underlying implementation.
static_assert(sizeof(HeapRef) == sizeof(pas_heap_ref*));

void* tzoneAllocateNonCompactSlow(size_t requestedSize, const TZoneSpecification& spec)
{
    HeapRef heapRef = *spec.addressOfHeapRef;
    if (tzoneMallocFallback != TZoneMallocFallback::DoNotFallBack) [[unlikely]] {
        if (tzoneMallocFallback == TZoneMallocFallback::Undecided) [[unlikely]] {
            TZoneHeapManager::ensureSingleton();
            return tzoneAllocateNonCompactSlow(requestedSize, spec);
        }

        RELEASE_BASSERT(tzoneMallocFallback == TZoneMallocFallback::ForceDebugMalloc);
        return api::malloc(requestedSize, CompactAllocationMode::NonCompact);
    }

    // Handle TZoneMallocFallback::DoNotFallBack.
    if (requestedSize != spec.size) [[unlikely]]
        heapRef = tzoneHeapManager->heapRefForTZoneTypeDifferentSize(requestedSize, spec);

    if (!heapRef) {
        heapRef = tzoneHeapManager->heapRefForTZoneType(spec);
        *spec.addressOfHeapRef = heapRef;
    }
    return bmalloc_iso_allocate_inline(TO_PAS_HEAPREF(heapRef), pas_non_compact_allocation_mode);
}

void* tzoneAllocateCompactSlow(size_t requestedSize, const TZoneSpecification& spec)
{
    HeapRef heapRef = *spec.addressOfHeapRef;
    if (tzoneMallocFallback != TZoneMallocFallback::DoNotFallBack) [[unlikely]] {
        if (tzoneMallocFallback == TZoneMallocFallback::Undecided) [[unlikely]] {
            TZoneHeapManager::ensureSingleton();
            return tzoneAllocateCompactSlow(requestedSize, spec);
        }

        RELEASE_BASSERT(tzoneMallocFallback == TZoneMallocFallback::ForceDebugMalloc);
        return api::malloc(requestedSize, CompactAllocationMode::Compact);
    }

    // Handle TZoneMallocFallback::DoNotFallBack.
    if (requestedSize != spec.size) [[unlikely]]
        heapRef = tzoneHeapManager->heapRefForTZoneTypeDifferentSize(requestedSize, spec);

    if (!heapRef) {
        heapRef = tzoneHeapManager->heapRefForTZoneType(spec);
        *spec.addressOfHeapRef = heapRef;
    }
    return bmalloc_iso_allocate_inline(TO_PAS_HEAPREF(heapRef), pas_always_compact_allocation_mode);
}

void* tzoneAllocateCompact(HeapRef heapRef)
{
    return bmalloc_iso_allocate_inline(TO_PAS_HEAPREF(heapRef), pas_always_compact_allocation_mode);
}

void* tzoneAllocateNonCompact(HeapRef heapRef)
{
    return bmalloc_iso_allocate_inline(TO_PAS_HEAPREF(heapRef), pas_non_compact_allocation_mode);
}

void tzoneFree(void* p)
{
    bmalloc_deallocate_inline(p);
}

#undef TO_PAS_HEAPREF

} } // namespace bmalloc::api

#endif // BUSE(TZONE)
