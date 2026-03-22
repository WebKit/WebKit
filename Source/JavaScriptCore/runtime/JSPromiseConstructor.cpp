/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
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

#include "AggregateError.h"
#include "BuiltinNames.h"
#include "CachedCall.h"
#include "GetterSetter.h"
#include "IteratorOperations.h"
#include "InterpreterInlines.h"
#include "JSCBuiltins.h"
#include "JSCInlines.h"
#include "JSFunctionWithFields.h"
#include "JSMicrotask.h"
#include "JSPromise.h"
#include "JSPromiseCombinatorsContext.h"
#include "JSPromiseCombinatorsGlobalContext.h"
#include "JSPromisePrototype.h"
#include "Microtask.h"
#include "ObjectConstructor.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSPromiseConstructor);
static JSC_DECLARE_HOST_FUNCTION(promiseConstructorFuncResolve);
static JSC_DECLARE_HOST_FUNCTION(promiseConstructorFuncReject);
static JSC_DECLARE_HOST_FUNCTION(promiseConstructorFuncWithResolvers);
static JSC_DECLARE_HOST_FUNCTION(promiseConstructorFuncAny);
static JSC_DECLARE_HOST_FUNCTION(promiseConstructorFuncRace);
static JSC_DECLARE_HOST_FUNCTION(promiseConstructorFuncAll);
static JSC_DECLARE_HOST_FUNCTION(promiseConstructorFuncAllSettled);

}

#include "JSPromiseConstructor.lut.h"

namespace JSC {

const ClassInfo JSPromiseConstructor::s_info = { "Function"_s, &Base::s_info, &promiseConstructorTable, nullptr, CREATE_METHOD_TABLE(JSPromiseConstructor) };

/* Source for JSPromiseConstructor.lut.h
@begin promiseConstructorTable
  resolve         promiseConstructorFuncResolve        DontEnum|Function 1 PromiseConstructorResolveIntrinsic
  reject          promiseConstructorFuncReject         DontEnum|Function 1 PromiseConstructorRejectIntrinsic
  race            promiseConstructorFuncRace           DontEnum|Function 1
  all             promiseConstructorFuncAll            DontEnum|Function 1
  allSettled      promiseConstructorFuncAllSettled     DontEnum|Function 1
  any             promiseConstructorFuncAny            DontEnum|Function 1
  withResolvers   promiseConstructorFuncWithResolvers  DontEnum|Function 0
@end
*/

JSPromiseConstructor* JSPromiseConstructor::create(VM& vm, Structure* structure, JSPromisePrototype* promisePrototype)
{
    JSGlobalObject* globalObject = structure->realm();
    FunctionExecutable* executable = promiseConstructorPromiseConstructorCodeGenerator(vm);
    JSPromiseConstructor* constructor = new (NotNull, allocateCell<JSPromiseConstructor>(vm)) JSPromiseConstructor(vm, executable, globalObject, structure);
    constructor->finishCreation(vm, promisePrototype);
    return constructor;
}

Structure* JSPromiseConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(JSFunctionType, StructureFlags), info());
}

JSPromiseConstructor::JSPromiseConstructor(VM& vm, FunctionExecutable* executable, JSGlobalObject* globalObject, Structure* structure)
    : Base(vm, executable, globalObject, structure)
{
}

void JSPromiseConstructor::finishCreation(VM& vm, JSPromisePrototype* promisePrototype)
{
    Base::finishCreation(vm);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, promisePrototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(1), PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);

    JSGlobalObject* globalObject = this->realm();

    putDirectNonIndexAccessorWithoutTransition(vm, vm.propertyNames->speciesSymbol, globalObject->promiseSpeciesGetterSetter(), PropertyAttribute::Accessor | PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->tryKeyword, promiseConstructorTryCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
}

JSC_DEFINE_HOST_FUNCTION(promiseConstructorFuncResolve, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue().toThis(globalObject, ECMAMode::strict());
    JSValue argument = callFrame->argument(0);

    if (!thisValue.isObject()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "|this| is not an object"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(JSPromise::promiseResolve(globalObject, asObject(thisValue), argument)));
}

JSC_DEFINE_HOST_FUNCTION(promiseConstructorFuncReject, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue().toThis(globalObject, ECMAMode::strict());
    JSValue argument = callFrame->argument(0);

    if (!thisValue.isObject()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "|this| is not an object"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(JSPromise::promiseReject(globalObject, asObject(thisValue), argument)));
}

JSC_DEFINE_HOST_FUNCTION(promiseConstructorFuncWithResolvers, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    JSValue thisValue = callFrame->thisValue().toThis(globalObject, ECMAMode::strict());
    return JSValue::encode(JSPromise::createNewPromiseCapability(globalObject, thisValue));
}

static bool isFastPromiseConstructor(JSGlobalObject* globalObject, JSValue value)
{
    if (value != globalObject->promiseConstructor()) [[unlikely]]
        return false;

    if (!globalObject->promiseResolveWatchpointSet().isStillValid()) [[unlikely]]
        return false;

    return true;
}

