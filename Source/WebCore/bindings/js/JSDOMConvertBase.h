/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "JSDOMExceptionHandling.h"
#include <JavaScriptCore/Error.h>

namespace WebCore {

// Conversion from JSValue -> Implementation
template<typename T> struct Converter;

namespace Detail {

template <typename T> inline T* getPtrOrRef(const T* p) { return const_cast<T*>(p); }
template <typename T> inline T& getPtrOrRef(const T& p) { return const_cast<T&>(p); }
template <typename T> inline T* getPtrOrRef(const RefPtr<T>& p) { return p.get(); }
template <typename T> inline T& getPtrOrRef(const Ref<T>& p) { return p.get(); }

}

struct DefaultExceptionThrower {
    void operator()(JSC::ExecState& state, JSC::ThrowScope& scope)
    {
        throwTypeError(&state, scope);
    }
};

template<typename T> typename Converter<T>::ReturnType convert(JSC::ExecState&, JSC::JSValue);
template<typename T> typename Converter<T>::ReturnType convert(JSC::ExecState&, JSC::JSValue, JSC::JSObject&);
template<typename T> typename Converter<T>::ReturnType convert(JSC::ExecState&, JSC::JSValue, JSDOMGlobalObject&);
template<typename T, typename ExceptionThrower> typename Converter<T>::ReturnType convert(JSC::ExecState&, JSC::JSValue, ExceptionThrower&&);
template<typename T, typename ExceptionThrower> typename Converter<T>::ReturnType convert(JSC::ExecState&, JSC::JSValue, JSC::JSObject&, ExceptionThrower&&);
template<typename T, typename ExceptionThrower> typename Converter<T>::ReturnType convert(JSC::ExecState&, JSC::JSValue, JSDOMGlobalObject&, ExceptionThrower&&);

template<typename T> inline typename Converter<T>::ReturnType convert(JSC::ExecState& state, JSC::JSValue value)
{
    return Converter<T>::convert(state, value);
}

template<typename T> inline typename Converter<T>::ReturnType convert(JSC::ExecState& state, JSC::JSValue value, JSC::JSObject& thisObject)
{
    return Converter<T>::convert(state, value, thisObject);
}

template<typename T> inline typename Converter<T>::ReturnType convert(JSC::ExecState& state, JSC::JSValue value, JSDOMGlobalObject& globalObject)
{
    return Converter<T>::convert(state, value, globalObject);
}

template<typename T, typename ExceptionThrower> inline typename Converter<T>::ReturnType convert(JSC::ExecState& state, JSC::JSValue value, ExceptionThrower&& exceptionThrower)
{
    return Converter<T>::convert(state, value, std::forward<ExceptionThrower>(exceptionThrower));
}

template<typename T, typename ExceptionThrower> inline typename Converter<T>::ReturnType convert(JSC::ExecState& state, JSC::JSValue value, JSC::JSObject& thisObject, ExceptionThrower&& exceptionThrower)
{
    return Converter<T>::convert(state, value, thisObject, std::forward<ExceptionThrower>(exceptionThrower));
}

template<typename T, typename ExceptionThrower> inline typename Converter<T>::ReturnType convert(JSC::ExecState& state, JSC::JSValue value, JSDOMGlobalObject& globalObject, ExceptionThrower&& exceptionThrower)
{
    return Converter<T>::convert(state, value, globalObject, std::forward<ExceptionThrower>(exceptionThrower));
}


template <typename T>
struct IsExceptionOr : public std::integral_constant<bool, WTF::IsTemplate<std::decay_t<T>, ExceptionOr>::value> { };


// Conversion from Implementation -> JSValue
template<typename T> struct JSConverter;

template<typename T, typename U> inline JSC::JSValue toJS(U&&);
template<typename T, typename U> inline JSC::JSValue toJS(JSC::ExecState&, U&&);
template<typename T, typename U> inline JSC::JSValue toJS(JSC::ExecState&, JSDOMGlobalObject&, U&&);
template<typename T, typename U> inline JSC::JSValue toJSNewlyCreated(JSC::ExecState&, JSDOMGlobalObject&, U&&);
template<typename T, typename U> inline auto toJS(JSC::ExecState&, JSC::ThrowScope&, U&&) -> std::enable_if_t<IsExceptionOr<U>::value, JSC::JSValue>;
template<typename T, typename U> inline auto toJS(JSC::ExecState&, JSC::ThrowScope&, U&&) -> std::enable_if_t<!IsExceptionOr<U>::value, JSC::JSValue>;
template<typename T, typename U> inline auto toJS(JSC::ExecState&, JSDOMGlobalObject&, JSC::ThrowScope&, U&&) -> std::enable_if_t<IsExceptionOr<U>::value, JSC::JSValue>;
template<typename T, typename U> inline auto toJS(JSC::ExecState&, JSDOMGlobalObject&, JSC::ThrowScope&, U&&) -> std::enable_if_t<!IsExceptionOr<U>::value, JSC::JSValue>;
template<typename T, typename U> inline auto toJSNewlyCreated(JSC::ExecState&, JSDOMGlobalObject&, JSC::ThrowScope&, U&&) -> std::enable_if_t<IsExceptionOr<U>::value, JSC::JSValue>;
template<typename T, typename U> inline auto toJSNewlyCreated(JSC::ExecState&, JSDOMGlobalObject&, JSC::ThrowScope&, U&&) -> std::enable_if_t<!IsExceptionOr<U>::value, JSC::JSValue>;

template<typename T, bool needsState = JSConverter<T>::needsState, bool needsGlobalObject = JSConverter<T>::needsGlobalObject>
struct JSConverterOverloader;

template<typename T>
struct JSConverterOverloader<T, true, true> {
    template<typename U> static JSC::JSValue convert(JSC::ExecState& state, JSDOMGlobalObject& globalObject, U&& value)
    {
        return JSConverter<T>::convert(state, globalObject, std::forward<U>(value));
    }
};

template<typename T>
struct JSConverterOverloader<T, true, false> {
    template<typename U> static JSC::JSValue convert(JSC::ExecState& state, U&& value)
    {
        return JSConverter<T>::convert(state, std::forward<U>(value));
    }

