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
#include "JSPromise.h"

#include "BuiltinNames.h"
#include "DeferredWorkTimer.h"
#include "ErrorInstance.h"
#include "GlobalObjectMethodTable.h"
#include "JSCInlines.h"
#include "JSFunctionWithFields.h"
#include "JSInternalFieldObjectImplInlines.h"
#include "JSInternalPromise.h"
#include "JSInternalPromiseConstructor.h"
#include "JSInternalPromisePrototype.h"
#include "JSMicrotask.h"
#include "JSPromiseCombinatorsContext.h"
#include "JSPromiseCombinatorsGlobalContext.h"
#include "JSPromiseConstructor.h"
#include "JSPromisePrototype.h"
#include "JSPromiseReaction.h"
#include "Microtask.h"
#include "ObjectConstructor.h"

namespace JSC {

const ClassInfo JSPromise::s_info = { "Promise"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSPromise) };

JSPromise* JSPromise::create(VM& vm, Structure* structure)
{
    JSPromise* promise = new (NotNull, allocateCell<JSPromise>(vm)) JSPromise(vm, structure);
    promise->finishCreation(vm);
    return promise;
}

JSPromise* JSPromise::createWithInitialValues(VM& vm, Structure* structure)
{
    return create(vm, structure);
}

Structure* JSPromise::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(JSPromiseType, StructureFlags), info());
}

JSPromise::JSPromise(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void JSPromise::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    auto values = initialValues();
    for (unsigned index = 0; index < values.size(); ++index)
        Base::internalField(index).set(vm, this, values[index]);
}

template<typename Visitor>
void JSPromise::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    auto* thisObject = jsCast<JSPromise*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
}

DEFINE_VISIT_CHILDREN(JSPromise);

JSValue JSPromise::createNewPromiseCapability(JSGlobalObject* globalObject, JSValue constructor)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto [promise, resolve, reject] = newPromiseCapability(globalObject, constructor);
    RETURN_IF_EXCEPTION(scope, { });
    return createPromiseCapability(vm, globalObject, promise, resolve, reject);
}

JSValue JSPromise::createPromiseCapability(VM& vm, JSGlobalObject* globalObject, JSObject* promise, JSObject* resolve, JSObject* reject)
{
    auto* capability = constructEmptyObject(vm, globalObject->promiseCapabilityObjectStructure());
    capability->putDirectOffset(vm, promiseCapabilityResolvePropertyOffset, resolve);
    capability->putDirectOffset(vm, promiseCapabilityRejectPropertyOffset, reject);
    capability->putDirectOffset(vm, promiseCapabilityPromisePropertyOffset, promise);
    return capability;
}

std::tuple<JSObject*, JSObject*, JSObject*> JSPromise::newPromiseCapability(JSGlobalObject* globalObject, JSValue constructor)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (constructor == globalObject->promiseConstructor()) {
        auto* promise = JSPromise::create(vm, globalObject->promiseStructure());
        auto [resolve, reject] = promise->createFirstResolvingFunctions(vm, globalObject);
        return { promise, resolve, reject };
    }

    if (constructor == globalObject->internalPromiseConstructor()) {
        auto* promise = JSInternalPromise::create(vm, globalObject->internalPromiseStructure());
        auto [resolve, reject] = promise->createFirstResolvingFunctions(vm, globalObject);
        return { promise, resolve, reject };
    }

    auto* executor = JSFunctionWithFields::create(vm, globalObject, vm.promiseCapabilityExecutorExecutable(), 2, emptyString());
    executor->setField(vm, JSFunctionWithFields::Field::ExecutorResolve, jsUndefined());
    executor->setField(vm, JSFunctionWithFields::Field::ExecutorReject, jsUndefined());

    MarkedArgumentBuffer args;
    args.append(executor);
    ASSERT(!args.hasOverflowed());
    JSObject* newObject = construct(globalObject, constructor, args, "argument is not a constructor"_s);
    RETURN_IF_EXCEPTION(scope, { });

    JSValue resolve = executor->getField(JSFunctionWithFields::Field::ExecutorResolve);
    JSValue reject = executor->getField(JSFunctionWithFields::Field::ExecutorReject);
    if (!resolve.isCallable()) [[unlikely]] {
        throwTypeError(globalObject, scope, "executor did not take a resolve function"_s);
        return { };
    }

    if (!reject.isCallable()) [[unlikely]] {
        throwTypeError(globalObject, scope, "executor did not take a reject function"_s);
        return { };
    }

    return { newObject, asObject(resolve), asObject(reject) };
}

