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

#ifndef ProcessThrottler_h
#define ProcessThrottler_h

#include "Logging.h"
#include "ProcessAssertion.h"
#include <variant>
#include <wtf/ProcessID.h>
#include <wtf/RefCounter.h>
#include <wtf/RunLoop.h>
#include <wtf/UniqueRef.h>
#include <wtf/WeakPtr.h>

#define PROCESSTHROTTLER_RELEASE_LOG(msg, ...) RELEASE_LOG(ProcessSuspension, "%p - [PID=%d] ProcessThrottler::" msg, this, m_processIdentifier, ##__VA_ARGS__)
#define PROCESSTHROTTLER_RELEASE_LOG_WITH_PID(msg, ...) RELEASE_LOG(ProcessSuspension, "%p - [PID=%d] ProcessThrottler::" msg, this, ##__VA_ARGS__)
#define PROCESSTHROTTLER_ACTIVITY_RELEASE_LOG(msg, ...) RELEASE_LOG(ProcessSuspension, "%p - [PID=%d, throttler=%p] ProcessThrottler::Activity::" msg, this, m_throttler->m_processIdentifier, m_throttler, ##__VA_ARGS__)

namespace WTF {
class TextStream;
}

namespace WebKit {
    
enum UserObservablePageCounterType { };
typedef RefCounter<UserObservablePageCounterType> UserObservablePageCounter;
enum ProcessSuppressionDisabledCounterType { };
typedef RefCounter<ProcessSuppressionDisabledCounterType> ProcessSuppressionDisabledCounter;
typedef ProcessSuppressionDisabledCounter::Token ProcessSuppressionDisabledToken;

enum class IsSuspensionImminent : bool { No, Yes };
enum class ProcessThrottleState : uint8_t {
    Suspended,
    Background,
    Foreground,
};

class ProcessThrottlerClient;

class ProcessThrottler : public CanMakeWeakPtr<ProcessThrottler> {
public:
    ProcessThrottler(ProcessThrottlerClient&, bool shouldTakeUIBackgroundAssertion);
    ~ProcessThrottler();

    enum class ActivityType { Background, Foreground };
    template<ActivityType type> class Activity {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        Activity(ProcessThrottler& throttler, ASCIILiteral name)
            : m_throttler(&throttler)
            , m_name(name)
        {
            ASSERT(isMainRunLoop());
            if (!throttler.addActivity(*this)) {
                m_throttler = nullptr;
                return;
            }

            if (!isQuietActivity()) {
                PROCESSTHROTTLER_ACTIVITY_RELEASE_LOG("Activity: Starting %" PUBLIC_LOG_STRING " activity / '%" PUBLIC_LOG_STRING "'",
                    type == ActivityType::Foreground ? "foreground" : "background", m_name.characters());
            }
        }

        ~Activity()
        {
            ASSERT(isMainRunLoop());
            if (isValid())
                invalidate();
        }

        bool isValid() const { return !!m_throttler; }
        ASCIILiteral name() const { return m_name; }
        bool isQuietActivity() const { return !m_name.characters(); }

    private:
        friend class ProcessThrottler;

        void invalidate()
        {
            ASSERT(isValid());
            if (!isQuietActivity()) {
                PROCESSTHROTTLER_ACTIVITY_RELEASE_LOG("invalidate: Ending %" PUBLIC_LOG_STRING " activity / '%" PUBLIC_LOG_STRING "'",
                    type == ActivityType::Foreground ? "foreground" : "background", m_name.characters());
            }
            m_throttler->removeActivity(*this);
            m_throttler = nullptr;
        }

        ProcessThrottler* m_throttler { nullptr };
        ASCIILiteral m_name;
    };

    using ForegroundActivity = Activity<ActivityType::Foreground>;
    UniqueRef<ForegroundActivity> foregroundActivity(ASCIILiteral name);

    using BackgroundActivity = Activity<ActivityType::Background>;
    UniqueRef<BackgroundActivity> backgroundActivity(ASCIILiteral name);

    using ActivityVariant = std::variant<std::nullptr_t, UniqueRef<BackgroundActivity>, UniqueRef<ForegroundActivity>>;
    static bool isValidBackgroundActivity(const ActivityVariant&);
    static bool isValidForegroundActivity(const ActivityVariant&);

    class TimedActivity {
    public:
        TimedActivity(Seconds timeout, ActivityVariant&& = nullptr);
        TimedActivity& operator=(ActivityVariant&&);

    private:
        void activityTimedOut();
        void updateTimer();

        RunLoop::Timer m_timer;
        Seconds m_timeout;
        ActivityVariant m_activity;
    };
    
    void didConnectToProcess(ProcessID);
    bool shouldBeRunnable() const { return m_foregroundActivities.size() || m_backgroundActivities.size(); }
    void setAllowsActivities(bool);
    void setShouldTakeSuspendedAssertion(bool);

private:
    friend WTF::TextStream& operator<<(WTF::TextStream&, const ProcessThrottler&);

    ProcessThrottleState expectedThrottleState();
    void updateThrottleStateIfNeeded();
    void updateThrottleStateNow();
    void setAssertionType(ProcessAssertionType);
    void setThrottleState(ProcessThrottleState);
    void prepareToSuspendTimeoutTimerFired();
    void sendPrepareToSuspendIPC(IsSuspensionImminent);
    void processReadyToSuspend();

    bool addActivity(ForegroundActivity&);
    bool addActivity(BackgroundActivity&);
    void removeActivity(ForegroundActivity&);
    void removeActivity(BackgroundActivity&);
    void invalidateAllActivities();
    String assertionName(ProcessAssertionType) const;
    std::optional<ProcessAssertionType> assertionTypeForState(ProcessThrottleState);

    void uiAssertionWillExpireImminently();
    void assertionWasInvalidated();

    void clearPendingRequestToSuspend();

    ProcessThrottlerClient& m_process;
    ProcessID m_processIdentifier { 0 };
    RefPtr<ProcessAssertion> m_assertion;
    RunLoop::Timer m_prepareToSuspendTimeoutTimer;
    HashSet<ForegroundActivity*> m_foregroundActivities;
    HashSet<BackgroundActivity*> m_backgroundActivities;
    std::optional<uint64_t> m_pendingRequestToSuspendID;
    ProcessThrottleState m_state { ProcessThrottleState::Suspended };
    bool m_shouldTakeUIBackgroundAssertion;
    bool m_shouldTakeSuspendedAssertion { true };
    bool m_allowsActivities { true };
};

inline auto ProcessThrottler::foregroundActivity(ASCIILiteral name) -> UniqueRef<ForegroundActivity>
{
    return makeUniqueRef<ForegroundActivity>(*this, name);
}

inline auto ProcessThrottler::backgroundActivity(ASCIILiteral name) -> UniqueRef<BackgroundActivity>
{
    return makeUniqueRef<BackgroundActivity>(*this, name);
}

WTF::TextStream& operator<<(WTF::TextStream&, const ProcessThrottler&);

} // namespace WebKit

#endif // ProcessThrottler_h
