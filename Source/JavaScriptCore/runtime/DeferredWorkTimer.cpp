/*
 * Copyright (C) 2017-2021 Apple Inc. All rights reserved.
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
static constexpr bool verbose = false;
}

inline DeferredWorkTimer::TicketData::TicketData(VM& vm, JSObject* scriptExecutionOwner, Vector<Strong<JSCell>>&& dependencies)
    : dependencies(WTFMove(dependencies))
    , scriptExecutionOwner(vm, scriptExecutionOwner)
{
}

inline VM& DeferredWorkTimer::TicketData::vm()
{
    ASSERT(!isCancelled());
    return target()->vm();
}

inline void DeferredWorkTimer::TicketData::cancel()
{
    scriptExecutionOwner.clear();
    dependencies.clear();
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
        dataLogLnIf(DeferredWorkTimerInternal::verbose, "Doing work on: ", RawPointer(ticket));

        auto pendingTicket = m_pendingTickets.find(ticket);
        // We may have already canceled this task or its owner may have been canceled.
        if (pendingTicket == m_pendingTickets.end())
            continue;
        ASSERT(ticket == pendingTicket->get());

        if (ticket->isCancelled()) {
            m_pendingTickets.remove(pendingTicket);
            continue;
        }

        // We shouldn't access the TicketData to get this globalObject until
        // after we confirm that the ticket is still valid (which we did above).
        auto globalObject = ticket->target()->structure()->globalObject();
        switch (globalObject->globalObjectMethodTable()->scriptExecutionStatus(globalObject, ticket->scriptExecutionOwner.get())) {
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
        std::unique_ptr<TicketData> ticketData = m_pendingTickets.take(pendingTicket);

        // Allow tasks we are about to run to schedule work.
        m_currentlyRunningTask = true;
        {
            auto dropper = DropLockForScope(locker);

            // This is the start of a runloop turn, we can release any weakrefs here.
            vm.finalizeSynchronousJSExecution();

            auto scope = DECLARE_CATCH_SCOPE(vm);
            task(ticket);
            ticketData = nullptr;
            if (Exception* exception = scope.exception()) {
                if (scope.clearExceptionExceptTermination())
                    globalObject->globalObjectMethodTable()->reportUncaughtExceptionAtEventLoop(globalObject, exception);
            }

            vm.drainMicrotasks();
            ASSERT(!vm.exceptionForInspection() || vm.hasPendingTerminationException());
        }
        m_currentlyRunningTask = false;
    }

    while (!suspendedTasks.isEmpty())
        m_tasks.prepend(suspendedTasks.takeLast());

    // It is theoretically possible that a client may cancel a pending ticket and
    // never call scheduleWorkSoon() on it. As such, it would not be found when
    // we iterated m_tasks above. We'll need to make sure to purge them here.
    m_pendingTickets.removeIf([] (auto& ticket) {
        return ticket->isCancelled();
    });

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

DeferredWorkTimer::Ticket DeferredWorkTimer::addPendingWork(VM& vm, JSObject* target, Vector<Strong<JSCell>>&& dependencies)
{
    ASSERT(vm.currentThreadIsHoldingAPILock() || (Thread::mayBeGCThread() && vm.heap.worldIsStopped()));
    for (unsigned i = 0; i < dependencies.size(); ++i)
        ASSERT(dependencies[i].get() != target);

    auto* globalObject = target->globalObject();
    JSObject* scriptExecutionOwner = globalObject->globalObjectMethodTable()->currentScriptExecutionOwner(globalObject);
    dependencies.append(Strong<JSCell>(vm, target));

    auto ticketData = makeUnique<TicketData>(vm, scriptExecutionOwner, WTFMove(dependencies));
    Ticket ticket = ticketData.get();

    dataLogLnIf(DeferredWorkTimerInternal::verbose, "Adding new pending ticket: ", RawPointer(ticket));
    auto result = m_pendingTickets.add(WTFMove(ticketData));
    RELEASE_ASSERT(result.isNewEntry);

    return ticket;
}

bool DeferredWorkTimer::hasPendingWork(Ticket ticket)
{
    auto result = m_pendingTickets.find(ticket);
    if (result == m_pendingTickets.end() || ticket->isCancelled())
        return false;
    ASSERT(ticket->vm().currentThreadIsHoldingAPILock() || (Thread::mayBeGCThread() && ticket->vm().heap.worldIsStopped()));
    return true;
}

bool DeferredWorkTimer::hasDependancyInPendingWork(Ticket ticket, JSCell* dependency)
{
    auto result = m_pendingTickets.find(ticket);
    if (result == m_pendingTickets.end() || ticket->isCancelled())
        return false;
    ASSERT(ticket->vm().currentThreadIsHoldingAPILock() || (Thread::mayBeGCThread() && ticket->vm().heap.worldIsStopped()));
    return (*result)->dependencies.contains(dependency);
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
    ASSERT(m_pendingTickets.contains(ticket));
    ASSERT(ticket->isCancelled() || ticket->vm().currentThreadIsHoldingAPILock() || (Thread::mayBeGCThread() && ticket->vm().heap.worldIsStopped()));

    bool result = false;
    if (!ticket->isCancelled()) {
        dataLogLnIf(DeferredWorkTimerInternal::verbose, "Canceling ticket: ", RawPointer(ticket));
        ticket->cancel();
        result = true;
    }

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
