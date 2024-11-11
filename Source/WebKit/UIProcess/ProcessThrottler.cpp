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

#include "AuxiliaryProcessProxy.h"
#include "Logging.h"
#include <optional>
#include <wtf/CheckedRef.h>
#include <wtf/CompletionHandler.h>
#include <wtf/EnumTraits.h>
#include <wtf/RunLoop.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/TextStream.h>

#if PLATFORM(COCOA)
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#endif

#define PROCESSTHROTTLER_RELEASE_LOG(msg, ...) RELEASE_LOG(ProcessSuspension, "%p - [PID=%d] ProcessThrottler::" msg, this, m_process->processID(), ##__VA_ARGS__)

namespace WebKit {
    
static constexpr Seconds processSuspensionTimeout { 20_s };
static constexpr Seconds removeAllAssertionsTimeout { 8_min };
static constexpr Seconds processAssertionCacheLifetime { 1_s };

Ref<ProcessThrottlerActivity> ProcessThrottlerActivity::create(ProcessThrottler& throttler, ASCIILiteral name, ProcessThrottlerActivityType type, IsQuietActivity isQuietActivity)
{
    return adoptRef(*new ProcessThrottlerActivity(throttler, name, type, isQuietActivity));
}

class ProcessThrottler::ProcessAssertionCache final : public CanMakeCheckedPtr<ProcessThrottler::ProcessAssertionCache> {
    WTF_MAKE_TZONE_ALLOCATED(ProcessThrottler::ProcessAssertionCache);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(ProcessAssertionCache);
public:
    void add(Ref<ProcessAssertion>&& assertion)
    {
        if (!m_isEnabled)
            return;

        auto type = assertion->type();
        assertion->setInvalidationHandler(nullptr);
        ASSERT(!m_entries.contains(type));
        m_entries.add(type, CachedAssertion::create(*this, WTFMove(assertion)));
    }

    RefPtr<ProcessAssertion> tryTake(ProcessAssertionType type)
    {
        if (auto assertion = m_entries.take(type); assertion && assertion->isValid())
            return assertion->release();
        return nullptr;
    }

    void remove(ProcessAssertionType type) { m_entries.remove(type); }

    void setEnabled(bool isEnabled)
    {
        if (m_isEnabled == isEnabled)
            return;

        m_isEnabled = isEnabled;
        if (!m_isEnabled)
            m_entries.clear();
    }

private:
    class CachedAssertion : public RefCounted<CachedAssertion> {
        WTF_MAKE_TZONE_ALLOCATED(CachedAssertion);
    public:
        static Ref<CachedAssertion> create(ProcessAssertionCache& cache, Ref<ProcessAssertion>&& assertion)
        {
            return adoptRef(*new CachedAssertion(cache, WTFMove(assertion)));
        }

        bool isValid() const { return m_assertion->isValid(); }
        Ref<ProcessAssertion> release() { return m_assertion.releaseNonNull(); }

    private:
        CachedAssertion(ProcessAssertionCache& cache, Ref<ProcessAssertion>&& assertion)
            : m_cache(cache)
            , m_assertion(WTFMove(assertion))
            , m_expirationTimer(RunLoop::main(), this, &CachedAssertion::entryExpired)
        {
            m_expirationTimer.startOneShot(processAssertionCacheLifetime);
        }

        void entryExpired()
        {
            ASSERT(m_assertion);
            m_cache->remove(m_assertion->type());
        }

        CheckedRef<ProcessAssertionCache> m_cache;
        RefPtr<ProcessAssertion> m_assertion;
        RunLoop::Timer m_expirationTimer;
    };

    HashMap<ProcessAssertionType, Ref<CachedAssertion>, IntHash<ProcessAssertionType>, WTF::StrongEnumHashTraits<ProcessAssertionType>> m_entries;
    bool m_isEnabled { true };
};

} // namespace WebKit

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL_NESTED(ProcessThrottlerProcessAssertionCache, ProcessThrottler::ProcessAssertionCache);
WTF_MAKE_TZONE_ALLOCATED_IMPL_NESTED(ProcessThrottlerProcessAssertionCacheCachedAssertion, ProcessThrottler::ProcessAssertionCache::CachedAssertion);

static uint64_t generatePrepareToSuspendRequestID()
{
    static uint64_t prepareToSuspendRequestID = 0;
    return ++prepareToSuspendRequestID;
}