JSPromise::DeferredData JSPromise::createDeferredData(JSGlobalObject* globalObject, JSPromiseConstructor* promiseConstructor)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto [ promiseCapability, resolveCapability, rejectCapability ] = newPromiseCapability(globalObject, promiseConstructor);
    RETURN_IF_EXCEPTION(scope, { });
    auto* promise = jsDynamicCast<JSPromise*>(promiseCapability);
    auto* resolve = jsDynamicCast<JSFunction*>(resolveCapability);
    auto* reject  = jsDynamicCast<JSFunction*>(rejectCapability);
    if (promise && resolve && reject)
        return DeferredData { promise, resolve, reject };

    throwTypeError(globalObject, scope, "constructor is producing a bad value"_s);
    return { };
}

JSPromise* JSPromise::resolvedPromise(JSGlobalObject* globalObject, JSValue value)
{
    return jsCast<JSPromise*>(promiseResolve(globalObject, globalObject->promiseConstructor(), value));
}

JSPromise* JSPromise::rejectedPromise(JSGlobalObject* globalObject, JSValue value)
{
    VM& vm = globalObject->vm();
    JSPromise* promise = JSPromise::create(vm, globalObject->promiseStructure());
    promise->reject(vm, globalObject, value);
    return promise;
}

void JSPromise::resolve(JSGlobalObject* globalObject, JSValue value)
{
    VM& vm = globalObject->vm();
    uint32_t flags = this->flags();
    ASSERT(!value.inherits<Exception>());
    if (!(flags & isFirstResolvingFunctionCalledFlag)) {
        internalField(Field::Flags).set(vm, this, jsNumber(flags | isFirstResolvingFunctionCalledFlag));
        resolvePromise(globalObject, value);
    }
}

void JSPromise::reject(VM& vm, JSGlobalObject* globalObject, JSValue value)
{
    uint32_t flags = this->flags();
    ASSERT(!value.inherits<Exception>());
    if (!(flags & isFirstResolvingFunctionCalledFlag)) {
        internalField(Field::Flags).set(vm, this, jsNumber(flags | isFirstResolvingFunctionCalledFlag));
        rejectPromise(vm, globalObject, value);
    }
}

void JSPromise::fulfill(VM& vm, JSGlobalObject* globalObject, JSValue value)
{
    uint32_t flags = this->flags();
    ASSERT(!value.inherits<Exception>());
    if (!(flags & isFirstResolvingFunctionCalledFlag)) {
        internalField(Field::Flags).set(vm, this, jsNumber(flags | isFirstResolvingFunctionCalledFlag));
        fulfillPromise(vm, globalObject, value);
    }
}

void JSPromise::performPromiseThenExported(VM& vm, JSGlobalObject* globalObject, JSValue onFulfilled, JSValue onRejected, JSValue promiseOrCapability)
{
    return performPromiseThen(vm, globalObject, onFulfilled, onRejected, promiseOrCapability);
}

void JSPromise::rejectAsHandled(VM& vm, JSGlobalObject* lexicalGlobalObject, JSValue value)
{
    // Setting isHandledFlag before calling reject since this removes round-trip between JSC and PromiseRejectionTracker, and it does not show an user-observable behavior.
    if (!(flags() & isFirstResolvingFunctionCalledFlag)) {
        markAsHandled();
        reject(vm, lexicalGlobalObject, value);
    }
}

void JSPromise::reject(VM& vm, JSGlobalObject* lexicalGlobalObject, Exception* reason)
{
    reject(vm, lexicalGlobalObject, reason->value());
}

void JSPromise::rejectAsHandled(VM& vm, JSGlobalObject* lexicalGlobalObject, Exception* reason)
{
    rejectAsHandled(vm, lexicalGlobalObject, reason->value());
}

JSPromise* JSPromise::rejectWithCaughtException(JSGlobalObject* globalObject, ThrowScope& scope)
{
    VM& vm = globalObject->vm();
    Exception* exception = scope.exception();
    ASSERT(exception);
    TRY_CLEAR_EXCEPTION(scope, nullptr);
    scope.release();
    reject(vm, globalObject, exception->value());
    return this;
}

