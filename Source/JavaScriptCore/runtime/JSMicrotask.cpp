/*
 * Copyright (C) 2013-2017 Apple Inc. All rights reserved.
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
#include "JSMicrotask.h"

#include "CatchScope.h"
#include "Debugger.h"
#include "DeferTermination.h"
#include "GlobalObjectMethodTable.h"
#include "JSGlobalObject.h"
#include "JSObjectInlines.h"
#include "JSPromise.h"
#include "JSPromisePrototype.h"
#include "JSPromiseReaction.h"
#include "Microtask.h"
#include "ObjectConstructor.h"

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

static ALWAYS_INLINE JSCell* dynamicCastToCell(JSValue value)
{
    if (value.isCell())
        return value.asCell();
    return nullptr;
}

static void promiseResolveThenableJobFastSlow(JSGlobalObject* globalObject, JSPromise* promise, JSPromise* promiseToResolve)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSObject* constructor = promiseSpeciesConstructor(globalObject, promise);
    if (scope.exception()) [[unlikely]]
        return;

    auto [resolve, reject] = promiseToResolve->createResolvingFunctions(vm, globalObject);

    auto capability = JSPromise::createNewPromiseCapability(globalObject, constructor);
    if (!scope.exception()) [[likely]] {
        promise->performPromiseThen(globalObject, resolve, reject, capability, jsUndefined());
        if (!scope.exception()) [[likely]]
            return;
    }

    JSValue error = scope.exception()->value();
    if (!scope.clearExceptionExceptTermination()) [[unlikely]]
        return;

    MarkedArgumentBuffer arguments;
    arguments.append(error);
    ASSERT(!arguments.hasOverflowed());
    auto callData = JSC::getCallData(reject);
    call(globalObject, reject, callData, jsUndefined(), arguments);
    EXCEPTION_ASSERT(scope.exception() || true);
}

static void promiseResolveThenableJobWithoutPromiseFastSlow(JSGlobalObject* globalObject, JSPromise* promise, JSValue onFulfilled, JSValue onRejected, JSValue context)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSObject* constructor = promiseSpeciesConstructor(globalObject, promise);
    if (scope.exception()) [[unlikely]]
        return;

    auto [resolve, reject] = JSPromise::createResolvingFunctionsWithoutPromise(vm, globalObject, onFulfilled, onRejected, context);

    auto capability = JSPromise::createNewPromiseCapability(globalObject, constructor);
    if (!scope.exception()) [[likely]] {
        promise->performPromiseThen(globalObject, resolve, reject, capability, jsUndefined());
        if (!scope.exception()) [[likely]]
            return;
    }

    JSValue error = scope.exception()->value();
    if (!scope.clearExceptionExceptTermination()) [[unlikely]]
        return;

    MarkedArgumentBuffer arguments;
    arguments.append(error);
    ASSERT(!arguments.hasOverflowed());
    auto callData = JSC::getCallData(reject);
    call(globalObject, reject, callData, jsUndefined(), arguments);
    EXCEPTION_ASSERT(scope.exception() || true);
}

static void promiseResolveThenableJob(JSGlobalObject* globalObject, JSValue promise, JSValue then, JSValue resolve, JSValue reject)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);

    {
        MarkedArgumentBuffer arguments;
        arguments.append(resolve);
        arguments.append(reject);
        ASSERT(!arguments.hasOverflowed());

        callMicrotask(globalObject, then, promise, dynamicCastToCell(then), arguments, "|then| is not a function"_s);
        if (!scope.exception()) [[likely]]
            return;
    }

    JSValue error = scope.exception()->value();
    if (!scope.clearExceptionExceptTermination()) [[unlikely]]
        return;

    MarkedArgumentBuffer arguments;
    arguments.append(error);
    ASSERT(!arguments.hasOverflowed());
    call(globalObject, reject, jsUndefined(), arguments, "|reject| is not a function"_s);
    EXCEPTION_ASSERT(scope.exception() || true);
}

void runInternalMicrotask(JSGlobalObject* globalObject, InternalMicrotask task, std::span<const JSValue, maxMicrotaskArguments> arguments)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    switch (task) {
    case InternalMicrotask::PromiseResolveThenableJobFast: {
        auto* promise = jsCast<JSPromise*>(arguments[0]);
        auto* promiseToResolve = jsCast<JSPromise*>(arguments[1]);

        if (!promiseSpeciesWatchpointIsValid(vm, promise)) [[unlikely]]
            RELEASE_AND_RETURN(scope, promiseResolveThenableJobFastSlow(globalObject, promise, promiseToResolve));

        switch (promise->status()) {
        case JSPromise::Status::Pending: {
            auto* reaction = JSPromiseReaction::create(vm, promiseToResolve, jsUndefined(), jsUndefined(), jsUndefined(), jsDynamicCast<JSPromiseReaction*>(promise->reactionsOrResult()));
            promise->setReactionsOrResult(vm, reaction);
            break;
        }
        case JSPromise::Status::Rejected: {
            if (!promise->isHandled()) {
                if (globalObject->globalObjectMethodTable()->promiseRejectionTracker) {
                    globalObject->globalObjectMethodTable()->promiseRejectionTracker(globalObject, promise, JSPromiseRejectionOperation::Handle);
                    RETURN_IF_EXCEPTION(scope, void());
                }
            }
            scope.release();
            globalObject->queueMicrotask(InternalMicrotask::PromiseResolveWithoutHandlerJob, promiseToResolve, promise->reactionsOrResult(), jsNumber(static_cast<int32_t>(JSPromise::Status::Rejected)), jsUndefined());
            break;
        }
        case JSPromise::Status::Fulfilled: {
            scope.release();
            globalObject->queueMicrotask(InternalMicrotask::PromiseResolveWithoutHandlerJob, promiseToResolve, promise->reactionsOrResult(), jsNumber(static_cast<int32_t>(JSPromise::Status::Fulfilled)), jsUndefined());
            break;
        }
        }
        promise->markAsHandled();
        return;
    }

    case InternalMicrotask::PromiseResolveThenableJobWithoutPromiseFast: {
        auto* promise = jsCast<JSPromise*>(arguments[0]);
        JSValue onFulfilled = arguments[1];
        JSValue onRejected = arguments[2];
        JSValue context = arguments[3];

        if (!promiseSpeciesWatchpointIsValid(vm, promise)) [[unlikely]]
            RELEASE_AND_RETURN(scope, promiseResolveThenableJobWithoutPromiseFastSlow(globalObject, promise, onFulfilled, onRejected, context));

        switch (promise->status()) {
        case JSPromise::Status::Pending: {
            auto* reaction = JSPromiseReaction::create(vm, jsUndefined(), onFulfilled, onRejected, context, jsDynamicCast<JSPromiseReaction*>(promise->reactionsOrResult()));
            promise->setReactionsOrResult(vm, reaction);
            break;
        }
        case JSPromise::Status::Rejected: {
            if (!promise->isHandled()) {
                if (globalObject->globalObjectMethodTable()->promiseRejectionTracker) {
                    globalObject->globalObjectMethodTable()->promiseRejectionTracker(globalObject, promise, JSPromiseRejectionOperation::Handle);
                    RETURN_IF_EXCEPTION(scope, void());
                }
            }

            scope.release();
            globalObject->queueMicrotask(InternalMicrotask::PromiseReactionJobWithoutPromise, onRejected, promise->reactionsOrResult(), context, jsUndefined());
            break;
        }
        case JSPromise::Status::Fulfilled: {
            scope.release();
            globalObject->queueMicrotask(InternalMicrotask::PromiseReactionJobWithoutPromise, onFulfilled, promise->reactionsOrResult(), context, jsUndefined());
            break;
        }
        }

        promise->markAsHandled();
        return;
    }

    case InternalMicrotask::PromiseResolveThenableJob: {
        JSValue promise = arguments[0];
        JSValue then = arguments[1];
        JSValue resolve = arguments[2];
        JSValue reject = arguments[3];
        RELEASE_AND_RETURN(scope, promiseResolveThenableJob(globalObject, promise, then, resolve, reject));
    }

    case InternalMicrotask::PromiseResolveWithoutHandlerJob: {
        auto* promise = jsCast<JSPromise*>(arguments[0]);
        JSValue resolution = arguments[1];
        switch (static_cast<JSPromise::Status>(arguments[2].asInt32())) {
        case JSPromise::Status::Pending: {
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        case JSPromise::Status::Fulfilled: {
            scope.release();
            promise->resolvePromise(globalObject, resolution);
            break;
        }
        case JSPromise::Status::Rejected: {
            scope.release();
            promise->rejectPromise(globalObject, resolution);
            break;
        }
        }
        return;
    }

    case InternalMicrotask::PromiseReactionJob: {
        JSValue promiseOrCapability = arguments[0];
        JSValue handler = arguments[1];
        JSValue context = arguments[3];

        ASSERT(!promiseOrCapability.isUndefinedOrNull());
        JSValue result;
        JSValue error;
        {
            auto catchScope = DECLARE_CATCH_SCOPE(vm);
            if (context.isUndefinedOrNull())
                result = callMicrotask(globalObject, handler, jsUndefined(), dynamicCastToCell(handler), ArgList { std::bit_cast<EncodedJSValue*>(arguments.data() + 2), 1 }, "handler is not a function"_s);
            else
                result = callMicrotask(globalObject, handler, jsUndefined(), dynamicCastToCell(context), ArgList { std::bit_cast<EncodedJSValue*>(arguments.data() + 2), 2 }, "handler is not a function"_s);

            if (catchScope.exception()) {
                error = catchScope.exception()->value();
                if (!catchScope.clearExceptionExceptTermination()) [[unlikely]] {
                    scope.release();
                    return;
                }
            }
        }

        if (error) {
            if (auto* promise = jsDynamicCast<JSPromise*>(promiseOrCapability))
                RELEASE_AND_RETURN(scope, promise->rejectPromise(globalObject, error));

            JSValue reject = promiseOrCapability.get(globalObject, vm.propertyNames->reject);
            RETURN_IF_EXCEPTION(scope, void());

            MarkedArgumentBuffer arguments;
            arguments.append(error);
            ASSERT(!arguments.hasOverflowed());
            scope.release();
            call(globalObject, reject, jsUndefined(), arguments, "reject is not a function"_s);
            return;
        }

        if (auto* promise = jsDynamicCast<JSPromise*>(promiseOrCapability))
            RELEASE_AND_RETURN(scope, promise->resolvePromise(globalObject, result));

        JSValue resolve = promiseOrCapability.get(globalObject, vm.propertyNames->resolve);
        RETURN_IF_EXCEPTION(scope, void());

        MarkedArgumentBuffer arguments;
        arguments.append(result);
        ASSERT(!arguments.hasOverflowed());
        scope.release();
        call(globalObject, resolve, jsUndefined(), arguments, "resolve is not a function"_s);
        return;
    }

    case InternalMicrotask::PromiseReactionJobWithoutPromise: {
        JSValue handler = arguments[0];
        JSValue context = arguments[2];
        if (context.isUndefinedOrNull()) {
            scope.release();
            callMicrotask(globalObject, handler, jsUndefined(), dynamicCastToCell(handler), ArgList { std::bit_cast<EncodedJSValue*>(arguments.data() + 1), 1 }, "handler is not a function"_s);
        } else {
            scope.release();
            callMicrotask(globalObject, handler, jsUndefined(), dynamicCastToCell(context), ArgList { std::bit_cast<EncodedJSValue*>(arguments.data() + 1), 2 }, "handler is not a function"_s);
        }
        return;
    }

    case InternalMicrotask::InvokeFunctionJob: {
        JSValue handler = arguments[0];
        scope.release();
        callMicrotask(globalObject, handler, jsUndefined(), nullptr, ArgList { }, "handler is not a function"_s);
        return;
    }

    case InternalMicrotask::Opaque: {
        RELEASE_ASSERT_NOT_REACHED();
        return;
    }
    }
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