    template<typename U> static JSC::JSValue convert(JSC::ExecState& state, JSDOMGlobalObject&, U&& value)
    {
        return JSConverter<T>::convert(state, std::forward<U>(value));
    }
};

template<typename T>
struct JSConverterOverloader<T, false, false> {
    template<typename U> static JSC::JSValue convert(JSC::ExecState&, U&& value)
    {
        return JSConverter<T>::convert(std::forward<U>(value));
    }

    template<typename U> static JSC::JSValue convert(JSC::ExecState&, JSDOMGlobalObject&, U&& value)
    {
        return JSConverter<T>::convert(std::forward<U>(value));
    }
};

template<typename T, typename U> inline JSC::JSValue toJS(U&& value)
{
    return JSConverter<T>::convert(std::forward<U>(value));
}

template<typename T, typename U> inline JSC::JSValue toJS(JSC::ExecState& state, U&& value)
{
    return JSConverterOverloader<T>::convert(state, std::forward<U>(value));
}

template<typename T, typename U> inline JSC::JSValue toJS(JSC::ExecState& state, JSDOMGlobalObject& globalObject, U&& value)
{
    return JSConverterOverloader<T>::convert(state, globalObject, std::forward<U>(value));
}

template<typename T, typename U> inline JSC::JSValue toJSNewlyCreated(JSC::ExecState& state, JSDOMGlobalObject& globalObject, U&& value)
{
    return JSConverter<T>::convertNewlyCreated(state, globalObject, std::forward<U>(value));
}

template<typename T, typename U> inline auto toJS(JSC::ExecState& state, JSC::ThrowScope& throwScope, U&& value) -> std::enable_if_t<IsExceptionOr<U>::value, JSC::JSValue>
{
    if (UNLIKELY(value.hasException())) {
        propagateException(state, throwScope, value.releaseException());
        return { };
    }

    return toJS<T>(state, value.releaseReturnValue());
}

template<typename T, typename U> inline auto toJS(JSC::ExecState& state, JSC::ThrowScope&, U&& value) -> std::enable_if_t<!IsExceptionOr<U>::value, JSC::JSValue>
{
    return toJS<T>(state, std::forward<U>(value));
}

template<typename T, typename U> inline auto toJS(JSC::ExecState& state, JSDOMGlobalObject& globalObject, JSC::ThrowScope& throwScope, U&& value) -> std::enable_if_t<IsExceptionOr<U>::value, JSC::JSValue>
{
    if (UNLIKELY(value.hasException())) {
        propagateException(state, throwScope, value.releaseException());
        return { };
    }

    return toJS<T>(state, globalObject, value.releaseReturnValue());
}

template<typename T, typename U> inline auto toJS(JSC::ExecState& state, JSDOMGlobalObject& globalObject, JSC::ThrowScope&, U&& value) -> std::enable_if_t<!IsExceptionOr<U>::value, JSC::JSValue>
{
    return toJS<T>(state, globalObject, std::forward<U>(value));
}

template<typename T, typename U> inline auto toJSNewlyCreated(JSC::ExecState& state, JSDOMGlobalObject& globalObject, JSC::ThrowScope& throwScope, U&& value) -> std::enable_if_t<IsExceptionOr<U>::value, JSC::JSValue>
{
    if (UNLIKELY(value.hasException())) {
        propagateException(state, throwScope, value.releaseException());
        return { };
    }

    return toJSNewlyCreated<T>(state, globalObject, value.releaseReturnValue());
}

template<typename T, typename U> inline auto toJSNewlyCreated(JSC::ExecState& state, JSDOMGlobalObject& globalObject, JSC::ThrowScope&, U&& value) -> std::enable_if_t<!IsExceptionOr<U>::value, JSC::JSValue>
{
    return toJSNewlyCreated<T>(state, globalObject, std::forward<U>(value));
}


template<typename T> struct DefaultConverter {
    using ReturnType = typename T::ImplementationType;

    // We assume the worst, subtypes can override to be less pessimistic.
    // An example of something that can have side effects
    // is having a converter that does JSC::JSValue::toNumber.
    // toNumber() in JavaScript can call arbitrary JS functions.
    //
    // An example of something that does not have side effects
    // is something having a converter that does JSC::JSValue::toBoolean.
    // toBoolean() in JS can't call arbitrary functions.
    static constexpr bool conversionHasSideEffects = true;
};

// Conversion from JSValue -> Implementation for variadic arguments
template<typename IDLType> struct VariadicConverter;

} // namespace WebCore
