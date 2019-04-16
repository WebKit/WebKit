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
#include "JSDOMPromiseDeferred.h"

#include "DOMWindow.h"
#include "JSDOMPromise.h"
#include "JSDOMWindow.h"
#include <JavaScriptCore/BuiltinNames.h>
#include <JavaScriptCore/Exception.h>
#include <JavaScriptCore/JSONObject.h>
#include <JavaScriptCore/JSPromiseConstructor.h>

namespace WebCore {
using namespace JSC;

JSC::JSValue DeferredPromise::promise() const
{
    ASSERT(deferred());
    return deferred()->promise();
}

void DeferredPromise::callFunction(ExecState& exec, JSValue function, JSValue resolution)
{
    if (!canInvokeCallback())
        return;

    VM& vm = exec.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    CallData callData;
    CallType callType = getCallData(vm, function, callData);
    ASSERT(callType != CallType::None);

    MarkedArgumentBuffer arguments;
    arguments.append(resolution);
    ASSERT(!arguments.hasOverflowed());

    call(&exec, function, callType, callData, jsUndefined(), arguments);

    // DeferredPromise should only be used by internal implementations that are well behaved.
    // In practice, the only exception we should ever see here is the TerminatedExecutionException.
    EXCEPTION_ASSERT_UNUSED(scope, !scope.exception() || isTerminatedExecutionException(vm, scope.exception()));

    if (m_mode == Mode::ClearPromiseOnResolve)
        clear();
}

void DeferredPromise::whenSettled(std::function<void()>&& callback)
{
    DOMPromise::whenPromiseIsSettled(globalObject(), deferred()->promise(), WTFMove(callback));
}

void DeferredPromise::reject()
{
    if (isSuspended())
        return;

    ASSERT(deferred());
    ASSERT(m_globalObject);
    auto& state = *m_globalObject->globalExec();
    JSC::JSLockHolder locker(&state);
    reject(state, JSC::jsUndefined());
}

void DeferredPromise::reject(std::nullptr_t)
{
    if (isSuspended())
        return;

    ASSERT(deferred());
    ASSERT(m_globalObject);
    auto& state = *m_globalObject->globalExec();
    JSC::JSLockHolder locker(&state);
    reject(state, JSC::jsNull());
}

void DeferredPromise::reject(Exception exception)
{
    if (isSuspended())
        return;

    ASSERT(deferred());
    ASSERT(m_globalObject);
    auto& state = *m_globalObject->globalExec();
    JSC::JSLockHolder locker(&state);

    if (exception.code() == ExistingExceptionError) {
        auto scope = DECLARE_CATCH_SCOPE(state.vm());

        EXCEPTION_ASSERT(scope.exception());

        auto error = scope.exception()->value();
        scope.clearException();

        reject<IDLAny>(error);
        return;
    }

    auto scope = DECLARE_THROW_SCOPE(state.vm());
    auto error = createDOMException(state, WTFMove(exception));
    if (UNLIKELY(scope.exception())) {
        ASSERT(isTerminatedExecutionException(state.vm(), scope.exception()));
        return;
    }

    reject(state, error);
}

void DeferredPromise::reject(ExceptionCode ec, const String& message)
{
    if (isSuspended())
        return;

    ASSERT(deferred());
    ASSERT(m_globalObject);
    auto& state = *m_globalObject->globalExec();
    JSC::JSLockHolder locker(&state);

    if (ec == ExistingExceptionError) {
        auto scope = DECLARE_CATCH_SCOPE(state.vm());

        EXCEPTION_ASSERT(scope.exception());

        auto error = scope.exception()->value();
        scope.clearException();

        reject<IDLAny>(error);
        return;
    }

    auto scope = DECLARE_THROW_SCOPE(state.vm());
    auto error = createDOMException(&state, ec, message);
    if (UNLIKELY(scope.exception())) {
        ASSERT(isTerminatedExecutionException(state.vm(), scope.exception()));
        return;
    }


    reject(state, error);
}

void DeferredPromise::reject(const JSC::PrivateName& privateName)
{
    if (isSuspended())
        return;

    ASSERT(deferred());
    ASSERT(m_globalObject);
    JSC::ExecState* state = m_globalObject->globalExec();
    JSC::JSLockHolder locker(state);
    reject(*state, JSC::Symbol::create(state->vm(), privateName.uid()));
}

void rejectPromiseWithExceptionIfAny(JSC::ExecState& state, JSDOMGlobalObject& globalObject, JSPromiseDeferred& promiseDeferred)
{
    VM& vm = state.vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);

    if (LIKELY(!scope.exception()))
        return;

    JSValue error = scope.exception()->value();
    scope.clearException();

    DeferredPromise::create(globalObject, promiseDeferred)->reject<IDLAny>(error);
}

Ref<DeferredPromise> createDeferredPromise(JSC::ExecState& state, JSDOMWindow& domWindow)
{
    JSC::JSPromiseDeferred* deferred = JSC::JSPromiseDeferred::tryCreate(&state, &domWindow);
    // deferred can only be null in workers.
    RELEASE_ASSERT(deferred);
    return DeferredPromise::create(domWindow, *deferred);
}

JSC::EncodedJSValue createRejectedPromiseWithTypeError(JSC::ExecState& state, const String& errorMessage, RejectedPromiseWithTypeErrorCause cause)
{
    ASSERT(state.lexicalGlobalObject());
    auto& globalObject = *state.lexicalGlobalObject();

    auto promiseConstructor = globalObject.promiseConstructor();
    auto rejectFunction = promiseConstructor->get(&state, state.vm().propertyNames->builtinNames().rejectPrivateName());
    auto* rejectionValue = static_cast<ErrorInstance*>(createTypeError(&state, errorMessage));
    if (cause == RejectedPromiseWithTypeErrorCause::NativeGetter)
        rejectionValue->setNativeGetterTypeError();

    CallData callData;
    auto callType = getCallData(state.vm(), rejectFunction, callData);
    ASSERT(callType != CallType::None);

    MarkedArgumentBuffer arguments;
    arguments.append(rejectionValue);
    ASSERT(!arguments.hasOverflowed());

    return JSValue::encode(call(&state, rejectFunction, callType, callData, promiseConstructor, arguments));
}

static inline JSC::JSValue parseAsJSON(JSC::ExecState* state, const String& data)
{
    JSC::JSLockHolder lock(state);
    return JSC::JSONParse(state, data);
}

void fulfillPromiseWithJSON(Ref<DeferredPromise>&& promise, const String& data)
{
    JSC::JSValue value = parseAsJSON(promise->globalObject()->globalExec(), data);
    if (!value)
        promise->reject(SyntaxError);
    else
        promise->resolve<IDLAny>(value);
}

void fulfillPromiseWithArrayBuffer(Ref<DeferredPromise>&& promise, ArrayBuffer* arrayBuffer)
{
    if (!arrayBuffer) {
        promise->reject<IDLAny>(createOutOfMemoryError(promise->globalObject()->globalExec()));
        return;
    }
    promise->resolve<IDLInterface<ArrayBuffer>>(*arrayBuffer);
}

void fulfillPromiseWithArrayBuffer(Ref<DeferredPromise>&& promise, const void* data, size_t length)
{
    fulfillPromiseWithArrayBuffer(WTFMove(promise), ArrayBuffer::tryCreate(data, length).get());
}

}
