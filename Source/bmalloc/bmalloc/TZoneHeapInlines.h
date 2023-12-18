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

#include "BPlatform.h"
#include "DeferredDecommitInlines.h"
#include "DeferredTriggerInlines.h"
#include "EligibilityResultInlines.h"
#include "FreeListInlines.h"
#include "IsoAllocatorInlines.h"
#include "IsoDeallocatorInlines.h"
#include "IsoDirectoryInlines.h"
#include "IsoDirectoryPageInlines.h"
#include "IsoHeapImplInlines.h"
#include "IsoPageInlines.h"
#include "IsoTLSAllocatorEntryInlines.h"
#include "IsoTLSDeallocatorEntryInlines.h"
#include "IsoTLSEntryInlines.h"
#include "IsoTLSInlines.h"
#include "TZoneHeap.h"

namespace bmalloc { namespace api {

#if !BUSE(LIBPAS)

#if BENABLE_MALLOC_HEAP_BREAKDOWN
template<typename Type>
IsoHeap<Type>::TZoneHeap(const char* heapClass)
    : m_zone(malloc_create_zone(0, 0))
{
    if (heapClass)
        malloc_set_zone_name(m_zone, heapClass);
}
#endif

template<typename Type>
void* TZoneHeap<Type>::allocate()
{
    bool abortOnFailure = true;
    return IsoTLS::allocate(*this, abortOnFailure);
}

template<typename Type>
void* TZoneHeap<Type>::tryAllocate()
{
    bool abortOnFailure = false;
    return IsoTLS::allocate(*this, abortOnFailure);
}

template<typename Type>
void TZoneHeap<Type>::deallocate(void* p)
{
    IsoTLS::deallocate(*this, p);
}

template<typename Type>
void TZoneHeap<Type>::scavenge()
{
    IsoTLS::scavenge(*this);
}

template<typename Type>
bool TZoneHeap<Type>::isInitialized()
{
    auto* atomic = reinterpret_cast<std::atomic<IsoHeapImpl<Config>*>*>(&m_impl);
    return atomic->load(std::memory_order_acquire);
}

template<typename Type>
void TZoneHeap<Type>::initialize()
{
    // We are using m_impl field as a guard variable of the initialization of TZoneHeap.
    // TZoneHeap::isInitialized gets m_impl with "acquire", and TZoneHeap::initialize stores
    // the value to m_impl with "release". To make TZoneHeap changes visible to any threads
    // when TZoneHeap::isInitialized returns true, we need to store the value to m_impl *after*
    // all the initialization finishes.
    auto* heap = new IsoHeapImpl<Config>();
    heap->addToAllIsoHeaps();
    setAllocatorOffset(heap->allocatorOffset());
    setDeallocatorOffset(heap->deallocatorOffset());
    auto* atomic = reinterpret_cast<std::atomic<IsoHeapImpl<Config>*>*>(&m_impl);
    atomic->store(heap, std::memory_order_release);
}

template<typename Type>
auto TZoneHeap<Type>::impl() -> IsoHeapImpl<Config>&
{
    IsoTLS::ensureHeap(*this);
    return *m_impl;
}

#endif // !BUSE(LIBPAS)

// This is most appropraite for template classes.

#define MAKE_BTZONE_MALLOCED_INLINE(isoType) \
public: \
    static ::bmalloc::api::TZoneHeap<isoType>& btzoneHeap() \
    { \
        static ::bmalloc::api::TZoneHeap<isoType> heap("WebKit_"#isoType); \
        return heap; \
    } \
    \
    void* operator new(size_t, void* p) { return p; } \
    void* operator new[](size_t, void* p) { return p; } \
    \
    void* operator new(size_t size) \
    { \
        RELEASE_BASSERT(size == sizeof(isoType)); \
        return btzoneHeap().allocate(); \
    } \
    \
    void operator delete(void* p) \
    { \
        btzoneHeap().deallocate(p); \
    } \
    \
    void* operator new[](size_t size) = delete; \
    void operator delete[](void* p) = delete; \
    \
    void* operator new(size_t, NotNullTag, void* location) \
    { \
        ASSERT(location); \
        return location; \
    } \
    static void freeAfterDestruction(void* p) \
    { \
        btzoneHeap().deallocate(p); \
    } \
    \
    using webkitFastMalloced = int; \
private: \
    using __makeBtzoneMallocedInlineMacroSemicolonifier BunusedTypeAlias = int

#define MAKE_BTZONE_MALLOCED_IMPL(isoType) \
::bmalloc::api::TZoneHeap<isoType>& isoType::btzoneHeap() \
{ \
    static ::bmalloc::api::TZoneHeap<isoType> heap("WebKit "#isoType); \
    return heap; \
} \
\
void* isoType::operator new(size_t size) \
{ \
    RELEASE_BASSERT(size == sizeof(isoType)); \
    return btzoneHeap().allocate(); \
} \
\
void isoType::operator delete(void* p) \
{ \
    btzoneHeap().deallocate(p); \
} \
\
void isoType::freeAfterDestruction(void* p) \
{ \
    btzoneHeap().deallocate(p); \
} \
\
struct MakeBtzoneMallocedImplMacroSemicolonifier##isoType { }

#define MAKE_BTZONE_MALLOCED_IMPL_NESTED(isoTypeName, isoType) \
::bmalloc::api::TZoneHeap<isoType>& isoType::btzoneHeap() \
{ \
    static ::bmalloc::api::TZoneHeap<isoType> heap("WebKit "#isoType); \
    return heap; \
} \
\
void* isoType::operator new(size_t size) \
{ \
    RELEASE_BASSERT(size == sizeof(isoType)); \
    return btzoneHeap().allocate(); \
} \
\
void isoType::operator delete(void* p) \
{ \
    btzoneHeap().deallocate(p); \
} \
\
void isoType::freeAfterDestruction(void* p) \
{ \
    btzoneHeap().deallocate(p); \
} \
\
struct MakeBtzoneMallocedImplMacroSemicolonifier##isoTypeName { }

#define MAKE_BTZONE_MALLOCED_IMPL_TEMPLATE(isoType) \
template<> \
::bmalloc::api::TZoneHeap<isoType>& isoType::btzoneHeap() \
{ \
    static ::bmalloc::api::TZoneHeap<isoType> heap("WebKit_"#isoType); \
    return heap; \
} \
\
template<> \
void* isoType::operator new(size_t size) \
{ \
    RELEASE_BASSERT(size == sizeof(isoType)); \
    return btzoneHeap().allocate(); \
} \
\
template<> \
void isoType::operator delete(void* p) \
{ \
    btzoneHeap().deallocate(p); \
} \
\
template<> \
void isoType::freeAfterDestruction(void* p) \
{ \
    btzoneHeap().deallocate(p); \
} \
\
struct MakeBtzoneMallocedImplMacroSemicolonifier##isoType { }

#define MAKE_BTZONE_MALLOCED_IMPL_NESTED_TEMPLATE(isoTypeName, isoType) \
template<> \
::bmalloc::api::TZoneHeap<isoType>& isoType::btzoneHeap() \
{ \
    static ::bmalloc::api::TZoneHeap<isoType> heap("WebKit "#isoType); \
    return heap; \
} \
\
template<> \
void* isoType::operator new(size_t size) \
{ \
    RELEASE_BASSERT(size == sizeof(isoType)); \
    return btzoneHeap().allocate(); \
} \
\
template<> \
void isoType::operator delete(void* p) \
{ \
    btzoneHeap().deallocate(p); \
} \
\
template<> \
void isoType::freeAfterDestruction(void* p) \
{ \
    btzoneHeap().deallocate(p); \
} \
\
struct MakeBtzoneMallocedImplMacroSemicolonifier##isoTypeName { }

} } // namespace bmalloc::api

