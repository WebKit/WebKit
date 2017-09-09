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

// FIXME: Ask dyld to put this in its own page, and mprotect the page after we ensure the gigacage.
// https://bugs.webkit.org/show_bug.cgi?id=174972
void* g_primitiveGigacageBasePtr;
void* g_jsValueGigacageBasePtr;

using namespace bmalloc;

namespace Gigacage {

static bool s_isDisablingPrimitiveGigacageDisabled;

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
                    basePtr(kind) = tryVMAllocate(GIGACAGE_SIZE, GIGACAGE_SIZE + GIGACAGE_RUNWAY);
                    if (!basePtr(kind)) {
                        fprintf(stderr, "FATAL: Could not allocate %s gigacage.\n", name(kind));
                        BCRASH();
                    }
                    
                    vmDeallocatePhysicalPages(basePtr(kind), GIGACAGE_SIZE + GIGACAGE_RUNWAY);
                });
        });
#endif // GIGACAGE_ENABLED
}

void disablePrimitiveGigacage()
{
    ensureGigacage();
    if (!g_primitiveGigacageBasePtr) {
        // It was never enabled. That means that we never even saved any callbacks. Or, we had already disabled
        // it before, and already called the callbacks.
        return;
    }
    
    PrimitiveDisableCallbacks& callbacks = *PerProcess<PrimitiveDisableCallbacks>::get();
    std::unique_lock<StaticMutex> lock(PerProcess<PrimitiveDisableCallbacks>::mutex());
    for (Callback& callback : callbacks.callbacks)
        callback.function(callback.argument);
    callbacks.callbacks.shrink(0);
    g_primitiveGigacageBasePtr = nullptr;
}

void addPrimitiveDisableCallback(void (*function)(void*), void* argument)
{
    ensureGigacage();
    if (!g_primitiveGigacageBasePtr) {
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