static JSObject* promiseRaceSlow(JSGlobalObject* globalObject, CallFrame* callFrame, JSValue thisValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto [promise, resolve, reject] = JSPromise::newPromiseCapability(globalObject, thisValue);
    RETURN_IF_EXCEPTION(scope, { });

    auto callReject = [&](JSValue exception) -> void {
        MarkedArgumentBuffer rejectArguments;
        rejectArguments.append(exception);
        ASSERT(!rejectArguments.hasOverflowed());
        auto rejectCallData = getCallDataInline(reject);
        scope.release();
        call(globalObject, reject, rejectCallData, jsUndefined(), rejectArguments);
    };
    auto callRejectWithScopeException = [&]() -> void {
        Exception* exception = scope.exception();
        ASSERT(exception);
        TRY_CLEAR_EXCEPTION(scope, void());
        callReject(exception->value());
    };

    JSValue promiseResolveValue = thisValue.get(globalObject, vm.propertyNames->resolve);
    if (scope.exception()) [[unlikely]] {
        callRejectWithScopeException();
        return promise;
    }

    if (!promiseResolveValue.isCallable()) [[unlikely]] {
        callReject(createTypeError(globalObject, "Promise resolve is not a function"_s));
        return promise;
    }
    CallData promiseResolveCallData = getCallDataInline(promiseResolveValue);
    ASSERT(promiseResolveCallData.type != CallData::Type::None);

    std::optional<CachedCall> cachedCallHolder;
    CachedCall* cachedCall = nullptr;
    if (promiseResolveCallData.type == CallData::Type::JS) [[likely]] {
        cachedCallHolder.emplace(globalObject, jsCast<JSFunction*>(promiseResolveValue), 1);
        if (scope.exception()) [[unlikely]] {
            callRejectWithScopeException();
            return promise;
        }
        cachedCall = &cachedCallHolder.value();
    }

    JSValue iterable = callFrame->argument(0);
    forEachInIterable(globalObject, iterable, [&](VM& vm, JSGlobalObject* globalObject, JSValue value) {
        auto scope = DECLARE_THROW_SCOPE(vm);

        JSValue nextPromise;
        if (cachedCall) [[likely]] {
            nextPromise = cachedCall->callWithArguments(globalObject, thisValue, value);
            RETURN_IF_EXCEPTION(scope, void());
        } else {
            MarkedArgumentBuffer arguments;
            arguments.append(value);
            ASSERT(!arguments.hasOverflowed());
            nextPromise = call(globalObject, promiseResolveValue, promiseResolveCallData, thisValue, arguments);
            RETURN_IF_EXCEPTION(scope, void());
        }
        ASSERT(nextPromise);

        JSValue then = nextPromise.get(globalObject, vm.propertyNames->then);
        RETURN_IF_EXCEPTION(scope, void());
        CallData thenCallData = getCallDataInline(then);
        if (thenCallData.type == CallData::Type::None) [[unlikely]] {
            throwTypeError(globalObject, scope, "then is not a function"_s);
            return;
        }
        MarkedArgumentBuffer thenArguments;
        thenArguments.append(resolve);
        thenArguments.append(reject);
        ASSERT(!thenArguments.hasOverflowed());
        scope.release();
        call(globalObject, then, thenCallData, nextPromise, thenArguments);
    });

    if (scope.exception()) [[unlikely]]
        callRejectWithScopeException();

    return promise;
}
JSC_DEFINE_HOST_FUNCTION(promiseConstructorFuncRace, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue().toThis(globalObject, ECMAMode::strict());

    if (!thisValue.isObject()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "|this| is not an object"_s);

    if (!isFastPromiseConstructor(globalObject, thisValue)) [[unlikely]]
        RELEASE_AND_RETURN(scope, JSValue::encode(promiseRaceSlow(globalObject, callFrame, thisValue)));

    auto* promise = JSPromise::create(vm, globalObject->promiseStructure());

    auto callReject = [&]() -> void {
        Exception* exception = scope.exception();
        ASSERT(exception);
        TRY_CLEAR_EXCEPTION(scope, void());
        scope.release();
        promise->reject(vm, globalObject, exception);
    };

    JSValue iterable = callFrame->argument(0);
    JSFunction* resolve = nullptr;
    JSFunction* reject = nullptr;
    forEachInIterable(globalObject, iterable, [&](VM& vm, JSGlobalObject* globalObject, JSValue value) {
        auto scope = DECLARE_THROW_SCOPE(vm);

        JSPromise* nextPromise = JSPromise::resolvedPromise(globalObject, value);
        RETURN_IF_EXCEPTION(scope, void());

        if (nextPromise->isThenFastAndNonObservable()) [[likely]] {
            auto* constructor = promiseSpeciesConstructor(globalObject, nextPromise);
            RETURN_IF_EXCEPTION(scope, void());
            if (constructor == globalObject->promiseConstructor()) [[likely]] {
                scope.release();
                nextPromise->performPromiseThenWithInternalMicrotask(vm, globalObject, InternalMicrotask::PromiseRaceResolveJob, promise, promise);
                return;
            }
        }

        if (!resolve || !reject)
            std::tie(resolve, reject) = promise->createFirstResolvingFunctions(vm, globalObject);
        JSValue then = nextPromise->get(globalObject, vm.propertyNames->then);
        RETURN_IF_EXCEPTION(scope, void());
        CallData thenCallData = getCallDataInline(then);
        if (thenCallData.type == CallData::Type::None) [[unlikely]] {
            throwTypeError(globalObject, scope, "then is not a function"_s);
            return;
        }
        MarkedArgumentBuffer thenArguments;
        thenArguments.append(resolve);
        thenArguments.append(reject);
        ASSERT(!thenArguments.hasOverflowed());
        scope.release();
        call(globalObject, then, thenCallData, nextPromise, thenArguments);
    });

    if (scope.exception()) [[unlikely]]
        callReject();

    return JSValue::encode(promise);
}

static JSObject* promiseAllSlow(JSGlobalObject* globalObject, CallFrame* callFrame, JSValue thisValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto [promise, resolve, reject] = JSPromise::newPromiseCapability(globalObject, thisValue);
    RETURN_IF_EXCEPTION(scope, { });

    auto callReject = [&](JSValue exception) -> void {
        MarkedArgumentBuffer rejectArguments;
        rejectArguments.append(exception);
        ASSERT(!rejectArguments.hasOverflowed());
        auto rejectCallData = getCallDataInline(reject);
        scope.release();
        call(globalObject, reject, rejectCallData, jsUndefined(), rejectArguments);
    };
    auto callRejectWithScopeException = [&]() -> void {
        Exception* exception = scope.exception();
        ASSERT(exception);
        TRY_CLEAR_EXCEPTION(scope, void());
        callReject(exception->value());
    };

    JSValue promiseResolveValue = thisValue.get(globalObject, vm.propertyNames->resolve);
    if (scope.exception()) [[unlikely]] {
        callRejectWithScopeException();
        return promise;
    }

    if (!promiseResolveValue.isCallable()) [[unlikely]] {
        callReject(createTypeError(globalObject, "Promise resolve is not a function"_s));
        return promise;
    }
    CallData promiseResolveCallData = getCallDataInline(promiseResolveValue);
    ASSERT(promiseResolveCallData.type != CallData::Type::None);

    std::optional<CachedCall> cachedCallHolder;
    CachedCall* cachedCall = nullptr;
    if (promiseResolveCallData.type == CallData::Type::JS) [[likely]] {
        cachedCallHolder.emplace(globalObject, jsCast<JSFunction*>(promiseResolveValue), 1);
        if (scope.exception()) [[unlikely]] {
            callRejectWithScopeException();
            return promise;
        }
        cachedCall = &cachedCallHolder.value();
    }

    JSArray* values = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithUndecided), 0);
    if (!values) [[unlikely]] {
        callReject(createOutOfMemoryError(globalObject));
        return promise;
    }

    JSPromiseCombinatorsGlobalContext* globalContext = JSPromiseCombinatorsGlobalContext::create(vm, promise, values, jsNumber(1));

    uint64_t index = 0;

    JSValue iterable = callFrame->argument(0);
    forEachInIterable(globalObject, iterable, [&](VM& vm, JSGlobalObject* globalObject, JSValue value) {
        auto scope = DECLARE_THROW_SCOPE(vm);

        values->putDirectIndex(globalObject, index, jsUndefined());
        RETURN_IF_EXCEPTION(scope, void());

        JSValue nextPromise;
        if (cachedCall) [[likely]] {
            nextPromise = cachedCall->callWithArguments(globalObject, thisValue, value);
            RETURN_IF_EXCEPTION(scope, void());
        } else {
            MarkedArgumentBuffer arguments;
            arguments.append(value);
            ASSERT(!arguments.hasOverflowed());
            nextPromise = call(globalObject, promiseResolveValue, promiseResolveCallData, thisValue, arguments);
            RETURN_IF_EXCEPTION(scope, void());
        }
        ASSERT(nextPromise);

        uint64_t count = globalContext->remainingElementsCount().toIndex(globalObject, "count exceeds size"_s);
        RETURN_IF_EXCEPTION(scope, void());
        globalContext->setRemainingElementsCount(vm, jsNumber(count + 1));

        uint64_t currentIndex = index++;

        JSPromiseCombinatorsContext* context = JSPromiseCombinatorsContext::create(vm, globalContext, currentIndex);

        auto* onFulfilled = JSFunctionWithFields::create(vm, globalObject, vm.promiseAllSlowFulfillFunctionExecutable(), 1, emptyString());
        onFulfilled->setField(vm, JSFunctionWithFields::Field::PromiseAllContext, context);
        onFulfilled->setField(vm, JSFunctionWithFields::Field::PromiseAllResolve, resolve);

        JSValue then = nextPromise.get(globalObject, vm.propertyNames->then);
        RETURN_IF_EXCEPTION(scope, void());
        CallData thenCallData = getCallDataInline(then);
        if (thenCallData.type == CallData::Type::None) [[unlikely]] {
            throwTypeError(globalObject, scope, "then is not a function"_s);
            return;
        }

        MarkedArgumentBuffer thenArguments;
        thenArguments.append(onFulfilled);
        thenArguments.append(reject);
        ASSERT(!thenArguments.hasOverflowed());
        scope.release();
        call(globalObject, then, thenCallData, nextPromise, thenArguments);
    });

    if (scope.exception()) [[unlikely]] {
        callRejectWithScopeException();
        return promise;
    }

    uint64_t count = globalContext->remainingElementsCount().toIndex(globalObject, "count exceeds size"_s);
    if (scope.exception()) [[unlikely]] {
        callRejectWithScopeException();
        return promise;
    }

    --count;
    globalContext->setRemainingElementsCount(vm, jsNumber(count));
    if (!count) {
        MarkedArgumentBuffer resolveArguments;
        resolveArguments.append(values);
        ASSERT(!resolveArguments.hasOverflowed());
        auto resolveCallData = getCallDataInline(resolve);
        scope.release();
        call(globalObject, resolve, resolveCallData, jsUndefined(), resolveArguments);
        if (scope.exception()) [[unlikely]] {
            callRejectWithScopeException();
            return promise;
        }
    }

    return promise;
}

