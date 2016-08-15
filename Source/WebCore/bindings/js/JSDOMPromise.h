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

namespace WebCore {

template<typename DOMClass>
struct TypeInspector {
private:
    template<typename T> static constexpr auto testIsVector(int) -> decltype(std::declval<T>().shrinkToFit(), bool()) { return true; }
    template<typename T> static constexpr bool testIsVector(...) { return false; }

    template<typename T> static constexpr auto testIsRef(int) -> decltype(T::isRef) { return true; }
    template<typename T> static constexpr bool testIsRef(...) { return false; }

    template<typename T> static constexpr auto testIsRefPtr(int) -> decltype(T::isRefPtr) { return true; }
    template<typename T> static constexpr bool testIsRefPtr(...) { return false; }

public:
    static constexpr bool isRefOrRefPtr = testIsRef<DOMClass>(0) || testIsRefPtr<DOMClass>(0);
    static constexpr bool isPassByValueType = std::is_pointer<DOMClass>::value
        || std::is_same<DOMClass, std::nullptr_t>::value
        || std::is_same<DOMClass, JSC::JSValue>::value
        || std::is_same<DOMClass, bool>::value;
    static constexpr bool isPassByConstRefType = testIsVector<DOMClass>(0)
        || std::is_same<DOMClass, String>::value;
};

template<typename DOMClass, typename Enable = void>
struct PromiseResultInspector {
public:
    static constexpr bool passByValue = false;
    static constexpr bool passByRef = true;
    static constexpr bool passByURef = false;
    static constexpr bool passByConstRef = false;

    typedef DOMClass& Type;
};

template<typename DOMClass>
struct PromiseResultInspector<DOMClass, typename std::enable_if<TypeInspector<DOMClass>::isPassByValueType>::type> {
public:
    static constexpr bool passByValue = true;
    static constexpr bool passByRef = false;
    static constexpr bool passByURef = false;
    static constexpr bool passByConstRef = false;

    typedef DOMClass Type;
};

template<typename DOMClass>
struct PromiseResultInspector<DOMClass, typename std::enable_if<TypeInspector<DOMClass>::isPassByConstRefType>::type> {
public:
    static constexpr bool passByValue = false;
    static constexpr bool passByRef = false;
    static constexpr bool passByURef = false;
    static constexpr bool passByConstRef = true;

    typedef const DOMClass& Type;
};

template<typename DOMClass>
struct PromiseResultInspector<DOMClass, typename std::enable_if<TypeInspector<DOMClass>::isRefOrRefPtr>::type> {
    static constexpr bool passByValue = false;
    static constexpr bool passByRef = false;
    static constexpr bool passByURef = true;
    static constexpr bool passByConstRef = false;
};

class DeferredWrapper {
public:
    DeferredWrapper(JSC::ExecState*, JSDOMGlobalObject*, JSC::JSPromiseDeferred*);

    template<class ResolveResultType> typename std::enable_if<PromiseResultInspector<ResolveResultType>::passByValue, void>::type
    resolve(ResolveResultType result) { resolveWithValue(result); }
    template<class ResolveResultType> typename std::enable_if<PromiseResultInspector<ResolveResultType>::passByRef, void>::type
    resolve(ResolveResultType& result) { resolveWithValue(result); }
    template<class ResolveResultType> typename std::enable_if<PromiseResultInspector<ResolveResultType>::passByURef, void>::type
    resolve(ResolveResultType&& result) { resolveWithValue(std::forward<ResolveResultType>(result)); }
    template<class ResolveResultType> typename std::enable_if<PromiseResultInspector<ResolveResultType>::passByConstRef, void>::type
    resolve(const ResolveResultType& result) { resolveWithValue(result); }

    template<class RejectResultType> typename std::enable_if<PromiseResultInspector<RejectResultType>::passByValue, void>::type
    reject(RejectResultType result) { rejectWithValue(result); }
    template<class RejectResultType> typename std::enable_if<PromiseResultInspector<RejectResultType>::passByRef, void>::type
    reject(RejectResultType& result) { rejectWithValue(result); }
    template<class RejectResultType> typename std::enable_if<PromiseResultInspector<RejectResultType>::passByURef, void>::type
    reject(RejectResultType&& result) { rejectWithValue(std::forward<RejectResultType>(result)); }
    template<class RejectResultType> typename std::enable_if<PromiseResultInspector<RejectResultType>::passByConstRef, void>::type
    reject(const RejectResultType& result) { rejectWithValue(result); }

    template<class ResolveResultType> void resolveWithNewlyCreated(Ref<ResolveResultType>&&);

    void reject(ExceptionCode, const String& = { });

    JSDOMGlobalObject& globalObject() const;
    JSC::JSValue promise() const;

private:
    void callFunction(JSC::ExecState&, JSC::JSValue function, JSC::JSValue resolution);
    void resolve(JSC::ExecState& state, JSC::JSValue resolution) { callFunction(state, m_deferred->resolve(), resolution); }
    void reject(JSC::ExecState& state, JSC::JSValue resolution) { callFunction(state, m_deferred->reject(), resolution); }

