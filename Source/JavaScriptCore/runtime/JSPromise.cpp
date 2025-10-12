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
#include "JSPromiseAllContext.h"
#include "JSPromiseAllGlobalContext.h"
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

    auto* executor = JSFunctionWithFields::create(vm, globalObject, vm.promiseCapabilityExecutorExecutable(), 2, nullString());
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
    promise->reject(globalObject, value);
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

void JSPromise::reject(JSGlobalObject* globalObject, JSValue value)
{
    VM& vm = globalObject->vm();
    uint32_t flags = this->flags();
    ASSERT(!value.inherits<Exception>());
    if (!(flags & isFirstResolvingFunctionCalledFlag)) {
        internalField(Field::Flags).set(vm, this, jsNumber(flags | isFirstResolvingFunctionCalledFlag));
        rejectPromise(globalObject, value);
    }
}

void JSPromise::fulfill(JSGlobalObject* globalObject, JSValue value)
{
    VM& vm = globalObject->vm();
    uint32_t flags = this->flags();
    ASSERT(!value.inherits<Exception>());
    if (!(flags & isFirstResolvingFunctionCalledFlag)) {
        internalField(Field::Flags).set(vm, this, jsNumber(flags | isFirstResolvingFunctionCalledFlag));
        fulfillPromise(globalObject, value);
    }
}

void JSPromise::performPromiseThenExported(JSGlobalObject* globalObject, JSValue onFulfilled, JSValue onRejected, JSValue promiseOrCapability, JSValue context)
{
    return performPromiseThen(globalObject, onFulfilled, onRejected, promiseOrCapability, context);
}

void JSPromise::rejectAsHandled(JSGlobalObject* lexicalGlobalObject, JSValue value)
{
    // Setting isHandledFlag before calling reject since this removes round-trip between JSC and PromiseRejectionTracker, and it does not show an user-observable behavior.
    if (!(flags() & isFirstResolvingFunctionCalledFlag)) {
        markAsHandled();
        reject(lexicalGlobalObject, value);
    }
}

void JSPromise::reject(JSGlobalObject* lexicalGlobalObject, Exception* reason)
{
    reject(lexicalGlobalObject, reason->value());
}

void JSPromise::rejectAsHandled(JSGlobalObject* lexicalGlobalObject, Exception* reason)
{
    rejectAsHandled(lexicalGlobalObject, reason->value());
}

JSPromise* JSPromise::rejectWithCaughtException(JSGlobalObject* globalObject, ThrowScope& scope)
{
    VM& vm = globalObject->vm();
    Exception* exception = scope.exception();
    ASSERT(exception);
    if (vm.isTerminationException(exception)) [[unlikely]] {
        scope.release();
        return this;
    }
    scope.clearException();
    scope.release();
    reject(globalObject, exception->value());
    return this;
}

void JSPromise::performPromiseThen(JSGlobalObject* globalObject, JSValue onFulfilled, JSValue onRejected, JSValue promiseOrCapability, JSValue context)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!onFulfilled.isCallable())
        onFulfilled = globalObject->promiseEmptyOnFulfilledFunction();

    if (!onRejected.isCallable())
        onRejected = globalObject->promiseEmptyOnRejectedFunction();

    JSValue reactionsOrResult = this->reactionsOrResult();
    switch (status()) {
    case JSPromise::Status::Pending: {
        auto* reaction = JSPromiseReaction::create(vm, promiseOrCapability, onFulfilled, onRejected, context, jsDynamicCast<JSPromiseReaction*>(reactionsOrResult));
        setReactionsOrResult(vm, reaction);
        break;
    }
    case JSPromise::Status::Rejected: {
        if (!isHandled()) {
            if (globalObject->globalObjectMethodTable()->promiseRejectionTracker) {
                globalObject->globalObjectMethodTable()->promiseRejectionTracker(globalObject, this, JSPromiseRejectionOperation::Handle);
                RETURN_IF_EXCEPTION(scope, void());
            }
        }
        scope.release();
        if (promiseOrCapability.isUndefinedOrNull()) {
            globalObject->queueMicrotask(InternalMicrotask::PromiseReactionJobWithoutPromise, onRejected, reactionsOrResult, context, jsUndefined());
            break;
        }
        globalObject->queueMicrotask(InternalMicrotask::PromiseReactionJob, promiseOrCapability, onRejected, reactionsOrResult, context);
        break;
    }
    case JSPromise::Status::Fulfilled: {
        scope.release();
        if (promiseOrCapability.isUndefinedOrNull()) {
            globalObject->queueMicrotask(InternalMicrotask::PromiseReactionJobWithoutPromise, onFulfilled, reactionsOrResult, context, jsUndefined());
            break;
        }
        globalObject->queueMicrotask(InternalMicrotask::PromiseReactionJob, promiseOrCapability, onFulfilled, reactionsOrResult, context);
        break;
    }
    }
    markAsHandled();
}

