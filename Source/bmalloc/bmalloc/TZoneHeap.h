/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#pragma once

#include "IsoConfig.h"
#include "Mutex.h"

#if BUSE(LIBPAS)
#include "bmalloc_heap_ref.h"
#endif

#if BENABLE_MALLOC_HEAP_BREAKDOWN
#include <malloc/malloc.h>
#endif

namespace bmalloc {

template<typename Config> class IsoHeapImpl;

namespace api {

// You have to declare TZoneHeaps this way:
//
// static TZoneHeap<type> myTypeHeap;
//
// It's not valid to create an TZoneHeap except in static storage.

#if BUSE(LIBPAS)
BEXPORT void* tzoneAllocate(pas_heap_ref&);
BEXPORT void* tzoneTryAllocate(pas_heap_ref&);
BEXPORT void tzoneDeallocate(void* ptr);

// The name "LibPasBmallocHeapType" is important for the pas_status_reporter to work right.
template<typename LibPasBmallocHeapType>
struct TZoneHeap {
    constexpr TZoneHeap(const char* = nullptr) { }

    void* allocate()
    {
        return tzoneAllocate(provideHeap());
    }

    void* tryAllocate()
    {
        return tzoneTryAllocate(provideHeap());
    }

    void deallocate(void* p)
    {
        tzoneDeallocate(p);
    }

    void scavenge()
    {
    }

    void initialize()
    {
    }

    bool isInitialized()
    {
        return true;
    }

    static pas_heap_ref& provideHeap()
    {
        static const bmalloc_type type = BMALLOC_TYPE_INITIALIZER(sizeof(LibPasBmallocHeapType), alignof(LibPasBmallocHeapType), __PRETTY_FUNCTION__);
        static pas_heap_ref heap = BMALLOC_HEAP_REF_INITIALIZER(&type);
        return heap;
    }
};
#else // BUSE(LIBPAS) -> so !BUSE(LIBPAS)
template<typename Type>
struct TZoneHeap {
    typedef IsoConfig<sizeof(Type)> Config;

#if BENABLE_MALLOC_HEAP_BREAKDOWN
    TZoneHeap(const char* = nullptr);
#else
    constexpr TZoneHeap(const char* = nullptr) { }
#endif

    void* allocate();
    void* tryAllocate();
    void deallocate(void* p);

    void scavenge();

    void initialize();
    bool isInitialized();

    unsigned allocatorOffset() { return m_allocatorOffsetPlusOne - 1; }
    void setAllocatorOffset(unsigned value) { m_allocatorOffsetPlusOne = value + 1; }

    unsigned deallocatorOffset() { return m_deallocatorOffsetPlusOne - 1; }
    void setDeallocatorOffset(unsigned value) { m_deallocatorOffsetPlusOne = value + 1; }

    IsoHeapImpl<Config>& impl();

    Mutex m_initializationLock;
    unsigned m_allocatorOffsetPlusOne { 0 };
    unsigned m_deallocatorOffsetPlusOne { 0 };
    IsoHeapImpl<Config>* m_impl { nullptr };

#if BENABLE_MALLOC_HEAP_BREAKDOWN
    malloc_zone_t* m_zone;
#endif
};
#endif // BUSE(LIBPAS) -> so end of !BUSE(LIBPAS)

// Use this together with MAKE_BISO_MALLOCED_IMPL.
#define MAKE_BTZONE_MALLOCED(isoType, exportMacro) \
public: \
    static exportMacro ::bmalloc::api::TZoneHeap<isoType>& btzoneHeap(); \
    \
    void* operator new(size_t, void* p) { return p; } \
    void* operator new[](size_t, void* p) { return p; } \
    \
    exportMacro void* operator new(size_t size);\
    exportMacro void operator delete(void* p);\
    \
    void* operator new[](size_t size) = delete; \
    void operator delete[](void* p) = delete; \
    \
    void* operator new(size_t, NotNullTag, void* location) \
    { \
        ASSERT(location); \
        return location; \
    } \
    exportMacro static void freeAfterDestruction(void*); \
    \
    using webkitFastMalloced = int; \
private: \
    using __makeTZoneMallocedMacroSemicolonifier BunusedTypeAlias = int

} } // namespace bmalloc::api
