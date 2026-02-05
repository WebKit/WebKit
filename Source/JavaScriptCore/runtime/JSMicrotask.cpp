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

#include "AggregateError.h"
#include "BuiltinNames.h"
#include "Debugger.h"
#include "DeferTermination.h"
#include "FunctionCodeBlock.h"
#include "FunctionExecutable.h"
#include "GlobalObjectMethodTable.h"
#include "Interpreter.h"
#include "InterpreterInlines.h"
#include "IteratorOperations.h"
#include "JSArray.h"
#include "JSAsyncGenerator.h"
#include "JSFunction.h"
#include "JSGenerator.h"
#include "JSGlobalObject.h"
#include "JSObjectInlines.h"
#include "JSPromise.h"
#include "JSPromiseCombinatorsContext.h"
#include "JSPromiseCombinatorsGlobalContext.h"
#include "JSPromiseConstructor.h"
#include "JSPromisePrototype.h"
#include "JSPromiseReaction.h"
#include "LLIntThunks.h"
#include "Microtask.h"
#include "ObjectConstructor.h"
#include "ThrowScope.h"
#include "TopExceptionScope.h"
#include "VMTrapsInlines.h"
#include <wtf/NoTailCalls.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

static ALWAYS_INLINE JSCell* dynamicCastToCell(JSValue value)
{
    if (value.isCell())
        return value.asCell();
    return nullptr;
}

template<typename... Args> requires (std::is_convertible_v<Args, JSValue> && ...)
static JSValue callMicrotask(JSGlobalObject* globalObject, JSValue functionObject, JSValue thisValue, JSCell* context, ASCIILiteral message, Args... args)
{
    NO_TAIL_CALLS();

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    static_assert(sizeof...(args) <= 6);

    auto callData = JSC::getCallDataInline(functionObject);
    if (callData.type == CallData::Type::None)
        return throwTypeError(globalObject, scope, message);

    ASSERT_WITH_MESSAGE(callData.type == CallData::Type::JS || callData.type == CallData::Type::Native, "Expected object to be callable but received %d", static_cast<int>(callData.type));
    ASSERT_WITH_MESSAGE(!thisValue.isEmpty(), "Expected thisValue to be non-empty. Use jsUndefined() if you meant to use undefined.");

    scope.assertNoException();

    ASSERT(!vm.isCollectorBusyOnCurrentThread());

    bool isJSCall = callData.type == CallData::Type::JS;
    JSScope* functionScope = nullptr;
    FunctionExecutable* functionExecutable = nullptr;
    TaggedNativeFunction nativeFunction;
    JSGlobalObject* calleeGlobalObject = nullptr;

    RefPtr<JSC::JITCode> jitCode;
    CodeBlock* newCodeBlock = nullptr;
    if (isJSCall) {
        functionScope = callData.js.scope;
        functionExecutable = callData.js.functionExecutable;
        calleeGlobalObject = functionScope->globalObject();
        if (!vm.isSafeToRecurseSoft()) [[unlikely]]
            return throwStackOverflowError(calleeGlobalObject, scope);
        {
            DeferTraps deferTraps(vm); // We can't jettison this code if we're about to run it.

            // Compile the callee:
            functionExecutable->prepareForExecution<FunctionExecutable>(vm, jsCast<JSFunction*>(functionObject.asCell()), functionScope, CodeSpecializationKind::CodeForCall, newCodeBlock);
            RETURN_IF_EXCEPTION_WITH_TRAPS_DEFERRED(scope, scope.exception());
            ASSERT(newCodeBlock);
            newCodeBlock->m_shouldAlwaysBeInlined = false;
        }
#if CPU(ARM64) && CPU(ADDRESS64) && !ENABLE(C_LOOP)
        if ((sizeof...(args) + 1) >= newCodeBlock->numParameters()) [[likely]] {
            auto* entry = functionExecutable->generatedJITCodeForCall()->addressForCall();
            auto* callee = asObject(functionObject.asCell());
            if constexpr (!sizeof...(args))
                return JSValue::decode(vmEntryToJavaScriptWith0Arguments(entry, &vm, newCodeBlock, callee, thisValue, context));
            else if constexpr (sizeof...(args) == 1)
                return JSValue::decode(vmEntryToJavaScriptWith1Arguments(entry, &vm, newCodeBlock, callee, thisValue, context, args...));
            else if constexpr (sizeof...(args) == 2)
                return JSValue::decode(vmEntryToJavaScriptWith2Arguments(entry, &vm, newCodeBlock, callee, thisValue, context, args...));
            else if constexpr (sizeof...(args) == 3)
                return JSValue::decode(vmEntryToJavaScriptWith3Arguments(entry, &vm, newCodeBlock, callee, thisValue, context, args...));
            else if constexpr (sizeof...(args) == 4)
                return JSValue::decode(vmEntryToJavaScriptWith4Arguments(entry, &vm, newCodeBlock, callee, thisValue, context, args...));
            else if constexpr (sizeof...(args) == 5)
                return JSValue::decode(vmEntryToJavaScriptWith5Arguments(entry, &vm, newCodeBlock, callee, thisValue, context, args...));
            else if constexpr (sizeof...(args) == 6)
                return JSValue::decode(vmEntryToJavaScriptWith6Arguments(entry, &vm, newCodeBlock, callee, thisValue, context, args...));
            else
                return { };
        }
#endif
        {
            AssertNoGC assertNoGC; // Ensure no GC happens. GC can replace CodeBlock in Executable.
            jitCode = functionExecutable->generatedJITCodeForCall();
        }
    } else {
        ASSERT(callData.type == CallData::Type::Native);
        nativeFunction = callData.native.function;
        calleeGlobalObject = asObject(functionObject)->globalObject();
        if (!vm.isSafeToRecurseSoft()) [[unlikely]]
            return throwStackOverflowError(calleeGlobalObject, scope);
    }

    // For native calls, fall back to the regular path
    scope.release();
    ProtoCallFrame protoCallFrame;
    std::array<EncodedJSValue, sizeof...(args)> argArray = { { JSValue::encode(args)... } };
    protoCallFrame.init(newCodeBlock, calleeGlobalObject, asObject(functionObject), thisValue, context, sizeof...(args) + 1, argArray.data());

    if (isJSCall) {
        ASSERT(jitCode == functionExecutable->generatedJITCodeForCall().ptr());
        return JSValue::decode(vmEntryToJavaScript(jitCode->addressForCall(), &vm, &protoCallFrame));
    }

#if ENABLE(WEBASSEMBLY)
    if (callData.native.isWasm)
        return JSValue::decode(vmEntryToWasm(jsCast<WebAssemblyFunction*>(functionObject)->jsToWasm(ArityCheckMode::MustCheckArity).taggedPtr(), &vm, &protoCallFrame));
#endif

    return JSValue::decode(vmEntryToNative(nativeFunction.taggedPtr(), &vm, &protoCallFrame));
}