JSC_DEFINE_HOST_FUNCTION(promiseConstructorFuncAll, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue().toThis(globalObject, ECMAMode::strict());

    if (!thisValue.isObject()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "|this| is not an object"_s);

    if (!isFastPromiseConstructor(globalObject, thisValue)) [[unlikely]]
        RELEASE_AND_RETURN(scope, JSValue::encode(promiseAllSlow(globalObject, callFrame, thisValue)));

    auto* promise = JSPromise::create(vm, globalObject->promiseStructure());

    auto callReject = [&]() -> void {
        Exception* exception = scope.exception();
        ASSERT(exception);
        TRY_CLEAR_EXCEPTION(scope, void());
        scope.release();
        promise->reject(vm, globalObject, exception);
    };

    JSArray* values = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithUndecided), 0);
    if (!values) [[unlikely]] {
        throwOutOfMemoryError(globalObject, scope);
        callReject();
        return JSValue::encode(promise);
    }

    JSPromiseCombinatorsGlobalContext* globalContext = JSPromiseCombinatorsGlobalContext::create(vm, promise, values, jsNumber(1));

    uint64_t index = 0;
    JSFunction* onRejected = nullptr;

    JSValue iterable = callFrame->argument(0);
    forEachInIterable(globalObject, iterable, [&](VM& vm, JSGlobalObject* globalObject, JSValue value) {
        auto scope = DECLARE_THROW_SCOPE(vm);

        values->putDirectIndex(globalObject, index, jsUndefined());
        RETURN_IF_EXCEPTION(scope, void());

        JSPromise* nextPromise = JSPromise::resolvedPromise(globalObject, value);
        RETURN_IF_EXCEPTION(scope, void());

        uint64_t count = globalContext->remainingElementsCount().toIndex(globalObject, "count exceeds size"_s);
        RETURN_IF_EXCEPTION(scope, void());
        globalContext->setRemainingElementsCount(vm, jsNumber(count + 1));

        JSPromiseCombinatorsContext* context = JSPromiseCombinatorsContext::create(vm, globalContext, index);

        if (nextPromise->isThenFastAndNonObservable()) [[likely]] {
            auto* constructor = promiseSpeciesConstructor(globalObject, nextPromise);
            RETURN_IF_EXCEPTION(scope, void());
            if (constructor == globalObject->promiseConstructor()) [[likely]] {
                scope.release();
                nextPromise->performPromiseThenWithInternalMicrotask(vm, globalObject, InternalMicrotask::PromiseAllResolveJob, promise, context);
                ++index;
                return;
            }
        }

        if (!onRejected) {
            auto [resolve, reject] = promise->createFirstResolvingFunctions(vm, globalObject);
            onRejected = reject;
        }
        JSValue then = nextPromise->get(globalObject, vm.propertyNames->then);
        RETURN_IF_EXCEPTION(scope, void());
        CallData thenCallData = getCallDataInline(then);
        if (thenCallData.type == CallData::Type::None) [[unlikely]] {
            throwTypeError(globalObject, scope, "then is not a function"_s);
            return;
        }

        auto* onFulfilled = JSFunctionWithFields::create(vm, globalObject, vm.promiseAllFulfillFunctionExecutable(), 1, emptyString());
        onFulfilled->setField(vm, JSFunctionWithFields::Field::PromiseAllContext, context);

        MarkedArgumentBuffer thenArguments;
        thenArguments.append(onFulfilled);
        thenArguments.append(onRejected);
        ASSERT(!thenArguments.hasOverflowed());
        scope.release();
        call(globalObject, then, thenCallData, nextPromise, thenArguments);
        ++index;
    });

    if (scope.exception()) [[unlikely]] {
        callReject();
        return JSValue::encode(promise);
    }

    uint64_t count = globalContext->remainingElementsCount().toIndex(globalObject, "count exceeds size"_s);
    if (scope.exception()) [[unlikely]] {
        callReject();
        return JSValue::encode(promise);
    }

    --count;
    globalContext->setRemainingElementsCount(vm, jsNumber(count));
    if (!count) {
        scope.release();
        promise->resolve(globalObject, vm, values);
        if (scope.exception()) [[unlikely]] {
            callReject();
            return JSValue::encode(promise);
        }
    }

    return JSValue::encode(promise);
}

