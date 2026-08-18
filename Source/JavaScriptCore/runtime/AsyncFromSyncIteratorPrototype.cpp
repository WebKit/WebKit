/*
 * Copyright (C) 2017 Oleksandr Skachkov <gskachkov@gmail.com>.
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
#include "AsyncFromSyncIteratorPrototype.h"

#include "IteratorOperations.h"
#include "JSArrayInlines.h"
#include "JSArrayIterator.h"
#include "JSArrayIteratorInlines.h"
#include "JSAsyncFromSyncIterator.h"
#include "JSCInlines.h"
#include "JSInternalFieldObjectImplInlines.h"
#include "JSMapIterator.h"
#include "JSPromise.h"
#include "JSSetIterator.h"
#include "Microtask.h"
#include "TopExceptionScope.h"

namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(asyncFromSyncIteratorPrototypeFuncReturn);
static JSC_DECLARE_HOST_FUNCTION(asyncFromSyncIteratorPrototypeFuncThrow);

const ClassInfo AsyncFromSyncIteratorPrototype::s_info = { "AsyncFromSyncIterator"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(AsyncFromSyncIteratorPrototype) };

AsyncFromSyncIteratorPrototype::AsyncFromSyncIteratorPrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void AsyncFromSyncIteratorPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));

    putDirectWithoutTransition(vm, vm.propertyNames->next, globalObject->asyncFromSyncIteratorPrototypeNextFunction(), static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->returnKeyword, asyncFromSyncIteratorPrototypeFuncReturn, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->throwKeyword, asyncFromSyncIteratorPrototypeFuncThrow, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
}

AsyncFromSyncIteratorPrototype* AsyncFromSyncIteratorPrototype::create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
{
    AsyncFromSyncIteratorPrototype* prototype = new (NotNull, allocateCell<AsyncFromSyncIteratorPrototype>(vm)) AsyncFromSyncIteratorPrototype(vm, structure);
    prototype->finishCreation(vm, globalObject);
    return prototype;
}

static void awaitAndContinue(JSGlobalObject* globalObject, VM& vm, JSAsyncFromSyncIterator* iterator, JSObject* target, bool closeSyncIteratorOnRejection, JSValue value, bool done)
{
    InternalMicrotask task = done ? InternalMicrotask::AsyncFromSyncIteratorDone : InternalMicrotask::AsyncFromSyncIteratorContinue;
    iterator->setTarget(vm, target, closeSyncIteratorOnRejection);
    JSPromise::resolveWithInternalMicrotaskForAsyncAwait(globalObject, vm, value, task, iterator);
}

static bool callSyncIteratorMethodAndExtract(JSGlobalObject* globalObject, JSValue method, JSObject* syncIterator, JSValue argument, JSValue& value)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto callData = JSC::getCallData(method);
    if (callData.type == CallData::Type::None) [[unlikely]] {
        throwTypeError(globalObject, scope, "Iterator method is not callable."_s);
        return { };
    }

    auto args = WTF::toArray<EncodedJSValue>({
        JSValue::encode(argument),
    });
    JSValue result = call(globalObject, method, callData, syncIterator, ArgList { args.data(), argument.isEmpty() ? 0u : 1u });
    RETURN_IF_EXCEPTION(scope, { });

    if (!result.isObject()) [[unlikely]] {
        throwTypeError(globalObject, scope, "Iterator result interface is not an object."_s);
        return { };
    }

    bool done = result.get(globalObject, vm.propertyNames->done).toBoolean(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    scope.release();
    value = result.get(globalObject, vm.propertyNames->value);
    return done;
}

static bool driveFastSyncIterator(JSGlobalObject* globalObject, VM& vm, JSObject* syncIterator, IterationMode mode, JSValue& value)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    switch (mode) {
    case IterationMode::FastArrayValues:
    case IterationMode::FastArrayKeys:
    case IterationMode::FastArrayEntries: {
        bool hasNext = uncheckedDowncast<JSArrayIterator>(syncIterator)->next(globalObject, value);
        RETURN_IF_EXCEPTION(scope, false);
        return !hasNext;
    }

    case IterationMode::FastMapKeys:
    case IterationMode::FastMapValues:
    case IterationMode::FastMapEntries: {
        bool hasNext = uncheckedDowncast<JSMapIterator>(syncIterator)->next(globalObject, value);
        RETURN_IF_EXCEPTION(scope, false);
        return !hasNext;
    }

    case IterationMode::FastSetValues:
    case IterationMode::FastSetEntries: {
        bool hasNext = uncheckedDowncast<JSSetIterator>(syncIterator)->next(globalObject, value);
        RETURN_IF_EXCEPTION(scope, false);
        return !hasNext;
    }

    default:
        RELEASE_ASSERT_NOT_REACHED();
        return true;
    }
}

static bool driveSyncIterator(JSGlobalObject* globalObject, VM& vm, JSAsyncFromSyncIterator* iterator, JSValue argument, JSValue& value)
{
    JSObject* syncIterator = iterator->syncIterator();
    if (iterator->iterationMode() != IterationMode::Generic)
        return driveFastSyncIterator(globalObject, vm, syncIterator, iterator->iterationMode(), value);
    return callSyncIteratorMethodAndExtract(globalObject, iterator->nextMethod(), syncIterator, argument, value);
}

// https://tc39.es/ecma262/#sec-%asyncfromsynciteratorprototype%.next
JSValue asyncFromSyncIteratorNext(JSGlobalObject* globalObject, JSAsyncFromSyncIterator* iterator, JSValue argument)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSPromise* promise = JSPromise::create(vm, globalObject->promiseStructure());

    JSValue value = jsUndefined();
    bool done = driveSyncIterator(globalObject, vm, iterator, argument, value);

    if (scope.exception()) [[unlikely]]
        RELEASE_AND_RETURN(scope, promise->rejectWithCaughtException(vm, scope));

    awaitAndContinue(globalObject, vm, iterator, promise, /* closeSyncIteratorOnRejection */ true, value, done);
    return promise;
}

