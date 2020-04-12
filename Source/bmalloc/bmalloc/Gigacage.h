/*
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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

#include "Algorithm.h"
#include "BAssert.h"
#include "BExport.h"
#include "BInline.h"
#include "BPlatform.h"
#include "Sizes.h"
#include <cstddef>
#include <inttypes.h>

#if ((BOS(DARWIN) || BOS(LINUX)) && \
    (BCPU(X86_64) || (BCPU(ARM64) && !defined(__ILP32__) && (!BPLATFORM(IOS_FAMILY) || BPLATFORM(IOS)))))
#define GIGACAGE_ENABLED 1
#else
#define GIGACAGE_ENABLED 0
#endif


namespace Gigacage {

enum Kind {
    Primitive,
    JSValue,
    NumberOfKinds
};

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

constexpr size_t configSizeToProtect = 16 * bmalloc::Sizes::kB;

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

struct Config {
    void* basePtr(Kind kind) const
    {
        RELEASE_BASSERT(kind < NumberOfKinds);
        return basePtrs[kind];
    }

    void setBasePtr(Kind kind, void* ptr)
    {
        RELEASE_BASSERT(kind < NumberOfKinds);
        basePtrs[kind] = ptr;
    }

    union {
        struct {
            // All the fields in this struct should be chosen such that their
            // initial value is 0 / null / falsy because Config is instantiated
            // as a global singleton.

            bool isEnabled;
            bool isPermanentlyFrozen;
            bool disablingPrimitiveGigacageIsForbidden;
            bool shouldBeEnabled;

            // We would like to just put the std::once_flag for these functions
            // here, but we can't because std::once_flag has a implicitly-deleted
            // default constructor. So, we use a boolean instead.
            bool shouldBeEnabledHasBeenCalled;
            bool ensureGigacageHasBeenCalled;

            void* start;
            size_t totalSize;
            void* basePtrs[NumberOfKinds];
        };
        char ensureSize[configSizeToProtect];
    };
};
static_assert(sizeof(Config) == configSizeToProtect, "Gigacage Config must fit in configSizeToProtect");

extern "C" alignas(configSizeToProtect) BEXPORT Config g_gigacageConfig;

// These constants are needed by the LLInt.
constexpr ptrdiff_t offsetOfPrimitiveGigacageBasePtr = Kind::Primitive * sizeof(void*);
constexpr ptrdiff_t offsetOfJSValueGigacageBasePtr = Kind::JSValue * sizeof(void*);


BINLINE bool isEnabled() { return g_gigacageConfig.isEnabled; }

BEXPORT void ensureGigacage();

BEXPORT void disablePrimitiveGigacage();

// This will call the disable callback immediately if the Primitive Gigacage is currently disabled.
BEXPORT void addPrimitiveDisableCallback(void (*)(void*), void*);
BEXPORT void removePrimitiveDisableCallback(void (*)(void*), void*);

BEXPORT void forbidDisablingPrimitiveGigacage();

BEXPORT bool isDisablingPrimitiveGigacageForbidden();
inline bool isPrimitiveGigacagePermanentlyEnabled() { return isDisablingPrimitiveGigacageForbidden(); }
inline bool canPrimitiveGigacageBeDisabled() { return !isDisablingPrimitiveGigacageForbidden(); }

BINLINE void* basePtr(Kind kind)
{
    return g_gigacageConfig.basePtr(kind);
}

BINLINE void* addressOfBasePtr(Kind kind)
{
    RELEASE_BASSERT(kind < NumberOfKinds);
    return &g_gigacageConfig.basePtrs[kind];
}

BINLINE bool isEnabled(Kind kind)
{
    return !!g_gigacageConfig.basePtr(kind);
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
    void* gigacageBasePtr = g_gigacageConfig.basePtr(kind);
    if (!gigacageBasePtr)
        return ptr;
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
BINLINE bool isEnabled() { return false; }
BINLINE bool isCaged(Kind, const void*) { return true; }
BINLINE bool isEnabled(Kind) { return false; }
template<typename T> BINLINE T* caged(Kind, T* ptr) { return ptr; }
template<typename T> BINLINE T* cagedMayBeNull(Kind, T* ptr) { return ptr; }
BINLINE void forbidDisablingPrimitiveGigacage() { }
BINLINE bool canPrimitiveGigacageBeDisabled() { return false; }
BINLINE void disablePrimitiveGigacage() { }
BINLINE void addPrimitiveDisableCallback(void (*)(void*), void*) { }
BINLINE void removePrimitiveDisableCallback(void (*)(void*), void*) { }

#endif // GIGACAGE_ENABLED

} // namespace Gigacage



