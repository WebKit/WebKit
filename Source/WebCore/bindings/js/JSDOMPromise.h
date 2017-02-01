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

#include "ActiveDOMCallback.h"
#include "JSDOMConvert.h"
#include <heap/StrongInlines.h>
#include <runtime/JSPromiseDeferred.h>

namespace WebCore {

class DeferredPromise : public RefCounted<DeferredPromise>, public ActiveDOMCallback {
public:
    static Ref<DeferredPromise> create(JSDOMGlobalObject& globalObject, JSC::JSPromiseDeferred& deferred)
    {
        return adoptRef(*new DeferredPromise(globalObject, deferred));
    }

    ~DeferredPromise();

    template<class IDLType> 
    void resolve(typename IDLType::ParameterType value)
    {
        if (isSuspended())
            return;
        ASSERT(m_deferred);
        ASSERT(m_globalObject);
        JSC::ExecState* exec = m_globalObject->globalExec();
        JSC::JSLockHolder locker(exec);
        resolve(*exec, toJS<IDLType>(*exec, *m_globalObject.get(), std::forward<typename IDLType::ParameterType>(value)));
    }

    void resolve()
    {
        if (isSuspended())
            return;
        ASSERT(m_deferred);
        ASSERT(m_globalObject);
        JSC::ExecState* exec = m_globalObject->globalExec();
        JSC::JSLockHolder locker(exec);
        resolve(*exec, JSC::jsUndefined());
    }

    template<class IDLType>
    void resolveWithNewlyCreated(typename IDLType::ParameterType value)
    {
        if (isSuspended())
            return;
        ASSERT(m_deferred);
        ASSERT(m_globalObject);
        JSC::ExecState* exec = m_globalObject->globalExec();
        JSC::JSLockHolder locker(exec);
        resolve(*exec, toJSNewlyCreated<IDLType>(*exec, *m_globalObject.get(), std::forward<typename IDLType::ParameterType>(value)));
    }

    template<class IDLType> 
    void reject(typename IDLType::ParameterType value)
    {
        if (isSuspended())
            return;
        ASSERT(m_deferred);
        ASSERT(m_globalObject);
        JSC::ExecState* exec = m_globalObject->globalExec();
        JSC::JSLockHolder locker(exec);
        reject(*exec, toJS<IDLType>(*exec, *m_globalObject.get(), std::forward<typename IDLType::ParameterType>(value)));
    }

    void reject();
    void reject(std::nullptr_t);
    void reject(Exception&&);
    void reject(ExceptionCode, const String& = { });
    void reject(const JSC::PrivateName&);

    template<typename Callback>
    void resolveWithCallback(Callback callback)
    {
        if (isSuspended())
            return;
        ASSERT(m_deferred);
        ASSERT(m_globalObject);
        JSC::ExecState* exec = m_globalObject->globalExec();
        JSC::JSLockHolder locker(exec);
        resolve(*exec, callback(*exec, *m_globalObject.get()));
    }

    template<typename Callback>
    void rejectWithCallback(Callback callback)
    {
        if (isSuspended())
            return;
        ASSERT(m_deferred);
        ASSERT(m_globalObject);
        JSC::ExecState* exec = m_globalObject->globalExec();
        JSC::JSLockHolder locker(exec);
        reject(*exec, callback(*exec, *m_globalObject.get()));
    }

    JSC::JSValue promise() const;

    bool isSuspended() { return !m_deferred || !canInvokeCallback(); } // The wrapper world has gone away or active DOM objects have been suspended.
    JSDOMGlobalObject* globalObject() { return m_globalObject.get(); }

    void visitAggregate(JSC::SlotVisitor& visitor) { visitor.append(m_deferred); }

private:
    DeferredPromise(JSDOMGlobalObject&, JSC::JSPromiseDeferred&);

    void clear();
    void contextDestroyed() override;

    void callFunction(JSC::ExecState&, JSC::JSValue function, JSC::JSValue resolution);
    void resolve(JSC::ExecState& state, JSC::JSValue resolution) { callFunction(state, m_deferred->resolve(), resolution); }
    void reject(JSC::ExecState& state, JSC::JSValue resolution) { callFunction(state, m_deferred->reject(), resolution); }

    JSC::Weak<JSC::JSPromiseDeferred> m_deferred;
    JSC::Weak<JSDOMGlobalObject> m_globalObject;
};

class DOMPromiseBase {
public:
    DOMPromiseBase(Ref<DeferredPromise>&& genericPromise)
        : m_promiseDeferred(WTFMove(genericPromise))
    {
    }

    DOMPromiseBase(DOMPromiseBase&& promise)
        : m_promiseDeferred(WTFMove(promise.m_promiseDeferred))
    {
    }

    DOMPromiseBase(const DOMPromiseBase& other)
        : m_promiseDeferred(other.m_promiseDeferred.copyRef())
    {
    }

    DOMPromiseBase& operator=(const DOMPromiseBase& other)
    {
        m_promiseDeferred = other.m_promiseDeferred.copyRef();
        return *this;
    }

    DOMPromiseBase& operator=(DOMPromiseBase&& other)
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
class DOMPromise : public DOMPromiseBase {
public:
    using DOMPromiseBase::DOMPromiseBase;
    using DOMPromiseBase::operator=;
    using DOMPromiseBase::promise;
    using DOMPromiseBase::reject;

    void resolve(typename IDLType::ParameterType value)
    { 
        m_promiseDeferred->resolve<IDLType>(std::forward<typename IDLType::ParameterType>(value));
    }
};

template<> class DOMPromise<void> : public DOMPromiseBase {
public:
    using DOMPromiseBase::DOMPromiseBase;
    using DOMPromiseBase::operator=;
    using DOMPromiseBase::promise;
    using DOMPromiseBase::reject;

    void resolve()
    { 
        m_promiseDeferred->resolve();
    }
};


Ref<DeferredPromise> createDeferredPromise(JSC::ExecState&, JSDOMWindow&);

void fulfillPromiseWithJSON(Ref<DeferredPromise>&&, const String&);
void fulfillPromiseWithArrayBuffer(Ref<DeferredPromise>&&, ArrayBuffer*);
void fulfillPromiseWithArrayBuffer(Ref<DeferredPromise>&&, const void*, size_t);
void rejectPromiseWithExceptionIfAny(JSC::ExecState&, JSDOMGlobalObject&, JSC::JSPromiseDeferred&);
JSC::EncodedJSValue createRejectedPromiseWithTypeError(JSC::ExecState&, const String&);

using PromiseFunction = void(JSC::ExecState&, Ref<DeferredPromise>&&);

enum class PromiseExecutionScope { WindowOnly, WindowOrWorker };

template<PromiseFunction promiseFunction, PromiseExecutionScope executionScope>
inline JSC::JSValue callPromiseFunction(JSC::ExecState& state)
{
    JSC::VM& vm = state.vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSDOMGlobalObject& globalObject = *JSC::jsCast<JSDOMGlobalObject*>(state.lexicalGlobalObject());
    JSC::JSPromiseDeferred* promiseDeferred = JSC::JSPromiseDeferred::create(&state, &globalObject);

    // promiseDeferred can be null when terminating a Worker abruptly.
    if (executionScope == PromiseExecutionScope::WindowOrWorker && !promiseDeferred)
        return JSC::jsUndefined();

    promiseFunction(state, DeferredPromise::create(globalObject, *promiseDeferred));

    rejectPromiseWithExceptionIfAny(state, globalObject, *promiseDeferred);
    ASSERT_UNUSED(scope, !scope.exception());
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
