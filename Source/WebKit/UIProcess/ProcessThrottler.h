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
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "Logging.h"
#include "ProcessAssertion.h"
#include <variant>
#include <wtf/ProcessID.h>
#include <wtf/RefCounter.h>
#include <wtf/RunLoop.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/UniqueRef.h>
#include <wtf/WeakPtr.h>

namespace WebKit {
class ProcessThrottlerActivity;
class ProcessThrottler;
}

namespace WTF {
template<typename T> struct IsDeprecatedWeakRefSmartPointerException;
template<> struct IsDeprecatedWeakRefSmartPointerException<WebKit::ProcessThrottlerActivity> : std::true_type { };
template<> struct IsDeprecatedWeakRefSmartPointerException<WebKit::ProcessThrottler> : std::true_type { };
}

namespace WTF {
class TextStream;
}

namespace WebKit {

class AuxiliaryProcessProxy;
class ProcessThrottler;

enum UserObservablePageCounterType { };
using UserObservablePageCounter = RefCounter<UserObservablePageCounterType>;

enum ProcessSuppressionDisabledCounterType { };
using ProcessSuppressionDisabledCounter = RefCounter<ProcessSuppressionDisabledCounterType>;
using ProcessSuppressionDisabledToken = ProcessSuppressionDisabledCounter::Token;

enum class IsQuietActivity : bool { No, Yes };
enum class IsSuspensionImminent : bool { No, Yes };
enum class ProcessThrottleState : uint8_t { Suspended, Background, Foreground };
enum class ProcessThrottlerActivityType : bool { Background, Foreground };

class ProcessThrottlerActivity : public CanMakeWeakPtr<ProcessThrottlerActivity> {
    WTF_MAKE_TZONE_ALLOCATED(ProcessThrottlerActivity);
    WTF_MAKE_NONCOPYABLE(ProcessThrottlerActivity);
public:
    ProcessThrottlerActivity(ProcessThrottler&, ASCIILiteral name, ProcessThrottlerActivityType, IsQuietActivity);

    ~ProcessThrottlerActivity()
    {
        ASSERT(isMainRunLoop());
        if (isValid())
            invalidate(ForceEnableActivityLogging::No);
    }

    bool isValid() const { return !!m_throttler; }
    ASCIILiteral name() const { return m_name; }
    bool isQuietActivity() const { return m_isQuietActivity == IsQuietActivity::Yes; }
    bool isForeground() const { return m_type != ProcessThrottlerActivityType::Background; }

private:
    friend class ProcessThrottler;

    enum class ForceEnableActivityLogging : bool { No, Yes };
    void invalidate(ForceEnableActivityLogging);

    WeakPtr<ProcessThrottler> m_throttler;
    ASCIILiteral m_name;
    ProcessThrottlerActivityType m_type;
    IsQuietActivity m_isQuietActivity;
};

class ProcessThrottlerTimedActivity {
    WTF_MAKE_TZONE_ALLOCATED(ProcessThrottlerTimedActivity);
    WTF_MAKE_NONCOPYABLE(ProcessThrottlerTimedActivity);
    using Activity = ProcessThrottlerActivity;
    using ActivityVariant = std::variant<std::nullptr_t, UniqueRef<Activity>>;
public:
    explicit ProcessThrottlerTimedActivity(Seconds timeout, ActivityVariant&& = nullptr);
    ProcessThrottlerTimedActivity& operator=(ActivityVariant&&);
    const ActivityVariant& activity() const { return m_activity; }

private:
    void activityTimedOut();
    void updateTimer();

    RunLoop::Timer m_timer;
    Seconds m_timeout;
    ActivityVariant m_activity;
};

class ProcessThrottler : public CanMakeWeakPtr<ProcessThrottler> {
public:
    ProcessThrottler(AuxiliaryProcessProxy&, bool shouldTakeUIBackgroundAssertion);
    ~ProcessThrottler();

    using Activity = ProcessThrottlerActivity;
    using ActivityVariant = std::variant<std::nullptr_t, UniqueRef<Activity>>;

    void ref();
    void deref();

    using ForegroundActivity = Activity;
    UniqueRef<Activity> foregroundActivity(ASCIILiteral name);

