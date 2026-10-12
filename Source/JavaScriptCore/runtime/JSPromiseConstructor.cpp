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
#include "MicrotaskQueueInlines.h"
#include "ObjectConstructor.h"
#include "PropertyNameArray.h"
#include "VMInlines.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSPromiseConstructor);
static JSC_DECLARE_HOST_FUNCTION(promiseConstructorFuncResolve);
static JSC_DECLARE_HOST_FUNCTION(promiseConstructorFuncReject);
static JSC_DECLARE_HOST_FUNCTION(promiseConstructorFuncWithResolvers);
static JSC_DECLARE_HOST_FUNCTION(promiseConstructorFuncAny);
static JSC_DECLARE_HOST_FUNCTION(promiseConstructorFuncRace);
static JSC_DECLARE_HOST_FUNCTION(promiseConstructorFuncAll);
static JSC_DECLARE_HOST_FUNCTION(promiseConstructorFuncAllSettled);
static JSC_DECLARE_HOST_FUNCTION(promiseConstructorFuncAllKeyed);
static JSC_DECLARE_HOST_FUNCTION(promiseConstructorFuncAllSettledKeyed);
static JSC_DECLARE_HOST_FUNCTION(promiseConstructorFuncIsPromise);

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

    JSGlobalObject* globalObject = this->realm();

    putDirectNonIndexAccessorWithoutTransition(vm, vm.propertyNames->speciesSymbol, globalObject->promiseSpeciesGetterSetter(), PropertyAttribute::Accessor | PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->tryKeyword, promiseConstructorTryCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));

    if (Options::usePromiseIsPromise())
        JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("isPromise"_s, promiseConstructorFuncIsPromise, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);

    if (Options::usePromiseAllKeyed()) {
        JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("allKeyed"_s, promiseConstructorFuncAllKeyed, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
        JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("allSettledKeyed"_s, promiseConstructorFuncAllSettledKeyed, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    }
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

static bool NODELETE isFastPromiseConstructor(JSGlobalObject* globalObject, JSValue value)
{
    if (value != globalObject->promiseConstructor()) [[unlikely]]
        return false;

    if (!globalObject->promiseResolveWatchpointSet().isStillValid()) [[unlikely]]
        return false;

    return true;
}

static ALWAYS_INLINE bool canSkipIntermediatePromise(JSGlobalObject* globalObject, JSValue value)
{
    if (!globalObject->promiseThenWatchpointSet().isStillValid()) [[unlikely]]
        return false;
    if (!value.isCell())
        return true;
    JSCell* cell = value.asCell();
    if (cell->type() == JSPromiseType)
        return false;
    if (!cell->isObject())
        return true;
    return isDefinitelyNonThenable(uncheckedDowncast<JSObject>(cell), globalObject);
}

static ALWAYS_INLINE unsigned vectorLengthHintForCombinator(JSValue iterable)
{
    if (!isJSArray(iterable))
        return 0;
    return std::min<unsigned>(uncheckedDowncast<JSArray>(iterable)->length(), MAX_STORAGE_VECTOR_LENGTH);
}

static JSObject* promiseRaceSlow(JSGlobalObject* globalObject, CallFrame* callFrame, JSValue thisValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto [promise, resolve, reject] = JSPromise::newPromiseCapability(globalObject, thisValue);
    RETURN_IF_EXCEPTION(scope, { });

    auto callReject = [&](JSValue exception) -> void {
        auto rejectArguments = WTF::toArray<EncodedJSValue>({
            JSValue::encode(exception),
        });
        auto rejectCallData = getCallDataInline(reject);
        scope.release();
        call(globalObject, reject, rejectCallData, jsUndefined(), ArgList { rejectArguments.data(), rejectArguments.size() });
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
        cachedCallHolder.emplace(globalObject, uncheckedDowncast<JSFunction>(promiseResolveValue), 1);
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
            auto arguments = WTF::toArray<EncodedJSValue>({
                JSValue::encode(value),
            });
            nextPromise = call(globalObject, promiseResolveValue, promiseResolveCallData, thisValue, ArgList { arguments.data(), arguments.size() });
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
        auto thenArguments = WTF::toArray<EncodedJSValue>({
            JSValue::encode(resolve),
            JSValue::encode(reject),
        });
        scope.release();
        call(globalObject, then, thenCallData, nextPromise, ArgList { thenArguments.data(), thenArguments.size() });
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
        promise->reject(vm, exception);
    };

    JSValue iterable = callFrame->argument(0);
    JSFunction* resolve = nullptr;
    JSFunction* reject = nullptr;
    forEachInIterable(globalObject, iterable, [&](VM& vm, JSGlobalObject* globalObject, JSValue value) {
        auto scope = DECLARE_THROW_SCOPE(vm);

        if (canSkipIntermediatePromise(globalObject, value)) {
            scope.release();
            globalObject->queueMicrotask(vm, InternalMicrotask::PromiseRaceResolveJob, static_cast<uint8_t>(JSPromise::Status::Fulfilled), promise, value, promise);
            return;
        }

        JSPromise* nextPromise = JSPromise::resolvedPromise(globalObject, value);
        RETURN_IF_EXCEPTION(scope, void());

        if (nextPromise->isThenFastAndNonObservable()) [[likely]] {
            auto* constructor = promiseSpeciesConstructor(globalObject, nextPromise);
            RETURN_IF_EXCEPTION(scope, void());
            if (constructor == globalObject->promiseConstructor()) [[likely]] {
                scope.release();
                nextPromise->performPromiseThenWithInternalMicrotask(vm, InternalMicrotask::PromiseRaceResolveJob, promise, promise);
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
        auto thenArguments = WTF::toArray<EncodedJSValue>({
            JSValue::encode(resolve),
            JSValue::encode(reject),
        });
        scope.release();
        call(globalObject, then, thenCallData, nextPromise, ArgList { thenArguments.data(), thenArguments.size() });
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
        auto rejectArguments = WTF::toArray<EncodedJSValue>({
            JSValue::encode(exception),
        });
        auto rejectCallData = getCallDataInline(reject);
        scope.release();
        call(globalObject, reject, rejectCallData, jsUndefined(), ArgList { rejectArguments.data(), rejectArguments.size() });
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
        cachedCallHolder.emplace(globalObject, uncheckedDowncast<JSFunction>(promiseResolveValue), 1);
        if (scope.exception()) [[unlikely]] {
            callRejectWithScopeException();
            return promise;
        }
        cachedCall = &cachedCallHolder.value();
    }

    JSValue iterable = callFrame->argument(0);
    JSArray* values = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous), 0, vectorLengthHintForCombinator(iterable));
    if (!values) [[unlikely]] {
        callReject(createOutOfMemoryError(globalObject));
        return promise;
    }

    JSPromiseCombinatorsGlobalContext* globalContext = JSPromiseCombinatorsGlobalContext::create(vm, promise, values, 1);

    uint64_t index = 0;

    forEachInIterable(globalObject, iterable, [&](VM& vm, JSGlobalObject* globalObject, JSValue value) {
        auto scope = DECLARE_THROW_SCOPE(vm);

        values->putDirectIndex(globalObject, index, jsUndefined());
        RETURN_IF_EXCEPTION(scope, void());

        JSValue nextPromise;
        if (cachedCall) [[likely]] {
            nextPromise = cachedCall->callWithArguments(globalObject, thisValue, value);
            RETURN_IF_EXCEPTION(scope, void());
        } else {
            auto arguments = WTF::toArray<EncodedJSValue>({
                JSValue::encode(value),
            });
            nextPromise = call(globalObject, promiseResolveValue, promiseResolveCallData, thisValue, ArgList { arguments.data(), arguments.size() });
            RETURN_IF_EXCEPTION(scope, void());
        }
        ASSERT(nextPromise);

        globalContext->setRemainingElementsCount(globalContext->remainingElementsCount() + 1);

        uint64_t currentIndex = index++;

        JSPromiseCombinatorsContext* context = JSPromiseCombinatorsContext::create(vm, globalContext, currentIndex);

        auto* onFulfilled = JSFunctionWithFields::create(vm, globalObject, vm.promiseAllSlowFulfillFunctionExecutable());
        onFulfilled->setField(vm, JSFunctionWithFields::Field::PromiseAllContext, context);
        onFulfilled->setField(vm, JSFunctionWithFields::Field::PromiseAllResolve, resolve);

        JSValue then = nextPromise.get(globalObject, vm.propertyNames->then);
        RETURN_IF_EXCEPTION(scope, void());
        CallData thenCallData = getCallDataInline(then);
        if (thenCallData.type == CallData::Type::None) [[unlikely]] {
            throwTypeError(globalObject, scope, "then is not a function"_s);
            return;
        }

        auto thenArguments = WTF::toArray<EncodedJSValue>({
            JSValue::encode(onFulfilled),
            JSValue::encode(reject),
        });
        scope.release();
        call(globalObject, then, thenCallData, nextPromise, ArgList { thenArguments.data(), thenArguments.size() });
    });

    if (scope.exception()) [[unlikely]] {
        callRejectWithScopeException();
        return promise;
    }

    uint64_t count = globalContext->remainingElementsCount() - 1;
    globalContext->setRemainingElementsCount(count);
    if (!count) {
        auto resolveArguments = WTF::toArray<EncodedJSValue>({
            JSValue::encode(values),
        });
        auto resolveCallData = getCallDataInline(resolve);
        scope.release();
        call(globalObject, resolve, resolveCallData, jsUndefined(), ArgList { resolveArguments.data(), resolveArguments.size() });
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
        promise->reject(vm, exception);
    };

    JSValue iterable = callFrame->argument(0);
    JSArray* values = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous), 0, vectorLengthHintForCombinator(iterable));
    if (!values) [[unlikely]] {
        throwOutOfMemoryError(globalObject, scope);
        callReject();
        return JSValue::encode(promise);
    }

    JSPromiseCombinatorsGlobalContext* globalContext = JSPromiseCombinatorsGlobalContext::create(vm, promise, values, 1);

    uint64_t index = 0;
    JSFunction* onRejected = nullptr;

    forEachInIterable(globalObject, iterable, [&](VM& vm, JSGlobalObject* globalObject, JSValue value) {
        auto scope = DECLARE_THROW_SCOPE(vm);

        values->putDirectIndex(globalObject, index, jsUndefined());
        RETURN_IF_EXCEPTION(scope, void());

        if (canSkipIntermediatePromise(globalObject, value)) {
            globalContext->setRemainingElementsCount(globalContext->remainingElementsCount() + 1);
            scope.release();
            globalObject->queueMicrotask(vm, InternalMicrotask::PromiseAllResolveJob, static_cast<uint8_t>(JSPromise::Status::Fulfilled), globalContext, value, jsNumber(index));
            ++index;
            return;
        }

        JSPromise* nextPromise = JSPromise::resolvedPromise(globalObject, value);
        RETURN_IF_EXCEPTION(scope, void());

        globalContext->setRemainingElementsCount(globalContext->remainingElementsCount() + 1);

        if (nextPromise->isThenFastAndNonObservable()) [[likely]] {
            auto* constructor = promiseSpeciesConstructor(globalObject, nextPromise);
            RETURN_IF_EXCEPTION(scope, void());
            if (constructor == globalObject->promiseConstructor()) [[likely]] {
                scope.release();
                nextPromise->performPromiseThenWithInternalMicrotask(vm, InternalMicrotask::PromiseAllResolveJob, globalContext, jsNumber(index));
                ++index;
                return;
            }
        }

        if (!onRejected)
            onRejected = promise->createFirstRejectFunction(vm, globalObject);
        JSValue then = nextPromise->get(globalObject, vm.propertyNames->then);
        RETURN_IF_EXCEPTION(scope, void());
        CallData thenCallData = getCallDataInline(then);
        if (thenCallData.type == CallData::Type::None) [[unlikely]] {
            throwTypeError(globalObject, scope, "then is not a function"_s);
            return;
        }

        JSPromiseCombinatorsContext* context = JSPromiseCombinatorsContext::create(vm, globalContext, index);
        auto* onFulfilled = JSFunctionWithFields::create(vm, globalObject, vm.promiseAllFulfillFunctionExecutable());
        onFulfilled->setField(vm, JSFunctionWithFields::Field::PromiseAllContext, context);

        auto thenArguments = WTF::toArray<EncodedJSValue>({
            JSValue::encode(onFulfilled),
            JSValue::encode(onRejected),
        });
        scope.release();
        call(globalObject, then, thenCallData, nextPromise, ArgList { thenArguments.data(), thenArguments.size() });
        ++index;
    });

    if (scope.exception()) [[unlikely]] {
        callReject();
        return JSValue::encode(promise);
    }

    uint64_t count = globalContext->remainingElementsCount() - 1;
    globalContext->setRemainingElementsCount(count);
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

static JSPromiseCombinatorsContext* takePromiseAllElementContext(VM& vm, JSFunctionWithFields* callee)
{
    auto* context = dynamicDowncast<JSPromiseCombinatorsContext>(callee->getField(JSFunctionWithFields::Field::PromiseAllContext));
    if (!context) [[unlikely]]
        return nullptr;
    callee->setField(vm, JSFunctionWithFields::Field::PromiseAllContext, jsNull());
    return context;
}

static JSPromiseCombinatorsContext* takePromiseAllSettledElementContext(VM& vm, JSFunctionWithFields* callee)
{
    auto* context = dynamicDowncast<JSPromiseCombinatorsContext>(callee->getField(JSFunctionWithFields::Field::PromiseAllSettledContext));
    if (!context) [[unlikely]]
        return nullptr;

    auto* other = dynamicDowncast<JSFunctionWithFields>(callee->getField(JSFunctionWithFields::Field::PromiseAllSettledOther));
    if (!other) [[unlikely]]
        return nullptr;

    callee->setField(vm, JSFunctionWithFields::Field::PromiseAllSettledContext, jsNull());
    callee->setField(vm, JSFunctionWithFields::Field::PromiseAllSettledOther, jsNull());
    other->setField(vm, JSFunctionWithFields::Field::PromiseAllSettledContext, jsNull());
    other->setField(vm, JSFunctionWithFields::Field::PromiseAllSettledOther, jsNull());
    return context;
}

JSC_DEFINE_HOST_FUNCTION(promiseAllFulfillFunction, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* context = takePromiseAllElementContext(vm, uncheckedDowncast<JSFunctionWithFields>(callFrame->jsCallee()));
    if (!context) [[unlikely]]
        return JSValue::encode(jsUndefined());

    auto* globalContext = context->globalContext();
    auto* promise = uncheckedDowncast<JSPromise>(globalContext->promise());
    auto* values = uncheckedDowncast<JSArray>(globalContext->values());

    JSValue value = callFrame->argument(0);
    uint64_t index = context->index();

    values->putDirectIndex(globalObject, index, value);
    RETURN_IF_EXCEPTION(scope, { });

    uint64_t count = globalContext->remainingElementsCount() - 1;
    globalContext->setRemainingElementsCount(count);
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

    auto* callee = uncheckedDowncast<JSFunctionWithFields>(callFrame->jsCallee());
    auto* context = dynamicDowncast<JSPromiseCombinatorsContext>(callee->getField(JSFunctionWithFields::Field::PromiseAllContext));
    if (!context) [[unlikely]]
        return JSValue::encode(jsUndefined());

    JSValue resolve = callee->getField(JSFunctionWithFields::Field::PromiseAllResolve);

    callee->setField(vm, JSFunctionWithFields::Field::PromiseAllContext, jsNull());
    callee->setField(vm, JSFunctionWithFields::Field::PromiseAllResolve, jsNull());

    auto* globalContext = context->globalContext();
    auto* values = uncheckedDowncast<JSArray>(globalContext->values());

    JSValue value = callFrame->argument(0);
    uint64_t index = context->index();

    values->putDirectIndex(globalObject, index, value);
    RETURN_IF_EXCEPTION(scope, { });

    uint64_t count = globalContext->remainingElementsCount() - 1;
    globalContext->setRemainingElementsCount(count);
    if (!count) {
        auto resolveArguments = WTF::toArray<EncodedJSValue>({
            JSValue::encode(values),
        });
        auto resolveCallData = getCallDataInline(resolve);
        scope.release();
        call(globalObject, resolve, resolveCallData, jsUndefined(), ArgList { resolveArguments.data(), resolveArguments.size() });
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
        auto rejectArguments = WTF::toArray<EncodedJSValue>({
            JSValue::encode(exception),
        });
        auto rejectCallData = getCallDataInline(reject);
        scope.release();
        call(globalObject, reject, rejectCallData, jsUndefined(), ArgList { rejectArguments.data(), rejectArguments.size() });
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
        cachedCallHolder.emplace(globalObject, uncheckedDowncast<JSFunction>(promiseResolveValue), 1);
        if (scope.exception()) [[unlikely]] {
            callRejectWithScopeException();
            return promise;
        }
        cachedCall = &cachedCallHolder.value();
    }

    JSValue iterable = callFrame->argument(0);
    JSArray* values = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous), 0, vectorLengthHintForCombinator(iterable));
    if (!values) [[unlikely]] {
        callReject(createOutOfMemoryError(globalObject));
        return promise;
    }

    JSPromiseCombinatorsGlobalContext* globalContext = JSPromiseCombinatorsGlobalContext::create(vm, resolve, values, 1);

    uint64_t index = 0;

    forEachInIterable(globalObject, iterable, [&](VM& vm, JSGlobalObject* globalObject, JSValue value) {
        auto scope = DECLARE_THROW_SCOPE(vm);

        values->putDirectIndex(globalObject, index, jsUndefined());
        RETURN_IF_EXCEPTION(scope, void());

        JSValue nextPromise;
        if (cachedCall) [[likely]] {
            nextPromise = cachedCall->callWithArguments(globalObject, thisValue, value);
            RETURN_IF_EXCEPTION(scope, void());
        } else {
            auto arguments = WTF::toArray<EncodedJSValue>({
                JSValue::encode(value),
            });
            nextPromise = call(globalObject, promiseResolveValue, promiseResolveCallData, thisValue, ArgList { arguments.data(), arguments.size() });
            RETURN_IF_EXCEPTION(scope, void());
        }
        ASSERT(nextPromise);

        globalContext->setRemainingElementsCount(globalContext->remainingElementsCount() + 1);

        uint64_t currentIndex = index++;

        JSPromiseCombinatorsContext* context = JSPromiseCombinatorsContext::create(vm, globalContext, currentIndex);

        auto* onFulfilled = JSFunctionWithFields::create(vm, globalObject, vm.promiseAllSettledSlowFulfillFunctionExecutable());
        onFulfilled->setField(vm, JSFunctionWithFields::Field::PromiseAllSettledContext, context);

        auto* onRejected = JSFunctionWithFields::create(vm, globalObject, vm.promiseAllSettledSlowRejectFunctionExecutable());
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

        auto thenArguments = WTF::toArray<EncodedJSValue>({
            JSValue::encode(onFulfilled),
            JSValue::encode(onRejected),
        });
        scope.release();
        call(globalObject, then, thenCallData, nextPromise, ArgList { thenArguments.data(), thenArguments.size() });
    });

    if (scope.exception()) [[unlikely]] {
        callRejectWithScopeException();
        return promise;
    }

    uint64_t count = globalContext->remainingElementsCount() - 1;
    globalContext->setRemainingElementsCount(count);
    if (!count) {
        auto resolveArguments = WTF::toArray<EncodedJSValue>({
            JSValue::encode(values),
        });
        auto resolveCallData = getCallDataInline(resolve);
        scope.release();
        call(globalObject, resolve, resolveCallData, jsUndefined(), ArgList { resolveArguments.data(), resolveArguments.size() });
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
        promise->reject(vm, exception);
    };

    JSValue iterable = callFrame->argument(0);
    JSArray* values = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous), 0, vectorLengthHintForCombinator(iterable));
    if (!values) [[unlikely]] {
        throwOutOfMemoryError(globalObject, scope);
        callReject();
        return JSValue::encode(promise);
    }

    JSPromiseCombinatorsGlobalContext* globalContext = JSPromiseCombinatorsGlobalContext::create(vm, promise, values, 1);

    uint64_t index = 0;

    forEachInIterable(globalObject, iterable, [&](VM& vm, JSGlobalObject* globalObject, JSValue value) {
        auto scope = DECLARE_THROW_SCOPE(vm);

        values->putDirectIndex(globalObject, index, jsUndefined());
        RETURN_IF_EXCEPTION(scope, void());

        if (canSkipIntermediatePromise(globalObject, value)) {
            globalContext->setRemainingElementsCount(globalContext->remainingElementsCount() + 1);
            scope.release();
            globalObject->queueMicrotask(vm, InternalMicrotask::PromiseAllSettledResolveJob, static_cast<uint8_t>(JSPromise::Status::Fulfilled), globalContext, value, jsNumber(index));
            ++index;
            return;
        }

        JSPromise* nextPromise = JSPromise::resolvedPromise(globalObject, value);
        RETURN_IF_EXCEPTION(scope, void());

        globalContext->setRemainingElementsCount(globalContext->remainingElementsCount() + 1);

        if (nextPromise->isThenFastAndNonObservable()) [[likely]] {
            auto* constructor = promiseSpeciesConstructor(globalObject, nextPromise);
            RETURN_IF_EXCEPTION(scope, void());
            if (constructor == globalObject->promiseConstructor()) [[likely]] {
                scope.release();
                nextPromise->performPromiseThenWithInternalMicrotask(vm, InternalMicrotask::PromiseAllSettledResolveJob, globalContext, jsNumber(index));
                ++index;
                return;
            }
        }

        JSPromiseCombinatorsContext* context = JSPromiseCombinatorsContext::create(vm, globalContext, index);
        auto* onFulfilled = JSFunctionWithFields::create(vm, globalObject, vm.promiseAllSettledFulfillFunctionExecutable());
        onFulfilled->setField(vm, JSFunctionWithFields::Field::PromiseAllSettledContext, context);

        auto* onRejected = JSFunctionWithFields::create(vm, globalObject, vm.promiseAllSettledRejectFunctionExecutable());
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

        auto thenArguments = WTF::toArray<EncodedJSValue>({
            JSValue::encode(onFulfilled),
            JSValue::encode(onRejected),
        });
        scope.release();
        call(globalObject, then, thenCallData, nextPromise, ArgList { thenArguments.data(), thenArguments.size() });
        ++index;
    });

    if (scope.exception()) [[unlikely]] {
        callReject();
        return JSValue::encode(promise);
    }

    uint64_t count = globalContext->remainingElementsCount() - 1;
    globalContext->setRemainingElementsCount(count);
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

    auto* context = takePromiseAllSettledElementContext(vm, uncheckedDowncast<JSFunctionWithFields>(callFrame->jsCallee()));
    if (!context) [[unlikely]]
        return JSValue::encode(jsUndefined());

    auto* globalContext = context->globalContext();
    auto* promise = uncheckedDowncast<JSPromise>(globalContext->promise());
    auto* values = uncheckedDowncast<JSArray>(globalContext->values());

    JSValue value = callFrame->argument(0);
    uint64_t index = context->index();

    JSObject* resultObject = createPromiseAllSettledFulfilledResult(globalObject, value);

    values->putDirectIndex(globalObject, index, resultObject);
    RETURN_IF_EXCEPTION(scope, { });

    uint64_t count = globalContext->remainingElementsCount() - 1;
    globalContext->setRemainingElementsCount(count);
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

    auto* context = takePromiseAllSettledElementContext(vm, uncheckedDowncast<JSFunctionWithFields>(callFrame->jsCallee()));
    if (!context) [[unlikely]]
        return JSValue::encode(jsUndefined());

    auto* globalContext = context->globalContext();
    auto* promise = uncheckedDowncast<JSPromise>(globalContext->promise());
    auto* values = uncheckedDowncast<JSArray>(globalContext->values());

    JSValue reason = callFrame->argument(0);
    uint64_t index = context->index();

    JSObject* resultObject = createPromiseAllSettledRejectedResult(globalObject, reason);

    values->putDirectIndex(globalObject, index, resultObject);
    RETURN_IF_EXCEPTION(scope, { });

    uint64_t count = globalContext->remainingElementsCount() - 1;
    globalContext->setRemainingElementsCount(count);
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

    auto* context = takePromiseAllSettledElementContext(vm, uncheckedDowncast<JSFunctionWithFields>(callFrame->jsCallee()));
    if (!context) [[unlikely]]
        return JSValue::encode(jsUndefined());

    auto* globalContext = context->globalContext();
    auto* values = uncheckedDowncast<JSArray>(globalContext->values());
    JSValue resolve = globalContext->promise();

    JSValue value = callFrame->argument(0);
    uint64_t index = context->index();

    JSObject* resultObject = createPromiseAllSettledFulfilledResult(globalObject, value);

    values->putDirectIndex(globalObject, index, resultObject);
    RETURN_IF_EXCEPTION(scope, { });

    uint64_t count = globalContext->remainingElementsCount() - 1;
    globalContext->setRemainingElementsCount(count);
    if (!count) {
        auto resolveArguments = WTF::toArray<EncodedJSValue>({
            JSValue::encode(values),
        });
        auto resolveCallData = getCallDataInline(resolve);
        scope.release();
        call(globalObject, resolve, resolveCallData, jsUndefined(), ArgList { resolveArguments.data(), resolveArguments.size() });
    }

    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(promiseAllSettledSlowRejectFunction, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* context = takePromiseAllSettledElementContext(vm, uncheckedDowncast<JSFunctionWithFields>(callFrame->jsCallee()));
    if (!context) [[unlikely]]
        return JSValue::encode(jsUndefined());

    auto* globalContext = context->globalContext();
    auto* values = uncheckedDowncast<JSArray>(globalContext->values());
    JSValue resolve = globalContext->promise();

    JSValue reason = callFrame->argument(0);
    uint64_t index = context->index();

    JSObject* resultObject = createPromiseAllSettledRejectedResult(globalObject, reason);
    RETURN_IF_EXCEPTION(scope, { });

    values->putDirectIndex(globalObject, index, resultObject);
    RETURN_IF_EXCEPTION(scope, { });

    uint64_t count = globalContext->remainingElementsCount() - 1;
    globalContext->setRemainingElementsCount(count);
    if (!count) {
        auto resolveArguments = WTF::toArray<EncodedJSValue>({
            JSValue::encode(values),
        });
        auto resolveCallData = getCallDataInline(resolve);
        scope.release();
        call(globalObject, resolve, resolveCallData, jsUndefined(), ArgList { resolveArguments.data(), resolveArguments.size() });
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
        auto rejectArguments = WTF::toArray<EncodedJSValue>({
            JSValue::encode(exception),
        });
        auto rejectCallData = getCallDataInline(reject);
        scope.release();
        call(globalObject, reject, rejectCallData, jsUndefined(), ArgList { rejectArguments.data(), rejectArguments.size() });
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
        cachedCallHolder.emplace(globalObject, uncheckedDowncast<JSFunction>(promiseResolveValue), 1);
        if (scope.exception()) [[unlikely]] {
            callRejectWithScopeException();
            return promise;
        }
        cachedCall = &cachedCallHolder.value();
    }

    JSValue iterable = callFrame->argument(0);
    JSArray* errors = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous), 0, vectorLengthHintForCombinator(iterable));
    if (!errors) [[unlikely]] {
        callReject(createOutOfMemoryError(globalObject));
        return promise;
    }

    JSPromiseCombinatorsGlobalContext* globalContext = JSPromiseCombinatorsGlobalContext::create(vm, promise, errors, 1);

    uint64_t index = 0;

    forEachInIterable(globalObject, iterable, [&](VM& vm, JSGlobalObject* globalObject, JSValue value) {
        auto scope = DECLARE_THROW_SCOPE(vm);

        errors->putDirectIndex(globalObject, index, jsUndefined());
        RETURN_IF_EXCEPTION(scope, void());

        JSValue nextPromise;
        if (cachedCall) [[likely]] {
            nextPromise = cachedCall->callWithArguments(globalObject, thisValue, value);
            RETURN_IF_EXCEPTION(scope, void());
        } else {
            auto arguments = WTF::toArray<EncodedJSValue>({
                JSValue::encode(value),
            });
            nextPromise = call(globalObject, promiseResolveValue, promiseResolveCallData, thisValue, ArgList { arguments.data(), arguments.size() });
            RETURN_IF_EXCEPTION(scope, void());
        }

        globalContext->setRemainingElementsCount(globalContext->remainingElementsCount() + 1);

        JSPromiseCombinatorsContext* context = JSPromiseCombinatorsContext::create(vm, globalContext, index);

        // For Promise.any slow path, use resolve directly as onFulfilled
        auto* onRejected = JSFunctionWithFields::create(vm, globalObject, vm.promiseAnySlowRejectFunctionExecutable());
        onRejected->setField(vm, JSFunctionWithFields::Field::PromiseAnyContext, context);
        onRejected->setField(vm, JSFunctionWithFields::Field::PromiseAnyReject, reject);

        JSValue then = nextPromise.get(globalObject, vm.propertyNames->then);
        RETURN_IF_EXCEPTION(scope, void());
        CallData thenCallData = getCallDataInline(then);
        if (thenCallData.type == CallData::Type::None) [[unlikely]] {
            throwTypeError(globalObject, scope, "then is not a function"_s);
            return;
        }

        auto thenArguments = WTF::toArray<EncodedJSValue>({
            JSValue::encode(resolve),
            JSValue::encode(onRejected),
        });
        scope.release();
        call(globalObject, then, thenCallData, nextPromise, ArgList { thenArguments.data(), thenArguments.size() });
        ++index;
    });

    if (scope.exception()) [[unlikely]] {
        callRejectWithScopeException();
        return promise;
    }

    uint64_t count = globalContext->remainingElementsCount() - 1;
    globalContext->setRemainingElementsCount(count);
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
        promise->reject(vm, exception);
    };

    JSValue resolve;
    JSValue iterable = callFrame->argument(0);
    JSArray* errors = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous), 0, vectorLengthHintForCombinator(iterable));
    if (!errors) [[unlikely]] {
        throwOutOfMemoryError(globalObject, scope);
        callReject();
        return JSValue::encode(promise);
    }

    JSPromiseCombinatorsGlobalContext* globalContext = JSPromiseCombinatorsGlobalContext::create(vm, promise, errors, 1);

    uint64_t index = 0;

    forEachInIterable(globalObject, iterable, [&](VM& vm, JSGlobalObject* globalObject, JSValue value) {
        auto scope = DECLARE_THROW_SCOPE(vm);

        errors->putDirectIndex(globalObject, index, jsUndefined());
        RETURN_IF_EXCEPTION(scope, void());

        if (canSkipIntermediatePromise(globalObject, value)) {
            globalContext->setRemainingElementsCount(globalContext->remainingElementsCount() + 1);
            scope.release();
            globalObject->queueMicrotask(vm, InternalMicrotask::PromiseAnyResolveJob, static_cast<uint8_t>(JSPromise::Status::Fulfilled), globalContext, value, jsNumber(index));
            ++index;
            return;
        }

        JSPromise* nextPromise = JSPromise::resolvedPromise(globalObject, value);
        RETURN_IF_EXCEPTION(scope, void());

        globalContext->setRemainingElementsCount(globalContext->remainingElementsCount() + 1);

        if (nextPromise->isThenFastAndNonObservable()) [[likely]] {
            auto* constructor = promiseSpeciesConstructor(globalObject, nextPromise);
            RETURN_IF_EXCEPTION(scope, void());
            if (constructor == globalObject->promiseConstructor()) [[likely]] {
                scope.release();
                nextPromise->performPromiseThenWithInternalMicrotask(vm, InternalMicrotask::PromiseAnyResolveJob, globalContext, jsNumber(index));
                ++index;
                return;
            }
        }

        // For Promise.any, onFulfilled just resolves the main promise directly
        if (!resolve)
            resolve = promise->createFirstResolveFunction(vm, globalObject);

        JSPromiseCombinatorsContext* context = JSPromiseCombinatorsContext::create(vm, globalContext, index);
        auto* onRejected = JSFunctionWithFields::create(vm, globalObject, vm.promiseAnyRejectFunctionExecutable());
        onRejected->setField(vm, JSFunctionWithFields::Field::PromiseAnyContext, context);

        JSValue then = nextPromise->get(globalObject, vm.propertyNames->then);
        RETURN_IF_EXCEPTION(scope, void());
        CallData thenCallData = getCallDataInline(then);
        if (thenCallData.type == CallData::Type::None) [[unlikely]] {
            throwTypeError(globalObject, scope, "then is not a function"_s);
            return;
        }

        auto thenArguments = WTF::toArray<EncodedJSValue>({
            JSValue::encode(resolve),
            JSValue::encode(onRejected),
        });
        scope.release();
        call(globalObject, then, thenCallData, nextPromise, ArgList { thenArguments.data(), thenArguments.size() });
        ++index;
    });

    if (scope.exception()) [[unlikely]] {
        callReject();
        return JSValue::encode(promise);
    }

    uint64_t count = globalContext->remainingElementsCount() - 1;
    globalContext->setRemainingElementsCount(count);
    if (!count) {
        auto* aggregateError = createAggregateError(vm, globalObject->errorStructure(ErrorType::AggregateError), errors, String(), jsUndefined());
        scope.release();
        promise->reject(vm, aggregateError);
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

    auto* callee = uncheckedDowncast<JSFunctionWithFields>(callFrame->jsCallee());
    auto* context = dynamicDowncast<JSPromiseCombinatorsContext>(callee->getField(JSFunctionWithFields::Field::PromiseAnyContext));
    if (!context) [[unlikely]]
        return JSValue::encode(jsUndefined());

    callee->setField(vm, JSFunctionWithFields::Field::PromiseAnyContext, jsNull());

    auto* globalContext = context->globalContext();
    auto* promise = uncheckedDowncast<JSPromise>(globalContext->promise());
    auto* errors = uncheckedDowncast<JSArray>(globalContext->values());

    JSValue reason = callFrame->argument(0);
    uint64_t index = context->index();

    errors->putDirectIndex(globalObject, index, reason);
    RETURN_IF_EXCEPTION(scope, { });

    uint64_t count = globalContext->remainingElementsCount() - 1;
    globalContext->setRemainingElementsCount(count);
    if (!count) {
        auto* aggregateError = createAggregateError(vm, globalObject->errorStructure(ErrorType::AggregateError), errors, String(), jsUndefined());
        scope.release();
        promise->reject(vm, aggregateError);
    }

    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(promiseAnySlowRejectFunction, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* callee = uncheckedDowncast<JSFunctionWithFields>(callFrame->jsCallee());
    auto* context = dynamicDowncast<JSPromiseCombinatorsContext>(callee->getField(JSFunctionWithFields::Field::PromiseAnyContext));
    if (!context) [[unlikely]]
        return JSValue::encode(jsUndefined());

    JSValue reject = callee->getField(JSFunctionWithFields::Field::PromiseAnyReject);

    callee->setField(vm, JSFunctionWithFields::Field::PromiseAnyContext, jsNull());
    callee->setField(vm, JSFunctionWithFields::Field::PromiseAnyReject, jsNull());

    auto* globalContext = context->globalContext();
    auto* errors = uncheckedDowncast<JSArray>(globalContext->values());

    JSValue reason = callFrame->argument(0);
    uint64_t index = context->index();

    errors->putDirectIndex(globalObject, index, reason);
    RETURN_IF_EXCEPTION(scope, { });

    uint64_t count = globalContext->remainingElementsCount() - 1;
    globalContext->setRemainingElementsCount(count);
    if (!count) {
        auto* aggregateError = createAggregateError(vm, globalObject->errorStructure(ErrorType::AggregateError), errors, String(), jsUndefined());
        auto rejectArguments = WTF::toArray<EncodedJSValue>({
            JSValue::encode(aggregateError),
        });
        auto rejectCallData = getCallDataInline(reject);
        scope.release();
        call(globalObject, reject, rejectCallData, jsUndefined(), ArgList { rejectArguments.data(), rejectArguments.size() });
    }

    return JSValue::encode(jsUndefined());
}

// Promise.allKeyed / Promise.allSettledKeyed implementation
// https://tc39.es/proposal-await-dictionary/

enum class PromiseKeyedCombinatorVariant : uint8_t { All, AllSettled };

static constexpr uint64_t elementKeyIsIndexFlag = 1ULL << 32;
static_assert(static_cast<uint64_t>(MAX_ARRAY_INDEX) < elementKeyIsIndexFlag);

static constexpr ASCIILiteral promiseKeyedCombinatorNotObjectMessage(PromiseKeyedCombinatorVariant variant)
{
    return variant == PromiseKeyedCombinatorVariant::All ? "Promise.allKeyed requires that the first argument be an object"_s : "Promise.allSettledKeyed requires that the first argument be an object"_s;
}

static ALWAYS_INLINE uint64_t defineKeyedPromiseCombinatorElement(JSGlobalObject* globalObject, JSObject* resultObject, PropertyName propertyName)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (std::optional<uint32_t> index = parseIndex(propertyName)) {
        resultObject->putDirectIndex(globalObject, index.value(), jsUndefined());
        RETURN_IF_EXCEPTION(scope, 0);
        return elementKeyIsIndexFlag | index.value();
    }

    PutPropertySlot slot(resultObject);
    resultObject->putDirect(vm, propertyName, jsUndefined(), slot);
    ASSERT(slot.type() == PutPropertySlot::NewProperty);
    PropertyOffset offset = slot.cachedOffset();
    ASSERT(isValidOffset(offset));
    ASSERT(static_cast<uint64_t>(offset) < elementKeyIsIndexFlag);
    return static_cast<uint64_t>(offset);
}

