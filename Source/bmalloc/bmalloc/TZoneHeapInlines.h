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

#if BUSE(TZONE)

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
TZoneHeapBase<Type>::TZoneHeapBase(const char* heapClass)
    : m_zone(malloc_create_zone(0, 0))
{
    if (heapClass)
        malloc_set_zone_name(m_zone, heapClass);
}
#endif

template<typename Type>
void* TZoneHeapBase<Type>::allocate()
{
    bool abortOnFailure = true;
    return IsoTLS::allocate(*this, abortOnFailure);
}

template<typename Type>
void* TZoneHeapBase<Type>::tryAllocate()
{
    bool abortOnFailure = false;
    return IsoTLS::allocate(*this, abortOnFailure);
}

template<typename Type>
void TZoneHeapBase<Type>::deallocate(void* p)
{
    IsoTLS::deallocate(*this, p);
}

template<typename Type>
void TZoneHeapBase<Type>::scavenge()
{
    IsoTLS::scavenge(*this);
}

template<typename Type>
bool TZoneHeapBase<Type>::isInitialized()
{
    auto* atomic = reinterpret_cast<std::atomic<IsoHeapImpl<Config>*>*>(&m_impl);
    return atomic->load(std::memory_order_acquire);
}

template<typename Type>
void TZoneHeapBase<Type>::initialize()
{
    // We are using m_impl field as a guard variable of the initialization of TZoneHeapBase.
    // TZoneHeapBase::isInitialized gets m_impl with "acquire", and TZoneHeapBase::initialize stores
    // the value to m_impl with "release". To make TZoneHeapBase changes visible to any threads
    // when TZoneHeapBase::isInitialized returns true, we need to store the value to m_impl *after*
    // all the initialization finishes.
    auto* heap = new IsoHeapImpl<Config>();
    heap->addToAllIsoHeaps();
    setAllocatorOffset(heap->allocatorOffset());
    setDeallocatorOffset(heap->deallocatorOffset());
    auto* atomic = reinterpret_cast<std::atomic<IsoHeapImpl<Config>*>*>(&m_impl);
    atomic->store(heap, std::memory_order_release);
}

template<typename Type>
auto TZoneHeapBase<Type>::impl() -> IsoHeapImpl<Config>&
{
    IsoTLS::ensureHeap(*this);
    return *m_impl;
}

#endif // !BUSE(LIBPAS)

// This is most appropraite for template classes.

#define MAKE_BTZONE_MALLOCED_INLINE(tzoneType, tzoneHeapType) \
public: \
    static ::bmalloc::api::tzoneHeapType<tzoneType>& btzoneHeap() \
    { \
        static ::bmalloc::api::tzoneHeapType<tzoneType> heap("WebKit_"#tzoneType); \
        return heap; \
    } \
    \
    void* operator new(size_t, void* p) { return p; } \
    void* operator new[](size_t, void* p) { return p; } \
    \
    void* operator new(size_t size) \
    { \
        if (size == sizeof(tzoneType)) \
            return btzoneHeap().allocate(); \
        return btzoneHeap().allocate(size); \
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
    using WTFIsFastAllocated = int; \
private: \
    using __makeBtzoneMallocedInlineMacroSemicolonifier BUNUSED_TYPE_ALIAS = int

#define MAKE_BTZONE_MALLOCED_IMPL(tzoneType, tzoneHeapType) \
::bmalloc::api::tzoneHeapType<tzoneType>& tzoneType::btzoneHeap() \
{ \
    static ::bmalloc::api::tzoneHeapType<tzoneType> heap("WebKit "#tzoneType); \
    return heap; \
} \
\
void* tzoneType::operator new(size_t size) \
{ \
    if (size == sizeof(tzoneType)) \
        return btzoneHeap().allocate(); \
    return btzoneHeap().allocate(size); \
} \
\
void tzoneType::operator delete(void* p) \
{ \
    btzoneHeap().deallocate(p); \
} \
\
void tzoneType::freeAfterDestruction(void* p) \
{ \
    btzoneHeap().deallocate(p); \
} \
\
struct MakeBtzoneMallocedImplMacroSemicolonifier##tzoneType { }

#define MAKE_BTZONE_MALLOCED_IMPL_NESTED(tzoneTypeName, tzoneType, tzoneHeapType) \
::bmalloc::api::tzoneHeapType<tzoneType>& tzoneType::btzoneHeap() \
{ \
    static ::bmalloc::api::tzoneHeapType<tzoneType> heap("WebKit "#tzoneType); \
    return heap; \
} \
\
void* tzoneType::operator new(size_t size) \
{ \
    if (size == sizeof(tzoneType)) \
        return btzoneHeap().allocate(); \
    return btzoneHeap().allocate(size); \
} \
\
void tzoneType::operator delete(void* p) \
{ \
    btzoneHeap().deallocate(p); \
} \
\
void tzoneType::freeAfterDestruction(void* p) \
{ \
    btzoneHeap().deallocate(p); \
} \
\
struct MakeBtzoneMallocedImplMacroSemicolonifier##tzoneTypeName { }

#define MAKE_BTZONE_MALLOCED_IMPL_TEMPLATE(tzoneType, tzoneHeapType) \
template<> \
::bmalloc::api::tzoneHeapType<tzoneType>& tzoneType::btzoneHeap() \
{ \
    static ::bmalloc::api::tzoneHeapType<tzoneType> heap("WebKit_"#tzoneType); \
    return heap; \
} \
\
template<> \
void* tzoneType::operator new(size_t size) \
{ \
    if (size == sizeof(tzoneType)) \
        return btzoneHeap().allocate(); \
    return btzoneHeap().allocate(size); \
} \
\
template<> \
void tzoneType::operator delete(void* p) \
{ \
    btzoneHeap().deallocate(p); \
} \
\
template<> \
void tzoneType::freeAfterDestruction(void* p) \
{ \
    btzoneHeap().deallocate(p); \
} \
\
struct MakeBtzoneMallocedImplMacroSemicolonifier##tzoneType { }

#define MAKE_BTZONE_MALLOCED_IMPL_NESTED_TEMPLATE(tzoneTypeName, tzoneType, tzoneHeapType) \
template<> \
::bmalloc::api::tzoneHeapType<tzoneType>& tzoneType::btzoneHeap() \
{ \
    static ::bmalloc::api::tzoneHeapType<tzoneType> heap("WebKit "#tzoneType); \
    return heap; \
} \
\
template<> \
void* tzoneType::operator new(size_t size) \
{ \
    if (size == sizeof(tzoneType)) \
        return btzoneHeap().allocate(); \
    return btzoneHeap().allocate(size); \
} \
\
template<> \
void tzoneType::operator delete(void* p) \
{ \
    btzoneHeap().deallocate(p); \
} \
\
template<> \
void tzoneType::freeAfterDestruction(void* p) \
{ \
    btzoneHeap().deallocate(p); \
} \
\
struct MakeBtzoneMallocedImplMacroSemicolonifier##tzoneTypeName { }

} } // namespace bmalloc::api

#endif // BUSE(TZONE)
