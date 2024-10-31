/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WindowEventLoop.h"

#include "CommonVM.h"
#include "CustomElementReactionQueue.h"
#include "Document.h"
#include "DocumentInlines.h"
#include "HTMLSlotElement.h"
#include "IdleCallbackController.h"
#include "Microtasks.h"
#include "MutationObserver.h"
#include "OpportunisticTaskScheduler.h"
#include "Page.h"
#include "SecurityOrigin.h"
#include "ThreadGlobalData.h"
#include "ThreadTimers.h"
#include <wtf/RobinHoodHashMap.h>
#include <wtf/RunLoop.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

static MemoryCompactRobinHoodHashMap<String, WindowEventLoop*>& windowEventLoopMap()
{
    RELEASE_ASSERT(isMainThread());
    static NeverDestroyed<MemoryCompactRobinHoodHashMap<String, WindowEventLoop*>> map;
    return map.get();
}

static String agentClusterKeyOrNullIfUnique(const SecurityOrigin& origin)
{
    auto computeKey = [&] {
        // https://html.spec.whatwg.org/multipage/webappapis.html#obtain-agent-cluster-key
        if (origin.isOpaque())
            return origin.toString();
        RegistrableDomain registrableDomain { origin.data() };
        if (registrableDomain.isEmpty())
            return origin.toString();
        return makeString(origin.protocol(), "://"_s, registrableDomain.string());
    };
    auto key = computeKey();
    if (key.isEmpty() || key == "null"_s)
        return { };
    return key;
}

Ref<WindowEventLoop> WindowEventLoop::eventLoopForSecurityOrigin(const SecurityOrigin& origin)
{
    auto key = agentClusterKeyOrNullIfUnique(origin);
    if (key.isNull())
        return create({ });

    auto addResult = windowEventLoopMap().add(key, nullptr);
    if (UNLIKELY(addResult.isNewEntry)) {
        auto newEventLoop = create(key);
        addResult.iterator->value = newEventLoop.ptr();
        return newEventLoop;
    }
    return *addResult.iterator->value;
}

inline Ref<WindowEventLoop> WindowEventLoop::create(const String& agentClusterKey)
{
    return adoptRef(*new WindowEventLoop(agentClusterKey));
}

inline WindowEventLoop::WindowEventLoop(const String& agentClusterKey)
    : m_agentClusterKey(agentClusterKey)
    , m_timer(*this, &WindowEventLoop::didReachTimeToRun)
    , m_idleTimer(*this, &WindowEventLoop::didFireIdleTimer)
    , m_perpetualTaskGroupForSimilarOriginWindowAgents(*this)
{
}

WindowEventLoop::~WindowEventLoop()
{
    if (m_agentClusterKey.isNull())
        return;
    auto didRemove = windowEventLoopMap().remove(m_agentClusterKey);
    RELEASE_ASSERT(didRemove);
}

void WindowEventLoop::scheduleToRun()
{
    m_timer.startOneShot(0_s);
}

bool WindowEventLoop::isContextThread() const
{
    return isMainThread();
}

MicrotaskQueue& WindowEventLoop::microtaskQueue()
{
    if (!m_microtaskQueue)
        m_microtaskQueue = makeUnique<MicrotaskQueue>(commonVM(), *this);
    return *m_microtaskQueue;
}

void WindowEventLoop::scheduleIdlePeriod()
{
    m_idleTimer.startOneShot(0_s);
}

void WindowEventLoop::opportunisticallyRunIdleCallbacks(std::optional<MonotonicTime> deadline)
{
    if (shouldEndIdlePeriod())
        return; // No need to schedule m_idleTimer since there is a task. didReachTimeToRun() will call this function.

    auto hasPendingIdleCallbacks = findMatchingAssociatedContext([&](ScriptExecutionContext& context) {
        if (RefPtr document = dynamicDowncast<Document>(context))
            return document->hasPendingIdleCallback();
        return false;
    });

    if (!hasPendingIdleCallbacks)
        return;

    auto now = MonotonicTime::now();
    if (auto scheduledWork = nextScheduledWorkTime()) {
        if (*scheduledWork < now + m_expectedIdleCallbackDuration) {
            // No pending tasks. Schedule m_idleTimer after all DOM timers and rAF.
            auto timeToScheduledWork = *scheduledWork - now;
            if (timeToScheduledWork < 0_s) // Timer may have been scheduled to fire in the past.
                timeToScheduledWork = 0_s;
            decayIdleCallbackDuration();
            m_idleTimer.startOneShot(timeToScheduledWork + 1_ms);
            return;
        }
    }

    if (deadline && *deadline < now + m_expectedIdleCallbackDuration) {
        // No pending tasks. Schedule m_idleTimer immediately.
        decayIdleCallbackDuration();
        m_idleTimer.startOneShot(0_s);
        return;
    }

    m_lastIdlePeriodStartTime = now;

    forEachAssociatedContext([&](ScriptExecutionContext& context) {
        RefPtr document = dynamicDowncast<Document>(context);
        if (!document || !document->hasPendingIdleCallback())
            return;
        auto* idleCallbackController = document->idleCallbackController();
        if (!idleCallbackController)
            return;
        idleCallbackController->startIdlePeriod();
    });

    auto duration = MonotonicTime::now() - m_lastIdlePeriodStartTime;
    m_expectedIdleCallbackDuration = (m_expectedIdleCallbackDuration + duration) / 2;
}

