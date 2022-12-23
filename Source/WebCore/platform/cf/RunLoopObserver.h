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

#pragma once

#include <CoreFoundation/CoreFoundation.h>
#include <wtf/Function.h>
#include <wtf/Noncopyable.h>
#include <wtf/RetainPtr.h>

namespace WebCore {

class RunLoopObserver {
    WTF_MAKE_NONCOPYABLE(RunLoopObserver); WTF_MAKE_FAST_ALLOCATED;
public:
    typedef Function<void ()> RunLoopObserverCallback;

    RunLoopObserver(CFIndex order, RunLoopObserverCallback&& callback)
        : m_order(order)
        , m_callback(WTFMove(callback))
    { }

    WEBCORE_EXPORT ~RunLoopObserver();

    WEBCORE_EXPORT void schedule(CFRunLoopRef = nullptr, CFRunLoopActivity = kCFRunLoopBeforeWaiting | kCFRunLoopExit);
    WEBCORE_EXPORT void invalidate();

    bool isScheduled() const { return m_runLoopObserver; }

    enum class WellKnownRunLoopOrders : CFIndex {
        CoreAnimationCommit     = 2000000,
        RenderingUpdate         = CoreAnimationCommit - 1,
        ActivityStateChange     = CoreAnimationCommit - 2,
        InspectorFrameBegin     = 0,
        InspectorFrameEnd       = CoreAnimationCommit + 1,
        PostRenderingUpdate     = CoreAnimationCommit + 2,
    };

protected:
    void runLoopObserverFired();

private:
    static void runLoopObserverFired(CFRunLoopObserverRef, CFRunLoopActivity, void* context);

    CFIndex m_order;
    RunLoopObserverCallback m_callback;
    RetainPtr<CFRunLoopObserverRef> m_runLoopObserver;
};

} // namespace WebCore

