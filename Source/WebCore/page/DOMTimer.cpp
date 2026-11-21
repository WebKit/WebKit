/*
 * Copyright (C) 2008, 2014 Apple Inc. All rights reserved.
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

#include "ContextDestructionObserverInlines.h"
#include "DocumentEventLoop.h"
#include "InspectorInstrumentation.h"
#include "Logging.h"
#include "OpportunisticTaskScheduler.h"
#include "Page.h"
#include "ScheduledAction.h"
#include "ScriptExecutionContextInlines.h"
#include "UserGestureIndicator.h"
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/MathExtras.h>
#include <wtf/Scope.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include "DocumentPage.h"

#if ENABLE(CONTENT_CHANGE_OBSERVER)
#include "ContentChangeObserver.h"
#include "DOMTimerHoldingTank.h"
#endif

namespace WebCore {

static constexpr Seconds minIntervalForOneShotTimers { 0_ms };
static constexpr Seconds minIntervalForRepeatingTimers { 1_ms };
static constexpr int maxTimerNestingLevel = 10;
static constexpr int maxTimerNestingLevelForOneShotTimers = 10;
static constexpr int maxTimerNestingLevelForRepeatingTimers = 5;

WTF_MAKE_TZONE_ALLOCATED_IMPL(DOMTimer);

DOMTimer::DOMTimer(ScriptExecutionContext& context, Function<void(ScriptExecutionContext&)>&& action, Seconds interval, Type type)
    : ActiveDOMObject(&context)
    , m_nestingLevel(context.timerNestingLevel())
    , m_action(WTF::move(action))
    , m_originalInterval(interval)
    , m_oneShot(type == Type::SingleShot)
    , m_currentTimerInterval(intervalClampedToMinimum())
    , m_userGestureTokenToForward(UserGestureIndicator::currentUserGesture())
{
    CheckedRef eventLoop = context.eventLoop();
    m_hasReachedMaxNestingLevel = m_nestingLevel >= (m_oneShot ? maxTimerNestingLevelForOneShotTimers : maxTimerNestingLevelForRepeatingTimers);
    if (m_oneShot) {
        m_timer = eventLoop->scheduleTask(m_currentTimerInterval, context, m_hasReachedMaxNestingLevel ? HasReachedMaxNestingLevel::Yes : HasReachedMaxNestingLevel::No, TaskSource::Timer, [weakThis = WeakPtr { *this }] {
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->fired();
        });
    } else {
        m_timer = eventLoop->scheduleRepeatingTask(m_originalInterval, m_currentTimerInterval, context, m_hasReachedMaxNestingLevel ? HasReachedMaxNestingLevel::Yes : HasReachedMaxNestingLevel::No, TaskSource::Timer, [weakThis = WeakPtr { *this }] {
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->fired();
        });
    }
}

DOMTimer::~DOMTimer() = default;

int DOMTimer::install(ScriptExecutionContext& context, std::unique_ptr<ScheduledAction> action, Seconds timeout, Type type)
{
    auto actionFunction = [action = WTF::move(action)](ScriptExecutionContext& context) mutable {
        action->execute(context);
    };
    return DOMTimer::install(context, WTF::move(actionFunction), timeout, type);
}

int DOMTimer::install(ScriptExecutionContext& context, Function<void(ScriptExecutionContext&)>&& action, Seconds timeout, Type type)
{
    Ref timer = adoptRef(*new DOMTimer(context, WTF::move(action), timeout, type));
    timer->suspendIfNeeded();
    timer->makeImminentlyScheduledWorkScopeIfPossible(context);

    // Keep asking for the next id until we're given one that we don't already have.
    do {
        timer->m_timeoutId = context.circularSequentialID();
    } while (!context.addTimeout(timer->m_timeoutId, timer.get()));

    InspectorInstrumentation::didInstallTimer(context, timer->m_timeoutId, timeout, type == Type::SingleShot);

#if ENABLE(CONTENT_CHANGE_OBSERVER)
    if (RefPtr document = dynamicDowncast<Document>(context)) {
        protect(document->contentChangeObserver())->didInstallDOMTimer(timer.get(), timeout, type == Type::SingleShot);
        if (DeferDOMTimersForScope::isDeferring())
            protect(document->domTimerHoldingTank())->add(timer.get());
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
    if (RefPtr document = dynamicDowncast<Document>(context)) {
        if (RefPtr timer = document->findTimeout(timeoutId)) {
            protect(document->contentChangeObserver())->didRemoveDOMTimer(*timer);
            if (auto* holdingTank = document->domTimerHoldingTankIfExists())
                holdingTank->remove(*timer);
        }
    }
#endif

    InspectorInstrumentation::didRemoveTimer(context, timeoutId);

    if (RefPtr timer = context.takeTimeout(timeoutId)) {
        timer->clearImminentlyScheduledWorkScope();
        timer->m_timer = nullptr;
    }
}

void DOMTimer::fired()
{
    // Retain this - if the timer is cancelled while this function is on the stack (implicitly and always
    // for one-shot timers, or if removeById is called on itself from within an interval timer fire) then
    // wait unit the end of this function to delete DOMTimer.
    Ref protectedThis { *this };

    ASSERT(scriptExecutionContext());
    Ref context = *scriptExecutionContext();

#if ENABLE(CONTENT_CHANGE_OBSERVER)
    if (RefPtr document = dynamicDowncast<Document>(context); document && m_oneShot) {
        if (auto* holdingTank = document->domTimerHoldingTankIfExists(); holdingTank && holdingTank->contains(*this)) {
            m_timer = protect(document->eventLoop())->scheduleTask(0_s, TaskSource::Timer, [weakThis = WeakPtr { *this }] {
                if (RefPtr protectedThis = weakThis.get())
                    protectedThis->fired();
            });
            return;
        }
    }
#endif

    context->setTimerNestingLevel(std::min(m_nestingLevel + 1, maxTimerNestingLevel));
    auto resetTimerNestingLevel = makeScopeExit([&] {
        context->setTimerNestingLevel(0);
    });

    if (RefPtr userGestureTokenToForward = m_userGestureTokenToForward; userGestureTokenToForward && userGestureTokenToForward->hasExpired(UserGestureToken::maximumIntervalForUserGestureForwarding))
        m_userGestureTokenToForward = nullptr;

    ASSERT(!context->activeDOMObjectsAreSuspended());
    UserGestureIndicator gestureIndicator(m_userGestureTokenToForward);
    // Only the first execution of a multi-shot timer should get an affirmative user gesture indicator.
    m_userGestureTokenToForward = nullptr;

    InspectorInstrumentation::willFireTimer(context, m_timeoutId, m_oneShot);

    // Simple case for non-one-shot timers.
    if (!m_oneShot) {
        if (m_nestingLevel < maxTimerNestingLevel) {
            m_nestingLevel++;
            m_hasReachedMaxNestingLevel = m_nestingLevel >= maxTimerNestingLevelForRepeatingTimers;
            protect(context->eventLoop())->setTimerHasReachedMaxNestingLevel(m_timer, m_hasReachedMaxNestingLevel);
            updateTimerIntervalIfNecessary();
        }

        m_action(context);

        InspectorInstrumentation::didFireTimer(context, m_timeoutId, m_oneShot);

        clearImminentlyScheduledWorkScope();
        return;
    }

    context->takeTimeout(m_timeoutId);

#if ENABLE(CONTENT_CHANGE_OBSERVER)
    ContentChangeObserver::DOMTimerScope observingScope(dynamicDowncast<Document>(context.get()), *this);
#endif
    m_action(context);

    InspectorInstrumentation::didFireTimer(context, m_timeoutId, m_oneShot);

    clearImminentlyScheduledWorkScope();
}

void DOMTimer::stop()
{
    // Need to release JS objects potentially protected by ScheduledAction
    // because they can form circular references back to the ScriptExecutionContext
    // which will cause a memory leak.
    m_timer = nullptr;
    m_action = nullptr;

    clearImminentlyScheduledWorkScope();
}

void DOMTimer::updateTimerIntervalIfNecessary()
{
    ASSERT(m_nestingLevel <= maxTimerNestingLevel);

    if (!scriptExecutionContext())
        return;

    auto previousInterval = m_currentTimerInterval;
    m_currentTimerInterval = intervalClampedToMinimum();
    if (previousInterval == m_currentTimerInterval)
        return;

    Ref context = *scriptExecutionContext();
    if (m_oneShot) {
        LOG(DOMTimers, "%p - Updating DOMTimer's fire interval from %.2f ms to %.2f ms due to throttling.", this, previousInterval.milliseconds(), m_currentTimerInterval.milliseconds());
        protect(context->eventLoop())->adjustTimerNextFireTime(m_timer, m_currentTimerInterval - previousInterval);
    } else {
        LOG(DOMTimers, "%p - Updating DOMTimer's repeat interval from %.2f ms to %.2f ms due to throttling.", this, previousInterval.milliseconds(), m_currentTimerInterval.milliseconds());
        protect(context->eventLoop())->adjustTimerRepeatInterval(m_timer, m_currentTimerInterval - previousInterval);
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

    // Apply the global (per Page) minimum.
    return std::max(interval, protect(scriptExecutionContext())->minimumDOMTimerInterval());
}

MonotonicTime ScriptExecutionContext::alignedFireTime(bool hasReachedMaxNestingLevel, MonotonicTime fireTime) const
{
    Seconds alignmentInterval = domTimerAlignmentInterval(hasReachedMaxNestingLevel);
    if (!alignmentInterval)
        return fireTime;
    
    static const double randomizedProportion = cryptographicallyRandomUnitInterval();

    // Force alignment to randomizedAlignment fraction of the way between alignemntIntervals, e.g.
    // if alignmentInterval is 10_ms and randomizedAlignment is 0.3 this will align to 3, 13, 23, ...
    Seconds randomizedOffset = alignmentInterval * randomizedProportion;
    MonotonicTime adjustedFireTime = fireTime - randomizedOffset;
    return adjustedFireTime - (adjustedFireTime % alignmentInterval) + alignmentInterval + randomizedOffset;
}

void DOMTimer::makeImminentlyScheduledWorkScopeIfPossible(ScriptExecutionContext& context)
{
    if (!m_oneShot || m_currentTimerInterval > 1_ms)
        return;

    RefPtr document = dynamicDowncast<Document>(context);
    if (!document)
        return;

    RefPtr page = document->page();
    if (!page)
        return;

    m_imminentlyScheduledWorkScope = page->opportunisticTaskScheduler().makeScheduledWorkScope();
}

void DOMTimer::clearImminentlyScheduledWorkScope()
{
    m_imminentlyScheduledWorkScope = nullptr;
}

} // namespace WebCore