void JSPromise::rejectPromise(JSGlobalObject* globalObject, JSValue argument)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(status() == Status::Pending);
    uint32_t flags = this->flags();
    auto* reactions = jsDynamicCast<JSPromiseReaction*>(this->reactionsOrResult());
    internalField(Field::Flags).set(vm, this, jsNumber(flags | static_cast<uint32_t>(Status::Rejected)));
    internalField(Field::ReactionsOrResult).set(vm, this, argument);

    if (!isHandled()) {
        if (globalObject->globalObjectMethodTable()->promiseRejectionTracker) {
            globalObject->globalObjectMethodTable()->promiseRejectionTracker(globalObject, this, JSPromiseRejectionOperation::Reject);
            RETURN_IF_EXCEPTION(scope, void());
        } else
            vm.promiseRejected(this);
    }

    if (!reactions)
        return;
    RELEASE_AND_RETURN(scope, triggerPromiseReactions(globalObject, Status::Rejected, reactions, argument));
}

void JSPromise::fulfillPromise(JSGlobalObject* globalObject, JSValue argument)
{
    VM& vm = globalObject->vm();

    ASSERT(status() == Status::Pending);
    uint32_t flags = this->flags();
    auto* reactions = jsDynamicCast<JSPromiseReaction*>(this->reactionsOrResult());
    internalField(Field::Flags).set(vm, this, jsNumber(flags | static_cast<uint32_t>(Status::Fulfilled)));
    internalField(Field::ReactionsOrResult).set(vm, this, argument);
    if (!reactions)
        return;
    triggerPromiseReactions(globalObject, Status::Fulfilled, reactions, argument);
}

