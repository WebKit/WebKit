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

#include "JSDOMConvertResult.h"
#include "JSDOMExceptionHandling.h"
#include <JavaScriptCore/Error.h>
#include <concepts>

namespace WebCore {

// Conversion from JSValue -> Implementation
template<typename IDL> struct Converter;

// Conversion from JSValue -> Implementation for variadic arguments
template<typename IDL> struct VariadicConverter;

// Conversion from Implementation -> JSValue
template<typename IDL> struct JSConverter;

namespace Detail {

template<typename T> inline T* getPtrOrRef(const T* p) { return const_cast<T*>(p); }
template<typename T> inline T& getPtrOrRef(const T& p) { return const_cast<T&>(p); }
template<typename T> inline T* getPtrOrRef(const RefPtr<T>& p) { return p.get(); }
template<typename T> inline T& getPtrOrRef(const Ref<T>& p) { return p.get(); }

}

template<typename F, typename IDL>
concept DefaultValueFunctor = std::invocable<F> && std::same_as<std::invoke_result_t<F>, ConversionResult<IDL>>;

template<typename F>
concept ExceptionThrowerFunctor = std::invocable<F, JSC::JSGlobalObject&, JSC::ThrowScope&>;

struct DefaultExceptionThrower {
    void operator()(JSC::JSGlobalObject& lexicalGlobalObject, JSC::ThrowScope& scope)
    {
        throwTypeError(&lexicalGlobalObject, scope);
    }
};

template<typename IDL> ConversionResult<IDL> convert(JSC::JSGlobalObject&, JSC::JSValue);
template<typename IDL> ConversionResult<IDL> convert(JSC::JSGlobalObject&, JSC::JSValue, JSC::JSObject&);
template<typename IDL> ConversionResult<IDL> convert(JSC::JSGlobalObject&, JSC::JSValue, JSDOMGlobalObject&);
template<typename IDL, typename ExceptionThrower> ConversionResult<IDL> convert(JSC::JSGlobalObject&, JSC::JSValue, ExceptionThrower&&);
template<typename IDL, typename ExceptionThrower> ConversionResult<IDL> convert(JSC::JSGlobalObject&, JSC::JSValue, JSC::JSObject&, ExceptionThrower&&);
template<typename IDL, typename ExceptionThrower> ConversionResult<IDL> convert(JSC::JSGlobalObject&, JSC::JSValue, JSDOMGlobalObject&, ExceptionThrower&&);

template<typename IDL> inline ConversionResult<IDL> convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value)
{
    return Converter<IDL>::convert(lexicalGlobalObject, value);
}

template<typename IDL> inline ConversionResult<IDL> convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value, JSC::JSObject& thisObject)
{
    return Converter<IDL>::convert(lexicalGlobalObject, value, thisObject);
}

template<typename IDL> inline ConversionResult<IDL> convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value, JSDOMGlobalObject& globalObject)
{
    return Converter<IDL>::convert(lexicalGlobalObject, value, globalObject);
}

template<typename IDL, typename ExceptionThrower> inline ConversionResult<IDL> convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value, ExceptionThrower&& exceptionThrower)
{
    return Converter<IDL>::convert(lexicalGlobalObject, value, std::forward<ExceptionThrower>(exceptionThrower));
}

template<typename IDL, typename ExceptionThrower> inline ConversionResult<IDL> convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value, JSC::JSObject& thisObject, ExceptionThrower&& exceptionThrower)
{
    return Converter<IDL>::convert(lexicalGlobalObject, value, thisObject, std::forward<ExceptionThrower>(exceptionThrower));
}

template<typename IDL, typename ExceptionThrower> inline ConversionResult<IDL> convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value, JSDOMGlobalObject& globalObject, ExceptionThrower&& exceptionThrower)
{
    return Converter<IDL>::convert(lexicalGlobalObject, value, globalObject, std::forward<ExceptionThrower>(exceptionThrower));
}

