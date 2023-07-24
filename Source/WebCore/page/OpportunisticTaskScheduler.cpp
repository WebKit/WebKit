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

namespace WebCore {

OpportunisticTaskDeferralScope::OpportunisticTaskDeferralScope(OpportunisticTaskScheduler& scheduler)
    : m_scheduler(&scheduler)
{
    scheduler.incrementDeferralCount();
}

OpportunisticTaskDeferralScope::OpportunisticTaskDeferralScope(OpportunisticTaskDeferralScope&& scope)
    : m_scheduler(std::exchange(scope.m_scheduler, { }))
{
}

OpportunisticTaskDeferralScope::~OpportunisticTaskDeferralScope()
{
    if (m_scheduler)
        m_scheduler->decrementDeferralCount();
}

OpportunisticTaskScheduler::OpportunisticTaskScheduler(Page& page)
    : m_page(&page)
    , m_runLoopObserver(makeUnique<RunLoopObserver>(RunLoopObserver::WellKnownOrder::PostRenderingUpdate, [weakThis = WeakPtr { this }] {
        if (auto strongThis = weakThis.get())
            strongThis->runLoopObserverFired();
    }, RunLoopObserver::Type::OneShot))
{
}

OpportunisticTaskScheduler::~OpportunisticTaskScheduler() = default;

void OpportunisticTaskScheduler::reschedule(MonotonicTime deadline)
{
    m_currentDeadline = deadline;

    if (m_runLoopObserver->isScheduled())
        return;

    if (m_taskDeferralCount)
        m_runLoopObserver->invalidate();
    else
        m_runLoopObserver->schedule();
}

std::unique_ptr<OpportunisticTaskDeferralScope> OpportunisticTaskScheduler::makeDeferralScope()
{
    return makeUnique<OpportunisticTaskDeferralScope>(*this);
}

void OpportunisticTaskScheduler::runLoopObserverFired()
{
    m_runLoopObserver->invalidate();

    if (m_taskDeferralCount)
        return;

    auto deadline = std::exchange(m_currentDeadline, MonotonicTime { });
    if (!deadline)
        return;

    if (ApproximateTime::now().secondsSinceEpoch() > deadline.secondsSinceEpoch())
        return;

    if (auto page = m_page.get())
        page->performOpportunisticallyScheduledTasks(deadline);
}

void OpportunisticTaskScheduler::incrementDeferralCount()
{
    if (++m_taskDeferralCount == 1)
        m_runLoopObserver->invalidate();
}

void OpportunisticTaskScheduler::decrementDeferralCount()
{
    if (!--m_taskDeferralCount && m_currentDeadline)
        m_runLoopObserver->schedule();
}

} // namespace WebCore
