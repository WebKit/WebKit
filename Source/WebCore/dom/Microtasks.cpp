/*
 * Copyright (C) 2014 Yoav Weiss (yoav@yoav.ws)
 * Copyright (C) 2015 Akamai Technologies Inc. All rights reserved.
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "Microtasks.h"

#include "CommonVM.h"
#include "EventLoop.h"
#include "RejectedPromiseTracker.h"
#include "ScriptExecutionContext.h"
#include "WorkerGlobalScope.h"
#include <JavaScriptCore/CatchScope.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/SetForScope.h>

namespace WebCore {

MicrotaskQueue::MicrotaskQueue(JSC::VM& vm, EventLoop& eventLoop)
    : m_vm(vm)
    , m_eventLoop(eventLoop)
{
}

MicrotaskQueue::~MicrotaskQueue() = default;

void MicrotaskQueue::append(std::unique_ptr<EventLoopTask>&& task)
{
    m_microtaskQueue.append(WTFMove(task));
}

void MicrotaskQueue::performMicrotaskCheckpoint()
{
    if (m_performingMicrotaskCheckpoint)
        return;

    SetForScope change(m_performingMicrotaskCheckpoint, true);
    Ref vm = this->vm();
    JSC::JSLockHolder locker(vm);
    auto catchScope = DECLARE_CATCH_SCOPE(vm);

    Vector<std::unique_ptr<EventLoopTask>> toKeep;
    while (!m_microtaskQueue.isEmpty() && !vm->executionForbidden()) {
        Vector<std::unique_ptr<EventLoopTask>> queue = WTFMove(m_microtaskQueue);
        for (auto& task : queue) {
            auto* group = task->group();
            if (!group || group->isStoppedPermanently())
                continue;
            if (group->isSuspended())
                toKeep.append(WTFMove(task));
            else {
                task->execute();
                if (UNLIKELY(!catchScope.clearExceptionExceptTermination()))
                    break; // Encountered termination.
            }
        }
    }

    vm->finalizeSynchronousJSExecution();
    m_microtaskQueue = WTFMove(toKeep);

    if (!vm->executionForbidden()) {
        auto checkpointTasks = std::exchange(m_checkpointTasks, { });
        for (auto& checkpointTask : checkpointTasks) {
            auto* group = checkpointTask->group();
            if (!group || group->isStoppedPermanently())
                continue;

            if (group->isSuspended()) {
                m_checkpointTasks.append(WTFMove(checkpointTask));
                continue;
            }

            checkpointTask->execute();
            if (UNLIKELY(!catchScope.clearExceptionExceptTermination()))
                break; // Encountered termination.
        }
    }

    // https://html.spec.whatwg.org/multipage/webappapis.html#perform-a-microtask-checkpoint (step 4).
    Ref { *m_eventLoop }->forEachAssociatedContext([vm = vm.copyRef()](auto& context) {
        if (UNLIKELY(vm->executionForbidden()))
            return;
        auto catchScope = DECLARE_CATCH_SCOPE(vm);
        if (CheckedPtr tracker = context.rejectedPromiseTracker())
            tracker->processQueueSoon();
        catchScope.clearExceptionExceptTermination();
    });

    // FIXME: We should cleanup Indexed Database transactions as per:
    // https://html.spec.whatwg.org/multipage/webappapis.html#perform-a-microtask-checkpoint (step 5).
}

void MicrotaskQueue::addCheckpointTask(std::unique_ptr<EventLoopTask>&& task)
{
    m_checkpointTasks.append(WTFMove(task));
}

bool MicrotaskQueue::hasMicrotasksForFullyActiveDocument() const
{
    return m_microtaskQueue.containsIf([](auto& task) {
        auto group = task->group();
        return group && !group->isStoppedPermanently() && !group->isSuspended();
    });
}

} // namespace WebCore
