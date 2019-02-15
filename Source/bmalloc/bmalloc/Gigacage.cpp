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

#include "Gigacage.h"

#include "CryptoRandom.h"
#include "Environment.h"
#include "PerProcess.h"
#include "ProcessCheck.h"
#include "VMAllocate.h"
#include "Vector.h"
#include "bmalloc.h"
#include <cstdio>
#include <mutex>

#if GIGACAGE_ENABLED

namespace Gigacage {

// This is exactly 32GB because inside JSC, indexed accesses for arrays, typed arrays, etc,
// use unsigned 32-bit ints as indices. The items those indices access are 8 bytes or less
// in size. 2^32 * 8 = 32GB. This means if an access on a caged type happens to go out of
// bounds, the access is guaranteed to land somewhere else in the cage or inside the runway.
// If this were less than 32GB, those OOB accesses could reach outside of the cage.
constexpr size_t gigacageRunway = 32llu * 1024 * 1024 * 1024;

// Note: g_gigacageBasePtrs[0] is reserved for storing the wasEnabled flag.
// The first gigacageBasePtr will start at g_gigacageBasePtrs[sizeof(void*)].
// This is done so that the wasEnabled flag will also be protected along with the
// gigacageBasePtrs.
alignas(gigacageBasePtrsSize) char g_gigacageBasePtrs[gigacageBasePtrsSize];

using namespace bmalloc;

namespace {

bool s_isDisablingPrimitiveGigacageDisabled;

void protectGigacageBasePtrs()
{
    uintptr_t basePtrs = reinterpret_cast<uintptr_t>(g_gigacageBasePtrs);
    // We might only get page size alignment, but that's also the minimum we need.
    RELEASE_BASSERT(!(basePtrs & (vmPageSize() - 1)));
    mprotect(g_gigacageBasePtrs, gigacageBasePtrsSize, PROT_READ);
}

void unprotectGigacageBasePtrs()
{
    mprotect(g_gigacageBasePtrs, gigacageBasePtrsSize, PROT_READ | PROT_WRITE);
}

class UnprotectGigacageBasePtrsScope {
public:
    UnprotectGigacageBasePtrsScope()
    {
        unprotectGigacageBasePtrs();
    }
    
    ~UnprotectGigacageBasePtrsScope()
    {
        protectGigacageBasePtrs();
    }
};

struct Callback {
    Callback() { }
    
    Callback(void (*function)(void*), void *argument)
        : function(function)
        , argument(argument)
    {
    }
    
    void (*function)(void*) { nullptr };
    void* argument { nullptr };
};

struct PrimitiveDisableCallbacks {
    PrimitiveDisableCallbacks(std::lock_guard<Mutex>&) { }
    