void JSPromise::resolvePromise(JSGlobalObject* globalObject, JSValue resolution)
{
    VM& vm = globalObject->vm();

    if (resolution == this) [[unlikely]] {
        Structure* errorStructure = globalObject->errorStructure(ErrorType::TypeError);
        auto* error = ErrorInstance::create(vm, errorStructure, "Cannot resolve a promise with itself"_s, jsUndefined(), nullptr, TypeNothing, ErrorType::TypeError, false);
        return rejectPromise(globalObject, error);
    }

    if (!resolution.isObject())
        return fulfillPromise(globalObject, resolution);

    auto* resolutionObject = asObject(resolution);
    if (resolutionObject->inherits<JSPromise>()) {
        auto* promise = jsCast<JSPromise*>(resolutionObject);
        if (promise->isThenFastAndNonObservable())
            return globalObject->queueMicrotask(InternalMicrotask::PromiseResolveThenableJobFast, resolutionObject, this, jsUndefined(), jsUndefined());
    }

    JSValue then;
    JSValue error;
    {
        auto catchScope = DECLARE_CATCH_SCOPE(vm);
        then = resolutionObject->get(globalObject, vm.propertyNames->then);
        if (catchScope.exception()) [[unlikely]] {
            error = catchScope.exception()->value();
            if (!catchScope.clearExceptionExceptTermination()) [[unlikely]]
                return;
        }
    }
    if (error) [[unlikely]]
        return rejectPromise(globalObject, error);

    if (!then.isCallable()) [[likely]]
        return fulfillPromise(globalObject, resolutionObject);

    auto [ resolve, reject ] = createResolvingFunctions(vm, globalObject);
    return globalObject->queueMicrotask(InternalMicrotask::PromiseResolveThenableJob, resolutionObject, then, resolve, reject);
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

    promise->rejectPromise(globalObject, argument);
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

    promise->reject(globalObject, argument);
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(promiseResolvingFunctionResolveWithoutPromise, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();

    auto* callee = jsCast<JSFunctionWithFields*>(callFrame->jsCallee());
    auto* other = jsDynamicCast<JSFunctionWithFields*>(callee->getField(JSFunctionWithFields::Field::ResolvingWithoutPromiseOther));
    if (!other) [[unlikely]]
        return JSValue::encode(jsUndefined());

    callee->setField(vm, JSFunctionWithFields::Field::ResolvingWithoutPromiseOther, jsNull());
    other->setField(vm, JSFunctionWithFields::Field::ResolvingWithoutPromiseOther, jsNull());

    auto* context = jsCast<JSPromiseAllGlobalContext*>(callee->getField(JSFunctionWithFields::Field::ResolvingWithoutPromiseContext));
    JSValue argument = callFrame->argument(0);

    JSPromise::resolveWithoutPromise(globalObject, argument, context->promise(), context->values(), context->remainingElementsCount());
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(promiseResolvingFunctionRejectWithoutPromise, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();

    auto* callee = jsCast<JSFunctionWithFields*>(callFrame->jsCallee());
    auto* other = jsDynamicCast<JSFunctionWithFields*>(callee->getField(JSFunctionWithFields::Field::ResolvingWithoutPromiseOther));
    if (!other) [[unlikely]]
        return JSValue::encode(jsUndefined());

    callee->setField(vm, JSFunctionWithFields::Field::ResolvingWithoutPromiseOther, jsNull());
    other->setField(vm, JSFunctionWithFields::Field::ResolvingWithoutPromiseOther, jsNull());

    auto* context = jsCast<JSPromiseAllGlobalContext*>(callee->getField(JSFunctionWithFields::Field::ResolvingWithoutPromiseContext));
    JSValue argument = callFrame->argument(0);

    JSPromise::rejectWithoutPromise(globalObject, argument, context->promise(), context->values(), context->remainingElementsCount());
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

std::tuple<JSFunction*, JSFunction*> JSPromise::createResolvingFunctionsWithoutPromise(VM& vm, JSGlobalObject* globalObject, JSValue onFulfilled, JSValue onRejected, JSValue context)
{
    auto* resolve = JSFunctionWithFields::create(vm, globalObject, vm.promiseResolvingFunctionResolveWithoutPromiseExecutable(), 1, nullString());
    auto* reject = JSFunctionWithFields::create(vm, globalObject, vm.promiseResolvingFunctionRejectWithoutPromiseExecutable(), 1, nullString());

    auto* all = JSPromiseAllGlobalContext::create(vm, globalObject->promiseAllGlobalContextStructure(), onFulfilled, onRejected, context);

    resolve->setField(vm, JSFunctionWithFields::Field::ResolvingWithoutPromiseContext, all);
    resolve->setField(vm, JSFunctionWithFields::Field::ResolvingWithoutPromiseOther, reject);

    reject->setField(vm, JSFunctionWithFields::Field::ResolvingWithoutPromiseContext, all);
    reject->setField(vm, JSFunctionWithFields::Field::ResolvingWithoutPromiseOther, resolve);

    return std::tuple { resolve, reject };
}

void JSPromise::triggerPromiseReactions(JSGlobalObject* globalObject, Status status, JSPromiseReaction* head, JSValue argument)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

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

        if (handler.isUndefinedOrNull()) {
            globalObject->queueMicrotask(InternalMicrotask::PromiseResolveWithoutHandlerJob, promise, argument, jsNumber(static_cast<int32_t>(status)), jsUndefined());
            RETURN_IF_EXCEPTION(scope, void());
            continue;
        }

        if (promise.isUndefinedOrNull()) {
            globalObject->queueMicrotask(InternalMicrotask::PromiseReactionJobWithoutPromise, handler, argument, context, jsUndefined());
            RETURN_IF_EXCEPTION(scope, void());
            continue;
        }

        globalObject->queueMicrotask(InternalMicrotask::PromiseReactionJob, promise, handler, argument, context);
        RETURN_IF_EXCEPTION(scope, void());
    }
}

void JSPromise::resolveWithoutPromiseForAsyncAwait(JSGlobalObject* globalObject, JSValue resolution, JSValue onFulfilled, JSValue onRejected, JSValue context)
{
    // This function has strong guarantee that each handler function (onFulfilled and onRejected) will be called at most once.
    // This is special version of resolveWithoutPromise which skips resolution's then handling.
    // https://github.com/tc39/ecma262/pull/1250

    VM& vm = globalObject->vm();

    if (resolution.inherits<JSPromise>()) {
        auto* promise = jsCast<JSPromise*>(resolution);
        if (promiseSpeciesWatchpointIsValid(vm, promise)) [[likely]]
            return promise->performPromiseThen(globalObject, onFulfilled, onRejected, jsUndefined(), context);

        JSValue constructor;
        JSValue error;
        {
            auto catchScope = DECLARE_CATCH_SCOPE(vm);
            constructor = promise->get(globalObject, vm.propertyNames->constructor);
            if (catchScope.exception()) [[unlikely]] {
                error = catchScope.exception()->value();
                if (!catchScope.clearExceptionExceptTermination()) [[unlikely]]
                    return;
            }
        }
        if (error) [[unlikely]] {
            MarkedArgumentBuffer arguments;
            arguments.append(error);
            arguments.append(context);
            ASSERT(!arguments.hasOverflowed());
            call(globalObject, onRejected, jsUndefined(), arguments, "onRejected is not a function"_s);
            return;
        }

        if (constructor == globalObject->promiseConstructor() || constructor == globalObject->internalPromiseConstructor())
            return promise->performPromiseThen(globalObject, onFulfilled, onRejected, jsUndefined(), context);
    }

    resolveWithoutPromise(globalObject, resolution, onFulfilled, onRejected, context);
}

void JSPromise::resolveWithoutPromise(JSGlobalObject* globalObject, JSValue resolution, JSValue onFulfilled, JSValue onRejected, JSValue context)
{
    VM& vm = globalObject->vm();

    if (!resolution.isObject())
        return fulfillWithoutPromise(globalObject, resolution, onFulfilled, onRejected, context);

    auto* resolutionObject = asObject(resolution);
    if (resolutionObject->inherits<JSPromise>()) {
        auto* promise = jsCast<JSPromise*>(resolutionObject);
        if (promise->isThenFastAndNonObservable())
            return globalObject->queueMicrotask(InternalMicrotask::PromiseResolveThenableJobWithoutPromiseFast, resolutionObject, onFulfilled, onRejected, context);
    }

    JSValue then;
    JSValue error;
    {
        auto catchScope = DECLARE_CATCH_SCOPE(vm);
        then = resolutionObject->get(globalObject, vm.propertyNames->then);
        if (catchScope.exception()) [[unlikely]] {
            error = catchScope.exception()->value();
            if (!catchScope.clearExceptionExceptTermination()) [[unlikely]]
                return;
        }
    }
    if (error) [[unlikely]]
        return rejectWithoutPromise(globalObject, error, onFulfilled, onRejected, context);

    if (!then.isCallable()) [[likely]]
        return fulfillWithoutPromise(globalObject, resolution, onFulfilled, onRejected, context);

    auto [ resolve, reject ] = createResolvingFunctionsWithoutPromise(vm, globalObject, onFulfilled, onRejected, context);
    return globalObject->queueMicrotask(InternalMicrotask::PromiseResolveThenableJob, resolutionObject, then, resolve, reject);
}

void JSPromise::rejectWithoutPromise(JSGlobalObject* globalObject, JSValue argument, JSValue onFulfilled, JSValue onRejected, JSValue context)
{
    UNUSED_PARAM(onFulfilled);
    globalObject->queueMicrotask(InternalMicrotask::PromiseReactionJobWithoutPromise, onRejected, argument, context, jsUndefined());
}

void JSPromise::fulfillWithoutPromise(JSGlobalObject* globalObject, JSValue argument, JSValue onFulfilled, JSValue onRejected, JSValue context)
{
    UNUSED_PARAM(onRejected);
    globalObject->queueMicrotask(InternalMicrotask::PromiseReactionJobWithoutPromise, onFulfilled, argument, context, jsUndefined());
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

JSValue JSPromise::then(JSGlobalObject* globalObject, JSValue onFulfilled, JSValue onRejected)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue resultPromise;
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
    performPromiseThen(globalObject, onFulfilled, onRejected, resultPromiseCapability, jsUndefined());
    return resultPromise;
}

JSValue JSPromise::promiseResolve(JSGlobalObject* globalObject, JSValue constructor, JSValue argument)
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

} // namespace JSC
