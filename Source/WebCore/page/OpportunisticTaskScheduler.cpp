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

#include "CommonVM.h"
#include "GCController.h"
#include "IdleCallbackController.h"
#include "Page.h"
#include <JavaScriptCore/HeapInlines.h>
#include <JavaScriptCore/JSGlobalObject.h>
#include <wtf/DataLog.h>
#include <wtf/SystemTracing.h>

namespace WebCore {

OpportunisticTaskScheduler::OpportunisticTaskScheduler(Page& page)
    : m_page(&page)
    , m_runLoopObserver(makeUnique<RunLoopObserver>(RunLoopObserver::WellKnownOrder::PostRenderingUpdate, [weakThis = WeakPtr { this }] {
        if (auto protectedThis = weakThis.get())
            protectedThis->runLoopObserverFired();
    }, RunLoopObserver::Type::OneShot))
{
}

OpportunisticTaskScheduler::~OpportunisticTaskScheduler() = default;

void OpportunisticTaskScheduler::rescheduleIfNeeded(MonotonicTime deadline)
{
    RefPtr page = m_page.get();
    if (page->isWaitingForLoadToFinish() || !page->isVisibleAndActive())
        return;

    auto hasIdleCallbacks = page->findMatchingLocalDocument([](const Document& document) {
        return document.hasPendingIdleCallback();
    });
    if (!hasIdleCallbacks && !page->settings().opportunisticSweepingAndGarbageCollectionEnabled())
        return;

    m_runloopCountAfterBeingScheduled = 0;
    m_currentDeadline = deadline;
    m_runLoopObserver->invalidate();
    if (!m_runLoopObserver->isScheduled())
        m_runLoopObserver->schedule();
}

Ref<ImminentlyScheduledWorkScope> OpportunisticTaskScheduler::makeScheduledWorkScope()
{
    return ImminentlyScheduledWorkScope::create(*this);
}

void OpportunisticTaskScheduler::runLoopObserverFired()
{
    constexpr bool verbose = false;

    if (!m_currentDeadline)
        return;

#if USE(WEB_THREAD)
    if (WebThreadIsEnabled())
        return;
#endif

    if (UNLIKELY(!m_page))
        return;

    RefPtr page = m_page.get();
    if (page->isWaitingForLoadToFinish())
        return;

    if (!page->isVisibleAndActive())
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

        dataLogLnIf(verbose, "[OPPORTUNISTIC TASK] GaveUp: task does not get scheduled ", remainingTime, " ", hasImminentlyScheduledWork(), " ", page->preferredRenderingUpdateInterval(), " ", m_runloopCountAfterBeingScheduled, " signpost:(", JSC::activeJSGlobalObjectSignpostIntervalCount.load(), ")");
        return false;
    }();

    if (!shouldRunTask) {
        dataLogLnIf(verbose, "[OPPORTUNISTIC TASK] RunLoopObserverInvalidate", " signpost:(", JSC::activeJSGlobalObjectSignpostIntervalCount.load(), ")");
        m_runLoopObserver->invalidate();
        m_runLoopObserver->schedule();
        return;
    }

    TraceScope tracingScope {
        PerformOpportunisticallyScheduledTasksStart,
        PerformOpportunisticallyScheduledTasksEnd,
        static_cast<uint64_t>(remainingTime.microseconds())
    };

    auto deadline = std::exchange(m_currentDeadline, MonotonicTime { });
    page->opportunisticallyRunIdleCallbacks(deadline);