static void promiseResolveThenableJobFastSlow(JSGlobalObject* globalObject, JSPromise* promise, JSPromise* promiseToResolve)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_TOP_EXCEPTION_SCOPE(vm);

    JSObject* constructor = promiseSpeciesConstructor(globalObject, promise);
    if (scope.exception()) [[unlikely]]
        return;

    auto [resolve, reject] = promiseToResolve->createResolvingFunctions(vm, globalObject);

    auto capability = JSPromise::createNewPromiseCapability(globalObject, constructor);
    if (!scope.exception()) [[likely]] {
        promise->performPromiseThen(vm, globalObject, resolve, reject, capability);
        return;
    }

    JSValue error = scope.exception()->value();
    if (!scope.clearExceptionExceptTermination()) [[unlikely]]
        return;

    MarkedArgumentBuffer arguments;
    arguments.append(error);
    ASSERT(!arguments.hasOverflowed());
    auto callData = JSC::getCallDataInline(reject);
    call(globalObject, reject, callData, jsUndefined(), arguments);
    EXCEPTION_ASSERT(scope.exception() || true);
}

static void promiseResolveThenableJobWithInternalMicrotaskFastSlow(JSGlobalObject* globalObject, JSPromise* promise, InternalMicrotask task, JSValue context)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_TOP_EXCEPTION_SCOPE(vm);

    JSObject* constructor = promiseSpeciesConstructor(globalObject, promise);
    if (scope.exception()) [[unlikely]]
        return;

    auto [resolve, reject] = JSPromise::createResolvingFunctionsWithInternalMicrotask(vm, globalObject, task, context);

    auto capability = JSPromise::createNewPromiseCapability(globalObject, constructor);
    if (!scope.exception()) [[likely]] {
        promise->performPromiseThen(vm, globalObject, resolve, reject, capability);
        return;
    }

    JSValue error = scope.exception()->value();
    if (!scope.clearExceptionExceptTermination()) [[unlikely]]
        return;

    MarkedArgumentBuffer arguments;
    arguments.append(error);
    ASSERT(!arguments.hasOverflowed());
    auto callData = JSC::getCallDataInline(reject);
    call(globalObject, reject, callData, jsUndefined(), arguments);
    EXCEPTION_ASSERT(scope.exception() || true);
}

