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
#include <cstddef>
#include <inttypes.h>

#if BCPU(ARM64)
#define PRIMITIVE_GIGACAGE_SIZE 0x80000000llu
#define JSVALUE_GIGACAGE_SIZE 0x40000000llu
#define GIGACAGE_ALLOCATION_CAN_FAIL 1
#else
#define PRIMITIVE_GIGACAGE_SIZE 0x800000000llu
#define JSVALUE_GIGACAGE_SIZE 0x400000000llu
#define GIGACAGE_ALLOCATION_CAN_FAIL 0
#endif

// In Linux, if `vm.overcommit_memory = 2` is specified, mmap with large size can fail if it exceeds the size of RAM.
// So we specify GIGACAGE_ALLOCATION_CAN_FAIL = 1.
#if BOS(LINUX)
#undef GIGACAGE_ALLOCATION_CAN_FAIL
#define GIGACAGE_ALLOCATION_CAN_FAIL 1
#endif

static_assert(bmalloc::isPowerOfTwo(PRIMITIVE_GIGACAGE_SIZE), "");
static_assert(bmalloc::isPowerOfTwo(JSVALUE_GIGACAGE_SIZE), "");

#define GIGACAGE_SIZE_TO_MASK(size) ((size) - 1)

#define PRIMITIVE_GIGACAGE_MASK GIGACAGE_SIZE_TO_MASK(PRIMITIVE_GIGACAGE_SIZE)
#define JSVALUE_GIGACAGE_MASK GIGACAGE_SIZE_TO_MASK(JSVALUE_GIGACAGE_SIZE)

#if ((BOS(DARWIN) || BOS(LINUX)) && \
    (BCPU(X86_64) || (BCPU(ARM64) && !defined(__ILP32__) && (!BPLATFORM(IOS_FAMILY) || BPLATFORM(IOS)))))
#define GIGACAGE_ENABLED 1
#else
#define GIGACAGE_ENABLED 0
#endif

#if BCPU(ARM64)
#define GIGACAGE_BASE_PTRS_SIZE 16384
#else
#define GIGACAGE_BASE_PTRS_SIZE 4096
#endif

extern "C" alignas(GIGACAGE_BASE_PTRS_SIZE) BEXPORT char g_gigacageBasePtrs[GIGACAGE_BASE_PTRS_SIZE];

namespace Gigacage {

BINLINE bool wasEnabled() { return g_gigacageBasePtrs[0]; }
BINLINE void setWasEnabled() { g_gigacageBasePtrs[0] = true; }

struct BasePtrs {
    uintptr_t reservedForFlags;
    void* primitive;
    void* jsValue;
};

enum Kind {
    ReservedForFlagsAndNotABasePtr = 0,
    Primitive,
    JSValue,
};

static_assert(offsetof(BasePtrs, primitive) == Kind::Primitive * sizeof(void*), "");
static_assert(offsetof(BasePtrs, jsValue) == Kind::JSValue * sizeof(void*), "");

static constexpr unsigned numKinds = 2;

BEXPORT void ensureGigacage();

BEXPORT void disablePrimitiveGigacage();

// This will call the disable callback immediately if the Primitive Gigacage is currently disabled.
BEXPORT void addPrimitiveDisableCallback(void (*)(void*), void*);
BEXPORT void removePrimitiveDisableCallback(void (*)(void*), void*);

BEXPORT void disableDisablingPrimitiveGigacageIfShouldBeEnabled();

BEXPORT bool isDisablingPrimitiveGigacageDisabled();
inline bool isPrimitiveGigacagePermanentlyEnabled() { return isDisablingPrimitiveGigacageDisabled(); }
inline bool canPrimitiveGigacageBeDisabled() { return !isDisablingPrimitiveGigacageDisabled(); }

BINLINE const char* name(Kind kind)
{
    switch (kind) {
    case ReservedForFlagsAndNotABasePtr:
        RELEASE_BASSERT_NOT_REACHED();
    case Primitive:
        return "Primitive";
    case JSValue:
        return "JSValue";
    }
    BCRASH();
    return nullptr;
}

BINLINE void*& basePtr(BasePtrs& basePtrs, Kind kind)
{
    switch (kind) {
    case ReservedForFlagsAndNotABasePtr:
        RELEASE_BASSERT_NOT_REACHED();
    case Primitive:
        return basePtrs.primitive;
    case JSValue:
        return basePtrs.jsValue;
    }
    BCRASH();
    return basePtrs.primitive;
}

BINLINE BasePtrs& basePtrs()
{
    return *reinterpret_cast<BasePtrs*>(reinterpret_cast<void*>(g_gigacageBasePtrs));
}

BINLINE void*& basePtr(Kind kind)
{
    return basePtr(basePtrs(), kind);
}

BINLINE bool isEnabled(Kind kind)
{
    return !!basePtr(kind);
}

BINLINE size_t size(Kind kind)
{
    switch (kind) {
    case ReservedForFlagsAndNotABasePtr:
        RELEASE_BASSERT_NOT_REACHED();
    case Primitive:
        return static_cast<size_t>(PRIMITIVE_GIGACAGE_SIZE);
    case JSValue:
        return static_cast<size_t>(JSVALUE_GIGACAGE_SIZE);
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
    return GIGACAGE_SIZE_TO_MASK(size(kind));
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
    void* gigacageBasePtr = basePtr(kind);
    if (!gigacageBasePtr)
        return ptr;
    return reinterpret_cast<T*>(
        reinterpret_cast<uintptr_t>(gigacageBasePtr) + (
            reinterpret_cast<uintptr_t>(ptr) & mask(kind)));
}

BINLINE bool isCaged(Kind kind, const void* ptr)
{
    return caged(kind, ptr) == ptr;
}

BEXPORT bool shouldBeEnabled();

} // namespace Gigacage