ProcessThrottler::ProcessThrottler(AuxiliaryProcessProxy& process, bool shouldTakeUIBackgroundAssertion)
    : m_assertionCache(makeUniqueRef<ProcessAssertionCache>())
    , m_process(process)
    , m_prepareToSuspendTimeoutTimer(RunLoop::main(), this, &ProcessThrottler::prepareToSuspendTimeoutTimerFired)
    , m_dropNearSuspendedAssertionTimer(RunLoop::main(), this, &ProcessThrottler::dropNearSuspendedAssertionTimerFired)
    , m_prepareToDropLastAssertionTimeoutTimer(RunLoop::main(), this, &ProcessThrottler::prepareToDropLastAssertionTimeoutTimerFired)
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
    updateThrottleStateIfNeeded(activity.name());
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

    updateThrottleStateIfNeeded({ });
}

void ProcessThrottler::invalidateAllActivities()
{
    ASSERT(isMainRunLoop());
    PROCESSTHROTTLER_RELEASE_LOG("invalidateAllActivities: BEGIN (foregroundActivityCount: %u, backgroundActivityCount: %u)", m_foregroundActivities.computeSize(), m_backgroundActivities.computeSize());
    while (!m_foregroundActivities.isEmptyIgnoringNullReferences())
        m_foregroundActivities.begin()->invalidate(ProcessThrottlerActivity::ForceEnableActivityLogging::Yes);
    while (!m_backgroundActivities.isEmptyIgnoringNullReferences())
        m_backgroundActivities.begin()->invalidate(ProcessThrottlerActivity::ForceEnableActivityLogging::Yes);
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
        case ProcessAssertionType::FinishTaskCanSleep:
        case ProcessAssertionType::FinishTaskInterruptable:
        case ProcessAssertionType::BoostedJetsam:
            ASSERT_NOT_REACHED(); // These other assertion types are not used by the ProcessThrottler.
            break;
        }
        return "Unknown"_s;
    }();

    return makeString(protectedProcess()->clientName(), ' ', typeString, " Assertion"_s);
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

    if (!m_isConnectedToProcess)
        return;

    Ref process = m_process.get();
    PROCESSTHROTTLER_RELEASE_LOG("setThrottleState: Updating process assertion type to %u (foregroundActivities=%u, backgroundActivities=%u)", WTF::enumToUnderlyingType(newType), m_foregroundActivities.computeSize(), m_backgroundActivities.computeSize());

    // Keep the previous assertion active until the new assertion is taken asynchronously.
    RefPtr previousAssertion = std::exchange(m_assertion, nullptr);
    if (previousAssertion)
        m_assertionCache->add(*previousAssertion);

    m_assertion = m_assertionCache->tryTake(newType);
    if (!m_assertion) {
        if (m_shouldTakeUIBackgroundAssertion) {
            Ref assertion = ProcessAndUIAssertion::create(process, assertionName(newType), newType, ProcessAssertion::Mode::Async, [previousAssertion = WTFMove(previousAssertion)] { });
            assertion->setUIAssertionExpirationHandler([weakThis = WeakPtr { *this }] {
                if (weakThis)
                    weakThis->uiAssertionWillExpireImminently();
            });
            m_assertion = WTFMove(assertion);
        } else
            m_assertion = ProcessAssertion::create(process, assertionName(newType), newType, ProcessAssertion::Mode::Async, [previousAssertion = WTFMove(previousAssertion)] { });
    }
    m_assertion->setInvalidationHandler([weakThis = WeakPtr { *this }] {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->assertionWasInvalidated();
    });

    if (isHoldingNearSuspendedAssertion()) {
        if (!m_shouldTakeNearSuspendedAssertion)
            clearAssertion();
        else if (m_shouldDropNearSuspendedAssertionAfterDelay)
            m_dropNearSuspendedAssertionTimer.startOneShot(removeAllAssertionsTimeout);
    } else
        m_dropNearSuspendedAssertionTimer.stop();

    protectedProcess()->didChangeThrottleState(newState);
}

void ProcessThrottler::ref() const
{
    // Forward ref-counting to our owner.
    m_process->ref();
}

void ProcessThrottler::deref() const
{
    // Forward ref-counting to our owner.
    m_process->deref();
}

