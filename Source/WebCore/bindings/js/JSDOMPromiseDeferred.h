/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#pragma once

#include "ExceptionOr.h"
#include "JSDOMConvert.h"
#include "JSDOMGuardedObject.h"
#include <JavaScriptCore/CatchScope.h>
#include <JavaScriptCore/JSPromiseDeferred.h>

namespace WebCore {

class JSDOMWindow;

class DeferredPromise : public DOMGuarded<JSC::JSPromiseDeferred> {
public:
    enum class Mode {
        ClearPromiseOnResolve,
        RetainPromiseOnResolve
    };

    static RefPtr<DeferredPromise> create(JSC::ExecState& state, JSDOMGlobalObject& globalObject, Mode mode = Mode::ClearPromiseOnResolve)
    {
        auto* promiseDeferred = JSC::JSPromiseDeferred::tryCreate(&state, &globalObject);
        if (!promiseDeferred)
            return nullptr;
        return adoptRef(new DeferredPromise(globalObject, *promiseDeferred, mode));
    }

    static Ref<DeferredPromise> create(JSDOMGlobalObject& globalObject, JSC::JSPromiseDeferred& deferred, Mode mode = Mode::ClearPromiseOnResolve)
    {
        return adoptRef(*new DeferredPromise(globalObject, deferred, mode));
    }

    template<class IDLType>
    void resolve(typename IDLType::ParameterType value)
    {
        if (isSuspended())
            return;
        ASSERT(deferred());
        ASSERT(globalObject());
        JSC::ExecState* exec = globalObject()->globalExec();
        JSC::JSLockHolder locker(exec);
        resolve(*exec, toJS<IDLType>(*exec, *globalObject(), std::forward<typename IDLType::ParameterType>(value)));
    }

    void resolve()
    {
        if (isSuspended())
            return;
        ASSERT(deferred());
        ASSERT(globalObject());
        JSC::ExecState* exec = globalObject()->globalExec();
        JSC::JSLockHolder locker(exec);
        resolve(*exec, JSC::jsUndefined());
    }

    template<class IDLType>
    void resolveWithNewlyCreated(typename IDLType::ParameterType value)
    {
        if (isSuspended())
            return;
        ASSERT(deferred());
        ASSERT(globalObject());
        JSC::ExecState* exec = globalObject()->globalExec();
        JSC::JSLockHolder locker(exec);
        resolve(*exec, toJSNewlyCreated<IDLType>(*exec, *globalObject(), std::forward<typename IDLType::ParameterType>(value)));
    }

    template<class IDLType>
    void reject(typename IDLType::ParameterType value)
    {
        if (isSuspended())
            return;
        ASSERT(deferred());
        ASSERT(globalObject());
        JSC::ExecState* exec = globalObject()->globalExec();
        JSC::JSLockHolder locker(exec);
        reject(*exec, toJS<IDLType>(*exec, *globalObject(), std::forward<typename IDLType::ParameterType>(value)));
    }

    void reject();
    void reject(std::nullptr_t);
    void reject(Exception);
    WEBCORE_EXPORT void reject(ExceptionCode, const String& = { });
    void reject(const JSC::PrivateName&);

    template<typename Callback>
    void resolveWithCallback(Callback callback)
    {
        if (isSuspended())
            return;
        ASSERT(deferred());
        ASSERT(globalObject());
        JSC::ExecState* exec = globalObject()->globalExec();
        JSC::JSLockHolder locker(exec);
        resolve(*exec, callback(*exec, *globalObject()));
    }

    template<typename Callback>
    void rejectWithCallback(Callback callback)
    {
        if (isSuspended())
            return;
        ASSERT(deferred());
        ASSERT(globalObject());
        JSC::ExecState* exec = globalObject()->globalExec();
        JSC::JSLockHolder locker(exec);
        reject(*exec, callback(*exec, *globalObject()));
    }

    JSC::JSValue promise() const;

    void whenSettled(std::function<void()>&&);

private:
    DeferredPromise(JSDOMGlobalObject& globalObject, JSC::JSPromiseDeferred& deferred, Mode mode)
        : DOMGuarded<JSC::JSPromiseDeferred>(globalObject, deferred)
        , m_mode(mode)
    {
    }

    JSC::JSPromiseDeferred* deferred() const { return guarded(); }

    WEBCORE_EXPORT void callFunction(JSC::ExecState&, JSC::JSValue function, JSC::JSValue resolution);

    void resolve(JSC::ExecState& state, JSC::JSValue resolution) { callFunction(state, deferred()->resolve(), resolution); }
    void reject(JSC::ExecState& state, JSC::JSValue resolution) { callFunction(state, deferred()->reject(), resolution); }

    Mode m_mode;
};

class DOMPromiseDeferredBase {
public:
    DOMPromiseDeferredBase(Ref<DeferredPromise>&& genericPromise)
        : m_promiseDeferred(WTFMove(genericPromise))
    {
    }

    DOMPromiseDeferredBase(DOMPromiseDeferredBase&& promise)
        : m_promiseDeferred(WTFMove(promise.m_promiseDeferred))
    {
    }

    DOMPromiseDeferredBase(const DOMPromiseDeferredBase& other)
        : m_promiseDeferred(other.m_promiseDeferred.copyRef())
    {
    }

    DOMPromiseDeferredBase& operator=(const DOMPromiseDeferredBase& other)
    {
        m_promiseDeferred = other.m_promiseDeferred.copyRef();
        return *this;
    }

