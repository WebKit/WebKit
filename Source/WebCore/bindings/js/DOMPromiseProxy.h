/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "JSDOMGlobalObject.h"
#include "JSDOMPromiseDeferred.h"
#include <wtf/Function.h>
#include <wtf/Optional.h>
#include <wtf/Vector.h>

namespace WebCore {

template<typename IDLType>
class DOMPromiseProxy {
    WTF_MAKE_FAST_ALLOCATED;
public:
    using Value = typename IDLType::StorageType;

    DOMPromiseProxy() = default;
    ~DOMPromiseProxy() = default;

    JSC::JSValue promise(JSC::JSGlobalObject&, JSDOMGlobalObject&);

    void clear();

    bool isFulfilled() const;

    void resolve(typename IDLType::ParameterType);
    void resolveWithNewlyCreated(typename IDLType::ParameterType);
    void reject(Exception);
    
private:
    Optional<ExceptionOr<Value>> m_valueOrException;
    Vector<Ref<DeferredPromise>, 1> m_deferredPromises;
};

template<>
class DOMPromiseProxy<IDLVoid> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    DOMPromiseProxy() = default;
    ~DOMPromiseProxy() = default;

    JSC::JSValue promise(JSC::JSGlobalObject&, JSDOMGlobalObject&);

    void clear();

    bool isFulfilled() const;

    void resolve();
    void reject(Exception);

private:
    Optional<ExceptionOr<void>> m_valueOrException;
    Vector<Ref<DeferredPromise>, 1> m_deferredPromises;
};

// Instead of storing the value of the resolution directly, DOMPromiseProxyWithResolveCallback
// allows the owner to specify callback to be called when the resolved value is needed. This is
// needed to avoid reference cycles when the resolved value is the owner, such as is the case with
// FontFace and FontFaceSet.
template<typename IDLType>
class DOMPromiseProxyWithResolveCallback {
    WTF_MAKE_FAST_ALLOCATED;
public:
    using ResolveCallback = WTF::Function<typename IDLType::ParameterType ()>;

    template <typename Class, typename BaseClass>
    DOMPromiseProxyWithResolveCallback(Class&, typename IDLType::ParameterType (BaseClass::*)());
    DOMPromiseProxyWithResolveCallback(ResolveCallback&&);
    ~DOMPromiseProxyWithResolveCallback() = default;

    JSC::JSValue promise(JSC::JSGlobalObject&, JSDOMGlobalObject&);

    void clear();

    bool isFulfilled() const;

    void resolve(typename IDLType::ParameterType);
    void resolveWithNewlyCreated(typename IDLType::ParameterType);
    void reject(Exception);
    
private:
    ResolveCallback m_resolveCallback;
    Optional<ExceptionOr<void>> m_valueOrException;
    Vector<Ref<DeferredPromise>, 1> m_deferredPromises;
};


// MARK: - DOMPromiseProxy<IDLType> generic implementation

template<typename IDLType>
inline JSC::JSValue DOMPromiseProxy<IDLType>::promise(JSC::JSGlobalObject& lexicalGlobalObject, JSDOMGlobalObject& globalObject)
{
    UNUSED_PARAM(lexicalGlobalObject);
    for (auto& deferredPromise : m_deferredPromises) {
        if (deferredPromise->globalObject() == &globalObject)
            return deferredPromise->promise();
    }

    // DeferredPromise can fail construction during worker abrupt termination.
    auto deferredPromise = DeferredPromise::create(globalObject, DeferredPromise::Mode::RetainPromiseOnResolve);
    if (!deferredPromise)
        return JSC::jsUndefined();

    if (m_valueOrException) {
        if (m_valueOrException->hasException())
            deferredPromise->reject(m_valueOrException->exception());
        else
            deferredPromise->template resolve<IDLType>(m_valueOrException->returnValue());
    }

    auto result = deferredPromise->promise();
    m_deferredPromises.append(deferredPromise.releaseNonNull());
    return result;
}

template<typename IDLType>
inline void DOMPromiseProxy<IDLType>::clear()
{
    m_valueOrException = WTF::nullopt;
    m_deferredPromises.clear();
}

template<typename IDLType>
inline bool DOMPromiseProxy<IDLType>::isFulfilled() const
{
    return m_valueOrException.hasValue();
}

template<typename IDLType>
inline void DOMPromiseProxy<IDLType>::resolve(typename IDLType::ParameterType value)
{
    ASSERT(!m_valueOrException);

    m_valueOrException = ExceptionOr<Value> { std::forward<typename IDLType::ParameterType>(value) };
    for (auto& deferredPromise : m_deferredPromises)
        deferredPromise->template resolve<IDLType>(m_valueOrException->returnValue());
}

template<typename IDLType>
inline void DOMPromiseProxy<IDLType>::resolveWithNewlyCreated(typename IDLType::ParameterType value)
{
    ASSERT(!m_valueOrException);

    m_valueOrException = ExceptionOr<Value> { std::forward<typename IDLType::ParameterType>(value) };
    for (auto& deferredPromise : m_deferredPromises)
        deferredPromise->template resolveWithNewlyCreated<IDLType>(m_valueOrException->returnValue());
}

