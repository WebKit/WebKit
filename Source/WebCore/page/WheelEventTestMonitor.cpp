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

#include "config.h"
#include "WheelEventTestMonitor.h"

#include "Logging.h"
#include "Page.h"
#include <wtf/OptionSet.h>
#include <wtf/RunLoop.h>
#include <wtf/text/TextStream.h>

#if !LOG_DISABLED
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#endif

namespace WebCore {

WheelEventTestMonitor::WheelEventTestMonitor(Page& page)
    : m_page(page)
{
}

void WheelEventTestMonitor::clearAllTestDeferrals()
{
    Locker locker { m_lock };

    ASSERT(isMainThread());
    m_deferCompletionReasons.clear();
    m_completionCallback = nullptr;
    m_everHadDeferral = false;
    m_receivedWheelEndOrCancel = false;
    m_receivedMomentumEnd = false;
    LOG_WITH_STREAM(WheelEventTestMonitor, stream << "  WheelEventTestMonitor::clearAllTestDeferrals: cleared all test state.");
}

void WheelEventTestMonitor::setTestCallbackAndStartMonitoring(bool expectWheelEndOrCancel, bool expectMomentumEnd, Function<void()>&& functionCallback)
{
    Locker locker { m_lock };

    ASSERT(isMainThread());
    m_completionCallback = WTFMove(functionCallback);
#if ENABLE(KINETIC_SCROLLING)
    m_expectWheelEndOrCancel = expectWheelEndOrCancel;
    m_expectMomentumEnd = expectMomentumEnd;
#else
    UNUSED_PARAM(expectWheelEndOrCancel);
    UNUSED_PARAM(expectMomentumEnd);
#endif

    m_page.scheduleRenderingUpdate(RenderingUpdateStep::WheelEventMonitorCallbacks);

    LOG_WITH_STREAM(WheelEventTestMonitor, stream << "  WheelEventTestMonitor::setTestCallbackAndStartMonitoring - expect end/cancel " << expectWheelEndOrCancel << ", expect momentum end " << expectMomentumEnd);
}

void WheelEventTestMonitor::deferForReason(ScrollableAreaIdentifier identifier, DeferReason reason)
{
    Locker locker { m_lock };

    m_deferCompletionReasons.ensure(identifier, [] {
        return OptionSet<DeferReason>();
    }).iterator->value.add(reason);

    m_everHadDeferral = true;

    LOG_WITH_STREAM(WheelEventTestMonitor, stream << "      (=) WheelEventTestMonitor::deferForReason: id=" << identifier << ", reason=" << reason);
}

void WheelEventTestMonitor::removeDeferralForReason(ScrollableAreaIdentifier identifier, DeferReason reason)
{
    Locker locker { m_lock };

    auto it = m_deferCompletionReasons.find(identifier);
    if (it == m_deferCompletionReasons.end()) {
        LOG_WITH_STREAM(WheelEventTestMonitor, stream << "      (=) WheelEventTestMonitor::removeDeferralForReason: failed to find defer for id=" << identifier << ", reason=" << reason);
        return;
    }

    LOG_WITH_STREAM(WheelEventTestMonitor, stream << "      (=) WheelEventTestMonitor::removeDeferralForReason: id=" << identifier << ", reason=" << reason);
    it->value.remove(reason);
    
    if (it->value.isEmpty())
        m_deferCompletionReasons.remove(it);

    scheduleCallbackCheck();
}

void WheelEventTestMonitor::receivedWheelEventWithPhases(PlatformWheelEventPhase phase, PlatformWheelEventPhase momentumPhase)
{
#if ENABLE(KINETIC_SCROLLING)
    Locker locker { m_lock };

    LOG_WITH_STREAM(WheelEventTestMonitor, stream << "      (=) WheelEventTestMonitor::receivedWheelEventWithPhases: phase=" << phase << " momentumPhase=" << momentumPhase);

    if (phase == PlatformWheelEventPhase::Ended || phase == PlatformWheelEventPhase::Cancelled)
        m_receivedWheelEndOrCancel = true;

    if (momentumPhase == PlatformWheelEventPhase::Ended)
        m_receivedMomentumEnd = true;
#else
    UNUSED_PARAM(phase);
    UNUSED_PARAM(momentumPhase);
#endif
}

void WheelEventTestMonitor::scheduleCallbackCheck()
{
    ensureOnMainThread([weakPage = WeakPtr { m_page }] {
        if (weakPage)
            weakPage->scheduleRenderingUpdate(RenderingUpdateStep::WheelEventMonitorCallbacks);
    });
}

void WheelEventTestMonitor::checkShouldFireCallbacks()
{
    ASSERT(isMainThread());
    {
        Locker locker { m_lock };

        if (!m_deferCompletionReasons.isEmpty()) {
            LOG_WITH_STREAM(WheelEventTestMonitor, stream << "  WheelEventTestMonitor::checkShouldFireCallbacks - scrolling still active, reasons " << m_deferCompletionReasons);
            return;
        }

        if (!m_everHadDeferral) {
            LOG_WITH_STREAM(WheelEventTestMonitor, stream << "  WheelEventTestMonitor::checkShouldFireCallbacks - have not yet seen any deferral reasons");
            return;
        }
        
        if (m_expectWheelEndOrCancel && !m_receivedWheelEndOrCancel) {
            LOG_WITH_STREAM(WheelEventTestMonitor, stream << "  WheelEventTestMonitor::checkShouldFireCallbacks - have not seen end of of wheel phase");
            return;
        }

        if (m_expectMomentumEnd && !m_receivedMomentumEnd) {
            LOG_WITH_STREAM(WheelEventTestMonitor, stream << "  WheelEventTestMonitor::checkShouldFireCallbacks - have not seen end of of momentum phase");
            return;
        }
    }

    if (auto functionCallback = WTFMove(m_completionCallback)) {
        LOG_WITH_STREAM(WheelEventTestMonitor, stream << "  WheelEventTestMonitor::checkShouldFireCallbacks: scrolling is idle, FIRING TEST");
        functionCallback();
    } else
        LOG_WITH_STREAM(WheelEventTestMonitor, stream << "  WheelEventTestMonitor::checkShouldFireCallbacks - no callback");
}

TextStream& operator<<(TextStream& ts, WheelEventTestMonitor::DeferReason reason)
{
    switch (reason) {
    case WheelEventTestMonitor::HandlingWheelEvent: ts << "handling wheel event"; break;
    case WheelEventTestMonitor::HandlingWheelEventOnMainThread: ts << "handling wheel event on main thread"; break;
    case WheelEventTestMonitor::PostMainThreadWheelEventHandling: ts << "post-main thread event handling"; break;
    case WheelEventTestMonitor::RubberbandInProgress: ts << "rubberbanding"; break;
    case WheelEventTestMonitor::ScrollSnapInProgress: ts << "scroll-snapping"; break;
    case WheelEventTestMonitor::ScrollAnimationInProgress: ts << "scroll animation"; break;
    case WheelEventTestMonitor::ScrollingThreadSyncNeeded: ts << "scrolling thread sync needed"; break;
    case WheelEventTestMonitor::ContentScrollInProgress: ts << "content scrolling"; break;
    case WheelEventTestMonitor::RequestedScrollPosition: ts << "requested scroll position"; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, const WheelEventTestMonitor::ScrollableAreaReasonMap& reasonMap)
{
    for (const auto& regionReasonsPair : reasonMap)
        ts << "   scroll region: " << regionReasonsPair.key << " reasons: " << regionReasonsPair.value;

    return ts;
}

} // namespace WebCore