    template<class RejectResultType> void rejectWithValue(RejectResultType&&);
    template<class ResolveResultType> void resolveWithValue(ResolveResultType&&);

    JSC::Strong<JSDOMGlobalObject> m_globalObject;
    JSC::Strong<JSC::JSPromiseDeferred> m_deferred;
};

void fulfillPromiseWithJSON(DeferredWrapper&, const String&);
void fulfillPromiseWithArrayBuffer(DeferredWrapper&, ArrayBuffer*);
void fulfillPromiseWithArrayBuffer(DeferredWrapper&, const void*, size_t);
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

// At the moment, Value cannot be a Ref<T> or RefPtr<T>, it should be a DOM class.
template <typename Value>
class DOMPromise {
public:
    DOMPromise(DeferredWrapper&& wrapper) : m_wrapper(WTFMove(wrapper)) { }
    DOMPromise(DOMPromise&& promise) : m_wrapper(WTFMove(promise.m_wrapper)) { }

    DOMPromise(const DOMPromise&) = default;
    DOMPromise& operator=(DOMPromise const&) = default;

    void resolve(typename PromiseResultInspector<Value>::Type value) { m_wrapper.resolve(value); }

    template<typename... ErrorType> void reject(ErrorType&&... error) { m_wrapper.reject(std::forward<ErrorType>(error)...); }

    DeferredWrapper& deferredWrapper() { return m_wrapper; }

private:
    DeferredWrapper m_wrapper;
};

template<class ResolveResultType>
inline void DeferredWrapper::resolveWithValue(ResolveResultType&& result)
{
    ASSERT(m_deferred);
    ASSERT(m_globalObject);
    JSC::ExecState* exec = m_globalObject->globalExec();
    JSC::JSLockHolder locker(exec);
    resolve(*exec, toJS(exec, m_globalObject.get(), std::forward<ResolveResultType>(result)));
}

template<class ResolveResultType>
inline void DeferredWrapper::resolveWithNewlyCreated(Ref<ResolveResultType>&& result)
{
    ASSERT(m_deferred);
    ASSERT(m_globalObject);
    JSC::ExecState* exec = m_globalObject->globalExec();
    JSC::JSLockHolder locker(exec);
    resolve(*exec, toJSNewlyCreated(exec, m_globalObject.get(), WTFMove(result)));
}

template<class RejectResultType>
inline void DeferredWrapper::rejectWithValue(RejectResultType&& result)
{
    ASSERT(m_deferred);
    ASSERT(m_globalObject);
    JSC::ExecState* exec = m_globalObject->globalExec();
    JSC::JSLockHolder locker(exec);
    reject(*exec, toJS(exec, m_globalObject.get(), std::forward<RejectResultType>(result)));
}

template<>
inline void DeferredWrapper::resolve(bool result)
{
    ASSERT(m_deferred);
    ASSERT(m_globalObject);
    JSC::ExecState* exec = m_globalObject->globalExec();
    JSC::JSLockHolder locker(exec);
    resolve(*exec, JSC::jsBoolean(result));
}

template<>
inline void DeferredWrapper::resolve(JSC::JSValue value)
{
    ASSERT(m_deferred);
    ASSERT(m_globalObject);
    JSC::ExecState* exec = m_globalObject->globalExec();
    JSC::JSLockHolder locker(exec);
    resolve(*exec, value);
}

template<>
inline void DeferredWrapper::reject(JSC::JSValue value)
{
    ASSERT(m_deferred);
    ASSERT(m_globalObject);
    JSC::ExecState* exec = m_globalObject->globalExec();
    JSC::JSLockHolder locker(exec);
    reject(*exec, value);
}

template<>
inline void DeferredWrapper::resolve(std::nullptr_t)
{
    ASSERT(m_deferred);
    ASSERT(m_globalObject);
    JSC::ExecState* exec = m_globalObject->globalExec();
    JSC::JSLockHolder locker(exec);
    resolve(*exec, JSC::jsUndefined());
}

template<>
inline void DeferredWrapper::reject(std::nullptr_t)
{
    ASSERT(m_deferred);
    ASSERT(m_globalObject);
    JSC::ExecState* exec = m_globalObject->globalExec();
    JSC::JSLockHolder locker(exec);
    reject(*exec, JSC::jsNull());
}

template<>
inline void DeferredWrapper::resolve(const String& result)
{
    ASSERT(m_deferred);
    ASSERT(m_globalObject);
    JSC::ExecState* exec = m_globalObject->globalExec();
    JSC::JSLockHolder locker(exec);
    resolve(*exec, jsString(exec, result));
}

template<>
inline void DeferredWrapper::reject(const String& result)
{
    ASSERT(m_deferred);
    ASSERT(m_globalObject);
    JSC::ExecState* exec = m_globalObject->globalExec();
    JSC::JSLockHolder locker(exec);
    reject(*exec, jsString(exec, result));
}

}

#endif // JSDOMPromise_h