    if (!page->settings().opportunisticSweepingAndGarbageCollectionEnabled()) {
        dataLogLnIf(verbose, "[OPPORTUNISTIC TASK] GaveUp: opportunistic sweep and GC is not enabled", " signpost:(", JSC::activeJSGlobalObjectSignpostIntervalCount.load(), ")");
        return;
    }

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

static bool isBusyForTimerBasedGC(JSC::VM& vm)
{
    bool isVisibleAndActive = false;
    bool hasPendingTasks = false;
    bool opportunisticSweepingAndGarbageCollectionEnabled = false;
    Page::forEachPage([&](auto& page) {
        if (page.isVisibleAndActive())
            isVisibleAndActive = true;
        if (page.isWaitingForLoadToFinish())
            hasPendingTasks = true;
        if (page.opportunisticTaskScheduler().hasImminentlyScheduledWork())
            hasPendingTasks = true;
        if (vm.deferredWorkTimer->hasImminentlyScheduledWork())
            hasPendingTasks = true;
        if (page.settings().opportunisticSweepingAndGarbageCollectionEnabled())
            opportunisticSweepingAndGarbageCollectionEnabled = true;
    });

    // If all pages are not visible, we do not care about this GC tasks. We should just run as requested.
    return opportunisticSweepingAndGarbageCollectionEnabled && isVisibleAndActive && hasPendingTasks;
}

OpportunisticTaskScheduler::FullGCActivityCallback::FullGCActivityCallback(JSC::Heap& heap)
    : Base(heap, JSC::Synchronousness::Sync)
    , m_vm(heap.vm())
    , m_runLoopObserver(makeUnique<RunLoopObserver>(RunLoopObserver::WellKnownOrder::PostRenderingUpdate, [this] {
        JSC::JSLockHolder locker(m_vm);
        m_version = 0;
        m_deferCount = 0;
        Base::doCollection(m_vm);
    }, RunLoopObserver::Type::OneShot))
{
}

// We would like to keep FullGCActivityCallback::doCollection and EdenGCActivityCallback::doCollection separate
// since we would like to encode more and more different heuristics for them.
void OpportunisticTaskScheduler::FullGCActivityCallback::doCollection(JSC::VM& vm)
{
    constexpr Seconds delay { 100_ms };
    constexpr unsigned deferCountThreshold = 3;

    if (isBusyForTimerBasedGC(vm)) {
        if (!m_version || m_version != vm.heap.objectSpace().markingVersion()) {
            m_version = vm.heap.objectSpace().markingVersion();
            m_deferCount = 0;
            m_delay = delay;
            setTimeUntilFire(delay);
            return;
        }

        // deferredWorkTimer->hasImminentlyScheduledWork() typically means a wasm compilation is happening right now so we REALLY don't want to GC now.
        if (++m_deferCount < deferCountThreshold || vm.deferredWorkTimer->hasImminentlyScheduledWork()) {
            m_delay = delay;
            setTimeUntilFire(delay);
            return;
        }

        m_runLoopObserver->invalidate();
        m_runLoopObserver->schedule();
        return;
    }

    JSC::JSLockHolder locker(m_vm);
    m_version = 0;
    m_deferCount = 0;
    Base::doCollection(vm);
}

OpportunisticTaskScheduler::EdenGCActivityCallback::EdenGCActivityCallback(JSC::Heap& heap)
    : Base(heap, JSC::Synchronousness::Sync)
    , m_vm(heap.vm())
    , m_runLoopObserver(makeUnique<RunLoopObserver>(RunLoopObserver::WellKnownOrder::PostRenderingUpdate, [this] {
        JSC::JSLockHolder locker(m_vm);
        m_version = 0;
        m_deferCount = 0;
        Base::doCollection(m_vm);
    }, RunLoopObserver::Type::OneShot))
{
}

void OpportunisticTaskScheduler::EdenGCActivityCallback::doCollection(JSC::VM& vm)
{
    constexpr Seconds delay { 10_ms };
    constexpr unsigned deferCountThreshold = 5;

    if (isBusyForTimerBasedGC(vm)) {
        if (!m_version || m_version != vm.heap.objectSpace().edenVersion()) {
            m_version = vm.heap.objectSpace().edenVersion();
            m_deferCount = 0;
            m_delay = delay;
            setTimeUntilFire(delay);
            return;
        }

        // deferredWorkTimer->hasImminentlyScheduledWork() typically means a wasm compilation is happening right now so we REALLY don't want to GC now.
        if (++m_deferCount < deferCountThreshold || vm.deferredWorkTimer->hasImminentlyScheduledWork()) {
            m_delay = delay;
            setTimeUntilFire(delay);
            return;
        }

        m_runLoopObserver->invalidate();
        m_runLoopObserver->schedule();
        return;
    }

    JSC::JSLockHolder locker(m_vm);
    m_version = 0;
    m_deferCount = 0;
    Base::doCollection(m_vm);
}

} // namespace WebCore
