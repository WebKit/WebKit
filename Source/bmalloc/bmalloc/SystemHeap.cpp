/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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

#include "SystemHeap.h"

#include "Algorithm.h"
#include "BAssert.h"
#include "BPlatform.h"
#include "VMAllocate.h"
#include <cstdlib>
#include <thread>

#if BENABLE(LIBPAS)
#include "pas_mte.h"
#include "pas_system_heap.h"
#endif

namespace bmalloc {

SystemHeap* systemHeapCache { nullptr };

DEFINE_STATIC_PER_PROCESS_STORAGE(SystemHeap);

#if BENABLE(MALLOC_HEAP_BREAKDOWN) || BOS(DARWIN)

static bool shouldUseDefaultMallocZone()
{
    if (getenv("SYSTEM_HEAP_USE_DEFAULT_ZONE"))
        return true;

    // The lite logging mode only intercepts allocations from the default zone.
    const char* mallocStackLogging = getenv("MallocStackLogging");
    if (mallocStackLogging && !strcmp(mallocStackLogging, "lite"))
        return true;

    return false;
}

SystemHeap::SystemHeap(const LockHolder&)
    : m_zone(malloc_default_zone())
    , m_pageSize(vmPageSize())
{
    if (!shouldUseDefaultMallocZone()) {
        m_zone = malloc_create_zone(0, 0);
        malloc_set_zone_name(m_zone, "WebKit Using System Malloc");
    }
}

void* SystemHeap::malloc(size_t size, FailureAction action)
{
    void* result = malloc_zone_malloc(m_zone, size);
    RELEASE_BASSERT(action == FailureAction::ReturnNull || result);
    return result;
}

void* SystemHeap::memalign(size_t alignment, size_t size, FailureAction action)
{
    void* result = malloc_zone_memalign(m_zone, alignment, size);
    RELEASE_BASSERT(action == FailureAction::ReturnNull || result);
    return result;
}

void* SystemHeap::realloc(void* object, size_t size, FailureAction action)
{
    void* result = malloc_zone_realloc(m_zone, object, size);
    RELEASE_BASSERT(action == FailureAction::ReturnNull || result);
    return result;
}

void SystemHeap::free(void* object)
{
    malloc_zone_free(m_zone, object);
}

void SystemHeap::scavenge()
{
    // Currently |goal| does not affect on the behavior of malloc_zone_pressure_relief if (1) we only scavenge one zone and (2) it is not nanomalloc.
    constexpr size_t goal = 0;
    malloc_zone_pressure_relief(m_zone, goal);
}

void SystemHeap::dump()
{
    constexpr bool verbose = true;
    malloc_zone_print(m_zone, verbose);
}

#elif BOS(WINDOWS)

// SystemHeap unimplemented on Windows
// This might be possible with _aligned_malloc, _aligned_realloc and _aligned_free, however there may be an impedence mismatch compared to Linux's APIs.
// For example, it might attempt to free larger contiguous regions with a single free which might fail.

SystemHeap::SystemHeap(const LockHolder&)
    : m_pageSize(vmPageSize())
{
}

void* SystemHeap::malloc(size_t size, FailureAction action)
{
    BUNUSED_PARAM(size);
    BUNUSED_PARAM(action);
    RELEASE_BASSERT_NOT_REACHED();
    return nullptr;
}

void* SystemHeap::memalign(size_t alignment, size_t size, FailureAction action)
{
    BUNUSED_PARAM(alignment);
    BUNUSED_PARAM(size);
    BUNUSED_PARAM(action);
    RELEASE_BASSERT_NOT_REACHED();
    return nullptr;
}

void* SystemHeap::realloc(void* object, size_t size, FailureAction action)
{
    BUNUSED_PARAM(object);
    BUNUSED_PARAM(size);
    BUNUSED_PARAM(action);
    RELEASE_BASSERT_NOT_REACHED();
    return nullptr;
}

void SystemHeap::free(void* object)
{
    BUNUSED_PARAM(object);
    RELEASE_BASSERT_NOT_REACHED();
}

void SystemHeap::scavenge()
{
}

void SystemHeap::dump()
{
}

#else

SystemHeap::SystemHeap(const LockHolder&)
    : m_pageSize(vmPageSize())
{
}

void* SystemHeap::malloc(size_t size, FailureAction action)
{
    void* result = ::malloc(size);
    RELEASE_BASSERT(action == FailureAction::ReturnNull || result);
    return result;
}

void* SystemHeap::memalign(size_t alignment, size_t size, FailureAction action)
{
    void* result = ::aligned_alloc(alignment, size);
    RELEASE_BASSERT(action == FailureAction::ReturnNull || result);
    return result;
}

void* SystemHeap::realloc(void* object, size_t size, FailureAction action)
{
    void* result = ::realloc(object, size);
    RELEASE_BASSERT(action == FailureAction::ReturnNull || result);
    return result;
}

void SystemHeap::free(void* object)
{
    ::free(object);
}

void SystemHeap::scavenge()
{
}

void SystemHeap::dump()
{
}

#endif

// FIXME: This looks an awful lot like the code in wtf/Gigacage.cpp for large allocation.
// https://bugs.webkit.org/show_bug.cgi?id=175086

void* SystemHeap::memalignLarge(size_t alignment, size_t size)
{
    alignment = roundUpToMultipleOf(m_pageSize, alignment);
    size = roundUpToMultipleOf(m_pageSize, size);
    void* result = tryVMAllocate(alignment, size);
    if (!result)
        return nullptr;
    {
        LockHolder locker(mutex());
        m_sizeMap[result] = size;
    }
    return result;
}

void SystemHeap::freeLarge(void* base)
{
    if (!base)
        return;

    size_t size;
    {
        LockHolder locker(mutex());
        size = m_sizeMap[base];
        size_t numErased = m_sizeMap.erase(base);
        RELEASE_BASSERT(numErased == 1);
    }

    vmDeallocate(base, size);
}

SystemHeap* SystemHeap::tryGetSlow()
{
    SystemHeap* result;
    if (Environment::get()->isSystemHeapEnabled()) {
        systemHeapCache = SystemHeap::get();
        result = systemHeapCache;
    } else {
        systemHeapCache = systemHeapDisabled();
        result = nullptr;
    }
    RELEASE_BASSERT(systemHeapCache);
    return result;
}

} // namespace bmalloc

