/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

namespace JSC {

static const bool verbose = false;

PromiseDeferredTimer::PromiseDeferredTimer(VM& vm)
    : Base(&vm)
{
}

void PromiseDeferredTimer::doWork()
{
    ASSERT(m_vm->currentThreadIsHoldingAPILock());
    LockHolder locker(m_taskLock);
    cancelTimer();
    if (!m_runTasks)
        return;

    while (!m_tasks.isEmpty()) {
        JSPromiseDeferred* ticket;
        Task task;
        std::tie(ticket, task) = m_tasks.takeLast();
        dataLogLnIf(verbose, "Doing work on promise: ", RawPointer(ticket));

        // We may have already canceled these promises.
        if (m_pendingPromises.contains(ticket)) {
            task();
            m_vm->drainMicrotasks();
        }

        auto waitingTasks = m_blockedTasks.take(ticket);
        for (const Task& unblockedTask : waitingTasks) {
            unblockedTask();
            m_vm->drainMicrotasks();
        }
    }

#if USE(CF)
    if (m_pendingPromises.isEmpty() && m_shouldStopRunLoopWhenAllPromisesFinish)
        CFRunLoopStop(m_runLoop.get());
#endif
}

#if USE(CF)
void PromiseDeferredTimer::runRunLoop()
{
    ASSERT(!m_vm->currentThreadIsHoldingAPILock());
    ASSERT(CFRunLoopGetCurrent() == m_runLoop.get());
    m_shouldStopRunLoopWhenAllPromisesFinish = true;
    if (m_pendingPromises.size())
        CFRunLoopRun();
}
#endif

void PromiseDeferredTimer::addPendingPromise(JSPromiseDeferred* ticket, Vector<Strong<JSCell>>&& dependencies)
{
    ASSERT(m_vm->currentThreadIsHoldingAPILock());
    for (unsigned i = 0; i < dependencies.size(); ++i)
        ASSERT(dependencies[i].get() != ticket);

    auto result = m_pendingPromises.add(ticket, Vector<Strong<JSCell>>());
    if (result.isNewEntry) {
        dataLogLnIf(verbose, "Adding new pending promise: ", RawPointer(ticket));
        dependencies.append(Strong<JSCell>(*m_vm, ticket));
        result.iterator->value = WTFMove(dependencies);
    } else {
        dataLogLnIf(verbose, "Adding new dependencies for promise: ", RawPointer(ticket));
        result.iterator->value.appendVector(dependencies);
    }

#ifndef NDEBUG
    ticket->promiseAsyncPending();
#endif
}

bool PromiseDeferredTimer::cancelPendingPromise(JSPromiseDeferred* ticket)
{
    ASSERT(m_vm->currentThreadIsHoldingAPILock());
    bool result = m_pendingPromises.remove(ticket);

    if (result)
        dataLogLnIf(verbose, "Canceling promise: ", RawPointer(ticket));

    auto blockedTasks = m_blockedTasks.take(ticket);
    for (const Task& task : blockedTasks)
        task();
    return result;
}

void PromiseDeferredTimer::scheduleWorkSoon(JSPromiseDeferred* ticket, Task&& task)
{
    LockHolder locker(m_taskLock);
    m_tasks.append(std::make_tuple(ticket, WTFMove(task)));
    if (!isScheduled())
        scheduleTimer(0);
}

void PromiseDeferredTimer::scheduleBlockedTask(JSPromiseDeferred* blockingTicket, Task&& task)
{
    ASSERT(m_vm->currentThreadIsHoldingAPILock());
    ASSERT(m_pendingPromises.contains(blockingTicket));
    auto result = m_blockedTasks.add(blockingTicket, Vector<Task>());
    result.iterator->value.append(WTFMove(task));
}

} // namespace JSC