static ALWAYS_INLINE void storeKeyedPromiseCombinatorElement(JSGlobalObject* globalObject, JSObject* resultObject, uint64_t elementKey, JSValue settled)
{
    VM& vm = globalObject->vm();
    if (elementKey & elementKeyIsIndexFlag) {
        resultObject->putDirectIndex(globalObject, static_cast<uint32_t>(elementKey), settled);
        return;
    }
    resultObject->putDirectOffset(vm, static_cast<PropertyOffset>(elementKey), settled);
}

void resolveKeyedPromiseCombinatorElement(JSGlobalObject* globalObject, JSPromiseCombinatorsGlobalContext* globalContext, uint64_t elementKey, JSValue settled)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* resultObject = asObject(globalContext->values());
    storeKeyedPromiseCombinatorElement(globalObject, resultObject, elementKey, settled);
    RETURN_IF_EXCEPTION(scope, void());

    uint64_t count = globalContext->remainingElementsCount() - 1;
    globalContext->setRemainingElementsCount(count);
    if (count)
        return;

    auto* promise = uncheckedDowncast<JSPromise>(globalContext->promise());
    scope.release();
    promise->resolve(globalObject, vm, resultObject);
}

static void resolveKeyedPromiseCombinatorElementSlow(JSGlobalObject* globalObject, JSPromiseCombinatorsGlobalContext* globalContext, uint64_t elementKey, JSValue settled)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* resultObject = asObject(globalContext->values());
    storeKeyedPromiseCombinatorElement(globalObject, resultObject, elementKey, settled);
    RETURN_IF_EXCEPTION(scope, void());

    uint64_t count = globalContext->remainingElementsCount() - 1;
    globalContext->setRemainingElementsCount(count);
    if (count)
        return;

    JSValue resolve = globalContext->promise();
    MarkedArgumentBuffer resolveArguments;
    resolveArguments.append(resultObject);
    ASSERT(!resolveArguments.hasOverflowed());
    auto resolveCallData = getCallDataInline(resolve);
    scope.release();
    call(globalObject, resolve, resolveCallData, jsUndefined(), resolveArguments);
}