void JSPromise::performPromiseThen(VM& vm, JSGlobalObject* globalObject, JSValue onFulfilled, JSValue onRejected, JSValue promiseOrCapability)
{
    if (!onFulfilled.isCallable())
        onFulfilled = globalObject->promiseEmptyOnFulfilledFunction();

    if (!onRejected.isCallable())
        onRejected = globalObject->promiseEmptyOnRejectedFunction();

    JSValue reactionsOrResult = this->reactionsOrResult();
    switch (status()) {
    case JSPromise::Status::Pending: {
        auto* reaction = JSPromiseReaction::create(vm, promiseOrCapability, onFulfilled, onRejected, jsUndefined(), jsDynamicCast<JSPromiseReaction*>(reactionsOrResult));
        setReactionsOrResult(vm, reaction);
        break;
    }
    case JSPromise::Status::Rejected: {
        if (!isHandled())
            globalObject->globalObjectMethodTable()->promiseRejectionTracker(globalObject, this, JSPromiseRejectionOperation::Handle);
        globalObject->queueMicrotask(InternalMicrotask::PromiseReactionJob, static_cast<uint8_t>(Status::Rejected), promiseOrCapability, onRejected, reactionsOrResult);
        break;
    }
    case JSPromise::Status::Fulfilled: {
        globalObject->queueMicrotask(InternalMicrotask::PromiseReactionJob, static_cast<uint8_t>(Status::Fulfilled), promiseOrCapability, onFulfilled, reactionsOrResult);
        break;
    }
    }
    markAsHandled();
}

void JSPromise::performPromiseThenWithInternalMicrotask(VM& vm, JSGlobalObject* globalObject, InternalMicrotask task, JSValue promise, JSValue context)
{
    JSValue reactionsOrResult = this->reactionsOrResult();
    switch (status()) {
    case JSPromise::Status::Pending: {
        JSValue encodedTask = jsNumber(static_cast<int32_t>(task));
        auto* reaction = JSPromiseReaction::create(vm, promise, encodedTask, encodedTask, context, jsDynamicCast<JSPromiseReaction*>(reactionsOrResult));
        setReactionsOrResult(vm, reaction);
        break;
    }
    case JSPromise::Status::Rejected: {
        if (!isHandled())
            globalObject->globalObjectMethodTable()->promiseRejectionTracker(globalObject, this, JSPromiseRejectionOperation::Handle);
        globalObject->queueMicrotask(task, static_cast<uint8_t>(Status::Rejected), promise, reactionsOrResult, context);
        break;
    }
    case JSPromise::Status::Fulfilled: {
        globalObject->queueMicrotask(task, static_cast<uint8_t>(Status::Fulfilled), promise, reactionsOrResult, context);
        break;
    }
    }
    markAsHandled();
}

bool isDefinitelyNonThenable(JSObject* object, JSGlobalObject* globalObject)
{
    if (!globalObject->promiseThenWatchpointSet().isStillValid()) [[unlikely]]
        return false;

    auto* structure = object->structure();
    if (globalObject->iteratorResultObjectStructure() == structure)
        return true;

    while (structure) {
        if (structure->hasSpecialProperties())
            return false;
        if (structure->typeInfo().overridesGetPrototype())
            return false;
        if (!structure->hasMonoProto())
            return false;
        structure = structure->storedPrototypeStructure();
    }
    return true;
}

void JSPromise::rejectPromise(VM& vm, JSGlobalObject* globalObject, JSValue argument)
{
    ASSERT(status() == Status::Pending);
    uint32_t flags = this->flags();
    auto* reactions = jsDynamicCast<JSPromiseReaction*>(this->reactionsOrResult());
    internalField(Field::Flags).set(vm, this, jsNumber(flags | static_cast<uint32_t>(Status::Rejected)));
    internalField(Field::ReactionsOrResult).set(vm, this, argument);

    if (!isHandled())
        globalObject->globalObjectMethodTable()->promiseRejectionTracker(globalObject, this, JSPromiseRejectionOperation::Reject);

    if (!reactions)
        return;
    triggerPromiseReactions(vm, globalObject, Status::Rejected, reactions, argument);
}