template<typename IDL, typename U> inline JSC::JSValue toJS(U&&);
template<typename IDL, typename U> inline JSC::JSValue toJS(JSC::JSGlobalObject&, U&&);
template<typename IDL, typename U> inline JSC::JSValue toJS(JSC::JSGlobalObject&, JSDOMGlobalObject&, U&&);
template<typename IDL, typename U> inline JSC::JSValue toJSNewlyCreated(JSC::JSGlobalObject&, JSDOMGlobalObject&, U&&);

template<typename IDL, typename U> inline JSC::JSValue toJS(JSC::JSGlobalObject&, JSC::ThrowScope&, U&& valueOrFunctor);
template<typename IDL, typename U> inline JSC::JSValue toJS(JSC::JSGlobalObject&, JSDOMGlobalObject&, JSC::ThrowScope&, U&& valueOrFunctor);
template<typename IDL, typename U> inline JSC::JSValue toJSNewlyCreated(JSC::JSGlobalObject&, JSDOMGlobalObject&, JSC::ThrowScope&, U&& valueOrFunctor);

template<typename IDL, bool needsState = JSConverter<IDL>::needsState, bool needsGlobalObject = JSConverter<IDL>::needsGlobalObject>
struct JSConverterOverloader;

template<typename IDL>
struct JSConverterOverloader<IDL, true, true> {
    template<typename U> static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, JSDOMGlobalObject& globalObject, U&& value)
    {
        return JSConverter<IDL>::convert(lexicalGlobalObject, globalObject, std::forward<U>(value));
    }
};

template<typename IDL>
struct JSConverterOverloader<IDL, true, false> {
    template<typename U> static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, U&& value)
    {
        return JSConverter<IDL>::convert(lexicalGlobalObject, std::forward<U>(value));
    }

    template<typename U> static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, JSDOMGlobalObject&, U&& value)
    {
        return JSConverter<IDL>::convert(lexicalGlobalObject, std::forward<U>(value));
    }
};

template<typename IDL>
struct JSConverterOverloader<IDL, false, false> {
    template<typename U> static JSC::JSValue convert(JSC::JSGlobalObject&, U&& value)
    {
        return JSConverter<IDL>::convert(std::forward<U>(value));
    }

    template<typename U> static JSC::JSValue convert(JSC::JSGlobalObject&, JSDOMGlobalObject&, U&& value)
    {
        return JSConverter<IDL>::convert(std::forward<U>(value));
    }
};

template<typename IDL, typename U> inline JSC::JSValue toJS(U&& value)
{
    return JSConverter<IDL>::convert(std::forward<U>(value));
}

template<typename IDL, typename U> inline JSC::JSValue toJS(JSC::JSGlobalObject& lexicalGlobalObject, U&& value)
{
    return JSConverterOverloader<IDL>::convert(lexicalGlobalObject, std::forward<U>(value));
}

template<typename IDL, typename U> inline JSC::JSValue toJS(JSC::JSGlobalObject& lexicalGlobalObject, JSDOMGlobalObject& globalObject, U&& value)
{
    return JSConverterOverloader<IDL>::convert(lexicalGlobalObject, globalObject, std::forward<U>(value));
}

template<typename IDL, typename U> inline JSC::JSValue toJSNewlyCreated(JSC::JSGlobalObject& lexicalGlobalObject, JSDOMGlobalObject& globalObject, U&& value)
{
    return JSConverter<IDL>::convertNewlyCreated(lexicalGlobalObject, globalObject, std::forward<U>(value));
}