static void promiseResolveThenableJob(JSGlobalObject* globalObject, JSValue promise, JSValue then, JSValue resolve, JSValue reject)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_TOP_EXCEPTION_SCOPE(vm);

    {
        callMicrotask(globalObject, then, promise, dynamicCastToCell(then), "|then| is not a function"_s, resolve, reject);
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

static void asyncFromSyncIteratorContinueOrDone(JSGlobalObject* globalObject, VM& vm, JSValue context, JSValue result, JSPromise::Status status, bool done)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* contextObject = asObject(context);
    JSValue promise = contextObject->getDirect(vm, vm.propertyNames->builtinNames().promisePrivateName());
    ASSERT(promise.inherits<JSPromise>());

    switch (status) {
    case JSPromise::Status::Pending: {
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
    case JSPromise::Status::Rejected: {
        JSValue syncIterator = contextObject->getDirect(vm, vm.propertyNames->builtinNames().syncIteratorPrivateName());
        if (syncIterator.isObject()) {
            JSValue returnMethod;
            JSValue error;
            {
                auto catchScope = DECLARE_TOP_EXCEPTION_SCOPE(vm);
                returnMethod = asObject(syncIterator)->get(globalObject, vm.propertyNames->returnKeyword);
                if (catchScope.exception()) [[unlikely]] {
                    error = catchScope.exception()->value();
                    if (!catchScope.clearExceptionExceptTermination()) [[unlikely]] {
                        scope.release();
                        return;
                    }
                }
            }
            if (error) [[unlikely]] {
                jsCast<JSPromise*>(promise)->reject(vm, globalObject, error);
                return;
            }
            if (returnMethod.isCallable()) {
                callMicrotask(globalObject, returnMethod, syncIterator, dynamicCastToCell(returnMethod), "return is not a function"_s);
                if (scope.exception()) [[unlikely]]
                    return;
            }
        }
        scope.release();
        jsCast<JSPromise*>(promise)->reject(vm, globalObject, result);
        break;
    }
    case JSPromise::Status::Fulfilled: {
        auto* resultObject = createIteratorResultObject(globalObject, result, done);
        scope.release();
        jsCast<JSPromise*>(promise)->resolve(globalObject, resultObject);
        break;
    }
    }
}

static void promiseRaceResolveJob(JSGlobalObject* globalObject, VM& vm, JSPromise* promise, JSValue resolution, JSPromise::Status status)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (promise->status() != JSPromise::Status::Pending)
        return;

    switch (status) {
    case JSPromise::Status::Pending: {
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
    case JSPromise::Status::Fulfilled: {
        scope.release();
        promise->resolve(globalObject, resolution);
        break;
    }
    case JSPromise::Status::Rejected: {
        scope.release();
        promise->reject(vm, globalObject, resolution);
        break;
    }
    }
}

static void promiseAllResolveJob(JSGlobalObject* globalObject, VM& vm, JSPromise* promise, JSValue resolution, JSPromiseCombinatorsContext* context, JSPromise::Status status)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* globalContext = jsCast<JSPromiseCombinatorsGlobalContext*>(context->globalContext());
    switch (status) {
    case JSPromise::Status::Pending: {
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
    case JSPromise::Status::Fulfilled: {
        auto* values = jsCast<JSArray*>(globalContext->values());
        uint64_t index = context->index();

        values->putDirectIndex(globalObject, index, resolution);
        RETURN_IF_EXCEPTION(scope, void());

        uint64_t count = globalContext->remainingElementsCount().toIndex(globalObject, "count exceeds size"_s);
        RETURN_IF_EXCEPTION(scope, void());

        --count;
        globalContext->setRemainingElementsCount(vm, jsNumber(count));
        if (!count) {
            scope.release();
            promise->resolve(globalObject, values);
        }
        break;
    }
    case JSPromise::Status::Rejected: {
        scope.release();
        promise->reject(vm, globalObject, resolution);
        break;
    }
    }
}

// This is similar to promiseAllResolveJob but uses fulfill instead of resolve.
// This is used for InternalPromise.internalAll to avoid looking up the then property
// which could have user-observable side effects.
static void internalPromiseAllResolveJob(JSGlobalObject* globalObject, VM& vm, JSPromise* promise, JSValue resolution, JSPromiseCombinatorsContext* context, JSPromise::Status status)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* globalContext = jsCast<JSPromiseCombinatorsGlobalContext*>(context->globalContext());
    switch (status) {
    case JSPromise::Status::Pending: {
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
    case JSPromise::Status::Fulfilled: {
        auto* values = jsCast<JSArray*>(globalContext->values());
        uint64_t index = context->index();

        values->putDirectIndex(globalObject, index, resolution);
        RETURN_IF_EXCEPTION(scope, void());

        uint64_t count = globalContext->remainingElementsCount().toIndex(globalObject, "count exceeds size"_s);
        RETURN_IF_EXCEPTION(scope, void());

        --count;
        globalContext->setRemainingElementsCount(vm, jsNumber(count));
        if (!count) {
            scope.release();
            // Use fulfill instead of resolve to avoid looking up the then property.
            promise->fulfill(vm, globalObject, values);
        }
        break;
    }
    case JSPromise::Status::Rejected: {
        scope.release();
        promise->reject(vm, globalObject, resolution);
        break;
    }
    }
}

static void promiseAllSettledResolveJob(JSGlobalObject* globalObject, VM& vm, JSPromise* promise, JSValue resolution, JSPromiseCombinatorsContext* context, JSPromise::Status status)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* globalContext = jsCast<JSPromiseCombinatorsGlobalContext*>(context->globalContext());
    auto* values = jsCast<JSArray*>(globalContext->values());
    uint64_t index = context->index();

    JSObject* resultObject = nullptr;
    switch (status) {
    case JSPromise::Status::Pending: {
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
    case JSPromise::Status::Fulfilled: {
        resultObject = createPromiseAllSettledFulfilledResult(globalObject, resolution);
        break;
    }
    case JSPromise::Status::Rejected: {
        resultObject = createPromiseAllSettledRejectedResult(globalObject, resolution);
        break;
    }
    }

    values->putDirectIndex(globalObject, index, resultObject);
    RETURN_IF_EXCEPTION(scope, void());

    uint64_t count = globalContext->remainingElementsCount().toIndex(globalObject, "count exceeds size"_s);
    RETURN_IF_EXCEPTION(scope, void());

    --count;
    globalContext->setRemainingElementsCount(vm, jsNumber(count));
    if (!count) {
        scope.release();
        promise->resolve(globalObject, values);
    }
}

static void promiseAnyResolveJob(JSGlobalObject* globalObject, VM& vm, JSPromise* promise, JSValue resolution, JSPromiseCombinatorsContext* context, JSPromise::Status status)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* globalContext = jsCast<JSPromiseCombinatorsGlobalContext*>(context->globalContext());

    switch (status) {
    case JSPromise::Status::Pending: {
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
    case JSPromise::Status::Fulfilled: {
        scope.release();
        promise->resolve(globalObject, resolution);
        break;
    }
    case JSPromise::Status::Rejected: {
        auto* errors = jsCast<JSArray*>(globalContext->values());
        uint64_t index = context->index();

        errors->putDirectIndex(globalObject, index, resolution);
        RETURN_IF_EXCEPTION(scope, void());

        uint64_t count = globalContext->remainingElementsCount().toIndex(globalObject, "count exceeds size"_s);
        RETURN_IF_EXCEPTION(scope, void());

        --count;
        globalContext->setRemainingElementsCount(vm, jsNumber(count));
        if (!count) {
            auto* aggregateError = createAggregateError(vm, globalObject->errorStructure(ErrorType::AggregateError), errors, String(), jsUndefined());
            scope.release();
            promise->reject(vm, globalObject, aggregateError);
        }
        break;
    }
    }
}

static bool isSuspendYieldState(int32_t state)
{
    return state > 0 && (state & JSAsyncGenerator::reasonMask) == static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorSuspendReason::Yield);
}