void JSPromise::fulfillPromise(VM& vm, JSGlobalObject* globalObject, JSValue argument)
{
    ASSERT(status() == Status::Pending);
    uint32_t flags = this->flags();
    auto* reactions = jsDynamicCast<JSPromiseReaction*>(this->reactionsOrResult());
    internalField(Field::Flags).set(vm, this, jsNumber(flags | static_cast<uint32_t>(Status::Fulfilled)));
    internalField(Field::ReactionsOrResult).set(vm, this, argument);
    if (!reactions)
        return;
    triggerPromiseReactions(vm, globalObject, Status::Fulfilled, reactions, argument);
}

void JSPromise::resolvePromise(JSGlobalObject* globalObject, JSValue resolution)
{
    VM& vm = globalObject->vm();

    if (resolution == this) [[unlikely]] {
        Structure* errorStructure = globalObject->errorStructure(ErrorType::TypeError);
        auto* error = ErrorInstance::create(vm, errorStructure, "Cannot resolve a promise with itself"_s, jsUndefined(), nullptr, TypeNothing, ErrorType::TypeError, false);
        return rejectPromise(vm, globalObject, error);
    }

    if (!resolution.isObject())
        return fulfillPromise(vm, globalObject, resolution);

    auto* resolutionObject = asObject(resolution);
    if (resolutionObject->inherits<JSPromise>()) {
        auto* promise = jsCast<JSPromise*>(resolutionObject);
        if (promise->isThenFastAndNonObservable())
            return globalObject->queueMicrotask(InternalMicrotask::PromiseResolveThenableJobFast, 0, resolutionObject, this, jsUndefined());
    }

    if (isDefinitelyNonThenable(resolutionObject, globalObject))
        return fulfillPromise(vm, globalObject, resolution);

    JSValue then;
    JSValue error;
    {
        auto catchScope = DECLARE_TOP_EXCEPTION_SCOPE(vm);
        then = resolutionObject->get(globalObject, vm.propertyNames->then);
        if (catchScope.exception()) [[unlikely]] {
            error = catchScope.exception()->value();
            if (!catchScope.clearExceptionExceptTermination()) [[unlikely]]
                return;
        }
    }
    if (error) [[unlikely]]
        return rejectPromise(vm, globalObject, error);

    if (!then.isCallable()) [[likely]]
        return fulfillPromise(vm, globalObject, resolutionObject);

    return globalObject->queueMicrotask(InternalMicrotask::PromiseResolveThenableJob, 0, resolutionObject, then, this);
}

