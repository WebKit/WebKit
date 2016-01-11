/*
 * Copyright (C) 2015 Apple, Inc. All rights reserved.
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
#include "WeakSetConstructor.h"

#include "Error.h"
#include "IteratorOperations.h"
#include "JSCJSValueInlines.h"
#include "JSCellInlines.h"
#include "JSGlobalObject.h"
#include "JSWeakSet.h"
#include "StructureInlines.h"
#include "WeakSetPrototype.h"

namespace JSC {

const ClassInfo WeakSetConstructor::s_info = { "Function", &Base::s_info, 0, CREATE_METHOD_TABLE(WeakSetConstructor) };

void WeakSetConstructor::finishCreation(VM& vm, WeakSetPrototype* prototype)
{
    Base::finishCreation(vm, prototype->classInfo()->className);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, prototype, DontEnum | DontDelete | ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(0), ReadOnly | DontEnum | DontDelete);
}

static EncodedJSValue JSC_HOST_CALL callWeakSet(ExecState* exec)
{
    return JSValue::encode(throwTypeError(exec, ASCIILiteral("WeakSet cannot be called as a function")));
}

static EncodedJSValue JSC_HOST_CALL constructWeakSet(ExecState* exec)
{
    JSGlobalObject* globalObject = asInternalFunction(exec->callee())->globalObject();
    Structure* weakSetStructure = InternalFunction::createSubclassStructure(exec, exec->newTarget(), globalObject->weakSetStructure());
    JSWeakSet* weakSet = JSWeakSet::create(exec, weakSetStructure);
    JSValue iterable = exec->argument(0);
    if (iterable.isUndefinedOrNull())
        return JSValue::encode(weakSet);

    JSValue adderFunction = weakSet->JSObject::get(exec, exec->propertyNames().add);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    CallData adderFunctionCallData;
    CallType adderFunctionCallType = getCallData(adderFunction, adderFunctionCallData);
    if (adderFunctionCallType == CallTypeNone)
        return JSValue::encode(throwTypeError(exec));

    JSValue iteratorFunction = iterable.get(exec, exec->propertyNames().iteratorSymbol);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    CallData iteratorFunctionCallData;
    CallType iteratorFunctionCallType = getCallData(iteratorFunction, iteratorFunctionCallData);
    if (iteratorFunctionCallType == CallTypeNone)
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
            return JSValue::encode(weakSet);

        JSValue nextValue = iteratorValue(exec, next);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());

        MarkedArgumentBuffer arguments;
        arguments.append(nextValue);
        call(exec, adderFunction, adderFunctionCallType, adderFunctionCallData, weakSet, arguments);
        if (exec->hadException()) {
            iteratorClose(exec, iterator);
            return JSValue::encode(jsUndefined());
        }
    }
    RELEASE_ASSERT_NOT_REACHED();
    return JSValue::encode(weakSet);
}

ConstructType WeakSetConstructor::getConstructData(JSCell*, ConstructData& constructData)
{
    constructData.native.function = constructWeakSet;
    return ConstructTypeHost;
}

CallType WeakSetConstructor::getCallData(JSCell*, CallData& callData)
{
    callData.native.function = callWeakSet;
    return CallTypeHost;
}

}