static void asyncGeneratorResumeNext(JSGlobalObject*, JSAsyncGenerator*);

template<IterationStatus status>
static void asyncGeneratorReject(JSGlobalObject* globalObject, JSAsyncGenerator* generator, JSValue error)
{
    VM& vm = globalObject->vm();

    auto [value, resumeMode, promise] = generator->dequeue(vm);
    ASSERT(promise);

    promise->reject(vm, globalObject, error);

    if constexpr (status == IterationStatus::Continue)
        asyncGeneratorResumeNext(globalObject, generator);
}

template<IterationStatus status>
static void asyncGeneratorResolve(JSGlobalObject* globalObject, JSAsyncGenerator* generator, JSValue value, bool done)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto [itemValue, itemResumeMode, promise] = generator->dequeue(vm);
    ASSERT(promise);

    auto* iteratorResult = createIteratorResultObject(globalObject, value, done);

    promise->resolve(globalObject, iteratorResult);
    RETURN_IF_EXCEPTION(scope, void());

    if constexpr (status == IterationStatus::Continue)
        RELEASE_AND_RETURN(scope, asyncGeneratorResumeNext(globalObject, generator));
}

template<IterationStatus status>
static bool asyncGeneratorBodyCall(JSGlobalObject* globalObject, JSAsyncGenerator* generator, JSValue resumeValue, int32_t resumeMode)
{
    VM& vm = globalObject->vm();

    int32_t state = generator->state();
    if (resumeMode == static_cast<int32_t>(JSGenerator::ResumeMode::ReturnMode) && isSuspendYieldState(state)) {
        state = (state & ~JSAsyncGenerator::reasonMask) | static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorSuspendReason::Await);
        generator->setState(state);
        JSPromise::resolveWithInternalMicrotaskForAsyncAwait(globalObject, resumeValue, InternalMicrotask::AsyncGeneratorBodyCallReturn, generator);
        return false;
    }

    generator->setState(static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::Executing));

    JSValue generatorFunction = generator->next();
    JSValue generatorThis = generator->thisValue();
    JSValue generatorFrame = generator->frame();

    JSValue value;
    JSValue error;
    {
        auto scope = DECLARE_TOP_EXCEPTION_SCOPE(vm);
        value = callMicrotask(globalObject, generatorFunction, generatorThis, generator, "handler is not a function"_s,
            generator, jsNumber(state >> JSAsyncGenerator::reasonShift), resumeValue, jsNumber(resumeMode), generatorFrame);
        if (scope.exception()) [[unlikely]] {
            error = scope.exception()->value();
            if (!scope.clearExceptionExceptTermination()) [[unlikely]]
                return false;
        }
    }

    if (error) [[unlikely]] {
        generator->setState(static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::Completed));
        asyncGeneratorReject<status>(globalObject, generator, error);
        return true;
    }

    state = generator->state();
    if (state == static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::Executing)) {
        generator->setState(static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::Completed));
        state = static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::Completed);
    }

    if (state > 0) {
        if ((state & JSAsyncGenerator::reasonMask) == static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorSuspendReason::Await)) {
            JSPromise::resolveWithInternalMicrotaskForAsyncAwait(globalObject, value, InternalMicrotask::AsyncGeneratorBodyCallNormal, generator);
            return false;
        }

        state = (state & ~JSAsyncGenerator::reasonMask) | static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorSuspendReason::Await);
        generator->setState(state);
        JSPromise::resolveWithInternalMicrotaskForAsyncAwait(globalObject, value, InternalMicrotask::AsyncGeneratorYieldAwaited, generator);
        return false;
    }

    if (state == static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::Completed)) {
        asyncGeneratorResolve<status>(globalObject, generator, value, true);
        return true;
    }

    return false;
}

