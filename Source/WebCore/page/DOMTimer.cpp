/*
 * Copyright (C) 2008, 2014 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "config.h"
#include "DOMTimer.h"

#include "HTMLPlugInElement.h"
#include "InspectorInstrumentation.h"
#include "Logging.h"
#include "Page.h"
#include "ScheduledAction.h"
#include "ScriptExecutionContext.h"
#include "Settings.h"
#include <wtf/HashMap.h>
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RandomNumber.h>
#include <wtf/StdLibExtras.h>

#if ENABLE(CONTENT_CHANGE_OBSERVER)
#include "ContentChangeObserver.h"
#include "DOMTimerHoldingTank.h"
#endif

namespace WebCore {

static constexpr Seconds minIntervalForNonUserObservableChangeTimers { 1_s }; // Empirically determined to maximize battery life.
static constexpr Seconds minIntervalForOneShotTimers { 0_ms };
static constexpr Seconds minIntervalForRepeatingTimers { 1_ms };
static constexpr int maxTimerNestingLevel = 10;
static constexpr int maxTimerNestingLevelForOneShotTimers = 10;
static constexpr int maxTimerNestingLevelForRepeatingTimers = 5;

class DOMTimerFireState {
public:
    DOMTimerFireState(ScriptExecutionContext& context, int nestingLevel)
        : m_context(context)
        , m_contextIsDocument(is<Document>(m_context))
        // For worker threads, don't update the current DOMTimerFireState.
        // Setting this from workers would not be thread-safe, and its not relevant to current uses.
        , m_initialDOMTreeVersion(m_contextIsDocument ? downcast<Document>(m_context).domTreeVersion() : 0)
        , m_previous(m_contextIsDocument ? std::exchange(current, this) : nullptr)
    {
        m_context.setTimerNestingLevel(nestingLevel);
    }

    ~DOMTimerFireState()
    {
        if (m_contextIsDocument)
            current = m_previous;
        m_context.setTimerNestingLevel(0);
    }

    Document* contextDocument() const { return m_contextIsDocument ? &downcast<Document>(m_context) : nullptr; }

    void setScriptMadeUserObservableChanges() { m_scriptMadeUserObservableChanges = true; }
    void setScriptMadeNonUserObservableChanges() { m_scriptMadeNonUserObservableChanges = true; }

    bool scriptMadeNonUserObservableChanges() const { return m_scriptMadeNonUserObservableChanges; }
    bool scriptMadeUserObservableChanges() const
    {
        if (m_scriptMadeUserObservableChanges)
            return true;

        Document* document = contextDocument();
        // To be conservative, we also consider any DOM Tree change to be user observable.
        return document && document->domTreeVersion() != m_initialDOMTreeVersion;
    }

    static DOMTimerFireState* current;

private:
    ScriptExecutionContext& m_context;
    bool m_contextIsDocument;
    bool m_scriptMadeNonUserObservableChanges { false };
    bool m_scriptMadeUserObservableChanges { false };
    uint64_t m_initialDOMTreeVersion;
    DOMTimerFireState* m_previous;
};

DOMTimerFireState* DOMTimerFireState::current = nullptr;

struct NestedTimersMap {
    typedef HashMap<int, Ref<DOMTimer>>::const_iterator const_iterator;

    static NestedTimersMap* instanceForContext(ScriptExecutionContext& context)
    {
        // For worker threads, we don't use NestedTimersMap as doing so would not
        // be thread safe.
        if (is<Document>(context))
            return &instance();
        return nullptr;
    }

    void startTracking()
    {
        // Make sure we start with an empty HashMap. In theory, it is possible the HashMap is not
        // empty if a timer fires during the execution of another timer (may happen with the
        // in-process Web Inspector).
        nestedTimers.clear();
        isTrackingNestedTimers = true;
    }

    void stopTracking()
    {
        isTrackingNestedTimers = false;
        nestedTimers.clear();
    }

    void add(int timeoutId, Ref<DOMTimer>&& timer)
    {
        if (isTrackingNestedTimers)
            nestedTimers.add(timeoutId, WTFMove(timer));
    }

    void remove(int timeoutId)
    {
        if (isTrackingNestedTimers)
            nestedTimers.remove(timeoutId);
    }

    const_iterator begin() const { return nestedTimers.begin(); }
    const_iterator end() const { return nestedTimers.end(); }

private:
    static NestedTimersMap& instance()
    {
        static NeverDestroyed<NestedTimersMap> map;
        return map;
    }

    static bool isTrackingNestedTimers;
    HashMap<int /* timeoutId */, Ref<DOMTimer>> nestedTimers;
};