JSC_DEFINE_HOST_FUNCTION(asyncFromSyncIteratorPrototypeFuncNext, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    auto* iterator = downcast<JSAsyncFromSyncIterator>(callFrame->thisValue());
    JSValue argument = callFrame->argumentCount() > 0 ? callFrame->uncheckedArgument(0) : JSValue();
    return JSValue::encode(asyncFromSyncIteratorNext(globalObject, iterator, argument));
}

// https://tc39.es/ecma262/#sec-%asyncfromsynciteratorprototype%.return
JSC_DEFINE_HOST_FUNCTION(asyncFromSyncIteratorPrototypeFuncReturn, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSPromise* promise = JSPromise::create(vm, globalObject->promiseStructure());

    auto* iterator = downcast<JSAsyncFromSyncIterator>(callFrame->thisValue());
    JSObject* syncIterator = iterator->syncIterator();

    JSValue returnMethod = syncIterator->get(globalObject, vm.propertyNames->returnKeyword);
    if (scope.exception()) [[unlikely]]
        RELEASE_AND_RETURN(scope, JSValue::encode(promise->rejectWithCaughtException(vm, scope)));

    if (returnMethod.isUndefinedOrNull()) {
        JSValue value = callFrame->argument(0);
        scope.release();
        promise->resolve(globalObject, vm, createIteratorResultObject(globalObject, value, true));
        return JSValue::encode(promise);
    }

    JSValue value;
    JSValue argument = callFrame->argumentCount() > 0 ? callFrame->uncheckedArgument(0) : JSValue();
    bool done = callSyncIteratorMethodAndExtract(globalObject, returnMethod, syncIterator, argument, value);
    if (scope.exception()) [[unlikely]]
        RELEASE_AND_RETURN(scope, JSValue::encode(promise->rejectWithCaughtException(vm, scope)));

    awaitAndContinue(globalObject, vm, iterator, promise, /* closeSyncIteratorOnRejection */ false, value, done);
    return JSValue::encode(promise);
}

