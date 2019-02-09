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

#include "config.h"
#include "WheelEventTestTrigger.h"

#include "Logging.h"

#if !LOG_DISABLED
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#endif

namespace WebCore {

WheelEventTestTrigger::WheelEventTestTrigger()
    : m_testTriggerTimer(RunLoop::current(), this, &WheelEventTestTrigger::triggerTestTimerFired)
{
}

void WheelEventTestTrigger::clearAllTestDeferrals()
{
    std::lock_guard<Lock> lock(m_testTriggerMutex);
    m_deferTestTriggerReasons.clear();
    m_testNotificationCallback = nullptr;
    m_testTriggerTimer.stop();
    LOG(WheelEventTestTriggers, "      (=) WheelEventTestTrigger::clearAllTestDeferrals: cleared all test state.");
}

void WheelEventTestTrigger::setTestCallbackAndStartNotificationTimer(WTF::Function<void()>&& functionCallback)
{
    {
        std::lock_guard<Lock> lock(m_testTriggerMutex);
        m_testNotificationCallback = WTFMove(functionCallback);
    }
    
    if (!m_testTriggerTimer.isActive())
        m_testTriggerTimer.startRepeating(1_s / 60.);
}

void WheelEventTestTrigger::deferTestsForReason(ScrollableAreaIdentifier identifier, DeferTestTriggerReason reason)
{
    std::lock_guard<Lock> lock(m_testTriggerMutex);
    auto it = m_deferTestTriggerReasons.find(identifier);
    if (it == m_deferTestTriggerReasons.end())
        it = m_deferTestTriggerReasons.add(identifier, DeferTestTriggerReasonSet()).iterator;
    
    LOG(WheelEventTestTriggers, "      (=) WheelEventTestTrigger::deferTestsForReason: id=%p, reason=%d", identifier, reason);
    it->value.insert(reason);
}

void WheelEventTestTrigger::removeTestDeferralForReason(ScrollableAreaIdentifier identifier, DeferTestTriggerReason reason)
{
    std::lock_guard<Lock> lock(m_testTriggerMutex);
    auto it = m_deferTestTriggerReasons.find(identifier);
    if (it == m_deferTestTriggerReasons.end())
        return;

    LOG(WheelEventTestTriggers, "      (=) WheelEventTestTrigger::removeTestDeferralForReason: id=%p, reason=%d", identifier, reason);
    it->value.erase(reason);
    
    if (it->value.empty())
        m_deferTestTriggerReasons.remove(it);
}

#if !LOG_DISABLED

static void dumpState(WTF::HashMap<WheelEventTestTrigger::ScrollableAreaIdentifier, WheelEventTestTrigger::DeferTestTriggerReasonSet> reasons)
{
    LOG(WheelEventTestTriggers, "   WheelEventTestTrigger::dumpState:");
    for (const auto& scrollRegion : reasons) {
        LOG(WheelEventTestTriggers, "   For scroll region %p", scrollRegion.key);
        StringBuilder reasons;
        for (const auto& reason : scrollRegion.value) {
            if (!reasons.isEmpty())
                reasons.appendLiteral(", ");
            reasons.appendNumber(static_cast<unsigned>(reason));
        }
        LOG(WheelEventTestTriggers, "     Reasons: %s", reasons.toString().utf8().data());
    }
}

#endif
    
void WheelEventTestTrigger::triggerTestTimerFired()
{
    WTF::Function<void()> functionCallback;

    {
        std::lock_guard<Lock> lock(m_testTriggerMutex);
        if (!m_deferTestTriggerReasons.isEmpty()) {
#if !LOG_DISABLED
            if (isLogChannelEnabled("WheelEventTestTriggers"))
                dumpState(m_deferTestTriggerReasons);
#endif
            return;
        }

        functionCallback = WTFMove(m_testNotificationCallback);
    }

    m_testTriggerTimer.stop();

    LOG(WheelEventTestTriggers, "  WheelEventTestTrigger::triggerTestTimerFired: FIRING TEST");
    if (functionCallback)
        functionCallback();
}

}
