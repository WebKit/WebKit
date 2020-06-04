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
    
ProcessAssertionType ProcessThrottler::expectedAssertionType()
{
    if (!m_foregroundActivities.isEmpty())
        return ProcessAssertionType::Foreground;
    if (!m_backgroundActivities.isEmpty())
        return ProcessAssertionType::Background;
    return ProcessAssertionType::Suspended;
}
    
void ProcessThrottler::updateAssertionTypeNow()
{
    setAssertionType(expectedAssertionType());
}

String ProcessThrottler::assertionName(ProcessAssertionType type) const
{
    ASCIILiteral typeString = [type] {
        switch (type) {
        case ProcessAssertionType::Foreground:
            return "Foreground"_s;
        case ProcessAssertionType::Background:
            return "Background"_s;
        case ProcessAssertionType::Suspended:
            return "Suspended"_s;
        case ProcessAssertionType::UnboundedNetworking:
        case ProcessAssertionType::MediaPlayback:
            ASSERT_NOT_REACHED(); // These other assertion types are not used by the ProcessThrottler.
            break;
        }
        return "Unknown"_s;
    }();

    return makeString(m_process.clientName(), " ", typeString, " Assertion");
}

void ProcessThrottler::setAssertionType(ProcessAssertionType newType)
{
    if (m_assertion && m_assertion->isValid() && m_assertion->type() == newType)
        return;

    PROCESSTHROTTLER_RELEASE_LOG("setAssertionType: Updating process assertion type to %u (foregroundActivities: %u, backgroundActivities: %u)", newType, m_foregroundActivities.size(), m_backgroundActivities.size());
    if (m_shouldTakeUIBackgroundAssertion)
        m_assertion = makeUnique<ProcessAndUIAssertion>(m_processIdentifier, assertionName(newType), newType);
    else
        m_assertion = makeUnique<ProcessAssertion>(m_processIdentifier, assertionName(newType), newType);
    m_process.didSetAssertionType(newType);
}
    
void ProcessThrottler::updateAssertionIfNeeded()
{
    if (!m_assertion)
        return;

    if (shouldBeRunnable()) {
        if (m_assertion->type() == ProcessAssertionType::Suspended || m_pendingRequestToSuspendID) {
            if (m_assertion->type() == ProcessAssertionType::Suspended)
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
        if (m_assertion->type() != ProcessAssertionType::Suspended) {
            m_prepareToSuspendTimeoutTimer.startOneShot(processSuspensionTimeout);
            sendPrepareToSuspendIPC(IsSuspensionImminent::No);
            return;
        }
    }

    updateAssertionTypeNow();
}

void ProcessThrottler::didConnectToProcess(ProcessID pid)
{
    PROCESSTHROTTLER_RELEASE_LOG_WITH_PID("didConnectToProcess:", pid);
    RELEASE_ASSERT(!m_assertion);

    m_processIdentifier = pid;
    setAssertionType(expectedAssertionType());
    RELEASE_ASSERT(m_assertion);
    m_assertion->setClient(*this);
}
    
void ProcessThrottler::prepareToSuspendTimeoutTimerFired()
{
    PROCESSTHROTTLER_RELEASE_LOG("prepareToSuspendTimeoutTimerFired: Updating process assertion to allow suspension");
    RELEASE_ASSERT(m_pendingRequestToSuspendID);
    updateAssertionTypeNow();
}
    
void ProcessThrottler::processReadyToSuspend()
{
    PROCESSTHROTTLER_RELEASE_LOG("processReadyToSuspend: Updating process assertion to allow suspension");

    RELEASE_ASSERT(m_pendingRequestToSuspendID);
    clearPendingRequestToSuspend();

    if (m_assertion->type() != ProcessAssertionType::Suspended)
        updateAssertionTypeNow();
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

    setAssertionType(isSuspensionImminent == IsSuspensionImminent::Yes ? ProcessAssertionType::Suspended : ProcessAssertionType::Background);
}

void ProcessThrottler::uiAssertionWillExpireImminently()
{
    PROCESSTHROTTLER_RELEASE_LOG("uiAssertionWillExpireImminently:");
    sendPrepareToSuspendIPC(IsSuspensionImminent::Yes);
    invalidateAllActivities();
    m_prepareToSuspendTimeoutTimer.stop();
}

void ProcessThrottler::assertionWasInvalidated()
{
    PROCESSTHROTTLER_RELEASE_LOG("assertionWasInvalidated:");
    invalidateAllActivities();
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

ProcessThrottler::TimedActivity::TimedActivity(Seconds timeout, ProcessThrottler::ActivityVariant&& activity)
    : m_timer(RunLoop::main(), this, &TimedActivity::activityTimedOut)
    , m_timeout(timeout)
    , m_activity(WTFMove(activity))
{
    updateTimer();
}

auto ProcessThrottler::TimedActivity::operator=(ProcessThrottler::ActivityVariant&& activity) -> TimedActivity&
{
    m_activity = WTFMove(activity);
    updateTimer();
    return *this;
}

void ProcessThrottler::TimedActivity::activityTimedOut()
{
    RELEASE_LOG_ERROR(ProcessSuspension, "%p - TimedActivity::activityTimedOut:", this);
    m_activity = nullptr;
}

void ProcessThrottler::TimedActivity::updateTimer()
{
    if (WTF::holds_alternative<std::nullptr_t>(m_activity))
        m_timer.stop();
    else
        m_timer.startOneShot(m_timeout);
}

} // namespace WebKit
