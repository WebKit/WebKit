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
#include "JSExecState.h"
#include "RejectedPromiseTracker.h"
#include "ScriptExecutionContext.h"
#include "WorkerGlobalScope.h"
#include <JavaScriptCore/JSGlobalObject.h>
#include <JavaScriptCore/MicrotaskQueueInlines.h>
#include <JavaScriptCore/TopExceptionScope.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/SetForScope.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MicrotaskQueue);
WTF_MAKE_COMPACT_TZONE_ALLOCATED_IMPL(WebCoreMicrotaskDispatcher);


JSC::QueuedTask::Result WebCoreMicrotaskDispatcher::currentRunnability() const
{
    auto group = m_group.get();
    if (!group || group->isStoppedPermanently())
        return JSC::QueuedTask::Result::Discard;
    if (group->isSuspended())
        return JSC::QueuedTask::Result::Suspended;
    return JSC::QueuedTask::Result::Executed;
}

Ref<MicrotaskQueue> MicrotaskQueue::create(JSC::VM& vm, EventLoop& eventLoop)
{
    return adoptRef(*new MicrotaskQueue(vm, eventLoop));
}

MicrotaskQueue::MicrotaskQueue(JSC::VM& vm, EventLoop& eventLoop)
    : JSC::MicrotaskQueue(vm)
    , m_eventLoop(eventLoop)
{
}

MicrotaskQueue::~MicrotaskQueue() = default;

void MicrotaskQueue::performMicrotaskCheckpoint(JSC::VM& vm)
{
    if (m_performingMicrotaskCheckpoint)
        return;

    SetForScope change(m_performingMicrotaskCheckpoint, true);
    JSC::JSLockHolder locker(vm);
    auto catchScope = DECLARE_TOP_EXCEPTION_SCOPE(vm);
    {
        SUPPRESS_UNCOUNTED_ARG auto& data = threadGlobalDataSingleton();
        auto* previousState = data.currentState();
        JSC::MicrotaskQueue::performMicrotaskCheckpoint</* useCallOnEachMicrotask */ false>(vm,
            [&](JSC::JSGlobalObject*, JSC::JSGlobalObject* nextGlobalObject) ALWAYS_INLINE_LAMBDA {
                data.setCurrentState(nextGlobalObject ? nextGlobalObject : previousState);
            });
        data.setCurrentState(previousState);
    }
    vm.finalizeSynchronousJSExecution();

    if (!vm.executionForbidden()) {
        auto checkpointTasks = std::exchange(m_checkpointTasks, { });
        for (auto& checkpointTask : checkpointTasks) {
            CheckedPtr group = checkpointTask->group();
            if (!group || group->isStoppedPermanently())
                continue;

            if (group->isSuspended()) {
                m_checkpointTasks.append(WTF::move(checkpointTask));
                continue;
            }

            checkpointTask->execute();
            if (!catchScope.clearExceptionExceptTermination()) [[unlikely]]
                break; // Encountered termination.
        }
    }

    // https://html.spec.whatwg.org/multipage/webappapis.html#perform-a-microtask-checkpoint (step 4).
    Ref { *m_eventLoop }->forEachAssociatedContext([&vm](auto& context) {
        if (vm.executionForbidden()) [[unlikely]]
            return;
        auto catchScope = DECLARE_TOP_EXCEPTION_SCOPE(vm);
        if (CheckedPtr tracker = context.rejectedPromiseTracker())
            tracker->processQueueSoon();
        catchScope.clearExceptionExceptTermination();
    });

    // FIXME: We should cleanup Indexed Database transactions as per:
    // https://html.spec.whatwg.org/multipage/webappapis.html#perform-a-microtask-checkpoint (step 5).
}

void MicrotaskQueue::addCheckpointTask(std::unique_ptr<EventLoopTask>&& task)
{
    m_checkpointTasks.append(WTF::move(task));
}

void MicrotaskQueue::scheduleToRunIfNeeded()
{
    if (RefPtr eventLoop = m_eventLoop.get())
        eventLoop->scheduleToRunIfNeeded();
}

} // namespace WebCore