JSC_DEFINE_HOST_FUNCTION(promiseResolvingFunctionResolve, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();

    auto* callee = jsCast<JSFunctionWithFields*>(callFrame->jsCallee());
    auto* other = jsDynamicCast<JSFunctionWithFields*>(callee->getField(JSFunctionWithFields::Field::ResolvingOther));
    if (!other) [[unlikely]]
        return JSValue::encode(jsUndefined());

    callee->setField(vm, JSFunctionWithFields::Field::ResolvingOther, jsNull());
    other->setField(vm, JSFunctionWithFields::Field::ResolvingOther, jsNull());

    auto* promise = jsCast<JSPromise*>(callee->getField(JSFunctionWithFields::Field::ResolvingPromise));
    JSValue argument = callFrame->argument(0);

    promise->resolvePromise(globalObject, argument);
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(promiseResolvingFunctionReject, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();

    auto* callee = jsCast<JSFunctionWithFields*>(callFrame->jsCallee());
    auto* other = jsDynamicCast<JSFunctionWithFields*>(callee->getField(JSFunctionWithFields::Field::ResolvingOther));
    if (!other) [[unlikely]]
        return JSValue::encode(jsUndefined());

    callee->setField(vm, JSFunctionWithFields::Field::ResolvingOther, jsNull());
    other->setField(vm, JSFunctionWithFields::Field::ResolvingOther, jsNull());

    auto* promise = jsCast<JSPromise*>(callee->getField(JSFunctionWithFields::Field::ResolvingPromise));
    JSValue argument = callFrame->argument(0);

    promise->rejectPromise(vm, globalObject, argument);
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(promiseFirstResolvingFunctionResolve, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    auto* callee = jsCast<JSFunctionWithFields*>(callFrame->jsCallee());
    auto* promise = jsCast<JSPromise*>(callee->getField(JSFunctionWithFields::Field::FirstResolvingPromise));
    JSValue argument = callFrame->argument(0);

    promise->resolve(globalObject, argument);
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(promiseFirstResolvingFunctionReject, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    auto* callee = jsCast<JSFunctionWithFields*>(callFrame->jsCallee());
    auto* promise = jsCast<JSPromise*>(callee->getField(JSFunctionWithFields::Field::FirstResolvingPromise));
    JSValue argument = callFrame->argument(0);

    promise->reject(globalObject->vm(), globalObject, argument);
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(promiseResolvingFunctionResolveWithInternalMicrotask, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();

    auto* callee = jsCast<JSFunctionWithFields*>(callFrame->jsCallee());
    auto* other = jsDynamicCast<JSFunctionWithFields*>(callee->getField(JSFunctionWithFields::Field::ResolvingWithInternalMicrotaskOther));
    if (!other) [[unlikely]]
        return JSValue::encode(jsUndefined());

    callee->setField(vm, JSFunctionWithFields::Field::ResolvingWithInternalMicrotaskOther, jsNull());
    other->setField(vm, JSFunctionWithFields::Field::ResolvingWithInternalMicrotaskOther, jsNull());

    auto* context = jsCast<JSPromiseCombinatorsGlobalContext*>(callee->getField(JSFunctionWithFields::Field::ResolvingWithInternalMicrotaskContext));
    JSValue argument = callFrame->argument(0);
    JSValue onFulfilled = context->promise();
    JSPromise::resolveWithInternalMicrotask(globalObject, argument, static_cast<InternalMicrotask>(onFulfilled.asInt32()), context->remainingElementsCount());
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(promiseResolvingFunctionRejectWithInternalMicrotask, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();

    auto* callee = jsCast<JSFunctionWithFields*>(callFrame->jsCallee());
    auto* other = jsDynamicCast<JSFunctionWithFields*>(callee->getField(JSFunctionWithFields::Field::ResolvingWithInternalMicrotaskOther));
    if (!other) [[unlikely]]
        return JSValue::encode(jsUndefined());

    callee->setField(vm, JSFunctionWithFields::Field::ResolvingWithInternalMicrotaskOther, jsNull());
    other->setField(vm, JSFunctionWithFields::Field::ResolvingWithInternalMicrotaskOther, jsNull());

    auto* context = jsCast<JSPromiseCombinatorsGlobalContext*>(callee->getField(JSFunctionWithFields::Field::ResolvingWithInternalMicrotaskContext));
    JSValue argument = callFrame->argument(0);
    JSValue onFulfilled = context->promise();
    JSPromise::rejectWithInternalMicrotask(globalObject, argument, static_cast<InternalMicrotask>(onFulfilled.asInt32()), context->remainingElementsCount());
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(promiseCapabilityExecutor, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* callee = jsCast<JSFunctionWithFields*>(callFrame->jsCallee());
    JSValue resolve = callee->getField(JSFunctionWithFields::Field::ExecutorResolve);
    if (!resolve.isUndefined()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "resolve function is already set"_s);

    JSValue reject = callee->getField(JSFunctionWithFields::Field::ExecutorReject);
    if (!reject.isUndefined()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "reject function is already set"_s);

    callee->setField(vm, JSFunctionWithFields::Field::ExecutorResolve, callFrame->argument(0));
    callee->setField(vm, JSFunctionWithFields::Field::ExecutorReject, callFrame->argument(1));

    return JSValue::encode(jsUndefined());
}

std::tuple<JSFunction*, JSFunction*> JSPromise::createResolvingFunctions(VM& vm, JSGlobalObject* globalObject)
{
    auto* resolve = JSFunctionWithFields::create(vm, globalObject, vm.promiseResolvingFunctionResolveExecutable(), 1, nullString());
    auto* reject = JSFunctionWithFields::create(vm, globalObject, vm.promiseResolvingFunctionRejectExecutable(), 1, nullString());

    resolve->setField(vm, JSFunctionWithFields::Field::ResolvingPromise, this);
    resolve->setField(vm, JSFunctionWithFields::Field::ResolvingOther, reject);

    reject->setField(vm, JSFunctionWithFields::Field::ResolvingPromise, this);
    reject->setField(vm, JSFunctionWithFields::Field::ResolvingOther, resolve);

    return std::tuple { resolve, reject };
}

std::tuple<JSFunction*, JSFunction*> JSPromise::createFirstResolvingFunctions(VM& vm, JSGlobalObject* globalObject)
{
    auto* resolve = JSFunctionWithFields::create(vm, globalObject, vm.promiseFirstResolvingFunctionResolveExecutable(), 1, nullString());
    auto* reject = JSFunctionWithFields::create(vm, globalObject, vm.promiseFirstResolvingFunctionRejectExecutable(), 1, nullString());

    resolve->setField(vm, JSFunctionWithFields::Field::FirstResolvingPromise, this);
    reject->setField(vm, JSFunctionWithFields::Field::FirstResolvingPromise, this);

    return std::tuple { resolve, reject };
}

std::tuple<JSFunction*, JSFunction*> JSPromise::createResolvingFunctionsWithInternalMicrotask(VM& vm, JSGlobalObject* globalObject, InternalMicrotask task, JSValue context)
{
    JSValue encodedTask = jsNumber(static_cast<int32_t>(task));

    auto* resolve = JSFunctionWithFields::create(vm, globalObject, vm.promiseResolvingFunctionResolveWithInternalMicrotaskExecutable(), 1, nullString());
    auto* reject = JSFunctionWithFields::create(vm, globalObject, vm.promiseResolvingFunctionRejectWithInternalMicrotaskExecutable(), 1, nullString());

    auto* all = JSPromiseCombinatorsGlobalContext::create(vm, encodedTask, encodedTask, context);

    resolve->setField(vm, JSFunctionWithFields::Field::ResolvingWithInternalMicrotaskContext, all);
    resolve->setField(vm, JSFunctionWithFields::Field::ResolvingWithInternalMicrotaskOther, reject);

    reject->setField(vm, JSFunctionWithFields::Field::ResolvingWithInternalMicrotaskContext, all);
    reject->setField(vm, JSFunctionWithFields::Field::ResolvingWithInternalMicrotaskOther, resolve);

    return std::tuple { resolve, reject };
}

void JSPromise::triggerPromiseReactions(VM& vm, JSGlobalObject* globalObject, Status status, JSPromiseReaction* head, JSValue argument)
{
    if (!head)
        return;

    // Reverse the order of singly-linked-list.
    JSPromiseReaction* previous = nullptr;
    {
        auto* current = head;
        while (current) {
            auto* next = current->next();
            current->setNext(vm, previous);
            previous = current;
            current = next;
        }
    }
    head = previous;

    bool isResolved = status == JSPromise::Status::Fulfilled;
    auto* current = head;
    while (current) {
        JSValue promise = current->promise();
        JSValue handler = isResolved ? current->onFulfilled() : current->onRejected();
        JSValue context = current->context();
        current = current->next();

        if (handler.isInt32()) {
            auto task = static_cast<InternalMicrotask>(handler.asInt32());
            globalObject->queueMicrotask(task, static_cast<uint8_t>(status), promise, argument, context);
            continue;
        }
        ASSERT(context.isUndefinedOrNull());
        globalObject->queueMicrotask(InternalMicrotask::PromiseReactionJob, static_cast<uint8_t>(status), promise, handler, argument);
    }
}

void JSPromise::resolveWithInternalMicrotaskForAsyncAwait(JSGlobalObject* globalObject, JSValue resolution, InternalMicrotask task, JSValue context)
{
    VM& vm = globalObject->vm();

    if (resolution.inherits<JSPromise>()) {
        auto* promise = jsCast<JSPromise*>(resolution);
        if (promiseSpeciesWatchpointIsValid(vm, promise)) [[likely]]
            return promise->performPromiseThenWithInternalMicrotask(vm, globalObject, task, jsUndefined(), context);

        JSValue constructor;
        JSValue error;
        {
            auto catchScope = DECLARE_TOP_EXCEPTION_SCOPE(vm);
            constructor = promise->get(globalObject, vm.propertyNames->constructor);
            if (catchScope.exception()) [[unlikely]] {
                error = catchScope.exception()->value();
                if (!catchScope.clearExceptionExceptTermination()) [[unlikely]]
                    return;
            }
        }
        if (error) [[unlikely]] {
            std::array<JSValue, maxMicrotaskArguments> arguments { {
                jsUndefined(),
                error,
                context,
            } };
            runInternalMicrotask(globalObject, task, static_cast<uint8_t>(JSPromise::Status::Rejected), arguments);
            return;
        }

        if (constructor == globalObject->promiseConstructor() || constructor == globalObject->internalPromiseConstructor())
            return promise->performPromiseThenWithInternalMicrotask(vm, globalObject, task, jsUndefined(), context);
    }

    resolveWithInternalMicrotask(globalObject, resolution, task, context);
}

void JSPromise::resolveWithInternalMicrotask(JSGlobalObject* globalObject, JSValue resolution, InternalMicrotask task, JSValue context)
{
    VM& vm = globalObject->vm();

    if (!resolution.isObject())
        return fulfillWithInternalMicrotask(globalObject, resolution, task, context);

    auto* resolutionObject = asObject(resolution);
    if (resolutionObject->inherits<JSPromise>()) {
        auto* promise = jsCast<JSPromise*>(resolutionObject);
        if (promise->isThenFastAndNonObservable())
            return globalObject->queueMicrotask(InternalMicrotask::PromiseResolveThenableJobWithInternalMicrotaskFast, static_cast<uint8_t>(task), resolutionObject, context, jsUndefined());
    }

    if (isDefinitelyNonThenable(resolutionObject, globalObject))
        return fulfillWithInternalMicrotask(globalObject, resolution, task, context);

    JSValue then;
    JSValue error;
    {
        auto catchScope = DECLARE_TOP_EXCEPTION_SCOPE(vm);
        then = resolutionObject->get(globalObject, vm.propertyNames->then);
        if (catchScope.exception()) [[unlikely]] {
            error = catchScope.exception()->value();
            if (!catchScope.clearExceptionExceptTermination()) [[unlikely]]
                return;
        }
    }
    if (error) [[unlikely]]
        return rejectWithInternalMicrotask(globalObject, error, task, context);

    if (!then.isCallable()) [[likely]]
        return fulfillWithInternalMicrotask(globalObject, resolution, task, context);

    return globalObject->queueMicrotask(InternalMicrotask::PromiseResolveThenableJobWithInternalMicrotask, static_cast<uint8_t>(task), resolutionObject, then, context);
}

void JSPromise::rejectWithInternalMicrotask(JSGlobalObject* globalObject, JSValue argument, InternalMicrotask task, JSValue context)
{
    globalObject->queueMicrotask(task, static_cast<uint8_t>(Status::Rejected), jsUndefined(), argument, context);
}

void JSPromise::fulfillWithInternalMicrotask(JSGlobalObject* globalObject, JSValue argument, InternalMicrotask task, JSValue context)
{
    globalObject->queueMicrotask(task, static_cast<uint8_t>(Status::Fulfilled), jsUndefined(), argument, context);
}

bool JSPromise::isThenFastAndNonObservable()
{
    JSGlobalObject* globalObject = this->globalObject();
    Structure* structure = this->structure();
    if (!globalObject->promiseThenWatchpointSet().isStillValid()) [[unlikely]] {
        if (inherits<JSInternalPromise>())
            return true;
        return false;
    }

    if (structure == globalObject->promiseStructure())
        return true;

    if (inherits<JSInternalPromise>())
        return true;

    if (getPrototypeDirect() != globalObject->promisePrototype())
        return false;

    VM& vm = globalObject->vm();
    if (getDirectOffset(vm, vm.propertyNames->then) != invalidOffset)
        return false;

    return true;
}

JSObject* promiseSpeciesConstructor(JSGlobalObject* globalObject, JSObject* thisObject)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (auto* promise = jsDynamicCast<JSPromise*>(thisObject)) [[likely]] {
        if (promiseSpeciesWatchpointIsValid(vm, promise)) [[likely]]
            return globalObject->promiseConstructor();
    }

    JSValue constructor = thisObject->get(globalObject, vm.propertyNames->constructor);
    RETURN_IF_EXCEPTION(scope, { });

    if (constructor.isUndefined())
        return globalObject->promiseConstructor();

    if (!constructor.isObject()) [[unlikely]] {
        throwTypeError(globalObject, scope, "|this|.constructor is not an Object or undefined"_s);
        return { };
    }

    constructor = asObject(constructor)->get(globalObject, vm.propertyNames->speciesSymbol);
    RETURN_IF_EXCEPTION(scope, { });

    if (constructor.isUndefinedOrNull())
        return globalObject->promiseConstructor();

    if (constructor.isConstructor()) [[likely]]
        return asObject(constructor);

    throwTypeError(globalObject, scope, "|this|.constructor[Symbol.species] is not a constructor"_s);
    return { };
}

Structure* createPromiseCapabilityObjectStructure(VM& vm, JSGlobalObject& globalObject)
{
    Structure* structure = globalObject.structureCache().emptyObjectStructureForPrototype(&globalObject, globalObject.objectPrototype(), JSFinalObject::defaultInlineCapacity);
    PropertyOffset offset;
    structure = Structure::addPropertyTransition(vm, structure, vm.propertyNames->resolve, 0, offset);
    RELEASE_ASSERT(offset == promiseCapabilityResolvePropertyOffset);
    structure = Structure::addPropertyTransition(vm, structure, vm.propertyNames->reject, 0, offset);
    RELEASE_ASSERT(offset == promiseCapabilityRejectPropertyOffset);
    structure = Structure::addPropertyTransition(vm, structure, vm.propertyNames->promise, 0, offset);
    RELEASE_ASSERT(offset == promiseCapabilityPromisePropertyOffset);
    return structure;
}

JSObject* JSPromise::then(JSGlobalObject* globalObject, JSValue onFulfilled, JSValue onRejected)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* resultPromise;
    JSValue resultPromiseCapability;
    if (promiseSpeciesWatchpointIsValid(vm, this)) [[likely]] {
        if (inherits<JSInternalPromise>())
            resultPromise = JSInternalPromise::create(vm, globalObject->internalPromiseStructure());
        else
            resultPromise = JSPromise::create(vm, globalObject->promiseStructure());
        resultPromiseCapability = resultPromise;
    } else {
        auto* constructor = promiseSpeciesConstructor(globalObject, this);
        RETURN_IF_EXCEPTION(scope, { });

        auto [promise, resolve, reject] = JSPromise::newPromiseCapability(globalObject, constructor);
        RETURN_IF_EXCEPTION(scope, { });

        resultPromise = promise;
        resultPromiseCapability = JSPromise::createPromiseCapability(vm, globalObject, promise, resolve, reject);
    }

    scope.release();
    performPromiseThen(vm, globalObject, onFulfilled, onRejected, resultPromiseCapability);
    return resultPromise;
}

JSObject* JSPromise::promiseResolve(JSGlobalObject* globalObject, JSObject* constructor, JSValue argument)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (argument.inherits<JSPromise>()) {
        auto* promise = jsCast<JSPromise*>(argument);
        if (promiseSpeciesWatchpointIsValid(vm, promise)) [[likely]]
            return promise;

        auto property = promise->get(globalObject, vm.propertyNames->constructor);
        RETURN_IF_EXCEPTION(scope, { });

        if (property == constructor)
            return promise;
    }

    if (constructor == globalObject->promiseConstructor()) [[likely]] {
        JSPromise* promise = JSPromise::create(vm, globalObject->promiseStructure());
        scope.release();
        promise->resolve(globalObject, argument);
        return promise;
    }

    auto [promise, resolve, reject] = newPromiseCapability(globalObject, constructor);
    RETURN_IF_EXCEPTION(scope, { });

    MarkedArgumentBuffer arguments;
    arguments.append(argument);
    ASSERT(!arguments.hasOverflowed());
    scope.release();
    call(globalObject, resolve, jsUndefined(), arguments, "resolve is not a function"_s);
    return promise;
}

JSObject* JSPromise::promiseReject(JSGlobalObject* globalObject, JSObject* constructor, JSValue argument)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (constructor == globalObject->promiseConstructor()) [[likely]] {
        JSPromise* promise = JSPromise::create(vm, globalObject->promiseStructure());
        promise->reject(vm, globalObject, argument);
        return promise;
    }

    auto [promise, resolve, reject] = newPromiseCapability(globalObject, constructor);
    RETURN_IF_EXCEPTION(scope, { });

    MarkedArgumentBuffer arguments;
    arguments.append(argument);
    ASSERT(!arguments.hasOverflowed());
    scope.release();
    call(globalObject, reject, jsUndefined(), arguments, "reject is not a function"_s);
    return promise;
}

} // namespace JSC
