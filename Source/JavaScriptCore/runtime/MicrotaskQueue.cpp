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
#include "JSCJSValueInlines.h"
#include "JSGlobalObject.h"
#include "JSMicrotask.h"
#include "JSObject.h"
#include "JSPromiseReaction.h"
#include "SlotVisitorInlines.h"
#include <wtf/SetForScope.h>
#include <wtf/TZoneMallocInlines.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MicrotaskQueue);
WTF_MAKE_COMPACT_TZONE_ALLOCATED_IMPL(MicrotaskDispatcher);
WTF_MAKE_COMPACT_TZONE_ALLOCATED_IMPL(DebuggableMicrotaskDispatcher);

bool QueuedTask::isRunnable() const
{
    if (RefPtr dispatcher = m_dispatcher.pointer())
        return dispatcher->isRunnable();
    return true;
}

QueuedTaskResult DebuggableMicrotaskDispatcher::run(QueuedTask& task)
{
    auto* globalObject = task.globalObject();
    VM& vm = globalObject->vm();
    auto catchScope = DECLARE_CATCH_SCOPE(vm);
    auto identifier = task.identifier();

    if (auto* debugger = globalObject->debugger(); debugger && identifier) [[unlikely]] {
        DeferTerminationForAWhile deferTerminationForAWhile(vm);
        debugger->willRunMicrotask(globalObject, identifier.value());
        if (!catchScope.clearExceptionExceptTermination()) [[unlikely]]
            return QueuedTask::Result::Executed;
    }

    runInternalMicrotaskImpl(globalObject, task.job(), task.payload(), task.arguments());
    if (!catchScope.clearExceptionExceptTermination()) [[unlikely]]
        return QueuedTask::Result::Executed;

    if (auto* debugger = globalObject->debugger(); debugger && identifier) [[unlikely]] {
        DeferTerminationForAWhile deferTerminationForAWhile(vm);
        debugger->didRunMicrotask(globalObject, identifier.value());
        if (!catchScope.clearExceptionExceptTermination()) [[unlikely]]
            return QueuedTask::Result::Executed;
    }

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

bool MarkedMicrotaskDeque::tryMergePromiseReactionChain(VM& vm, QueuedTask& newTask)
{
    if (m_queue.isEmpty())
        return false;

    QueuedTask& lastTask = m_queue.last();

    // Must match: task type, globalObject, dispatcher, status (payload).
    if (lastTask.job() != InternalMicrotask::PromiseReactionChain)
        return false;
    if (lastTask.globalObject() != newTask.globalObject())
        return false;
    if (lastTask.dispatcher() != newTask.dispatcher())
        return false;
    if (lastTask.payload() != newTask.payload())
        return false;

    // Get tail of existing chain and head/tail of new chain.
    auto* existingTail = jsCast<JSPromiseReaction*>(lastTask.m_arguments[1].asCell());
    auto* newHead = jsCast<JSPromiseReaction*>(newTask.m_arguments[0].asCell());
    auto* newTail = jsCast<JSPromiseReaction*>(newTask.m_arguments[1].asCell());

    // Link chains: existing tail -> new head (write barrier handled by setNext).
    existingTail->setNext(vm, newHead);

    // Update tail pointer in existing task.
    lastTask.m_arguments[1] = JSValue(newTail);

    // Handle GC marking: if merged into already-marked entry, reset cursor.
    size_t lastIndex = m_queue.size() - 1;
    if (lastIndex < m_markedBefore)
        m_markedBefore = lastIndex;

    return true;
}

void MicrotaskQueue::enqueue(QueuedTask&& task)
{
    auto* globalObject = task.globalObject();
    auto identifier = task.identifier();
    if (globalObject) {
        if (auto* debugger = globalObject->debugger(); debugger && identifier) [[unlikely]] {
            m_queue.enqueue(WTF::move(task));
            debugger->didQueueMicrotask(globalObject, identifier.value());
            return;
        }

        if (task.job() == InternalMicrotask::PromiseReactionChain) {
            VM& vm = globalObject->vm();
            if (m_queue.tryMergePromiseReactionChain(vm, task))
                return;
        }
    }

    m_queue.enqueue(WTF::move(task));
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
        visitor.appendUnbarriered(task.m_globalObject);
        visitor.appendUnbarriered(task.m_arguments, QueuedTask::maxArguments);
    }
    m_markedBefore = m_queue.size();
}
DEFINE_VISIT_AGGREGATE(MarkedMicrotaskDeque);

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
