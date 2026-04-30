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
#include "MicrotaskCallInlines.h"
#include "JSArray.h"
#include "JSAsyncGenerator.h"
#include "JSFunction.h"
#include "JSGenerator.h"
#include "JSGlobalObject.h"
#include "JSObjectInlines.h"
#include "JSModuleLoader.h"
#include "JSModuleNamespaceObject.h"
#include "JSModuleRecord.h"
#include "JSPromise.h"
#include "JSPromiseCombinatorsContext.h"
#include "JSPromiseCombinatorsGlobalContext.h"
#include "JSPromiseConstructor.h"
#include "JSPromisePrototype.h"
#include "JSPromiseReaction.h"
#include "LLIntThunks.h"
#include "Microtask.h"
#include "ModuleGraphLoadingState.h"
#include "ModuleLoaderPayload.h"
#include "ModuleLoadingContext.h"
#include "ModuleRegistryEntry.h"
#include "ObjectConstructor.h"
#include "ScriptFetcher.h"
#include "ThrowScope.h"
#include "TopExceptionScope.h"
#include "VMTrapsInlines.h"
#include <wtf/NoTailCalls.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

static ALWAYS_INLINE JSCell* NODELETE dynamicCastToCell(JSValue value)
{
    if (value.isCell())
        return value.asCell();
    return nullptr;
}