void ProcessThrottler::updateThrottleStateIfNeeded(ASCIILiteral lastAddedActivity)
{
    if (!m_isConnectedToProcess)
        return;

    if (shouldBeRunnable()) {
        if (m_state == ProcessThrottleState::Suspended || m_pendingRequestToSuspendID) {
#if !RELEASE_LOG_DISABLED
            const char* probableWakeupReason = !lastAddedActivity.isNull() ? lastAddedActivity.characters() : "unknown";
            if (m_state == ProcessThrottleState::Suspended)
                PROCESSTHROTTLER_RELEASE_LOG("updateThrottleStateIfNeeded: sending ProcessDidResume IPC because the process was suspended (probable wakeup reason: %" PUBLIC_LOG_STRING ")", probableWakeupReason);
            else
                PROCESSTHROTTLER_RELEASE_LOG("updateThrottleStateIfNeeded: sending ProcessDidResume IPC because the WebProcess is still processing request to suspend=%" PRIu64 " (probable wakeup reason: %" PUBLIC_LOG_STRING ")", *m_pendingRequestToSuspendID, probableWakeupReason);
#endif
            protectedProcess()->sendProcessDidResume(expectedThrottleState() == ProcessThrottleState::Foreground ? AuxiliaryProcessProxy::ResumeReason::ForegroundActivity : AuxiliaryProcessProxy::ResumeReason::BackgroundActivity);
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

void ProcessThrottler::didConnectToProcess(AuxiliaryProcessProxy& process)
{
    PROCESSTHROTTLER_RELEASE_LOG("didConnectToProcess");
    RELEASE_ASSERT(!m_assertion);

    m_isConnectedToProcess = true;
    updateThrottleStateNow();
    RELEASE_ASSERT(m_assertion || (m_state == ProcessThrottleState::Suspended && !m_shouldTakeNearSuspendedAssertion));
}

void ProcessThrottler::didDisconnectFromProcess()
{
    PROCESSTHROTTLER_RELEASE_LOG("didDisconnectFromProcess:");

    m_dropNearSuspendedAssertionTimer.stop();
    clearPendingRequestToSuspend();
    m_isConnectedToProcess = false;
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
    if (!m_isConnectedToProcess)
        return;

    if (m_pendingRequestToSuspendID) {
        // Do not send a new PrepareToSuspend IPC for imminent suspension if we've already sent a non-imminent PrepareToSuspend IPC.
        RELEASE_ASSERT(isSuspensionImminent == IsSuspensionImminent::Yes);
        PROCESSTHROTTLER_RELEASE_LOG("sendPrepareToSuspendIPC: Not sending PrepareToSuspend(isSuspensionImminent=%d) IPC because there is already one in flight (%" PRIu64 ")", isSuspensionImminent == IsSuspensionImminent::Yes, *m_pendingRequestToSuspendID);
    } else {
        m_pendingRequestToSuspendID = generatePrepareToSuspendRequestID();
        Ref process = m_process.get();
        double remainingRunTime = ProcessAssertion::remainingRunTimeInSeconds(process->processID());
        PROCESSTHROTTLER_RELEASE_LOG("sendPrepareToSuspendIPC: Sending PrepareToSuspend(%" PRIu64 ", isSuspensionImminent=%d) IPC, remainingRunTime=%fs", *m_pendingRequestToSuspendID, isSuspensionImminent == IsSuspensionImminent::Yes, remainingRunTime);
        process->sendPrepareToSuspend(isSuspensionImminent, remainingRunTime, [this, weakThis = WeakPtr { *this }, requestToSuspendID = *m_pendingRequestToSuspendID]() mutable {
            RefPtr protectedThis = weakThis.get();
            if (protectedThis && m_pendingRequestToSuspendID && *m_pendingRequestToSuspendID == requestToSuspendID)
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

bool ProcessThrottler::isValidBackgroundActivity(const ProcessThrottler::Activity* activity)
{
    return activity && activity->isValid() && !activity->isForeground();
}

bool ProcessThrottler::isValidForegroundActivity(const ProcessThrottler::Activity* activity)
{
    return activity && activity->isValid() && activity->isForeground();
}

void ProcessThrottler::setAllowsActivities(bool allow)
{
    if (m_allowsActivities == allow)
        return;

    PROCESSTHROTTLER_RELEASE_LOG("setAllowsActivities %d", allow);

    m_assertionCache->setEnabled(allow);
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
        if (!m_assertion && m_isConnectedToProcess) {
            PROCESSTHROTTLER_RELEASE_LOG("setShouldTakeNearSuspendedAssertion: Taking near-suspended assertion");
            setThrottleState(ProcessThrottleState::Suspended);
        }
    } else {
        if (isHoldingNearSuspendedAssertion()) {
            PROCESSTHROTTLER_RELEASE_LOG("setShouldTakeNearSuspendedAssertion: Releasing near-suspended assertion");
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
    protectedProcess()->prepareToDropLastAssertion([this, weakThis = WeakPtr { *this }] {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        PROCESSTHROTTLER_RELEASE_LOG("clearAssertion: Releasing near-suspended assertion");
        m_prepareToDropLastAssertionTimeoutTimer.stop();
        m_assertionToClearAfterPrepareToDropLastAssertion = nullptr;
        if (!m_assertion)
            protectedProcess()->didDropLastAssertion();
    });
}

Ref<AuxiliaryProcessProxy> ProcessThrottler::protectedProcess() const
{
    return m_process.get();
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(ProcessThrottlerTimedActivity);

Ref<ProcessThrottlerTimedActivity> ProcessThrottlerTimedActivity::create(Seconds timeout, RefPtr<Activity>&& activity)
{
    return adoptRef(*new ProcessThrottlerTimedActivity(timeout, WTFMove(activity)));
}

ProcessThrottlerTimedActivity::ProcessThrottlerTimedActivity(Seconds timeout, RefPtr<ProcessThrottlerTimedActivity::Activity>&& activity)
    : m_timer(RunLoop::main(), this, &ProcessThrottlerTimedActivity::activityTimedOut)
    , m_timeout(timeout)
    , m_activity(WTFMove(activity))
{
    updateTimer();
}

void ProcessThrottlerTimedActivity::setActivity(RefPtr<ProcessThrottlerTimedActivity::Activity>&& activity)
{
    m_activity = WTFMove(activity);
    updateTimer();
}

void ProcessThrottlerTimedActivity::activityTimedOut()
{
    RELEASE_LOG(ProcessSuspension, "%p - ProcessThrottlerTimedActivity::activityTimedOut: %" PUBLIC_LOG_STRING " (timeout: %.f sec)", this, m_activity ? m_activity->name().characters() : "null", m_timeout.seconds());
    // Use std::exchange to make sure that m_activity is in a good state when the underlying
    // ProcessThrottlerActivity gets destroyed. This is important as destroying the activity runs code
    // that may use / modify m_activity.
    std::exchange(m_activity, nullptr);
}

void ProcessThrottlerTimedActivity::updateTimer()
{
    if (!m_activity)
        m_timer.stop();
    else
        m_timer.startOneShot(m_timeout);
}

void ProcessThrottlerTimedActivity::setTimeout(Seconds timeout)
{
    if (timeout < 0_s || m_timeout == timeout)
        return;

    m_timeout = timeout;

    if (m_timer.isActive()) {
        Seconds secondsUntilFire = std::max(m_timer.secondsUntilFire(), 0_s);
        m_timer.startOneShot(timeout > secondsUntilFire ? timeout - secondsUntilFire : 0_s);
    }
}

#define PROCESSTHROTTLER_ACTIVITY_RELEASE_LOG(msg, ...) RELEASE_LOG(ProcessSuspension, "%p - [PID=%d, throttler=%p] ProcessThrottler::Activity::" msg, this, m_throttler ? m_throttler->m_process->processID() : 0, m_throttler.get(), ##__VA_ARGS__)

WTF_MAKE_TZONE_ALLOCATED_IMPL(ProcessThrottlerActivity);

ProcessThrottlerActivity::ProcessThrottlerActivity(ProcessThrottler& throttler, ASCIILiteral name, ProcessThrottlerActivityType type, IsQuietActivity isQuiet)
    : m_throttler(&throttler)
    , m_name(name)
    , m_type(type)
    , m_isQuietActivity(isQuiet)
{
    ASSERT(isMainRunLoop());
    if (!throttler.addActivity(*this)) {
        m_throttler = nullptr;
        return;
    }
    if (!isQuietActivity()) {
        PROCESSTHROTTLER_ACTIVITY_RELEASE_LOG("Activity: Starting %" PUBLIC_LOG_STRING " activity / '%" PUBLIC_LOG_STRING "'",
            type == ProcessThrottlerActivityType::Foreground ? "foreground" : "background", m_name.characters());
    }
}

void ProcessThrottlerActivity::invalidate(ForceEnableActivityLogging forceEnableActivityLogging)
{
    ASSERT(isValid());
    RefPtr throttler = m_throttler.get();
    if (!throttler)
        return;

    if (!isQuietActivity() || forceEnableActivityLogging == ForceEnableActivityLogging::Yes) {
        PROCESSTHROTTLER_ACTIVITY_RELEASE_LOG("invalidate: Ending %" PUBLIC_LOG_STRING " activity / '%" PUBLIC_LOG_STRING "'",
            m_type == ProcessThrottlerActivityType::Foreground ? "foreground" : "background", m_name.characters());
    }
    throttler->removeActivity(*this);
    m_throttler = nullptr;
}

#undef PROCESSTHROTTLER_ACTIVITY_RELEASE_LOG

template <typename T>
static void logActivityNames(WTF::TextStream& ts, ASCIILiteral description, const T& activities, bool& didLog)
{
    if (activities.isEmptyIgnoringNullReferences())
        return;

    ts << (didLog ? ", "_s : ""_s) << description << ": "_s;
    didLog = true;

    bool isFirstItem = true;
    for (const auto& activity : activities) {
        if (!isFirstItem)
            ts << ", "_s;
        ts << activity.name();
        isFirstItem = false;
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
