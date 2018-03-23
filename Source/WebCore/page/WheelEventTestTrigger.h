/*
 * Copyright (C) 2015 Apple Inc.  All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

#pragma once

#include <functional>
#include <wtf/Function.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/RunLoop.h>
#include <wtf/StdSet.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

class WheelEventTestTrigger : public ThreadSafeRefCounted<WheelEventTestTrigger> {
    WTF_MAKE_NONCOPYABLE(WheelEventTestTrigger); WTF_MAKE_FAST_ALLOCATED;
public:
    WheelEventTestTrigger();

    WEBCORE_EXPORT void setTestCallbackAndStartNotificationTimer(WTF::Function<void()>&&);
    WEBCORE_EXPORT void clearAllTestDeferrals();
    
    enum DeferTestTriggerReason {
        RubberbandInProgress,
        ScrollSnapInProgress,
        ScrollingThreadSyncNeeded,
        ContentScrollInProgress
    };
    using DeferTestTriggerReasonSet = StdSet<DeferTestTriggerReason>;
    typedef const void* ScrollableAreaIdentifier;
    void WEBCORE_EXPORT deferTestsForReason(ScrollableAreaIdentifier, DeferTestTriggerReason);
    void WEBCORE_EXPORT removeTestDeferralForReason(ScrollableAreaIdentifier, DeferTestTriggerReason);
    void triggerTestTimerFired();

private:
    WTF::Function<void()> m_testNotificationCallback;
    RunLoop::Timer<WheelEventTestTrigger> m_testTriggerTimer;
    mutable Lock m_testTriggerMutex;
    WTF::HashMap<ScrollableAreaIdentifier, DeferTestTriggerReasonSet> m_deferTestTriggerReasons;
};

} // namespace WebCore
