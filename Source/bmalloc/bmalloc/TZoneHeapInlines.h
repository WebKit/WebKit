/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if BUSE(TZONE)

#include "TZoneHeap.h"

namespace bmalloc { namespace api {

// This is most appropriate for classes defined in a .cpp/.mm file.

#define MAKE_BTZONE_MALLOCED_COMMON_INLINE(_type, _compactMode, _exportMacro) \
public: \
    using HeapRef = ::bmalloc::api::HeapRef; \
    using SizeAndAlignment = ::bmalloc::api::SizeAndAlignment; \
    using TZoneMallocFallback = ::bmalloc::api::TZoneMallocFallback; \
    using CompactAllocationMode = ::bmalloc::CompactAllocationMode; \
    \
    BINLINE void* operator new(size_t, void* p) { return p; } \
    BINLINE void* operator new[](size_t, void* p) { return p; } \
    \
    void* operator new[](size_t size) = delete; \
    void operator delete[](void* p) = delete; \
    \
    BINLINE void* operator new(size_t, NotNullTag, void* location) \
    { \
        ASSERT(location); \
        return location; \
    } \
    \
    void* operator new(size_t size) \
    { \
        static HeapRef s_heapRef; \
        static const TZoneSpecification s_heapSpec = { &s_heapRef, sizeof(_type), ::bmalloc::api::compactAllocationMode<_type>(), SizeAndAlignment::encode<_type>() TZONE_SPEC_NAME_ARG(#_type) TZONE_DYNAMIC_COMPACTION_ARG(_type) }; \
    \
        if (!s_heapRef || size != sizeof(_type)) { [[unlikely]] \
            if constexpr (::bmalloc::api::compactAllocationMode<_type>() == CompactAllocationMode::Compact) \
                return ::bmalloc::api::tzoneAllocateCompactSlow(size, s_heapSpec); \
            return ::bmalloc::api::tzoneAllocate ## _compactMode ## Slow(size, s_heapSpec); \
        } \
        BASSERT(::bmalloc::api::tzoneMallocFallback > TZoneMallocFallback::ForceDebugMalloc); \
        if constexpr (::bmalloc::api::compactAllocationMode<_type>() == CompactAllocationMode::Compact) \
            return ::bmalloc::api::tzoneAllocateCompact(s_heapRef); \
        if (::bmalloc::api::shouldDynamicallyCompact(s_heapSpec)) \
            return ::bmalloc::api::tzoneAllocateCompact(s_heapRef); \
        return ::bmalloc::api::tzoneAllocate ## _compactMode(s_heapRef); \
    } \
    \
    BINLINE void operator delete(void* p) \
    { \
        ::bmalloc::api::tzoneFree(p); \
    } \
    \
    BINLINE static void freeAfterDestruction(void* p) \
    { \
        ::bmalloc::api::tzoneFree(p); \
    } \
    \
    using WTFIsFastMallocAllocated = int;

#define MAKE_BTZONE_MALLOCED_INLINE(_type, _compactMode) \
public: \
    MAKE_BTZONE_MALLOCED_COMMON_INLINE(_type, _compactMode, BNOEXPORT) \
private: \
    using __makeBtzoneMallocedInlineMacroSemicolonifier BUNUSED_TYPE_ALIAS = int


#define MAKE_BTZONE_MALLOCED_IMPL(_type, _compactMode) \
::bmalloc::api::HeapRef _type::s_heapRef; \
\
const TZoneSpecification _type::s_heapSpec = { &_type::s_heapRef, sizeof(_type), ::bmalloc::api::compactAllocationMode<_type>(), SizeAndAlignment::encode<_type>() TZONE_SPEC_NAME_ARG(#_type) TZONE_DYNAMIC_COMPACTION_ARG(_type) }; \
\
void* _type::operatorNewSlow(size_t size) \
{ \
    if constexpr (::bmalloc::api::compactAllocationMode<_type>() == CompactAllocationMode::Compact) \
        return ::bmalloc::api::tzoneAllocateCompactSlow(size, s_heapSpec); \
    if (::bmalloc::api::shouldDynamicallyCompact(s_heapSpec)) \
        return ::bmalloc::api::tzoneAllocateCompactSlow(size, s_heapSpec); \
    return ::bmalloc::api::tzoneAllocate ## _compactMode ## Slow(size, s_heapSpec); \
} \
\
using __makeBtzoneMallocedInlineMacroSemicolonifier BUNUSED_TYPE_ALIAS = int

} } // namespace bmalloc::api

#endif // BUSE(TZONE)