JSC_DEFINE_HOST_FUNCTION(promiseAllFulfillFunction, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* callee = jsCast<JSFunctionWithFields*>(callFrame->jsCallee());
    auto* context = jsDynamicCast<JSPromiseCombinatorsContext*>(callee->getField(JSFunctionWithFields::Field::PromiseAllContext));
    if (!context) [[unlikely]]
        return JSValue::encode(jsUndefined());

    callee->setField(vm, JSFunctionWithFields::Field::PromiseAllContext, jsNull());

    auto* globalContext = jsCast<JSPromiseCombinatorsGlobalContext*>(context->globalContext());
    auto* promise = jsCast<JSPromise*>(globalContext->promise());
    auto* values = jsCast<JSArray*>(globalContext->values());

    JSValue value = callFrame->argument(0);
    uint64_t index = context->index();

    values->putDirectIndex(globalObject, index, value);
    RETURN_IF_EXCEPTION(scope, { });

    uint64_t count = globalContext->remainingElementsCount().toIndex(globalObject, "count exceeds size"_s);
    RETURN_IF_EXCEPTION(scope, { });

    --count;
    globalContext->setRemainingElementsCount(vm, jsNumber(count));
    if (!count) {
        scope.release();
        promise->resolve(globalObject, vm, values);
    }

    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(promiseAllSlowFulfillFunction, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* callee = jsCast<JSFunctionWithFields*>(callFrame->jsCallee());
    auto* context = jsDynamicCast<JSPromiseCombinatorsContext*>(callee->getField(JSFunctionWithFields::Field::PromiseAllContext));
    if (!context) [[unlikely]]
        return JSValue::encode(jsUndefined());

    JSValue resolve = callee->getField(JSFunctionWithFields::Field::PromiseAllResolve);

    callee->setField(vm, JSFunctionWithFields::Field::PromiseAllContext, jsNull());
    callee->setField(vm, JSFunctionWithFields::Field::PromiseAllResolve, jsNull());

    auto* globalContext = jsCast<JSPromiseCombinatorsGlobalContext*>(context->globalContext());
    auto* values = jsCast<JSArray*>(globalContext->values());

    JSValue value = callFrame->argument(0);
    uint64_t index = context->index();

    values->putDirectIndex(globalObject, index, value);
    RETURN_IF_EXCEPTION(scope, { });

    uint64_t count = globalContext->remainingElementsCount().toIndex(globalObject, "count exceeds size"_s);
    RETURN_IF_EXCEPTION(scope, { });

    --count;
    globalContext->setRemainingElementsCount(vm, jsNumber(count));
    if (!count) {
        MarkedArgumentBuffer resolveArguments;
        resolveArguments.append(values);
        ASSERT(!resolveArguments.hasOverflowed());
        auto resolveCallData = getCallDataInline(resolve);
        scope.release();
        call(globalObject, resolve, resolveCallData, jsUndefined(), resolveArguments);
    }

    return JSValue::encode(jsUndefined());
}

static JSObject* promiseAllSettledSlow(JSGlobalObject* globalObject, CallFrame* callFrame, JSValue thisValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto [promise, resolve, reject] = JSPromise::newPromiseCapability(globalObject, thisValue);
    RETURN_IF_EXCEPTION(scope, { });

    auto callReject = [&](JSValue exception) -> void {
        MarkedArgumentBuffer rejectArguments;
        rejectArguments.append(exception);
        ASSERT(!rejectArguments.hasOverflowed());
        auto rejectCallData = getCallDataInline(reject);
        scope.release();
        call(globalObject, reject, rejectCallData, jsUndefined(), rejectArguments);
    };
    auto callRejectWithScopeException = [&]() -> void {
        Exception* exception = scope.exception();
        ASSERT(exception);
        TRY_CLEAR_EXCEPTION(scope, void());
        callReject(exception->value());
    };

    JSValue promiseResolveValue = thisValue.get(globalObject, vm.propertyNames->resolve);
    if (scope.exception()) [[unlikely]] {
        callRejectWithScopeException();
        return promise;
    }

    if (!promiseResolveValue.isCallable()) [[unlikely]] {
        callReject(createTypeError(globalObject, "Promise resolve is not a function"_s));
        return promise;
    }
    CallData promiseResolveCallData = getCallDataInline(promiseResolveValue);
    ASSERT(promiseResolveCallData.type != CallData::Type::None);

    std::optional<CachedCall> cachedCallHolder;
    CachedCall* cachedCall = nullptr;
    if (promiseResolveCallData.type == CallData::Type::JS) [[likely]] {
        cachedCallHolder.emplace(globalObject, jsCast<JSFunction*>(promiseResolveValue), 1);
        if (scope.exception()) [[unlikely]] {
            callRejectWithScopeException();
            return promise;
        }
        cachedCall = &cachedCallHolder.value();
    }

    JSArray* values = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithUndecided), 0);
    if (!values) [[unlikely]] {
        callReject(createOutOfMemoryError(globalObject));
        return promise;
    }

    JSPromiseCombinatorsGlobalContext* globalContext = JSPromiseCombinatorsGlobalContext::create(vm, resolve, values, jsNumber(1));

    uint64_t index = 0;

    JSValue iterable = callFrame->argument(0);
    forEachInIterable(globalObject, iterable, [&](VM& vm, JSGlobalObject* globalObject, JSValue value) {
        auto scope = DECLARE_THROW_SCOPE(vm);

        values->putDirectIndex(globalObject, index, jsUndefined());
        RETURN_IF_EXCEPTION(scope, void());

        JSValue nextPromise;
        if (cachedCall) [[likely]] {
            nextPromise = cachedCall->callWithArguments(globalObject, thisValue, value);
            RETURN_IF_EXCEPTION(scope, void());
        } else {
            MarkedArgumentBuffer arguments;
            arguments.append(value);
            ASSERT(!arguments.hasOverflowed());
            nextPromise = call(globalObject, promiseResolveValue, promiseResolveCallData, thisValue, arguments);
            RETURN_IF_EXCEPTION(scope, void());
        }
        ASSERT(nextPromise);

        uint64_t count = globalContext->remainingElementsCount().toIndex(globalObject, "count exceeds size"_s);
        RETURN_IF_EXCEPTION(scope, void());
        globalContext->setRemainingElementsCount(vm, jsNumber(count + 1));

        uint64_t currentIndex = index++;

        JSPromiseCombinatorsContext* context = JSPromiseCombinatorsContext::create(vm, globalContext, currentIndex);

        auto* onFulfilled = JSFunctionWithFields::create(vm, globalObject, vm.promiseAllSettledSlowFulfillFunctionExecutable(), 1, emptyString());
        onFulfilled->setField(vm, JSFunctionWithFields::Field::PromiseAllSettledContext, context);

        auto* onRejected = JSFunctionWithFields::create(vm, globalObject, vm.promiseAllSettledSlowRejectFunctionExecutable(), 1, emptyString());
        onRejected->setField(vm, JSFunctionWithFields::Field::PromiseAllSettledContext, context);

        onFulfilled->setField(vm, JSFunctionWithFields::Field::PromiseAllSettledOther, onRejected);
        onRejected->setField(vm, JSFunctionWithFields::Field::PromiseAllSettledOther, onFulfilled);

        JSValue then = nextPromise.get(globalObject, vm.propertyNames->then);
        RETURN_IF_EXCEPTION(scope, void());
        CallData thenCallData = getCallDataInline(then);
        if (thenCallData.type == CallData::Type::None) [[unlikely]] {
            throwTypeError(globalObject, scope, "then is not a function"_s);
            return;
        }

        MarkedArgumentBuffer thenArguments;
        thenArguments.append(onFulfilled);
        thenArguments.append(onRejected);
        ASSERT(!thenArguments.hasOverflowed());
        scope.release();
        call(globalObject, then, thenCallData, nextPromise, thenArguments);
    });

    if (scope.exception()) [[unlikely]] {
        callRejectWithScopeException();
        return promise;
    }

    uint64_t count = globalContext->remainingElementsCount().toIndex(globalObject, "count exceeds size"_s);
    if (scope.exception()) [[unlikely]] {
        callRejectWithScopeException();
        return promise;
    }

    --count;
    globalContext->setRemainingElementsCount(vm, jsNumber(count));
    if (!count) {
        MarkedArgumentBuffer resolveArguments;
        resolveArguments.append(values);
        ASSERT(!resolveArguments.hasOverflowed());
        auto resolveCallData = getCallDataInline(resolve);
        scope.release();
        call(globalObject, resolve, resolveCallData, jsUndefined(), resolveArguments);
        if (scope.exception()) [[unlikely]] {
            callRejectWithScopeException();
            return promise;
        }
    }

    return promise;
}

