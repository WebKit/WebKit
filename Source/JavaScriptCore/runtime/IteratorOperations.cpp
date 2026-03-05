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

#include "CachedCall.h"
#include "InterpreterInlines.h"
#include "JSAsyncFromSyncIterator.h"
#include "JSCInlines.h"
#include "ObjectConstructor.h"
#include "VMEntryScopeInlines.h"

namespace JSC {

static JSValue iteratorNextImpl(JSGlobalObject* globalObject, IterationRecord iterationRecord, JSValue argument)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue iterator = iterationRecord.iterator;
    JSValue nextFunction = iterationRecord.nextMethod;

    auto nextFunctionCallData = JSC::getCallDataInline(nextFunction);
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

JSValue iteratorNextWithCachedCall(JSGlobalObject* globalObject, IterationRecord iterationRecord, CachedCall* cachedCall, JSValue argument)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue iterator = iterationRecord.iterator;

    ASSERT(JSC::getCallDataInline(iterationRecord.nextMethod).type == CallData::Type::JS);

    JSValue result;
    if (argument)
        result = cachedCall->callWithArguments(globalObject, iterator, argument);
    else
        result = cachedCall->callWithArguments(globalObject, iterator);
    RETURN_IF_EXCEPTION(scope, { });

    if (!result.isObject()) [[unlikely]]
        return throwTypeError(globalObject, scope, "Iterator result interface is not an object."_s);

    return result;
}

JSValue iteratorNext(JSGlobalObject* globalObject, IterationRecord iterationRecord, JSValue argument)
{
    return iteratorNextImpl(globalObject, iterationRecord, argument);
}

JSValue iteratorNextExported(JSGlobalObject* globalObject, IterationRecord iterationRecord, JSValue argument)
{
    return iteratorNextImpl(globalObject, iterationRecord, argument);
}

JSValue iteratorValue(JSGlobalObject* globalObject, JSValue iterResult)
{
    return iterResult.get(globalObject, globalObject->vm().propertyNames->value);
}

static bool iteratorCompleteImpl(JSGlobalObject* globalObject, JSValue iterResult)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue done = iterResult.get(globalObject, globalObject->vm().propertyNames->done);
    RETURN_IF_EXCEPTION(scope, true);
    RELEASE_AND_RETURN(scope, done.toBoolean(globalObject));
}

bool iteratorComplete(JSGlobalObject* globalObject, JSValue iterResult)
{
    return iteratorCompleteImpl(globalObject, iterResult);
}

bool iteratorCompleteExported(JSGlobalObject* globalObject, JSValue iterResult)
{
    return iteratorCompleteImpl(globalObject, iterResult);
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

JSValue iteratorStepWithCachedCall(JSGlobalObject* globalObject, IterationRecord iterationRecord, CachedCall* cachedCall)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue result = iteratorNextWithCachedCall(globalObject, iterationRecord, cachedCall);
    RETURN_IF_EXCEPTION(scope, JSValue());
    bool done = iteratorComplete(globalObject, result);
    RETURN_IF_EXCEPTION(scope, JSValue());
    if (done)
        return jsBoolean(false);
    return result;
}

void iteratorClose(JSGlobalObject* globalObject, JSValue iterator)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Exception* exception = scope.exception();
    TRY_CLEAR_EXCEPTION(scope, void());

    JSValue returnFunction = iterator.get(globalObject, vm.propertyNames->returnKeyword);
    if (scope.exception()) [[unlikely]] {
        if (exception)
            throwException(globalObject, scope, exception);
        return;
    }

    if (returnFunction.isUndefinedOrNull()) {
        if (exception)
            throwException(globalObject, scope, exception);
        return;
    }

    auto returnFunctionCallData = JSC::getCallDataInline(returnFunction);
    if (returnFunctionCallData.type == CallData::Type::None) {
        if (exception)
            throwException(globalObject, scope, exception);
        else
            throwTypeError(globalObject, scope);
        return;
    }

    MarkedArgumentBuffer returnFunctionArguments;
    ASSERT(!returnFunctionArguments.hasOverflowed());
    JSValue innerResult = call(globalObject, returnFunction, returnFunctionCallData, iterator, returnFunctionArguments);

    if (exception) {
        throwException(globalObject, scope, exception);
        return;
    }

    RETURN_IF_EXCEPTION(scope, void());

    if (!innerResult.isObject()) {
        throwTypeError(globalObject, scope, "Iterator result interface is not an object."_s);
        return;
    }
}

