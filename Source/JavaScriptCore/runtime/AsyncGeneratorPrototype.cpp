/*
 * Copyright (C) 2017 Oleksandr Skachkov <gskachkov@gmail.com>.
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "AsyncGeneratorPrototype.h"

#include "JSCInlines.h"
#include "IteratorOperations.h"
#include "JSAsyncGenerator.h"
#include "JSGenerator.h"
#include "JSMicrotask.h"
#include "JSPromise.h"
#include "MicrotaskCall.h"

namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(asyncGeneratorPrototypeReturn);
static JSC_DECLARE_HOST_FUNCTION(asyncGeneratorPrototypeThrow);

} // namespace JSC

#include "AsyncGeneratorPrototype.lut.h"

namespace JSC {

const ClassInfo AsyncGeneratorPrototype::s_info = { "AsyncGenerator"_s, &Base::s_info, &asyncGeneratorPrototypeTable, nullptr, CREATE_METHOD_TABLE(AsyncGeneratorPrototype) };

/* Source for AsyncGeneratorPrototype.lut.h
@begin asyncGeneratorPrototypeTable
  return    asyncGeneratorPrototypeReturn   DontEnum|Function 1
  throw     asyncGeneratorPrototypeThrow    DontEnum|Function 1
@end
*/

// https://tc39.es/ecma262/#sec-asyncgenerator-prototype-next
JSValue asyncGeneratorNext(JSGlobalObject* globalObject, JSAsyncGenerator* generator, JSValue argument, MicrotaskCallCache* microtaskCallCache)
{
    VM& vm = globalObject->vm();
    auto* promise = JSPromise::create(vm, globalObject->promiseStructure());

    // 5. Let state be gen.[[AsyncGeneratorState]].
    int32_t state = generator->state();
    // 6. If state is completed, then
    if (state == static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::Completed)) {
        // 6.a. Let iteratorResult be CreateIteratorResultObject(undefined, true).
        // 6.b. Perform ! Call(promiseCapability.[[Resolve]], undefined, « iteratorResult »).
        auto* iteratorResult = createIteratorResultObject(globalObject, jsUndefined(), /* done */ true);
        promise->resolve(globalObject, vm, iteratorResult);
        return promise;
    }

    // 7. Let completion be NormalCompletion(value).
    // 8. Perform AsyncGeneratorEnqueue(gen, completion, promiseCapability).
    generator->enqueue(vm, argument, static_cast<int32_t>(JSGenerator::ResumeMode::NormalMode), promise);

    // 9. If state is either suspended-start or suspended-yield, then
    if (state == static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::Init) || JSAsyncGenerator::isSuspendedYieldState(state)) {
        // 9.a. Perform AsyncGeneratorResume(gen, completion).
        asyncGeneratorResume(globalObject, generator, microtaskCallCache);
    } else {
        // 10. Else,
        // 10.a. Assert: state is either executing or draining-queue.
        ASSERT(JSAsyncGenerator::isExecutingState(state) || state == static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::DrainingQueue));
    }

    // 11. Return promiseCapability.[[Promise]].
    return promise;
}

JSC_DEFINE_HOST_FUNCTION(asyncGeneratorPrototypeNext, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();

    // 3. Let result be Completion(AsyncGeneratorValidate(gen, empty)).
    // 4. IfAbruptRejectPromise(result, promiseCapability).
    auto* generator = dynamicDowncast<JSAsyncGenerator>(callFrame->thisValue());
    if (!generator) [[unlikely]] {
        auto* promise = JSPromise::create(vm, globalObject->promiseStructure());
        promise->reject(vm, createTypeError(globalObject, "|this| should be an async generator"_s));
        return JSValue::encode(promise);
    }

    return JSValue::encode(asyncGeneratorNext(globalObject, generator, callFrame->argument(0), &vm.syncResumeCallCache()));
}

