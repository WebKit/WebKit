/*
 * Copyright (C) 2017-2024 Apple Inc. All rights reserved.
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

#include "CatchScope.h"
#include "DeferredWorkTimerInlines.h"
#include "GlobalObjectMethodTable.h"
#include "JSGlobalObject.h"
#include "VM.h"
#include <wtf/RunLoop.h>
#include <wtf/Scope.h>
#include <wtf/TZoneMallocInlines.h>

namespace JSC {

WTF_MAKE_TZONE_ALLOCATED_IMPL(DeferredWorkTimer::TicketData);

namespace DeferredWorkTimerInternal {
static constexpr bool verbose = false;
}

inline DeferredWorkTimer::TicketData::TicketData(WorkType type, JSObject* scriptExecutionOwner, Vector<JSCell*>&& dependencies)
    : m_type(type)
    , m_dependencies(WTFMove(dependencies))
    , m_scriptExecutionOwner(scriptExecutionOwner)
{
    ASSERT_WITH_MESSAGE(!m_dependencies.isEmpty(), "dependencies shouldn't be empty since it should contain the target");
    ASSERT_WITH_MESSAGE(isTargetObject(), "target must be a JSObject");
    target()->globalObject()->addWeakTicket(this);
}

inline Ref<DeferredWorkTimer::TicketData> DeferredWorkTimer::TicketData::create(WorkType type, JSObject* scriptExecutionOwner, Vector<JSCell*>&& dependencies)
{
    return adoptRef(*new TicketData(type, scriptExecutionOwner, WTFMove(dependencies)));
}

inline VM& DeferredWorkTimer::TicketData::vm()
{
    ASSERT(!isCancelled());
    return target()->vm();
}

inline void DeferredWorkTimer::TicketData::cancel()
{
    dataLogLnIf(DeferredWorkTimerInternal::verbose, "Canceling ticket: ", RawPointer(this));
    m_isCancelled = true;
}

inline void DeferredWorkTimer::TicketData::cancelAndClear()
{
    cancel();
    m_dependencies.clear();
    m_scriptExecutionOwner = nullptr;
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

    SUPPRESS_UNCOUNTED_LAMBDA_CAPTURE auto stopRunLoopIfNecessary = makeScopeExit([&] {
        locker.assertIsHolding(m_taskLock);
        if (vm.hasPendingTerminationException()) {
            vm.setExecutionForbidden();
            if (m_shouldStopRunLoopWhenAllTicketsFinish)
                RunLoop::currentSingleton().stop();
            return;
        }

        if (m_shouldStopRunLoopWhenAllTicketsFinish && m_pendingTickets.isEmpty()) {
            ASSERT(m_tasks.isEmpty());
            RunLoop::currentSingleton().stop();
        }
    });

    Vector<std::tuple<Ticket, Task>> suspendedTasks;

    while (!m_tasks.isEmpty()) {
        auto [ticket, task] = m_tasks.takeFirst();
        dataLogLnIf(DeferredWorkTimerInternal::verbose, "Doing work on: ", RawPointer(ticket));

        auto pendingTicket = m_pendingTickets.find(ticket);
        // We may have already canceled this task or its owner may have been canceled.
        if (pendingTicket == m_pendingTickets.end())
            continue;
        ASSERT(ticket == pendingTicket->ptr());

        if (ticket->isCancelled()) {
            m_pendingTickets.remove(pendingTicket);
            continue;
        }

        // We shouldn't access the TicketData to get this globalObject until
        // after we confirm that the ticket is still valid (which we did above).
        auto globalObject = ticket->target()->globalObject();
        switch (globalObject->globalObjectMethodTable()->scriptExecutionStatus(globalObject, ticket->scriptExecutionOwner())) {
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
        // But we want to keep ticketData while running task since its globalObject ensures dependencies are strongly held.
        auto ticketData = m_pendingTickets.take(pendingTicket);

        {
            // Allow tasks we are about to run to schedule work.
            SetForScope<bool> runningTask(m_currentlyRunningTask, true);
            auto dropper = DropLockForScope(locker);

            // This is the start of a runloop turn, we can release any weakrefs here.
            vm.finalizeSynchronousJSExecution();

            auto scope = DECLARE_CATCH_SCOPE(vm);
            task(ticket);
            ticketData = nullptr;
            if (Exception* exception = scope.exception()) {
                if (scope.clearExceptionExceptTermination())
                    globalObject->globalObjectMethodTable()->reportUncaughtExceptionAtEventLoop(globalObject, exception);
                else if (vm.hasPendingTerminationException()) [[unlikely]]
                    return;
            }

            vm.drainMicrotasks();
            if (vm.hasPendingTerminationException()) [[unlikely]]
                return;

            scope.assertNoException();
        }
    }

    while (!suspendedTasks.isEmpty())
        m_tasks.prepend(suspendedTasks.takeLast());

    // It is theoretically possible that a client may cancel a pending ticket and
    // never call scheduleWorkSoon() on it. As such, it would not be found when
    // we iterated m_tasks above. We'll need to make sure to purge them here.
    m_pendingTickets.removeIf([] (auto& ticket) {
        return ticket->isCancelled();
    });
}

void DeferredWorkTimer::runRunLoop()
{
    ASSERT(!m_apiLock->vm()->currentThreadIsHoldingAPILock());
    ASSERT(&RunLoop::currentSingleton() == &m_apiLock->vm()->runLoop());
    m_shouldStopRunLoopWhenAllTicketsFinish = true;
    if (!m_pendingTickets.isEmpty())
        RunLoop::run();
}

DeferredWorkTimer::Ticket DeferredWorkTimer::addPendingWork(WorkType type, VM& vm, JSObject* target, Vector<JSCell*>&& dependencies)
{
    ASSERT_UNUSED(vm, vm.currentThreadIsHoldingAPILock() || (Thread::mayBeGCThread() && vm.heap.worldIsStopped()));
    for (unsigned i = 0; i < dependencies.size(); ++i)
        ASSERT(dependencies[i] != target && dependencies[i]);

    auto* globalObject = target->globalObject();
    JSObject* scriptExecutionOwner = globalObject->globalObjectMethodTable()->currentScriptExecutionOwner(globalObject);
    dependencies.append(target);

    auto ticketData = TicketData::create(type, scriptExecutionOwner, WTFMove(dependencies));
    Ticket ticket = ticketData.ptr();

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

bool DeferredWorkTimer::hasDependencyInPendingWork(Ticket ticket, JSCell* dependency)
{
    auto result = m_pendingTickets.find(ticket);
    if (result == m_pendingTickets.end() || ticket->isCancelled())
        return false;
    ASSERT(ticket->vm().currentThreadIsHoldingAPILock() || (Thread::mayBeGCThread() && ticket->vm().heap.worldIsStopped()));
    return (*result)->dependencies().contains(dependency);
}

void DeferredWorkTimer::scheduleWorkSoon(Ticket ticket, Task&& task)
{
    Locker locker { m_taskLock };
    m_tasks.append(std::make_tuple(ticket, WTFMove(task)));
    if (!isScheduled() && !m_currentlyRunningTask)
        setTimeUntilFire(0_s);
}

// Since TicketData is ThreadSafeWeakPtr now, we should optimize the DeferredWorkTimer's
// workflow, e.g. directly clear the TicketData from cancelPendingWork.
// https://bugs.webkit.org/show_bug.cgi?id=276538
bool DeferredWorkTimer::cancelPendingWork(Ticket ticket)
{
    ASSERT(m_pendingTickets.contains(ticket));
    ASSERT(ticket->isCancelled() || ticket->vm().currentThreadIsHoldingAPILock() || (Thread::mayBeGCThread() && ticket->vm().heap.worldIsStopped()));

    bool result = false;
    if (!ticket->isCancelled()) {
        ticket->cancel();
        result = true;
    }

    return result;
}

void DeferredWorkTimer::cancelPendingWorkSafe(JSGlobalObject* globalObject)
{
    Locker locker { m_taskLock };

    dataLogLnIf(DeferredWorkTimerInternal::verbose, "Cancel pending work for globalObject ", RawPointer(globalObject));
    for (Ref<TicketData> ticket : *globalObject->m_weakTickets) {
        if (!ticket->isCancelled())
            cancelPendingWork(ticket.ptr());
        m_tasks.append(std::make_tuple(ticket.ptr(), [](DeferredWorkTimer::Ticket) { }));
    }
    if (!isScheduled() && !m_currentlyRunningTask)
        setTimeUntilFire(0_s);
}

void DeferredWorkTimer::cancelPendingWork(VM& vm)
{
    ASSERT(vm.heap.isInPhase(CollectorPhase::End));
    Locker locker { m_taskLock };

    dataLogLnIf(DeferredWorkTimerInternal::verbose, "Cancel pending work for vm ", RawPointer(&vm));
    auto isValid = [&](auto& ticket) {
        bool isTargetGlobalObjectLive = vm.heap.isMarked(ticket->target()->globalObject());
#if ASSERT_ENABLED
        if (isTargetGlobalObjectLive) {
            for (JSCell* dependency : ticket->dependencies())
                ASSERT(vm.heap.isMarked(dependency));
        }
#endif
        return isTargetGlobalObjectLive && vm.heap.isMarked(ticket->scriptExecutionOwner());
    };

    bool needToFire = false;
    for (auto& ticket : m_pendingTickets) {
        if (ticket->isCancelled() || !isValid(ticket)) {
            // At this point, no one can visit or need the dependencies.
            // So, they are safe to clear here for better debugging and testing.
            ticket->cancelAndClear();
            needToFire = true;
        }
    }
    // GC can be triggered before an invalid and scheduled ticket is fired. In that case,
    // we also need to remove the corresponding pending task. Since doWork handles all cases
    // for removal, we should let it handle that for consistency.
    if (needToFire && !isScheduled() && !m_currentlyRunningTask)
        setTimeUntilFire(0_s);
}

void DeferredWorkTimer::didResumeScriptExecutionOwner()
{
    ASSERT(!m_currentlyRunningTask);
    Locker locker { m_taskLock };
    if (!isScheduled() && m_tasks.size())
        setTimeUntilFire(0_s);
}

bool DeferredWorkTimer::hasAnyPendingWork() const
{
    ASSERT(m_apiLock->vm()->currentThreadIsHoldingAPILock() || (Thread::mayBeGCThread() && m_apiLock->vm()->heap.worldIsStopped()));
    return !m_pendingTickets.isEmpty();
}

bool DeferredWorkTimer::hasImminentlyScheduledWork() const
{
    ASSERT(m_apiLock->vm()->currentThreadIsHoldingAPILock() || (Thread::mayBeGCThread() && m_apiLock->vm()->heap.worldIsStopped()));
    for (auto& ticket : m_pendingTickets) {
        if (ticket->isCancelled())
            continue;
        if (ticket->type() == WorkType::ImminentlyScheduled)
            return true;
    }
    return false;
}

} // namespace JSC