    using BackgroundActivity = Activity;
    UniqueRef<Activity> backgroundActivity(ASCIILiteral name);
    UniqueRef<Activity> quietBackgroundActivity(ASCIILiteral name);

    static bool isValidBackgroundActivity(const ActivityVariant&);
    static bool isValidForegroundActivity(const ActivityVariant&);

    using TimedActivity = ProcessThrottlerTimedActivity;

    void didConnectToProcess(AuxiliaryProcessProxy&);
    void didDisconnectFromProcess();
    bool shouldBeRunnable() const { return !m_foregroundActivities.isEmptyIgnoringNullReferences() || !m_backgroundActivities.isEmptyIgnoringNullReferences(); }
    void setAllowsActivities(bool);
    void setShouldDropNearSuspendedAssertionAfterDelay(bool);
    void setShouldTakeNearSuspendedAssertion(bool);
    bool isSuspended() const;
    ProcessThrottleState currentState() const { return m_state; }
    bool isHoldingNearSuspendedAssertion() const { return m_assertion && m_assertion->type() == ProcessAssertionType::NearSuspended; }

    void invalidateAllActivitiesAndDropAssertion();

private:
    friend class ProcessThrottlerActivity;
    friend WTF::TextStream& operator<<(WTF::TextStream&, const ProcessThrottler&);

    ProcessThrottleState expectedThrottleState();
    void updateThrottleStateIfNeeded();
    void updateThrottleStateNow();
    void setAssertionType(ProcessAssertionType);
    void setThrottleState(ProcessThrottleState);
    void prepareToSuspendTimeoutTimerFired();
    void dropNearSuspendedAssertionTimerFired();
    void prepareToDropLastAssertionTimeoutTimerFired();
    void sendPrepareToSuspendIPC(IsSuspensionImminent);
    void processReadyToSuspend();

    bool addActivity(Activity&);
    void removeActivity(Activity&);
    void invalidateAllActivities();
    String assertionName(ProcessAssertionType) const;
    ProcessAssertionType assertionTypeForState(ProcessThrottleState);

    void uiAssertionWillExpireImminently();
    void assertionWasInvalidated();

    void clearPendingRequestToSuspend();
    void clearAssertion();

    class ProcessAssertionCache;

    Ref<AuxiliaryProcessProxy> protectedProcess() const;

    UniqueRef<ProcessAssertionCache> m_assertionCache;
    WeakRef<AuxiliaryProcessProxy> m_process;
    RefPtr<ProcessAssertion> m_assertion;
    RefPtr<ProcessAssertion> m_assertionToClearAfterPrepareToDropLastAssertion;
    RunLoop::Timer m_prepareToSuspendTimeoutTimer;
    RunLoop::Timer m_dropNearSuspendedAssertionTimer;
    RunLoop::Timer m_prepareToDropLastAssertionTimeoutTimer;
    WeakHashSet<Activity> m_foregroundActivities;
    WeakHashSet<Activity> m_backgroundActivities;
    std::optional<uint64_t> m_pendingRequestToSuspendID;
    ProcessThrottleState m_state { ProcessThrottleState::Suspended };
    bool m_shouldDropNearSuspendedAssertionAfterDelay { false };
    const bool m_shouldTakeUIBackgroundAssertion { false };
    bool m_shouldTakeNearSuspendedAssertion { true };
    bool m_allowsActivities { true };
    bool m_isConnectedToProcess { false };
};

inline auto ProcessThrottler::foregroundActivity(ASCIILiteral name) -> UniqueRef<Activity>
{
    return makeUniqueRef<Activity>(*this, name, ProcessThrottlerActivityType::Foreground, IsQuietActivity::No);
}

inline auto ProcessThrottler::backgroundActivity(ASCIILiteral name) -> UniqueRef<Activity>
{
    return makeUniqueRef<Activity>(*this, name, ProcessThrottlerActivityType::Background, IsQuietActivity::No);
}

inline auto ProcessThrottler::quietBackgroundActivity(ASCIILiteral name) -> UniqueRef<Activity>
{
    return makeUniqueRef<Activity>(*this, name, ProcessThrottlerActivityType::Background, IsQuietActivity::Yes);
}

WTF::TextStream& operator<<(WTF::TextStream&, const ProcessThrottler&);

} // namespace WebKit