static constexpr PropertyOffset valuePropertyOffset = 0;
static constexpr PropertyOffset donePropertyOffset = 1;

Structure* createIteratorResultObjectStructure(VM& vm, JSGlobalObject& globalObject)
{
    constexpr unsigned inlineCapacity = 2;
    Structure* iteratorResultStructure = globalObject.structureCache().emptyObjectStructureForPrototype(&globalObject, globalObject.objectPrototype(), inlineCapacity);
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
    resultObject->putDirectOffset(vm, valuePropertyOffset, value);
    resultObject->putDirectOffset(vm, donePropertyOffset, jsBoolean(done));
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

    auto iteratorMethodCallData = JSC::getCallDataInline(iteratorMethod);
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
    
    auto iteratorFunctionCallData = JSC::getCallDataInline(iteratorFunction);
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

IterationRecord iteratorDirect(JSGlobalObject* globalObject, JSValue object)
{
    return { object, object.get(globalObject, globalObject->vm().propertyNames->next) };
}

// https://tc39.es/ecma262/multipage/abstract-operations.html#sec-getiterator, ASYNC kind
static IterationRecord getAsyncIteratorImpl(JSGlobalObject& globalObject, JSValue iterable)
{
    auto& vm = globalObject.vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    auto* iterableObject = iterable.getObject();
    if (!iterableObject) [[unlikely]] {
        throwTypeError(&globalObject, throwScope, "iterable should be an object"_s);
        return { };
    }

    CallData callData;
    auto method = iterableObject->getMethod(&globalObject, callData, vm.propertyNames->asyncIteratorSymbol, "asyncIteratorSymbol property should be callable"_s);
    RETURN_IF_EXCEPTION(throwScope, { });

    if (method.isUndefined()) {
        auto syncMethod = iteratorMethod(&globalObject, iterableObject);
        RETURN_IF_EXCEPTION(throwScope, { });

        if (syncMethod.isUndefined()) [[unlikely]] {
            throwTypeError(&globalObject, throwScope, "iterable should have an iterator symbol"_s);
            return { };
        }

        callData = getCallData(syncMethod);
        auto iterator = call(&globalObject, syncMethod, callData, iterableObject, { });
        RETURN_IF_EXCEPTION(throwScope, { });

        auto* iteratorObject = iterator.getObject();
        if (!iteratorObject) [[unlikely]] {
            throwTypeError(&globalObject, throwScope, "iterator method should return an object"_s);
            return { };
        }

        auto syncIteratorRecord = iteratorDirect(&globalObject, iterator);
        RETURN_IF_EXCEPTION(throwScope, { });

        auto* asyncFromSyncIterator = JSAsyncFromSyncIterator::create(vm, globalObject.asyncFromSyncIteratorStructure(), syncIteratorRecord.iterator, syncIteratorRecord.nextMethod);
        RETURN_IF_EXCEPTION(throwScope, { });

        auto record = iteratorDirect(&globalObject, asyncFromSyncIterator);
        RETURN_IF_EXCEPTION(throwScope, { });
        return record;
    }

    if (method.isUndefined()) [[unlikely]] {
        throwTypeError(&globalObject, throwScope, "iterable should have an iterator symbol"_s);
        return { };
    }

    callData = getCallData(method);
    auto iterator = call(&globalObject, method, callData, iterableObject, { });
    RETURN_IF_EXCEPTION(throwScope, { });

    auto* iteratorObject = iterator.getObject();
    if (!iteratorObject) [[unlikely]] {
        throwTypeError(&globalObject, throwScope, "iterator method should return an object"_s);
        return { };
    }

    RELEASE_AND_RETURN(throwScope, iteratorDirect(&globalObject, iterator));
}

IterationRecord getAsyncIterator(JSGlobalObject& globalObject, JSValue iterable)
{
    return getAsyncIteratorImpl(globalObject, iterable);
}

IterationRecord getAsyncIteratorExported(JSGlobalObject& globalObject, JSValue iterable)
{
    return getAsyncIteratorImpl(globalObject, iterable);
}

IterableValidationResult validateIterable(VM&, JSValue iterable, JSValue symbolIterator)
{
    if (!symbolIterator.isCallable()) [[unlikely]] {
        if (iterable.isNumber())
            return IterableValidationResult::NumberNotIterable;
        if (iterable.isBoolean())
            return IterableValidationResult::BooleanNotIterable;
        if (iterable.isSymbol())
            return IterableValidationResult::SymbolNotIterable;
        if (iterable.isNull())
            return IterableValidationResult::NullNotIterable;
        if (iterable.isUndefined())
            return IterableValidationResult::UndefinedNotIterable;
        if (iterable.isObject())
            return IterableValidationResult::ObjectNotIterable;
        return IterableValidationResult::ValueNotIterable;
    }

    return IterableValidationResult::Valid;
}


ASCIILiteral getIteratorErrorMessage(IterableValidationResult result, JSValue iterable)
{
    switch (result) {
    case IterableValidationResult::NullNotIterable:
        return "null is not an object"_s;
    case IterableValidationResult::UndefinedNotIterable:
        return "undefined is not an object"_s;
    case IterableValidationResult::NumberNotIterable:
        return "number is not iterable"_s;
    case IterableValidationResult::BooleanNotIterable:
        return iterable.asBoolean() ? "true is not iterable"_s : "false is not iterable"_s;
    case IterableValidationResult::SymbolNotIterable:
        return "value is not iterable"_s;
    case IterableValidationResult::ObjectNotIterable:
        return "{} is not iterable"_s;
    case IterableValidationResult::ValueNotIterable:
        return "value is not iterable"_s;
    case IterableValidationResult::Valid:
        break;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return ""_s;
}

IterationMode getIterationMode(VM&, JSGlobalObject* globalObject, JSValue iterable, JSValue symbolIterator)
{
    if (!isJSArray(iterable))
        return IterationMode::Generic;

    if (!globalObject->arrayIteratorProtocolWatchpointSet().isStillValid())
        return IterationMode::Generic;

    // This is correct because we just checked the watchpoint is still valid.
    JSFunction* symbolIteratorFunction = jsDynamicCast<JSFunction*>(symbolIterator);
    if (!symbolIteratorFunction)
        return IterationMode::Generic;

    // We don't want to allocate the values function just to check if it's the same as our function so we use the concurrent accessor.
    // FIXME: This only works for arrays from the same global object as ourselves but we should be able to support any pairing.
    if (globalObject->arrayProtoValuesFunctionConcurrently() != symbolIteratorFunction)
        return IterationMode::Generic;

    return IterationMode::FastArray;
}

IterationMode getIterationMode(VM&, JSGlobalObject* globalObject, JSValue iterable)
{
    if (!isJSArray(iterable))
        return IterationMode::Generic;

    JSArray* array = jsCast<JSArray*>(iterable);
    Structure* structure = array->structure();
    // FIXME: We want to support broader JSArrays as long as array[@@iterator] is not defined.
    if (!globalObject->isOriginalArrayStructure(structure))
        return IterationMode::Generic;

    if (!globalObject->arrayIteratorProtocolWatchpointSet().isStillValid())
        return IterationMode::Generic;

    // Now, Array has original Array Structures and arrayIteratorProtocolWatchpointSet is not fired.
    // This means,
    // 1. Array.prototype is [[Prototype]].
    // 2. array[@@iterator] is not overridden.
    // 3. Array.prototype[@@iterator] is an expected one.
    // So, we can say this will create an expected ArrayIterator.
    return IterationMode::FastArray;
}

} // namespace JSC
