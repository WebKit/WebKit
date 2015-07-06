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

#if ENABLE(PROMISES)

#include "Error.h"
#include "Exception.h"
#include "IteratorOperations.h"
#include "JSCBuiltins.h"
#include "JSCJSValueInlines.h"
#include "JSCellInlines.h"
#include "JSPromise.h"
#include "JSPromisePrototype.h"
#include "Lookup.h"
#include "NumberObject.h"
#include "StructureInlines.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSPromiseConstructor);

}

#include "JSPromiseConstructor.lut.h"

namespace JSC {

const ClassInfo JSPromiseConstructor::s_info = { "Function", &InternalFunction::s_info, &promiseConstructorTable, CREATE_METHOD_TABLE(JSPromiseConstructor) };

/* Source for JSPromiseConstructor.lut.h
@begin promiseConstructorTable
  resolve         JSPromiseConstructorFuncResolve             DontEnum|Function 1
  reject          JSPromiseConstructorFuncReject              DontEnum|Function 1
  race            JSPromiseConstructorFuncRace                DontEnum|Function 1
  all             JSPromiseConstructorFuncAll                 DontEnum|Function 1
@end
*/

JSPromiseConstructor* JSPromiseConstructor::create(VM& vm, Structure* structure, JSPromisePrototype* promisePrototype)
{
    JSPromiseConstructor* constructor = new (NotNull, allocateCell<JSPromiseConstructor>(vm.heap)) JSPromiseConstructor(vm, structure);
    constructor->finishCreation(vm, promisePrototype);
    return constructor;
}

Structure* JSPromiseConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

JSPromiseConstructor::JSPromiseConstructor(VM& vm, Structure* structure)
    : InternalFunction(vm, structure)
{
}

void JSPromiseConstructor::finishCreation(VM& vm, JSPromisePrototype* promisePrototype)
{
    Base::finishCreation(vm, ASCIILiteral("Promise"));
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, promisePrototype, DontEnum | DontDelete | ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(1), ReadOnly | DontEnum | DontDelete);
}

static EncodedJSValue JSC_HOST_CALL constructPromise(ExecState* exec)
{
    VM& vm = exec->vm();
    JSGlobalObject* globalObject = exec->callee()->globalObject();

    JSPromise* promise = JSPromise::create(vm, globalObject);

    JSFunction* initializePromise = globalObject->initializePromiseFunction();
    CallData callData;
    CallType callType = getCallData(initializePromise, callData);
    ASSERT(callType != CallTypeNone);

    MarkedArgumentBuffer arguments;
    arguments.append(exec->argument(0));
    call(exec, initializePromise, callType, callData, promise, arguments);

    return JSValue::encode(promise);
}

ConstructType JSPromiseConstructor::getConstructData(JSCell*, ConstructData& constructData)
{
    constructData.native.function = constructPromise;
    return ConstructTypeHost;
}

CallType JSPromiseConstructor::getCallData(JSCell*, CallData& callData)
{
    callData.native.function = constructPromise;
    return CallTypeHost;
}

bool JSPromiseConstructor::getOwnPropertySlot(JSObject* object, ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    return getStaticFunctionSlot<InternalFunction>(exec, promiseConstructorTable, jsCast<JSPromiseConstructor*>(object), propertyName, slot);
}

JSPromise* constructPromise(ExecState* exec, JSGlobalObject* globalObject, JSFunction* resolver)
{
    JSPromiseConstructor* promiseConstructor = globalObject->promiseConstructor();

    ConstructData constructData;
    ConstructType constructType = getConstructData(promiseConstructor, constructData);
    ASSERT(constructType != ConstructTypeNone);

    MarkedArgumentBuffer arguments;
    arguments.append(resolver);

    return jsCast<JSPromise*>(construct(exec, promiseConstructor, constructType, constructData, arguments));
}

} // namespace JSC

#endif // ENABLE(PROMISES)