static void asyncGeneratorResumeNext(JSGlobalObject* globalObject, JSAsyncGenerator* generator)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    while (true) {
        int32_t state = generator->state();

        ASSERT(state != static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::Executing));

        if (state == static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::AwaitingReturn))
            return;

        if (generator->isQueueEmpty())
            return;

        JSValue nextValue = generator->resumeValue();
        int32_t resumeMode = generator->resumeMode();

        if (resumeMode != static_cast<int32_t>(JSGenerator::ResumeMode::NormalMode)) {
            if (state == static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::Init)) {
                generator->setState(static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::Completed));
                state = static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::Completed);
            }

            if (state == static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::Completed)) {
                if (resumeMode == static_cast<int32_t>(JSGenerator::ResumeMode::ReturnMode)) {
                    generator->setState(static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::AwaitingReturn));
                    RELEASE_AND_RETURN(scope, JSPromise::resolveWithInternalMicrotaskForAsyncAwait(globalObject, nextValue, InternalMicrotask::AsyncGeneratorResumeNext, generator));
                }

                ASSERT(resumeMode == static_cast<int32_t>(JSGenerator::ResumeMode::ThrowMode));
                asyncGeneratorReject<IterationStatus::Done>(globalObject, generator, nextValue);
                continue;
            }
        } else if (state == static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::Completed)) {
            asyncGeneratorResolve<IterationStatus::Done>(globalObject, generator, jsUndefined(), true);
            RETURN_IF_EXCEPTION(scope, void());
            continue;
        }

        ASSERT(state == static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::Init) || isSuspendYieldState(state));
        bool next = asyncGeneratorBodyCall<IterationStatus::Done>(globalObject, generator, nextValue, resumeMode);
        RETURN_IF_EXCEPTION(scope, void());
        if (!next)
            return;
    }
}

static void asyncGeneratorYieldAwaited(JSGlobalObject* globalObject, JSAsyncGenerator* generator, JSValue result, JSPromise::Status status)
{
    switch (status) {
    case JSPromise::Status::Pending:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    case JSPromise::Status::Rejected:
        asyncGeneratorBodyCall<IterationStatus::Continue>(globalObject, generator, result, static_cast<int32_t>(JSGenerator::ResumeMode::ThrowMode));
        return;
    case JSPromise::Status::Fulfilled: {
        int32_t state = generator->state();
        state = (state & ~JSAsyncGenerator::reasonMask) | static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorSuspendReason::Yield);
        generator->setState(state);
        asyncGeneratorResolve<IterationStatus::Continue>(globalObject, generator, result, false);
        return;
    }
    }
}

static void asyncGeneratorBodyCallNormal(JSGlobalObject* globalObject, JSAsyncGenerator* generator, JSValue result, JSPromise::Status status)
{
    switch (status) {
    case JSPromise::Status::Pending:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    case JSPromise::Status::Rejected:
        asyncGeneratorBodyCall<IterationStatus::Continue>(globalObject, generator, result, static_cast<int32_t>(JSGenerator::ResumeMode::ThrowMode));
        return;
    case JSPromise::Status::Fulfilled:
        asyncGeneratorBodyCall<IterationStatus::Continue>(globalObject, generator, result, static_cast<int32_t>(JSGenerator::ResumeMode::NormalMode));
        return;
    }
}

static void asyncGeneratorBodyCallReturn(JSGlobalObject* globalObject, JSAsyncGenerator* generator, JSValue result, JSPromise::Status status)
{
    switch (status) {
    case JSPromise::Status::Pending:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    case JSPromise::Status::Rejected:
        asyncGeneratorBodyCall<IterationStatus::Continue>(globalObject, generator, result, static_cast<int32_t>(JSGenerator::ResumeMode::ThrowMode));
        return;
    case JSPromise::Status::Fulfilled:
        asyncGeneratorBodyCall<IterationStatus::Continue>(globalObject, generator, result, static_cast<int32_t>(JSGenerator::ResumeMode::ReturnMode));
        return;
    }
}

static void asyncGeneratorResumeNextReturn(JSGlobalObject* globalObject, JSAsyncGenerator* generator, JSValue result, JSPromise::Status status)
{
    generator->setState(static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::Completed));

    switch (status) {
    case JSPromise::Status::Pending:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    case JSPromise::Status::Rejected:
        asyncGeneratorReject<IterationStatus::Continue>(globalObject, generator, result);
        return;
    case JSPromise::Status::Fulfilled:
        asyncGeneratorResolve<IterationStatus::Continue>(globalObject, generator, result, true);
        return;
    }
}