template<typename... Args> requires (std::is_convertible_v<Args, JSValue> && ...)
static JSValue callMicrotask(JSGlobalObject* globalObject, JSValue functionObject, JSValue thisValue, JSCell* context, ASCIILiteral message, MicrotaskCall* microtaskCall, Args... args)
{
    NO_TAIL_CALLS();

    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    static_assert(sizeof...(args) <= MicrotaskCall::maxCallArguments);

    if (microtaskCall && microtaskCall->canUseCall(functionObject)) [[likely]] {
        if (!vm.isSafeToRecurseSoft()) [[unlikely]]
            return throwStackOverflowError(globalObject, scope);
        auto* jsFunction = uncheckedDowncast<JSFunction>(functionObject.asCell());
        if (auto result = microtaskCall->tryCallWithArguments(vm, jsFunction, thisValue, context, args...)) [[likely]] {
            scope.release();
            return result;
        }
        RETURN_IF_EXCEPTION_WITH_TRAPS_DEFERRED(scope, scope.exception());
    }

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
        if (!vm.isSafeToRecurseSoft()) [[unlikely]]
            return throwStackOverflowError(functionScope->realm(), scope);

        {
            DeferTraps deferTraps(vm); // We can't jettison this code if we're about to run it.

            // Compile the callee:
            functionExecutable->prepareForExecution<FunctionExecutable>(vm, uncheckedDowncast<JSFunction>(functionObject.asCell()), functionScope, CodeSpecializationKind::CodeForCall, newCodeBlock);
            RETURN_IF_EXCEPTION_WITH_TRAPS_DEFERRED(scope, scope.exception());
            ASSERT(newCodeBlock);
            newCodeBlock->m_shouldAlwaysBeInlined = false;
        }

        if (microtaskCall) {
            auto* jsFunction = uncheckedDowncast<JSFunction>(functionObject.asCell());
            microtaskCall->initialize(vm, jsFunction);
            RETURN_IF_EXCEPTION_WITH_TRAPS_DEFERRED(scope, scope.exception());

            if (auto result = microtaskCall->tryCallWithArguments(vm, jsFunction, thisValue, context, args...)) [[likely]] {
                scope.release();
                return result;
            }
            RETURN_IF_EXCEPTION_WITH_TRAPS_DEFERRED(scope, scope.exception());
        }

#if (CPU(ARM64) || CPU(X86_64)) && CPU(ADDRESS64) && !ENABLE(C_LOOP)
        if ((sizeof...(args) + 1) >= newCodeBlock->numParameters()) [[likely]] {
            auto* entry = functionExecutable->generatedJITCodeAddressForCall();
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

        calleeGlobalObject = functionScope->realm();
        {
            AssertNoGC assertNoGC; // Ensure no GC happens. GC can replace CodeBlock in Executable.
            jitCode = functionExecutable->generatedJITCodeForCall();
        }
    } else {
        ASSERT(callData.type == CallData::Type::Native);
        nativeFunction = callData.native.function;
        calleeGlobalObject = asObject(functionObject)->realm();
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
        return JSValue::decode(vmEntryToWasm(uncheckedDowncast<WebAssemblyFunction>(functionObject)->jsToWasm(ArityCheckMode::MustCheckArity).taggedPtr(), &vm, &protoCallFrame));
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
        callMicrotask(globalObject, then, promise, dynamicCastToCell(then), "|then| is not a function"_s, nullptr, resolve, reject);
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
                uncheckedDowncast<JSPromise>(promise)->reject(vm, globalObject, error);
                return;
            }
            if (returnMethod.isCallable()) {
                callMicrotask(globalObject, returnMethod, syncIterator, dynamicCastToCell(returnMethod), "return is not a function"_s, nullptr);
                if (scope.exception()) [[unlikely]]
                    return;
            }
        }
        scope.release();
        uncheckedDowncast<JSPromise>(promise)->reject(vm, globalObject, result);
        break;
    }
    case JSPromise::Status::Fulfilled: {
        auto* resultObject = createIteratorResultObject(globalObject, result, done);
        scope.release();
        uncheckedDowncast<JSPromise>(promise)->resolve(globalObject, vm, resultObject);
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
        promise->resolve(globalObject, vm, resolution);
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

    auto* globalContext = context->globalContext();
    switch (status) {
    case JSPromise::Status::Pending: {
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
    case JSPromise::Status::Fulfilled: {
        auto* values = uncheckedDowncast<JSArray>(globalContext->values());
        uint64_t index = context->index();

        values->putDirectIndex(globalObject, index, resolution);
        RETURN_IF_EXCEPTION(scope, void());

        uint64_t count = globalContext->remainingElementsCount().toIndex(globalObject, "count exceeds size"_s);
        RETURN_IF_EXCEPTION(scope, void());

        --count;
        globalContext->setRemainingElementsCount(vm, jsNumber(count));
        if (!count) {
            scope.release();
            promise->resolve(globalObject, vm, values);
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

    auto* globalContext = context->globalContext();
    auto* values = uncheckedDowncast<JSArray>(globalContext->values());
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
        promise->resolve(globalObject, vm, values);
    }
}

static void promiseAnyResolveJob(JSGlobalObject* globalObject, VM& vm, JSPromise* promise, JSValue resolution, JSPromiseCombinatorsContext* context, JSPromise::Status status)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* globalContext = context->globalContext();

    switch (status) {
    case JSPromise::Status::Pending: {
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
    case JSPromise::Status::Fulfilled: {
        scope.release();
        promise->resolve(globalObject, vm, resolution);
        break;
    }
    case JSPromise::Status::Rejected: {
        auto* errors = uncheckedDowncast<JSArray>(globalContext->values());
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

static bool NODELETE isSuspendYieldState(int32_t state)
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

    promise->resolve(globalObject, vm, iteratorResult);
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
        JSPromise::resolveWithInternalMicrotaskForAsyncAwait(globalObject, vm, resumeValue, InternalMicrotask::AsyncGeneratorBodyCallReturn, generator);
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
        value = callMicrotask(globalObject, generatorFunction, generatorThis, generator, "handler is not a function"_s, nullptr,
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
            JSPromise::resolveWithInternalMicrotaskForAsyncAwait(globalObject, vm, value, InternalMicrotask::AsyncGeneratorBodyCallNormal, generator);
            return false;
        }

        state = (state & ~JSAsyncGenerator::reasonMask) | static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorSuspendReason::Await);
        generator->setState(state);
        JSPromise::resolveWithInternalMicrotaskForAsyncAwait(globalObject, vm, value, InternalMicrotask::AsyncGeneratorYieldAwaited, generator);
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
                    RELEASE_AND_RETURN(scope, JSPromise::resolveWithInternalMicrotaskForAsyncAwait(globalObject, vm, nextValue, InternalMicrotask::AsyncGeneratorResumeNext, generator));
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
    auto* resultPromise = uncheckedDowncast<JSPromise>(context->promise());
    JSValue originalValue = context->values();
    bool wasFulfilled = context->remainingElementsCount().asBoolean();

    if (status == JSPromise::Status::Rejected) {
        resultPromise->rejectPromise(vm, globalObject, settledValue);
        return;
    }

    if (wasFulfilled)
        resultPromise->resolvePromise(globalObject, vm, originalValue);
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
        result = callMicrotask(globalObject, onFinally, jsUndefined(), dynamicCastToCell(onFinally), "onFinally is not a function"_s, nullptr);
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
        auto* promise = uncheckedDowncast<JSPromise>(result);
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

static void asyncModuleExecutionDone(JSGlobalObject* globalObject, ThrowScope& scope, std::span<const JSValue, maxMicrotaskArguments> arguments, uint8_t payload)
{
    scope.release();
    auto* module = uncheckedDowncast<JSModuleRecord>(arguments[2]);
    auto status = static_cast<JSPromise::Status>(payload);
    if (status == JSPromise::Status::Fulfilled)
        module->asyncExecutionFulfilled(globalObject);
    else {
        ASSERT(status == JSPromise::Status::Rejected);
        module->asyncExecutionRejected(globalObject, arguments[1]);
    }
}

static void asyncModuleExecutionResume(JSGlobalObject* globalObject, VM& vm, ThrowScope& scope, std::span<const JSValue, maxMicrotaskArguments> arguments, uint8_t payload)
{
    auto* module = uncheckedDowncast<JSModuleRecord>(arguments[2]);
    JSValue resolution = arguments[1];
    auto status = static_cast<JSPromise::Status>(payload);
    auto* capability = module->asyncCapability();

    JSValue resumeMode = jsNumber(status == JSPromise::Status::Fulfilled
        ? static_cast<int32_t>(JSGenerator::ResumeMode::NormalMode)
        : static_cast<int32_t>(JSGenerator::ResumeMode::ThrowMode));

    JSValue result = module->evaluate(globalObject, resolution, resumeMode);

    if (scope.exception())
        capability->rejectWithCaughtException(globalObject, scope);
    else {
        JSValue state = module->internalField(AbstractModuleRecord::Field::State).get();
        if (!state.isNumber() || state.asNumber() == static_cast<int32_t>(JSGenerator::State::Executing))
            capability->resolve(globalObject, vm, result);
        else
            JSPromise::resolveWithInternalMicrotaskForAsyncAwait(globalObject, vm, result, InternalMicrotask::AsyncModuleExecutionResume, module);
    }
}

static void moduleRegistryFetchSettled(JSGlobalObject* globalObject, VM& vm, ThrowScope& scope, std::span<const JSValue, maxMicrotaskArguments> arguments, uint8_t payload)
{
    // arguments[0] = pre-created modulePromise
    // arguments[1] = resolution (JSSourceCode*) or rejection (error)
    // arguments[2] = ModuleRegistryEntry*
    auto* entry = uncheckedDowncast<ModuleRegistryEntry>(arguments[2]);
    auto* modulePromise = uncheckedDowncast<JSPromise>(arguments[0]);
    auto status = static_cast<JSPromise::Status>(payload);
    if (status == JSPromise::Status::Fulfilled) {
        auto* jsSourceCode = downcast<JSSourceCode>(arguments[1]);
        JSPromise* makeModulePromise = JSModuleLoader::makeModule(globalObject, entry->key(), jsSourceCode);
        if (scope.exception()) {
            modulePromise->rejectWithCaughtException(globalObject, scope);
            return;
        }
        makeModulePromise->performPromiseThenWithInternalMicrotask(vm, globalObject, InternalMicrotask::ModuleRegistryModuleSettled, modulePromise, entry);
    } else {
        JSValue errorValue = arguments[1];
        if (auto* error = dynamicDowncast<ErrorInstance>(errorValue))
            JSModuleLoader::attachErrorInfo(globalObject, error, nullptr, entry->key(), entry->moduleType(), JSModuleLoader::ModuleFailure::Kind::Instantiation);
        entry->setFetchError(globalObject, errorValue);
        modulePromise->reject(vm, globalObject, errorValue);
    }
}

static void moduleRegistryModuleSettled(JSGlobalObject* globalObject, VM& vm, std::span<const JSValue, maxMicrotaskArguments> arguments, uint8_t payload)
{
    // arguments[0] = pre-created modulePromise
    // arguments[1] = resolution (AbstractModuleRecord*) or rejection (error)
    // arguments[2] = ModuleRegistryEntry*
    auto* entry = uncheckedDowncast<ModuleRegistryEntry>(arguments[2]);
    auto* modulePromise = uncheckedDowncast<JSPromise>(arguments[0]);
    auto status = static_cast<JSPromise::Status>(payload);
    if (status == JSPromise::Status::Fulfilled) {
        auto* moduleRecord = downcast<AbstractModuleRecord>(arguments[1]);
        entry->fetchComplete(globalObject, moduleRecord);
        modulePromise->fulfill(vm, globalObject, moduleRecord);
    } else {
        JSValue errorValue = arguments[1];
        entry->setEvaluationError(globalObject, errorValue);
        modulePromise->reject(vm, globalObject, errorValue);
    }
}

static void moduleGraphLoadingError(JSGlobalObject* globalObject, VM& vm, ThrowScope& scope, std::span<const JSValue, maxMicrotaskArguments> arguments, uint8_t payload)
{
    // arguments[0] = unused (jsUndefined)
    // arguments[1] = resolution value or error
    // arguments[2] = ModuleGraphLoadingState*
    auto status = static_cast<JSPromise::Status>(payload);
    if (status == JSPromise::Status::Rejected) {
        auto* state = uncheckedDowncast<ModuleGraphLoadingState>(arguments[2]);
        JSValue errorValue = arguments[1];
        if (auto* error = dynamicDowncast<ErrorInstance>(errorValue)) {
            errorValue = JSModuleLoader::maybeDuplicateFetchError(globalObject, error);
            RETURN_IF_EXCEPTION(scope, void());
        }
        state->promise()->reject(vm, globalObject, errorValue);
    }
}

static void moduleLoadStep(JSGlobalObject* globalObject, VM& vm, ThrowScope& scope, std::span<const JSValue, maxMicrotaskArguments> arguments, uint8_t payload)
{
    // arguments[0] = pre-created loadPromise
    // arguments[1] = resolution value or error
    // arguments[2] = ModuleLoadingContext*
    auto* context = uncheckedDowncast<ModuleLoadingContext>(arguments[2]);
    auto* loadPromise = uncheckedDowncast<JSPromise>(arguments[0]);
    auto status = static_cast<JSPromise::Status>(payload);

    switch (context->step()) {
    case ModuleLoadingContext::Step::Main: {
        // modulePromise settled: on fulfillment, call loadRequestedModules and chain next step
        if (status == JSPromise::Status::Fulfilled) {
            auto* module = downcast<AbstractModuleRecord>(arguments[1]);
            context->module(vm, module);
            JSPromise* requestedPromise = globalObject->moduleLoader()->loadRequestedModules(globalObject, module, context->scriptFetcher());
            if (scope.exception()) {
                loadPromise->rejectWithCaughtException(globalObject, scope);
                return;
            }
            context->setStep(ModuleLoadingContext::Step::Requested);
            requestedPromise->performPromiseThenWithInternalMicrotask(vm, globalObject, InternalMicrotask::ModuleLoadStep, loadPromise, context);
        } else
            loadPromise->reject(vm, globalObject, arguments[1]);
        return;
    }
    case ModuleLoadingContext::Step::Requested: {
        // loadRequestedModules settled: on fulfillment, call finishLoading and update entry
        if (status == JSPromise::Status::Fulfilled) {
            auto* module = context->module();
            globalObject->moduleLoader()->finishLoadingImportedModule(globalObject, context->referrer(), context->moduleRequest(), context->payload(), module, context->scriptFetcher());
            if (scope.exception()) {
                loadPromise->rejectWithCaughtException(globalObject, scope);
                return;
            }

            // setEntryRecord logic
            auto* entry = context->entry();
            if (auto* cyclic = dynamicDowncast<CyclicModuleRecord>(module); cyclic && cyclic->status() != CyclicModuleRecord::Status::Unlinked) {
                ASSERT(cyclic->status() != CyclicModuleRecord::Status::Linking);
                loadPromise->fulfill(vm, globalObject, entry->record());
            } else {
                entry->setRecord(vm, module);
                entry->setStatus(ModuleRegistryEntry::Status::Fetched);
                loadPromise->fulfill(vm, globalObject, entry->record());
            }
        } else {
            // onRejected logic: store evaluation error on entry
            auto* entry = context->entry();
            JSValue errorValue = arguments[1];
            entry->setEvaluationError(globalObject, errorValue);
            loadPromise->reject(vm, globalObject, errorValue);
        }
        return;
    }
    case ModuleLoadingContext::Step::Cached: {
        // Cached loadPromise settled: on fulfillment, call finishLoading
        if (status == JSPromise::Status::Fulfilled) {
            auto* module = downcast<AbstractModuleRecord>(arguments[1]);
            globalObject->moduleLoader()->finishLoadingImportedModule(globalObject, context->referrer(), context->moduleRequest(), context->payload(), module, context->scriptFetcher());
            if (scope.exception()) {
                loadPromise->rejectWithCaughtException(globalObject, scope);
                return;
            }
            loadPromise->fulfill(vm, globalObject, module);
        } else {
            auto* entry = context->entry();
            JSValue errorValue = arguments[1];
            entry->setEvaluationError(globalObject, errorValue);
            loadPromise->reject(vm, globalObject, errorValue);
        }
        return;
    }
    }

    RELEASE_ASSERT_NOT_REACHED();
}

static void moduleLoadTopSettled(JSGlobalObject* globalObject, VM& vm, ThrowScope& scope, std::span<const JSValue, maxMicrotaskArguments> arguments, uint8_t payload)
{
    // loadModule first overload: fetch promise settled
    // arguments[0] = pre-created intermediatePromise
    // arguments[1] = resolution (JSSourceCode*) or error
    // arguments[2] = ModuleLoadingContext*
    auto* context = uncheckedDowncast<ModuleLoadingContext>(arguments[2]);
    auto* intermediatePromise = uncheckedDowncast<JSPromise>(arguments[0]);
    auto status = static_cast<JSPromise::Status>(payload);
    if (status == JSPromise::Status::Fulfilled) {
        auto* jsSourceCode = downcast<JSSourceCode>(arguments[1]);

        const Identifier& specifier = context->moduleRequest().m_specifier;
        auto type = context->moduleRequest().type();
        ScriptFetcher* scriptFetcher = context->scriptFetcher();

        globalObject->moduleLoader()->provideFetch(globalObject, specifier, type, jsSourceCode);
        if (scope.exception()) {
            intermediatePromise->rejectWithCaughtException(globalObject, scope);
            return;
        }

        JSPromise* statePromise = JSPromise::create(vm, globalObject->promiseStructure());
        statePromise->markAsHandled();
        AbstractModuleRecord::ModuleRequest request { specifier, ScriptFetchParameters::create(type) };
        // combinedCell is the host-defined payload AND the AND-join state for loadPromise+statePromise.
        // For dynamic import we wrap statePromise; for graph load we use the ModuleGraphLoadingState directly.
        JSCell* combinedCell;
        JSPromise* loadPromise;

        if (context->dynamic()) {
            combinedCell = ModuleLoaderPayload::create(vm, statePromise);
            loadPromise = globalObject->moduleLoader()->loadModule(globalObject, globalObject, request, combinedCell, scriptFetcher, false, context->useImportMap());
        } else {
            combinedCell = ModuleGraphLoadingState::create(vm, statePromise, scriptFetcher);
            loadPromise = globalObject->moduleLoader()->loadModule(globalObject, globalObject, request, combinedCell, scriptFetcher, context->evaluate(), context->useImportMap());
            if (scope.exception()) {
                intermediatePromise->rejectWithCaughtException(globalObject, scope);
                return;
            }
            // Specifier transform: instead of creating a closure, use a microtask
            JSPromise* transformedStatePromise = JSPromise::create(vm, globalObject->promiseStructure());
            transformedStatePromise->markAsHandled();
            statePromise->performPromiseThenWithInternalMicrotask(vm, globalObject, InternalMicrotask::ModuleLoadSpecifierTransform, transformedStatePromise, context);
            statePromise = transformedStatePromise;
        }

        if (scope.exception()) {
            intermediatePromise->rejectWithCaughtException(globalObject, scope);
            return;
        }

        JSPromise* combinedPromise = JSPromise::create(vm, globalObject->promiseStructure());
        combinedPromise->markAsHandled();

        loadPromise->performPromiseThenWithInternalMicrotask(vm, globalObject, InternalMicrotask::ModuleLoadCombinedLoadSettled, combinedPromise, combinedCell);
        statePromise->performPromiseThenWithInternalMicrotask(vm, globalObject, InternalMicrotask::ModuleLoadCombinedStateSettled, combinedPromise, combinedCell);

        intermediatePromise->pipeFrom(vm, combinedPromise);
    } else {
        // onFetchRejected logic
        const Identifier& specifier = context->moduleRequest().m_specifier;
        auto type = context->moduleRequest().type();
        ModuleRegistryEntry* entry = globalObject->moduleLoader()->ensureRegistered(globalObject, specifier, type);
        if (scope.exception()) {
            intermediatePromise->rejectWithCaughtException(globalObject, scope);
            return;
        }
        JSValue errorValue = arguments[1];
        if (auto* error = dynamicDowncast<ErrorInstance>(errorValue)) {
            auto failure = JSModuleLoader::getErrorInfo(globalObject, error);
            if (failure.isEvaluationError(specifier, type))
                entry->setEvaluationError(globalObject, error);
            else
                entry->setFetchError(globalObject, error);
        }
        intermediatePromise->reject(vm, globalObject, errorValue);
    }
}

static void moduleLoadTopRejected(JSGlobalObject* globalObject, VM& vm, ThrowScope& scope, std::span<const JSValue, maxMicrotaskArguments> arguments, uint8_t payload)
{
    // loadModule first overload: onLoadRejected
    // arguments[0] = pre-created resultPromise
    // arguments[1] = resolution or error
    // arguments[2] = ModuleLoadingContext*
    auto* context = uncheckedDowncast<ModuleLoadingContext>(arguments[2]);
    auto* resultPromise = uncheckedDowncast<JSPromise>(arguments[0]);
    auto status = static_cast<JSPromise::Status>(payload);
    if (status == JSPromise::Status::Fulfilled)
        resultPromise->fulfill(vm, globalObject, arguments[1]);
    else {
        const Identifier& specifier = context->moduleRequest().m_specifier;
        auto type = context->moduleRequest().type();
        ModuleRegistryEntry* entry = globalObject->moduleLoader()->ensureRegistered(globalObject, specifier, type);
        if (scope.exception()) {
            resultPromise->rejectWithCaughtException(globalObject, scope);
            return;
        }
        if (JSValue fetchErrorValue = entry->fetchError()) {
            if (ErrorInstance* fetchError = dynamicDowncast<ErrorInstance>(fetchErrorValue)) {
                ErrorInstance* fetchErrorCopy = JSModuleLoader::maybeDuplicateFetchError(globalObject, fetchError);
                if (scope.exception()) {
                    resultPromise->rejectWithCaughtException(globalObject, scope);
                    return;
                }
                resultPromise->reject(vm, globalObject, fetchErrorCopy);
            } else
                resultPromise->reject(vm, globalObject, fetchErrorValue);
            return;
        }
        JSValue error = arguments[1];
        entry->setEvaluationError(globalObject, error);
        resultPromise->reject(vm, globalObject, error);
    }
}

static void moduleLoadSpecifierTransform(JSGlobalObject* globalObject, VM& vm, ThrowScope& scope, std::span<const JSValue, maxMicrotaskArguments> arguments, uint8_t payload)
{
    // Transforms resolution to specifier identifier
    // arguments[0] = pre-created transformedStatePromise
    // arguments[1] = resolution
    // arguments[2] = ModuleLoadingContext*
    auto* transformedPromise = uncheckedDowncast<JSPromise>(arguments[0]);
    auto status = static_cast<JSPromise::Status>(payload);
    if (status == JSPromise::Status::Fulfilled) {
        auto* context = uncheckedDowncast<ModuleLoadingContext>(arguments[2]);
        scope.release();
        transformedPromise->fulfill(vm, globalObject, identifierToJSValue(vm, context->moduleRequest().m_specifier));
    } else
        transformedPromise->reject(vm, globalObject, arguments[1]);
}

static void moduleLoadCombinedLoadSettled(JSGlobalObject* globalObject, VM& vm, ThrowScope& scope, std::span<const JSValue, maxMicrotaskArguments> arguments, uint8_t payload)
{
    // Combined promise: load side settled
    // arguments[0] = combinedPromise
    // arguments[1] = resolution or error
    // arguments[2] = ModuleGraphLoadingState* (graph load) or ModuleLoaderPayload* (dynamic import)
    auto* combinedPromise = uncheckedDowncast<JSPromise>(arguments[0]);
    auto* combinedCell = arguments[2].asCell();
    ASSERT(isModuleLoaderHostDefinedPayload(combinedCell));
    auto status = static_cast<JSPromise::Status>(payload);
    scope.release();
    bool fullySettled;
    if (auto* state = dynamicDowncast<ModuleGraphLoadingState>(combinedCell))
        fullySettled = state->decrementRemaining();
    else
        fullySettled = uncheckedDowncast<ModuleLoaderPayload>(combinedCell)->decrementRemaining();
    switch (combinedPromise->status()) {
    case JSPromise::Status::Pending: {
        if (status == JSPromise::Status::Fulfilled) {
            if (!fullySettled)
                return;
            JSValue fulfillmentValue;
            if (auto* state = dynamicDowncast<ModuleGraphLoadingState>(combinedCell))
                fulfillmentValue = state->fulfillment();
            else
                fulfillmentValue = uncheckedDowncast<ModuleLoaderPayload>(combinedCell)->fulfillment();
            ASSERT(fulfillmentValue);
            combinedPromise->fulfill(vm, globalObject, fulfillmentValue);
        } else
            combinedPromise->reject(vm, globalObject, arguments[1]);
        return;
    }
    default:
        return;
    }
}

static void moduleLoadCombinedStateSettled(JSGlobalObject* globalObject, VM& vm, ThrowScope& scope, std::span<const JSValue, maxMicrotaskArguments> arguments, uint8_t payload)
{
    // Combined promise: state side settled
    // arguments[0] = combinedPromise
    // arguments[1] = resolution or error
    // arguments[2] = ModuleGraphLoadingState* (graph load) or ModuleLoaderPayload* (dynamic import)
    auto* combinedPromise = uncheckedDowncast<JSPromise>(arguments[0]);
    auto* combinedCell = arguments[2].asCell();
    ASSERT(isModuleLoaderHostDefinedPayload(combinedCell));
    auto status = static_cast<JSPromise::Status>(payload);
    scope.release();
    bool fullySettled;
    if (auto* state = dynamicDowncast<ModuleGraphLoadingState>(combinedCell)) {
        fullySettled = state->decrementRemaining();
        if (status == JSPromise::Status::Fulfilled && !fullySettled)
            state->setFulfillment(vm, arguments[1]);
    } else {
        auto* p = uncheckedDowncast<ModuleLoaderPayload>(combinedCell);
        fullySettled = p->decrementRemaining();
        if (status == JSPromise::Status::Fulfilled && !fullySettled)
            p->setFulfillment(vm, arguments[1]);
    }
    switch (combinedPromise->status()) {
    case JSPromise::Status::Pending:
        if (status == JSPromise::Status::Fulfilled) {
            if (fullySettled)
                combinedPromise->fulfill(vm, globalObject, arguments[1]);
        } else
            combinedPromise->reject(vm, globalObject, arguments[1]);
        return;
    default:
        return;
    }
}

static void moduleLoadLinkEvaluateSettled(JSGlobalObject* globalObject, VM& vm, ThrowScope& scope, std::span<const JSValue, maxMicrotaskArguments> arguments, uint8_t payload)
{
    // loadModule second overload: onFulfilled
    // arguments[0] = pre-created resultPromise
    // arguments[1] = resolution (AbstractModuleRecord*) or error
    // arguments[2] = ModuleLoadingContext*
    auto* context = uncheckedDowncast<ModuleLoadingContext>(arguments[2]);
    auto* resultPromise = uncheckedDowncast<JSPromise>(arguments[0]);
    auto status = static_cast<JSPromise::Status>(payload);
    if (status == JSPromise::Status::Fulfilled) {
        auto* record = downcast<AbstractModuleRecord>(arguments[1]);
        if (context->evaluate()) {
            record->link(globalObject, context->scriptFetcher());
            JSModuleLoader::attachErrorInfo(globalObject, scope, record, record->moduleKey(), record->moduleType(), JSModuleLoader::ModuleFailure::Kind::Instantiation);
            if (scope.exception()) {
                resultPromise->rejectWithCaughtException(globalObject, scope);
                return;
            }
            JSPromise* evaluatePromise = record->evaluate(globalObject);
            JSModuleLoader::attachErrorInfo(globalObject, scope, record, record->moduleKey(), record->moduleType(), JSModuleLoader::ModuleFailure::Kind::Evaluation);
            if (scope.exception()) {
                resultPromise->rejectWithCaughtException(globalObject, scope);
                return;
            }
            // Chain: when evaluation completes, resolve resultPromise with record
            evaluatePromise->performPromiseThenWithInternalMicrotask(vm, globalObject, InternalMicrotask::ModuleLoadReturnRecord, resultPromise, record);
        } else
            resultPromise->fulfill(vm, globalObject, identifierToJSValue(vm, record->moduleKey()));
    } else
        resultPromise->reject(vm, globalObject, arguments[1]);
}

static void moduleLoadReturnRecord(JSGlobalObject* globalObject, VM& vm, ThrowScope& scope, std::span<const JSValue, maxMicrotaskArguments> arguments, uint8_t payload)
{
    // Resolves promise with the record after evaluation completes
    // arguments[0] = resultPromise
    // arguments[1] = resolution or error
    // arguments[2] = AbstractModuleRecord*
    auto* resultPromise = uncheckedDowncast<JSPromise>(arguments[0]);
    auto status = static_cast<JSPromise::Status>(payload);
    scope.release();
    if (status == JSPromise::Status::Fulfilled)
        resultPromise->fulfill(vm, globalObject, arguments[2]);
    else
        resultPromise->reject(vm, globalObject, arguments[1]);
}

static void moduleLoadStoreError(JSGlobalObject* globalObject, ThrowScope& scope, std::span<const JSValue, maxMicrotaskArguments> arguments, uint8_t payload)
{
    // loadModule second overload: fire-and-forget error categorization
    // arguments[0] = unused
    // arguments[1] = resolution or error
    // arguments[2] = ModuleLoadingContext*
    auto status = static_cast<JSPromise::Status>(payload);
    if (status == JSPromise::Status::Rejected) {
        auto* context = uncheckedDowncast<ModuleLoadingContext>(arguments[2]);
        JSValue errorValue = arguments[1];
        const Identifier& specifier = context->moduleRequest().m_specifier;
        auto type = context->moduleRequest().type();
        ModuleRegistryEntry* entry = globalObject->moduleLoader()->ensureRegistered(globalObject, specifier, type);
        RETURN_IF_EXCEPTION(scope, void());
        if (auto* error = dynamicDowncast<ErrorInstance>(errorValue)) {
            auto failure = JSModuleLoader::getErrorInfo(globalObject, error);
            if (failure.isEvaluationError(specifier, type))
                entry->setEvaluationError(globalObject, error);
            else if (JSModuleLoader::isFetchError(globalObject, error))
                entry->setFetchError(globalObject, error);
            else
                entry->setInstantiationError(globalObject, error);
        } else
            entry->setEvaluationError(globalObject, errorValue);
    }
}

static void dynamicImportLoadSettled(JSGlobalObject* globalObject, VM& vm, ThrowScope& scope, std::span<const JSValue, maxMicrotaskArguments> arguments, uint8_t payload)
{
    // https://tc39.es/ecma262/#sec-ContinueDynamicImport
    // Step-4 rejectedClosure or Step-6 linkAndEvaluateClosure
    //
    // continueDynamicImport: loadPromise settled
    // arguments[0] = capabilityPromise
    // arguments[1] = resolution or error
    // arguments[2] = AbstractModuleRecord*
    auto* capabilityPromise = uncheckedDowncast<JSPromise>(arguments[0]);
    auto* module = uncheckedDowncast<AbstractModuleRecord>(arguments[2]);
    auto status = static_cast<JSPromise::Status>(payload);
    if (status == JSPromise::Status::Fulfilled) {
        // Step-6 linkAndEvaluateClosure
        // 6.a. Let link be Completion(module.Link()).
        module->link(globalObject, nullptr);

        // 6.b. If link is an abrupt completion, then
        if (Exception* exception = scope.exception()) [[unlikely]] {
            // 6.b.i. Perform ! Call(promiseCapability.[[Reject]], undefined, « link.[[Value]] »).
            JSModuleLoader::attachErrorInfo(globalObject, exception, module, module->moduleKey(), module->moduleType(), JSModuleLoader::ModuleFailure::Kind::Instantiation);
            capabilityPromise->rejectWithCaughtException(globalObject, scope);
            return;
        }

        // 6.c. Let evaluatePromise be module.Evaluate().
        JSPromise* evaluatePromise = module->evaluate(globalObject);
        if (scope.exception()) [[unlikely]] {
            capabilityPromise->rejectWithCaughtException(globalObject, scope);
            return;
        }

        // 6.d-f. Perform PerformPromiseThen(evaluatePromise, onFulfilled, onRejected).
        evaluatePromise->performPromiseThenWithInternalMicrotask(vm, globalObject, InternalMicrotask::DynamicImportEvaluateSettled, capabilityPromise, module);
    } else {
        // Step-4 rejectedClosure
        // 4.a. Perform ! Call(promiseCapability.[[Reject]], undefined, « reason »).
        capabilityPromise->reject(vm, globalObject, arguments[1]);
    }
}

static void dynamicImportEvaluateSettled(JSGlobalObject* globalObject, VM& vm, ThrowScope& scope, std::span<const JSValue, maxMicrotaskArguments> arguments, uint8_t payload)
{
    // https://tc39.es/ecma262/#sec-ContinueDynamicImport
    // Step-4 rejectedClosure or Step-6.c fulfilledClosure
    //
    // continueDynamicImport: evaluate settled
    // arguments[0] = capabilityPromise
    // arguments[1] = resolution or error
    // arguments[2] = AbstractModuleRecord*
    auto* capabilityPromise = uncheckedDowncast<JSPromise>(arguments[0]);
    auto* module = uncheckedDowncast<AbstractModuleRecord>(arguments[2]);
    auto status = static_cast<JSPromise::Status>(payload);
    if (status == JSPromise::Status::Fulfilled) {
        // 6.d.i. Let namespace be GetModuleNamespace(module).
        JSModuleNamespaceObject* moduleNamespace = module->getModuleNamespace(globalObject);
        if (scope.exception()) [[unlikely]] {
            capabilityPromise->rejectWithCaughtException(globalObject, scope);
            return;
        }

        // 6.d.ii. Perform ! Call(promiseCapability.[[Resolve]], undefined, « namespace »).
        // This step resolves the promiseCapability with the namespace. However,
        // capabilityPromise here is the internal statePromise from moduleLoadTopSettled,
        // not the user-visible import() promise. The actual spec-required resolve()
        // happens in importModuleNamespace. Use fulfill here to avoid unnecessary
        // thenable unwrapping on internal pipeline.
        //
        // FIXME: This is different from the spec while user-observable behavior is correctly
        // aligned (as "resolve" will happen in resultPromise side from dynamic import).
        // But ideally, this carried capabilityPromise should be the last user-observable
        // promise and we should do "resolve" here. This requires some clean up.
        capabilityPromise->fulfill(vm, globalObject, moduleNamespace);
    } else
        capabilityPromise->reject(vm, globalObject, arguments[1]);
}

static void importModuleNamespace(JSGlobalObject* globalObject, VM& vm, ThrowScope&, std::span<const JSValue, maxMicrotaskArguments> arguments, uint8_t payload)
{
    // requestImportModule: namespace getter
    // arguments[0] = resultPromise
    // arguments[1] = module namespace (from dynamic import pipeline) or error
    // arguments[2] = unused
    auto* resultPromise = uncheckedDowncast<JSPromise>(arguments[0]);
    auto status = static_cast<JSPromise::Status>(payload);
    if (status == JSPromise::Status::Fulfilled) {
        // The value is a JSModuleNamespaceObject forwarded from the internal
        // pipeline (dynamicImportEvaluateSettled → combinedPromise → here).
        // resultPromise is the user-visible import() promise. Must use resolve() per spec:
        // ContinueDynamicImport https://tc39.es/ecma262/#sec-ContinueDynamicImport
        // Step 6.d.ii: Call(promiseCapability.[[Resolve]], undefined, « namespace »).
        // A module namespace that exports "then" is a thenable per spec.
        auto* moduleNamespace = downcast<JSModuleNamespaceObject>(arguments[1]);
        resultPromise->resolve(globalObject, vm, moduleNamespace);
    } else
        resultPromise->reject(vm, globalObject, arguments[1]);
    return;
}

static void promiseResolveWithoutHandlerJobSlow(JSGlobalObject* globalObject, VM& vm, JSValue capability, JSValue resolution, JSPromise::Status status)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (status == JSPromise::Status::Rejected) {
        JSValue reject = capability.get(globalObject, vm.propertyNames->reject);
        RETURN_IF_EXCEPTION(scope, void());

        MarkedArgumentBuffer arguments;
        arguments.append(resolution);
        ASSERT(!arguments.hasOverflowed());
        scope.release();
        call(globalObject, reject, jsUndefined(), arguments, "reject is not a function"_s);
        return;
    }

    JSValue resolve = capability.get(globalObject, vm.propertyNames->resolve);
    RETURN_IF_EXCEPTION(scope, void());

    MarkedArgumentBuffer arguments;
    arguments.append(resolution);
    ASSERT(!arguments.hasOverflowed());
    scope.release();
    call(globalObject, resolve, jsUndefined(), arguments, "resolve is not a function"_s);
}

static void promiseResolveWithoutHandlerJob(JSGlobalObject* globalObject, VM& vm, JSValue promiseOrCapability, JSValue resolution, JSPromise::Status status)
{
    if (auto* promise = dynamicDowncast<JSPromise>(promiseOrCapability)) [[likely]] {
        switch (status) {
        case JSPromise::Status::Pending:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        case JSPromise::Status::Fulfilled:
            promise->resolvePromise(globalObject, vm, resolution);
            break;
        case JSPromise::Status::Rejected:
            promise->rejectPromise(vm, globalObject, resolution);
            break;
        }
        return;
    }

    promiseResolveWithoutHandlerJobSlow(globalObject, vm, promiseOrCapability, resolution, status);
}

void runInternalMicrotask(JSGlobalObject* globalObject, VM& vm, InternalMicrotask task, uint8_t payload, std::span<const JSValue, maxMicrotaskArguments> arguments, MicrotaskCall* microtaskCall)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    switch (task) {
    case InternalMicrotask::None: {
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    case InternalMicrotask::PromiseResolveThenableJobFast: {
        auto* promise = uncheckedDowncast<JSPromise>(arguments[0]);
        auto* promiseToResolve = uncheckedDowncast<JSPromise>(arguments[1]);

        if (!promiseSpeciesWatchpointIsValid(vm, promise)) [[unlikely]]
            RELEASE_AND_RETURN(scope, promiseResolveThenableJobFastSlow(globalObject, promise, promiseToResolve));

        scope.release();
        promise->performPromiseThenWithInternalMicrotask(vm, globalObject, InternalMicrotask::PromiseResolveWithoutHandlerJob, promiseToResolve, jsUndefined());
        return;
    }

    case InternalMicrotask::PromiseResolveThenableJobWithInternalMicrotaskFast: {
        auto* promise = uncheckedDowncast<JSPromise>(arguments[0]);
        JSValue context = arguments[1];
        auto task = static_cast<InternalMicrotask>(payload);

        if (!promiseSpeciesWatchpointIsValid(vm, promise)) [[unlikely]]
            RELEASE_AND_RETURN(scope, promiseResolveThenableJobWithInternalMicrotaskFastSlow(globalObject, promise, task, context));

        JSValue reactionsOrResult = promise->reactionsOrResult();
        switch (promise->status()) {
        case JSPromise::Status::Pending: {
            auto* reaction = JSSlimPromiseReaction::create(vm, jsUndefined(), task, context, reactionsOrResult ? uncheckedDowncast<JSPromiseReaction>(reactionsOrResult) : nullptr);
            promise->setReactionsOrResult(vm, reaction);
            promise->markAsHandled();
            break;
        }
        case JSPromise::Status::Rejected: {
            if (!promise->isHandled())
                globalObject->globalObjectMethodTable()->promiseRejectionTracker(globalObject, promise, JSPromiseRejectionOperation::Handle);
            JSPromise::rejectWithInternalMicrotask(vm, globalObject, reactionsOrResult, task, context);
            promise->markAsHandled();
            break;
        }
        case JSPromise::Status::Fulfilled: {
            JSPromise::fulfillWithInternalMicrotask(vm, globalObject, reactionsOrResult, task, context);
            break;
        }
        }
        return;
    }

    case InternalMicrotask::PromiseResolveThenableJob: {
        JSValue promise = arguments[0];
        JSValue then = arguments[1];
        JSPromise* promiseToResolve = uncheckedDowncast<JSPromise>(arguments[2]);
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
        RELEASE_AND_RETURN(scope, promiseResolveWithoutHandlerJob(globalObject, vm, arguments[0], arguments[1], static_cast<JSPromise::Status>(payload)));
    }

    case InternalMicrotask::PromiseFulfillWithoutHandlerJob: {
        auto* promise = uncheckedDowncast<JSPromise>(arguments[0]);
        JSValue resolution = arguments[1];
        switch (static_cast<JSPromise::Status>(payload)) {
        case JSPromise::Status::Pending:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        case JSPromise::Status::Fulfilled:
            scope.release();
            promise->fulfillPromise(vm, globalObject, resolution);
            break;
        case JSPromise::Status::Rejected:
            scope.release();
            promise->rejectPromise(vm, globalObject, resolution);
            break;
        }
        return;
    }

    case InternalMicrotask::PromiseRaceResolveJob:
        RELEASE_AND_RETURN(scope, promiseRaceResolveJob(globalObject, vm, uncheckedDowncast<JSPromise>(arguments[0]), arguments[1], static_cast<JSPromise::Status>(payload)));

    case InternalMicrotask::PromiseAllResolveJob:
        RELEASE_AND_RETURN(scope, promiseAllResolveJob(globalObject, vm, uncheckedDowncast<JSPromise>(arguments[0]), arguments[1], uncheckedDowncast<JSPromiseCombinatorsContext>(arguments[2]), static_cast<JSPromise::Status>(payload)));

    case InternalMicrotask::PromiseAllSettledResolveJob:
        RELEASE_AND_RETURN(scope, promiseAllSettledResolveJob(globalObject, vm, uncheckedDowncast<JSPromise>(arguments[0]), arguments[1], uncheckedDowncast<JSPromiseCombinatorsContext>(arguments[2]), static_cast<JSPromise::Status>(payload)));

    case InternalMicrotask::PromiseAnyResolveJob:
        RELEASE_AND_RETURN(scope, promiseAnyResolveJob(globalObject, vm, uncheckedDowncast<JSPromise>(arguments[0]), arguments[1], uncheckedDowncast<JSPromiseCombinatorsContext>(arguments[2]), static_cast<JSPromise::Status>(payload)));

    case InternalMicrotask::PromiseReactionJob: {
        JSValue promiseOrCapability = arguments[0];
        JSValue handler = arguments[1];

        JSValue result;
        JSValue error;
        {
            auto catchScope = DECLARE_TOP_EXCEPTION_SCOPE(vm);
            result = callMicrotask(globalObject, handler, jsUndefined(), dynamicCastToCell(handler), "handler is not a function"_s, microtaskCall, arguments[2]);
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

            ASSERT(result || error);
        }

        if (error) {
            if (auto* promise = dynamicDowncast<JSPromise>(promiseOrCapability))
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

        if (auto* promise = dynamicDowncast<JSPromise>(promiseOrCapability))
            RELEASE_AND_RETURN(scope, promise->resolvePromise(globalObject, vm, result));

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
        callMicrotask(globalObject, handler, jsUndefined(), nullptr, "handler is not a function"_s, microtaskCall);
        return;
    }

    case InternalMicrotask::AsyncFunctionResume: {
        JSValue resolution = arguments[1];
        auto* generator = uncheckedDowncast<JSGenerator>(arguments[2]);
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
            value = callMicrotask(globalObject, next, thisValue, generator, "handler is not a function"_s, microtaskCall,
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
            auto* promise = uncheckedDowncast<JSPromise>(generator->context());
            scope.release();
            promise->reject(vm, globalObject, error);
            return;
        }

        if (generator->state() == static_cast<int32_t>(JSGenerator::State::Executing)) {
            auto* promise = uncheckedDowncast<JSPromise>(generator->context());
            scope.release();
            promise->resolve(globalObject, vm, value);
            return;
        }

        scope.release();
        JSPromise::resolveWithInternalMicrotaskForAsyncAwait(globalObject, vm, value, InternalMicrotask::AsyncFunctionResume, generator);
        return;
    }

    case InternalMicrotask::AsyncFromSyncIteratorContinue:
    case InternalMicrotask::AsyncFromSyncIteratorDone:
        RELEASE_AND_RETURN(scope, asyncFromSyncIteratorContinueOrDone(globalObject, vm, arguments[2], arguments[1], static_cast<JSPromise::Status>(payload), task == InternalMicrotask::AsyncFromSyncIteratorDone));

    case InternalMicrotask::AsyncGeneratorYieldAwaited: {
        RELEASE_AND_RETURN(scope, asyncGeneratorYieldAwaited(globalObject, uncheckedDowncast<JSAsyncGenerator>(arguments[2]), arguments[1], static_cast<JSPromise::Status>(payload)));
    }

    case InternalMicrotask::AsyncGeneratorBodyCallNormal: {
        RELEASE_AND_RETURN(scope, asyncGeneratorBodyCallNormal(globalObject, uncheckedDowncast<JSAsyncGenerator>(arguments[2]), arguments[1], static_cast<JSPromise::Status>(payload)));
    }

    case InternalMicrotask::AsyncGeneratorBodyCallReturn: {
        RELEASE_AND_RETURN(scope, asyncGeneratorBodyCallReturn(globalObject, uncheckedDowncast<JSAsyncGenerator>(arguments[2]), arguments[1], static_cast<JSPromise::Status>(payload)));
    }

    case InternalMicrotask::AsyncGeneratorResumeNext: {
        RELEASE_AND_RETURN(scope, asyncGeneratorResumeNextReturn(globalObject, uncheckedDowncast<JSAsyncGenerator>(arguments[2]), arguments[1], static_cast<JSPromise::Status>(payload)));
    }

    case InternalMicrotask::PromiseFinallyReactionJob: {
        // Phase 1: Original promise settled
        // arguments[0] = resultPromise
        // arguments[1] = value/reason from original promise
        // arguments[2] = context (JSPromiseCombinatorsGlobalContext: promise=resultPromise, values=onFinally)
        // payload = Fulfilled/Rejected status
        scope.release();
        promiseFinallyReactionJob(globalObject, vm,
            uncheckedDowncast<JSPromise>(arguments[0]),
            arguments[1],
            uncheckedDowncast<JSPromiseCombinatorsGlobalContext>(arguments[2]),
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
            uncheckedDowncast<JSPromiseCombinatorsGlobalContext>(arguments[2]),
            static_cast<JSPromise::Status>(payload));
        return;
    }

    case InternalMicrotask::AsyncModuleExecutionDone: {
        asyncModuleExecutionDone(globalObject, scope, arguments, payload);
        return;
    }

    case InternalMicrotask::AsyncModuleExecutionResume: {
        asyncModuleExecutionResume(globalObject, vm, scope, arguments, payload);
        return;
    }

    case InternalMicrotask::ModuleRegistryFetchSettled: {
        moduleRegistryFetchSettled(globalObject, vm, scope, arguments, payload);
        return;
    }

    case InternalMicrotask::ModuleRegistryModuleSettled: {
        moduleRegistryModuleSettled(globalObject, vm, arguments, payload);
        return;
    }

    case InternalMicrotask::ModuleGraphLoadingError: {
        moduleGraphLoadingError(globalObject, vm, scope, arguments, payload);
        return;
    }

    case InternalMicrotask::ModuleLoadStep: {
        moduleLoadStep(globalObject, vm, scope, arguments, payload);
        return;
    }

    case InternalMicrotask::ModuleLoadTopSettled: {
        moduleLoadTopSettled(globalObject, vm, scope, arguments, payload);
        return;
    }

    case InternalMicrotask::ModuleLoadTopRejected: {
        moduleLoadTopRejected(globalObject, vm, scope, arguments, payload);
        return;
    }

    case InternalMicrotask::ModuleLoadSpecifierTransform: {
        moduleLoadSpecifierTransform(globalObject, vm, scope, arguments, payload);
        return;
    }

    case InternalMicrotask::ModuleLoadCombinedLoadSettled: {
        moduleLoadCombinedLoadSettled(globalObject, vm, scope, arguments, payload);
        return;
    }

    case InternalMicrotask::ModuleLoadCombinedStateSettled: {
        moduleLoadCombinedStateSettled(globalObject, vm, scope, arguments, payload);
        return;
    }

    case InternalMicrotask::ModuleLoadLinkEvaluateSettled: {
        moduleLoadLinkEvaluateSettled(globalObject, vm, scope, arguments, payload);
        return;
    }

    case InternalMicrotask::ModuleLoadReturnRecord: {
        moduleLoadReturnRecord(globalObject, vm, scope, arguments, payload);
        return;
    }

    case InternalMicrotask::ModuleLoadReturnModuleKey: {
        // loadAndEvaluateModule: extract module key from AbstractModuleRecord
        // arguments[0] = resultPromise
        // arguments[1] = resolution (AbstractModuleRecord*) or error
        auto* resultPromise = uncheckedDowncast<JSPromise>(arguments[0]);
        auto status = static_cast<JSPromise::Status>(payload);
        scope.release();
        if (status == JSPromise::Status::Fulfilled) {
            auto* module = downcast<AbstractModuleRecord>(arguments[1]);
            resultPromise->fulfillPromise(vm, globalObject, identifierToJSValue(vm, module->moduleKey()));
        } else
            resultPromise->rejectPromise(vm, globalObject, arguments[1]);
        return;
    }

    case InternalMicrotask::ModuleLoadStoreError: {
        moduleLoadStoreError(globalObject, scope, arguments, payload);
        return;
    }

    case InternalMicrotask::DynamicImportLoadSettled: {
        dynamicImportLoadSettled(globalObject, vm, scope, arguments, payload);
        return;
    }

    case InternalMicrotask::DynamicImportEvaluateSettled: {
        dynamicImportEvaluateSettled(globalObject, vm, scope, arguments, payload);
        return;
    }

    case InternalMicrotask::ImportModuleNamespace: {
        importModuleNamespace(globalObject, vm, scope, arguments, payload);
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
