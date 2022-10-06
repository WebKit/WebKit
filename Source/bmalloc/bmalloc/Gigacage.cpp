/*
 * Copyright (C) 2017-2021 Apple Inc. All rights reserved.
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
#include "Mutex.h"
#include "ProcessCheck.h"
#include "StaticPerProcess.h"
#include "VMAllocate.h"
#include "Vector.h"
#include "bmalloc.h"
#include <cstdio>

#if BOS(DARWIN)
#include <mach/mach.h>
#endif

#if BUSE(LIBPAS)
#include "iso_heap.h"
#endif

#if GIGACAGE_ENABLED

namespace Gigacage {

#if !BENABLE(UNIFIED_AND_FREEZABLE_CONFIG_RECORD)
Config g_gigacageConfig;
#endif

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

} // namespace Gigacage

namespace bmalloc {

struct PrimitiveDisableCallbacks : public StaticPerProcess<PrimitiveDisableCallbacks> {
    PrimitiveDisableCallbacks(const LockHolder&) { }
    
    Vector<Gigacage::Callback> callbacks;
};
DECLARE_STATIC_PER_PROCESS_STORAGE(PrimitiveDisableCallbacks);
DEFINE_STATIC_PER_PROCESS_STORAGE(PrimitiveDisableCallbacks);

} // namespace bmalloc

namespace Gigacage {

// This is exactly 32GB because inside JSC, indexed accesses for arrays, typed arrays, etc,
// use unsigned 32-bit ints as indices. The items those indices access are 8 bytes or less
// in size. 2^32 * 8 = 32GB. This means if an access on a caged type happens to go out of
// bounds, the access is guaranteed to land somewhere else in the cage or inside the runway.
// If this were less than 32GB, those OOB accesses could reach outside of the cage.
constexpr size_t gigacageRunway = 32llu * bmalloc::Sizes::GB;

bool disablePrimitiveGigacageRequested = false;

using namespace bmalloc;

namespace {

size_t runwaySize(Kind kind)
{
    switch (kind) {
    case Kind::Primitive:
        return gigacageRunway;
    case Kind::JSValue:
        return 0;
    case Kind::NumberOfKinds:
        RELEASE_BASSERT_NOT_REACHED();
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
            RELEASE_BASSERT(!g_gigacageConfig.ensureGigacageHasBeenCalled);
            g_gigacageConfig.ensureGigacageHasBeenCalled = true;

            if (!shouldBeEnabled())
                return;

#if BENABLE(UNIFIED_AND_FREEZABLE_CONFIG_RECORD)
            // We might only get page size alignment, but that's also the minimum
            // alignment we need for freezing the Config.
            RELEASE_BASSERT(!(reinterpret_cast<size_t>(&WebConfig::g_config) & (vmPageSize() - 1)));
#endif

            Kind shuffledKinds[NumberOfKinds];
            for (unsigned i = 0; i < NumberOfKinds; ++i)
                shuffledKinds[i] = static_cast<Kind>(i);
            
            // We just go ahead and assume that 64 bits is enough randomness. That's trivially true right
            // now, but would stop being true if we went crazy with gigacages. Based on my math, 21 is the
            // largest value of n so that n! <= 2^64.
            static_assert(NumberOfKinds <= 21, "too many kinds");
            uint64_t random;
            cryptoRandom(reinterpret_cast<unsigned char*>(&random), sizeof(random));
            for (unsigned i = NumberOfKinds; i--;) {
                unsigned limit = i + 1;
                unsigned j = static_cast<unsigned>(random % limit);
                random /= limit;
                std::swap(shuffledKinds[i], shuffledKinds[j]);
            }

            auto alignTo = [] (Kind kind, size_t totalSize) -> size_t {
                return roundUpToMultipleOf(alignment(kind), totalSize);
            };
            auto bump = [] (Kind kind, size_t totalSize) -> size_t {
                return totalSize + maxSize(kind);
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
                void* gigacageBasePtr = reinterpret_cast<char*>(base) + nextCage;
                g_gigacageConfig.setBasePtr(kind, gigacageBasePtr);
                nextCage = bump(kind, nextCage);

                uint64_t random[2];
                cryptoRandom(reinterpret_cast<unsigned char*>(random), sizeof(random));
                size_t gigacageSize = maxSize(kind);
                size_t size = roundDownToMultipleOf(vmPageSize(), gigacageSize - (random[0] % maximumCageSizeReductionForSlide));
                g_gigacageConfig.setAllocSize(kind, size);
                ptrdiff_t offset = roundDownToMultipleOf(vmPageSize(), random[1] % (gigacageSize - size));
                void* thisBase = reinterpret_cast<unsigned char*>(gigacageBasePtr) + offset;
                g_gigacageConfig.setAllocBasePtr(kind, thisBase);

#if BUSE(LIBPAS)
                bmalloc_force_auxiliary_heap_into_reserved_memory(
                    &api::heapForKind(kind),
                    reinterpret_cast<uintptr_t>(thisBase),
                    reinterpret_cast<uintptr_t>(thisBase) + size);
#endif
                
                if (runwaySize(kind) > 0) {
                    char* runway = reinterpret_cast<char*>(base) + nextCage;
                    // Make OOB accesses into the runway crash.
                    vmRevokePermissions(runway, runwaySize(kind));
                    nextCage += runwaySize(kind);
                }
            }

            g_gigacageConfig.start = base;
            g_gigacageConfig.totalSize = totalSize;
            vmDeallocatePhysicalPages(base, totalSize);
            g_gigacageConfig.isEnabled = true;
        });
}

void disablePrimitiveGigacage()
{
    if (g_gigacageConfig.disablingPrimitiveGigacageIsForbidden)
        fprintf(stderr, "FATAL: Disabling Primitive gigacage is forbidden, but we don't want that in this process.\n");

    RELEASE_BASSERT(!g_gigacageConfig.disablingPrimitiveGigacageIsForbidden);

    ensureGigacage();
    disablePrimitiveGigacageRequested = true;
    if (!g_gigacageConfig.basePtrs[Primitive]) {
        // It was never enabled. That means that we never even saved any callbacks. Or, we had already disabled
        // it before, and already called the callbacks.
        return;
    }
    
    PrimitiveDisableCallbacks& callbacks = *PrimitiveDisableCallbacks::get();
    UniqueLockHolder lock(PrimitiveDisableCallbacks::mutex());
    for (Callback& callback : callbacks.callbacks)
        callback.function(callback.argument);
    callbacks.callbacks.shrink(0);
}

void addPrimitiveDisableCallback(void (*function)(void*), void* argument)
{
    ensureGigacage();
    if (!g_gigacageConfig.basePtrs[Primitive]) {
        // It was already disabled or we were never able to enable it.
        function(argument);
        return;
    }
    
    PrimitiveDisableCallbacks& callbacks = *PrimitiveDisableCallbacks::get();
    UniqueLockHolder lock(PrimitiveDisableCallbacks::mutex());
    callbacks.callbacks.push(Callback(function, argument));
}

void removePrimitiveDisableCallback(void (*function)(void*), void* argument)
{
    PrimitiveDisableCallbacks& callbacks = *PrimitiveDisableCallbacks::get();
    UniqueLockHolder lock(PrimitiveDisableCallbacks::mutex());
    for (size_t i = 0; i < callbacks.callbacks.size(); ++i) {
        if (callbacks.callbacks[i].function == function
            && callbacks.callbacks[i].argument == argument) {
            callbacks.callbacks[i] = callbacks.callbacks.last();
            callbacks.callbacks.pop();
            return;
        }
    }
}

static bool verifyGigacageIsEnabled()
{
    bool isEnabled = g_gigacageConfig.isEnabled;
    for (size_t i = 0; i < NumberOfKinds; ++i)
        isEnabled = isEnabled && g_gigacageConfig.basePtrs[i];
    isEnabled = isEnabled && g_gigacageConfig.start;
    isEnabled = isEnabled && g_gigacageConfig.totalSize;
    return isEnabled;
}

void forbidDisablingPrimitiveGigacage()
{
    ensureGigacage();
    RELEASE_BASSERT(g_gigacageConfig.shouldBeEnabledHasBeenCalled
        && (GIGACAGE_ALLOCATION_CAN_FAIL || !g_gigacageConfig.shouldBeEnabled || verifyGigacageIsEnabled()));

    if (!g_gigacageConfig.disablingPrimitiveGigacageIsForbidden)
        g_gigacageConfig.disablingPrimitiveGigacageIsForbidden = true;
    RELEASE_BASSERT(disablingPrimitiveGigacageIsForbidden());
}

bool shouldBeEnabled()
{
    static std::once_flag onceFlag;
    std::call_once(
        onceFlag,
        [] {
            RELEASE_BASSERT(!g_gigacageConfig.shouldBeEnabledHasBeenCalled);
            g_gigacageConfig.shouldBeEnabledHasBeenCalled = true;

            bool debugHeapEnabled = Environment::get()->isDebugHeapEnabled();
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
            
            g_gigacageConfig.shouldBeEnabled = true;
        });
    return g_gigacageConfig.shouldBeEnabled;
}

void* allocBase(Kind kind)
{
    return g_gigacageConfig.allocBasePtr(kind);
}

size_t size(Kind kind)
{
    return g_gigacageConfig.allocSize(kind);
}

size_t footprint(Kind kind)
{
#if BUSE(LIBPAS)
    BUNUSED(kind);
    return 0;
#else
    return PerProcess<PerHeapKind<Heap>>::get()->at(heapKind(kind)).footprint();
#endif
}

} // namespace Gigacage

#endif // GIGACAGE_ENABLED