// https://tc39.es/proposal-await-dictionary/#sec-performpromiseallkeyed steps 1 and 5.a-5.b.ii.
static ALWAYS_INLINE void forEachKeyedPromiseCombinatorElement(JSGlobalObject* globalObject, JSObject* promises, JSObject* resultObject, NOESCAPE const Invocable<void(JSValue, uint64_t)> auto& callback)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    PropertyNameArrayBuilder propertyNames(vm, PropertyNameMode::StringsAndSymbols, PrivateSymbolMode::Exclude);
    promises->methodTable()->getOwnPropertyNames(promises, globalObject, propertyNames, DontEnumPropertiesMode::Include);
    RETURN_IF_EXCEPTION(scope, void());

    for (const auto& propertyName : propertyNames) {
        PropertySlot slot(promises, PropertySlot::InternalMethodType::GetOwnProperty);
        bool hasProperty = promises->getOwnPropertySlotInline(globalObject, propertyName, slot);
        RETURN_IF_EXCEPTION(scope, void());
        if (!hasProperty || (slot.attributes() & PropertyAttribute::DontEnum))
            continue;

        JSValue value;
        if (!slot.isTaintedByOpaqueObject()) [[likely]]
            value = slot.getValue(globalObject, propertyName);
        else
            value = promises->get(globalObject, propertyName);
        RETURN_IF_EXCEPTION(scope, void());

        uint64_t elementKey = defineKeyedPromiseCombinatorElement(globalObject, resultObject, propertyName);
        RETURN_IF_EXCEPTION(scope, void());

        callback(value, elementKey);
        RETURN_IF_EXCEPTION(scope, void());
    }
}