static void promiseFinallyAwaitJob(JSGlobalObject* globalObject, VM& vm, JSValue settledValue, JSPromiseCombinatorsGlobalContext* context, JSPromise::Status status)
{
    auto* resultPromise = jsCast<JSPromise*>(context->promise());
    JSValue originalValue = context->values();
    bool wasFulfilled = context->remainingElementsCount().asBoolean();

    if (status == JSPromise::Status::Rejected) {
        resultPromise->rejectPromise(vm, globalObject, settledValue);
        return;
    }

    if (wasFulfilled)
        resultPromise->resolvePromise(globalObject, originalValue);
    else
        resultPromise->rejectPromise(vm, globalObject, originalValue);
}

static void promiseFinallyReactionJob(JSGlobalObject* globalObject, VM& vm, JSPromise* resultPromise, JSValue valueOrReason, JSPromiseCombinatorsGlobalContext* context, JSPromise::Status status)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue onFinally = context->values();

    JSValue result;
    JSValue error;
    {
        auto catchScope = DECLARE_TOP_EXCEPTION_SCOPE(vm);
        result = callMicrotask(globalObject, onFinally, jsUndefined(), dynamicCastToCell(onFinally), "onFinally is not a function"_s);
        if (catchScope.exception()) {
            error = catchScope.exception()->value();
            if (!catchScope.clearExceptionExceptTermination()) [[unlikely]] {
                scope.release();
                return;
            }
        }
    }

    if (error) {
        resultPromise->rejectPromise(vm, globalObject, error);
        return;
    }

    context->setValues(vm, valueOrReason);
    context->setRemainingElementsCount(vm, jsBoolean(status == JSPromise::Status::Fulfilled));

    if (result.inherits<JSPromise>()) {
        auto* promise = jsCast<JSPromise*>(result);
        if (promise->isThenFastAndNonObservable()) {
            scope.release();
            promise->performPromiseThenWithInternalMicrotask(vm, globalObject, InternalMicrotask::PromiseFinallyAwaitJob, resultPromise, context);
            return;
        }
    }

    if (!result.isObject()) {
        scope.release();
        promiseFinallyAwaitJob(globalObject, vm, result, context, JSPromise::Status::Fulfilled);
        return;
    }

    auto* resolutionObject = asObject(result);
    if (isDefinitelyNonThenable(resolutionObject, globalObject)) {
        scope.release();
        promiseFinallyAwaitJob(globalObject, vm, result, context, JSPromise::Status::Fulfilled);
        return;
    }

    JSValue then;
    {
        auto catchScope = DECLARE_TOP_EXCEPTION_SCOPE(vm);
        then = resolutionObject->get(globalObject, vm.propertyNames->then);
        if (catchScope.exception()) [[unlikely]] {
            error = catchScope.exception()->value();
            if (!catchScope.clearExceptionExceptTermination()) [[unlikely]] {
                scope.release();
                return;
            }
        }
    }
    if (error) [[unlikely]] {
        scope.release();
        promiseFinallyAwaitJob(globalObject, vm, error, context, JSPromise::Status::Rejected);
        return;
    }

    if (!then.isCallable()) [[likely]] {
        scope.release();
        promiseFinallyAwaitJob(globalObject, vm, result, context, JSPromise::Status::Fulfilled);
        return;
    }

    auto [resolve, reject] = JSPromise::createResolvingFunctionsWithInternalMicrotask(vm, globalObject, InternalMicrotask::PromiseFinallyAwaitJob, context);
    scope.release();
    promiseResolveThenableJob(globalObject, resolutionObject, then, resolve, reject);
}

