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
#include "JSPromisePrototype.h"

#include "BuiltinNames.h"
#include "CallData.h"
#include "JSCInlines.h"
#include "JSFunctionWithFields.h"
#include "JSInternalPromise.h"
#include "JSPromise.h"
#include "JSPromiseCombinatorsGlobalContext.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSPromisePrototype);

static JSC_DECLARE_HOST_FUNCTION(promiseProtoFuncCatch);
static JSC_DECLARE_HOST_FUNCTION(promiseProtoFuncFinally);

static JSC_DECLARE_HOST_FUNCTION(promiseFinallyThenFinallyFunc);
static JSC_DECLARE_HOST_FUNCTION(promiseFinallyCatchFinallyFunc);
static JSC_DECLARE_HOST_FUNCTION(promiseFinallyValueThunkFunc);
static JSC_DECLARE_HOST_FUNCTION(promiseFinallyThrowerFunc);

}

#include "JSPromisePrototype.lut.h"

namespace JSC {

const ClassInfo JSPromisePrototype::s_info = { "Promise"_s, &Base::s_info, &promisePrototypeTable, nullptr, CREATE_METHOD_TABLE(JSPromisePrototype) };

/* Source for JSPromisePrototype.lut.h
@begin promisePrototypeTable
  finally      promiseProtoFuncFinally  DontEnum|Function 1
@end
*/

JSPromisePrototype* JSPromisePrototype::create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
{
    JSPromisePrototype* object = new (NotNull, allocateCell<JSPromisePrototype>(vm)) JSPromisePrototype(vm, structure);
    object->finishCreation(vm, globalObject);
    object->addOwnInternalSlots(vm, globalObject);
    return object;
}

Structure* JSPromisePrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

JSPromisePrototype::JSPromisePrototype(VM& vm, Structure* structure)
    : JSNonFinalObject(vm, structure)
{
}

void JSPromisePrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().thenPublicName(), globalObject->promiseProtoThenFunction(), static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_NATIVE_INTRINSIC_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->catchKeyword, promiseProtoFuncCatch, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public, PromisePrototypeCatchIntrinsic);
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

void JSPromisePrototype::addOwnInternalSlots(VM& vm, JSGlobalObject* globalObject)
{
    putDirectWithoutTransition(vm, vm.propertyNames->builtinNames().thenPrivateName(), globalObject->promiseProtoThenFunction(), PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
}

bool promiseSpeciesWatchpointIsValid(VM& vm, JSPromise* thisObject)
{
    auto* structure = thisObject->structure();
    JSGlobalObject* globalObject = structure->globalObject();
    if (globalObject->promiseSpeciesWatchpointSet().state() != IsWatched) [[unlikely]] {
        if (structure->classInfoForCells() == JSInternalPromise::info())
            return true;
        return false;
    }

    if (structure == globalObject->promiseStructure())
        return true;

    if (structure->classInfoForCells() == JSInternalPromise::info())
        return true;

    ASSERT(globalObject->promiseSpeciesWatchpointSet().state() != ClearWatchpoint);
    auto* promisePrototype = globalObject->promisePrototype();
    if (promisePrototype != structure->storedPrototype(thisObject))
        return false;

    if (!thisObject->hasCustomProperties())
        return true;

    return thisObject->getDirectOffset(vm, vm.propertyNames->constructor) == invalidOffset;
}

JSC_DEFINE_HOST_FUNCTION(promiseProtoFuncThen, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue().toThis(globalObject, ECMAMode::strict());

    JSValue onFulfilled = callFrame->argument(0);
    JSValue onRejected = callFrame->argument(1);

    auto* promise = jsDynamicCast<JSPromise*>(thisValue);
    if (!promise) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "|this| is not a Promise");

    RELEASE_AND_RETURN(scope, JSValue::encode(promise->then(globalObject, onFulfilled, onRejected)));
}