#if BENABLE(LIBPAS)

#if BUSE(LIBPAS)

using namespace bmalloc;

bool pas_system_heap_is_enabled(pas_heap_config_kind kind)
{
    switch (kind) {
    case pas_heap_config_kind_bmalloc:
        return !!SystemHeap::tryGet();
    case pas_heap_config_kind_jit:
    case pas_heap_config_kind_pas_utility:
        return false;
    default:
        BCRASH();
        return false;
    }
}

void* pas_system_heap_malloc(size_t size)
{
    auto systemHeap = SystemHeap::getExisting();
    PAS_PROFILE(SYSTEM_HEAP_ALLOCATION, systemHeap, size, 0, pas_non_compact_allocation_mode);
    PAS_MTE_HANDLE(SYSTEM_HEAP_ALLOCATION, systemHeap, size, 0, pas_non_compact_allocation_mode);
    return systemHeap->malloc(size, FailureAction::ReturnNull);
}

void* pas_system_heap_memalign(size_t alignment, size_t size)
{
    auto systemHeap = SystemHeap::getExisting();
    PAS_PROFILE(SYSTEM_HEAP_ALLOCATION, systemHeap, size, alignment, pas_non_compact_allocation_mode);
    PAS_MTE_HANDLE(SYSTEM_HEAP_ALLOCATION, systemHeap, size, alignment, pas_non_compact_allocation_mode);
    return systemHeap->memalign(alignment, size, FailureAction::ReturnNull);
}

