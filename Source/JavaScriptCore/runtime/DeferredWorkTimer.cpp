/*
 * Copyright (C) 2017-2020 Apple Inc. All rights reserved.
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
#include "DeferredWorkTimer.h"

#include "JSPromise.h"
#include "StrongInlines.h"
#include "VM.h"
#include <wtf/RunLoop.h>

namespace JSC {

namespace DeferredWorkTimerInternal {
static const bool verbose = false;
}

DeferredWorkTimer::DeferredWorkTimer(VM& vm)
    : Base(vm)
{
}

void DeferredWorkTimer::doWork(VM& vm)
{
    ASSERT(vm.currentThreadIsHoldingAPILock());
    m_taskLock.lock();
    cancelTimer();
    if (!m_runTasks) {
        m_taskLock.unlock();
        return;
    }

    while (!m_tasks.isEmpty()) {
        auto [ticket, task] = m_tasks.takeFirst();
        dataLogLnIf(DeferredWorkTimerInternal::verbose, "Doing work on: ", RawPointer(ticket));

        // We may have already canceled these promises.
        if (m_pendingTickets.contains(ticket)) {
            // Allow tasks we run now to schedule work.
            m_currentlyRunningTask = true;
            m_taskLock.unlock(); 

            // This is the start of a runloop turn, we can release any weakrefs here.
            vm.finalizeSynchronousJSExecution();

            auto scope = DECLARE_CATCH_SCOPE(vm);
            task();
            if (Exception* exception = scope.exception()) {
                auto* globalObject = ticket->globalObject();
                scope.clearException();
                globalObject->globalObjectMethodTable()->reportUncaughtExceptionAtEventLoop(globalObject, exception);
            }

            vm.drainMicrotasks();
            ASSERT(!vm.exceptionForInspection());

            m_taskLock.lock();
            m_currentlyRunningTask = false;
        }
    }

    if (m_pendingTickets.isEmpty() && m_shouldStopRunLoopWhenAllTicketsFinish) {
#if USE(CF)
        CFRunLoopStop(vm.runLoop());
#else
        RunLoop::current().stop();
#endif
    }

    m_taskLock.unlock();
}

void DeferredWorkTimer::runRunLoop()
{
    ASSERT(!m_apiLock->vm()->currentThreadIsHoldingAPILock());
#if USE(CF)
    ASSERT(CFRunLoopGetCurrent() == m_apiLock->vm()->runLoop());
#endif
    m_shouldStopRunLoopWhenAllTicketsFinish = true;
    if (m_pendingTickets.size()) {
#if USE(CF)
        CFRunLoopRun();
#else
        RunLoop::run();
#endif
    }
}

void DeferredWorkTimer::addPendingWork(VM& vm, Ticket ticket, Vector<Strong<JSCell>>&& dependencies)
{
    ASSERT(vm.currentThreadIsHoldingAPILock());
    for (unsigned i = 0; i < dependencies.size(); ++i)
        ASSERT(dependencies[i].get() != ticket);

    auto result = m_pendingTickets.add(ticket, Vector<Strong<JSCell>>());
    if (result.isNewEntry) {
        dataLogLnIf(DeferredWorkTimerInternal::verbose, "Adding new pending ticket: ", RawPointer(ticket));
        dependencies.append(Strong<JSCell>(vm, ticket));
        result.iterator->value = WTFMove(dependencies);
    } else {
        dataLogLnIf(DeferredWorkTimerInternal::verbose, "Adding new dependencies for ticket: ", RawPointer(ticket));
        result.iterator->value.appendVector(dependencies);
    }
}

bool DeferredWorkTimer::hasPendingWork(Ticket ticket)
{
    ASSERT(ticket->vm().currentThreadIsHoldingAPILock());
    return m_pendingTickets.contains(ticket);
}

bool DeferredWorkTimer::hasDependancyInPendingWork(Ticket ticket, JSCell* dependency)
{
    ASSERT(ticket->vm().currentThreadIsHoldingAPILock());
    ASSERT(m_pendingTickets.contains(ticket));

    auto result = m_pendingTickets.get(ticket);
    return result.contains(dependency);
}

bool DeferredWorkTimer::cancelPendingWork(Ticket ticket)
{
    ASSERT(ticket->vm().currentThreadIsHoldingAPILock());
    bool result = m_pendingTickets.remove(ticket);

    if (result)
        dataLogLnIf(DeferredWorkTimerInternal::verbose, "Canceling ticket: ", RawPointer(ticket));

    return result;
}

void DeferredWorkTimer::scheduleWorkSoon(Ticket ticket, Task&& task)
{
    LockHolder locker(m_taskLock);
    m_tasks.append(std::make_tuple(ticket, WTFMove(task)));
    if (!isScheduled() && !m_currentlyRunningTask)
        setTimeUntilFire(0_s);
}

} // namespace JSC
