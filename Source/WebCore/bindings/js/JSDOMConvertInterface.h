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

#include "IDLTypes.h"
#include "JSDOMConvertBase.h"
#include <JavaScriptCore/Error.h>

namespace WebCore {

template<typename ImplementationClass> struct JSDOMWrapperConverterTraits;

template<typename T, typename Enable = void>
struct JSToWrappedOverloader {
    using ReturnType = typename JSDOMWrapperConverterTraits<T>::ToWrappedReturnType;
    using WrapperType = typename JSDOMWrapperConverterTraits<T>::WrapperClass;

    static ReturnType toWrapped(JSC::ExecState& state, JSC::JSValue value)
    {
        return WrapperType::toWrapped(state.vm(), value);
    }
};

template<typename T>
struct JSToWrappedOverloader<T, typename std::enable_if<JSDOMWrapperConverterTraits<T>::needsState>::type> {
    using ReturnType = typename JSDOMWrapperConverterTraits<T>::ToWrappedReturnType;
    using WrapperType = typename JSDOMWrapperConverterTraits<T>::WrapperClass;

    static ReturnType toWrapped(JSC::ExecState& state, JSC::JSValue value)
    {
        return WrapperType::toWrapped(state, value);
    }
};

template<typename T> struct Converter<IDLInterface<T>> : DefaultConverter<IDLInterface<T>> {
    using ReturnType = typename JSDOMWrapperConverterTraits<T>::ToWrappedReturnType;
    using WrapperType = typename JSDOMWrapperConverterTraits<T>::WrapperClass;

    template<typename ExceptionThrower = DefaultExceptionThrower>
    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value, ExceptionThrower&& exceptionThrower = ExceptionThrower())
    {
        auto& vm = state.vm();
        auto scope = DECLARE_THROW_SCOPE(vm);
        ReturnType object = JSToWrappedOverloader<T>::toWrapped(state, value);
        if (UNLIKELY(!object))
            exceptionThrower(state, scope);
        return object;
    }
};

template<typename T> struct JSConverter<IDLInterface<T>> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = true;

    template <typename U>
    static JSC::JSValue convert(JSC::ExecState& state, JSDOMGlobalObject& globalObject, const U& value)
    {
        return toJS(&state, &globalObject, Detail::getPtrOrRef(value));
    }

    template<typename U>
    static JSC::JSValue convertNewlyCreated(JSC::ExecState& state, JSDOMGlobalObject& globalObject, U&& value)
    {
        return toJSNewlyCreated(&state, &globalObject, std::forward<U>(value));
    }
};

template<typename T> struct VariadicConverter<IDLInterface<T>> {
    using Item = std::reference_wrapper<T>;

    static Optional<Item> convert(JSC::ExecState& state, JSC::JSValue value)
    {
        auto* result = Converter<IDLInterface<T>>::convert(state, value);
        if (!result)
            return WTF::nullopt;
        return Optional<Item> { *result };
    }
};

} // namespace WebCore
