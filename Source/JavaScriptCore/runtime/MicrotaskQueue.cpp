/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "MicrotaskQueue.h"

#include "Debugger.h"
#include "DeferTermination.h"
#include "GlobalObjectMethodTable.h"
#include "JSCJSValueInlines.h"
#include "JSGlobalObject.h"
#include "JSMicrotask.h"
#include "JSMicrotaskDispatcher.h"
#include "JSObject.h"
#include "MicrotaskCallInlines.h"
#include "MicrotaskQueueInlines.h"
#include "ScriptProfilingScope.h"
#include "SlotVisitorInlines.h"
#include <wtf/TZoneMallocInlines.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MicrotaskQueue);
WTF_MAKE_COMPACT_TZONE_ALLOCATED_IMPL(MicrotaskDispatcher);
WTF_MAKE_COMPACT_TZONE_ALLOCATED_IMPL(DebuggableMicrotaskDispatcher);

bool QueuedTask::isRunnable() const
{
    if (isJSMicrotaskDispatcher()) [[unlikely]]
        return uncheckedDowncast<JSMicrotaskDispatcher>(dispatcher())->dispatcher()->isRunnable();
    return uncheckedDowncast<JSGlobalObject>(dispatcher())->microtaskRunnability() == QueuedTaskResult::Executed;
}

static bool runMicrotask(JSGlobalObject* globalObject, TopExceptionScope& catchScope, VM& vm, QueuedTask& task, MicrotaskCall* microtaskCall)
{
    runInternalMicrotask(globalObject, vm, task.job(), task.payload(), task.arguments(), microtaskCall);
    if (auto* exception = catchScope.exception()) [[unlikely]] {
        if (!catchScope.clearExceptionExceptTermination()) [[unlikely]]
            return false;
        globalObject->globalObjectMethodTable()->reportUncaughtExceptionAtEventLoop(globalObject, exception);
        return catchScope.clearExceptionExceptTermination();
    }
    return true;
}

void runMicrotaskWithDebugger(JSGlobalObject* globalObject, VM& vm, QueuedTask& task)
{
    auto catchScope = DECLARE_TOP_EXCEPTION_SCOPE(vm);
    auto identifier = task.identifier();

    if (auto* debugger = globalObject->debugger(); debugger && identifier) [[unlikely]] {
        DeferTerminationForAWhile deferTerminationForAWhile(vm);
        debugger->willRunMicrotask(globalObject, identifier.value());
        if (!catchScope.clearExceptionExceptTermination()) [[unlikely]]
            return;
    }

    if (!runMicrotask(globalObject, catchScope, vm, task, nullptr)) [[unlikely]]
        return;

    if (auto* debugger = globalObject->debugger(); debugger && identifier) [[unlikely]] {
        DeferTerminationForAWhile deferTerminationForAWhile(vm);
        debugger->didRunMicrotask(globalObject, identifier.value());
        catchScope.clearExceptionExceptTermination();
    }
}

QueuedTaskResult DebuggableMicrotaskDispatcher::run(QueuedTask& task)
{
    auto* globalObject = task.globalObject();
    runMicrotaskWithDebugger(globalObject, globalObject->vm(), task);
    return QueuedTask::Result::Executed;
}

bool DebuggableMicrotaskDispatcher::isRunnable() const
{
    return true;
}

MicrotaskQueue::MicrotaskQueue(VM& vm)
{
    vm.m_microtaskQueues.append(this);
}

Ref<MicrotaskQueue> MicrotaskQueue::create(VM& vm)
{
    return adoptRef(*new MicrotaskQueue(vm));
}

MicrotaskQueue::~MicrotaskQueue()
{
    if (isOnList())
        remove();
}

template<typename Visitor>
void MicrotaskQueue::visitAggregateImpl(Visitor& visitor)
{
    m_queue.visitAggregate(visitor);
    m_toKeep.visitAggregate(visitor);
}
DEFINE_VISIT_AGGREGATE(MicrotaskQueue);

void MicrotaskQueue::enqueueSlow(QueuedTask&& task)
{
    auto* globalObject = task.globalObject();
    auto identifier = task.identifier();
    m_queue.enqueue(WTF::move(task));
    if (globalObject) {
        if (auto* debugger = globalObject->debugger(); debugger && identifier) [[unlikely]]
            debugger->didQueueMicrotask(globalObject, identifier.value());
    }
    if (!m_isScheduledToRun) [[unlikely]]
        scheduleToRunIfNeeded();
}

