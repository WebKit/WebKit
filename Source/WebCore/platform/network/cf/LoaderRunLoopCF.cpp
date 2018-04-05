/*
 * Copyright (C) 2009, 2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "LoaderRunLoopCF.h"

#if USE(CFURLCONNECTION)

#include <CoreFoundation/CoreFoundation.h>
#include <limits>
#include <mutex>
#include <wtf/AutodrainedPool.h>
#include <wtf/Condition.h>
#include <wtf/Lock.h>
#include <wtf/MainThread.h>
#include <wtf/Threading.h>

namespace WebCore {

static CFRunLoopRef loaderRunLoopObject = 0;

static Lock loaderRunLoopMutex;
static StaticCondition loaderRunLoopConditionVariable;

static void emptyPerform(void*) 
{
}

CFRunLoopRef loaderRunLoop()
{
    ASSERT(isMainThread());

    std::unique_lock<Lock> lock(loaderRunLoopMutex);

    if (!loaderRunLoopObject) {
        Thread::create("WebCore: CFNetwork Loader", [] {
            {
                std::lock_guard<Lock> lock(loaderRunLoopMutex);

                loaderRunLoopObject = CFRunLoopGetCurrent();

                // Must add a source to the run loop to prevent CFRunLoopRun() from exiting.
                CFRunLoopSourceContext ctxt = {0, (void*)1 /*must be non-null*/, 0, 0, 0, 0, 0, 0, 0, emptyPerform};
                CFRunLoopSourceRef bogusSource = CFRunLoopSourceCreate(0, 0, &ctxt);
                CFRunLoopAddSource(loaderRunLoopObject, bogusSource, kCFRunLoopDefaultMode);

                loaderRunLoopConditionVariable.notifyOne();
            }

            SInt32 result;
            do {
                AutodrainedPool pool;
                result = CFRunLoopRunInMode(kCFRunLoopDefaultMode, std::numeric_limits<double>::max(), true);
            } while (result != kCFRunLoopRunStopped && result != kCFRunLoopRunFinished);
        });

        loaderRunLoopConditionVariable.wait(lock, [] { return loaderRunLoopObject; });
    }

    return loaderRunLoopObject;
}

} // namespace WebCore

#endif // USE(CFURLCONNECTION)