bool NestedTimersMap::isTrackingNestedTimers = false;

DOMTimer::DOMTimer(ScriptExecutionContext& context, Function<void(ScriptExecutionContext&)>&& action, Seconds interval, bool oneShot)
    : SuspendableTimerBase(&context)
    , m_nestingLevel(context.timerNestingLevel())
    , m_action(WTFMove(action))
    , m_originalInterval(interval)
    , m_throttleState(Undetermined)
    , m_oneShot(oneShot)
    , m_currentTimerInterval(intervalClampedToMinimum())
    , m_userGestureTokenToForward(UserGestureIndicator::currentUserGesture())
{
    if (oneShot)
        startOneShot(m_currentTimerInterval);
    else
        startRepeating(m_currentTimerInterval);
}

DOMTimer::~DOMTimer() = default;

int DOMTimer::install(ScriptExecutionContext& context, std::unique_ptr<ScheduledAction> action, Seconds timeout, bool oneShot)
{
    auto actionFunction = [action = WTFMove(action)](ScriptExecutionContext& context) mutable {
        action->execute(context);
    };
    return DOMTimer::install(context, WTFMove(actionFunction), timeout, oneShot);
}

int DOMTimer::install(ScriptExecutionContext& context, Function<void(ScriptExecutionContext&)>&& action, Seconds timeout, bool oneShot)
{
    Ref<DOMTimer> timer = adoptRef(*new DOMTimer(context, WTFMove(action), timeout, oneShot));
    timer->suspendIfNeeded();

    // Keep asking for the next id until we're given one that we don't already have.
    do {
        timer->m_timeoutId = context.circularSequentialID();
    } while (!context.addTimeout(timer->m_timeoutId, timer.get()));

    InspectorInstrumentation::didInstallTimer(context, timer->m_timeoutId, timeout, oneShot);

    // Keep track of nested timer installs.
    if (NestedTimersMap* nestedTimers = NestedTimersMap::instanceForContext(context))
        nestedTimers->add(timer->m_timeoutId, timer.get());
#if ENABLE(CONTENT_CHANGE_OBSERVER)
    if (is<Document>(context)) {
        auto& document = downcast<Document>(context);
        document.contentChangeObserver().didInstallDOMTimer(timer.get(), timeout, oneShot);
        if (DeferDOMTimersForScope::isDeferring())
            document.domTimerHoldingTank().add(timer.get());
    }
#endif
    return timer->m_timeoutId;
}

void DOMTimer::removeById(ScriptExecutionContext& context, int timeoutId)
{
    // timeout IDs have to be positive, and 0 and -1 are unsafe to
    // even look up since they are the empty and deleted value
    // respectively
    if (timeoutId <= 0)
        return;

#if ENABLE(CONTENT_CHANGE_OBSERVER)
    if (is<Document>(context)) {
        auto& document = downcast<Document>(context);
        if (auto* timer = document.findTimeout(timeoutId)) {
            document.contentChangeObserver().didRemoveDOMTimer(*timer);
            if (auto* holdingTank = document.domTimerHoldingTankIfExists())
                holdingTank->remove(*timer);
        }
    }
#endif

    if (NestedTimersMap* nestedTimers = NestedTimersMap::instanceForContext(context))
        nestedTimers->remove(timeoutId);

    InspectorInstrumentation::didRemoveTimer(context, timeoutId);
    context.removeTimeout(timeoutId);
}