    Vector<Callback> callbacks;
};

size_t runwaySize(Kind kind)
{
    switch (kind) {
    case Kind::ReservedForFlagsAndNotABasePtr:
        RELEASE_BASSERT_NOT_REACHED();
    case Kind::Primitive:
        return gigacageRunway;
    case Kind::JSValue:
        return 0;
    }
    return 0;
}

} // anonymous namespace

void ensureGigacage()
{
    static std::once_flag onceFlag;
    std::call_once(
        onceFlag,
        [] {
            if (!shouldBeEnabled())
                return;
            
            Kind shuffledKinds[numKinds];
            for (unsigned i = 0; i < numKinds; ++i)
                shuffledKinds[i] = static_cast<Kind>(i + 1); // + 1 to skip Kind::ReservedForFlagsAndNotABasePtr.
            
            // We just go ahead and assume that 64 bits is enough randomness. That's trivially true right
            // now, but would stop being true if we went crazy with gigacages. Based on my math, 21 is the
            // largest value of n so that n! <= 2^64.
            static_assert(numKinds <= 21, "too many kinds");
            uint64_t random;
            cryptoRandom(reinterpret_cast<unsigned char*>(&random), sizeof(random));
            for (unsigned i = numKinds; i--;) {
                unsigned limit = i + 1;
                unsigned j = static_cast<unsigned>(random % limit);
                random /= limit;
                std::swap(shuffledKinds[i], shuffledKinds[j]);
            }

            auto alignTo = [] (Kind kind, size_t totalSize) -> size_t {
                return roundUpToMultipleOf(alignment(kind), totalSize);
            };
            auto bump = [] (Kind kind, size_t totalSize) -> size_t {
                return totalSize + size(kind);
            };
            
            size_t totalSize = 0;
            size_t maxAlignment = 0;
            
            for (Kind kind : shuffledKinds) {
                totalSize = bump(kind, alignTo(kind, totalSize));
                totalSize += runwaySize(kind);
                maxAlignment = std::max(maxAlignment, alignment(kind));
            }

            // FIXME: Randomize where this goes.
            // https://bugs.webkit.org/show_bug.cgi?id=175245
            void* base = tryVMAllocate(maxAlignment, totalSize, VMTag::JSGigacage);
            if (!base) {
                if (GIGACAGE_ALLOCATION_CAN_FAIL)
                    return;
                fprintf(stderr, "FATAL: Could not allocate gigacage memory with maxAlignment = %lu, totalSize = %lu.\n", maxAlignment, totalSize);
                fprintf(stderr, "(Make sure you have not set a virtual memory limit.)\n");
                BCRASH();
            }

            size_t nextCage = 0;
            for (Kind kind : shuffledKinds) {
                nextCage = alignTo(kind, nextCage);
                basePtr(kind) = reinterpret_cast<char*>(base) + nextCage;
                nextCage = bump(kind, nextCage);
                if (runwaySize(kind) > 0) {
                    char* runway = reinterpret_cast<char*>(base) + nextCage;
                    // Make OOB accesses into the runway crash.
                    vmRevokePermissions(runway, runwaySize(kind));
                    nextCage += runwaySize(kind);
                }
            }
            
            vmDeallocatePhysicalPages(base, totalSize);
            setWasEnabled();
            protectGigacageBasePtrs();
        });
}

void disablePrimitiveGigacage()
{
    ensureGigacage();
    if (!basePtrs().primitive) {
        // It was never enabled. That means that we never even saved any callbacks. Or, we had already disabled
        // it before, and already called the callbacks.
        return;
    }
    
    PrimitiveDisableCallbacks& callbacks = *PerProcess<PrimitiveDisableCallbacks>::get();
    std::unique_lock<Mutex> lock(PerProcess<PrimitiveDisableCallbacks>::mutex());
    for (Callback& callback : callbacks.callbacks)
        callback.function(callback.argument);
    callbacks.callbacks.shrink(0);
    UnprotectGigacageBasePtrsScope unprotectScope;
    basePtrs().primitive = nullptr;
}

void addPrimitiveDisableCallback(void (*function)(void*), void* argument)
{
    ensureGigacage();
    if (!basePtrs().primitive) {
        // It was already disabled or we were never able to enable it.
        function(argument);
        return;
    }
    
    PrimitiveDisableCallbacks& callbacks = *PerProcess<PrimitiveDisableCallbacks>::get();
    std::unique_lock<Mutex> lock(PerProcess<PrimitiveDisableCallbacks>::mutex());
    callbacks.callbacks.push(Callback(function, argument));
}

void removePrimitiveDisableCallback(void (*function)(void*), void* argument)
{
    PrimitiveDisableCallbacks& callbacks = *PerProcess<PrimitiveDisableCallbacks>::get();
    std::unique_lock<Mutex> lock(PerProcess<PrimitiveDisableCallbacks>::mutex());
    for (size_t i = 0; i < callbacks.callbacks.size(); ++i) {
        if (callbacks.callbacks[i].function == function
            && callbacks.callbacks[i].argument == argument) {
            callbacks.callbacks[i] = callbacks.callbacks.last();
            callbacks.callbacks.pop();
            return;
        }
    }
}

static void primitiveGigacageDisabled(void*)
{
    if (GIGACAGE_ALLOCATION_CAN_FAIL && !wasEnabled())
        return;

    static bool s_false;
    fprintf(stderr, "FATAL: Primitive gigacage disabled, but we don't want that in this process.\n");
    if (!s_false)
        BCRASH();
}

void disableDisablingPrimitiveGigacageIfShouldBeEnabled()
{
    if (shouldBeEnabled()) {
        addPrimitiveDisableCallback(primitiveGigacageDisabled, nullptr);
        s_isDisablingPrimitiveGigacageDisabled = true;
    }
}

bool isDisablingPrimitiveGigacageDisabled()
{
    return s_isDisablingPrimitiveGigacageDisabled;
}

bool shouldBeEnabled()
{
    static bool cached = false;
    static std::once_flag onceFlag;
    std::call_once(
        onceFlag,
        [] {
            bool debugHeapEnabled = PerProcess<Environment>::get()->isDebugHeapEnabled();
            if (debugHeapEnabled)
                return;

            if (!gigacageEnabledForProcess())
                return;
            
            if (char* gigacageEnabled = getenv("GIGACAGE_ENABLED")) {
                if (!strcasecmp(gigacageEnabled, "no") || !strcasecmp(gigacageEnabled, "false") || !strcasecmp(gigacageEnabled, "0")) {
                    fprintf(stderr, "Warning: disabling gigacage because GIGACAGE_ENABLED=%s!\n", gigacageEnabled);
                    return;
                } else if (strcasecmp(gigacageEnabled, "yes") && strcasecmp(gigacageEnabled, "true") && strcasecmp(gigacageEnabled, "1"))
                    fprintf(stderr, "Warning: invalid argument to GIGACAGE_ENABLED: %s\n", gigacageEnabled);
            }
            
            cached = true;
        });
    return cached;
}

} // namespace Gigacage

#endif // GIGACAGE_ENABLED