// https://tc39.es/ecma262/#sec-%asyncfromsynciteratorprototype%.throw
JSC_DEFINE_HOST_FUNCTION(asyncFromSyncIteratorPrototypeFuncThrow, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSPromise* promise = JSPromise::create(vm, globalObject->promiseStructure());

    auto* iterator = downcast<JSAsyncFromSyncIterator>(callFrame->thisValue());
    JSObject* syncIterator = iterator->syncIterator();

    JSValue throwMethod = syncIterator->get(globalObject, vm.propertyNames->throwKeyword);
    if (scope.exception()) [[unlikely]]
        RELEASE_AND_RETURN(scope, JSValue::encode(promise->rejectWithCaughtException(vm, scope)));

    if (throwMethod.isUndefinedOrNull()) {
        JSValue returnMethod = syncIterator->get(globalObject, vm.propertyNames->returnKeyword);
        if (scope.exception()) [[unlikely]]
            RELEASE_AND_RETURN(scope, JSValue::encode(promise->rejectWithCaughtException(vm, scope)));
        if (!returnMethod.isUndefinedOrNull()) {
            auto returnCallData = JSC::getCallData(returnMethod);
            if (returnCallData.type == CallData::Type::None) [[unlikely]] {
                throwTypeError(globalObject, scope, "Iterator return method is not callable."_s);
                RELEASE_AND_RETURN(scope, JSValue::encode(promise->rejectWithCaughtException(vm, scope)));
            }

            JSValue returnResult = call(globalObject, returnMethod, returnCallData, syncIterator, { });
            if (scope.exception()) [[unlikely]]
                RELEASE_AND_RETURN(scope, JSValue::encode(promise->rejectWithCaughtException(vm, scope)));

            if (!returnResult.isObject()) [[unlikely]] {
                throwTypeError(globalObject, scope, "Iterator result interface is not an object."_s);
                RELEASE_AND_RETURN(scope, JSValue::encode(promise->rejectWithCaughtException(vm, scope)));
            }
        }

        throwTypeError(globalObject, scope, "Iterator does not provide a throw method."_s);
        RELEASE_AND_RETURN(scope, JSValue::encode(promise->rejectWithCaughtException(vm, scope)));
    }

    JSValue value;
    JSValue argument = callFrame->argumentCount() > 0 ? callFrame->uncheckedArgument(0) : JSValue();
    bool done = callSyncIteratorMethodAndExtract(globalObject, throwMethod, syncIterator, argument, value);
    if (scope.exception()) [[unlikely]]
        RELEASE_AND_RETURN(scope, JSValue::encode(promise->rejectWithCaughtException(vm, scope)));

    awaitAndContinue(globalObject, vm, iterator, promise, /* closeSyncIteratorOnRejection */ true, value, done);
    return JSValue::encode(promise);
}

void driveAsyncFromSyncIteratorWithDriver(JSGlobalObject* globalObject, JSAsyncFromSyncIterator* iterator, JSObject* driver, JSValue resumeValue)
{
    VM& vm = globalObject->vm();

    JSValue value = jsUndefined();
    bool done = false;

    {
        auto catchScope = DECLARE_TOP_EXCEPTION_SCOPE(vm);
        done = driveSyncIterator(globalObject, vm, iterator, resumeValue, value);

        if (catchScope.exception()) [[unlikely]] {
            JSValue error = catchScope.exception()->value();
            if (!catchScope.clearExceptionExceptTermination()) [[unlikely]]
                return;
            JSPromise::rejectWithInternalMicrotask(vm, globalObject, error, InternalMicrotask::AsyncGeneratorDriverResume, driver);
            return;
        }
    }

    awaitAndContinue(globalObject, vm, iterator, driver, /* closeSyncIteratorOnRejection */ true, value, done);
}

} // namespace JSC