void* pas_system_heap_realloc(void* ptr, size_t size)
{
    auto systemHeap = SystemHeap::getExisting();
    PAS_PROFILE(SYSTEM_HEAP_REALLOCATION, systemHeap, ptr, size, pas_non_compact_allocation_mode);
    PAS_MTE_HANDLE(SYSTEM_HEAP_REALLOCATION, systemHeap, ptr, size, pas_non_compact_allocation_mode);
    return systemHeap->realloc(ptr, size, FailureAction::ReturnNull);
}

void* pas_system_heap_malloc_compact(size_t size)
{
    auto systemHeap = SystemHeap::getExisting();
    PAS_PROFILE(SYSTEM_HEAP_ALLOCATION, systemHeap, size, 0, pas_always_compact_allocation_mode);
    PAS_MTE_HANDLE(SYSTEM_HEAP_ALLOCATION, systemHeap, size, 0, pas_always_compact_allocation_mode);
    return systemHeap->malloc(size, FailureAction::ReturnNull);
}

void* pas_system_heap_memalign_compact(size_t alignment, size_t size)
{
    auto systemHeap = SystemHeap::getExisting();
    PAS_PROFILE(SYSTEM_HEAP_ALLOCATION, systemHeap, size, alignment, pas_always_compact_allocation_mode);
    PAS_MTE_HANDLE(SYSTEM_HEAP_ALLOCATION, systemHeap, size, alignment, pas_always_compact_allocation_mode);
    return systemHeap->memalign(alignment, size, FailureAction::ReturnNull);
}

void* pas_system_heap_realloc_compact(void* ptr, size_t size)
{
    auto systemHeap = SystemHeap::getExisting();
    PAS_PROFILE(SYSTEM_HEAP_REALLOCATION, systemHeap, ptr, size, pas_always_compact_allocation_mode);
    PAS_MTE_HANDLE(SYSTEM_HEAP_REALLOCATION, systemHeap, ptr, size, pas_always_compact_allocation_mode);
    return systemHeap->realloc(ptr, size, FailureAction::ReturnNull);
}

void pas_system_heap_free(void* ptr)
{
    SystemHeap::getExisting()->free(ptr);
}

#else // BUSE(LIBPAS) -> so !BUSE(LIBPAS)

bool pas_system_heap_is_enabled(pas_heap_config_kind kind)
{
    BUNUSED_PARAM(kind);
    return false;
}

void* pas_system_heap_malloc(size_t size)
{
    BUNUSED_PARAM(size);
    RELEASE_BASSERT_NOT_REACHED();
    return nullptr;
}

void* pas_system_heap_memalign(size_t alignment, size_t size)
{
    BUNUSED_PARAM(size);
    BUNUSED_PARAM(alignment);
    RELEASE_BASSERT_NOT_REACHED();
    return nullptr;
}

void* pas_system_heap_realloc(void* ptr, size_t size)
{
    BUNUSED_PARAM(ptr);
    BUNUSED_PARAM(size);
    RELEASE_BASSERT_NOT_REACHED();
    return nullptr;
}

void* pas_system_heap_malloc_compact(size_t size)
{
    BUNUSED_PARAM(size);
    RELEASE_BASSERT_NOT_REACHED();
    return nullptr;
}

void* pas_system_heap_memalign_compact(size_t alignment, size_t size)
{
    BUNUSED_PARAM(size);
    BUNUSED_PARAM(alignment);
    RELEASE_BASSERT_NOT_REACHED();
    return nullptr;
}

void* pas_system_heap_realloc_compact(void* ptr, size_t size)
{
    BUNUSED_PARAM(ptr);
    BUNUSED_PARAM(size);
    RELEASE_BASSERT_NOT_REACHED();
    return nullptr;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
void pas_system_heap_free(void* ptr)
{
    BUNUSED_PARAM(ptr);
    RELEASE_BASSERT_NOT_REACHED();
}
#pragma clang diagnostic pop

#endif // BUSE(LIBPAS) -> so end of !BUSE(LIBPAS)

#endif // BENABLE(LIBPAS)

