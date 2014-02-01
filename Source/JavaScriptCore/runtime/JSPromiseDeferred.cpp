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
#include "JSPromiseDeferred.h"

#include "Error.h"
#include "JSCJSValueInlines.h"
#include "JSCellInlines.h"
#include "JSPromise.h"
#include "JSPromiseConstructor.h"
#include "JSPromiseFunctions.h"
#include "SlotVisitorInlines.h"

namespace JSC {

const ClassInfo JSPromiseDeferred::s_info = { "JSPromiseDeferred", 0, 0, 0, CREATE_METHOD_TABLE(JSPromiseDeferred) };

JSPromiseDeferred* JSPromiseDeferred::create(ExecState* exec, JSGlobalObject* globalObject)
{
    VM& vm = exec->vm();
    
    JSFunction* resolver = createDeferredConstructionFunction(vm, globalObject);

    JSPromise* promise = constructPromise(exec, globalObject, resolver);
    JSValue resolve = resolver->get(exec, vm.propertyNames->resolvePrivateName);
    JSValue reject = resolver->get(exec, vm.propertyNames->rejectPrivateName);

    return JSPromiseDeferred::create(vm, promise, resolve, reject);
}

JSPromiseDeferred* JSPromiseDeferred::create(VM& vm, JSObject* promise, JSValue resolve, JSValue reject)
{
    JSPromiseDeferred* deferred = new (NotNull, allocateCell<JSPromiseDeferred>(vm.heap)) JSPromiseDeferred(vm);
    deferred->finishCreation(vm, promise, resolve, reject);
    return deferred;
}

JSPromiseDeferred::JSPromiseDeferred(VM& vm)
    : Base(vm, vm.promiseDeferredStructure.get())
{
}

void JSPromiseDeferred::finishCreation(VM& vm, JSObject* promise, JSValue resolve, JSValue reject)
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
    COMPILE_ASSERT(StructureFlags & OverridesVisitChildren, OverridesVisitChildrenWithoutSettingFlag);
    ASSERT(thisObject->structure()->typeInfo().overridesVisitChildren());

    Base::visitChildren(thisObject, visitor);

    visitor.append(&thisObject->m_promise);
    visitor.append(&thisObject->m_resolve);
    visitor.append(&thisObject->m_reject);
}

JSValue createJSPromiseDeferredFromConstructor(ExecState* exec, JSValue C)
{
    // -- This implements the GetDeferred(C) abstract operation --

    // 1. If IsConstructor(C) is false, throw a TypeError.
    if (!C.isObject())
        return throwTypeError(exec);

    ConstructData constructData;
    ConstructType constructType = getConstructData(C, constructData);
    if (constructType == ConstructTypeNone)
        return throwTypeError(exec);

    VM& vm = exec->vm();

    // 2. Let 'resolver' be a new built-in function object as defined in Deferred Construction Functions.
    JSFunction* resolver = createDeferredConstructionFunction(vm, asObject(C)->globalObject());

    // 3. Let 'promise' be the result of calling the [[Construct]] internal method of 'C' with
    //    an argument list containing the single item resolver.
    MarkedArgumentBuffer constructArguments;
    constructArguments.append(resolver);
    JSObject* promise = construct(exec, C, constructType, constructData, constructArguments);

    // 4. ReturnIfAbrupt(promise).
    if (exec->hadException())
        return jsUndefined();

    // 5. Let 'resolve' be the value of resolver's [[Resolve]] internal slot.
    JSValue resolve = resolver->get(exec, vm.propertyNames->resolvePrivateName);

    // 6. If IsCallable(resolve) is false, throw a TypeError.
    CallData resolveCallData;
    CallType resolveCallType = getCallData(resolve, resolveCallData);
    if (resolveCallType == CallTypeNone)
        return throwTypeError(exec);

    // 7. Let 'reject' be the value of resolver's [[Reject]] internal slot.
    JSValue reject = resolver->get(exec, vm.propertyNames->rejectPrivateName);

    // 8. If IsCallable(reject) is false, throw a TypeError.
    CallData rejectCallData;
    CallType rejectCallType = getCallData(reject, rejectCallData);
    if (rejectCallType == CallTypeNone)
        return throwTypeError(exec);

    // 9. Return the Deferred { [[Promise]]: promise, [[Resolve]]: resolve, [[Reject]]: reject }.
    return JSPromiseDeferred::create(exec->vm(), promise, resolve, reject);
}