JSC_DEFINE_HOST_FUNCTION(promiseConstructorFuncAllSettled, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue().toThis(globalObject, ECMAMode::strict());

    if (!thisValue.isObject()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "|this| is not an object"_s);

    if (!isFastPromiseConstructor(globalObject, thisValue)) [[unlikely]]
        RELEASE_AND_RETURN(scope, JSValue::encode(promiseAllSettledSlow(globalObject, callFrame, thisValue)));

    auto* promise = JSPromise::create(vm, globalObject->promiseStructure());

    auto callReject = [&]() -> void {
        Exception* exception = scope.exception();
        ASSERT(exception);
        TRY_CLEAR_EXCEPTION(scope, void());
        scope.release();
        promise->reject(vm, globalObject, exception);
    };

    JSArray* values = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithUndecided), 0);
    if (!values) [[unlikely]] {
        throwOutOfMemoryError(globalObject, scope);
        callReject();
        return JSValue::encode(promise);
    }

    JSPromiseCombinatorsGlobalContext* globalContext = JSPromiseCombinatorsGlobalContext::create(vm, promise, values, jsNumber(1));

    uint64_t index = 0;

    JSValue iterable = callFrame->argument(0);
    forEachInIterable(globalObject, iterable, [&](VM& vm, JSGlobalObject* globalObject, JSValue value) {
        auto scope = DECLARE_THROW_SCOPE(vm);

        values->putDirectIndex(globalObject, index, jsUndefined());
        RETURN_IF_EXCEPTION(scope, void());

        JSPromise* nextPromise = JSPromise::resolvedPromise(globalObject, value);
        RETURN_IF_EXCEPTION(scope, void());

        uint64_t count = globalContext->remainingElementsCount().toIndex(globalObject, "count exceeds size"_s);
        RETURN_IF_EXCEPTION(scope, void());
        globalContext->setRemainingElementsCount(vm, jsNumber(count + 1));

        JSPromiseCombinatorsContext* context = JSPromiseCombinatorsContext::create(vm, globalContext, index);

        if (nextPromise->isThenFastAndNonObservable()) [[likely]] {
            auto* constructor = promiseSpeciesConstructor(globalObject, nextPromise);
            RETURN_IF_EXCEPTION(scope, void());
            if (constructor == globalObject->promiseConstructor()) [[likely]] {
                scope.release();
                nextPromise->performPromiseThenWithInternalMicrotask(vm, globalObject, InternalMicrotask::PromiseAllSettledResolveJob, promise, context);
                ++index;
                return;
            }
        }

        auto* onFulfilled = JSFunctionWithFields::create(vm, globalObject, vm.promiseAllSettledFulfillFunctionExecutable(), 1, emptyString());
        onFulfilled->setField(vm, JSFunctionWithFields::Field::PromiseAllSettledContext, context);

        auto* onRejected = JSFunctionWithFields::create(vm, globalObject, vm.promiseAllSettledRejectFunctionExecutable(), 1, emptyString());
        onRejected->setField(vm, JSFunctionWithFields::Field::PromiseAllSettledContext, context);

        onFulfilled->setField(vm, JSFunctionWithFields::Field::PromiseAllSettledOther, onRejected);
        onRejected->setField(vm, JSFunctionWithFields::Field::PromiseAllSettledOther, onFulfilled);

        JSValue then = nextPromise->get(globalObject, vm.propertyNames->then);
        RETURN_IF_EXCEPTION(scope, void());
        CallData thenCallData = getCallDataInline(then);
        if (thenCallData.type == CallData::Type::None) [[unlikely]] {
            throwTypeError(globalObject, scope, "then is not a function"_s);
            return;
        }

        MarkedArgumentBuffer thenArguments;
        thenArguments.append(onFulfilled);
        thenArguments.append(onRejected);
        ASSERT(!thenArguments.hasOverflowed());
        scope.release();
        call(globalObject, then, thenCallData, nextPromise, thenArguments);
        ++index;
    });

    if (scope.exception()) [[unlikely]] {
        callReject();
        return JSValue::encode(promise);
    }

    uint64_t count = globalContext->remainingElementsCount().toIndex(globalObject, "count exceeds size"_s);
    if (scope.exception()) [[unlikely]] {
        callReject();
        return JSValue::encode(promise);
    }

    --count;
    globalContext->setRemainingElementsCount(vm, jsNumber(count));
    if (!count) {
        scope.release();
        promise->resolve(globalObject, vm, values);
        if (scope.exception()) [[unlikely]] {
            callReject();
            return JSValue::encode(promise);
        }
    }

    return JSValue::encode(promise);
}

