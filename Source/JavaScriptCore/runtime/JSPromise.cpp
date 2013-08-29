/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "JSPromise.h"

#if ENABLE(PROMISES)

#include "Error.h"
#include "JSCJSValueInlines.h"
#include "JSCellInlines.h"
#include "JSPromiseResolver.h"
#include "SlotVisitorInlines.h"
#include "StrongInlines.h"
#include "StructureInlines.h"

namespace JSC {

class JSPromiseTaskContext : public TaskContext {
public:
    static PassRefPtr<JSPromiseTaskContext> create(VM& vm, JSPromise* promise)
    {
        return adoptRef(new JSPromiseTaskContext(vm, promise));
    }

    JSPromise& promise() const { return *m_promise.get(); }

private:
    JSPromiseTaskContext(VM& vm, JSPromise* promise)
    {
        m_promise.set(vm, promise);
    }

    Strong<JSPromise> m_promise;
};

const ClassInfo JSPromise::s_info = { "Promise", &Base::s_info, 0, 0, CREATE_METHOD_TABLE(JSPromise) };

JSPromise* JSPromise::create(VM& vm, Structure* structure)
{
    JSPromise* promise = new (NotNull, allocateCell<JSPromise>(vm.heap)) JSPromise(vm, structure);
    promise->finishCreation(vm);
    return promise;
}

JSPromise* JSPromise::createWithResolver(VM& vm, JSGlobalObject* globalObject)
{
    JSPromise* promise = new (NotNull, allocateCell<JSPromise>(vm.heap)) JSPromise(vm, globalObject->promiseStructure());
    promise->finishCreation(vm);

    JSPromiseResolver* resolver = JSPromiseResolver::create(vm, globalObject->promiseResolverStructure(), promise);
    promise->setResolver(vm, resolver);

    return promise;
}

Structure* JSPromise::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

JSPromise::JSPromise(VM& vm, Structure* structure)
    : JSDestructibleObject(vm, structure)
    , m_state(Pending)
{
}

void JSPromise::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
}

void JSPromise::destroy(JSCell* cell)
{
    static_cast<JSPromise*>(cell)->JSPromise::~JSPromise();
}

void JSPromise::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    JSPromise* thisObject = jsCast<JSPromise*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    COMPILE_ASSERT(StructureFlags & OverridesVisitChildren, OverridesVisitChildrenWithoutSettingFlag);
    ASSERT(thisObject->structure()->typeInfo().overridesVisitChildren());

    Base::visitChildren(thisObject, visitor);

    visitor.append(&thisObject->m_resolver);
    visitor.append(&thisObject->m_result);

    for (size_t i = 0; i < thisObject->m_fulfillCallbacks.size(); ++i)
        visitor.append(&thisObject->m_fulfillCallbacks[i]);
    for (size_t i = 0; i < thisObject->m_rejectCallbacks.size(); ++i)
        visitor.append(&thisObject->m_rejectCallbacks[i]);
}

void JSPromise::setResolver(VM& vm, JSPromiseResolver* resolver)
{
    m_resolver.set(vm, this, resolver);
}

JSPromiseResolver* JSPromise::resolver() const
{
    return m_resolver.get();
}

void JSPromise::setResult(VM& vm, JSValue result)
{
    m_result.set(vm, this, result);
}

JSValue JSPromise::result() const
{
    return m_result.get();
}

void JSPromise::setState(JSPromise::State state)
{
    ASSERT(m_state == Pending);
    m_state = state;
}

JSPromise::State JSPromise::state() const
{
    return m_state;
}

void JSPromise::appendCallbacks(ExecState* exec, InternalFunction* fulfillCallback, InternalFunction* rejectCallback)
{
    // 1. Append fulfillCallback to promise's fulfill callbacks.
    m_fulfillCallbacks.append(WriteBarrier<InternalFunction>(exec->vm(), this, fulfillCallback));
    
    // 2. Append rejectCallback to promise' reject callbacks.
    m_rejectCallbacks.append(WriteBarrier<InternalFunction>(exec->vm(), this, rejectCallback));

    // 3. If promise's state is fulfilled, queue a task to process promise's fulfill callbacks with promise's result.
    if (m_state == Fulfilled) {
        queueTaskToProcessFulfillCallbacks(exec);
        return;
    }

    // 4. If promise's state is rejected, queue a task to process promise's reject callbacks with promise's result.
    if (m_state == Rejected) {
        queueTaskToProcessRejectCallbacks(exec);
        return;
    }
}

void JSPromise::queueTaskToProcessFulfillCallbacks(ExecState* exec)
{
    JSGlobalObject* globalObject = this->globalObject();
    if (globalObject->globalObjectMethodTable()->queueTaskToEventLoop)
        globalObject->globalObjectMethodTable()->queueTaskToEventLoop(globalObject, processFulfillCallbacksForTask, JSPromiseTaskContext::create(exec->vm(), this));
    else
        WTFLogAlways("ERROR: Event loop not supported.");
}

void JSPromise::queueTaskToProcessRejectCallbacks(ExecState* exec)
{
    JSGlobalObject* globalObject = this->globalObject();
    if (globalObject->globalObjectMethodTable()->queueTaskToEventLoop)
        globalObject->globalObjectMethodTable()->queueTaskToEventLoop(globalObject, processRejectCallbacksForTask, JSPromiseTaskContext::create(exec->vm(), this));
    else
        WTFLogAlways("ERROR: Event loop not supported.");
}

void JSPromise::processFulfillCallbacksForTask(ExecState* exec, TaskContext* taskContext)
{
    JSPromiseTaskContext* promiseTaskContext = static_cast<JSPromiseTaskContext*>(taskContext);
    JSPromise& promise = promiseTaskContext->promise();

    promise.processFulfillCallbacksWithValue(exec, promise.result());
}

void JSPromise::processRejectCallbacksForTask(ExecState* exec, TaskContext* taskContext)
{
    JSPromiseTaskContext* promiseTaskContext = static_cast<JSPromiseTaskContext*>(taskContext);
    JSPromise& promise = promiseTaskContext->promise();

    promise.processRejectCallbacksWithValue(exec, promise.result());
}

void JSPromise::processFulfillCallbacksWithValue(ExecState* exec, JSValue value)
{
    ASSERT(m_state == Fulfilled);

    for (size_t i = 0; i < m_fulfillCallbacks.size(); ++i) {
        JSValue callback = m_fulfillCallbacks[i].get();

        CallData callData;
        CallType callType = JSC::getCallData(callback, callData);
        ASSERT(callType != CallTypeNone);

        MarkedArgumentBuffer arguments;
        arguments.append(value);
        call(exec, callback, callType, callData, this, arguments);
    }
    
    m_fulfillCallbacks.clear();
}

void JSPromise::processRejectCallbacksWithValue(ExecState* exec, JSValue value)
{
    ASSERT(m_state == Rejected);

    for (size_t i = 0; i < m_rejectCallbacks.size(); ++i) {
        JSValue callback = m_rejectCallbacks[i].get();

        CallData callData;
        CallType callType = JSC::getCallData(callback, callData);
        ASSERT(callType != CallTypeNone);

        MarkedArgumentBuffer arguments;
        arguments.append(value);
        call(exec, callback, callType, callData, this, arguments);
    }
    
    m_rejectCallbacks.clear();
}

} // namespace JSC

#endif // ENABLE(PROMISES)
