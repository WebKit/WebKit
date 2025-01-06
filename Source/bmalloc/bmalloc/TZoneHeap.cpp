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

#include "TZoneHeap.h"

#if BUSE(TZONE)

#include "IsoHeap.h"
#include "IsoMallocFallback.h"
#include "TZoneHeapManager.h"
#include "bmalloc.h"
#include "bmalloc_heap_internal.h"
#include "bmalloc_heap_ref.h"

#if !BUSE(LIBPAS)
#error TZONE implementation requires LIBPAS
#endif

namespace bmalloc { namespace api {

BEXPORT void (*tzoneFreeWithFastFallback)(void*);
BEXPORT void (*tzoneFreeWithIsoFallback)(void*);

#define TO_PAS_HEAPREF(heapRef) std::bit_cast<pas_heap_ref*>(heapRef)

// HeapRef is an opaque alias for pas_heap_ref* in the underlying implementation.
static_assert(sizeof(HeapRef) == sizeof(pas_heap_ref*));

void* tzoneAllocateNonCompactWithFastFallbackSlow(size_t requestedSize, const TZoneSpecification& spec)
{
    HeapRef heapRef = *spec.addressOfHeapRef;
    if (BUNLIKELY(tzoneMallocFallback != TZoneMallocFallback::DoNotFallBack)) {
        if (tzoneMallocFallback == TZoneMallocFallback::ForceSpecifiedFallBack)
            return bmalloc_allocate_inline(requestedSize, pas_non_compact_allocation_mode);

        RELEASE_BASSERT(tzoneMallocFallback == TZoneMallocFallback::ForceDebugMalloc);
        IsoMallocFallback::MallocResult result = IsoMallocFallback::tryMalloc(requestedSize, CompactAllocationMode::NonCompact);
        BASSERT(result.didFallBack);
        return result.ptr;
    }

    // Handle TZoneMallocFallback::DoNotFallBack.
    if (BUNLIKELY(requestedSize != spec.size))
        heapRef = tzoneHeapManager->heapRefForTZoneTypeDifferentSize(requestedSize, spec);

    if (!heapRef) {
        heapRef = tzoneHeapManager->heapRefForTZoneType(spec);
        *spec.addressOfHeapRef = heapRef;
    }
    return bmalloc_iso_allocate_inline(TO_PAS_HEAPREF(heapRef), pas_non_compact_allocation_mode);
}

void* tzoneAllocateCompactWithFastFallbackSlow(size_t requestedSize, const TZoneSpecification& spec)
{
    HeapRef heapRef = *spec.addressOfHeapRef;
    if (BUNLIKELY(tzoneMallocFallback != TZoneMallocFallback::DoNotFallBack)) {
        if (BLIKELY(tzoneMallocFallback == TZoneMallocFallback::ForceSpecifiedFallBack))
            return bmalloc_allocate_inline(requestedSize, pas_maybe_compact_allocation_mode);

        RELEASE_BASSERT(tzoneMallocFallback == TZoneMallocFallback::ForceDebugMalloc);
        IsoMallocFallback::MallocResult result = IsoMallocFallback::tryMalloc(requestedSize, CompactAllocationMode::Compact);
        BASSERT(result.didFallBack);
        return result.ptr;
    }

    // Handle TZoneMallocFallback::DoNotFallBack.
    if (BUNLIKELY(requestedSize != spec.size))
        heapRef = tzoneHeapManager->heapRefForTZoneTypeDifferentSize(requestedSize, spec);

    if (!heapRef) {
        heapRef = tzoneHeapManager->heapRefForTZoneType(spec);
        *spec.addressOfHeapRef = heapRef;
    }
    return bmalloc_iso_allocate_inline(TO_PAS_HEAPREF(heapRef), pas_maybe_compact_allocation_mode);
}

void* tzoneAllocateNonCompactWithIsoFallbackSlow(size_t requestedSize, const TZoneSpecification& spec)
{
    HeapRef heapRef = *spec.addressOfHeapRef;
    if (BUNLIKELY(tzoneMallocFallback != TZoneMallocFallback::DoNotFallBack)) {
        if (BLIKELY(tzoneMallocFallback == TZoneMallocFallback::ForceSpecifiedFallBack)) {
            if (!heapRef) {
                heapRef = tzoneHeapManager->heapRefForIsoFallback(spec);
                *spec.addressOfHeapRef = heapRef;
            }
            return bmalloc_iso_allocate_inline(TO_PAS_HEAPREF(heapRef), pas_non_compact_allocation_mode);
        }

        RELEASE_BASSERT(tzoneMallocFallback == TZoneMallocFallback::ForceDebugMalloc);
        IsoMallocFallback::MallocResult result = IsoMallocFallback::tryMalloc(requestedSize, CompactAllocationMode::NonCompact);
        BASSERT(result.didFallBack);
        return result.ptr;
    }

    // Handle TZoneMallocFallback::DoNotFallBack.
    if (BUNLIKELY(requestedSize != spec.size))
        heapRef = tzoneHeapManager->heapRefForTZoneTypeDifferentSize(requestedSize, spec);

    if (!heapRef) {
        heapRef = tzoneHeapManager->heapRefForTZoneType(spec);
        *spec.addressOfHeapRef = heapRef;
    }
    return bmalloc_iso_allocate_inline(TO_PAS_HEAPREF(heapRef), pas_non_compact_allocation_mode);
}

void* tzoneAllocateCompactWithIsoFallbackSlow(size_t requestedSize, const TZoneSpecification& spec)
{
    HeapRef heapRef = *spec.addressOfHeapRef;
    if (BUNLIKELY(tzoneMallocFallback != TZoneMallocFallback::DoNotFallBack)) {
        if (BLIKELY(tzoneMallocFallback == TZoneMallocFallback::ForceSpecifiedFallBack)) {
            if (!heapRef) {
                heapRef = tzoneHeapManager->heapRefForIsoFallback(spec);
                *spec.addressOfHeapRef = heapRef;
            }
            return bmalloc_iso_allocate_inline(TO_PAS_HEAPREF(heapRef), pas_maybe_compact_allocation_mode);
        }

        RELEASE_BASSERT(tzoneMallocFallback == TZoneMallocFallback::ForceDebugMalloc);
        IsoMallocFallback::MallocResult result = IsoMallocFallback::tryMalloc(requestedSize, CompactAllocationMode::Compact);
        BASSERT(result.didFallBack);
        return result.ptr;
    }

    // Handle TZoneMallocFallback::DoNotFallBack.
    if (BUNLIKELY(requestedSize != spec.size))
        heapRef = tzoneHeapManager->heapRefForTZoneTypeDifferentSize(requestedSize, spec);

    if (!heapRef) {
        heapRef = tzoneHeapManager->heapRefForTZoneType(spec);
        *spec.addressOfHeapRef = heapRef;
    }
    return bmalloc_iso_allocate_inline(TO_PAS_HEAPREF(heapRef), pas_maybe_compact_allocation_mode);
}

void* tzoneAllocateCompact(HeapRef heapRef)
{
    return bmalloc_iso_allocate_inline(TO_PAS_HEAPREF(heapRef), pas_maybe_compact_allocation_mode);
}

void* tzoneAllocateNonCompact(HeapRef heapRef)
{
    return bmalloc_iso_allocate_inline(TO_PAS_HEAPREF(heapRef), pas_non_compact_allocation_mode);
}

void tzoneFreeFast(void* p)
{
    bmalloc_deallocate_inline(p);
}

void tzoneFreeWithDebugMalloc(void* p)
{
    RELEASE_BASSERT(tzoneMallocFallback == TZoneMallocFallback::ForceDebugMalloc);
    IsoMallocFallback::tryFree(p);
}

#if BUSE_TZONE_PREINITIALIZATION
void tzonePreInitializeHeapRefs(const TZoneSpecification* start, const TZoneSpecification* end)
{
    if (tzoneMallocFallback == TZoneMallocFallback::Undecided)
        determineTZoneMallocFallback();
    if (tzoneMallocFallback == TZoneMallocFallback::DoNotFallBack) {
        if (!tzoneHeapManager)
            TZoneHeapManager::ensureSingleton();
        tzoneHeapManager->preInitializeHeapRefs(start, end);
    }
}
#endif // BUSE_TZONE_PREINITIALIZATION

#undef TO_PAS_HEAPREF

} } // namespace bmalloc::api

#endif // BUSE(TZONE)