JSC_DEFINE_HOST_FUNCTION(promiseAllSettledFulfillFunction, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* callee = jsCast<JSFunctionWithFields*>(callFrame->jsCallee());
    auto* context = jsDynamicCast<JSPromiseCombinatorsContext*>(callee->getField(JSFunctionWithFields::Field::PromiseAllSettledContext));
    if (!context) [[unlikely]]
        return JSValue::encode(jsUndefined());

    auto* other = jsDynamicCast<JSFunctionWithFields*>(callee->getField(JSFunctionWithFields::Field::PromiseAllSettledOther));
    if (!other) [[unlikely]]
        return JSValue::encode(jsUndefined());

    callee->setField(vm, JSFunctionWithFields::Field::PromiseAllSettledContext, jsNull());
    callee->setField(vm, JSFunctionWithFields::Field::PromiseAllSettledOther, jsNull());
    other->setField(vm, JSFunctionWithFields::Field::PromiseAllSettledContext, jsNull());
    other->setField(vm, JSFunctionWithFields::Field::PromiseAllSettledOther, jsNull());

    auto* globalContext = jsCast<JSPromiseCombinatorsGlobalContext*>(context->globalContext());
    auto* promise = jsCast<JSPromise*>(globalContext->promise());
    auto* values = jsCast<JSArray*>(globalContext->values());

    JSValue value = callFrame->argument(0);
    uint64_t index = context->index();

    JSObject* resultObject = createPromiseAllSettledFulfilledResult(globalObject, value);

    values->putDirectIndex(globalObject, index, resultObject);
    RETURN_IF_EXCEPTION(scope, { });

    uint64_t count = globalContext->remainingElementsCount().toIndex(globalObject, "count exceeds size"_s);
    RETURN_IF_EXCEPTION(scope, { });

    --count;
    globalContext->setRemainingElementsCount(vm, jsNumber(count));
    if (!count) {
        scope.release();
        promise->resolve(globalObject, vm, values);
    }

    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(promiseAllSettledRejectFunction, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* callee = jsCast<JSFunctionWithFields*>(callFrame->jsCallee());
    auto* context = jsDynamicCast<JSPromiseCombinatorsContext*>(callee->getField(JSFunctionWithFields::Field::PromiseAllSettledContext));
    if (!context) [[unlikely]]
        return JSValue::encode(jsUndefined());

    auto* other = jsDynamicCast<JSFunctionWithFields*>(callee->getField(JSFunctionWithFields::Field::PromiseAllSettledOther));
    if (!other) [[unlikely]]
        return JSValue::encode(jsUndefined());

    callee->setField(vm, JSFunctionWithFields::Field::PromiseAllSettledContext, jsNull());
    callee->setField(vm, JSFunctionWithFields::Field::PromiseAllSettledOther, jsNull());
    other->setField(vm, JSFunctionWithFields::Field::PromiseAllSettledContext, jsNull());
    other->setField(vm, JSFunctionWithFields::Field::PromiseAllSettledOther, jsNull());

    auto* globalContext = jsCast<JSPromiseCombinatorsGlobalContext*>(context->globalContext());
    auto* promise = jsCast<JSPromise*>(globalContext->promise());
    auto* values = jsCast<JSArray*>(globalContext->values());

    JSValue reason = callFrame->argument(0);
    uint64_t index = context->index();

    JSObject* resultObject = createPromiseAllSettledRejectedResult(globalObject, reason);

    values->putDirectIndex(globalObject, index, resultObject);
    RETURN_IF_EXCEPTION(scope, { });

    uint64_t count = globalContext->remainingElementsCount().toIndex(globalObject, "count exceeds size"_s);
    RETURN_IF_EXCEPTION(scope, { });

    --count;
    globalContext->setRemainingElementsCount(vm, jsNumber(count));
    if (!count) {
        scope.release();
        promise->resolve(globalObject, vm, values);
    }

    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(promiseAllSettledSlowFulfillFunction, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* callee = jsCast<JSFunctionWithFields*>(callFrame->jsCallee());
    auto* context = jsDynamicCast<JSPromiseCombinatorsContext*>(callee->getField(JSFunctionWithFields::Field::PromiseAllSettledContext));
    if (!context) [[unlikely]]
        return JSValue::encode(jsUndefined());

    auto* other = jsDynamicCast<JSFunctionWithFields*>(callee->getField(JSFunctionWithFields::Field::PromiseAllSettledOther));
    if (!other) [[unlikely]]
        return JSValue::encode(jsUndefined());

    callee->setField(vm, JSFunctionWithFields::Field::PromiseAllSettledContext, jsNull());
    callee->setField(vm, JSFunctionWithFields::Field::PromiseAllSettledOther, jsNull());
    other->setField(vm, JSFunctionWithFields::Field::PromiseAllSettledContext, jsNull());
    other->setField(vm, JSFunctionWithFields::Field::PromiseAllSettledOther, jsNull());

    auto* globalContext = jsCast<JSPromiseCombinatorsGlobalContext*>(context->globalContext());
    auto* values = jsCast<JSArray*>(globalContext->values());
    JSValue resolve = globalContext->promise();

    JSValue value = callFrame->argument(0);
    uint64_t index = context->index();

    JSObject* resultObject = createPromiseAllSettledFulfilledResult(globalObject, value);

    values->putDirectIndex(globalObject, index, resultObject);
    RETURN_IF_EXCEPTION(scope, { });

    uint64_t count = globalContext->remainingElementsCount().toIndex(globalObject, "count exceeds size"_s);
    RETURN_IF_EXCEPTION(scope, { });

    --count;
    globalContext->setRemainingElementsCount(vm, jsNumber(count));
    if (!count) {
        MarkedArgumentBuffer resolveArguments;
        resolveArguments.append(values);
        ASSERT(!resolveArguments.hasOverflowed());
        auto resolveCallData = getCallDataInline(resolve);
        scope.release();
        call(globalObject, resolve, resolveCallData, jsUndefined(), resolveArguments);
    }

    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(promiseAllSettledSlowRejectFunction, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* callee = jsCast<JSFunctionWithFields*>(callFrame->jsCallee());
    auto* context = jsDynamicCast<JSPromiseCombinatorsContext*>(callee->getField(JSFunctionWithFields::Field::PromiseAllSettledContext));
    if (!context) [[unlikely]]
        return JSValue::encode(jsUndefined());

    auto* other = jsDynamicCast<JSFunctionWithFields*>(callee->getField(JSFunctionWithFields::Field::PromiseAllSettledOther));
    if (!other) [[unlikely]]
        return JSValue::encode(jsUndefined());

    callee->setField(vm, JSFunctionWithFields::Field::PromiseAllSettledContext, jsNull());
    callee->setField(vm, JSFunctionWithFields::Field::PromiseAllSettledOther, jsNull());
    other->setField(vm, JSFunctionWithFields::Field::PromiseAllSettledContext, jsNull());
    other->setField(vm, JSFunctionWithFields::Field::PromiseAllSettledOther, jsNull());

    auto* globalContext = jsCast<JSPromiseCombinatorsGlobalContext*>(context->globalContext());
    auto* values = jsCast<JSArray*>(globalContext->values());
    JSValue resolve = globalContext->promise();

    JSValue reason = callFrame->argument(0);
    uint64_t index = context->index();

    JSObject* resultObject = createPromiseAllSettledRejectedResult(globalObject, reason);
    RETURN_IF_EXCEPTION(scope, { });

    values->putDirectIndex(globalObject, index, resultObject);
    RETURN_IF_EXCEPTION(scope, { });

    uint64_t count = globalContext->remainingElementsCount().toIndex(globalObject, "count exceeds size"_s);
    RETURN_IF_EXCEPTION(scope, { });

    --count;
    globalContext->setRemainingElementsCount(vm, jsNumber(count));
    if (!count) {
        MarkedArgumentBuffer resolveArguments;
        resolveArguments.append(values);
        ASSERT(!resolveArguments.hasOverflowed());
        auto resolveCallData = getCallDataInline(resolve);
        scope.release();
        call(globalObject, resolve, resolveCallData, jsUndefined(), resolveArguments);
    }

    return JSValue::encode(jsUndefined());
}

static constexpr PropertyOffset promiseAllSettledStatusPropertyOffset = 0;
static constexpr PropertyOffset promiseAllSettledValuePropertyOffset = 1;
static constexpr PropertyOffset promiseAllSettledReasonPropertyOffset = 1;

Structure* createPromiseAllSettledFulfilledResultStructure(VM& vm, JSGlobalObject& globalObject)
{
    constexpr unsigned inlineCapacity = 2;
    Structure* structure = globalObject.structureCache().emptyObjectStructureForPrototype(&globalObject, globalObject.objectPrototype(), inlineCapacity);
    PropertyOffset offset;
    structure = Structure::addPropertyTransition(vm, structure, vm.propertyNames->status, 0, offset);
    RELEASE_ASSERT(offset == promiseAllSettledStatusPropertyOffset);
    structure = Structure::addPropertyTransition(vm, structure, vm.propertyNames->value, 0, offset);
    RELEASE_ASSERT(offset == promiseAllSettledValuePropertyOffset);
    return structure;
}

Structure* createPromiseAllSettledRejectedResultStructure(VM& vm, JSGlobalObject& globalObject)
{
    constexpr unsigned inlineCapacity = 2;
    Structure* structure = globalObject.structureCache().emptyObjectStructureForPrototype(&globalObject, globalObject.objectPrototype(), inlineCapacity);
    PropertyOffset offset;
    structure = Structure::addPropertyTransition(vm, structure, vm.propertyNames->status, 0, offset);
    RELEASE_ASSERT(offset == promiseAllSettledStatusPropertyOffset);
    structure = Structure::addPropertyTransition(vm, structure, vm.propertyNames->reason, 0, offset);
    RELEASE_ASSERT(offset == promiseAllSettledReasonPropertyOffset);
    return structure;
}

JSObject* createPromiseAllSettledFulfilledResult(JSGlobalObject* globalObject, JSValue value)
{
    VM& vm = globalObject->vm();
    JSObject* resultObject = constructEmptyObject(vm, globalObject->promiseAllSettledFulfilledResultStructure());
    resultObject->putDirectOffset(vm, promiseAllSettledStatusPropertyOffset, vm.smallStrings.fulfilledString());
    resultObject->putDirectOffset(vm, promiseAllSettledValuePropertyOffset, value);
    return resultObject;
}

JSObject* createPromiseAllSettledRejectedResult(JSGlobalObject* globalObject, JSValue reason)
{
    VM& vm = globalObject->vm();
    JSObject* resultObject = constructEmptyObject(vm, globalObject->promiseAllSettledRejectedResultStructure());
    resultObject->putDirectOffset(vm, promiseAllSettledStatusPropertyOffset, vm.smallStrings.rejectedString());
    resultObject->putDirectOffset(vm, promiseAllSettledReasonPropertyOffset, reason);
    return resultObject;
}

// Promise.any implementation
static JSObject* promiseAnySlow(JSGlobalObject* globalObject, CallFrame* callFrame, JSValue thisValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto [promise, resolve, reject] = JSPromise::newPromiseCapability(globalObject, thisValue);
    RETURN_IF_EXCEPTION(scope, { });

    auto callReject = [&](JSValue exception) -> void {
        MarkedArgumentBuffer rejectArguments;
        rejectArguments.append(exception);
        ASSERT(!rejectArguments.hasOverflowed());
        auto rejectCallData = getCallDataInline(reject);
        scope.release();
        call(globalObject, reject, rejectCallData, jsUndefined(), rejectArguments);
    };
    auto callRejectWithScopeException = [&]() -> void {
        Exception* exception = scope.exception();
        ASSERT(exception);
        TRY_CLEAR_EXCEPTION(scope, void());
        callReject(exception->value());
    };

    JSValue promiseResolveValue = thisValue.get(globalObject, vm.propertyNames->resolve);
    if (scope.exception()) [[unlikely]] {
        callRejectWithScopeException();
        return promise;
    }

    if (!promiseResolveValue.isCallable()) [[unlikely]] {
        callReject(createTypeError(globalObject, "Promise resolve is not a function"_s));
        return promise;
    }
    CallData promiseResolveCallData = getCallDataInline(promiseResolveValue);
    ASSERT(promiseResolveCallData.type != CallData::Type::None);

    std::optional<CachedCall> cachedCallHolder;
    CachedCall* cachedCall = nullptr;
    if (promiseResolveCallData.type == CallData::Type::JS) [[likely]] {
        cachedCallHolder.emplace(globalObject, jsCast<JSFunction*>(promiseResolveValue), 1);
        if (scope.exception()) [[unlikely]] {
            callRejectWithScopeException();
            return promise;
        }
        cachedCall = &cachedCallHolder.value();
    }

    JSArray* errors = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithUndecided), 0);
    if (!errors) [[unlikely]] {
        callReject(createOutOfMemoryError(globalObject));
        return promise;
    }

    JSPromiseCombinatorsGlobalContext* globalContext = JSPromiseCombinatorsGlobalContext::create(vm, promise, errors, jsNumber(1));

    uint64_t index = 0;

    JSValue iterable = callFrame->argument(0);
    forEachInIterable(globalObject, iterable, [&](VM& vm, JSGlobalObject* globalObject, JSValue value) {
        auto scope = DECLARE_THROW_SCOPE(vm);

        errors->putDirectIndex(globalObject, index, jsUndefined());
        RETURN_IF_EXCEPTION(scope, void());

        JSValue nextPromise;
        if (cachedCall) [[likely]] {
            nextPromise = cachedCall->callWithArguments(globalObject, thisValue, value);
            RETURN_IF_EXCEPTION(scope, void());
        } else {
            MarkedArgumentBuffer arguments;
            arguments.append(value);
            ASSERT(!arguments.hasOverflowed());
            nextPromise = call(globalObject, promiseResolveValue, promiseResolveCallData, thisValue, arguments);
            RETURN_IF_EXCEPTION(scope, void());
        }

        uint64_t count = globalContext->remainingElementsCount().toIndex(globalObject, "count exceeds size"_s);
        RETURN_IF_EXCEPTION(scope, void());
        globalContext->setRemainingElementsCount(vm, jsNumber(count + 1));

        JSPromiseCombinatorsContext* context = JSPromiseCombinatorsContext::create(vm, globalContext, index);

        // For Promise.any slow path, use resolve directly as onFulfilled
        auto* onRejected = JSFunctionWithFields::create(vm, globalObject, vm.promiseAnySlowRejectFunctionExecutable(), 1, emptyString());
        onRejected->setField(vm, JSFunctionWithFields::Field::PromiseAnyContext, context);
        onRejected->setField(vm, JSFunctionWithFields::Field::PromiseAnyReject, reject);

        JSValue then = nextPromise.get(globalObject, vm.propertyNames->then);
        RETURN_IF_EXCEPTION(scope, void());
        CallData thenCallData = getCallDataInline(then);
        if (thenCallData.type == CallData::Type::None) [[unlikely]] {
            throwTypeError(globalObject, scope, "then is not a function"_s);
            return;
        }

        MarkedArgumentBuffer thenArguments;
        thenArguments.append(resolve);
        thenArguments.append(onRejected);
        ASSERT(!thenArguments.hasOverflowed());
        scope.release();
        call(globalObject, then, thenCallData, nextPromise, thenArguments);
        ++index;
    });

    if (scope.exception()) [[unlikely]] {
        callRejectWithScopeException();
        return promise;
    }

    uint64_t count = globalContext->remainingElementsCount().toIndex(globalObject, "count exceeds size"_s);
    if (scope.exception()) [[unlikely]] {
        callRejectWithScopeException();
        return promise;
    }

    --count;
    globalContext->setRemainingElementsCount(vm, jsNumber(count));
    if (!count) {
        auto* aggregateError = createAggregateError(vm, globalObject->errorStructure(ErrorType::AggregateError), errors, String(), jsUndefined());
        callReject(aggregateError);
        if (scope.exception()) [[unlikely]] {
            callRejectWithScopeException();
            return promise;
        }
    }

    return promise;
}

JSC_DEFINE_HOST_FUNCTION(promiseConstructorFuncAny, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue().toThis(globalObject, ECMAMode::strict());

    if (!thisValue.isObject()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "|this| is not an object"_s);

    if (!isFastPromiseConstructor(globalObject, thisValue)) [[unlikely]]
        RELEASE_AND_RETURN(scope, JSValue::encode(promiseAnySlow(globalObject, callFrame, thisValue)));

    auto* promise = JSPromise::create(vm, globalObject->promiseStructure());

    auto callReject = [&]() -> void {
        Exception* exception = scope.exception();
        ASSERT(exception);
        TRY_CLEAR_EXCEPTION(scope, void());
        scope.release();
        promise->reject(vm, globalObject, exception);
    };

    JSArray* errors = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithUndecided), 0);
    if (!errors) [[unlikely]] {
        throwOutOfMemoryError(globalObject, scope);
        callReject();
        return JSValue::encode(promise);
    }

    JSPromiseCombinatorsGlobalContext* globalContext = JSPromiseCombinatorsGlobalContext::create(vm, promise, errors, jsNumber(1));

    uint64_t index = 0;

    JSValue resolve;
    JSValue iterable = callFrame->argument(0);
    forEachInIterable(globalObject, iterable, [&](VM& vm, JSGlobalObject* globalObject, JSValue value) {
        auto scope = DECLARE_THROW_SCOPE(vm);

        errors->putDirectIndex(globalObject, index, jsUndefined());
        RETURN_IF_EXCEPTION(scope, void());

        JSPromise* nextPromise = JSPromise::resolvedPromise(globalObject, value);
        RETURN_IF_EXCEPTION(scope, void());

        uint64_t count = globalContext->remainingElementsCount().toIndex(globalObject, "count exceeds size"_s);
        RETURN_IF_EXCEPTION(scope, void());
        globalContext->setRemainingElementsCount(vm, jsNumber(count + 1));

        JSPromiseCombinatorsContext* context = JSPromiseCombinatorsContext::create(vm, globalContext, index);

        if (nextPromise->isThenFastAndNonObservable()) [[likely]] {
            auto* constructor = promiseSpeciesConstructor(globalObject, nextPromise);
            RETURN_IF_EXCEPTION(scope, void());
            if (constructor == globalObject->promiseConstructor()) [[likely]] {
                scope.release();
                nextPromise->performPromiseThenWithInternalMicrotask(vm, globalObject, InternalMicrotask::PromiseAnyResolveJob, promise, context);
                ++index;
                return;
            }
        }

        // For Promise.any, onFulfilled just resolves the main promise directly
        if (!resolve) {
            auto [onFulfilled, onRejected] = promise->createFirstResolvingFunctions(vm, globalObject);
            resolve = onFulfilled;
        }

        auto* onRejected = JSFunctionWithFields::create(vm, globalObject, vm.promiseAnyRejectFunctionExecutable(), 1, emptyString());
        onRejected->setField(vm, JSFunctionWithFields::Field::PromiseAnyContext, context);

        JSValue then = nextPromise->get(globalObject, vm.propertyNames->then);
        RETURN_IF_EXCEPTION(scope, void());
        CallData thenCallData = getCallDataInline(then);
        if (thenCallData.type == CallData::Type::None) [[unlikely]] {
            throwTypeError(globalObject, scope, "then is not a function"_s);
            return;
        }

        MarkedArgumentBuffer thenArguments;
        thenArguments.append(resolve);
        thenArguments.append(onRejected);
        ASSERT(!thenArguments.hasOverflowed());
        scope.release();
        call(globalObject, then, thenCallData, nextPromise, thenArguments);
        ++index;
    });

    if (scope.exception()) [[unlikely]] {
        callReject();
        return JSValue::encode(promise);
    }

    uint64_t count = globalContext->remainingElementsCount().toIndex(globalObject, "count exceeds size"_s);
    if (scope.exception()) [[unlikely]] {
        callReject();
        return JSValue::encode(promise);
    }

    --count;
    globalContext->setRemainingElementsCount(vm, jsNumber(count));
    if (!count) {
        auto* aggregateError = createAggregateError(vm, globalObject->errorStructure(ErrorType::AggregateError), errors, String(), jsUndefined());
        scope.release();
        promise->reject(vm, globalObject, aggregateError);
        if (scope.exception()) [[unlikely]] {
            callReject();
            return JSValue::encode(promise);
        }
    }

    return JSValue::encode(promise);
}

