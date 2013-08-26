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
#include "JSPromiseConstructor.h"

#include "Error.h"
#include "JSCJSValueInlines.h"
#include "JSCellInlines.h"
#include "JSPromise.h"
#include "JSPromiseCallback.h"
#include "JSPromisePrototype.h"
#include "JSPromiseResolver.h"
#include "Lookup.h"
#include "StructureInlines.h"

namespace JSC {

ASSERT_HAS_TRIVIAL_DESTRUCTOR(JSPromiseConstructor);

// static Promise fulfill(any value);
static EncodedJSValue JSC_HOST_CALL JSPromiseConstructorFuncFulfill(ExecState*);
// static Promise resolve(any value); // same as any(value)
static EncodedJSValue JSC_HOST_CALL JSPromiseConstructorFuncResolve(ExecState*);
// static Promise reject(any value);
static EncodedJSValue JSC_HOST_CALL JSPromiseConstructorFuncReject(ExecState*);

}

#include "JSPromiseConstructor.lut.h"

namespace JSC {

const ClassInfo JSPromiseConstructor::s_info = { "Function", &InternalFunction::s_info, 0, ExecState::promiseConstructorTable, CREATE_METHOD_TABLE(JSPromiseConstructor) };

/* Source for JSPromiseConstructor.lut.h
@begin promiseConstructorTable
  fulfill         JSPromiseConstructorFuncFulfill             DontEnum|Function 1
  resolve         JSPromiseConstructorFuncResolve             DontEnum|Function 1
  reject          JSPromiseConstructorFuncReject              DontEnum|Function 1
@end
*/

JSPromiseConstructor* JSPromiseConstructor::create(ExecState* exec, JSGlobalObject* globalObject, Structure* structure, JSPromisePrototype* promisePrototype)
{
    JSPromiseConstructor* constructor = new (NotNull, allocateCell<JSPromiseConstructor>(*exec->heap())) JSPromiseConstructor(globalObject, structure);
    constructor->finishCreation(exec, promisePrototype);
    return constructor;
}

Structure* JSPromiseConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

JSPromiseConstructor::JSPromiseConstructor(JSGlobalObject* globalObject, Structure* structure)
    : InternalFunction(globalObject, structure) 
{
}

void JSPromiseConstructor::finishCreation(ExecState* exec, JSPromisePrototype* promisePrototype)
{
    Base::finishCreation(exec->vm(), "Promise");
    putDirectWithoutTransition(exec->vm(), exec->propertyNames().prototype, promisePrototype, DontEnum | DontDelete | ReadOnly);
    putDirectWithoutTransition(exec->vm(), exec->propertyNames().length, jsNumber(1), ReadOnly | DontEnum | DontDelete);
}

static EncodedJSValue JSC_HOST_CALL constructPromise(ExecState* exec)
{
    if (!exec->argumentCount())
        return throwVMError(exec, createTypeError(exec, "Expected at least one argument"));

    JSValue function = exec->argument(0);

    CallData callData;
    CallType callType = getCallData(function, callData);
    if (callType == CallTypeNone)
        return throwVMError(exec, createTypeError(exec, "Expected function as as first argument"));

    JSGlobalObject* globalObject = asInternalFunction(exec->callee())->globalObject();
    
    // 1. Let promise be a new promise.
    JSPromise* promise = JSPromise::createWithResolver(exec->vm(), globalObject);

    // 2. Let resolver be promise's associated resolver.
    JSPromiseResolver* resolver = promise->resolver();

    // 3. Set init's callback this value to promise.
    // 4. Invoke init with resolver passed as parameter.
    MarkedArgumentBuffer initArguments;
    initArguments.append(resolver);
    call(exec, function, callType, callData, promise, initArguments);

    // 5. If init threw an exception, catch it, and then, if resolver's resolved flag
    //    is unset, run resolver's reject with the thrown exception as argument.
    if (exec->hadException()) {
        JSValue exception = exec->exception();
        exec->clearException();

        resolver->rejectIfNotResolved(exec, exception);
    }

    return JSValue::encode(promise);
}

ConstructType JSPromiseConstructor::getConstructData(JSCell*, ConstructData& constructData)
{
    constructData.native.function = constructPromise;
    return ConstructTypeHost;
}

CallType JSPromiseConstructor::getCallData(JSCell*, CallData&)
{
    return CallTypeNone;
}

bool JSPromiseConstructor::getOwnPropertySlot(JSObject* object, ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    return getStaticFunctionSlot<InternalFunction>(exec, ExecState::promiseConstructorTable(exec), jsCast<JSPromiseConstructor*>(object), propertyName, slot);
}

EncodedJSValue JSC_HOST_CALL JSPromiseConstructorFuncFulfill(ExecState* exec)
{
    if (!exec->argumentCount())
        return throwVMError(exec, createTypeError(exec, "Expected at least one argument"));

    JSGlobalObject* globalObject = exec->callee()->globalObject();

    JSPromise* promise = JSPromise::createWithResolver(exec->vm(), globalObject);
    promise->resolver()->fulfill(exec, exec->argument(0));

    return JSValue::encode(promise);
}

EncodedJSValue JSC_HOST_CALL JSPromiseConstructorFuncResolve(ExecState* exec)
{
    if (!exec->argumentCount())
        return throwVMError(exec, createTypeError(exec, "Expected at least one argument"));

    JSGlobalObject* globalObject = exec->callee()->globalObject();

    JSPromise* promise = JSPromise::createWithResolver(exec->vm(), globalObject);
    promise->resolver()->resolve(exec, exec->argument(0));

    return JSValue::encode(promise);
}

EncodedJSValue JSC_HOST_CALL JSPromiseConstructorFuncReject(ExecState* exec)
{
    if (!exec->argumentCount())
        return throwVMError(exec, createTypeError(exec, "Expected at least one argument"));

    JSGlobalObject* globalObject = exec->callee()->globalObject();

    JSPromise* promise = JSPromise::createWithResolver(exec->vm(), globalObject);
    promise->resolver()->reject(exec, exec->argument(0));

    return JSValue::encode(promise);
}

} // namespace JSC