JSC_DEFINE_HOST_FUNCTION(promiseProtoFuncCatch, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue().toThis(globalObject, ECMAMode::strict());
    JSValue onRejected = callFrame->argument(0);

    if (auto* promise = jsDynamicCast<JSPromise*>(thisValue); promise && promise->isThenFastAndNonObservable()) [[likely]]
        RELEASE_AND_RETURN(scope, JSValue::encode(promise->then(globalObject, jsUndefined(), onRejected)));

    JSValue then = thisValue.get(globalObject, vm.propertyNames->then);
    RETURN_IF_EXCEPTION(scope, { });

    auto thenCallData = getCallDataInline(then);
    if (thenCallData.type == CallData::Type::None) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "|this|.then is not a function"_s);
    MarkedArgumentBuffer thenArguments;
    thenArguments.append(jsUndefined());
    thenArguments.append(onRejected);
    ASSERT(!thenArguments.hasOverflowed());
    RELEASE_AND_RETURN(scope, JSValue::encode(call(globalObject, then, thenCallData, thisValue, thenArguments)));
}

JSC_DEFINE_HOST_FUNCTION(promiseFinallyValueThunkFunc, (JSGlobalObject*, CallFrame* callFrame))
{
    auto* callee = jsCast<JSFunctionWithFields*>(callFrame->jsCallee());
    JSValue value = callee->getField(JSFunctionWithFields::Field::ResolvingPromise);
    return JSValue::encode(value);
}

JSC_DEFINE_HOST_FUNCTION(promiseFinallyThrowerFunc, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto* callee = jsCast<JSFunctionWithFields*>(callFrame->jsCallee());
    JSValue reason = callee->getField(JSFunctionWithFields::Field::ResolvingPromise);
    return throwVMError(globalObject, scope, reason);
}

JSC_DEFINE_HOST_FUNCTION(promiseFinallyThenFinallyFunc, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* callee = jsCast<JSFunctionWithFields*>(callFrame->jsCallee());
    JSValue onFinally = callee->getField(JSFunctionWithFields::Field::ResolvingPromise);
    JSObject* constructor = jsCast<JSObject*>(callee->getField(JSFunctionWithFields::Field::ResolvingOther));
    JSValue value = callFrame->argument(0);

    JSValue result = call(globalObject, onFinally, jsUndefined(), ArgList { }, "onFinally is not a function"_s);
    RETURN_IF_EXCEPTION(scope, { });

    auto* resolvedPromise = JSPromise::promiseResolve(globalObject, constructor, result);
    RETURN_IF_EXCEPTION(scope, { });

    NativeExecutable* thunkExecutable = vm.getHostFunction(promiseFinallyValueThunkFunc, ImplementationVisibility::Public, callHostFunctionAsConstructor, nullString());
    auto* valueThunk = JSFunctionWithFields::create(vm, globalObject, thunkExecutable, 0, nullString());
    valueThunk->setField(vm, JSFunctionWithFields::Field::ResolvingPromise, value);

    JSValue then = resolvedPromise->get(globalObject, vm.propertyNames->then);
    RETURN_IF_EXCEPTION(scope, { });
    auto thenCallData = getCallDataInline(then);
    if (thenCallData.type == CallData::Type::None)
        return throwVMTypeError(globalObject, scope, "|this|.then is not a function"_s);
    MarkedArgumentBuffer thenArgs;
    thenArgs.append(valueThunk);
    thenArgs.append(jsUndefined());
    ASSERT(!thenArgs.hasOverflowed());
    RELEASE_AND_RETURN(scope, JSValue::encode(call(globalObject, then, thenCallData, resolvedPromise, thenArgs)));
}

JSC_DEFINE_HOST_FUNCTION(promiseFinallyCatchFinallyFunc, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* callee = jsCast<JSFunctionWithFields*>(callFrame->jsCallee());
    JSValue onFinally = callee->getField(JSFunctionWithFields::Field::ResolvingPromise);
    JSObject* constructor = jsCast<JSObject*>(callee->getField(JSFunctionWithFields::Field::ResolvingOther));
    JSValue reason = callFrame->argument(0);

    JSValue result = call(globalObject, onFinally, jsUndefined(), ArgList { }, "onFinally is not a function"_s);
    RETURN_IF_EXCEPTION(scope, { });

    auto* resolvedPromise = JSPromise::promiseResolve(globalObject, constructor, result);
    RETURN_IF_EXCEPTION(scope, { });

    NativeExecutable* throwerExecutable = vm.getHostFunction(promiseFinallyThrowerFunc, ImplementationVisibility::Public, callHostFunctionAsConstructor, nullString());
    auto* thrower = JSFunctionWithFields::create(vm, globalObject, throwerExecutable, 0, nullString());
    thrower->setField(vm, JSFunctionWithFields::Field::ResolvingPromise, reason);

    JSValue then = resolvedPromise->get(globalObject, vm.propertyNames->then);
    RETURN_IF_EXCEPTION(scope, { });
    auto thenCallData = getCallDataInline(then);
    if (thenCallData.type == CallData::Type::None)
        return throwVMTypeError(globalObject, scope, "|this|.then is not a function"_s);
    MarkedArgumentBuffer thenArgs;
    thenArgs.append(thrower);
    thenArgs.append(jsUndefined());
    ASSERT(!thenArgs.hasOverflowed());
    RELEASE_AND_RETURN(scope, JSValue::encode(call(globalObject, then, thenCallData, resolvedPromise, thenArgs)));
}

