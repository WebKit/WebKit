/*
 * Copyright (C) 2013, 2016 Apple Inc. All rights reserved.
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

#include "BuiltinNames.h"
#include "Error.h"
#include "Exception.h"
#include "GetterSetter.h"
#include "IteratorOperations.h"
#include "JSCBuiltins.h"
#include "JSCInlines.h"
#include "JSFunction.h"
#include "JSPromise.h"
#include "JSPromisePrototype.h"
#include "Lookup.h"
#include "NumberObject.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSPromiseConstructor);

}

#include "JSPromiseConstructor.lut.h"

namespace JSC {

const ClassInfo JSPromiseConstructor::s_info = { "Function", &Base::s_info, &promiseConstructorTable, nullptr, CREATE_METHOD_TABLE(JSPromiseConstructor) };

/* Source for JSPromiseConstructor.lut.h
@begin promiseConstructorTable
  resolve         JSBuiltin             DontEnum|Function 1
  reject          JSBuiltin             DontEnum|Function 1
  race            JSBuiltin             DontEnum|Function 1
  all             JSBuiltin             DontEnum|Function 1
@end
*/

JSPromiseConstructor* JSPromiseConstructor::create(VM& vm, Structure* structure, JSPromisePrototype* promisePrototype, GetterSetter* speciesSymbol)
{
    JSPromiseConstructor* constructor = new (NotNull, allocateCell<JSPromiseConstructor>(vm.heap)) JSPromiseConstructor(vm, structure);
    constructor->finishCreation(vm, promisePrototype, speciesSymbol);
    constructor->addOwnInternalSlots(vm, structure->globalObject());
    return constructor;
}

Structure* JSPromiseConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
}


static EncodedJSValue JSC_HOST_CALL callPromise(ExecState*);
static EncodedJSValue JSC_HOST_CALL constructPromise(ExecState*);

JSPromiseConstructor::JSPromiseConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, callPromise, constructPromise)
{
}

JSPromiseConstructor::JSPromiseConstructor(VM& vm, Structure* structure, NativeFunction functionForCall, NativeFunction functionForConstruct)
    : Base(vm, structure, functionForCall, functionForConstruct)
{
}

void JSPromiseConstructor::finishCreation(VM& vm, JSPromisePrototype* promisePrototype, GetterSetter* speciesSymbol)
{
    Base::finishCreation(vm, vm.propertyNames->Promise.string(), NameVisibility::Visible, NameAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, promisePrototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(1), PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
    putDirectNonIndexAccessorWithoutTransition(vm, vm.propertyNames->speciesSymbol, speciesSymbol, PropertyAttribute::Accessor | PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum);
}

void JSPromiseConstructor::addOwnInternalSlots(VM& vm, JSGlobalObject* globalObject)
{
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().resolvePrivateName(), promiseConstructorResolveCodeGenerator, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->builtinNames().rejectPrivateName(), promiseConstructorRejectCodeGenerator, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
}

static EncodedJSValue JSC_HOST_CALL constructPromise(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSGlobalObject* globalObject = exec->jsCallee()->globalObject(vm);

    JSValue newTarget = exec->newTarget();
    if (newTarget.isUndefined())
        return throwVMTypeError(exec, scope);

    Structure* promiseStructure = InternalFunction::createSubclassStructure(exec, exec->newTarget(), globalObject->promiseStructure());
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    JSPromise* promise = JSPromise::create(vm, promiseStructure);
    promise->initialize(exec, globalObject,  exec->argument(0));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    return JSValue::encode(promise);
}

static EncodedJSValue JSC_HOST_CALL callPromise(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(exec, scope, "Promise"));
}

} // namespace JSC