ThenableStatus updateDeferredFromPotentialThenable(ExecState* exec, JSValue x, JSPromiseDeferred* deferred)
{
    // 1. If Type(x) is not Object, return "not a thenable".
    if (!x.isObject())
        return NotAThenable;
    
    // 2. Let 'then' be the result of calling Get(x, "then").
    JSValue thenValue = x.get(exec, exec->vm().propertyNames->then);
    
    // 3. If then is an abrupt completion,
    if (exec->hadException()) {
        // i. Let 'rejectResult' be the result of calling the [[Call]] internal method of
        //    deferred.[[Reject]] with undefined as thisArgument and a List containing
        //    then.[[value]] as argumentsList.
        JSValue exception = exec->exception();
        exec->clearException();

        performDeferredReject(exec, deferred, exception);

        // ii. ReturnIfAbrupt(rejectResult).
        // NOTE: Nothing to do.

        // iii. Return.
        return WasAThenable;
    }

    // 4. Let 'then' be then.[[value]].
    // Note: Nothing to do.

    // 5. If IsCallable(then) is false, return "not a thenable".
    CallData thenCallData;
    CallType thenCallType = getCallData(thenValue, thenCallData);
    if (thenCallType == CallTypeNone)
        return NotAThenable;

    // 6. Let 'thenCallResult' be the result of calling the [[Call]] internal method of
    //    'then' passing x as thisArgument and a List containing deferred.[[Resolve]] and
    //    deferred.[[Reject]] as argumentsList.
    MarkedArgumentBuffer thenArguments;
    thenArguments.append(deferred->resolve());
    thenArguments.append(deferred->reject());
    
    call(exec, thenValue, thenCallType, thenCallData, x, thenArguments);

    // 7. If 'thenCallResult' is an abrupt completion,
    if (exec->hadException()) {
        // i. Let 'rejectResult' be the result of calling the [[Call]] internal method of
        //    deferred.[[Reject]] with undefined as thisArgument and a List containing
        //    thenCallResult.[[value]] as argumentsList.
        JSValue exception = exec->exception();
        exec->clearException();

        performDeferredReject(exec, deferred, exception);

        // ii. ReturnIfAbrupt(rejectResult).
        // NOTE: Nothing to do.
    }

    return WasAThenable;
}

void performDeferredResolve(ExecState* exec, JSPromiseDeferred* deferred, JSValue argument)
{
    JSValue deferredResolve = deferred->resolve();

    CallData resolveCallData;
    CallType resolveCallType = getCallData(deferredResolve, resolveCallData);
    ASSERT(resolveCallType != CallTypeNone);

    MarkedArgumentBuffer arguments;
    arguments.append(argument);

    call(exec, deferredResolve, resolveCallType, resolveCallData, jsUndefined(), arguments);
}

void performDeferredReject(ExecState* exec, JSPromiseDeferred* deferred, JSValue argument)
{
    JSValue deferredReject = deferred->reject();

    CallData rejectCallData;
    CallType rejectCallType = getCallData(deferredReject, rejectCallData);
    ASSERT(rejectCallType != CallTypeNone);

    MarkedArgumentBuffer arguments;
    arguments.append(argument);

    call(exec, deferredReject, rejectCallType, rejectCallData, jsUndefined(), arguments);
}

JSValue abruptRejection(ExecState* exec, JSPromiseDeferred* deferred)
{
    ASSERT(exec->hadException());
    JSValue argument = exec->exception();
    exec->clearException();

    // i. Let 'rejectResult' be the result of calling the [[Call]] internal method
    // of deferred.[[Reject]] with undefined as thisArgument and a List containing
    // argument.[[value]] as argumentsList.
    performDeferredReject(exec, deferred, argument);

    // ii. ReturnIfAbrupt(rejectResult).
    if (exec->hadException())
        return jsUndefined();

    // iii. Return deferred.[[Promise]].
    return deferred->promise();
}

} // namespace JSC
