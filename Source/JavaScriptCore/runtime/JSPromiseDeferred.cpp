/*
 * Copyright (C) 2013-2017 Apple Inc. All rights reserved.
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
#include "JSPromiseDeferred.h"

#include "BuiltinNames.h"
#include "Error.h"
#include "Exception.h"
#include "JSCInlines.h"
#include "JSObjectInlines.h"
#include "JSPromise.h"
#include "JSPromiseConstructor.h"
#include "PromiseDeferredTimer.h"

namespace JSC {

const ClassInfo JSPromiseDeferred::s_info = { "JSPromiseDeferred", nullptr, nullptr, nullptr, CREATE_METHOD_TABLE(JSPromiseDeferred) };

JSPromiseDeferred::DeferredData JSPromiseDeferred::createDeferredData(ExecState* exec, JSGlobalObject* globalObject, JSPromiseConstructor* promiseConstructor)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSFunction* newPromiseCapabilityFunction = globalObject->newPromiseCapabilityFunction();
    CallData callData;
    CallType callType = JSC::getCallData(exec->vm(), newPromiseCapabilityFunction, callData);
    ASSERT(callType != CallType::None);

    MarkedArgumentBuffer arguments;
    arguments.append(promiseConstructor);
    ASSERT(!arguments.hasOverflowed());
    JSValue deferred = call(exec, newPromiseCapabilityFunction, callType, callData, jsUndefined(), arguments);
    RETURN_IF_EXCEPTION(scope, { });

    DeferredData result;
    result.promise = jsCast<JSPromise*>(deferred.get(exec, vm.propertyNames->builtinNames().promisePrivateName()));
    RETURN_IF_EXCEPTION(scope, { });
    result.resolve = jsCast<JSFunction*>(deferred.get(exec, vm.propertyNames->builtinNames().resolvePrivateName()));
    RETURN_IF_EXCEPTION(scope, { });
    result.reject = jsCast<JSFunction*>(deferred.get(exec, vm.propertyNames->builtinNames().rejectPrivateName()));
    RETURN_IF_EXCEPTION(scope, { });

    return result;
}

JSPromiseDeferred* JSPromiseDeferred::tryCreate(ExecState* exec, JSGlobalObject* globalObject)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    DeferredData data = createDeferredData(exec, globalObject, globalObject->promiseConstructor());
    RETURN_IF_EXCEPTION(scope, { });
    return JSPromiseDeferred::create(vm, data.promise, data.resolve, data.reject);
}

JSPromiseDeferred* JSPromiseDeferred::create(VM& vm, JSPromise* promise, JSFunction* resolve, JSFunction* reject)
{
    JSPromiseDeferred* deferred = new (NotNull, allocateCell<JSPromiseDeferred>(vm.heap)) JSPromiseDeferred(vm);
    deferred->finishCreation(vm, promise, resolve, reject);
    return deferred;
}

JSPromiseDeferred::JSPromiseDeferred(VM& vm)
    : JSPromiseDeferred(vm, vm.promiseDeferredStructure.get())
{
}

JSPromiseDeferred::JSPromiseDeferred(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

static inline void callFunction(ExecState* exec, JSValue function, JSValue value)
{
    CallData callData;
    CallType callType = getCallData(exec->vm(), function, callData);
    ASSERT(callType != CallType::None);

    MarkedArgumentBuffer arguments;
    arguments.append(value);
    ASSERT(!arguments.hasOverflowed());

    call(exec, function, callType, callData, jsUndefined(), arguments);
}

void JSPromiseDeferred::resolve(ExecState* exec, JSValue value)
{
    callFunction(exec, m_resolve.get(), value);
    bool wasPending = exec->vm().promiseDeferredTimer->cancelPendingPromise(this);
    ASSERT_UNUSED(wasPending, wasPending == m_promiseIsAsyncPending);
}

void JSPromiseDeferred::reject(ExecState* exec, JSValue reason)
{
    callFunction(exec, m_reject.get(), reason);
    bool wasPending = exec->vm().promiseDeferredTimer->cancelPendingPromise(this);
    ASSERT_UNUSED(wasPending, wasPending == m_promiseIsAsyncPending);
}

void JSPromiseDeferred::reject(ExecState* exec, Exception* reason)
{
    reject(exec, reason->value());
}

void JSPromiseDeferred::finishCreation(VM& vm, JSPromise* promise, JSFunction* resolve, JSFunction* reject)
{
    Base::finishCreation(vm);
    m_promise.set(vm, this, promise);
    m_resolve.set(vm, this, resolve);
    m_reject.set(vm, this, reject);
}

void JSPromiseDeferred::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    JSPromiseDeferred* thisObject = jsCast<JSPromiseDeferred*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    Base::visitChildren(thisObject, visitor);

    visitor.append(thisObject->m_promise);
    visitor.append(thisObject->m_resolve);
    visitor.append(thisObject->m_reject);
}

} // namespace JSC
