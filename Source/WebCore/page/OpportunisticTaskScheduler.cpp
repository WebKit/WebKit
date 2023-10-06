/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "OpportunisticTaskScheduler.h"

#include "Page.h"
#include <wtf/SystemTracing.h>

namespace WebCore {

OpportunisticTaskScheduler::OpportunisticTaskScheduler(Page& page)
    : m_page(&page)
    , m_runLoopObserver(makeUnique<RunLoopObserver>(RunLoopObserver::WellKnownOrder::PostRenderingUpdate, [weakThis = WeakPtr { this }] {
        if (auto strongThis = weakThis.get())
            strongThis->runLoopObserverFired();
    }, RunLoopObserver::Type::Repeating))
{
    m_runLoopObserver->schedule();
}

OpportunisticTaskScheduler::~OpportunisticTaskScheduler() = default;

void OpportunisticTaskScheduler::reschedule(MonotonicTime deadline)
{
    m_runloopCountAfterBeingScheduled = 0;
    m_currentDeadline = deadline;
}

Ref<ImminentlyScheduledWorkScope> OpportunisticTaskScheduler::makeScheduledWorkScope()
{
    return ImminentlyScheduledWorkScope::create(*this);
}

void OpportunisticTaskScheduler::runLoopObserverFired()
{
    if (!m_currentDeadline)
        return;

    auto page = m_page;
    if (UNLIKELY(!page))
        return;

    if (page->isWaitingForLoadToFinish() || !page->isVisibleAndActive())
        return;

    auto currentTime = ApproximateTime::now();
    auto remainingTime = m_currentDeadline.secondsSinceEpoch() - currentTime.secondsSinceEpoch();
    if (remainingTime < 0_ms)
        return;

    m_runloopCountAfterBeingScheduled++;

    bool shouldRunTask = [&] {
        if (!hasImminentlyScheduledWork())
            return true;

        static constexpr auto fractionOfRenderingIntervalWhenScheduledWorkIsImminent = 0.72;
        if (remainingTime > fractionOfRenderingIntervalWhenScheduledWorkIsImminent * page->preferredRenderingUpdateInterval())
            return true;

        static constexpr auto minimumRunloopCountWhenScheduledWorkIsImminent = 4;
        if (m_runloopCountAfterBeingScheduled > minimumRunloopCountWhenScheduledWorkIsImminent)
            return true;

        return false;
    }();

    if (!shouldRunTask)
        return;

    TraceScope tracingScope {
        PerformOpportunisticallyScheduledTasksStart,
        PerformOpportunisticallyScheduledTasksEnd,
        static_cast<uint64_t>(remainingTime.microseconds())
    };

    auto deadline = std::exchange(m_currentDeadline, MonotonicTime { });
    page->opportunisticallyRunIdleCallbacks();
    if (UNLIKELY(!page))
        return;

    if (!page->settings().opportunisticSweepingAndGarbageCollectionEnabled())
        return;

    page->performOpportunisticallyScheduledTasks(deadline);
}

ImminentlyScheduledWorkScope::ImminentlyScheduledWorkScope(OpportunisticTaskScheduler& scheduler)
    : m_scheduler(&scheduler)
{
    scheduler.m_imminentlyScheduledWorkCount++;
}

ImminentlyScheduledWorkScope::~ImminentlyScheduledWorkScope()
{
    if (m_scheduler)
        m_scheduler->m_imminentlyScheduledWorkCount--;
}

} // namespace WebCore