JSC_DEFINE_HOST_FUNCTION(promiseAnyRejectFunction, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* callee = jsCast<JSFunctionWithFields*>(callFrame->jsCallee());
    auto* context = jsDynamicCast<JSPromiseCombinatorsContext*>(callee->getField(JSFunctionWithFields::Field::PromiseAnyContext));
    if (!context) [[unlikely]]
        return JSValue::encode(jsUndefined());

    callee->setField(vm, JSFunctionWithFields::Field::PromiseAnyContext, jsNull());

    auto* globalContext = jsCast<JSPromiseCombinatorsGlobalContext*>(context->globalContext());
    auto* promise = jsCast<JSPromise*>(globalContext->promise());
    auto* errors = jsCast<JSArray*>(globalContext->values());

    JSValue reason = callFrame->argument(0);
    uint64_t index = context->index();

    errors->putDirectIndex(globalObject, index, reason);
    RETURN_IF_EXCEPTION(scope, { });

    uint64_t count = globalContext->remainingElementsCount().toIndex(globalObject, "count exceeds size"_s);
    RETURN_IF_EXCEPTION(scope, { });

    --count;
    globalContext->setRemainingElementsCount(vm, jsNumber(count));
    if (!count) {
        auto* aggregateError = createAggregateError(vm, globalObject->errorStructure(ErrorType::AggregateError), errors, String(), jsUndefined());
        scope.release();
        promise->reject(vm, globalObject, aggregateError);
    }

    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(promiseAnySlowRejectFunction, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* callee = jsCast<JSFunctionWithFields*>(callFrame->jsCallee());
    auto* context = jsDynamicCast<JSPromiseCombinatorsContext*>(callee->getField(JSFunctionWithFields::Field::PromiseAnyContext));
    if (!context) [[unlikely]]
        return JSValue::encode(jsUndefined());

    JSValue reject = callee->getField(JSFunctionWithFields::Field::PromiseAnyReject);

    callee->setField(vm, JSFunctionWithFields::Field::PromiseAnyContext, jsNull());
    callee->setField(vm, JSFunctionWithFields::Field::PromiseAnyReject, jsNull());

    auto* globalContext = jsCast<JSPromiseCombinatorsGlobalContext*>(context->globalContext());
    auto* errors = jsCast<JSArray*>(globalContext->values());

    JSValue reason = callFrame->argument(0);
    uint64_t index = context->index();

    errors->putDirectIndex(globalObject, index, reason);
    RETURN_IF_EXCEPTION(scope, { });

    uint64_t count = globalContext->remainingElementsCount().toIndex(globalObject, "count exceeds size"_s);
    RETURN_IF_EXCEPTION(scope, { });

    --count;
    globalContext->setRemainingElementsCount(vm, jsNumber(count));
    if (!count) {
        auto* aggregateError = createAggregateError(vm, globalObject->errorStructure(ErrorType::AggregateError), errors, String(), jsUndefined());
        MarkedArgumentBuffer rejectArguments;
        rejectArguments.append(aggregateError);
        ASSERT(!rejectArguments.hasOverflowed());
        auto rejectCallData = getCallDataInline(reject);
        scope.release();
        call(globalObject, reject, rejectCallData, jsUndefined(), rejectArguments);
    }

    return JSValue::encode(jsUndefined());
}

} // namespace JSC