inline bool DOMTimer::isDOMTimersThrottlingEnabled(Document& document) const
{
    auto* page = document.page();
    if (!page)
        return true;
    return page->settings().domTimersThrottlingEnabled();
}

void DOMTimer::updateThrottlingStateIfNecessary(const DOMTimerFireState& fireState)
{
    Document* contextDocument = fireState.contextDocument();
    // We don't throttle timers in worker threads.
    if (!contextDocument)
        return;

    if (UNLIKELY(!isDOMTimersThrottlingEnabled(*contextDocument))) {
        if (m_throttleState == ShouldThrottle) {
            // Unthrottle the timer in case it was throttled before the setting was updated.
            LOG(DOMTimers, "%p - Unthrottling DOM timer because throttling was disabled via settings.", this);
            m_throttleState = ShouldNotThrottle;
            updateTimerIntervalIfNecessary();
        }
        return;
    }

    if (fireState.scriptMadeUserObservableChanges()) {
        if (m_throttleState != ShouldNotThrottle) {
            m_throttleState = ShouldNotThrottle;
            updateTimerIntervalIfNecessary();
        }
    } else if (fireState.scriptMadeNonUserObservableChanges()) {
        if (m_throttleState != ShouldThrottle) {
            m_throttleState = ShouldThrottle;
            updateTimerIntervalIfNecessary();
        }
    }
}

void DOMTimer::scriptDidInteractWithPlugin()
{
    if (!DOMTimerFireState::current)
        return;

    DOMTimerFireState::current->setScriptMadeUserObservableChanges();
}

void DOMTimer::fired()
{
    // Retain this - if the timer is cancelled while this function is on the stack (implicitly and always
    // for one-shot timers, or if removeById is called on itself from within an interval timer fire) then
    // wait unit the end of this function to delete DOMTimer.
    Ref<DOMTimer> protectedThis(*this);

    ASSERT(scriptExecutionContext());
    ScriptExecutionContext& context = *scriptExecutionContext();

#if PLATFORM(IOS_FAMILY)
    if (is<Document>(context)) {
        auto& document = downcast<Document>(context);
        if (auto* holdingTank = document.domTimerHoldingTankIfExists(); holdingTank && holdingTank->contains(*this)) {
            if (m_oneShot)
                startOneShot(0_s);
            return;
        }
    }
#endif

    DOMTimerFireState fireState(context, std::min(m_nestingLevel + 1, maxTimerNestingLevel));

    if (m_userGestureTokenToForward && m_userGestureTokenToForward->hasExpired(UserGestureToken::maximumIntervalForUserGestureForwarding))
        m_userGestureTokenToForward = nullptr;

    ASSERT(!isSuspended());
    ASSERT(!context.activeDOMObjectsAreSuspended());
    UserGestureIndicator gestureIndicator(m_userGestureTokenToForward);
    // Only the first execution of a multi-shot timer should get an affirmative user gesture indicator.
    m_userGestureTokenToForward = nullptr;

    InspectorInstrumentation::willFireTimer(context, m_timeoutId, m_oneShot);

    // Simple case for non-one-shot timers.
    if (isActive()) {
        if (m_nestingLevel < maxTimerNestingLevel) {
            m_nestingLevel++;
            updateTimerIntervalIfNecessary();
        }

        m_action(context);

        InspectorInstrumentation::didFireTimer(context, m_timeoutId, m_oneShot);

        updateThrottlingStateIfNecessary(fireState);
        return;
    }

    context.removeTimeout(m_timeoutId);

    // Keep track nested timer installs.
    NestedTimersMap* nestedTimers = NestedTimersMap::instanceForContext(context);
    if (nestedTimers)
        nestedTimers->startTracking();

#if ENABLE(CONTENT_CHANGE_OBSERVER)
    ContentChangeObserver::DOMTimerScope observingScope(dynamicDowncast<Document>(context), *this);
#endif
    m_action(context);

    InspectorInstrumentation::didFireTimer(context, m_timeoutId, m_oneShot);

    // Check if we should throttle nested single-shot timers.
    if (nestedTimers) {
        for (auto& idAndTimer : *nestedTimers) {
            auto& timer = idAndTimer.value;
            if (timer->isActive() && timer->m_oneShot)
                timer->updateThrottlingStateIfNecessary(fireState);
        }
        nestedTimers->stopTracking();
    }
}

