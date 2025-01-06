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

#pragma once

#include "BPlatform.h"

#if BUSE(TZONE)

#if !BUSE(LIBPAS)
#error TZONE implementation requires LIBPAS
#endif

#include "Algorithm.h"
#include "BInline.h"

#if BCOMPILER(CLANG)
#define BUSE_TZONE_PREINITIALIZATION 1
#else
#define BUSE_TZONE_PREINITIALIZATION 0
#endif

#define BUSE_TZONE_SPEC_NAME_ARG 0
#if BUSE_TZONE_SPEC_NAME_ARG
#define TZONE_SPEC_NAME_ARG(x)  , x
#else
#define TZONE_SPEC_NAME_ARG(x)
#endif

namespace bmalloc {

namespace api {

enum class TZoneMallocFallback : uint8_t {
    Undecided,
    ForceDebugMalloc,
    ForceSpecifiedFallBack,
    DoNotFallBack
};

extern BEXPORT TZoneMallocFallback tzoneMallocFallback;

enum class TZoneAllocationFallback : uint8_t { FastFallback, IsoFallback };

using HeapRef = void*;

static constexpr size_t sizeClassFor(size_t size)
{
    constexpr unsigned tzoneSmallSizeThreshold = 512;
    constexpr double tzoneMidSizeGrowthRate = 1.05;
    constexpr unsigned tzoneMidSizeThreshold = 7872;
    constexpr double tzoneLargeSizeGrowthRate = 1.3;

    if (size <= tzoneSmallSizeThreshold)
        return roundUpToMultipleOf<16>(size);
    double nextSize = tzoneSmallSizeThreshold;
    size_t previousRoundedNextSize = 0;
    size_t roundedNextSize = tzoneSmallSizeThreshold;
    do {
        previousRoundedNextSize = roundedNextSize;
        nextSize = nextSize * tzoneMidSizeGrowthRate;
        roundedNextSize = roundUpToMultipleOf<16>(nextSize);
        if (size < previousRoundedNextSize)
            continue;
        if (size <= roundedNextSize)
            return roundedNextSize;
    } while (roundedNextSize < tzoneMidSizeThreshold);
    do {
        previousRoundedNextSize = roundedNextSize;
        nextSize = nextSize * tzoneLargeSizeGrowthRate;
        roundedNextSize = roundUpToMultipleOf<16>(nextSize);
        if (size < previousRoundedNextSize)
            continue;
    } while (size > roundedNextSize);
    return roundedNextSize;
}

struct SizeAndAlignment {
    using Value = uint64_t;

    static constexpr Value encode(unsigned size, unsigned alignment)
    {
        return (static_cast<uint64_t>(alignment) << 32) | size;
    }

    template<typename T>
    static constexpr Value encode()
    {
        size_t size = roundUpToMultipleOf<16>(::bmalloc::api::sizeClassFor(sizeof(T)));
        size_t alignment = roundUpToMultipleOf<16>(alignof(T));
        return encode(size, alignment);
    }

    static unsigned decodeSize(Value value) { return value; }
    static unsigned decodeAlignment(Value value) { return value >> 32; }

