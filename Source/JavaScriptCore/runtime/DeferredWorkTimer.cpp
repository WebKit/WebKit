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
    Locker locker { m_taskLock };
    cancelTimer();
    if (!m_runTasks)
        return;

    Vector<std::tuple<Ticket, Task>> suspendedTasks;

    while (!m_tasks.isEmpty()) {
        auto [ticket, task] = m_tasks.takeFirst();
        auto globalObject = ticket->structure(vm)->globalObject();
        dataLogLnIf(DeferredWorkTimerInternal::verbose, "Doing work on: ", RawPointer(ticket));

        auto pendingTicket = m_pendingTickets.find(ticket);
        // We may have already canceled this task or its owner may have been canceled.
        if (pendingTicket == m_pendingTickets.end())
            continue;

        switch (globalObject->globalObjectMethodTable()->scriptExecutionStatus(globalObject, pendingTicket->value.scriptExecutionOwner.get())) {
        case ScriptExecutionStatus::Suspended:
            suspendedTasks.append(std::make_tuple(ticket, WTFMove(task)));
            continue;
        case ScriptExecutionStatus::Stopped:
            m_pendingTickets.remove(pendingTicket);
            continue;
        case ScriptExecutionStatus::Running:
            break;
        }

        // Remove ticket from m_pendingTickets since we are going to run it.
        // But we want to keep ticketData while running task since it ensures dependencies are strongly held.
        auto ticketData = WTFMove(pendingTicket->value);
        m_pendingTickets.remove(pendingTicket);

        // Allow tasks we are about to run to schedule work.
        m_currentlyRunningTask = true;
        {
            auto dropper = DropLockForScope(locker);

            // This is the start of a runloop turn, we can release any weakrefs here.
            vm.finalizeSynchronousJSExecution();

            auto scope = DECLARE_CATCH_SCOPE(vm);
            task(ticket, WTFMove(ticketData));
            if (Exception* exception = scope.exception()) {
                auto* globalObject = ticket->globalObject();
                scope.clearException();
                globalObject->globalObjectMethodTable()->reportUncaughtExceptionAtEventLoop(globalObject, exception);
            }

            vm.drainMicrotasks();
            ASSERT(!vm.exceptionForInspection());
        }
        m_currentlyRunningTask = false;
    }

    while (!suspendedTasks.isEmpty())
        m_tasks.prepend(suspendedTasks.takeLast());

    if (m_pendingTickets.isEmpty() && m_shouldStopRunLoopWhenAllTicketsFinish) {
        ASSERT(m_tasks.isEmpty());
        RunLoop::current().stop();
    }
}

void DeferredWorkTimer::runRunLoop()
{
    ASSERT(!m_apiLock->vm()->currentThreadIsHoldingAPILock());
    ASSERT(&RunLoop::current() == &m_apiLock->vm()->runLoop());
    m_shouldStopRunLoopWhenAllTicketsFinish = true;
    if (m_pendingTickets.size())
        RunLoop::run();
}

void DeferredWorkTimer::addPendingWork(VM& vm, Ticket ticket, Vector<Strong<JSCell>>&& dependencies)
{
    ASSERT(vm.currentThreadIsHoldingAPILock() || (Thread::mayBeGCThread() && vm.heap.worldIsStopped()));
    for (unsigned i = 0; i < dependencies.size(); ++i)
        ASSERT(dependencies[i].get() != ticket);

    auto globalObject = ticket->globalObject();
    auto result = m_pendingTickets.ensure(ticket, [&] {
        dataLogLnIf(DeferredWorkTimerInternal::verbose, "Adding new pending ticket: ", RawPointer(ticket));
        JSObject* scriptExecutionOwner = globalObject->globalObjectMethodTable()->currentScriptExecutionOwner(globalObject);
        dependencies.append(Strong<JSCell>(vm, ticket));
        return TicketData { WTFMove(dependencies), Strong<JSObject>(vm, scriptExecutionOwner) };
    });
    if (!result.isNewEntry) {
        dataLogLnIf(DeferredWorkTimerInternal::verbose, "Adding new dependencies for ticket: ", RawPointer(ticket));
        result.iterator->value.dependencies.appendVector(WTFMove(dependencies));
    }
}

bool DeferredWorkTimer::hasPendingWork(Ticket ticket)
{
    ASSERT(ticket->vm().currentThreadIsHoldingAPILock() || (Thread::mayBeGCThread() && ticket->vm().heap.worldIsStopped()));
    return m_pendingTickets.contains(ticket);
}

bool DeferredWorkTimer::hasDependancyInPendingWork(Ticket ticket, JSCell* dependency)
{
    ASSERT(ticket->vm().currentThreadIsHoldingAPILock() || (Thread::mayBeGCThread() && ticket->vm().heap.worldIsStopped()));
    ASSERT(m_pendingTickets.contains(ticket));

    auto result = m_pendingTickets.get(ticket);
    return result.dependencies.contains(dependency);
}

void DeferredWorkTimer::scheduleWorkSoon(Ticket ticket, Task&& task)
{
    Locker locker { m_taskLock };
    m_tasks.append(std::make_tuple(ticket, WTFMove(task)));
    if (!isScheduled() && !m_currentlyRunningTask)
        setTimeUntilFire(0_s);
}

bool DeferredWorkTimer::cancelPendingWork(Ticket ticket)
{
    ASSERT(ticket->vm().currentThreadIsHoldingAPILock() || (Thread::mayBeGCThread() && ticket->vm().heap.worldIsStopped()));
    bool result = m_pendingTickets.remove(ticket);

    if (result)
        dataLogLnIf(DeferredWorkTimerInternal::verbose, "Canceling ticket: ", RawPointer(ticket));

    return result;
}

void DeferredWorkTimer::didResumeScriptExecutionOwner()
{
    ASSERT(!m_currentlyRunningTask);
    Locker locker { m_taskLock };
    if (!isScheduled() && m_tasks.size())
        setTimeUntilFire(0_s);
}

} // namespace JSC
