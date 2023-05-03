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
#include <wtf/UniqueRef.h>
#include <wtf/WeakPtr.h>

namespace WTF {
class TextStream;
}

namespace WebKit {
    
class ProcessThrottler;
class ProcessThrottlerClient;

enum UserObservablePageCounterType { };
using UserObservablePageCounter = RefCounter<UserObservablePageCounterType>;

enum ProcessSuppressionDisabledCounterType { };
using ProcessSuppressionDisabledCounter = RefCounter<ProcessSuppressionDisabledCounterType>;
using ProcessSuppressionDisabledToken = ProcessSuppressionDisabledCounter::Token;

enum PageAllowedToRunInTheBackgroundCounterType { };
using PageAllowedToRunInTheBackgroundCounter = RefCounter<PageAllowedToRunInTheBackgroundCounterType>;

enum class IsSuspensionImminent : bool { No, Yes };
enum class ProcessThrottleState : uint8_t { Suspended, Background, Foreground };
enum class ProcessThrottlerActivityType : bool { Background, Foreground };

class ProcessThrottlerActivity {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(ProcessThrottlerActivity);
public:
    ProcessThrottlerActivity(ProcessThrottler&, ASCIILiteral name, ProcessThrottlerActivityType);

    ~ProcessThrottlerActivity()
    {
        ASSERT(isMainRunLoop());
        if (isValid())
            invalidate();
    }

    bool isValid() const { return !!m_throttler; }
    ASCIILiteral name() const { return m_name; }
    bool isQuietActivity() const { return m_name.isNull(); }
    bool isForeground() const { return m_type != ProcessThrottlerActivityType::Background; }

private:
    friend class ProcessThrottler;

    void invalidate();

    ProcessThrottler* m_throttler { nullptr };
    ASCIILiteral m_name;
    ProcessThrottlerActivityType m_type;
};

class ProcessThrottlerTimedActivity {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(ProcessThrottlerTimedActivity);
    using Activity = ProcessThrottlerActivity;
    using ActivityVariant = std::variant<std::nullptr_t, UniqueRef<Activity>>;
    public:
        explicit ProcessThrottlerTimedActivity(Seconds timeout, ActivityVariant&& = nullptr);
        ProcessThrottlerTimedActivity& operator=(ActivityVariant&&);

    private:
        void activityTimedOut();
        void updateTimer();

        RunLoop::Timer m_timer;
        Seconds m_timeout;
        ActivityVariant m_activity;
};

class ProcessThrottler : public CanMakeWeakPtr<ProcessThrottler> {
public:
    ProcessThrottler(ProcessThrottlerClient&, bool shouldTakeUIBackgroundAssertion);
    ~ProcessThrottler();

    using Activity = ProcessThrottlerActivity;
    using ActivityVariant = std::variant<std::nullptr_t, UniqueRef<Activity>>;

    using ForegroundActivity = Activity;
    UniqueRef<Activity> foregroundActivity(ASCIILiteral name);

    using BackgroundActivity = Activity;
    UniqueRef<Activity> backgroundActivity(ASCIILiteral name);

    static bool isValidBackgroundActivity(const ActivityVariant&);
    static bool isValidForegroundActivity(const ActivityVariant&);

    // If any page holds one of these tokens, we will never release the "suspended" assertion which
    // means that the page will not be suspended when in the background, except if the application
    // also gets backgrounded.
    PageAllowedToRunInTheBackgroundCounter::Token pageAllowedToRunInTheBackgroundToken();

    using TimedActivity = ProcessThrottlerTimedActivity;

    void didConnectToProcess(ProcessID);
    void didDisconnectFromProcess();
    bool shouldBeRunnable() const { return m_foregroundActivities.size() || m_backgroundActivities.size(); }
    void setAllowsActivities(bool);
    void setShouldDropSuspendedAssertionAfterDelay(bool shouldDropAfterDelay) { m_shouldDropSuspendedAssertionAfterDelay = shouldDropAfterDelay; }
    void setShouldTakeSuspendedAssertion(bool);
    bool isSuspended() const { return m_processIdentifier && !m_assertion; }
    ProcessThrottleState currentState() const { return m_state; }

private:
    friend class ProcessThrottlerActivity;
    friend WTF::TextStream& operator<<(WTF::TextStream&, const ProcessThrottler&);

    ProcessThrottleState expectedThrottleState();
    void updateThrottleStateIfNeeded();
    void updateThrottleStateNow();
    void setAssertionType(ProcessAssertionType);
    void setThrottleState(ProcessThrottleState);
    void prepareToSuspendTimeoutTimerFired();
    void dropSuspendedAssertionTimerFired();
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
    void numberOfPagesAllowedToRunInTheBackgroundChanged();

    ProcessThrottlerClient& m_process;
    ProcessID m_processIdentifier { 0 };
    RefPtr<ProcessAssertion> m_assertion;
    RunLoop::Timer m_prepareToSuspendTimeoutTimer;
    RunLoop::Timer m_dropSuspendedAssertionTimer;
    HashSet<Activity*> m_foregroundActivities;
    HashSet<Activity*> m_backgroundActivities;
    std::optional<uint64_t> m_pendingRequestToSuspendID;
    ProcessThrottleState m_state { ProcessThrottleState::Suspended };
    PageAllowedToRunInTheBackgroundCounter m_pageAllowedToRunInTheBackgroundCounter;
    bool m_shouldDropSuspendedAssertionAfterDelay { false };
    bool m_shouldTakeUIBackgroundAssertion { false };
    bool m_shouldTakeSuspendedAssertion { true };
    bool m_allowsActivities { true };
};

#define PROCESSTHROTTLER_ACTIVITY_RELEASE_LOG(msg, ...) RELEASE_LOG(ProcessSuspension, "%p - [PID=%d, throttler=%p] ProcessThrottler::Activity::" msg, this, m_throttler->m_processIdentifier, m_throttler, ##__VA_ARGS__)

inline ProcessThrottlerActivity::ProcessThrottlerActivity(ProcessThrottler& throttler, ASCIILiteral name, ProcessThrottlerActivityType type)
    : m_throttler(&throttler)
    , m_name(name)
    , m_type(type)
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

inline void ProcessThrottlerActivity::invalidate()
{
    ASSERT(isValid());
    if (!isQuietActivity()) {
        PROCESSTHROTTLER_ACTIVITY_RELEASE_LOG("invalidate: Ending %" PUBLIC_LOG_STRING " activity / '%" PUBLIC_LOG_STRING "'",
            m_type == ProcessThrottlerActivityType::Foreground ? "foreground" : "background", m_name.characters());
    }
    m_throttler->removeActivity(*this);
    m_throttler = nullptr;
}

#undef PROCESSTHROTTLER_ACTIVITY_RELEASE_LOG

inline auto ProcessThrottler::foregroundActivity(ASCIILiteral name) -> UniqueRef<Activity>
{
    return makeUniqueRef<Activity>(*this, name, ProcessThrottlerActivityType::Foreground);
}

inline auto ProcessThrottler::backgroundActivity(ASCIILiteral name) -> UniqueRef<Activity>
{
    return makeUniqueRef<Activity>(*this, name, ProcessThrottlerActivityType::Background);
}

WTF::TextStream& operator<<(WTF::TextStream&, const ProcessThrottler&);

} // namespace WebKit
