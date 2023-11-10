/*
 * Copyright (C) 2011, 2014 Apple Inc. All rights reserved.
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

#import "config.h"
#import "RunLoopObserver.h"

#import <CoreFoundation/CoreFoundation.h>

namespace WebCore {

static constexpr CFIndex cfRunLoopOrder(RunLoopObserver::WellKnownOrder order)
{
    constexpr auto coreAnimationCommit = 2000000;
    switch (order) {
    case RunLoopObserver::WellKnownOrder::GraphicsCommit:
        return coreAnimationCommit;
    case RunLoopObserver::WellKnownOrder::RenderingUpdate:
        return coreAnimationCommit - 1;
    case RunLoopObserver::WellKnownOrder::ActivityStateChange:
        return coreAnimationCommit - 2;
    case RunLoopObserver::WellKnownOrder::InspectorFrameBegin:
        return 0;
    case RunLoopObserver::WellKnownOrder::InspectorFrameEnd:
        return coreAnimationCommit + 1;
    case RunLoopObserver::WellKnownOrder::PostRenderingUpdate:
        return coreAnimationCommit + 2;
    case RunLoopObserver::WellKnownOrder::DisplayRefreshMonitor:
        return 1;
    }
    ASSERT_NOT_REACHED();
    return coreAnimationCommit;
}

static constexpr CFRunLoopActivity cfRunLoopActivity(OptionSet<RunLoopObserver::Activity> activity)
{
    CFRunLoopActivity cfActivity { 0 };
    if (activity.contains(RunLoopObserver::Activity::BeforeWaiting))
        cfActivity |= kCFRunLoopBeforeWaiting;
    if (activity.contains(RunLoopObserver::Activity::Entry))
        cfActivity |= kCFRunLoopEntry;
    if (activity.contains(RunLoopObserver::Activity::Exit))
        cfActivity |= kCFRunLoopExit;
    if (activity.contains(RunLoopObserver::Activity::AfterWaiting))
        cfActivity |= kCFRunLoopAfterWaiting;
    return cfActivity;
}

void RunLoopObserver::runLoopObserverFired(CFRunLoopObserverRef, CFRunLoopActivity, void* context)
{
    static_cast<RunLoopObserver*>(context)->runLoopObserverFired();
}

void RunLoopObserver::schedule(PlatformRunLoop runLoop, OptionSet<Activity> activity)
{
    if (!runLoop)
        runLoop = CFRunLoopGetCurrent();

    // Make sure we wake up the loop or the observer could be delayed until some other source fires.
    CFRunLoopWakeUp(runLoop);

    if (m_runLoopObserver)
        return;

    CFRunLoopObserverContext context = { 0, this, 0, 0, 0 };
    m_runLoopObserver = adoptCF(CFRunLoopObserverCreate(kCFAllocatorDefault, cfRunLoopActivity(activity), isRepeating(), cfRunLoopOrder(m_order), runLoopObserverFired, &context));

    CFRunLoopAddObserver(runLoop, m_runLoopObserver.get(), kCFRunLoopCommonModes);
}

void RunLoopObserver::invalidate()
{
    if (m_runLoopObserver) {
        CFRunLoopObserverInvalidate(m_runLoopObserver.get());
        m_runLoopObserver = nullptr;   
    }
}

bool RunLoopObserver::isScheduled() const
{
    return m_runLoopObserver;
}

} // namespace WebCore