    static constexpr unsigned long hash(Value value)
    {
        return (decodeSize(value) ^ decodeAlignment(value)) >> 3;
    }
};

struct TZoneSpecification {
    HeapRef* addressOfHeapRef;
    unsigned size;
    SizeAndAlignment::Value sizeAndAlignment;
#if BUSE_TZONE_SPEC_NAME_ARG
    const char* name;
#endif
};

BEXPORT void determineTZoneMallocFallback();

BEXPORT void* tzoneAllocateCompact(HeapRef);
BEXPORT void* tzoneAllocateNonCompact(HeapRef);

BEXPORT void* tzoneAllocateNonCompactWithFastFallbackSlow(size_t requestedSize, const TZoneSpecification&);
BEXPORT void* tzoneAllocateCompactWithFastFallbackSlow(size_t requestedSize, const TZoneSpecification&);
BEXPORT void* tzoneAllocateNonCompactWithIsoFallbackSlow(size_t requestedSize, const TZoneSpecification&);
BEXPORT void* tzoneAllocateCompactWithIsoFallbackSlow(size_t requestedSize, const TZoneSpecification&);

extern BEXPORT void (*tzoneFreeWithFastFallback)(void*);
extern BEXPORT void (*tzoneFreeWithIsoFallback)(void*);

#define MAKE_BTZONE_MALLOCED_COMMON(_type, _compactMode, _fallbackMode, _exportMacro) \
public: \
    using HeapRef = ::bmalloc::api::HeapRef; \
    using SizeAndAlignment = ::bmalloc::api::SizeAndAlignment; \
    using TZoneMallocFallback = ::bmalloc::api::TZoneMallocFallback; \
private: \
    static _exportMacro HeapRef s_heapRef; \
    static _exportMacro const TZoneSpecification s_heapSpec; \
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
        if (BUNLIKELY(!s_heapRef || size != sizeof(_type))) \
            BMUST_TAIL_CALL return operatorNewSlow(size); \
        BASSERT(::bmalloc::api::tzoneMallocFallback >= TZoneMallocFallback::ForceSpecifiedFallBack); \
        return ::bmalloc::api::tzoneAllocate ## _compactMode(s_heapRef); \
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

#define MAKE_BTZONE_MALLOCED_COMMON_NON_TEMPLATE(_type, _compactMode, _fallbackMode, _exportMacro) \
private: \
    static _exportMacro BNO_INLINE void* operatorNewSlow(size_t);

#define MAKE_BTZONE_MALLOCED_COMMON_TEMPLATE(_type, _compactMode, _fallbackMode, _exportMacro) \
private: \
    static _exportMacro BNO_INLINE void* operatorNewSlow(size_t size) \
    { \
        static const TZoneSpecification s_heapSpec TZONE_SPEC_ATTRIBUTE = { &s_heapRef, sizeof(_type), SizeAndAlignment::encode<_type>() TZONE_SPEC_NAME_ARG(#_type) }; \
        return ::bmalloc::api::tzoneAllocate ## _compactMode ## With ## _fallbackMode ## Slow(size, s_heapSpec); \
    }

#define MAKE_BTZONE_MALLOCED(_type, _compactMode, _fallbackMode, _exportMacro) \
    MAKE_BTZONE_MALLOCED_COMMON(_type, _compactMode, _fallbackMode, _exportMacro) \
    MAKE_BTZONE_MALLOCED_COMMON_NON_TEMPLATE(_type, _compactMode, _fallbackMode, _exportMacro) \
private: \
    using __makeTZoneMallocedMacroSemicolonifier BUNUSED_TYPE_ALIAS = int

#define MAKE_STRUCT_BTZONE_MALLOCED(_type, _compactMode, _fallbackMode, _exportMacro) \
    MAKE_BTZONE_MALLOCED_COMMON(_type, _compactMode, _fallbackMode, _exportMacro) \
    MAKE_BTZONE_MALLOCED_COMMON_NON_TEMPLATE(_type, _compactMode, _fallbackMode, _exportMacro) \
public: \
    using __makeTZoneMallocedMacroSemicolonifier BUNUSED_TYPE_ALIAS = int

#define MAKE_BTZONE_MALLOCED_TEMPLATE(_type, _compactMode, _fallbackMode, _exportMacro) \
    MAKE_BTZONE_MALLOCED_COMMON(_type, _compactMode, _fallbackMode, _exportMacro) \
    MAKE_BTZONE_MALLOCED_COMMON_TEMPLATE(_type, _compactMode, _fallbackMode, _exportMacro) \
private: \
    using __makeTZoneMallocedMacroSemicolonifier BUNUSED_TYPE_ALIAS = int


#define MAKE_BTZONE_MALLOCED_TEMPLATE_IMPL(_templateParameters, _type, _fallbackMode) \
    _templateParameters ::bmalloc::api::HeapRef _type::s_heapRef

// The following requires these 3 macros to be defined:
// TZONE_TEMPLATE_PARAMS, TZONE_TYPE
#define MAKE_BTZONE_MALLOCED_TEMPLATE_IMPL_WITH_MULTIPLE_PARAMETERS() \
    TZONE_TEMPLATE_PARAMS \
    ::bmalloc::api::HeapRef TZONE_TYPE::s_heapRef

#if BUSE_TZONE_PREINITIALIZATION

BEXPORT void tzonePreInitializeHeapRefs(const TZoneSpecification* start, const TZoneSpecification* end);

#define TZONE_SPEC_ATTRIBUTE __attribute__((used, section("__DATA_CONST,__tzone_spec")))

// The following is for pre-initializing HeapRefs.
// Use these at each framework/process initialization.
#define DECLARE_TZONE_HEAPREF_SPECIFICATION_BOUNDS(_regionName) \
    extern const TZoneSpecification startOf ## _regionName ## TZoneSpecifications __asm("section$start$__DATA_CONST$__tzone_spec"); \
    extern const TZoneSpecification endOf ## _regionName ## TZoneSpecifications __asm("section$end$__DATA_CONST$__tzone_spec")

#define PREINITIALIZE_TZONE_HEAPREFS(_regionName) \
    ::bmalloc::api::tzonePreInitializeHeapRefs(&startOf ## _regionName ## TZoneSpecifications, &endOf ## _regionName ## TZoneSpecifications)

#else // BUSE_TZONE_PREINITIALIZATION

#define TZONE_SPEC_ATTRIBUTE
#define DECLARE_TZONE_HEAPREF_SPECIFICATION_BOUNDS(_regionName) \
    using __thisIsHereToForceASemicolonAfterThisMacro UNUSED_TYPE_ALIAS = int
#define PREINITIALIZE_TZONE_HEAPREFS(_regionName) \
    using __thisIsHereToForceASemicolonAfterThisMacro UNUSED_TYPE_ALIAS = int

#endif // BUSE_TZONE_PREINITIALIZATION

} } // namespace bmalloc::api

using TZoneSpecification = ::bmalloc::api::TZoneSpecification;

#else

#define DECLARE_TZONE_HEAPREF_SPECIFICATION_BOUNDS(_regionName) \
    using __thisIsHereToForceASemicolonAfterThisMacro UNUSED_TYPE_ALIAS = int
#define PREINITIALIZE_TZONE_HEAPREFS(_regionName) \
    using __thisIsHereToForceASemicolonAfterThisMacro UNUSED_TYPE_ALIAS = int

#endif // BUSE(TZONE)
