/*
 * Copyright (C) 2015 Yusuke Suzuki <utatane.tea@gmail.com>.
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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
#include "IteratorOperations.h"

#include "JSCInlines.h"
#include "ObjectConstructor.h"

namespace JSC {

JSValue iteratorNext(JSGlobalObject* globalObject, IterationRecord iterationRecord, JSValue argument)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue iterator = iterationRecord.iterator;
    JSValue nextFunction = iterationRecord.nextMethod;

    auto nextFunctionCallData = getCallData(vm, nextFunction);
    if (nextFunctionCallData.type == CallData::Type::None)
        return throwTypeError(globalObject, scope);

    MarkedArgumentBuffer nextFunctionArguments;
    if (!argument.isEmpty())
        nextFunctionArguments.append(argument);
    ASSERT(!nextFunctionArguments.hasOverflowed());
    JSValue result = call(globalObject, nextFunction, nextFunctionCallData, iterator, nextFunctionArguments);
    RETURN_IF_EXCEPTION(scope, JSValue());

    if (!result.isObject())
        return throwTypeError(globalObject, scope, "Iterator result interface is not an object."_s);

    return result;
}

JSValue iteratorValue(JSGlobalObject* globalObject, JSValue iterResult)
{
    return iterResult.get(globalObject, globalObject->vm().propertyNames->value);
}

bool iteratorComplete(JSGlobalObject* globalObject, JSValue iterResult)
{
    JSValue done = iterResult.get(globalObject, globalObject->vm().propertyNames->done);
    return done.toBoolean(globalObject);
}

JSValue iteratorStep(JSGlobalObject* globalObject, IterationRecord iterationRecord)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue result = iteratorNext(globalObject, iterationRecord);
    RETURN_IF_EXCEPTION(scope, JSValue());
    bool done = iteratorComplete(globalObject, result);
    RETURN_IF_EXCEPTION(scope, JSValue());
    if (done)
        return jsBoolean(false);
    return result;
}

void iteratorClose(JSGlobalObject* globalObject, IterationRecord iterationRecord)
{
    VM& vm = globalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    auto catchScope = DECLARE_CATCH_SCOPE(vm);

    Exception* exception = nullptr;
    if (UNLIKELY(catchScope.exception())) {
        exception = catchScope.exception();
        catchScope.clearException();
    }

    JSValue returnFunction = iterationRecord.iterator.get(globalObject, vm.propertyNames->returnKeyword);
    if (UNLIKELY(throwScope.exception()) || returnFunction.isUndefined()) {
        if (exception)
            throwException(globalObject, throwScope, exception);
        return;
    }

    auto returnFunctionCallData = getCallData(vm, returnFunction);
    if (returnFunctionCallData.type == CallData::Type::None) {
        if (exception)
            throwException(globalObject, throwScope, exception);
        else
            throwTypeError(globalObject, throwScope);
        return;
    }

    MarkedArgumentBuffer returnFunctionArguments;
    ASSERT(!returnFunctionArguments.hasOverflowed());
    JSValue innerResult = call(globalObject, returnFunction, returnFunctionCallData, iterationRecord.iterator, returnFunctionArguments);

    if (exception) {
        throwException(globalObject, throwScope, exception);
        return;
    }

    RETURN_IF_EXCEPTION(throwScope, void());

    if (!innerResult.isObject()) {
        throwTypeError(globalObject, throwScope, "Iterator result interface is not an object."_s);
        return;
    }
}

static const PropertyOffset valuePropertyOffset = 0;
static const PropertyOffset donePropertyOffset = 1;

Structure* createIteratorResultObjectStructure(VM& vm, JSGlobalObject& globalObject)
{
    Structure* iteratorResultStructure = vm.structureCache.emptyObjectStructureForPrototype(&globalObject, globalObject.objectPrototype(), JSFinalObject::defaultInlineCapacity());
    PropertyOffset offset;
    iteratorResultStructure = Structure::addPropertyTransition(vm, iteratorResultStructure, vm.propertyNames->value, 0, offset);
    RELEASE_ASSERT(offset == valuePropertyOffset);
    iteratorResultStructure = Structure::addPropertyTransition(vm, iteratorResultStructure, vm.propertyNames->done, 0, offset);
    RELEASE_ASSERT(offset == donePropertyOffset);
    return iteratorResultStructure;
}

JSObject* createIteratorResultObject(JSGlobalObject* globalObject, JSValue value, bool done)
{
    VM& vm = globalObject->vm();
    JSObject* resultObject = constructEmptyObject(vm, globalObject->iteratorResultObjectStructure());
    resultObject->putDirect(vm, valuePropertyOffset, value);
    resultObject->putDirect(vm, donePropertyOffset, jsBoolean(done));
    return resultObject;
}

bool hasIteratorMethod(JSGlobalObject* globalObject, JSValue value)
{
    auto& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!value.isObject())
        return false;

    JSObject* object = asObject(value);
    CallData callData;
    JSValue applyMethod = object->getMethod(globalObject, callData, vm.propertyNames->iteratorSymbol, "Symbol.iterator property should be callable"_s);
    RETURN_IF_EXCEPTION(scope, false);

    return !applyMethod.isUndefined();
}

JSValue iteratorMethod(JSGlobalObject* globalObject, JSObject* object)
{
    auto& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    CallData callData;
    JSValue method = object->getMethod(globalObject, callData, vm.propertyNames->iteratorSymbol, "Symbol.iterator property should be callable"_s);
    RETURN_IF_EXCEPTION(scope, jsUndefined());

    return method;
}

IterationRecord iteratorForIterable(JSGlobalObject* globalObject, JSObject* object, JSValue iteratorMethod)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto iteratorMethodCallData = getCallData(vm, iteratorMethod);
    if (iteratorMethodCallData.type == CallData::Type::None) {
        throwTypeError(globalObject, scope);
        return { };
    }

    ArgList iteratorMethodArguments;
    JSValue iterator = call(globalObject, iteratorMethod, iteratorMethodCallData, object, iteratorMethodArguments);
    RETURN_IF_EXCEPTION(scope, { });

    if (!iterator.isObject()) {
        throwTypeError(globalObject, scope);
        return { };
    }

    JSValue nextMethod = iterator.getObject()->get(globalObject, vm.propertyNames->next);
    RETURN_IF_EXCEPTION(scope, { });

    return { iterator, nextMethod };
}

IterationRecord iteratorForIterable(JSGlobalObject* globalObject, JSValue iterable)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    JSValue iteratorFunction = iterable.get(globalObject, vm.propertyNames->iteratorSymbol);
    RETURN_IF_EXCEPTION(scope, { });
    
    auto iteratorFunctionCallData = getCallData(vm, iteratorFunction);
    if (iteratorFunctionCallData.type == CallData::Type::None) {
        throwTypeError(globalObject, scope);
        return { };
    }

    ArgList iteratorFunctionArguments;
    JSValue iterator = call(globalObject, iteratorFunction, iteratorFunctionCallData, iterable, iteratorFunctionArguments);
    RETURN_IF_EXCEPTION(scope, { });

    if (!iterator.isObject()) {
        throwTypeError(globalObject, scope);
        return { };
    }

    JSValue nextMethod = iterator.getObject()->get(globalObject, vm.propertyNames->next);
    RETURN_IF_EXCEPTION(scope, { });

    return { iterator, nextMethod };
}

} // namespace JSC