// https://tc39.es/ecma262/#sec-asyncgenerator-prototype-return
JSC_DEFINE_HOST_FUNCTION(asyncGeneratorPrototypeReturn, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto* promise = JSPromise::create(vm, globalObject->promiseStructure());

    // 3. Let result be Completion(AsyncGeneratorValidate(gen, empty)).
    // 4. IfAbruptRejectPromise(result, promiseCapability).
    auto* generator = dynamicDowncast<JSAsyncGenerator>(callFrame->thisValue());
    if (!generator) [[unlikely]] {
        promise->reject(vm, createTypeError(globalObject, "|this| should be an async generator"_s));
        return JSValue::encode(promise);
    }

    // 5. Let completion be ReturnCompletion(value).
    // 6. Perform AsyncGeneratorEnqueue(gen, completion, promiseCapability).
    generator->enqueue(vm, callFrame->argument(0), static_cast<int32_t>(JSGenerator::ResumeMode::ReturnMode), promise);

    // 7. Let state be gen.[[AsyncGeneratorState]].
    int32_t state = generator->state();
    // 8. If state is either suspended-start or completed, then
    if (state == static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::Init) || state == static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::Completed)) {
        // 8.a. Set gen.[[AsyncGeneratorState]] to draining-queue.
        // 8.b. Perform AsyncGeneratorAwaitReturn(gen).
        generator->setState(static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::DrainingQueue));
        asyncGeneratorAwaitReturn(globalObject, generator);
    } else if (JSAsyncGenerator::isSuspendedYieldState(state)) {
        // 9. Else if state is suspended-yield, then
        // 9.a. Perform AsyncGeneratorResume(gen, completion).
        asyncGeneratorResume(globalObject, generator, &vm.syncResumeCallCache());
    } else {
        // 10. Else,
        // 10.a. Assert: state is either executing or draining-queue.
        ASSERT(JSAsyncGenerator::isExecutingState(state) || state == static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::DrainingQueue));
    }

    // 11. Return promiseCapability.[[Promise]].
    return JSValue::encode(promise);
}

// https://tc39.es/ecma262/#sec-asyncgenerator-prototype-throw
JSC_DEFINE_HOST_FUNCTION(asyncGeneratorPrototypeThrow, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto* promise = JSPromise::create(vm, globalObject->promiseStructure());

    // 3. Let result be Completion(AsyncGeneratorValidate(gen, empty)).
    // 4. IfAbruptRejectPromise(result, promiseCapability).
    auto* generator = dynamicDowncast<JSAsyncGenerator>(callFrame->thisValue());
    if (!generator) [[unlikely]] {
        promise->reject(vm, createTypeError(globalObject, "|this| should be an async generator"_s));
        return JSValue::encode(promise);
    }

    JSValue exception = callFrame->argument(0);
    int32_t state = generator->state();
    // 5. Let state be gen.[[AsyncGeneratorState]].
    // 6. If state is suspended-start, then
    if (state == static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::Init)) {
        // 6.a. Set gen.[[AsyncGeneratorState]] to completed.
        // 6.b. Set state to completed.
        generator->setState(static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::Completed));
        state = static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::Completed);
    }

    // 7. If state is completed, then
    if (state == static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::Completed)) {
        // 7.a. Perform ! Call(promiseCapability.[[Reject]], undefined, « exception »).
        // 7.b. Return promiseCapability.[[Promise]].
        promise->reject(vm, exception);
        return JSValue::encode(promise);
    }

    // 8. Let completion be ThrowCompletion(exception).
    // 9. Perform AsyncGeneratorEnqueue(gen, completion, promiseCapability).
    generator->enqueue(vm, exception, static_cast<int32_t>(JSGenerator::ResumeMode::ThrowMode), promise);

    // 10. If state is suspended-yield, then
    // 10.a. Perform AsyncGeneratorResume(gen, completion).
    if (JSAsyncGenerator::isSuspendedYieldState(state))
        asyncGeneratorResume(globalObject, generator, &vm.syncResumeCallCache());
    else {
        // 11. Else,
        // 11.a. Assert: state is either executing or draining-queue.
        ASSERT(JSAsyncGenerator::isExecutingState(state) || state == static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::DrainingQueue));
    }

    // 12. Return promiseCapability.[[Promise]].
    return JSValue::encode(promise);
}

void AsyncGeneratorPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    putDirectWithoutTransition(vm, vm.propertyNames->next, globalObject->asyncGeneratorPrototypeNextFunction(), static_cast<unsigned>(PropertyAttribute::DontEnum));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

} // namespace JSC
