/*
 * Copyright (C) 2015-2019 Apple Inc.  All rights reserved.
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

#include "PlatformWheelEvent.h"
#include <functional>
#include <wtf/Function.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

class Page;

class WheelEventTestMonitor : public ThreadSafeRefCounted<WheelEventTestMonitor> {
    WTF_MAKE_NONCOPYABLE(WheelEventTestMonitor); WTF_MAKE_FAST_ALLOCATED;
public:
    WheelEventTestMonitor(Page&);

    WEBCORE_EXPORT void setTestCallbackAndStartMonitoring(bool expectWheelEndOrCancel, bool expectMomentumEnd, WTF::Function<void()>&&);
    WEBCORE_EXPORT void clearAllTestDeferrals();
    
    enum DeferReason {
        HandlingWheelEvent                  = 1 << 0,
        HandlingWheelEventOnMainThread      = 1 << 1,
        PostMainThreadWheelEventHandling    = 1 << 2,
        RubberbandInProgress                = 1 << 3,
        ScrollSnapInProgress                = 1 << 4,
        ScrollingThreadSyncNeeded           = 1 << 5,
        ContentScrollInProgress             = 1 << 6,
        RequestedScrollPosition             = 1 << 7,
    };
    typedef const void* ScrollableAreaIdentifier;

    WEBCORE_EXPORT void receivedWheelEvent(const PlatformWheelEvent&);
    WEBCORE_EXPORT void deferForReason(ScrollableAreaIdentifier, DeferReason);
    WEBCORE_EXPORT void removeDeferralForReason(ScrollableAreaIdentifier, DeferReason);
    
    void checkShouldFireCallbacks();

    using ScrollableAreaReasonMap = WTF::HashMap<ScrollableAreaIdentifier, OptionSet<DeferReason>>;

private:
    void scheduleCallbackCheck();

    WTF::Function<void()> m_completionCallback;
    Page& m_page;

    Lock m_lock;
    ScrollableAreaReasonMap m_deferCompletionReasons WTF_GUARDED_BY_LOCK(m_lock);
    bool m_expectWheelEndOrCancel WTF_GUARDED_BY_LOCK(m_lock) { false };
    bool m_receivedWheelEndOrCancel WTF_GUARDED_BY_LOCK(m_lock) { false };
    bool m_expectMomentumEnd WTF_GUARDED_BY_LOCK(m_lock) { false };
    bool m_receivedMomentumEnd WTF_GUARDED_BY_LOCK(m_lock) { false };
    bool m_everHadDeferral WTF_GUARDED_BY_LOCK(m_lock) { false };
};

class WheelEventTestMonitorCompletionDeferrer {
public:
    WheelEventTestMonitorCompletionDeferrer(WheelEventTestMonitor* monitor, WheelEventTestMonitor::ScrollableAreaIdentifier identifier, WheelEventTestMonitor::DeferReason reason)
        : m_monitor(monitor)
        , m_identifier(identifier)
        , m_reason(reason)
    {
        if (m_monitor)
            m_monitor->deferForReason(m_identifier, m_reason);
    }
    
    WheelEventTestMonitorCompletionDeferrer(WheelEventTestMonitorCompletionDeferrer&& other)
        : m_monitor(WTFMove(other.m_monitor))
        , m_identifier(other.m_identifier)
        , m_reason(other.m_reason)
    {
    }

    ~WheelEventTestMonitorCompletionDeferrer()
    {
        if (m_monitor)
            m_monitor->removeDeferralForReason(m_identifier, m_reason);
    }

private:
    RefPtr<WheelEventTestMonitor> m_monitor;
    WheelEventTestMonitor::ScrollableAreaIdentifier m_identifier;
    WheelEventTestMonitor::DeferReason m_reason;
};

WTF::TextStream& operator<<(WTF::TextStream&, WheelEventTestMonitor::DeferReason);
WTF::TextStream& operator<<(WTF::TextStream&, const WheelEventTestMonitor::ScrollableAreaReasonMap&);

} // namespace WebCore
