/*
 * Copyright (C) 2013, 2016 Apple Inc. All rights reserved.
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
#include "JSDOMPromise.h"

#include "ExceptionCode.h"
#include "JSDOMError.h"
#include "JSDOMWindow.h"
#include <builtins/BuiltinNames.h>
#include <runtime/Exception.h>
#include <runtime/JSONObject.h>
#include <runtime/JSPromiseConstructor.h>

using namespace JSC;

namespace WebCore {

DeferredPromise::DeferredPromise(JSDOMGlobalObject& globalObject, JSPromiseDeferred& promiseDeferred)
    : ActiveDOMCallback(globalObject.scriptExecutionContext())
    , m_deferred(&promiseDeferred)
    , m_globalObject(&globalObject)
{
    auto locker = lockDuringMarking(globalObject.vm().heap, globalObject.gcLock());
    globalObject.vm().heap.writeBarrier(&globalObject, &promiseDeferred);
    globalObject.deferredPromises(locker).add(this);
}

DeferredPromise::~DeferredPromise()
{
    clear();
}

void DeferredPromise::clear()
{
    ASSERT(!m_deferred || m_globalObject);
    if (m_deferred && m_globalObject) {
        auto locker = lockDuringMarking(m_globalObject->vm().heap, m_globalObject->gcLock());
        m_globalObject->deferredPromises(locker).remove(this);
    }
    m_deferred.clear();
}

void DeferredPromise::contextDestroyed()
{
    ActiveDOMCallback::contextDestroyed();
    clear();
}

JSC::JSValue DeferredPromise::promise() const
{
    ASSERT(m_deferred);
    return m_deferred->promise();
}

void DeferredPromise::callFunction(ExecState& exec, JSValue function, JSValue resolution)
{
    if (!canInvokeCallback())
        return;

    CallData callData;
    CallType callType = getCallData(function, callData);
    ASSERT(callType != CallType::None);

    MarkedArgumentBuffer arguments;
    arguments.append(resolution);

    call(&exec, function, callType, callData, jsUndefined(), arguments);

    clear();
}

void DeferredPromise::reject()
{
    if (isSuspended())
        return;

    ASSERT(m_deferred);
    ASSERT(m_globalObject);
    auto& state = *m_globalObject->globalExec();
    JSC::JSLockHolder locker(&state);
    reject(state, JSC::jsUndefined());
}

void DeferredPromise::reject(std::nullptr_t)
{
    if (isSuspended())
        return;

    ASSERT(m_deferred);
    ASSERT(m_globalObject);
    auto& state = *m_globalObject->globalExec();
    JSC::JSLockHolder locker(&state);
    reject(state, JSC::jsNull());
}

void DeferredPromise::reject(Exception&& exception)
{
    if (isSuspended())
        return;

    ASSERT(m_deferred);
    ASSERT(m_globalObject);
    auto& state = *m_globalObject->globalExec();
    JSC::JSLockHolder locker(&state);
    reject(state, createDOMException(state, WTFMove(exception)));
}

void DeferredPromise::reject(ExceptionCode ec, const String& message)
{
    if (isSuspended())
        return;

    ASSERT(m_deferred);
    ASSERT(m_globalObject);
    JSC::ExecState* state = m_globalObject->globalExec();
    JSC::JSLockHolder locker(state);
    reject(*state, createDOMException(state, ec, message));
}

void DeferredPromise::reject(const JSC::PrivateName& privateName)
{
    if (isSuspended())
        return;

    ASSERT(m_deferred);
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
    JSC::JSPromiseDeferred* deferred = JSC::JSPromiseDeferred::create(&state, &domWindow);
    // deferred can only be null in workers.
    ASSERT(deferred);
    return DeferredPromise::create(domWindow, *deferred);
}

JSC::EncodedJSValue createRejectedPromiseWithTypeError(JSC::ExecState& state, const String& errorMessage)
{
    ASSERT(state.lexicalGlobalObject());
    auto& globalObject = *state.lexicalGlobalObject();

    auto promiseConstructor = globalObject.promiseConstructor();
    auto rejectFunction = promiseConstructor->get(&state, state.vm().propertyNames->builtinNames().rejectPrivateName());
    auto rejectionValue = createTypeError(&state, errorMessage);

    CallData callData;
    auto callType = getCallData(rejectFunction, callData);
    ASSERT(callType != CallType::None);

    MarkedArgumentBuffer arguments;
    arguments.append(rejectionValue);

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
        promise->reject(SYNTAX_ERR);
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