template<typename IDLType>
inline void DOMPromiseProxy<IDLType>::reject(Exception exception)
{
    ASSERT(!m_valueOrException);

    m_valueOrException = ExceptionOr<Value> { WTFMove(exception) };
    for (auto& deferredPromise : m_deferredPromises)
        deferredPromise->reject(m_valueOrException->exception());
}


// MARK: - DOMPromiseProxy<IDLVoid> specialization

inline JSC::JSValue DOMPromiseProxy<IDLVoid>::promise(JSC::JSGlobalObject& lexicalGlobalObject, JSDOMGlobalObject& globalObject)
{
    UNUSED_PARAM(lexicalGlobalObject);
    for (auto& deferredPromise : m_deferredPromises) {
        if (deferredPromise->globalObject() == &globalObject)
            return deferredPromise->promise();
    }

    // DeferredPromise can fail construction during worker abrupt termination.
    auto deferredPromise = DeferredPromise::create(globalObject, DeferredPromise::Mode::RetainPromiseOnResolve);
    if (!deferredPromise)
        return JSC::jsUndefined();

    if (m_valueOrException) {
        if (m_valueOrException->hasException())
            deferredPromise->reject(m_valueOrException->exception());
        else
            deferredPromise->resolve();
    }

    auto result = deferredPromise->promise();
    m_deferredPromises.append(deferredPromise.releaseNonNull());
    return result;
}

inline void DOMPromiseProxy<IDLVoid>::clear()
{
    m_valueOrException = WTF::nullopt;
    m_deferredPromises.clear();
}

inline bool DOMPromiseProxy<IDLVoid>::isFulfilled() const
{
    return m_valueOrException.hasValue();
}

inline void DOMPromiseProxy<IDLVoid>::resolve()
{
    ASSERT(!m_valueOrException);
    m_valueOrException = ExceptionOr<void> { };
    for (auto& deferredPromise : m_deferredPromises)
        deferredPromise->resolve();
}

inline void DOMPromiseProxy<IDLVoid>::reject(Exception exception)
{
    ASSERT(!m_valueOrException);
    m_valueOrException = ExceptionOr<void> { WTFMove(exception) };
    for (auto& deferredPromise : m_deferredPromises)
        deferredPromise->reject(m_valueOrException->exception());
}

// MARK: - DOMPromiseProxyWithResolveCallback<IDLType> implementation

template<typename IDLType>
template <typename Class, typename BaseClass>
inline DOMPromiseProxyWithResolveCallback<IDLType>::DOMPromiseProxyWithResolveCallback(Class& object, typename IDLType::ParameterType (BaseClass::*function)())
    : m_resolveCallback(std::bind(function, &object))
{
}

template<typename IDLType>
inline DOMPromiseProxyWithResolveCallback<IDLType>::DOMPromiseProxyWithResolveCallback(ResolveCallback&& function)
    : m_resolveCallback(WTFMove(function))
{
}

template<typename IDLType>
inline JSC::JSValue DOMPromiseProxyWithResolveCallback<IDLType>::promise(JSC::JSGlobalObject& lexicalGlobalObject, JSDOMGlobalObject& globalObject)
{
    UNUSED_PARAM(lexicalGlobalObject);
    for (auto& deferredPromise : m_deferredPromises) {
        if (deferredPromise->globalObject() == &globalObject)
            return deferredPromise->promise();
    }

    // DeferredPromise can fail construction during worker abrupt termination.
    auto deferredPromise = DeferredPromise::create(globalObject, DeferredPromise::Mode::RetainPromiseOnResolve);
    if (!deferredPromise)
        return JSC::jsUndefined();

    if (m_valueOrException) {
        if (m_valueOrException->hasException())
            deferredPromise->reject(m_valueOrException->exception());
        else
            deferredPromise->template resolve<IDLType>(m_resolveCallback());
    }

    auto result = deferredPromise->promise();
    m_deferredPromises.append(deferredPromise.releaseNonNull());
    return result;
}

template<typename IDLType>
inline void DOMPromiseProxyWithResolveCallback<IDLType>::clear()
{
    m_valueOrException = WTF::nullopt;
    m_deferredPromises.clear();
}

template<typename IDLType>
inline bool DOMPromiseProxyWithResolveCallback<IDLType>::isFulfilled() const
{
    return m_valueOrException.hasValue();
}

template<typename IDLType>
inline void DOMPromiseProxyWithResolveCallback<IDLType>::resolve(typename IDLType::ParameterType value)
{
    ASSERT(!m_valueOrException);

    m_valueOrException = ExceptionOr<void> { };
    for (auto& deferredPromise : m_deferredPromises)
        deferredPromise->template resolve<IDLType>(value);
}

template<typename IDLType>
inline void DOMPromiseProxyWithResolveCallback<IDLType>::resolveWithNewlyCreated(typename IDLType::ParameterType value)
{
    ASSERT(!m_valueOrException);

    m_valueOrException = ExceptionOr<void> { };
    for (auto& deferredPromise : m_deferredPromises)
        deferredPromise->template resolveWithNewlyCreated<IDLType>(value);
}

template<typename IDLType>
inline void DOMPromiseProxyWithResolveCallback<IDLType>::reject(Exception exception)
{
    ASSERT(!m_valueOrException);

    m_valueOrException = ExceptionOr<void> { WTFMove(exception) };
    for (auto& deferredPromise : m_deferredPromises)
        deferredPromise->reject(m_valueOrException->exception());
}

}