    DOMPromiseDeferredBase& operator=(DOMPromiseDeferredBase&& other)
    {
        m_promiseDeferred = WTFMove(other.m_promiseDeferred);
        return *this;
    }

    void reject()
    {
        m_promiseDeferred->reject();
    }

    template<typename... ErrorType> 
    void reject(ErrorType&&... error)
    {
        m_promiseDeferred->reject(std::forward<ErrorType>(error)...);
    }

    template<typename IDLType>
    void rejectType(typename IDLType::ParameterType value)
    {
        m_promiseDeferred->reject<IDLType>(std::forward<typename IDLType::ParameterType>(value));
    }

    JSC::JSValue promise() const { return m_promiseDeferred->promise(); };

protected:
    Ref<DeferredPromise> m_promiseDeferred;
};

template<typename IDLType> 
class DOMPromiseDeferred : public DOMPromiseDeferredBase {
public:
    using DOMPromiseDeferredBase::DOMPromiseDeferredBase;
    using DOMPromiseDeferredBase::operator=;
    using DOMPromiseDeferredBase::promise;
    using DOMPromiseDeferredBase::reject;

    void resolve(typename IDLType::ParameterType value)
    { 
        m_promiseDeferred->resolve<IDLType>(std::forward<typename IDLType::ParameterType>(value));
    }

    void settle(ExceptionOr<typename IDLType::ParameterType>&& result)
    {
        if (result.hasException()) {
            reject(result.releaseException());
            return;
        }
        resolve(result.releaseReturnValue());
    }
};

template<> class DOMPromiseDeferred<void> : public DOMPromiseDeferredBase {
public:
    using DOMPromiseDeferredBase::DOMPromiseDeferredBase;
    using DOMPromiseDeferredBase::operator=;
    using DOMPromiseDeferredBase::promise;
    using DOMPromiseDeferredBase::reject;

    void resolve()
    { 
        m_promiseDeferred->resolve();
    }

    void settle(ExceptionOr<void>&& result)
    {
        if (result.hasException()) {
            reject(result.releaseException());
            return;
        }
        resolve();
    }
};


Ref<DeferredPromise> createDeferredPromise(JSC::ExecState&, JSDOMWindow&);

void fulfillPromiseWithJSON(Ref<DeferredPromise>&&, const String&);
void fulfillPromiseWithArrayBuffer(Ref<DeferredPromise>&&, ArrayBuffer*);
void fulfillPromiseWithArrayBuffer(Ref<DeferredPromise>&&, const void*, size_t);
WEBCORE_EXPORT void rejectPromiseWithExceptionIfAny(JSC::ExecState&, JSDOMGlobalObject&, JSC::JSPromiseDeferred&);
JSC::EncodedJSValue createRejectedPromiseWithTypeError(JSC::ExecState&, const String&);

using PromiseFunction = void(JSC::ExecState&, Ref<DeferredPromise>&&);

enum class PromiseExecutionScope { WindowOnly, WindowOrWorker };

template<PromiseFunction promiseFunction, PromiseExecutionScope executionScope>
inline JSC::JSValue callPromiseFunction(JSC::ExecState& state)
{
    JSC::VM& vm = state.vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);

    auto& globalObject = callerGlobalObject(state);
    JSC::JSPromiseDeferred* promiseDeferred = JSC::JSPromiseDeferred::tryCreate(&state, &globalObject);

    // promiseDeferred can be null when terminating a Worker abruptly.
    if (executionScope == PromiseExecutionScope::WindowOrWorker && !promiseDeferred)
        return JSC::jsUndefined();

    promiseFunction(state, DeferredPromise::create(globalObject, *promiseDeferred));

    rejectPromiseWithExceptionIfAny(state, globalObject, *promiseDeferred);
    EXCEPTION_ASSERT_UNUSED(scope, !scope.exception());
    return promiseDeferred->promise();
}

template<PromiseExecutionScope executionScope, typename PromiseFunctor>
inline JSC::JSValue callPromiseFunction(JSC::ExecState& state, PromiseFunctor functor)
{
    JSC::VM& vm = state.vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);

    auto& globalObject = callerGlobalObject(state);
    JSC::JSPromiseDeferred* promiseDeferred = JSC::JSPromiseDeferred::tryCreate(&state, &globalObject);

    // promiseDeferred can be null when terminating a Worker abruptly.
    if (executionScope == PromiseExecutionScope::WindowOrWorker && !promiseDeferred)
        return JSC::jsUndefined();

    functor(state, DeferredPromise::create(globalObject, *promiseDeferred));

    rejectPromiseWithExceptionIfAny(state, globalObject, *promiseDeferred);
    EXCEPTION_ASSERT_UNUSED(scope, !scope.exception());
    return promiseDeferred->promise();
}

using BindingPromiseFunction = JSC::EncodedJSValue(JSC::ExecState*, Ref<DeferredPromise>&&);
template<BindingPromiseFunction bindingFunction>
inline void bindingPromiseFunctionAdapter(JSC::ExecState& state, Ref<DeferredPromise>&& promise)
{
    bindingFunction(&state, WTFMove(promise));
}

template<BindingPromiseFunction bindingPromiseFunction, PromiseExecutionScope executionScope>
inline JSC::JSValue callPromiseFunction(JSC::ExecState& state)
{
    return callPromiseFunction<bindingPromiseFunctionAdapter<bindingPromiseFunction>, executionScope>(state);
}

} // namespace WebCore