bool WindowEventLoop::shouldEndIdlePeriod()
{
    if (hasTasksForFullyActiveDocument())
        return true;
    if (microtaskQueue().hasMicrotasksForFullyActiveDocument())
        return true;
    return false;
}

MonotonicTime WindowEventLoop::computeIdleDeadline()
{
    auto idleDeadline = m_lastIdlePeriodStartTime + 50_ms;

    auto workTime = nextScheduledWorkTime();
    if (workTime && *workTime < idleDeadline)
        idleDeadline = *workTime;

    return idleDeadline;
}

std::optional<MonotonicTime> WindowEventLoop::nextScheduledWorkTime() const
{
    auto timerTime = nextTimerFireTime();
    auto renderingTime = nextRenderingTime();
    if (!timerTime)
        return renderingTime;
    if (!renderingTime)
        return timerTime;
    return *timerTime < *renderingTime ? *timerTime : *renderingTime;
}

std::optional<MonotonicTime> WindowEventLoop::nextRenderingTime() const
{
    std::optional<MonotonicTime> nextRenderingTime;
    const_cast<WindowEventLoop*>(this)->forEachAssociatedContext([&](ScriptExecutionContext& context) {
        RefPtr document = dynamicDowncast<Document>(context);
        if (!document)
            return;
        RefPtr page = document->page();
        if (!page)
            return;
        auto renderingUpdateTimeForPage = page->nextRenderingUpdateTimestamp();
        if (!renderingUpdateTimeForPage)
            return;
        if (!nextRenderingTime || *renderingUpdateTimeForPage < *nextRenderingTime)
            nextRenderingTime = *renderingUpdateTimeForPage;
    });
    return nextRenderingTime;
}

void WindowEventLoop::didReachTimeToRun()
{
    Ref protectedThis { *this }; // Executing tasks may remove the last reference to this WindowEventLoop.
    auto deadline = ApproximateTime::now() + ThreadTimers::maxDurationOfFiringTimers;
    run(deadline);
    opportunisticallyRunIdleCallbacks(deadline.approximateMonotonicTime());
}

void WindowEventLoop::didFireIdleTimer()
{
    Ref protectedThis { *this }; // Executing idle tasks may remove the last reference to this WindowEventLoop.
    opportunisticallyRunIdleCallbacks();
}

void WindowEventLoop::queueMutationObserverCompoundMicrotask()
{
    if (m_mutationObserverCompoundMicrotaskQueuedFlag)
        return;
    m_mutationObserverCompoundMicrotaskQueuedFlag = true;
    m_perpetualTaskGroupForSimilarOriginWindowAgents.queueMicrotask([this] {
        // We can't make a Ref to WindowEventLoop in the lambda capture as that would result in a reference cycle & leak.
        Ref protectedThis { *this };
        m_mutationObserverCompoundMicrotaskQueuedFlag = false;

        // FIXME: This check doesn't exist in the spec.
        if (m_deliveringMutationRecords)
            return;
        m_deliveringMutationRecords = true;
        MutationObserver::notifyMutationObservers(*this);
        m_deliveringMutationRecords = false;
    });
}

CustomElementQueue& WindowEventLoop::backupElementQueue()
{
    if (!m_processingBackupElementQueue) {
        m_processingBackupElementQueue = true;
        m_perpetualTaskGroupForSimilarOriginWindowAgents.queueMicrotask([this] {
            // We can't make a Ref to WindowEventLoop in the lambda capture as that would result in a reference cycle & leak.
            Ref protectedThis { *this };
            m_processingBackupElementQueue = false;
            ASSERT(m_customElementQueue);
            CustomElementReactionQueue::processBackupQueue(*m_customElementQueue);
        });
    }
    if (!m_customElementQueue)
        m_customElementQueue = makeUnique<CustomElementQueue>();
    return *m_customElementQueue;
}

void WindowEventLoop::breakToAllowRenderingUpdate()
{
#if PLATFORM(MAC)
    // On Mac rendering updates happen in a runloop observer.
    // Avoid running timers and doing other work (like processing asyncronous IPC) until it is completed.

    // FIXME: Also bail out from the task loop in EventLoop::run().
    threadGlobalData().threadTimers().breakFireLoopForRenderingUpdate();

    RunLoop::main().suspendFunctionDispatchForCurrentCycle();
#endif
}

} // namespace WebCore
