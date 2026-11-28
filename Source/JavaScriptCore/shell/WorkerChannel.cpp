/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "WorkerChannel.h"

#include "GlobalObjectMethodTable.h"
#include "JSCInlines.h"
#include "ObjectConstructor.h"
#include "StructuredClone.h"
#include "TopExceptionScope.h"
#include <mutex>
#include <wtf/ThreadSpecific.h>

namespace JSCShell {

using namespace JSC;

static ThreadSpecific<WorkerChannel*>& currentWorkerSideStorage()
{
    static ThreadSpecific<WorkerChannel*>* result;
    static std::once_flag flag;
    std::call_once(
        flag,
        [] {
            result = new ThreadSpecific<WorkerChannel*>();
        });
    return *result;
}

WorkerChannel* WorkerChannel::currentWorkerSide()
{
    return *currentWorkerSideStorage();
}

void WorkerChannel::open(Side side, VM& vm, JSObject* target)
{
    JSLockHolder lock(vm);
    Locker locker { m_lock };
    auto keepAlive = vm.deferredWorkTimer->addPendingWork(DeferredWorkTimer::WorkType::AtSomePoint, vm, target, { });
    if (side == Side::Worker)
        *currentWorkerSideStorage() = this;

    auto& port = this->port(side);
    port.runLoop = &RunLoop::currentSingleton();
    port.vm = &vm;
    port.keepAlive = WTF::move(keepAlive);
    port.state = PortState::Open;
    if (m_isTerminated) {
        terminateWorkerIfStarted();
        return;
    }
    for (size_t i = port.messages.size(); i--;)
        scheduleDelivery(side);
}

void WorkerChannel::close(Side side)
{
    DeferredWorkTimer::WeakTicket keepAlive;
    VM* vm;
    {
        Locker locker { m_lock };
        auto& port = this->port(side);
        if (port.state != PortState::Open)
            return;
        port.state = PortState::Closed;
        port.messages.clear();
        keepAlive = std::exchange(port.keepAlive, { });
        vm = port.vm;
    }
    JSLockHolder lock(*vm);
    vm->deferredWorkTimer->scheduleWorkSoonIfActive(keepAlive, [](DeferredWorkTimer::Ticket&) { });
}

void WorkerChannel::closeWorkerIfItCannotReceive(JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_TOP_EXCEPTION_SCOPE(vm);

    JSValue handler = globalObject->get(globalObject, Identifier::fromString(vm, "onmessage"_s));
    if (scope.exception()) [[unlikely]] {
        scope.clearExceptionExceptTermination();
        return;
    }
    if (handler.isCallable())
        return;
    close(Side::Worker);
}

void WorkerChannel::scheduleDelivery(Side to)
{
    protect(port(to).runLoop)->dispatch([channel = Ref { *this }, to] {
        channel->deliverNext(to);
    });
}

void WorkerChannel::post(Side from, Ref<Message>&& message)
{
    Side to = from == Side::Parent ? Side::Worker : Side::Parent;
    Locker locker { m_lock };
    if (m_isTerminated)
        return;
    auto& port = this->port(to);
    if (port.state == PortState::Closed)
        return;
    port.messages.append(WTF::move(message));
    if (port.state == PortState::Open)
        scheduleDelivery(to);
}

void WorkerChannel::deliverNext(Side to)
{
    DeferredWorkTimer::WeakTicket keepAlive;
    VM* vm;
    RefPtr<Message> message;
    {
        Locker locker { m_lock };
        auto& port = this->port(to);
        if (port.state != PortState::Open || m_isTerminated || port.messages.isEmpty())
            return;
        message = port.messages.takeFirst();
        keepAlive = port.keepAlive;
        vm = port.vm;
    }

    JSLockHolder lock(*vm);
    RefPtr ticket = keepAlive;
    if (!ticket || ticket->isCancelled())
        return;
    JSObject* target = ticket->target();
    JSGlobalObject* globalObject = target->realm();

    vm->finalizeSynchronousJSExecution();
    auto scope = DECLARE_TOP_EXCEPTION_SCOPE(*vm);
    [&] {
        JSValue handler = target->get(globalObject, Identifier::fromString(*vm, "onmessage"_s));
        RETURN_IF_EXCEPTION(scope, void());
        auto callData = JSC::getCallData(handler);
        if (callData.type == CallData::Type::None)
            return;

        auto result = JSCCloneDeserializer::deserialize(globalObject, message->data(), message->sideChannels());
        RETURN_IF_EXCEPTION(scope, void());
        if (result.code != SerializationReturnCode::SuccessfullyCompleted) {
            auto throwScope = DECLARE_THROW_SCOPE(*vm);
            throwSerializationError(globalObject, throwScope, result.code);
            return;
        }

        JSObject* event = constructEmptyObject(globalObject);
        event->putDirect(*vm, Identifier::fromString(*vm, "data"_s), result.value);
        MarkedArgumentBuffer arguments;
        arguments.append(event);
        ASSERT(!arguments.hasOverflowed());
        JSValue thisValue = target == globalObject ? globalObject->globalThis() : target;
        call(globalObject, handler, callData, thisValue, arguments);
    }();

    if (Exception* exception = scope.exception()) {
        if (scope.clearExceptionExceptTermination())
            globalObject->globalObjectMethodTable()->reportUncaughtExceptionAtEventLoop(globalObject, exception);
    }
    if (!vm->hasPendingTerminationException())
        vm->drainMicrotasks();
    if (vm->hasPendingTerminationException()) {
        vm->setExecutionForbidden();
        RunLoop::currentSingleton().stop();
    }
}

void WorkerChannel::terminate()
{
    Locker locker { m_lock };
    if (m_isTerminated)
        return;
    m_isTerminated = true;
    m_worker.messages.clear();
    m_parent.messages.clear();
    terminateWorkerIfStarted();
}

// Terminating a worker needs its VM, so whichever of terminate() and open() runs second does the
// work. A termination request raised before the worker enters its VM takes effect on entry, so the
// worker never gets to run its script.
void WorkerChannel::terminateWorkerIfStarted()
{
    if (!m_worker.vm)
        return;
    m_worker.vm->notifyNeedTermination();
    protect(m_worker.runLoop)->dispatch([channel = Ref { *this }] {
        channel->stopWorker();
    });
}

void WorkerChannel::stopWorker()
{
    close(Side::Worker);
    VM* vm;
    {
        Locker locker { m_lock };
        vm = m_worker.vm;
    }
    if (!vm)
        return;
    JSLockHolder lock(*vm);
    vm->setExecutionForbidden();
    RunLoop::currentSingleton().stop();
}

void WorkerChannel::workerDidExit()
{
    *currentWorkerSideStorage() = nullptr;
    Locker locker { m_lock };
    m_worker = { };
    if (m_parent.state == PortState::Open) {
        protect(m_parent.runLoop)->dispatch([channel = Ref { *this }] {
            channel->close(Side::Parent);
        });
    }
}

} // namespace JSCShell
