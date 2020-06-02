/*
 * Copyright (C) 2017-2020 Apple Inc. All rights reserved.
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

#include "BAssert.h"
#include "BExport.h"
#include "BInline.h"
#include "BPlatform.h"
#include "GigacageConfig.h"
#include "Sizes.h"
#include "StdLibExtras.h"
#include <cstddef>
#include <inttypes.h>

#if ((BOS(DARWIN) || BOS(LINUX)) && \
    (BCPU(X86_64) || (BCPU(ARM64) && !defined(__ILP32__) && (!BPLATFORM(IOS_FAMILY) || BPLATFORM(IOS)))))
#define GIGACAGE_ENABLED 1
#else
#define GIGACAGE_ENABLED 0
#endif


namespace Gigacage {

BINLINE const char* name(Kind kind)
{
    switch (kind) {
    case Primitive:
        return "Primitive";
    case JSValue:
        return "JSValue";
    case NumberOfKinds:
        break;
    }
    BCRASH();
    return nullptr;
}

#if GIGACAGE_ENABLED

#if BOS_EFFECTIVE_ADDRESS_WIDTH < 48
constexpr size_t primitiveGigacageSize = 2 * bmalloc::Sizes::GB;
constexpr size_t jsValueGigacageSize = 2 * bmalloc::Sizes::GB;
constexpr size_t maximumCageSizeReductionForSlide = bmalloc::Sizes::GB / 4;
#else
constexpr size_t primitiveGigacageSize = 32 * bmalloc::Sizes::GB;
constexpr size_t jsValueGigacageSize = 16 * bmalloc::Sizes::GB;
constexpr size_t maximumCageSizeReductionForSlide = 4 * bmalloc::Sizes::GB;
#endif

// In Linux, if `vm.overcommit_memory = 2` is specified, mmap with large size can fail if it exceeds the size of RAM.
// So we specify GIGACAGE_ALLOCATION_CAN_FAIL = 1.
#if BOS(LINUX)
#define GIGACAGE_ALLOCATION_CAN_FAIL 1
#else
#define GIGACAGE_ALLOCATION_CAN_FAIL 0
#endif


static_assert(bmalloc::isPowerOfTwo(primitiveGigacageSize), "");
static_assert(bmalloc::isPowerOfTwo(jsValueGigacageSize), "");
static_assert(primitiveGigacageSize > maximumCageSizeReductionForSlide, "");
static_assert(jsValueGigacageSize > maximumCageSizeReductionForSlide, "");

constexpr size_t gigacageSizeToMask(size_t size) { return size - 1; }

constexpr size_t primitiveGigacageMask = gigacageSizeToMask(primitiveGigacageSize);
constexpr size_t jsValueGigacageMask = gigacageSizeToMask(jsValueGigacageSize);

// These constants are needed by the LLInt.
constexpr ptrdiff_t offsetOfPrimitiveGigacageBasePtr = Kind::Primitive * sizeof(void*);
constexpr ptrdiff_t offsetOfJSValueGigacageBasePtr = Kind::JSValue * sizeof(void*);

extern "C" BEXPORT bool disablePrimitiveGigacageRequested;

BINLINE bool isEnabled() { return g_gigacageConfig.isEnabled; }

BEXPORT void ensureGigacage();

BEXPORT void disablePrimitiveGigacage();

// This will call the disable callback immediately if the Primitive Gigacage is currently disabled.
BEXPORT void addPrimitiveDisableCallback(void (*)(void*), void*);
BEXPORT void removePrimitiveDisableCallback(void (*)(void*), void*);

BEXPORT void forbidDisablingPrimitiveGigacage();

BINLINE bool disablingPrimitiveGigacageIsForbidden()
{
    return g_gigacageConfig.disablingPrimitiveGigacageIsForbidden;
}

BINLINE bool disableNotRequestedForPrimitiveGigacage()
{
    return !disablePrimitiveGigacageRequested;
}

BINLINE bool isEnabled(Kind kind)
{
    if (kind == Primitive)
        return g_gigacageConfig.basePtr(Primitive) && (disablingPrimitiveGigacageIsForbidden() || disableNotRequestedForPrimitiveGigacage());
    return g_gigacageConfig.basePtr(kind);
}

BINLINE void* basePtr(Kind kind)
{
    BASSERT(isEnabled(kind));
    return g_gigacageConfig.basePtr(kind);
}

BINLINE void* addressOfBasePtr(Kind kind)
{
    RELEASE_BASSERT(kind < NumberOfKinds);
    return &g_gigacageConfig.basePtrs[kind];
}

BINLINE size_t size(Kind kind)
{
    switch (kind) {
    case Primitive:
        return static_cast<size_t>(primitiveGigacageSize);
    case JSValue:
        return static_cast<size_t>(jsValueGigacageSize);
    case NumberOfKinds:
        break;
    }
    BCRASH();
    return 0;
}

BINLINE size_t alignment(Kind kind)
{
    return size(kind);
}

BINLINE size_t mask(Kind kind)
{
    return gigacageSizeToMask(size(kind));
}

template<typename Func>
void forEachKind(const Func& func)
{
    func(Primitive);
    func(JSValue);
}

template<typename T>
BINLINE T* caged(Kind kind, T* ptr)
{
    BASSERT(ptr);
    if (!isEnabled(kind))
        return ptr;
    void* gigacageBasePtr = basePtr(kind);
    return reinterpret_cast<T*>(
        reinterpret_cast<uintptr_t>(gigacageBasePtr) + (
            reinterpret_cast<uintptr_t>(ptr) & mask(kind)));
}

template<typename T>
BINLINE T* cagedMayBeNull(Kind kind, T* ptr)
{
    if (!ptr)
        return ptr;
    return caged(kind, ptr);
}

BINLINE bool isCaged(Kind kind, const void* ptr)
{
    return caged(kind, ptr) == ptr;
}

BINLINE bool contains(const void* ptr)
{
    auto* start = reinterpret_cast<const uint8_t*>(g_gigacageConfig.start);
    auto* p = reinterpret_cast<const uint8_t*>(ptr);
    return static_cast<size_t>(p - start) < g_gigacageConfig.totalSize;
}

BEXPORT bool shouldBeEnabled();

#else // GIGACAGE_ENABLED

BINLINE void* basePtr(Kind)
{
    BCRASH();
    static void* unreachable;
    return unreachable;
}
BINLINE size_t size(Kind) { BCRASH(); return 0; }
BINLINE void ensureGigacage() { }
BINLINE bool contains(const void*) { return false; }
BINLINE bool disablingPrimitiveGigacageIsForbidden() { return false; }
BINLINE bool isEnabled() { return false; }
BINLINE bool isCaged(Kind, const void*) { return true; }
BINLINE bool isEnabled(Kind) { return false; }
template<typename T> BINLINE T* caged(Kind, T* ptr) { return ptr; }
template<typename T> BINLINE T* cagedMayBeNull(Kind, T* ptr) { return ptr; }
BINLINE void forbidDisablingPrimitiveGigacage() { }
BINLINE void disablePrimitiveGigacage() { }
BINLINE void addPrimitiveDisableCallback(void (*)(void*), void*) { }
BINLINE void removePrimitiveDisableCallback(void (*)(void*), void*) { }

#endif // GIGACAGE_ENABLED

} // namespace Gigacage