static std::tuple<JSFunctionWithFields*, JSFunctionWithFields*> createPromiseAllSettledKeyedElementFunctions(VM& vm, JSGlobalObject* globalObject, JSPromiseCombinatorsContext* context, NativeExecutable* fulfillExecutable, NativeExecutable* rejectExecutable)
{
    auto* onFulfilled = JSFunctionWithFields::create(vm, globalObject, fulfillExecutable);
    onFulfilled->setField(vm, JSFunctionWithFields::Field::PromiseAllSettledContext, context);

    auto* onRejected = JSFunctionWithFields::create(vm, globalObject, rejectExecutable);
    onRejected->setField(vm, JSFunctionWithFields::Field::PromiseAllSettledContext, context);

    onFulfilled->setField(vm, JSFunctionWithFields::Field::PromiseAllSettledOther, onRejected);
    onRejected->setField(vm, JSFunctionWithFields::Field::PromiseAllSettledOther, onFulfilled);
    return { onFulfilled, onRejected };
}

template<PromiseKeyedCombinatorVariant variant>
static JSObject* promiseAllKeyedSlow(JSGlobalObject* globalObject, CallFrame* callFrame, JSValue thisValue)
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

    JSValue promisesValue = callFrame->argument(0);
    if (!promisesValue.isObject()) [[unlikely]] {
        callReject(createTypeError(globalObject, promiseKeyedCombinatorNotObjectMessage(variant)));
        return promise;
    }
    JSObject* promises = asObject(promisesValue);

    std::optional<CachedCall> cachedCallHolder;
    CachedCall* cachedCall = nullptr;
    if (promiseResolveCallData.type == CallData::Type::JS) [[likely]] {
        cachedCallHolder.emplace(globalObject, uncheckedDowncast<JSFunction>(promiseResolveValue), 1);
        if (scope.exception()) [[unlikely]] {
            callRejectWithScopeException();
            return promise;
        }
        cachedCall = &cachedCallHolder.value();
    }

    JSObject* resultObject = constructEmptyObject(vm, globalObject->nullPrototypeObjectStructure());

    // As in promiseAllSettledSlow, the promise slot of the global context holds |resolve| for the element closures.
    JSPromiseCombinatorsGlobalContext* globalContext = JSPromiseCombinatorsGlobalContext::create(vm, resolve, resultObject, 1);

    forEachKeyedPromiseCombinatorElement(globalObject, promises, resultObject, [&](JSValue value, uint64_t elementKey) {
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

        globalContext->setRemainingElementsCount(globalContext->remainingElementsCount() + 1);

        JSPromiseCombinatorsContext* context = JSPromiseCombinatorsContext::create(vm, globalContext, elementKey);

        JSObject* onFulfilled;
        JSObject* onRejected;
        if constexpr (variant == PromiseKeyedCombinatorVariant::All) {
            auto* resolveElement = JSFunctionWithFields::create(vm, globalObject, vm.promiseAllKeyedSlowFulfillFunctionExecutable());
            resolveElement->setField(vm, JSFunctionWithFields::Field::PromiseAllContext, context);
            onFulfilled = resolveElement;
            onRejected = reject;
        } else
            std::tie(onFulfilled, onRejected) = createPromiseAllSettledKeyedElementFunctions(vm, globalObject, context, vm.promiseAllSettledKeyedSlowFulfillFunctionExecutable(), vm.promiseAllSettledKeyedSlowRejectFunctionExecutable());

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

    uint64_t count = globalContext->remainingElementsCount() - 1;
    globalContext->setRemainingElementsCount(count);
    if (!count) {
        MarkedArgumentBuffer resolveArguments;
        resolveArguments.append(resultObject);
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

template<PromiseKeyedCombinatorVariant variant>
static ALWAYS_INLINE EncodedJSValue promiseAllKeyedImpl(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    constexpr InternalMicrotask resolveJob = variant == PromiseKeyedCombinatorVariant::All ? InternalMicrotask::PromiseAllKeyedResolveJob : InternalMicrotask::PromiseAllSettledKeyedResolveJob;

    JSValue thisValue = callFrame->thisValue().toThis(globalObject, ECMAMode::strict());

    if (!thisValue.isObject()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "|this| is not an object"_s);

    if (!isFastPromiseConstructor(globalObject, thisValue)) [[unlikely]]
        RELEASE_AND_RETURN(scope, JSValue::encode(promiseAllKeyedSlow<variant>(globalObject, callFrame, thisValue)));

    auto* promise = JSPromise::create(vm, globalObject->promiseStructure());

    auto callReject = [&]() -> void {
        Exception* exception = scope.exception();
        ASSERT(exception);
        TRY_CLEAR_EXCEPTION(scope, void());
        scope.release();
        promise->reject(vm, exception);
    };

    JSValue promisesValue = callFrame->argument(0);
    if (!promisesValue.isObject()) [[unlikely]] {
        throwTypeError(globalObject, scope, promiseKeyedCombinatorNotObjectMessage(variant));
        callReject();
        return JSValue::encode(promise);
    }
    JSObject* promises = asObject(promisesValue);

    JSObject* resultObject = constructEmptyObject(vm, globalObject->nullPrototypeObjectStructure());

    JSPromiseCombinatorsGlobalContext* globalContext = JSPromiseCombinatorsGlobalContext::create(vm, promise, resultObject, 1);

    JSFunction* firstRejectFunction = nullptr;

    forEachKeyedPromiseCombinatorElement(globalObject, promises, resultObject, [&](JSValue value, uint64_t elementKey) {
        auto scope = DECLARE_THROW_SCOPE(vm);

        if (canSkipIntermediatePromise(globalObject, value)) {
            globalContext->setRemainingElementsCount(globalContext->remainingElementsCount() + 1);
            scope.release();
            globalObject->queueMicrotask(vm, resolveJob, static_cast<uint8_t>(JSPromise::Status::Fulfilled), globalContext, value, jsNumber(elementKey));
            return;
        }

        JSPromise* nextPromise = JSPromise::resolvedPromise(globalObject, value);
        RETURN_IF_EXCEPTION(scope, void());

        globalContext->setRemainingElementsCount(globalContext->remainingElementsCount() + 1);

        if (nextPromise->isThenFastAndNonObservable()) [[likely]] {
            auto* constructor = promiseSpeciesConstructor(globalObject, nextPromise);
            RETURN_IF_EXCEPTION(scope, void());
            if (constructor == globalObject->promiseConstructor()) [[likely]] {
                scope.release();
                nextPromise->performPromiseThenWithInternalMicrotask(vm, resolveJob, globalContext, jsNumber(elementKey));
                return;
            }
        }

        JSValue then = nextPromise->get(globalObject, vm.propertyNames->then);
        RETURN_IF_EXCEPTION(scope, void());
        CallData thenCallData = getCallDataInline(then);
        if (thenCallData.type == CallData::Type::None) [[unlikely]] {
            throwTypeError(globalObject, scope, "then is not a function"_s);
            return;
        }

        JSPromiseCombinatorsContext* context = JSPromiseCombinatorsContext::create(vm, globalContext, elementKey);

        JSObject* onFulfilled;
        JSObject* onRejected;
        if constexpr (variant == PromiseKeyedCombinatorVariant::All) {
            auto* resolveElement = JSFunctionWithFields::create(vm, globalObject, vm.promiseAllKeyedFulfillFunctionExecutable());
            resolveElement->setField(vm, JSFunctionWithFields::Field::PromiseAllContext, context);
            onFulfilled = resolveElement;
            if (!firstRejectFunction)
                firstRejectFunction = promise->createFirstRejectFunction(vm, globalObject);
            onRejected = firstRejectFunction;
        } else
            std::tie(onFulfilled, onRejected) = createPromiseAllSettledKeyedElementFunctions(vm, globalObject, context, vm.promiseAllSettledKeyedFulfillFunctionExecutable(), vm.promiseAllSettledKeyedRejectFunctionExecutable());

        MarkedArgumentBuffer thenArguments;
        thenArguments.append(onFulfilled);
        thenArguments.append(onRejected);
        ASSERT(!thenArguments.hasOverflowed());
        scope.release();
        call(globalObject, then, thenCallData, nextPromise, thenArguments);
    });

    if (scope.exception()) [[unlikely]] {
        callReject();
        return JSValue::encode(promise);
    }

    uint64_t count = globalContext->remainingElementsCount() - 1;
    globalContext->setRemainingElementsCount(count);
    if (!count) {
        scope.release();
        promise->resolve(globalObject, vm, resultObject);
        if (scope.exception()) [[unlikely]] {
            callReject();
            return JSValue::encode(promise);
        }
    }

    return JSValue::encode(promise);
}

JSC_DEFINE_HOST_FUNCTION(promiseConstructorFuncAllKeyed, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return promiseAllKeyedImpl<PromiseKeyedCombinatorVariant::All>(globalObject, callFrame);
}

JSC_DEFINE_HOST_FUNCTION(promiseConstructorFuncAllSettledKeyed, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return promiseAllKeyedImpl<PromiseKeyedCombinatorVariant::AllSettled>(globalObject, callFrame);
}

JSC_DEFINE_HOST_FUNCTION(promiseAllKeyedFulfillFunction, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    auto* context = takePromiseAllElementContext(globalObject->vm(), uncheckedDowncast<JSFunctionWithFields>(callFrame->jsCallee()));
    if (!context) [[unlikely]]
        return JSValue::encode(jsUndefined());
    resolveKeyedPromiseCombinatorElement(globalObject, context->globalContext(), context->index(), callFrame->argument(0));
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(promiseAllKeyedSlowFulfillFunction, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    auto* context = takePromiseAllElementContext(globalObject->vm(), uncheckedDowncast<JSFunctionWithFields>(callFrame->jsCallee()));
    if (!context) [[unlikely]]
        return JSValue::encode(jsUndefined());
    resolveKeyedPromiseCombinatorElementSlow(globalObject, context->globalContext(), context->index(), callFrame->argument(0));
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(promiseAllSettledKeyedFulfillFunction, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    auto* context = takePromiseAllSettledElementContext(globalObject->vm(), uncheckedDowncast<JSFunctionWithFields>(callFrame->jsCallee()));
    if (!context) [[unlikely]]
        return JSValue::encode(jsUndefined());
    resolveKeyedPromiseCombinatorElement(globalObject, context->globalContext(), context->index(), createPromiseAllSettledFulfilledResult(globalObject, callFrame->argument(0)));
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(promiseAllSettledKeyedRejectFunction, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    auto* context = takePromiseAllSettledElementContext(globalObject->vm(), uncheckedDowncast<JSFunctionWithFields>(callFrame->jsCallee()));
    if (!context) [[unlikely]]
        return JSValue::encode(jsUndefined());
    resolveKeyedPromiseCombinatorElement(globalObject, context->globalContext(), context->index(), createPromiseAllSettledRejectedResult(globalObject, callFrame->argument(0)));
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(promiseAllSettledKeyedSlowFulfillFunction, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    auto* context = takePromiseAllSettledElementContext(globalObject->vm(), uncheckedDowncast<JSFunctionWithFields>(callFrame->jsCallee()));
    if (!context) [[unlikely]]
        return JSValue::encode(jsUndefined());
    resolveKeyedPromiseCombinatorElementSlow(globalObject, context->globalContext(), context->index(), createPromiseAllSettledFulfilledResult(globalObject, callFrame->argument(0)));
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(promiseAllSettledKeyedSlowRejectFunction, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    auto* context = takePromiseAllSettledElementContext(globalObject->vm(), uncheckedDowncast<JSFunctionWithFields>(callFrame->jsCallee()));
    if (!context) [[unlikely]]
        return JSValue::encode(jsUndefined());
    resolveKeyedPromiseCombinatorElementSlow(globalObject, context->globalContext(), context->index(), createPromiseAllSettledRejectedResult(globalObject, callFrame->argument(0)));
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(promiseConstructorFuncIsPromise, (JSGlobalObject*, CallFrame* callFrame))
{
    return JSValue::encode(jsBoolean(callFrame->argument(0).inherits<JSPromise>()));
}

} // namespace JSC