void runInternalMicrotask(JSGlobalObject* globalObject, InternalMicrotask task, uint8_t payload, std::span<const JSValue, maxMicrotaskArguments> arguments)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    switch (task) {
    case InternalMicrotask::PromiseResolveThenableJobFast: {
        auto* promise = jsCast<JSPromise*>(arguments[0]);
        auto* promiseToResolve = jsCast<JSPromise*>(arguments[1]);

        if (!promiseSpeciesWatchpointIsValid(vm, promise)) [[unlikely]]
            RELEASE_AND_RETURN(scope, promiseResolveThenableJobFastSlow(globalObject, promise, promiseToResolve));

        scope.release();
        promise->performPromiseThenWithInternalMicrotask(vm, globalObject, InternalMicrotask::PromiseResolveWithoutHandlerJob, promiseToResolve, jsUndefined());
        return;
    }

    case InternalMicrotask::PromiseResolveThenableJobWithInternalMicrotaskFast: {
        auto* promise = jsCast<JSPromise*>(arguments[0]);
        JSValue context = arguments[1];
        auto task = static_cast<InternalMicrotask>(payload);

        if (!promiseSpeciesWatchpointIsValid(vm, promise)) [[unlikely]]
            RELEASE_AND_RETURN(scope, promiseResolveThenableJobWithInternalMicrotaskFastSlow(globalObject, promise, task, context));

        switch (promise->status()) {
        case JSPromise::Status::Pending: {
            JSValue encodedTask = jsNumber(static_cast<int32_t>(task));
            auto* reaction = JSPromiseReaction::create(vm, jsUndefined(), encodedTask, encodedTask, context, jsDynamicCast<JSPromiseReaction*>(promise->reactionsOrResult()));
            promise->setReactionsOrResult(vm, reaction);
            break;
        }
        case JSPromise::Status::Rejected: {
            if (!promise->isHandled())
                globalObject->globalObjectMethodTable()->promiseRejectionTracker(globalObject, promise, JSPromiseRejectionOperation::Handle);
            JSPromise::rejectWithInternalMicrotask(globalObject, promise->reactionsOrResult(), task, context);
            break;
        }
        case JSPromise::Status::Fulfilled: {
            JSPromise::fulfillWithInternalMicrotask(globalObject, promise->reactionsOrResult(), task, context);
            break;
        }
        }

        promise->markAsHandled();
        return;
    }

    case InternalMicrotask::PromiseResolveThenableJob: {
        JSValue promise = arguments[0];
        JSValue then = arguments[1];
        JSPromise* promiseToResolve = jsCast<JSPromise*>(arguments[2]);
        auto [resolve, reject] = promiseToResolve->createResolvingFunctions(vm, globalObject);
        RELEASE_AND_RETURN(scope, promiseResolveThenableJob(globalObject, promise, then, resolve, reject));
    }

    case InternalMicrotask::PromiseResolveThenableJobWithInternalMicrotask: {
        auto task = static_cast<InternalMicrotask>(payload);
        JSValue promise = arguments[0];
        JSValue then = arguments[1];
        JSValue context = arguments[2];
        auto [resolve, reject] = JSPromise::createResolvingFunctionsWithInternalMicrotask(vm, globalObject, task, context);
        RELEASE_AND_RETURN(scope, promiseResolveThenableJob(globalObject, promise, then, resolve, reject));
    }

    case InternalMicrotask::PromiseResolveWithoutHandlerJob: {
        auto* promise = jsCast<JSPromise*>(arguments[0]);
        JSValue resolution = arguments[1];
        switch (static_cast<JSPromise::Status>(payload)) {
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
            promise->rejectPromise(vm, globalObject, resolution);
            break;
        }
        }
        return;
    }

    case InternalMicrotask::PromiseRaceResolveJob:
        RELEASE_AND_RETURN(scope, promiseRaceResolveJob(globalObject, vm, jsCast<JSPromise*>(arguments[0]), arguments[1], static_cast<JSPromise::Status>(payload)));

    case InternalMicrotask::PromiseAllResolveJob:
        RELEASE_AND_RETURN(scope, promiseAllResolveJob(globalObject, vm, jsCast<JSPromise*>(arguments[0]), arguments[1], jsCast<JSPromiseCombinatorsContext*>(arguments[2]), static_cast<JSPromise::Status>(payload)));

    case InternalMicrotask::PromiseAllSettledResolveJob:
        RELEASE_AND_RETURN(scope, promiseAllSettledResolveJob(globalObject, vm, jsCast<JSPromise*>(arguments[0]), arguments[1], jsCast<JSPromiseCombinatorsContext*>(arguments[2]), static_cast<JSPromise::Status>(payload)));

    case InternalMicrotask::PromiseAnyResolveJob:
        RELEASE_AND_RETURN(scope, promiseAnyResolveJob(globalObject, vm, jsCast<JSPromise*>(arguments[0]), arguments[1], jsCast<JSPromiseCombinatorsContext*>(arguments[2]), static_cast<JSPromise::Status>(payload)));

    case InternalMicrotask::InternalPromiseAllResolveJob:
        RELEASE_AND_RETURN(scope, internalPromiseAllResolveJob(globalObject, vm, jsCast<JSPromise*>(arguments[0]), arguments[1], jsCast<JSPromiseCombinatorsContext*>(arguments[2]), static_cast<JSPromise::Status>(payload)));

    case InternalMicrotask::PromiseReactionJob: {
        JSValue promiseOrCapability = arguments[0];
        JSValue handler = arguments[1];

        JSValue result;
        JSValue error;
        {
            auto catchScope = DECLARE_TOP_EXCEPTION_SCOPE(vm);
            result = callMicrotask(globalObject, handler, jsUndefined(), dynamicCastToCell(handler), "handler is not a function"_s, arguments[2]);
            if (catchScope.exception()) {
                if (promiseOrCapability.isUndefinedOrNull()) {
                    scope.release();
                    return;
                }
                error = catchScope.exception()->value();
                if (!catchScope.clearExceptionExceptTermination()) [[unlikely]] {
                    scope.release();
                    return;
                }
            }

            if (promiseOrCapability.isUndefinedOrNull()) {
                scope.release();
                return;
            }
        }

        if (error) {
            if (auto* promise = jsDynamicCast<JSPromise*>(promiseOrCapability))
                RELEASE_AND_RETURN(scope, promise->rejectPromise(vm, globalObject, error));

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

    case InternalMicrotask::InvokeFunctionJob: {
        JSValue handler = arguments[0];
        scope.release();
        callMicrotask(globalObject, handler, jsUndefined(), nullptr, "handler is not a function"_s);
        return;
    }

    case InternalMicrotask::AsyncFunctionResume: {
        JSValue resolution = arguments[1];
        auto* generator = jsCast<JSGenerator*>(arguments[2]);
        JSGenerator::ResumeMode resumeMode = JSGenerator::ResumeMode::NormalMode;
        switch (static_cast<JSPromise::Status>(payload)) {
        case JSPromise::Status::Pending: {
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        case JSPromise::Status::Rejected: {
            resumeMode = JSGenerator::ResumeMode::ThrowMode;
            break;
        }
        case JSPromise::Status::Fulfilled: {
            resumeMode = JSGenerator::ResumeMode::NormalMode;
            break;
        }
        }

        int32_t state = generator->state();
        generator->setState(static_cast<int32_t>(JSGenerator::State::Executing));
        JSValue next = generator->next();
        JSValue thisValue = generator->thisValue();
        JSValue frame = generator->frame();
        JSValue value;
        JSValue error;
        {
            auto catchScope = DECLARE_TOP_EXCEPTION_SCOPE(vm);
            value = callMicrotask(globalObject, next, thisValue, generator, "handler is not a function"_s,
                generator, jsNumber(state), resolution, jsNumber(static_cast<int32_t>(resumeMode)), frame);
            if (catchScope.exception()) {
                error = catchScope.exception()->value();
                if (!catchScope.clearExceptionExceptTermination()) [[unlikely]] {
                    scope.release();
                    return;
                }
            }
        }

        if (error) {
            auto* promise = jsCast<JSPromise*>(generator->context());
            scope.release();
            promise->reject(vm, globalObject, error);
            return;
        }

        if (generator->state() == static_cast<int32_t>(JSGenerator::State::Executing)) {
            auto* promise = jsCast<JSPromise*>(generator->context());
            scope.release();
            promise->resolve(globalObject, value);
            return;
        }

        scope.release();
        JSPromise::resolveWithInternalMicrotaskForAsyncAwait(globalObject, value, InternalMicrotask::AsyncFunctionResume, generator);
        return;
    }

    case InternalMicrotask::AsyncFromSyncIteratorContinue:
    case InternalMicrotask::AsyncFromSyncIteratorDone:
        RELEASE_AND_RETURN(scope, asyncFromSyncIteratorContinueOrDone(globalObject, vm, arguments[2], arguments[1], static_cast<JSPromise::Status>(payload), task == InternalMicrotask::AsyncFromSyncIteratorDone));

    case InternalMicrotask::AsyncGeneratorYieldAwaited: {
        RELEASE_AND_RETURN(scope, asyncGeneratorYieldAwaited(globalObject, jsCast<JSAsyncGenerator*>(arguments[2]), arguments[1], static_cast<JSPromise::Status>(payload)));
    }

    case InternalMicrotask::AsyncGeneratorBodyCallNormal: {
        RELEASE_AND_RETURN(scope, asyncGeneratorBodyCallNormal(globalObject, jsCast<JSAsyncGenerator*>(arguments[2]), arguments[1], static_cast<JSPromise::Status>(payload)));
    }

    case InternalMicrotask::AsyncGeneratorBodyCallReturn: {
        RELEASE_AND_RETURN(scope, asyncGeneratorBodyCallReturn(globalObject, jsCast<JSAsyncGenerator*>(arguments[2]), arguments[1], static_cast<JSPromise::Status>(payload)));
    }

    case InternalMicrotask::AsyncGeneratorResumeNext: {
        RELEASE_AND_RETURN(scope, asyncGeneratorResumeNextReturn(globalObject, jsCast<JSAsyncGenerator*>(arguments[2]), arguments[1], static_cast<JSPromise::Status>(payload)));
    }

    case InternalMicrotask::PromiseFinallyReactionJob: {
        // Phase 1: Original promise settled
        // arguments[0] = resultPromise
        // arguments[1] = value/reason from original promise
        // arguments[2] = context (JSPromiseCombinatorsGlobalContext: promise=resultPromise, values=onFinally)
        // payload = Fulfilled/Rejected status
        scope.release();
        promiseFinallyReactionJob(globalObject, vm,
            jsCast<JSPromise*>(arguments[0]),
            arguments[1],
            jsCast<JSPromiseCombinatorsGlobalContext*>(arguments[2]),
            static_cast<JSPromise::Status>(payload));
        return;
    }

    case InternalMicrotask::PromiseFinallyAwaitJob: {
        // Phase 2: onFinally's result settled
        // arguments[0] = unused (we get resultPromise from context)
        // arguments[1] = settled value from onFinally's result
        // arguments[2] = context (JSPromiseCombinatorsGlobalContext: promise=resultPromise, values=originalValue, remainingElementsCount=wasFulfilled)
        // payload = status of onFinally's result
        scope.release();
        promiseFinallyAwaitJob(globalObject, vm,
            arguments[1],
            jsCast<JSPromiseCombinatorsGlobalContext*>(arguments[2]),
            static_cast<JSPromise::Status>(payload));
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
