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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SetConstructor.h"

#include "Error.h"
#include "GetterSetter.h"
#include "IteratorOperations.h"
#include "JSCJSValueInlines.h"
#include "JSCellInlines.h"
#include "JSGlobalObject.h"
#include "JSSet.h"
#include "MapData.h"
#include "SetPrototype.h"
#include "StructureInlines.h"

namespace JSC {

const ClassInfo SetConstructor::s_info = { "Function", &Base::s_info, 0, CREATE_METHOD_TABLE(SetConstructor) };

void SetConstructor::finishCreation(VM& vm, SetPrototype* setPrototype, GetterSetter* speciesSymbol)
{
    Base::finishCreation(vm, setPrototype->classInfo()->className);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, setPrototype, DontEnum | DontDelete | ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(0), ReadOnly | DontEnum | DontDelete);
    putDirectNonIndexAccessor(vm, vm.propertyNames->speciesSymbol, speciesSymbol, Accessor | ReadOnly | DontEnum);
}

static EncodedJSValue JSC_HOST_CALL callSet(ExecState* exec)
{
    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(exec, "Set"));
}

static EncodedJSValue JSC_HOST_CALL constructSet(ExecState* exec)
{
    JSGlobalObject* globalObject = asInternalFunction(exec->callee())->globalObject();
    Structure* setStructure = InternalFunction::createSubclassStructure(exec, exec->newTarget(), globalObject->setStructure());
    if (exec->hadException())
        return JSValue::encode(JSValue());
    JSSet* set = JSSet::create(exec, setStructure);
    JSValue iterable = exec->argument(0);
    if (iterable.isUndefinedOrNull())
        return JSValue::encode(set);

    JSValue adderFunction = set->get(exec, exec->propertyNames().add);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    CallData adderFunctionCallData;
    CallType adderFunctionCallType = getCallData(adderFunction, adderFunctionCallData);
    if (adderFunctionCallType == CallType::None)
        return JSValue::encode(throwTypeError(exec));

    JSValue iteratorFunction = iterable.get(exec, exec->propertyNames().iteratorSymbol);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    CallData iteratorFunctionCallData;
    CallType iteratorFunctionCallType = getCallData(iteratorFunction, iteratorFunctionCallData);
    if (iteratorFunctionCallType == CallType::None)
        return JSValue::encode(throwTypeError(exec));

    ArgList iteratorFunctionArguments;
    JSValue iterator = call(exec, iteratorFunction, iteratorFunctionCallType, iteratorFunctionCallData, iterable, iteratorFunctionArguments);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    if (!iterator.isObject())
        return JSValue::encode(throwTypeError(exec));

    while (true) {
        JSValue next = iteratorStep(exec, iterator);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());

        if (next.isFalse())
            return JSValue::encode(set);

        JSValue nextValue = iteratorValue(exec, next);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());

        MarkedArgumentBuffer arguments;
        arguments.append(nextValue);
        call(exec, adderFunction, adderFunctionCallType, adderFunctionCallData, set, arguments);
        if (exec->hadException()) {
            iteratorClose(exec, iterator);
            return JSValue::encode(jsUndefined());
        }
    }
    RELEASE_ASSERT_NOT_REACHED();
    return JSValue::encode(set);
}

ConstructType SetConstructor::getConstructData(JSCell*, ConstructData& constructData)
{
    constructData.native.function = constructSet;
    return ConstructType::Host;
}

CallType SetConstructor::getCallData(JSCell*, CallData& callData)
{
    callData.native.function = callSet;
    return CallType::Host;
}

}