JSC_DEFINE_HOST_FUNCTION(promiseProtoFuncFinally, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue().toThis(globalObject, ECMAMode::strict());
    if (!thisValue.isObject())
        return throwVMTypeError(globalObject, scope, "|this| is not an object"_s);

    JSValue onFinally = callFrame->argument(0);

    if (!onFinally.isCallable()) {
        if (auto* promise = jsDynamicCast<JSPromise*>(thisValue); promise && promise->isThenFastAndNonObservable()) [[likely]]
            RELEASE_AND_RETURN(scope, JSValue::encode(promise->then(globalObject, onFinally, onFinally)));

        JSValue then = thisValue.get(globalObject, vm.propertyNames->then);
        RETURN_IF_EXCEPTION(scope, { });

        auto thenCallData = getCallDataInline(then);
        if (thenCallData.type == CallData::Type::None) [[unlikely]]
            return throwVMTypeError(globalObject, scope, "|this|.then is not a function"_s);
        MarkedArgumentBuffer thenArguments;
        thenArguments.append(onFinally);
        thenArguments.append(onFinally);
        ASSERT(!thenArguments.hasOverflowed());
        RELEASE_AND_RETURN(scope, JSValue::encode(call(globalObject, then, thenCallData, thisValue, thenArguments)));
    }

    auto* promise = jsDynamicCast<JSPromise*>(thisValue);
    if (promise && promise->isThenFastAndNonObservable() && promiseSpeciesWatchpointIsValid(vm, promise)) [[likely]] {
        JSPromise* resultPromise = JSPromise::create(vm, globalObject->promiseStructure());
        auto* context = JSPromiseCombinatorsGlobalContext::create(vm, resultPromise, onFinally, jsUndefined());
        promise->performPromiseThenWithInternalMicrotask(vm, globalObject, InternalMicrotask::PromiseFinallyReactionJob, resultPromise, context);
        return JSValue::encode(resultPromise);
    }

    JSObject* constructor = promiseSpeciesConstructor(globalObject, asObject(thisValue));
    RETURN_IF_EXCEPTION(scope, { });

    JSValue then = thisValue.get(globalObject, vm.propertyNames->then);
    RETURN_IF_EXCEPTION(scope, { });

    auto thenCallData = getCallDataInline(then);
    if (thenCallData.type == CallData::Type::None) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "|this|.then is not a function"_s);

    NativeExecutable* thenFinallyExecutable = vm.getHostFunction(promiseFinallyThenFinallyFunc, ImplementationVisibility::Public, callHostFunctionAsConstructor, nullString());
    auto* thenFinally = JSFunctionWithFields::create(vm, globalObject, thenFinallyExecutable, 1, nullString());
    thenFinally->setField(vm, JSFunctionWithFields::Field::ResolvingPromise, onFinally);
    thenFinally->setField(vm, JSFunctionWithFields::Field::ResolvingOther, constructor);

    NativeExecutable* catchFinallyExecutable = vm.getHostFunction(promiseFinallyCatchFinallyFunc, ImplementationVisibility::Public, callHostFunctionAsConstructor, nullString());
    auto* catchFinally = JSFunctionWithFields::create(vm, globalObject, catchFinallyExecutable, 1, nullString());
    catchFinally->setField(vm, JSFunctionWithFields::Field::ResolvingPromise, onFinally);
    catchFinally->setField(vm, JSFunctionWithFields::Field::ResolvingOther, constructor);

    MarkedArgumentBuffer thenArguments;
    thenArguments.append(thenFinally);
    thenArguments.append(catchFinally);
    ASSERT(!thenArguments.hasOverflowed());
    RELEASE_AND_RETURN(scope, JSValue::encode(call(globalObject, then, thenCallData, thisValue, thenArguments)));
}

} // namespace JSC
