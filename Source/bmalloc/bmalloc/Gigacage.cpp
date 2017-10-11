/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "Environment.h"
#include "PerProcess.h"
#include "VMAllocate.h"
#include "Vector.h"
#include "bmalloc.h"
#include <cstdio>
#include <mutex>

char g_gigacageBasePtrs[GIGACAGE_BASE_PTRS_SIZE] __attribute__((aligned(GIGACAGE_BASE_PTRS_SIZE)));

using namespace bmalloc;

namespace Gigacage {

namespace {

bool s_isDisablingPrimitiveGigacageDisabled;

void protectGigacageBasePtrs()
{
    uintptr_t basePtrs = reinterpret_cast<uintptr_t>(g_gigacageBasePtrs);
    // We might only get page size alignment, but that's also the minimum we need.
    RELEASE_BASSERT(!(basePtrs & (vmPageSize() - 1)));
    mprotect(g_gigacageBasePtrs, GIGACAGE_BASE_PTRS_SIZE, PROT_READ);
}

void unprotectGigacageBasePtrs()
{
    mprotect(g_gigacageBasePtrs, GIGACAGE_BASE_PTRS_SIZE, PROT_READ | PROT_WRITE);
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

} // anonymous namespce

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
    PrimitiveDisableCallbacks(std::lock_guard<StaticMutex>&) { }
    
    Vector<Callback> callbacks;
};

void ensureGigacage()
{
#if GIGACAGE_ENABLED
    static std::once_flag onceFlag;
    std::call_once(
        onceFlag,
        [] {
            if (!shouldBeEnabled())
                return;
            
            forEachKind(
                [&] (Kind kind) {
                    // FIXME: Randomize where this goes.
                    // https://bugs.webkit.org/show_bug.cgi?id=175245
                    basePtr(kind) = tryVMAllocate(alignment(kind), totalSize(kind));
                    if (!basePtr(kind)) {
                        fprintf(stderr, "FATAL: Could not allocate %s gigacage.\n", name(kind));
                        BCRASH();
                    }
                    
                    vmDeallocatePhysicalPages(basePtr(kind), totalSize(kind));
                });
            
            protectGigacageBasePtrs();
        });
#endif // GIGACAGE_ENABLED
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
    std::unique_lock<StaticMutex> lock(PerProcess<PrimitiveDisableCallbacks>::mutex());
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
    std::unique_lock<StaticMutex> lock(PerProcess<PrimitiveDisableCallbacks>::mutex());
    callbacks.callbacks.push(Callback(function, argument));
}

void removePrimitiveDisableCallback(void (*function)(void*), void* argument)
{
    PrimitiveDisableCallbacks& callbacks = *PerProcess<PrimitiveDisableCallbacks>::get();
    std::unique_lock<StaticMutex> lock(PerProcess<PrimitiveDisableCallbacks>::mutex());
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
    return GIGACAGE_ENABLED && !PerProcess<Environment>::get()->isDebugHeapEnabled();
}

} // namespace Gigacage



