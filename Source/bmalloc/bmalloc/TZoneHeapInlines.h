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

#include "BPlatform.h"

#if BUSE(TZONE)

#include "TZoneHeap.h"

namespace bmalloc { namespace api {

// This is most appropriate for classes defined in a .cpp/.mm file.

#define MAKE_BTZONE_MALLOCED_COMMON_INLINE(_type, _compactMode, _fallbackMode, _exportMacro) \
public: \
    using HeapRef = ::bmalloc::api::HeapRef; \
    using SizeAndAlignment = ::bmalloc::api::SizeAndAlignment; \
    using TZoneMallocFallback = ::bmalloc::api::TZoneMallocFallback; \
private: \
    static BNO_INLINE void* operatorNewSlow(size_t size) \
    { \
        const auto& heapSpec = tzoneHeap().second; \
        return ::bmalloc::api::tzoneAllocate ## _compactMode ## With ## _fallbackMode ## Slow(size, heapSpec); \
    } \
    \
    static _exportMacro std::pair<HeapRef&, const TZoneSpecification&> tzoneHeap() \
    { \
        static HeapRef s_heapRef; \
        static const TZoneSpecification s_heapSpec TZONE_SPEC_ATTRIBUTE = { &s_heapRef, sizeof(_type), SizeAndAlignment::encode<_type>() TZONE_SPEC_NAME_ARG(#_type) }; \
        return { s_heapRef, s_heapSpec }; \
    } \
    \
public: \
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
        auto heapRef = tzoneHeap().first; \
        if (BUNLIKELY(!heapRef || size != sizeof(_type))) \
            BMUST_TAIL_CALL return operatorNewSlow(size); \
        BASSERT(::bmalloc::api::tzoneMallocFallback >= TZoneMallocFallback::ForceSpecifiedFallBack); \
        return ::bmalloc::api::tzoneAllocate ## _compactMode(heapRef); \
    } \
    \
    BINLINE void operator delete(void* p) \
    { \
        ::bmalloc::api::tzoneFreeWith ## _fallbackMode(p); \
    } \
    \
    BINLINE static void freeAfterDestruction(void* p) \
    { \
        ::bmalloc::api::tzoneFreeWith ## _fallbackMode(p); \
    } \
    \
    using WTFIsFastAllocated = int;

#define MAKE_BTZONE_MALLOCED_INLINE(_type, _compactMode) \
public: \
    MAKE_BTZONE_MALLOCED_COMMON_INLINE(_type, _compactMode, FastFallback, BNOEXPORT) \
private: \
    using __makeBtzoneMallocedInlineMacroSemicolonifier BUNUSED_TYPE_ALIAS = int


#define MAKE_BTZONE_MALLOCED_IMPL(_type, _compactMode, _fallbackMode) \
::bmalloc::api::HeapRef _type::s_heapRef; \
\
const TZoneSpecification _type::s_heapSpec TZONE_SPEC_ATTRIBUTE = { &_type::s_heapRef, sizeof(_type), SizeAndAlignment::encode<_type>() TZONE_SPEC_NAME_ARG(#_type) }; \
\
void* _type::operatorNewSlow(size_t size) \
{ \
    return ::bmalloc::api::tzoneAllocate ## _compactMode ## With ## _fallbackMode ## Slow(size, s_heapSpec); \
} \
\
using __makeBtzoneMallocedInlineMacroSemicolonifier BUNUSED_TYPE_ALIAS = int

} } // namespace bmalloc::api

#endif // BUSE(TZONE)