void DOMTimer::didStop()
{
    // Need to release JS objects potentially protected by ScheduledAction
    // because they can form circular references back to the ScriptExecutionContext
    // which will cause a memory leak.
    m_action = nullptr;
}

void DOMTimer::updateTimerIntervalIfNecessary()
{
    ASSERT(m_nestingLevel <= maxTimerNestingLevel);

    auto previousInterval = m_currentTimerInterval;
    m_currentTimerInterval = intervalClampedToMinimum();
    if (previousInterval == m_currentTimerInterval)
        return;

    if (m_oneShot) {
        LOG(DOMTimers, "%p - Updating DOMTimer's fire interval from %.2f ms to %.2f ms due to throttling.", this, previousInterval.milliseconds(), m_currentTimerInterval.milliseconds());
        augmentFireInterval(m_currentTimerInterval - previousInterval);
    } else {
        ASSERT(repeatInterval() == previousInterval);
        LOG(DOMTimers, "%p - Updating DOMTimer's repeat interval from %.2f ms to %.2f ms due to throttling.", this, previousInterval.milliseconds(), m_currentTimerInterval.milliseconds());
        augmentRepeatInterval(m_currentTimerInterval - previousInterval);
    }
}

Seconds DOMTimer::intervalClampedToMinimum() const
{
    ASSERT(scriptExecutionContext());
    ASSERT(m_nestingLevel <= maxTimerNestingLevel);

    Seconds interval = std::max(m_oneShot ? minIntervalForOneShotTimers : minIntervalForRepeatingTimers, m_originalInterval);

    // Only apply throttling to repeating timers.
    if (m_nestingLevel < (m_oneShot ? maxTimerNestingLevelForOneShotTimers : maxTimerNestingLevelForRepeatingTimers))
        return interval;

    // Apply two throttles - the global (per Page) minimum, and also a per-timer throttle.
    interval = std::max(interval, scriptExecutionContext()->minimumDOMTimerInterval());
    if (m_throttleState == ShouldThrottle)
        interval = std::max(interval, minIntervalForNonUserObservableChangeTimers);
    return interval;
}

std::optional<MonotonicTime> DOMTimer::alignedFireTime(MonotonicTime fireTime) const
{
    Seconds alignmentInterval = scriptExecutionContext()->domTimerAlignmentInterval(m_nestingLevel >= (m_oneShot ? maxTimerNestingLevelForOneShotTimers : maxTimerNestingLevelForRepeatingTimers));
    if (!alignmentInterval)
        return std::nullopt;
    
    static const double randomizedProportion = randomNumber();

    // Force alignment to randomizedAlignment fraction of the way between alignemntIntervals, e.g.
    // if alignmentInterval is 10_ms and randomizedAlignment is 0.3 this will align to 3, 13, 23, ...
    Seconds randomizedOffset = alignmentInterval * randomizedProportion;
    MonotonicTime adjustedFireTime = fireTime - randomizedOffset;
    return adjustedFireTime - (adjustedFireTime % alignmentInterval) + alignmentInterval + randomizedOffset;
}

const char* DOMTimer::activeDOMObjectName() const
{
    return "DOMTimer";
}

} // namespace WebCore