bool MarkedMicrotaskDeque::hasMicrotasksForFullyActiveDocument() const
{
    for (auto& task : m_queue) {
        if (task.isRunnable())
            return true;
    }
    return false;
}

template<typename Visitor>
void MarkedMicrotaskDeque::visitAggregateImpl(Visitor& visitor)
{
    // Because content in the queue will not be changed, we need to scan it only once per an entry during one GC cycle.
    // We record the previous scan's index, and restart scanning again in CollectorPhase::FixPoint from that.
    // When new GC phase begins, this cursor is reset to zero (beginMarking). This optimization is introduced because
    // some of application have massive size of MicrotaskQueue depth. For example, in parallel-promises-es2015-native.js
    // benchmark, it becomes 251670 at most.
    // This cursor is adjusted when an entry is dequeued. And we do not use any locking here, and that's fine: these
    // values are read by GC when CollectorPhase::FixPoint and CollectorPhase::Begin, and both suspend the mutator, thus,
    // there is no concurrency issue.
    for (auto iterator = m_queue.begin() + m_markedBefore, end = m_queue.end(); iterator != end; ++iterator) {
        auto& task = *iterator;
        visitor.appendUnbarriered(task.dispatcher());
        visitor.appendUnbarriered(task.m_arguments, QueuedTask::maxArguments);
    }
    m_markedBefore = m_queue.size();
}
DEFINE_VISIT_AGGREGATE(MarkedMicrotaskDeque);

template<bool useCallOnEachMicrotask>
ALWAYS_INLINE std::pair<JSGlobalObject*, bool> MicrotaskQueue::drainImpl(JSGlobalObject* currentGlobalObject, VM& vm, TopExceptionScope& catchScope)
{
    MicrotaskCall microtaskCall(vm);

    while (!m_queue.isEmpty()) {
        auto& front = m_queue.front();

        if (!front.isJSMicrotaskDispatcher()) [[likely]] {
            auto* globalObject = uncheckedDowncast<JSGlobalObject>(front.dispatcher());
            auto result = globalObject->microtaskRunnability();
            if (result != QueuedTask::Result::Executed) [[unlikely]] {
                auto task = m_queue.dequeue();
                if (result == QueuedTask::Result::Suspended)
                    m_toKeep.enqueue(WTF::move(task));
                continue;
            }

            if (globalObject != currentGlobalObject) [[unlikely]]
                return { globalObject, false };

            auto task = m_queue.dequeue();
            if (!runMicrotask(globalObject, catchScope, vm, task, &microtaskCall)) [[unlikely]] {
                clear();
                return { nullptr, true };
            }
        } else {
            auto* jsMicrotaskDispatcher = uncheckedDowncast<JSMicrotaskDispatcher>(front.dispatcher());
            auto* globalObject = front.globalObject();

            if (globalObject != currentGlobalObject) [[unlikely]]
                return { globalObject, false };

            auto task = m_queue.dequeue();
            QueuedTask::Result result;
            {
                ScriptProfilingScope profilingScope(globalObject, ProfilingReason::Microtask);
                result = jsMicrotaskDispatcher->dispatcher()->run(task);
            }

            switch (result) {
            case QueuedTask::Result::Executed:
                break;
            case QueuedTask::Result::Discard:
                break;
            case QueuedTask::Result::Suspended:
                m_toKeep.enqueue(WTF::move(task));
                break;
            }

            if (!catchScope.clearExceptionExceptTermination()) [[unlikely]] {
                clear();
                return { nullptr, true };
            }
        }

        if constexpr (useCallOnEachMicrotask) {
            vm.callOnEachMicrotaskTick();
            if (!catchScope.clearExceptionExceptTermination()) [[unlikely]] {
                clear();
                return { nullptr, true };
            }
        }
    }

    return { nullptr, true };
}

std::pair<JSGlobalObject*, bool> MicrotaskQueue::drainWithoutUseCallOnEachMicrotask(JSGlobalObject* currentGlobalObject, VM& vm, TopExceptionScope& catchScope)
{
    return drainImpl</* useCallOnEachMicrotask */ false>(currentGlobalObject, vm, catchScope);
}

std::pair<JSGlobalObject*, bool> MicrotaskQueue::drainWithUseCallOnEachMicrotask(JSGlobalObject* currentGlobalObject, VM& vm, TopExceptionScope& catchScope)
{
    return drainImpl</* useCallOnEachMicrotask */ true>(currentGlobalObject, vm, catchScope);
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
