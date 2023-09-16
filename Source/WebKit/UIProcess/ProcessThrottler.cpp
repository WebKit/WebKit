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
#include <optional>
#include <wtf/CompletionHandler.h>
#include <wtf/EnumTraits.h>
#include <wtf/text/TextStream.h>

#if PLATFORM(COCOA)
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#endif

#define PROCESSTHROTTLER_RELEASE_LOG(msg, ...) RELEASE_LOG(ProcessSuspension, "%p - [PID=%d] ProcessThrottler::" msg, this, m_processID, ##__VA_ARGS__)
#define PROCESSTHROTTLER_RELEASE_LOG_WITH_PID(msg, ...) RELEASE_LOG(ProcessSuspension, "%p - [PID=%d] ProcessThrottler::" msg, this, ##__VA_ARGS__)

namespace WebKit {
    
static const Seconds processSuspensionTimeout { 20_s };
static const Seconds removeAllAssertionsTimeout { 8_min };

static uint64_t generatePrepareToSuspendRequestID()
{
    static uint64_t prepareToSuspendRequestID = 0;
    return ++prepareToSuspendRequestID;
}

ProcessThrottler::ProcessThrottler(ProcessThrottlerClient& process, bool shouldTakeUIBackgroundAssertion)
    : m_process(process)
    , m_prepareToSuspendTimeoutTimer(RunLoop::main(), this, &ProcessThrottler::prepareToSuspendTimeoutTimerFired)
    , m_dropNearSuspendedAssertionTimer(RunLoop::main(), this, &ProcessThrottler::dropNearSuspendedAssertionTimerFired)
    , m_prepareToDropLastAssertionTimeoutTimer(RunLoop::main(), this, &ProcessThrottler::prepareToDropLastAssertionTimeoutTimerFired)
    , m_pageAllowedToRunInTheBackgroundCounter([this](RefCounterEvent) { numberOfPagesAllowedToRunInTheBackgroundChanged(); })
    , m_shouldTakeUIBackgroundAssertion(shouldTakeUIBackgroundAssertion)
{
}

ProcessThrottler::~ProcessThrottler()
{
    invalidateAllActivities();
}

bool ProcessThrottler::addActivity(Activity& activity)
{
    ASSERT(isMainRunLoop());
    if (!m_allowsActivities) {
        if (!activity.isQuietActivity())
            PROCESSTHROTTLER_RELEASE_LOG("addActivity: not allowed to add %s activity %s", activity.isForeground() ? "foreground" : "background", activity.name().characters());
        return false;
    }

    if (activity.isForeground())
        m_foregroundActivities.add(activity);
    else
        m_backgroundActivities.add(activity);
    updateThrottleStateIfNeeded();
    return true;
}

void ProcessThrottler::removeActivity(Activity& activity)
{
    ASSERT(isMainRunLoop());
    if (!m_allowsActivities) {
        ASSERT(m_foregroundActivities.isEmptyIgnoringNullReferences());
        ASSERT(m_backgroundActivities.isEmptyIgnoringNullReferences());
        return;
    }

    bool wasRemoved;
    if (activity.isForeground())
        wasRemoved = m_foregroundActivities.remove(activity);
    else
        wasRemoved = m_backgroundActivities.remove(activity);
    ASSERT(wasRemoved);
    if (!wasRemoved)
        return;

    updateThrottleStateIfNeeded();
}

void ProcessThrottler::invalidateAllActivities()
{
    ASSERT(isMainRunLoop());
    PROCESSTHROTTLER_RELEASE_LOG("invalidateAllActivities: BEGIN (foregroundActivityCount: %u, backgroundActivityCount: %u)", m_foregroundActivities.computeSize(), m_backgroundActivities.computeSize());
    while (!m_foregroundActivities.isEmptyIgnoringNullReferences())
        m_foregroundActivities.begin()->invalidate();
    while (!m_backgroundActivities.isEmptyIgnoringNullReferences())
        m_backgroundActivities.begin()->invalidate();
    PROCESSTHROTTLER_RELEASE_LOG("invalidateAllActivities: END");
}

void ProcessThrottler::invalidateAllActivitiesAndDropAssertion()
{
    PROCESSTHROTTLER_RELEASE_LOG("invalidateAllActivitiesAndDropAssertion:");
    invalidateAllActivities();
    clearPendingRequestToSuspend();
    m_dropNearSuspendedAssertionTimer.stop();
    clearAssertion();
}

ProcessThrottleState ProcessThrottler::expectedThrottleState()
{
    if (!m_foregroundActivities.isEmptyIgnoringNullReferences())
        return ProcessThrottleState::Foreground;
    if (!m_backgroundActivities.isEmptyIgnoringNullReferences())
        return ProcessThrottleState::Background;
    return ProcessThrottleState::Suspended;
}

void ProcessThrottler::updateThrottleStateNow()
{
    setThrottleState(expectedThrottleState());
}

String ProcessThrottler::assertionName(ProcessAssertionType type) const
{
    ASCIILiteral typeString = [type] {
        switch (type) {
        case ProcessAssertionType::Foreground:
            return "Foreground"_s;
        case ProcessAssertionType::Background:
            return "Background"_s;
        case ProcessAssertionType::NearSuspended:
            return "NearSuspended"_s;
        case ProcessAssertionType::UnboundedNetworking:
        case ProcessAssertionType::MediaPlayback:
        case ProcessAssertionType::FinishTaskInterruptable:
        case ProcessAssertionType::BoostedJetsam:
            ASSERT_NOT_REACHED(); // These other assertion types are not used by the ProcessThrottler.
            break;
        }
        return "Unknown"_s;
    }();

    return makeString(m_process.clientName(), " ", typeString, " Assertion");
}

ProcessAssertionType ProcessThrottler::assertionTypeForState(ProcessThrottleState state)
{
    switch (state) {
    case ProcessThrottleState::Foreground:
        return ProcessAssertionType::Foreground;
    case ProcessThrottleState::Background:
        return ProcessAssertionType::Background;
    case ProcessThrottleState::Suspended:
        return ProcessAssertionType::NearSuspended;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

void ProcessThrottler::setThrottleState(ProcessThrottleState newState)
{
    m_state = newState;

    ProcessAssertionType newType = assertionTypeForState(newState);

    if (m_assertion && m_assertion->isValid() && m_assertion->type() == newType)
        return;

    PROCESSTHROTTLER_RELEASE_LOG("setThrottleState: Updating process assertion type to %u (foregroundActivities=%u, backgroundActivities=%u)", WTF::enumToUnderlyingType(newType), m_foregroundActivities.computeSize(), m_backgroundActivities.computeSize());

    // Keep the previous assertion active until the new assertion is taken asynchronously.
    auto previousAssertion = std::exchange(m_assertion, nullptr);
    if (m_shouldTakeUIBackgroundAssertion) {
        auto assertion = ProcessAndUIAssertion::create(m_processID, assertionName(newType), newType, ProcessAssertion::Mode::Async, m_process.environmentIdentifier(), [previousAssertion = WTFMove(previousAssertion)] { });
        assertion->setUIAssertionExpirationHandler([weakThis = WeakPtr { *this }] {
            if (weakThis)
                weakThis->uiAssertionWillExpireImminently();
        });
        m_assertion = WTFMove(assertion);
    } else
        m_assertion = ProcessAssertion::create(m_processID, assertionName(newType), newType, ProcessAssertion::Mode::Async, m_process.environmentIdentifier(), [previousAssertion = WTFMove(previousAssertion)] { });

    m_assertion->setInvalidationHandler([weakThis = WeakPtr { *this }] {
        if (weakThis)
            weakThis->assertionWasInvalidated();
    });

    if (isHoldingNearSuspendedAssertion()) {
        if (!m_shouldTakeNearSuspendedAssertion)
            clearAssertion();
        else if (m_shouldDropNearSuspendedAssertionAfterDelay)
            m_dropNearSuspendedAssertionTimer.startOneShot(removeAllAssertionsTimeout);
    } else
        m_dropNearSuspendedAssertionTimer.stop();

    m_process.didChangeThrottleState(newState);
}

void ProcessThrottler::updateThrottleStateIfNeeded()
{
    if (!m_processID)
        return;

    if (shouldBeRunnable()) {
        if (m_state == ProcessThrottleState::Suspended || m_pendingRequestToSuspendID) {
            if (m_state == ProcessThrottleState::Suspended)
                PROCESSTHROTTLER_RELEASE_LOG("updateThrottleStateIfNeeded: sending ProcessDidResume IPC because the process was suspended");
            else
                PROCESSTHROTTLER_RELEASE_LOG("updateThrottleStateIfNeeded: sending ProcessDidResume IPC because the WebProcess is still processing request to suspend=%" PRIu64, *m_pendingRequestToSuspendID);
            m_process.sendProcessDidResume(expectedThrottleState() == ProcessThrottleState::Foreground ? ProcessThrottlerClient::ResumeReason::ForegroundActivity : ProcessThrottlerClient::ResumeReason::BackgroundActivity);
            clearPendingRequestToSuspend();
        }
    } else {
        // If the process is currently runnable but will be suspended then first give it a chance to complete what it was doing
        // and clean up - move it to the background and send it a message to notify. Schedule a timeout so it can't stay running
        // in the background for too long.
        if (m_state != ProcessThrottleState::Suspended) {
            m_prepareToSuspendTimeoutTimer.startOneShot(processSuspensionTimeout);
            sendPrepareToSuspendIPC(IsSuspensionImminent::No);
            return;
        }
    }

    updateThrottleStateNow();
}

void ProcessThrottler::didConnectToProcess(ProcessID pid)
{
    PROCESSTHROTTLER_RELEASE_LOG_WITH_PID("didConnectToProcess:", pid);
    RELEASE_ASSERT(!m_assertion);

    m_processID = pid;
    updateThrottleStateNow();
    RELEASE_ASSERT(m_assertion || (m_state == ProcessThrottleState::Suspended && !m_shouldTakeNearSuspendedAssertion));
}

void ProcessThrottler::didDisconnectFromProcess()
{
    PROCESSTHROTTLER_RELEASE_LOG_WITH_PID("didDisconnectFromProcess:", m_processID);

    m_dropNearSuspendedAssertionTimer.stop();
    clearPendingRequestToSuspend();
    m_processID = 0;
    m_assertion = nullptr;
}
    
void ProcessThrottler::prepareToSuspendTimeoutTimerFired()
{
    PROCESSTHROTTLER_RELEASE_LOG("prepareToSuspendTimeoutTimerFired: Updating process assertion to allow suspension");
    RELEASE_ASSERT(m_pendingRequestToSuspendID);
    updateThrottleStateNow();
}

void ProcessThrottler::dropNearSuspendedAssertionTimerFired()
{
    PROCESSTHROTTLER_RELEASE_LOG("dropNearSuspendedAssertionTimerFired: Removing near-suspended process assertion");
    RELEASE_ASSERT(isHoldingNearSuspendedAssertion());
    ASSERT(m_shouldDropNearSuspendedAssertionAfterDelay);
    if (m_pageAllowedToRunInTheBackgroundCounter.value())
        PROCESSTHROTTLER_RELEASE_LOG("dropNearSuspendedAssertionTimerFired: Not releasing near-suspended assertion because a page is allowed to run in the background");
    else
        clearAssertion();
}

void ProcessThrottler::processReadyToSuspend()
{
    PROCESSTHROTTLER_RELEASE_LOG("processReadyToSuspend: Updating process assertion to allow suspension");

    RELEASE_ASSERT(m_pendingRequestToSuspendID);
    clearPendingRequestToSuspend();

    if (m_state != ProcessThrottleState::Suspended)
        updateThrottleStateNow();
}

void ProcessThrottler::clearPendingRequestToSuspend()
{
    m_prepareToSuspendTimeoutTimer.stop();
    m_pendingRequestToSuspendID = std::nullopt;
}

void ProcessThrottler::sendPrepareToSuspendIPC(IsSuspensionImminent isSuspensionImminent)
{
    if (m_pendingRequestToSuspendID) {
        // Do not send a new PrepareToSuspend IPC for imminent suspension if we've already sent a non-imminent PrepareToSuspend IPC.
        RELEASE_ASSERT(isSuspensionImminent == IsSuspensionImminent::Yes);
        PROCESSTHROTTLER_RELEASE_LOG("sendPrepareToSuspendIPC: Not sending PrepareToSuspend(isSuspensionImminent=%d) IPC because there is already one in flight (%" PRIu64 ")", isSuspensionImminent == IsSuspensionImminent::Yes, *m_pendingRequestToSuspendID);
    } else {
        m_pendingRequestToSuspendID = generatePrepareToSuspendRequestID();
        double remainingRunTime = ProcessAssertion::remainingRunTimeInSeconds(m_processID);
        PROCESSTHROTTLER_RELEASE_LOG("sendPrepareToSuspendIPC: Sending PrepareToSuspend(%" PRIu64 ", isSuspensionImminent=%d) IPC, remainingRunTime=%fs", *m_pendingRequestToSuspendID, isSuspensionImminent == IsSuspensionImminent::Yes, remainingRunTime);
        m_process.sendPrepareToSuspend(isSuspensionImminent, remainingRunTime, [this, weakThis = WeakPtr { *this }, requestToSuspendID = *m_pendingRequestToSuspendID]() mutable {
            if (weakThis && m_pendingRequestToSuspendID && *m_pendingRequestToSuspendID == requestToSuspendID)
                processReadyToSuspend();
        });
    }

    setThrottleState(isSuspensionImminent == IsSuspensionImminent::Yes ? ProcessThrottleState::Suspended : ProcessThrottleState::Background);
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

bool ProcessThrottler::isValidBackgroundActivity(const ActivityVariant& variant)
{
    if (!std::holds_alternative<UniqueRef<Activity>>(variant))
        return false;
    auto& activity = std::get<UniqueRef<Activity>>(variant).get();
    return activity.isValid() && !activity.isForeground();
}

bool ProcessThrottler::isValidForegroundActivity(const ActivityVariant& variant)
{
    if (!std::holds_alternative<UniqueRef<Activity>>(variant))
        return false;
    auto& activity = std::get<UniqueRef<Activity>>(variant).get();
    return activity.isValid() && activity.isForeground();
}

void ProcessThrottler::setAllowsActivities(bool allow)
{
    if (m_allowsActivities == allow)
        return;

    PROCESSTHROTTLER_RELEASE_LOG("setAllowsActivities %d", allow);

    if (!allow) {
        // Invalidate the activities before setting m_allowsActivities to false, so that the activities
        // are able to remove themselves from the map.
        invalidateAllActivities();
    }

    ASSERT(m_foregroundActivities.isEmptyIgnoringNullReferences());
    ASSERT(m_backgroundActivities.isEmptyIgnoringNullReferences());
    m_allowsActivities = allow;
}

void ProcessThrottler::setShouldTakeNearSuspendedAssertion(bool shouldTakeNearSuspendedAssertion)
{
    m_shouldTakeNearSuspendedAssertion = shouldTakeNearSuspendedAssertion;
    if (shouldTakeNearSuspendedAssertion) {
        if (!m_assertion && m_processID) {
            PROCESSTHROTTLER_RELEASE_LOG_WITH_PID("setShouldTakeNearSuspendedAssertion: Taking near-suspended assertion", m_processID);
            setThrottleState(ProcessThrottleState::Suspended);
        }
    } else {
        if (isHoldingNearSuspendedAssertion()) {
            PROCESSTHROTTLER_RELEASE_LOG_WITH_PID("setShouldTakeNearSuspendedAssertion: Releasing near-suspended assertion", m_processID);
            m_dropNearSuspendedAssertionTimer.stop();
            clearAssertion();
        }
    }
}

void ProcessThrottler::setShouldDropNearSuspendedAssertionAfterDelay(bool shouldDropAfterDelay)
{
    if (m_shouldDropNearSuspendedAssertionAfterDelay == shouldDropAfterDelay)
        return;

    m_shouldDropNearSuspendedAssertionAfterDelay = shouldDropAfterDelay;
    if (shouldDropAfterDelay) {
        if (isHoldingNearSuspendedAssertion())
            m_dropNearSuspendedAssertionTimer.startOneShot(removeAllAssertionsTimeout);
    } else
        m_dropNearSuspendedAssertionTimer.stop();
}

void ProcessThrottler::prepareToDropLastAssertionTimeoutTimerFired()
{
    PROCESSTHROTTLER_RELEASE_LOG("prepareToDropLastAssertionTimeoutTimerFired:");
    m_assertionToClearAfterPrepareToDropLastAssertion = nullptr;
}

void ProcessThrottler::clearAssertion()
{
    if (!m_assertion)
        return;

    PROCESSTHROTTLER_RELEASE_LOG("clearAssertion:");

    if (!m_prepareToDropLastAssertionTimeoutTimer.isActive())
        m_prepareToDropLastAssertionTimeoutTimer.startOneShot(10_s);

    m_assertionToClearAfterPrepareToDropLastAssertion = std::exchange(m_assertion, nullptr);
    m_process.prepareToDropLastAssertion([this, weakThis = WeakPtr { *this }] {
        if (!weakThis)
            return;
        PROCESSTHROTTLER_RELEASE_LOG("clearAssertion: Releasing near-suspended assertion");
        m_prepareToDropLastAssertionTimeoutTimer.stop();
        m_assertionToClearAfterPrepareToDropLastAssertion = nullptr;
        if (!m_assertion)
            m_process.didDropLastAssertion();
    });
}

PageAllowedToRunInTheBackgroundCounter::Token ProcessThrottler::pageAllowedToRunInTheBackgroundToken()
{
    return m_pageAllowedToRunInTheBackgroundCounter.count();
}

void ProcessThrottler::numberOfPagesAllowedToRunInTheBackgroundChanged()
{
    if (m_pageAllowedToRunInTheBackgroundCounter.value())
        return;

    if (m_dropNearSuspendedAssertionTimer.isActive())
        return;

    if (isHoldingNearSuspendedAssertion()) {
        PROCESSTHROTTLER_RELEASE_LOG("numberOfPagesAllowedToRunInTheBackgroundChanged: Releasing near-suspended assertion");
        clearAssertion();
    }
}

ProcessThrottlerTimedActivity::ProcessThrottlerTimedActivity(Seconds timeout, ProcessThrottler::ActivityVariant&& activity)
    : m_timer(RunLoop::main(), this, &ProcessThrottlerTimedActivity::activityTimedOut)
    , m_timeout(timeout)
    , m_activity(WTFMove(activity))
{
    updateTimer();
}

auto ProcessThrottlerTimedActivity::operator=(ProcessThrottler::ActivityVariant&& activity) -> ProcessThrottlerTimedActivity&
{
    m_activity = WTFMove(activity);
    updateTimer();
    return *this;
}

void ProcessThrottlerTimedActivity::activityTimedOut()
{
    RELEASE_LOG_ERROR(ProcessSuspension, "%p - ProcessThrottlerTimedActivity::activityTimedOut:", this);
    // Use variant::swap() to make sure that m_activity is in a good state when the underlying
    // ProcessThrottlerActivity gets destroyed. This is important as destroying the activity runs code
    // that may use / modify m_activity.
    ActivityVariant nullActivity { nullptr };
    m_activity.swap(nullActivity);
}

void ProcessThrottlerTimedActivity::updateTimer()
{
    if (std::holds_alternative<std::nullptr_t>(m_activity))
        m_timer.stop();
    else
        m_timer.startOneShot(m_timeout);
}

template <typename T>
static void logActivityNames(WTF::TextStream& ts, ASCIILiteral description, const T& activities, bool& didLog)
{
    if (activities.isEmptyIgnoringNullReferences())
        return;

    ts << (didLog ? ", "_s : ""_s) << description << ": "_s;
    didLog = true;

    bool isFirstItem = true;
    for (const auto& activity : activities) {
        if (!activity.isQuietActivity()) {
            if (!isFirstItem)
                ts << ", "_s;
            ts << activity.name();
            isFirstItem = false;
        }
    }
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const ProcessThrottler& throttler)
{
    bool didLog = false;
    logActivityNames(ts, "foreground_activities"_s, throttler.m_foregroundActivities, didLog);
    logActivityNames(ts, "background_activities"_s, throttler.m_backgroundActivities, didLog);

    if (auto assertion = throttler.m_assertion; assertion && assertion->isValid()) {
        ts << (didLog ? ", "_s : ""_s) << "assertion: "_s << processAssertionTypeDescription(assertion->type()) << " ("_s << ProcessAssertion::remainingRunTimeInSeconds(assertion->pid()) << " sec remaining)"_s;
        didLog = true;
    }

    return didLog ? ts : ts << "no-assertion-state"_s;
}

} // namespace WebKit
