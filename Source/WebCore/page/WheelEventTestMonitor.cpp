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
#include <wtf/OptionSet.h>
#include <wtf/text/TextStream.h>

#if !LOG_DISABLED
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#endif

namespace WebCore {

WheelEventTestMonitor::WheelEventTestMonitor()
    : m_testForCompletionTimer(RunLoop::current(), this, &WheelEventTestMonitor::triggerTestTimerFired)
{
}

void WheelEventTestMonitor::clearAllTestDeferrals()
{
    ASSERT(isMainThread());
    m_deferCompletionReasons.clear();
    m_completionCallback = nullptr;
    m_testForCompletionTimer.stop();
    LOG_WITH_STREAM(WheelEventTestMonitor, stream << "      (=) WheelEventTestMonitor::clearAllTestDeferrals: cleared all test state.");
}

void WheelEventTestMonitor::setTestCallbackAndStartNotificationTimer(WTF::Function<void()>&& functionCallback)
{
    ASSERT(isMainThread());
    m_completionCallback = WTFMove(functionCallback);
    
    if (!m_testForCompletionTimer.isActive())
        m_testForCompletionTimer.startRepeating(1_s / 60.);
}

void WheelEventTestMonitor::deferForReason(ScrollableAreaIdentifier identifier, DeferReason reason)
{
    ASSERT(isMainThread());
    m_deferCompletionReasons.ensure(identifier, [] {
        return OptionSet<DeferReason>();
    }).iterator->value.add(reason);
    
    LOG_WITH_STREAM(WheelEventTestMonitor, stream << "      (=) WheelEventTestMonitor::deferForReason: id=" << identifier << ", reason=" << reason);
}

void WheelEventTestMonitor::removeDeferralForReason(ScrollableAreaIdentifier identifier, DeferReason reason)
{
    ASSERT(isMainThread());
    auto it = m_deferCompletionReasons.find(identifier);
    if (it == m_deferCompletionReasons.end())
        return;

    LOG_WITH_STREAM(WheelEventTestMonitor, stream << "      (=) WheelEventTestMonitor::removeDeferralForReason: id=" << identifier << ", reason=" << reason);
    it->value.remove(reason);
    
    if (it->value.isEmpty())
        m_deferCompletionReasons.remove(it);
}
    
void WheelEventTestMonitor::triggerTestTimerFired()
{
    ASSERT(isMainThread());
    if (!m_deferCompletionReasons.isEmpty()) {
        LOG_WITH_STREAM(WheelEventTestMonitor, stream << "  WheelEventTestMonitor::triggerTestTimerFired - scrolling still active, reasons " << m_deferCompletionReasons);
        return;
    }

    auto functionCallback = WTFMove(m_completionCallback);
    m_testForCompletionTimer.stop();

    LOG_WITH_STREAM(WheelEventTestMonitor, stream << "  WheelEventTestMonitor::triggerTestTimerFired: scrolling is idle, FIRING TEST");
    if (functionCallback)
        functionCallback();
}

TextStream& operator<<(TextStream& ts, WheelEventTestMonitor::DeferReason reason)
{
    switch (reason) {
    case WheelEventTestMonitor::HandlingWheelEvent: ts << "handling wheel event"; break;
    case WheelEventTestMonitor::RubberbandInProgress: ts << "rubberbanding"; break;
    case WheelEventTestMonitor::ScrollSnapInProgress: ts << "scroll-snapping"; break;
    case WheelEventTestMonitor::ScrollingThreadSyncNeeded: ts << "scrolling thread sync needed"; break;
    case WheelEventTestMonitor::ContentScrollInProgress: ts << "content scrolling"; break;
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
