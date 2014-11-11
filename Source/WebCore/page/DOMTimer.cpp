/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
#include "PluginViewBase.h"
#include "ScheduledAction.h"
#include "ScriptExecutionContext.h"
#include "UserGestureIndicator.h"
#include <wtf/CurrentTime.h>
#include <wtf/HashSet.h>
#include <wtf/MathExtras.h>
#include <wtf/StdLibExtras.h>

#if PLATFORM(IOS)
#include "Chrome.h"
#include "ChromeClient.h"
#include "Frame.h"
#include "Page.h"
#include "WKContentObservation.h"
#endif

namespace WebCore {

static const int maxIntervalForUserGestureForwarding = 1000; // One second matches Gecko.
static const int minIntervalForNonUserObservablePluginScriptTimers = 1000; // Empirically determined to maximize battery life.
static const int maxTimerNestingLevel = 5;
static const double oneMillisecond = 0.001;

struct DOMTimerFireState {
    DOMTimerFireState(ScriptExecutionContext* context)
        : scriptDidInteractWithNonUserObservablePlugin(false)
        , scriptDidInteractWithUserObservablePlugin(false)
        , shouldSetCurrent(context->isDocument())
    {
        // For worker threads, don't update the current DOMTimerFireState.
        // Setting this from workers would not be thread-safe, and its not relevant to current uses.
        if (shouldSetCurrent) {
            previous = current;
            current = this;
        }
    }

    ~DOMTimerFireState()
    {
        if (shouldSetCurrent)
            current = previous;
    }

    static DOMTimerFireState* current;

