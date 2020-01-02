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
#include <wtf/CompletionHandler.h>

namespace WebKit {
    
static const Seconds processSuspensionTimeout { 20_s };

static uint64_t generatePrepareToSuspendRequestID()
{
    static uint64_t prepareToSuspendRequestID = 0;
    return ++prepareToSuspendRequestID;
}

ProcessThrottler::ProcessThrottler(ProcessThrottlerClient& process, bool shouldTakeUIBackgroundAssertion)
    : m_process(process)
    , m_prepareToSuspendTimeoutTimer(RunLoop::main(), this, &ProcessThrottler::prepareToSuspendTimeoutTimerFired)
    , m_shouldTakeUIBackgroundAssertion(shouldTakeUIBackgroundAssertion)
{
}

ProcessThrottler::~ProcessThrottler()
{
    invalidateAllActivities();
}

void ProcessThrottler::addActivity(ForegroundActivity& activity)
{
    m_foregroundActivities.add(&activity);
    updateAssertionIfNeeded();
}

void ProcessThrottler::addActivity(BackgroundActivity& activity)
{
    m_backgroundActivities.add(&activity);
    updateAssertionIfNeeded();
}

void ProcessThrottler::removeActivity(ForegroundActivity& activity)
{
    m_foregroundActivities.remove(&activity);
    updateAssertionIfNeeded();
}

void ProcessThrottler::removeActivity(BackgroundActivity& activity)
{
    m_backgroundActivities.remove(&activity);
    updateAssertionIfNeeded();
}

void ProcessThrottler::invalidateAllActivities()
{
    PROCESSTHROTTLER_RELEASE_LOG("invalidateAllActivities: BEGIN");
    while (!m_foregroundActivities.isEmpty())
        (*m_foregroundActivities.begin())->invalidate();
    while (!m_backgroundActivities.isEmpty())
        (*m_backgroundActivities.begin())->invalidate();
    PROCESSTHROTTLER_RELEASE_LOG("invalidateAllActivities: END");
}
    
AssertionState ProcessThrottler::expectedAssertionState()
{
    if (!m_foregroundActivities.isEmpty())
        return AssertionState::Foreground;
    if (!m_backgroundActivities.isEmpty())
        return AssertionState::Background;
    return AssertionState::Suspended;
}
    
void ProcessThrottler::updateAssertionStateNow()
{
    setAssertionState(expectedAssertionState());
}

void ProcessThrottler::setAssertionState(AssertionState newState)
{
    RELEASE_ASSERT(m_assertion);
    if (m_assertion->state() == newState)
        return;

    PROCESSTHROTTLER_RELEASE_LOG("setAssertionState: Updating process assertion state to %u (foregroundActivities: %u, backgroundActivities: %u)", newState, m_foregroundActivities.size(), m_backgroundActivities.size());
    m_assertion->setState(newState);
    m_process.didSetAssertionState(newState);
}
    
void ProcessThrottler::updateAssertionIfNeeded()
{
    if (!m_assertion)
        return;

    if (shouldBeRunnable()) {
        if (m_assertion->state() == AssertionState::Suspended || m_pendingRequestToSuspendID) {
            if (m_assertion->state() == AssertionState::Suspended)
                PROCESSTHROTTLER_RELEASE_LOG("updateAssertionIfNeeded: sending ProcessDidResume IPC because the process was suspended");
            else
                PROCESSTHROTTLER_RELEASE_LOG("updateAssertionIfNeeded: sending ProcessDidResume IPC because the WebProcess is still processing request to suspend: %" PRIu64, *m_pendingRequestToSuspendID);
            m_process.sendProcessDidResume();
            clearPendingRequestToSuspend();
        }
    } else {
        // If the process is currently runnable but will be suspended then first give it a chance to complete what it was doing
        // and clean up - move it to the background and send it a message to notify. Schedule a timeout so it can't stay running
        // in the background for too long.
        if (m_assertion->state() != AssertionState::Suspended) {
            m_prepareToSuspendTimeoutTimer.startOneShot(processSuspensionTimeout);
            sendPrepareToSuspendIPC(IsSuspensionImminent::No);
            return;
        }
    }

    updateAssertionStateNow();
}

void ProcessThrottler::didConnectToProcess(ProcessID pid)
{
    PROCESSTHROTTLER_RELEASE_LOG_WITH_PID("didConnectToProcess:", pid);
    RELEASE_ASSERT(!m_assertion);

    if (m_shouldTakeUIBackgroundAssertion)
        m_assertion = makeUnique<ProcessAndUIAssertion>(pid, "Web content visibility"_s, expectedAssertionState());
    else
        m_assertion = makeUnique<ProcessAssertion>(pid, "Web content visibility"_s, expectedAssertionState());

    m_processIdentifier = pid;
    m_process.didSetAssertionState(expectedAssertionState());
    m_assertion->setClient(*this);
}
    
void ProcessThrottler::prepareToSuspendTimeoutTimerFired()
{
    PROCESSTHROTTLER_RELEASE_LOG("prepareToSuspendTimeoutTimerFired: Updating process assertion to allow suspension");
    RELEASE_ASSERT(m_pendingRequestToSuspendID);
    updateAssertionStateNow();
}
    
void ProcessThrottler::processReadyToSuspend()
{
    PROCESSTHROTTLER_RELEASE_LOG("processReadyToSuspend: Updating process assertion to allow suspension");

    RELEASE_ASSERT(m_pendingRequestToSuspendID);
    clearPendingRequestToSuspend();

    if (m_assertion->state() != AssertionState::Suspended)
        updateAssertionStateNow();
}

void ProcessThrottler::clearPendingRequestToSuspend()
{
    m_prepareToSuspendTimeoutTimer.stop();
    m_pendingRequestToSuspendID = WTF::nullopt;
}

void ProcessThrottler::sendPrepareToSuspendIPC(IsSuspensionImminent isSuspensionImminent)
{
    PROCESSTHROTTLER_RELEASE_LOG("sendPrepareToSuspendIPC: isSuspensionImminent: %d", isSuspensionImminent == IsSuspensionImminent::Yes);
    if (m_pendingRequestToSuspendID) {
        // Do not send a new PrepareToSuspend IPC for imminent suspension if we've already sent a non-imminent PrepareToSuspend IPC.
        RELEASE_ASSERT(isSuspensionImminent == IsSuspensionImminent::Yes);
        PROCESSTHROTTLER_RELEASE_LOG("sendPrepareToSuspendIPC: Not sending PrepareToSuspend() IPC because there is already one in flight (%" PRIu64 ")", *m_pendingRequestToSuspendID);
    } else {
        m_pendingRequestToSuspendID = generatePrepareToSuspendRequestID();
        PROCESSTHROTTLER_RELEASE_LOG("sendPrepareToSuspendIPC: Sending PrepareToSuspend(%" PRIu64 ", isSuspensionImminent: %d) IPC", *m_pendingRequestToSuspendID, isSuspensionImminent == IsSuspensionImminent::Yes);
        m_process.sendPrepareToSuspend(isSuspensionImminent, [this, weakThis = makeWeakPtr(*this), requestToSuspendID = *m_pendingRequestToSuspendID]() mutable {
            if (weakThis && m_pendingRequestToSuspendID && *m_pendingRequestToSuspendID == requestToSuspendID)
                processReadyToSuspend();
        });
    }

    setAssertionState(isSuspensionImminent == IsSuspensionImminent::Yes ? AssertionState::Suspended : AssertionState::Background);
}

void ProcessThrottler::uiAssertionWillExpireImminently()
{
    PROCESSTHROTTLER_RELEASE_LOG("uiAssertionWillExpireImminently:");
    sendPrepareToSuspendIPC(IsSuspensionImminent::Yes);
    invalidateAllActivities();
    m_prepareToSuspendTimeoutTimer.stop();
}

bool ProcessThrottler::isValidBackgroundActivity(const ProcessThrottler::ActivityVariant& activity)
{
    if (!WTF::holds_alternative<UniqueRef<ProcessThrottler::BackgroundActivity>>(activity))
        return false;
    return WTF::get<UniqueRef<ProcessThrottler::BackgroundActivity>>(activity)->isValid();
}

bool ProcessThrottler::isValidForegroundActivity(const ProcessThrottler::ActivityVariant& activity)
{
    if (!WTF::holds_alternative<UniqueRef<ProcessThrottler::ForegroundActivity>>(activity))
        return false;
    return WTF::get<UniqueRef<ProcessThrottler::ForegroundActivity>>(activity)->isValid();
}

} // namespace WebKit
