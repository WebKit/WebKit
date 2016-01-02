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

#ifndef JSDOMPromise_h
#define JSDOMPromise_h

#include "JSDOMBinding.h"
#include <heap/StrongInlines.h>
#include <runtime/JSPromiseDeferred.h>
#include <wtf/Optional.h>

namespace WebCore {

class DeferredWrapper {
public:
    DeferredWrapper(JSC::ExecState*, JSDOMGlobalObject*, JSC::JSPromiseDeferred*);

    template<class ResolveResultType>
    void resolve(const ResolveResultType&);

    template<class RejectResultType>
    void reject(const RejectResultType&);

    JSDOMGlobalObject& globalObject() const;

private:
    void callFunction(JSC::ExecState&, JSC::JSValue function, JSC::JSValue resolution);
    void resolve(JSC::ExecState& state, JSC::JSValue resolution) { callFunction(state, m_deferred->resolve(), resolution); }
    void reject(JSC::ExecState& state, JSC::JSValue resolution) { callFunction(state, m_deferred->reject(), resolution); }

    JSC::Strong<JSDOMGlobalObject> m_globalObject;
    JSC::Strong<JSC::JSPromiseDeferred> m_deferred;
};

void rejectPromiseWithExceptionIfAny(JSC::ExecState&, JSDOMGlobalObject&, JSC::JSPromiseDeferred&);

inline JSC::JSValue callPromiseFunction(JSC::ExecState& state, JSC::EncodedJSValue promiseFunction(JSC::ExecState*, JSC::JSPromiseDeferred*))
{
    JSDOMGlobalObject& globalObject = *JSC::jsCast<JSDOMGlobalObject*>(state.lexicalGlobalObject());
    JSC::JSPromiseDeferred& promiseDeferred = *JSC::JSPromiseDeferred::create(&state, &globalObject);
    promiseFunction(&state, &promiseDeferred);

    rejectPromiseWithExceptionIfAny(state, globalObject, promiseDeferred);
    ASSERT(!state.hadException());
    return promiseDeferred.promise();
}

template <typename Value, typename Error>
class DOMPromise {
public:
    DOMPromise(DeferredWrapper&& wrapper) : m_wrapper(WTFMove(wrapper)) { }
    DOMPromise(DOMPromise&& promise) : m_wrapper(WTFMove(promise.m_wrapper)) { }

    DOMPromise(const DOMPromise&)= delete;
    DOMPromise& operator=(DOMPromise const&) = delete;

    void resolve(const Value& value) { m_wrapper.resolve<Value>(value); }
    void reject(const Error& error) { m_wrapper.reject<Error>(error); }

private:
    DeferredWrapper m_wrapper;
};

template<class ResolveResultType>
inline void DeferredWrapper::resolve(const ResolveResultType& result)
{
    ASSERT(m_deferred);
    ASSERT(m_globalObject);
    JSC::ExecState* exec = m_globalObject->globalExec();
    JSC::JSLockHolder locker(exec);
    resolve(*exec, toJS(exec, m_globalObject.get(), result));
}

template<class RejectResultType>
inline void DeferredWrapper::reject(const RejectResultType& result)
{
    ASSERT(m_deferred);
    ASSERT(m_globalObject);
    JSC::ExecState* exec = m_globalObject->globalExec();
    JSC::JSLockHolder locker(exec);
    reject(*exec, toJS(exec, m_globalObject.get(), result));
}

template<>
inline void DeferredWrapper::reject(const std::nullptr_t&)
{
    ASSERT(m_deferred);
    ASSERT(m_globalObject);
    JSC::ExecState* exec = m_globalObject->globalExec();
    JSC::JSLockHolder locker(exec);
    reject(*exec, JSC::jsNull());
}

template<>
inline void DeferredWrapper::reject(const JSC::JSValue& value)
{
    ASSERT(m_deferred);
    ASSERT(m_globalObject);
    JSC::ExecState* exec = m_globalObject->globalExec();
    JSC::JSLockHolder locker(exec);
    reject(*exec, value);
}

template<>
inline void DeferredWrapper::reject<ExceptionCode>(const ExceptionCode& ec)
{
    ASSERT(m_deferred);
    ASSERT(m_globalObject);
    JSC::ExecState* exec = m_globalObject->globalExec();
    JSC::JSLockHolder locker(exec);
    reject(*exec, createDOMException(exec, ec));
}

template<>
inline void DeferredWrapper::resolve<String>(const String& result)
{
    ASSERT(m_deferred);
    ASSERT(m_globalObject);
    JSC::ExecState* exec = m_globalObject->globalExec();
    JSC::JSLockHolder locker(exec);
    resolve(*exec, jsString(exec, result));
}

template<>
inline void DeferredWrapper::resolve<bool>(const bool& result)
{
    ASSERT(m_deferred);
    ASSERT(m_globalObject);
    JSC::ExecState* exec = m_globalObject->globalExec();
    JSC::JSLockHolder locker(exec);
    resolve(*exec, JSC::jsBoolean(result));
}

template<>
inline void DeferredWrapper::resolve<JSC::JSValue>(const JSC::JSValue& value)
{
    ASSERT(m_deferred);
    ASSERT(m_globalObject);
    JSC::ExecState* exec = m_globalObject->globalExec();
    JSC::JSLockHolder locker(exec);
    resolve(*exec, value);
}
template<>
inline void DeferredWrapper::resolve<Vector<unsigned char>>(const Vector<unsigned char>& result)
{
    ASSERT(m_deferred);
    ASSERT(m_globalObject);
    JSC::ExecState* exec = m_globalObject->globalExec();
    JSC::JSLockHolder locker(exec);
    RefPtr<ArrayBuffer> buffer = ArrayBuffer::create(result.data(), result.size());
    resolve(*exec, toJS(exec, m_globalObject.get(), buffer.get()));
}

template<>
inline void DeferredWrapper::resolve(const std::nullptr_t&)
{
    ASSERT(m_deferred);
    ASSERT(m_globalObject);
    JSC::ExecState* exec = m_globalObject->globalExec();
    JSC::JSLockHolder locker(exec);
    resolve(*exec, JSC::jsUndefined());
}

template<>
inline void DeferredWrapper::reject<String>(const String& result)
{
    ASSERT(m_deferred);
    ASSERT(m_globalObject);
    JSC::ExecState* exec = m_globalObject->globalExec();
    JSC::JSLockHolder locker(exec);
    reject(*exec, jsString(exec, result));
}

}

#endif // JSDOMPromise_h
