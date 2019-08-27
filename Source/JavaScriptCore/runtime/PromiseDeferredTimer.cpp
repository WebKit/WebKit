/*
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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
 */

#include "config.h"
#include "PromiseDeferredTimer.h"

#include "JSPromiseDeferred.h"
#include "StrongInlines.h"
#include "VM.h"
#include <wtf/Locker.h>
#include <wtf/RunLoop.h>

namespace JSC {

namespace PromiseDeferredTimerInternal {
static const bool verbose = false;
}

PromiseDeferredTimer::PromiseDeferredTimer(VM& vm)
    : Base(vm)
{
}

void PromiseDeferredTimer::doWork(VM& vm)
{
    ASSERT(vm.currentThreadIsHoldingAPILock());
    m_taskLock.lock();
    cancelTimer();
    if (!m_runTasks) {
        m_taskLock.unlock();
        return;
    }

    while (!m_tasks.isEmpty()) {
        auto [ticket, task] = m_tasks.takeLast();
        dataLogLnIf(PromiseDeferredTimerInternal::verbose, "Doing work on promise: ", RawPointer(ticket));

        // We may have already canceled these promises.
        if (m_pendingPromises.contains(ticket)) {
            // Allow tasks we run now to schedule work.
            m_currentlyRunningTask = true;
            m_taskLock.unlock(); 

            task();
            vm.drainMicrotasks();

            m_taskLock.lock();
            m_currentlyRunningTask = false;
        }
    }

    if (m_pendingPromises.isEmpty() && m_shouldStopRunLoopWhenAllPromisesFinish) {
#if USE(CF)
        CFRunLoopStop(vm.runLoop());
#else
        RunLoop::current().stop();
#endif
    }

    m_taskLock.unlock();
}

void PromiseDeferredTimer::runRunLoop()
{
    ASSERT(!m_apiLock->vm()->currentThreadIsHoldingAPILock());
#if USE(CF)
    ASSERT(CFRunLoopGetCurrent() == m_apiLock->vm()->runLoop());
#endif
    m_shouldStopRunLoopWhenAllPromisesFinish = true;
    if (m_pendingPromises.size()) {
#if USE(CF)
        CFRunLoopRun();
#else
        RunLoop::run();
#endif
    }
}

void PromiseDeferredTimer::addPendingPromise(VM& vm, JSPromiseDeferred* ticket, Vector<Strong<JSCell>>&& dependencies)
{
    ASSERT(vm.currentThreadIsHoldingAPILock());
    for (unsigned i = 0; i < dependencies.size(); ++i)
        ASSERT(dependencies[i].get() != ticket);

    auto result = m_pendingPromises.add(ticket, Vector<Strong<JSCell>>());
    if (result.isNewEntry) {
        dataLogLnIf(PromiseDeferredTimerInternal::verbose, "Adding new pending promise: ", RawPointer(ticket));
        dependencies.append(Strong<JSCell>(vm, ticket));
        result.iterator->value = WTFMove(dependencies);
    } else {
        dataLogLnIf(PromiseDeferredTimerInternal::verbose, "Adding new dependencies for promise: ", RawPointer(ticket));
        result.iterator->value.appendVector(dependencies);
    }

#ifndef NDEBUG
    ticket->promiseAsyncPending();
#endif
}

bool PromiseDeferredTimer::hasPendingPromise(JSPromiseDeferred* ticket)
{
    ASSERT(ticket->vm().currentThreadIsHoldingAPILock());
    return m_pendingPromises.contains(ticket);
}

bool PromiseDeferredTimer::hasDependancyInPendingPromise(JSPromiseDeferred* ticket, JSCell* dependency)
{
    ASSERT(ticket->vm().currentThreadIsHoldingAPILock());
    ASSERT(m_pendingPromises.contains(ticket));

    auto result = m_pendingPromises.get(ticket);
    return result.contains(dependency);
}

bool PromiseDeferredTimer::cancelPendingPromise(JSPromiseDeferred* ticket)
{
    ASSERT(ticket->vm().currentThreadIsHoldingAPILock());
    bool result = m_pendingPromises.remove(ticket);

    if (result)
        dataLogLnIf(PromiseDeferredTimerInternal::verbose, "Canceling promise: ", RawPointer(ticket));

    return result;
}

void PromiseDeferredTimer::scheduleWorkSoon(JSPromiseDeferred* ticket, Task&& task)
{
    LockHolder locker(m_taskLock);
    m_tasks.append(std::make_tuple(ticket, WTFMove(task)));
    if (!isScheduled() && !m_currentlyRunningTask)
        setTimeUntilFire(0_s);
}

} // namespace JSC
