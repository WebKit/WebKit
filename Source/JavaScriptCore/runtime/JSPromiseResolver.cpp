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
#include "JSPromiseResolver.h"

#include "JSCJSValueInlines.h"
#include "JSCellInlines.h"
#include "JSGlobalObject.h"
#include "JSPromise.h"
#include "JSPromiseCallback.h"
#include "SlotVisitorInlines.h"
#include "StructureInlines.h"

namespace JSC {

const ClassInfo JSPromiseResolver::s_info = { "PromiseResolver", &Base::s_info, 0, 0, CREATE_METHOD_TABLE(JSPromiseResolver) };

JSPromiseResolver* JSPromiseResolver::create(VM& vm, Structure* structure, JSPromise* promise)
{
    JSPromiseResolver* object = new (NotNull, allocateCell<JSPromiseResolver>(vm.heap)) JSPromiseResolver(vm, structure);
    object->finishCreation(vm, promise);
    return object;
}

Structure* JSPromiseResolver::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

JSPromiseResolver::JSPromiseResolver(VM& vm, Structure* structure)
    : JSNonFinalObject(vm, structure)
    , m_isResolved(false)
{
}

void JSPromiseResolver::finishCreation(VM& vm, JSPromise* promise)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    m_promise.set(vm, this, promise);
}

void JSPromiseResolver::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    JSPromiseResolver* thisObject = jsCast<JSPromiseResolver*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    COMPILE_ASSERT(StructureFlags & OverridesVisitChildren, OverridesVisitChildrenWithoutSettingFlag);
    ASSERT(thisObject->structure()->typeInfo().overridesVisitChildren());

    Base::visitChildren(thisObject, visitor);
    visitor.append(&thisObject->m_promise);
}

JSPromise* JSPromiseResolver::promise() const
{
    return m_promise.get();
}

void JSPromiseResolver::fulfillIfNotResolved(ExecState* exec, JSValue value)
{
    if (!m_isResolved) {
        m_isResolved = true;
        fulfill(exec, value);
    }
}

void JSPromiseResolver::resolveIfNotResolved(ExecState* exec, JSValue value)
{
    if (!m_isResolved) {
        m_isResolved = true;
        resolve(exec, value);
    }
}

void JSPromiseResolver::rejectIfNotResolved(ExecState* exec, JSValue value)
{
    if (!m_isResolved) {
        m_isResolved = true;
        reject(exec, value);
    }
}

void JSPromiseResolver::fulfill(ExecState* exec, JSValue value, ResolverMode mode)
{
    // 1. Let promise be the context object's associated promise.
    // 2. Set promise's state to fulfilled.
    m_promise->setState(JSPromise::Fulfilled);

    // 3. Set promise's result to value.
    m_promise->setResult(exec->vm(), value);

    // 4. If the synchronous flag is set, process promise's fulfill callbacks with value.
    if (mode == ResolveSynchronously) {
        m_promise->processFulfillCallbacksWithValue(exec, value);
        return;
    }
    
    // 5. Otherwise, the synchronous flag is unset, queue a task to process promise's fulfill callbacks with value.
    m_promise->queueTaskToProcessFulfillCallbacks(exec);
}

void JSPromiseResolver::resolve(ExecState* exec, JSValue value, ResolverMode mode)
{
    // 1. Let then be null.
    JSValue then = jsNull();
    
    // 2. If value is a JavaScript Object, set then to the result of calling the JavaScript [[Get]] internal
    //    method of value with property name then.
    if (value.isObject()) {
        then = value.get(exec, exec->propertyNames().then);

        // 3. If calling the [[Get]] internal method threw an exception, catch it and run reject with the thrown
        //    exception and the synchronous flag if set, and then terminate these steps.
        if (exec->hadException()) {
            JSValue exception = exec->exception();
            exec->clearException();

            reject(exec, exception, mode);
            return;
        }

        // 4. If JavaScript IsCallable(then) is true, run these substeps and then terminate these steps:
        CallData callData;
        CallType callType = JSC::getCallData(then, callData);
        if (callType != CallTypeNone) {
            // 4.1. Let fulfillCallback be a promise callback for the context object and its resolve algorithm.
            JSPromiseCallback* fulfillCallback = JSPromiseCallback::create(exec, globalObject(), globalObject()->promiseCallbackStructure(), this, JSPromiseCallback::Resolve);

            // 4.2. Let rejectCallback be a promise callback for the context object and its reject algorithm.
            JSPromiseCallback* rejectCallback = JSPromiseCallback::create(exec, globalObject(), globalObject()->promiseCallbackStructure(), this, JSPromiseCallback::Reject);
            
            // 4.3. Call the JavaScript [[Call]] internal method of then with this value value and fulfillCallback
            //      and rejectCallback as arguments.
            MarkedArgumentBuffer thenArguments;
            thenArguments.append(fulfillCallback);
            thenArguments.append(rejectCallback);
            call(exec, then, callType, callData, value, thenArguments);

            // 4.4 If calling the [[Call]] internal method threw an exception, catch it and run context object's
            //     reject with the thrown exception and the synchronous flag if set.
            if (exec->hadException()) {
                JSValue exception = exec->exception();
                exec->clearException();

                reject(exec, exception, mode);
                return;
            }
            return;
        }
    }

    // 5. Run context object's fulfill with value and the synchronous flag if set.
    fulfill(exec, value, mode);
}

void JSPromiseResolver::reject(ExecState* exec, JSValue value, ResolverMode mode)
{
    // 1. Let promise be the context object's associated promise.
    // 2. Set promise's state to rejected.
    m_promise->setState(JSPromise::Rejected);

    // 3. Set promise's result to value.
    m_promise->setResult(exec->vm(), value);

    // 4. If the synchronous flag is set, process promise's reject callbacks with value.
    if (mode == ResolveSynchronously) {
        m_promise->processRejectCallbacksWithValue(exec, value);
        return;
    }
    
    // 5. Otherwise, the synchronous flag is unset, queue a task to process promise's reject callbacks with value.
    m_promise->queueTaskToProcessRejectCallbacks(exec);
}

} // namespace JSC
