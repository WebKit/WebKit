/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GCActivityCallback.h"

#include "APIShims.h"
#include "Heap.h"
#include "JSGlobalData.h"
#include "JSLock.h"
#include "JSObject.h"
#include "ScopeChain.h"
#include <wtf/RetainPtr.h>
#include <wtf/WTFThreadData.h>

#if !USE(CF)
#error "This file should only be used on CF platforms."
#endif

namespace JSC {

struct DefaultGCActivityCallbackPlatformData {
    static void timerDidFire(CFRunLoopTimerRef, void *info);

    RetainPtr<CFRunLoopTimerRef> timer;
    RetainPtr<CFRunLoopRef> runLoop;
    CFRunLoopTimerContext context;
    double delay;
    DefaultGCActivityCallback* thisObject;
    JSGlobalData* globalData;
    Mutex shutdownMutex;
};

const double gcTimeSlicePerMB = 0.01; // Percentage of CPU time we will spend to reclaim 1 MB
const double maxGCTimeSlice = 0.05; // The maximum amount of CPU time we want to use for opportunistic timer-triggered collections.
const double timerSlop = 2.0; // Fudge factor to avoid performance cost of resetting timer.
const double pagingTimeOut = 0.1; // Time in seconds to allow opportunistic timer to iterate over all blocks to see if the Heap is paged out.
const CFTimeInterval decade = 60 * 60 * 24 * 365 * 10;
const CFTimeInterval hour = 60 * 60;

void DefaultGCActivityCallbackPlatformData::timerDidFire(CFRunLoopTimerRef, void *info)
{
    DefaultGCActivityCallbackPlatformData* d = static_cast<DefaultGCActivityCallbackPlatformData*>(info);
    d->shutdownMutex.lock();
    if (!d->globalData) {
        d->shutdownMutex.unlock();
        delete d->thisObject;
        return;
    }
    {
        // We don't ref here to prevent us from resurrecting the ref count of a "dead" JSGlobalData.
        APIEntryShim shim(d->globalData, APIEntryShimWithoutLock::DontRefGlobalData);

        Heap* heap = &d->globalData->heap;
#if !PLATFORM(IOS)
        double startTime = WTF::monotonicallyIncreasingTime();
        if (heap->isPagedOut(startTime + pagingTimeOut)) {
            heap->activityCallback()->cancel();
            heap->increaseLastGCLength(pagingTimeOut);
            d->shutdownMutex.unlock();
            return;
        }
#endif
        heap->collectAllGarbage();
    }
    d->shutdownMutex.unlock();
}

DefaultGCActivityCallback::DefaultGCActivityCallback(Heap* heap)
{
    commonConstructor(heap, CFRunLoopGetCurrent());
}

DefaultGCActivityCallback::DefaultGCActivityCallback(Heap* heap, CFRunLoopRef runLoop)
{
    commonConstructor(heap, runLoop);
}

DefaultGCActivityCallback::~DefaultGCActivityCallback()
{
    CFRunLoopRemoveTimer(d->runLoop.get(), d->timer.get(), kCFRunLoopCommonModes);
    CFRunLoopTimerInvalidate(d->timer.get());
}

void DefaultGCActivityCallback::commonConstructor(Heap* heap, CFRunLoopRef runLoop)
{
    d = adoptPtr(new DefaultGCActivityCallbackPlatformData);

    memset(&d->context, 0, sizeof(CFRunLoopTimerContext));
    d->context.info = d.get();
    d->thisObject = this;
    d->globalData = heap->globalData();
    d->runLoop = runLoop;
    d->timer.adoptCF(CFRunLoopTimerCreate(0, decade, decade, 0, 0, DefaultGCActivityCallbackPlatformData::timerDidFire, &d->context));
    d->delay = decade;
    CFRunLoopAddTimer(d->runLoop.get(), d->timer.get(), kCFRunLoopCommonModes);
}

void DefaultGCActivityCallback::invalidate()
{
    d->globalData = 0;
    // We set the next fire date in the distant past to cause the timer to immediately fire so that
    // the timer on the remote thread realizes that the VM is shutting down.
    CFRunLoopTimerSetNextFireDate(d->timer.get(), CFAbsoluteTimeGetCurrent() - decade);
}

void DefaultGCActivityCallback::didStartVMShutdown()
{
    if (CFRunLoopGetCurrent() == d->runLoop.get()) {
        invalidate();
        delete this;
        return;
    }
    ASSERT(!d->globalData->apiLock().currentThreadIsHoldingLock());
    MutexLocker locker(d->shutdownMutex);
    invalidate();
}

static void scheduleTimer(DefaultGCActivityCallbackPlatformData* d, double newDelay)
{
    if (newDelay * timerSlop > d->delay)
        return;
    double delta = d->delay - newDelay;
    d->delay = newDelay;
    CFRunLoopTimerSetNextFireDate(d->timer.get(), CFRunLoopTimerGetNextFireDate(d->timer.get()) - delta);
}

static void cancelTimer(DefaultGCActivityCallbackPlatformData* d)
{
    d->delay = decade;
    CFRunLoopTimerSetNextFireDate(d->timer.get(), CFAbsoluteTimeGetCurrent() + decade);
}

void DefaultGCActivityCallback::didAllocate(size_t bytes)
{
    // The first byte allocated in an allocation cycle will report 0 bytes to didAllocate. 
    // We pretend it's one byte so that we don't ignore this allocation entirely.
    if (!bytes)
        bytes = 1;
    Heap* heap = &static_cast<DefaultGCActivityCallbackPlatformData*>(d->context.info)->globalData->heap;
    double gcTimeSlice = std::min((static_cast<double>(bytes) / MB) * gcTimeSlicePerMB, maxGCTimeSlice);
    double newDelay = heap->lastGCLength() / gcTimeSlice;
    scheduleTimer(d.get(), newDelay);
}

void DefaultGCActivityCallback::willCollect()
{
    cancelTimer(d.get());
}

void DefaultGCActivityCallback::synchronize()
{
    if (CFRunLoopGetCurrent() == d->runLoop.get())
        return;
    CFRunLoopRemoveTimer(d->runLoop.get(), d->timer.get(), kCFRunLoopCommonModes);
    d->runLoop = CFRunLoopGetCurrent();
    CFRunLoopAddTimer(d->runLoop.get(), d->timer.get(), kCFRunLoopCommonModes);
}

void DefaultGCActivityCallback::cancel()
{
    cancelTimer(d.get());
}

}