    bool scriptDidInteractWithNonUserObservablePlugin;
    bool scriptDidInteractWithUserObservablePlugin;

private:
    bool shouldSetCurrent;
    DOMTimerFireState* previous;
};

DOMTimerFireState* DOMTimerFireState::current = nullptr;

static inline bool shouldForwardUserGesture(int interval, int nestingLevel)
{
    return UserGestureIndicator::processingUserGesture()
        && interval <= maxIntervalForUserGestureForwarding
        && !nestingLevel; // Gestures should not be forwarded to nested timers.
}

DOMTimer::DOMTimer(ScriptExecutionContext* context, std::unique_ptr<ScheduledAction> action, int interval, bool singleShot)
    : SuspendableTimer(context)
    , m_nestingLevel(context->timerNestingLevel())
    , m_action(WTF::move(action))
    , m_originalInterval(interval)
    , m_throttleState(Undetermined)
    , m_currentTimerInterval(intervalClampedToMinimum())
    , m_shouldForwardUserGesture(shouldForwardUserGesture(interval, m_nestingLevel))
{
    RefPtr<DOMTimer> reference = adoptRef(this);

    // Keep asking for the next id until we're given one that we don't already have.
    do {
        m_timeoutId = context->circularSequentialID();
    } while (!context->addTimeout(m_timeoutId, reference));

    if (singleShot)
        startOneShot(m_currentTimerInterval);
    else
        startRepeating(m_currentTimerInterval);
}

int DOMTimer::install(ScriptExecutionContext* context, std::unique_ptr<ScheduledAction> action, int timeout, bool singleShot)
{
    // DOMTimer constructor passes ownership of the initial ref on the object to the constructor.
    // This reference will be released automatically when a one-shot timer fires, when the context
    // is destroyed, or if explicitly cancelled by removeById. 
    DOMTimer* timer = new DOMTimer(context, WTF::move(action), timeout, singleShot);
#if PLATFORM(IOS)
    if (context->isDocument()) {
        Document& document = toDocument(*context);
        bool didDeferTimeout = document.frame() && document.frame()->timersPaused();
        if (!didDeferTimeout && timeout <= 100 && singleShot) {
            WKSetObservedContentChange(WKContentIndeterminateChange);
            WebThreadAddObservedContentModifier(timer); // Will only take affect if not already visibility change.
        }
    }
#endif

    timer->suspendIfNeeded();
    InspectorInstrumentation::didInstallTimer(context, timer->m_timeoutId, timeout, singleShot);

    return timer->m_timeoutId;
}

void DOMTimer::removeById(ScriptExecutionContext* context, int timeoutId)
{
    // timeout IDs have to be positive, and 0 and -1 are unsafe to
    // even look up since they are the empty and deleted value
    // respectively
    if (timeoutId <= 0)
        return;

    InspectorInstrumentation::didRemoveTimer(context, timeoutId);
    context->removeTimeout(timeoutId);
}

void DOMTimer::scriptDidInteractWithPlugin(HTMLPlugInElement& pluginElement)
{
    if (!DOMTimerFireState::current)
        return;

    if (pluginElement.isUserObservable())
        DOMTimerFireState::current->scriptDidInteractWithUserObservablePlugin = true;
    else
        DOMTimerFireState::current->scriptDidInteractWithNonUserObservablePlugin = true;
}

void DOMTimer::fired()
{
    // Retain this - if the timer is cancelled while this function is on the stack (implicitly and always
    // for one-shot timers, or if removeById is called on itself from within an interval timer fire) then
    // wait unit the end of this function to delete DOMTimer.
    RefPtr<DOMTimer> reference = this;

    ScriptExecutionContext* context = scriptExecutionContext();
    ASSERT(context);

    DOMTimerFireState fireState(context);

#if PLATFORM(IOS)
    Document* document = nullptr;
    if (context->isDocument()) {
        document = toDocument(context);
        ASSERT(!document->frame()->timersPaused());
    }
#endif
    context->setTimerNestingLevel(std::min(m_nestingLevel + 1, maxTimerNestingLevel));

    ASSERT(!isSuspended());
    ASSERT(!context->activeDOMObjectsAreSuspended());
    UserGestureIndicator gestureIndicator(m_shouldForwardUserGesture ? DefinitelyProcessingUserGesture : PossiblyProcessingUserGesture);
    // Only the first execution of a multi-shot timer should get an affirmative user gesture indicator.
    m_shouldForwardUserGesture = false;

    InspectorInstrumentationCookie cookie = InspectorInstrumentation::willFireTimer(context, m_timeoutId);

    // Simple case for non-one-shot timers.
    if (isActive()) {
        if (m_nestingLevel < maxTimerNestingLevel) {
            m_nestingLevel++;
            updateTimerIntervalIfNecessary();
        }

        m_action->execute(context);

        InspectorInstrumentation::didFireTimer(cookie);

        if (fireState.scriptDidInteractWithUserObservablePlugin && m_throttleState != ShouldNotThrottle) {
            m_throttleState = ShouldNotThrottle;
            updateTimerIntervalIfNecessary();
        } else if (fireState.scriptDidInteractWithNonUserObservablePlugin && m_throttleState == Undetermined) {
            m_throttleState = ShouldThrottle;
            updateTimerIntervalIfNecessary();
        }

        return;
    }

    context->removeTimeout(m_timeoutId);

#if PLATFORM(IOS)
    bool shouldReportLackOfChanges;
    bool shouldBeginObservingChanges;
    if (document) {
        shouldReportLackOfChanges = WebThreadCountOfObservedContentModifiers() == 1;
        shouldBeginObservingChanges = WebThreadContainsObservedContentModifier(this);
    } else {
        shouldReportLackOfChanges = false;
        shouldBeginObservingChanges = false;
    }

    if (shouldBeginObservingChanges) {
        WKBeginObservingContentChanges(false);
        WebThreadRemoveObservedContentModifier(this);
    }
#endif

    m_action->execute(context);

#if PLATFORM(IOS)
    if (shouldBeginObservingChanges) {
        WKStopObservingContentChanges();

        if (WKObservedContentChange() == WKContentVisibilityChange || shouldReportLackOfChanges)
            if (document && document->page())
                document->page()->chrome().client().observedContentChange(document->frame());
    }
#endif

    InspectorInstrumentation::didFireTimer(cookie);

    context->setTimerNestingLevel(0);
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

    double previousInterval = m_currentTimerInterval;
    m_currentTimerInterval = intervalClampedToMinimum();

    if (WTF::withinEpsilon(previousInterval, m_currentTimerInterval))
        return;

    if (repeatInterval()) {
        ASSERT(WTF::withinEpsilon(repeatInterval(), previousInterval));
        augmentRepeatInterval(m_currentTimerInterval - previousInterval);
    } else
        augmentFireInterval(m_currentTimerInterval - previousInterval);
}

double DOMTimer::intervalClampedToMinimum() const
{
    ASSERT(scriptExecutionContext());
    ASSERT(m_nestingLevel <= maxTimerNestingLevel);

    double intervalInSeconds = std::max(oneMillisecond, m_originalInterval * oneMillisecond);

    // Only apply throttling to repeating timers.
    if (m_nestingLevel < maxTimerNestingLevel)
        return intervalInSeconds;

    // Apply two throttles - the global (per Page) minimum, and also a per-timer throttle.
    intervalInSeconds = std::max(intervalInSeconds, scriptExecutionContext()->minimumTimerInterval());
    if (m_throttleState == ShouldThrottle)
        intervalInSeconds = std::max(intervalInSeconds, minIntervalForNonUserObservablePluginScriptTimers * oneMillisecond);
    return intervalInSeconds;
}

double DOMTimer::alignedFireTime(double fireTime) const
{
    double alignmentInterval = scriptExecutionContext()->timerAlignmentInterval();
    if (alignmentInterval) {
        double currentTime = monotonicallyIncreasingTime();
        if (fireTime <= currentTime)
            return fireTime;

        double alignedTime = ceil(fireTime / alignmentInterval) * alignmentInterval;
        return alignedTime;
    }

    return fireTime;
}

} // namespace WebCore
