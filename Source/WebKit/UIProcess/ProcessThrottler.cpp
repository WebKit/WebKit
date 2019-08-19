/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO , PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ProcessThrottler.h"

#include "Logging.h"
#include "ProcessThrottlerClient.h"

namespace WebKit {
    
static const Seconds processSuspensionTimeout { 30_s };
    
ProcessThrottler::ProcessThrottler(ProcessThrottlerClient& process, bool shouldTakeUIBackgroundAssertion)
    : m_process(process)
    , m_suspendTimer(RunLoop::main(), this, &ProcessThrottler::suspendTimerFired)
    , m_foregroundCounter([this](RefCounterEvent) { updateAssertion(); })
    , m_backgroundCounter([this](RefCounterEvent) { updateAssertion(); })
    , m_shouldTakeUIBackgroundAssertion(shouldTakeUIBackgroundAssertion)
{
}
    
AssertionState ProcessThrottler::assertionState()
{
    ASSERT(!m_suspendTimer.isActive());
    
    if (m_foregroundCounter.value())
        return AssertionState::Foreground;
    if (m_backgroundCounter.value())
        return AssertionState::Background;
    return AssertionState::Suspended;
}
    
void ProcessThrottler::updateAssertionNow()
{
    m_suspendTimer.stop();
    if (m_assertion) {
        auto newState = assertionState();
        if (m_assertion->state() == newState)
            return;
        RELEASE_LOG(ProcessSuspension, "%p - ProcessThrottler::updateAssertionNow() updating process assertion state to %u (foregroundActivities: %lu, backgroundActivities: %lu)", this, newState, m_foregroundCounter.value(), m_backgroundCounter.value());
        m_assertion->setState(newState);
        m_process.didSetAssertionState(newState);
    }
}
    
void ProcessThrottler::updateAssertion()
{
    bool shouldBeRunnable = this->shouldBeRunnable();

    // If the process is currently runnable but will be suspended then first give it a chance to complete what it was doing
    // and clean up - move it to the background and send it a message to notify. Schedule a timeout so it can't stay running
    // in the background for too long.
    if (m_assertion && m_assertion->state() != AssertionState::Suspended && !shouldBeRunnable) {
        ++m_suspendMessageCount;
        RELEASE_LOG(ProcessSuspension, "%p - ProcessThrottler::updateAssertion() sending PrepareToSuspend IPC", this);
        m_process.sendPrepareToSuspend();
        m_suspendTimer.startOneShot(processSuspensionTimeout);
        m_assertion->setState(AssertionState::Background);
        m_process.didSetAssertionState(AssertionState::Background);
        return;
    }

    if (shouldBeRunnable) {
        // If we're currently waiting for the Web process to do suspension cleanup, but no longer need to be suspended, tell the Web process to cancel the cleanup.
        if (m_suspendTimer.isActive())
            m_process.sendCancelPrepareToSuspend();

        if ((m_assertion && m_assertion->state() == AssertionState::Suspended) || m_uiAssertionExpired) {
            m_process.sendProcessDidResume();
            m_uiAssertionExpired = false;
        }
    }

    updateAssertionNow();
}

void ProcessThrottler::didConnectToProcess(ProcessID pid)
{
    RELEASE_LOG(ProcessSuspension, "%p - ProcessThrottler::didConnectToProcess(%d)", this, pid);

    m_suspendTimer.stop();
    if (m_shouldTakeUIBackgroundAssertion)
        m_assertion = makeUnique<ProcessAndUIAssertion>(pid, "Web content visibility"_s, assertionState());
    else
        m_assertion = makeUnique<ProcessAssertion>(pid, "Web content visibility"_s, assertionState());
    m_process.didSetAssertionState(assertionState());
    m_assertion->setClient(*this);
}
    
void ProcessThrottler::suspendTimerFired()
{
    updateAssertionNow();
}
    
void ProcessThrottler::processReadyToSuspend()
{
    if (!--m_suspendMessageCount)
        updateAssertionNow();
    ASSERT(m_suspendMessageCount >= 0);
}

void ProcessThrottler::didCancelProcessSuspension()
{
    if (!--m_suspendMessageCount)
        updateAssertionNow();
    ASSERT(m_suspendMessageCount >= 0);
}

void ProcessThrottler::uiAssertionWillExpireImminently()
{
    m_process.sendProcessWillSuspendImminently();
    m_uiAssertionExpired = true;
}

}