template<typename IDL, typename U> inline JSC::JSValue toJS(JSC::JSGlobalObject& lexicalGlobalObject, JSC::ThrowScope& throwScope, U&& valueOrFunctor)
{
    if constexpr (std::is_invocable_v<U>) {
        using FunctorReturnType = std::invoke_result_t<U>;

        if constexpr (std::is_same_v<void, FunctorReturnType>) {
            valueOrFunctor();
            return JSC::jsUndefined();
        } else if constexpr (std::is_same_v<ExceptionOr<void>, FunctorReturnType>) {
            auto result = valueOrFunctor();
            if (UNLIKELY(result.hasException())) {
                propagateException(lexicalGlobalObject, throwScope, result.releaseException());
                return { };
            }
            return JSC::jsUndefined();
        } else
            return toJS<IDL>(lexicalGlobalObject, throwScope, valueOrFunctor());
    } else {
        if constexpr (IsExceptionOr<U>) {
            if (UNLIKELY(valueOrFunctor.hasException())) {
                propagateException(lexicalGlobalObject, throwScope, valueOrFunctor.releaseException());
                return { };
            }

            return toJS<IDL>(lexicalGlobalObject, valueOrFunctor.releaseReturnValue());
        } else
            return toJS<IDL>(lexicalGlobalObject, std::forward<U>(valueOrFunctor));
    }
}

template<typename IDL, typename U> inline JSC::JSValue toJS(JSC::JSGlobalObject& lexicalGlobalObject, JSDOMGlobalObject& globalObject, JSC::ThrowScope& throwScope, U&& valueOrFunctor)
{
    if constexpr (std::is_invocable_v<U>) {
        using FunctorReturnType = std::invoke_result_t<U>;

        if constexpr (std::is_same_v<void, FunctorReturnType>) {
            valueOrFunctor();
            return JSC::jsUndefined();
        } else if constexpr (std::is_same_v<ExceptionOr<void>, FunctorReturnType>) {
            auto result = valueOrFunctor();
            if (UNLIKELY(result.hasException())) {
                propagateException(lexicalGlobalObject, throwScope, result.releaseException());
                return { };
            }
            return JSC::jsUndefined();
        } else
            return toJS<IDL>(lexicalGlobalObject, globalObject, throwScope, valueOrFunctor());
    } else {
        if constexpr (IsExceptionOr<U>) {
            if (UNLIKELY(valueOrFunctor.hasException())) {
                propagateException(lexicalGlobalObject, throwScope, valueOrFunctor.releaseException());
                return { };
            }

            return toJS<IDL>(lexicalGlobalObject, globalObject, valueOrFunctor.releaseReturnValue());
        } else
            return toJS<IDL>(lexicalGlobalObject, globalObject, std::forward<U>(valueOrFunctor));
    }
}

template<typename IDL, typename U> inline JSC::JSValue toJSNewlyCreated(JSC::JSGlobalObject& lexicalGlobalObject, JSDOMGlobalObject& globalObject, JSC::ThrowScope& throwScope, U&& valueOrFunctor)
{
    if constexpr (std::is_invocable_v<U>) {
        using FunctorReturnType = std::invoke_result_t<U>;

        if constexpr (std::is_same_v<void, FunctorReturnType>) {
            valueOrFunctor();
            return JSC::jsUndefined();
        } else if constexpr (std::is_same_v<ExceptionOr<void>, FunctorReturnType>) {
            auto result = valueOrFunctor();
            if (UNLIKELY(result.hasException())) {
                propagateException(lexicalGlobalObject, throwScope, result.releaseException());
                return { };
            }
            return JSC::jsUndefined();
        } else
            return toJSNewlyCreated<IDL>(lexicalGlobalObject, globalObject, throwScope, valueOrFunctor());

    } else {
        if constexpr (IsExceptionOr<U>) {
            if (UNLIKELY(valueOrFunctor.hasException())) {
                propagateException(lexicalGlobalObject, throwScope, valueOrFunctor.releaseException());
                return { };
            }

            return toJSNewlyCreated<IDL>(lexicalGlobalObject, globalObject, valueOrFunctor.releaseReturnValue());
        } else
            return toJSNewlyCreated<IDL>(lexicalGlobalObject, globalObject, std::forward<U>(valueOrFunctor));
    }
}

template<typename T> struct DefaultConverter {
    using ReturnType = typename T::ConversionResultType;

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

} // namespace WebCore
