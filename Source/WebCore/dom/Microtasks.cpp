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
#include "JSDOMExceptionHandling.h"
#include "JSExecState.h"
#include "RejectedPromiseTracker.h"
#include "ScriptExecutionContext.h"
#include "WorkerGlobalScope.h"
#include <JavaScriptCore/MicrotaskQueueInlines.h>
#include <JavaScriptCore/ScriptProfilingScope.h>
#include <JavaScriptCore/TopExceptionScope.h>
#include <JavaScriptCore/VMEntryScopeInlines.h>
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

MicrotaskQueue::MicrotaskQueue(JSC::VM& vm, EventLoop& eventLoop)
    : m_vm(vm)
    , m_eventLoop(eventLoop)
    , m_microtaskQueue(vm)
{
}

MicrotaskQueue::~MicrotaskQueue() = default;

void MicrotaskQueue::append(JSC::QueuedTask&& task)
{
    m_microtaskQueue.enqueue(WTF::move(task));
}

void MicrotaskQueue::runJSMicrotaskWithDebugger(JSC::JSGlobalObject* globalObject, JSC::VM& vm, JSC::QueuedTask& task)
{
    auto scope = DECLARE_TOP_EXCEPTION_SCOPE(vm);

    auto identifier = task.identifier();
    if (auto* debugger = globalObject->debugger(); debugger && identifier) [[unlikely]] {
        JSC::DeferTerminationForAWhile deferTerminationForAWhile(vm);
        debugger->willRunMicrotask(globalObject, identifier.value());
        if (!scope.clearExceptionExceptTermination()) [[unlikely]]
            return;
    }

    runJSMicrotask(globalObject, vm, task);
    if (!scope.clearExceptionExceptTermination()) [[unlikely]]
        return;

    if (auto* debugger = globalObject->debugger(); debugger && identifier) [[unlikely]] {
        JSC::DeferTerminationForAWhile deferTerminationForAWhile(vm);
        debugger->didRunMicrotask(globalObject, identifier.value());
        if (!scope.clearExceptionExceptTermination()) [[unlikely]]
            return;
    }
}

void MicrotaskQueue::runJSMicrotask(JSC::JSGlobalObject* globalObject, JSC::VM& vm, JSC::QueuedTask& task)
{
    auto scope = DECLARE_TOP_EXCEPTION_SCOPE(vm);
    JSC::runInternalMicrotask(globalObject, task.job(), task.payload(), task.arguments());
    if (scope.exception()) [[unlikely]] {
        auto* exception = scope.exception();
        if (!scope.clearExceptionExceptTermination()) [[unlikely]]
            return;
        reportException(globalObject, exception);
        if (!scope.clearExceptionExceptTermination()) [[unlikely]]
            return;
    }
}

void MicrotaskQueue::performMicrotaskCheckpoint()
{
    if (m_performingMicrotaskCheckpoint)
        return;

    SetForScope change(m_performingMicrotaskCheckpoint, true);
    Ref vm = this->vm();
    JSC::JSLockHolder locker(vm);
    auto catchScope = DECLARE_TOP_EXCEPTION_SCOPE(vm);
    {
        SUPPRESS_UNCOUNTED_ARG auto& data = threadGlobalDataSingleton();
        auto* previousState = data.currentState();
        std::optional<JSC::VMEntryScope> entryScope;
        JSC::JSGlobalObject* currentGlobalObject = nullptr;
        m_microtaskQueue.performMicrotaskCheckpoint</* useCallOnEachMicrotask */ false>(vm,
            [&](JSC::QueuedTask& task) ALWAYS_INLINE_LAMBDA {
                RefPtr dispatcher = downcast<WebCoreMicrotaskDispatcher>(task.dispatcher());
                if (!dispatcher) [[unlikely]]
                    return JSC::QueuedTask::Result::Discard;

                auto result = dispatcher->currentRunnability();
                if (result == JSC::QueuedTask::Result::Executed) {
                    switch (dispatcher->type()) {
                    case WebCoreMicrotaskDispatcher::Type::WebCoreJS: {
                        auto* globalObject = task.globalObject();
                        data.setCurrentState(globalObject);
                        if (currentGlobalObject != globalObject) {
                            if (!entryScope)
                                entryScope.emplace(vm, globalObject);
                            else
                                entryScope->setGlobalObject(globalObject);
                            currentGlobalObject = globalObject;
                        }
                        runJSMicrotask(globalObject, vm, task);
                        break;
                    }
                    case WebCoreMicrotaskDispatcher::Type::None:
                    case WebCoreMicrotaskDispatcher::Type::JSCDebuggable:
                    case WebCoreMicrotaskDispatcher::Type::WebCoreJSDebuggable:
                    case WebCoreMicrotaskDispatcher::Type::WebCoreUserGestureIndicator:
                    case WebCoreMicrotaskDispatcher::Type::WebCoreFunction:
                        entryScope = std::nullopt;
                        currentGlobalObject = nullptr;
                        data.setCurrentState(previousState);
                        dispatcher->run(task);
                        break;
                    }
                }
                return result;
            });
        data.setCurrentState(previousState);
    }
    vm->finalizeSynchronousJSExecution();

    if (!vm->executionForbidden()) {
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
    Ref { *m_eventLoop }->forEachAssociatedContext([vm = vm.copyRef()](auto& context) {
        if (vm->executionForbidden()) [[unlikely]]
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

bool MicrotaskQueue::hasMicrotasksForFullyActiveDocument() const
{
    return m_microtaskQueue.hasMicrotasksForFullyActiveDocument();
}

} // namespace WebCore
