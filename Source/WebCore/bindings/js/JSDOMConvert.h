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

#include "BufferSource.h"
#include "IDLTypes.h"
#include "JSDOMBinding.h"

namespace WebCore {

// Conversion from JSValue -> Implementation
template<typename T> struct Converter;

enum class IntegerConversionConfiguration { Normal, EnforceRange, Clamp };
enum class StringConversionConfiguration { Normal, TreatNullAsEmptyString };

struct DefaultExceptionThrower {
    void operator()(JSC::ExecState& state, JSC::ThrowScope& scope)
    {
        throwTypeError(&state, scope);
    }
};

template<typename T> typename Converter<T>::ReturnType convert(JSC::ExecState&, JSC::JSValue);
template<typename T> typename Converter<T>::ReturnType convert(JSC::ExecState&, JSC::JSValue, JSC::JSObject&);
template<typename T> typename Converter<T>::ReturnType convert(JSC::ExecState&, JSC::JSValue, JSDOMGlobalObject&);
template<typename T> typename Converter<T>::ReturnType convert(JSC::ExecState&, JSC::JSValue, IntegerConversionConfiguration);
template<typename T> typename Converter<T>::ReturnType convert(JSC::ExecState&, JSC::JSValue, StringConversionConfiguration);
template<typename T, typename ExceptionThrower> typename Converter<T>::ReturnType convert(JSC::ExecState&, JSC::JSValue, ExceptionThrower&&);
template<typename T, typename ExceptionThrower> typename Converter<T>::ReturnType convert(JSC::ExecState&, JSC::JSValue, JSC::JSObject&, ExceptionThrower&&);
template<typename T, typename ExceptionThrower> typename Converter<T>::ReturnType convert(JSC::ExecState&, JSC::JSValue, JSDOMGlobalObject&, ExceptionThrower&&);

// Specialized by generated code for IDL dictionary conversion.
template<typename T> T convertDictionary(JSC::ExecState&, JSC::JSValue);

// Specialized by generated code for IDL enumeration conversion.
template<typename T> std::optional<T> parseEnumeration(JSC::ExecState&, JSC::JSValue);
template<typename T> T convertEnumeration(JSC::ExecState&, JSC::JSValue);
template<typename T> const char* expectedEnumerationValues();

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

template<typename T> inline typename Converter<T>::ReturnType convert(JSC::ExecState& state, JSC::JSValue value, IntegerConversionConfiguration configuration)
{
    return Converter<T>::convert(state, value, configuration);
}

template<typename T> inline typename Converter<T>::ReturnType convert(JSC::ExecState& state, JSC::JSValue value, StringConversionConfiguration configuration)
{
    return Converter<T>::convert(state, value, configuration);
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

// Conversion from Implementation -> JSValue
template<typename T> struct JSConverter;

template<typename T, typename U> inline JSC::JSValue toJS(U&&);
template<typename T, typename U> inline JSC::JSValue toJS(JSC::ExecState&, U&&);
template<typename T, typename U> inline JSC::JSValue toJS(JSC::ExecState&, JSDOMGlobalObject&, U&&);
template<typename T, typename U> inline JSC::JSValue toJS(JSC::ExecState&, JSC::ThrowScope&, ExceptionOr<U>&&);
template<typename T, typename U> inline JSC::JSValue toJS(JSC::ExecState&, JSDOMGlobalObject&, JSC::ThrowScope&, ExceptionOr<U>&&);
template<typename T, typename U> inline JSC::JSValue toJSNewlyCreated(JSC::ExecState&, JSDOMGlobalObject&, U&&);
template<typename T, typename U> inline JSC::JSValue toJSNewlyCreated(JSC::ExecState&, JSDOMGlobalObject&, JSC::ThrowScope&, ExceptionOr<U>&&);

// Specialized by generated code for IDL enumeration conversion.
template<typename T> JSC::JSString* convertEnumerationToJS(JSC::ExecState&, T);


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

template<typename T, typename U> inline JSC::JSValue toJS(JSC::ExecState& state, JSC::ThrowScope& throwScope, ExceptionOr<U>&& value)
{
    if (UNLIKELY(value.hasException())) {
        propagateException(state, throwScope, value.releaseException());
        return { };
    }

    return toJS<T>(state, value.releaseReturnValue());
}

template<typename T, typename U> inline JSC::JSValue toJS(JSC::ExecState& state, JSDOMGlobalObject& globalObject, JSC::ThrowScope& throwScope, ExceptionOr<U>&& value)
{
    if (UNLIKELY(value.hasException())) {
        propagateException(state, throwScope, value.releaseException());
        return { };
    }

    return toJS<T>(state, globalObject, value.releaseReturnValue());
}

template<typename T, typename U> inline JSC::JSValue toJSNewlyCreated(JSC::ExecState& state, JSDOMGlobalObject& globalObject, U&& value)
{
    return JSConverter<T>::convertNewlyCreated(state, globalObject, std::forward<U>(value));
}

template<typename T, typename U> inline JSC::JSValue toJSNewlyCreated(JSC::ExecState& state, JSDOMGlobalObject& globalObject, JSC::ThrowScope& throwScope, ExceptionOr<U>&& value)
{
    if (UNLIKELY(value.hasException())) {
        propagateException(state, throwScope, value.releaseException());
        return { };
    }

    return toJSNewlyCreated<T>(state, globalObject, value.releaseReturnValue());
}


template<typename T> struct DefaultConverter {
    using ReturnType = typename T::ImplementationType;
};

// MARK: -
// MARK: Nullable type

namespace Detail {
    template<typename IDLType>
    struct NullableConversionType;

    template<typename IDLType> 
    struct NullableConversionType {
        using Type = typename IDLNullable<IDLType>::ImplementationType;
    };

    template<typename T>
    struct NullableConversionType<IDLInterface<T>> {
        using Type = typename Converter<IDLInterface<T>>::ReturnType;
    };
}

template<typename T> struct Converter<IDLNullable<T>> : DefaultConverter<IDLNullable<T>> {
    using ReturnType = typename Detail::NullableConversionType<T>::Type;
    
    // 1. If Type(V) is not Object, and the conversion to an IDL value is being performed
    // due to V being assigned to an attribute whose type is a nullable callback function
    // that is annotated with [TreatNonObjectAsNull], then return the IDL nullable type T?
    // value null.
    //
    // NOTE: Handled elsewhere.
    //
    // 2. Otherwise, if V is null or undefined, then return the IDL nullable type T? value null.
    // 3. Otherwise, return the result of converting V using the rules for the inner IDL type T.

    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value)
    {
        if (value.isUndefinedOrNull())
            return T::nullValue();
        return Converter<T>::convert(state, value);
    }
    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value, JSC::JSObject& thisObject)
    {
        if (value.isUndefinedOrNull())
            return T::nullValue();
        return Converter<T>::convert(state, value, thisObject);
    }
    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value, JSDOMGlobalObject& globalObject)
    {
        if (value.isUndefinedOrNull())
            return T::nullValue();
        return Converter<T>::convert(state, value, globalObject);
    }
    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value, IntegerConversionConfiguration configuration)
    {
        if (value.isUndefinedOrNull())
            return T::nullValue();
        return Converter<T>::convert(state, value, configuration);
    }
    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value, StringConversionConfiguration configuration)
    {
        if (value.isUndefinedOrNull())
            return T::nullValue();
        return Converter<T>::convert(state, value, configuration);
    }
    template<typename ExceptionThrower = DefaultExceptionThrower>
    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value, ExceptionThrower&& exceptionThrower)
    {
        if (value.isUndefinedOrNull())
            return T::nullValue();
        return Converter<T>::convert(state, value, std::forward<ExceptionThrower>(exceptionThrower));
    }
    template<typename ExceptionThrower = DefaultExceptionThrower>
    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value, JSC::JSObject& thisObject, ExceptionThrower&& exceptionThrower)
    {
        if (value.isUndefinedOrNull())
            return T::nullValue();
        return Converter<T>::convert(state, value, thisObject, std::forward<ExceptionThrower>(exceptionThrower));
    }
    template<typename ExceptionThrower = DefaultExceptionThrower>
    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value, JSDOMGlobalObject& globalObject, ExceptionThrower&& exceptionThrower)
    {
        if (value.isUndefinedOrNull())
            return T::nullValue();
        return Converter<T>::convert(state, value, globalObject, std::forward<ExceptionThrower>(exceptionThrower));
    }
};

template<typename T> struct JSConverter<IDLNullable<T>> {
    using ImplementationType = typename IDLNullable<T>::ImplementationType;

    static constexpr bool needsState = JSConverter<T>::needsState;
    static constexpr bool needsGlobalObject = JSConverter<T>::needsGlobalObject;

    template<typename U>
    static JSC::JSValue convert(U&& value)
    {
        if (T::isNullValue(value))
            return JSC::jsNull();
        return JSConverter<T>::convert(T::extractValueFromNullable(value));
    }
    template<typename U>
    static JSC::JSValue convert(JSC::ExecState& state, U&& value)
    {
        if (T::isNullValue(value))
            return JSC::jsNull();
        return JSConverter<T>::convert(state, T::extractValueFromNullable(value));
    }
    template<typename U>
    static JSC::JSValue convert(JSC::ExecState& state, JSDOMGlobalObject& globalObject, U&& value)
    {
        if (T::isNullValue(value))
            return JSC::jsNull();
        return JSConverter<T>::convert(state, globalObject, T::extractValueFromNullable(value));
    }

    template<typename U>
    static JSC::JSValue convertNewlyCreated(JSC::ExecState& state, JSDOMGlobalObject& globalObject, U&& value)
    {
        if (T::isNullValue(value))
            return JSC::jsNull();
        return JSConverter<T>::convert(state, globalObject, T::extractValueFromNullable(value));
    }
};

// MARK: -
// MARK: Boolean type

template<> struct Converter<IDLBoolean> : DefaultConverter<IDLBoolean> {
    static bool convert(JSC::ExecState& state, JSC::JSValue value)
    {
        return value.toBoolean(&state);
    }
};

template<> struct JSConverter<IDLBoolean> {
    static constexpr bool needsState = false;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(bool value)
    {
        return JSC::jsBoolean(value);
    }
};

// ArrayBuffer support.
template<> struct JSDOMWrapperConverterTraits<JSC::ArrayBuffer> {
    using WrapperClass = JSC::JSArrayBuffer;
    using ToWrappedReturnType = JSC::ArrayBuffer*;
};

// ArrayBufferView support.
template<> struct JSDOMWrapperConverterTraits<JSC::ArrayBufferView> {
    using WrapperClass = JSC::JSArrayBufferView;
    using ToWrappedReturnType = RefPtr<ArrayBufferView>;
};

// Typed arrays support.
template<typename Adaptor> struct JSDOMWrapperConverterTraits<JSC::GenericTypedArrayView<Adaptor>> {
    using WrapperClass = JSC::JSGenericTypedArrayView<Adaptor>;
    using ToWrappedReturnType = RefPtr<JSC::GenericTypedArrayView<Adaptor>>;
};

// MARK: -
// MARK: Interface type

template<typename T> struct Converter<IDLInterface<T>> : DefaultConverter<IDLInterface<T>> {
    using ReturnType = typename JSDOMWrapperConverterTraits<T>::ToWrappedReturnType;
    using WrapperType = typename JSDOMWrapperConverterTraits<T>::WrapperClass;

    template<typename ExceptionThrower = DefaultExceptionThrower>
    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value, ExceptionThrower&& exceptionThrower = ExceptionThrower())
    {
        JSC::VM& vm = state.vm();
        auto scope = DECLARE_THROW_SCOPE(vm);
        ReturnType object = WrapperType::toWrapped(value);
        if (UNLIKELY(!object))
            exceptionThrower(state, scope);
        return object;
    }
};

namespace Detail {

template <typename T> inline T* getPtrOrRef(const T* p) { return const_cast<T*>(p); }
template <typename T> inline T& getPtrOrRef(const T& p) { return const_cast<T&>(p); }
template <typename T> inline T* getPtrOrRef(const RefPtr<T>& p) { return const_cast<T*>(p.get()); }
template <typename T> inline T& getPtrOrRef(const Ref<T>& p) { return const_cast<T&>(p.get()); }

}

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

// MARK: -
// MARK: Any type

template<> struct Converter<IDLAny> : DefaultConverter<IDLAny> {
    static JSC::JSValue convert(JSC::ExecState&, JSC::JSValue value)
    {
        return value;
    }
};

template<> struct JSConverter<IDLAny> {
    static constexpr bool needsState = false;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(const JSC::JSValue& value)
    {
        return value;
    }
};

// MARK: -
// MARK: Integer types

template<> struct Converter<IDLByte> : DefaultConverter<IDLByte> {
    static int8_t convert(JSC::ExecState& state, JSC::JSValue value, IntegerConversionConfiguration configuration = IntegerConversionConfiguration::Normal)
    {
        switch (configuration) {
        case IntegerConversionConfiguration::Normal:
            break;
        case IntegerConversionConfiguration::EnforceRange:
            return toInt8EnforceRange(state, value);
        case IntegerConversionConfiguration::Clamp:
            return toInt8Clamp(state, value);
        }
        return toInt8(state, value);
    }
};

template<> struct JSConverter<IDLByte> {
    using Type = typename IDLByte::ImplementationType;

    static constexpr bool needsState = false;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(Type value)
    {
        return JSC::jsNumber(value);
    }
};

template<> struct Converter<IDLOctet> : DefaultConverter<IDLOctet> {
    static uint8_t convert(JSC::ExecState& state, JSC::JSValue value, IntegerConversionConfiguration configuration = IntegerConversionConfiguration::Normal)
    {
        switch (configuration) {
        case IntegerConversionConfiguration::Normal:
            break;
        case IntegerConversionConfiguration::EnforceRange:
            return toUInt8EnforceRange(state, value);
        case IntegerConversionConfiguration::Clamp:
            return toUInt8Clamp(state, value);
        }
        return toUInt8(state, value);
    }
};

template<> struct JSConverter<IDLOctet> {
    using Type = typename IDLOctet::ImplementationType;

    static constexpr bool needsState = false;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(Type value)
    {
        return JSC::jsNumber(value);
    }
};

template<> struct Converter<IDLShort> : DefaultConverter<IDLShort> {
    static int16_t convert(JSC::ExecState& state, JSC::JSValue value, IntegerConversionConfiguration configuration = IntegerConversionConfiguration::Normal)
    {
        switch (configuration) {
        case IntegerConversionConfiguration::Normal:
            break;
        case IntegerConversionConfiguration::EnforceRange:
            return toInt16EnforceRange(state, value);
        case IntegerConversionConfiguration::Clamp:
            return toInt16Clamp(state, value);
        }
        return toInt16(state, value);
    }
};

template<> struct JSConverter<IDLShort> {
    using Type = typename IDLShort::ImplementationType;

    static constexpr bool needsState = false;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(Type value)
    {
        return JSC::jsNumber(value);
    }
};

template<> struct Converter<IDLUnsignedShort> : DefaultConverter<IDLUnsignedShort> {
    static uint16_t convert(JSC::ExecState& state, JSC::JSValue value, IntegerConversionConfiguration configuration = IntegerConversionConfiguration::Normal)
    {
        switch (configuration) {
        case IntegerConversionConfiguration::Normal:
            break;
        case IntegerConversionConfiguration::EnforceRange:
            return toUInt16EnforceRange(state, value);
        case IntegerConversionConfiguration::Clamp:
            return toUInt16Clamp(state, value);
        }
        return toUInt16(state, value);
    }
};

template<> struct JSConverter<IDLUnsignedShort> {
    using Type = typename IDLUnsignedShort::ImplementationType;

    static constexpr bool needsState = false;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(Type value)
    {
        return JSC::jsNumber(value);
    }
};

template<> struct Converter<IDLLong> : DefaultConverter<IDLLong> {
    static int32_t convert(JSC::ExecState& state, JSC::JSValue value, IntegerConversionConfiguration configuration = IntegerConversionConfiguration::Normal)
    {
        switch (configuration) {
        case IntegerConversionConfiguration::Normal:
            break;
        case IntegerConversionConfiguration::EnforceRange:
            return toInt32EnforceRange(state, value);
        case IntegerConversionConfiguration::Clamp:
            return toInt32Clamp(state, value);
        }
        return value.toInt32(&state);
    }
};

template<> struct JSConverter<IDLLong> {
    using Type = typename IDLLong::ImplementationType;

    static constexpr bool needsState = false;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(Type value)
    {
        return JSC::jsNumber(value);
    }
};

template<> struct Converter<IDLUnsignedLong> : DefaultConverter<IDLUnsignedLong> {
    static uint32_t convert(JSC::ExecState& state, JSC::JSValue value, IntegerConversionConfiguration configuration = IntegerConversionConfiguration::Normal)
    {
        switch (configuration) {
        case IntegerConversionConfiguration::Normal:
            break;
        case IntegerConversionConfiguration::EnforceRange:
            return toUInt32EnforceRange(state, value);
        case IntegerConversionConfiguration::Clamp:
            return toUInt32Clamp(state, value);
        }
        return value.toUInt32(&state);
    }
};

template<> struct JSConverter<IDLUnsignedLong> {
    using Type = typename IDLUnsignedLong::ImplementationType;

    static constexpr bool needsState = false;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(Type value)
    {
        return JSC::jsNumber(value);
    }
};

template<> struct Converter<IDLLongLong> : DefaultConverter<IDLLongLong> {
    static int64_t convert(JSC::ExecState& state, JSC::JSValue value, IntegerConversionConfiguration configuration = IntegerConversionConfiguration::Normal)
    {
        if (value.isInt32())
            return value.asInt32();

        switch (configuration) {
        case IntegerConversionConfiguration::Normal:
            break;
        case IntegerConversionConfiguration::EnforceRange:
            return toInt64EnforceRange(state, value);
        case IntegerConversionConfiguration::Clamp:
            return toInt64Clamp(state, value);
        }
        return toInt64(state, value);
    }
};

template<> struct JSConverter<IDLLongLong> {
    using Type = typename IDLLongLong::ImplementationType;

    static constexpr bool needsState = false;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(Type value)
    {
        return JSC::jsNumber(value);
    }
};

template<> struct Converter<IDLUnsignedLongLong> : DefaultConverter<IDLUnsignedLongLong> {
    static uint64_t convert(JSC::ExecState& state, JSC::JSValue value, IntegerConversionConfiguration configuration = IntegerConversionConfiguration::Normal)
    {
        if (value.isUInt32())
            return value.asUInt32();

        switch (configuration) {
        case IntegerConversionConfiguration::Normal:
            break;
        case IntegerConversionConfiguration::EnforceRange:
            return toUInt64EnforceRange(state, value);
        case IntegerConversionConfiguration::Clamp:
            return toUInt64Clamp(state, value);
        }
        return toUInt64(state, value);
    }
};

template<> struct JSConverter<IDLUnsignedLongLong> {
    using Type = typename IDLUnsignedLongLong::ImplementationType;

    static constexpr bool needsState = false;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(Type value)
    {
        return JSC::jsNumber(value);
    }
};

// MARK: -
// MARK: Floating point types

template<> struct Converter<IDLFloat> : DefaultConverter<IDLFloat> {
    static float convert(JSC::ExecState& state, JSC::JSValue value)
    {
        JSC::VM& vm = state.vm();
        auto scope = DECLARE_THROW_SCOPE(vm);
        double number = value.toNumber(&state);
        if (UNLIKELY(!std::isfinite(number)))
            throwNonFiniteTypeError(state, scope);
        return static_cast<float>(number);
    }
};

template<> struct JSConverter<IDLFloat> {
    using Type = typename IDLFloat::ImplementationType;

    static constexpr bool needsState = false;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(Type value)
    {
        return JSC::jsNumber(value);
    }
};

template<> struct Converter<IDLUnrestrictedFloat> : DefaultConverter<IDLUnrestrictedFloat> {
    static float convert(JSC::ExecState& state, JSC::JSValue value)
    {
        return static_cast<float>(value.toNumber(&state));
    }
};

template<> struct JSConverter<IDLUnrestrictedFloat> {
    using Type = typename IDLUnrestrictedFloat::ImplementationType;

    static constexpr bool needsState = false;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(Type value)
    {
        return JSC::jsNumber(value);
    }
};

template<> struct Converter<IDLDouble> : DefaultConverter<IDLDouble> {
    static double convert(JSC::ExecState& state, JSC::JSValue value)
    {
        JSC::VM& vm = state.vm();
        auto scope = DECLARE_THROW_SCOPE(vm);
        double number = value.toNumber(&state);
        if (UNLIKELY(!std::isfinite(number)))
            throwNonFiniteTypeError(state, scope);
        return number;
    }
};

template<> struct JSConverter<IDLDouble> {
    using Type = typename IDLDouble::ImplementationType;

    static constexpr bool needsState = false;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(Type value)
    {
        return JSC::jsNumber(value);
    }
};

template<> struct Converter<IDLUnrestrictedDouble> : DefaultConverter<IDLUnrestrictedDouble> {
    static double convert(JSC::ExecState& state, JSC::JSValue value)
    {
        return value.toNumber(&state);
    }
};

template<> struct JSConverter<IDLUnrestrictedDouble> {
    using Type = typename IDLUnrestrictedDouble::ImplementationType;

    static constexpr bool needsState = false;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(Type value)
    {
        return JSC::jsNumber(value);
    }

    // Add overload for MediaTime.
    static JSC::JSValue convert(MediaTime value)
    {
        return JSC::jsNumber(value.toDouble());
    }
};

// MARK: -
// MARK: String types

template<> struct Converter<IDLDOMString> : DefaultConverter<IDLDOMString> {
    static String convert(JSC::ExecState& state, JSC::JSValue value, StringConversionConfiguration configuration = StringConversionConfiguration::Normal)
    {
        if (configuration == StringConversionConfiguration::TreatNullAsEmptyString && value.isNull())
            return emptyString();
        return value.toWTFString(&state);
    }
};

template<> struct JSConverter<IDLDOMString> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::ExecState& state, const String& value)
    {
        return JSC::jsStringWithCache(&state, value);
    }
};

template<> struct Converter<IDLByteString> : DefaultConverter<IDLByteString> {
    static String convert(JSC::ExecState& state, JSC::JSValue value, StringConversionConfiguration configuration = StringConversionConfiguration::Normal)
    {
        if (configuration == StringConversionConfiguration::TreatNullAsEmptyString && value.isNull())
            return emptyString();
        return valueToByteString(state, value);
    }
};

template<> struct JSConverter<IDLByteString> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::ExecState& state, const String& value)
    {
        return JSC::jsStringWithCache(&state, value);
    }
};

template<> struct Converter<IDLUSVString> : DefaultConverter<IDLUSVString> {
    static String convert(JSC::ExecState& state, JSC::JSValue value, StringConversionConfiguration configuration = StringConversionConfiguration::Normal)
    {
        if (configuration == StringConversionConfiguration::TreatNullAsEmptyString && value.isNull())
            return emptyString();
        return valueToUSVString(state, value);
    }
};

template<> struct JSConverter<IDLUSVString> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::ExecState& state, const String& value)
    {
        return JSC::jsStringWithCache(&state, value);
    }
};

// MARK: -
// MARK: Array-like types

namespace Detail {
    template<typename IDLType>
    struct ArrayConverterBase;

    template<typename IDLType> 
    struct ArrayConverterBase {
        using ReturnType = Vector<typename IDLType::ImplementationType>;
    };

    template<typename T>
    struct ArrayConverterBase<IDLInterface<T>> {
        using ReturnType = Vector<RefPtr<T>>;
    };

    template<typename IDLType>
    struct ArrayConverter : ArrayConverterBase<IDLType> {
        using ReturnType = typename ArrayConverterBase<IDLType>::ReturnType;

        static ReturnType convert(JSC::ExecState& state, JSC::JSValue value)
        {
            auto& vm = state.vm();
            auto scope = DECLARE_THROW_SCOPE(vm);

            if (!value.isObject()) {
                throwSequenceTypeError(state, scope);
                return { };
            }

            ReturnType result;
            forEachInIterable(&state, value, [&result](JSC::VM& vm, JSC::ExecState* state, JSC::JSValue jsValue) {
                auto scope = DECLARE_THROW_SCOPE(vm);

                auto convertedValue = Converter<IDLType>::convert(*state, jsValue);
                if (UNLIKELY(scope.exception()))
                    return;
                result.append(WTFMove(convertedValue));
            });
            return result;
        }
    };
}

template<typename T> struct Converter<IDLSequence<T>> : DefaultConverter<IDLSequence<T>> {
    using ReturnType = typename Detail::ArrayConverter<T>::ReturnType;

    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value)
    {
        return Detail::ArrayConverter<T>::convert(state, value);
    }
};

template<typename T> struct JSConverter<IDLSequence<T>> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = true;

    template<typename U, size_t inlineCapacity>
    static JSC::JSValue convert(JSC::ExecState& exec, JSDOMGlobalObject& globalObject, const Vector<U, inlineCapacity>& vector)
    {
        JSC::MarkedArgumentBuffer list;
        for (auto& element : vector)
            list.append(toJS<T>(exec, globalObject, element));
        return JSC::constructArray(&exec, nullptr, &globalObject, list);
    }
};

template<typename T> struct Converter<IDLFrozenArray<T>> : DefaultConverter<IDLFrozenArray<T>> {
    using ReturnType = typename Detail::ArrayConverter<T>::ReturnType;

    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value)
    {
        return Detail::ArrayConverter<T>::convert(state, value);
    }
};

template<typename T> struct JSConverter<IDLFrozenArray<T>> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = true;

    template<typename U, size_t inlineCapacity>
    static JSC::JSValue convert(JSC::ExecState& exec, JSDOMGlobalObject& globalObject, const Vector<U, inlineCapacity>& vector)
    {
        JSC::MarkedArgumentBuffer list;
        for (auto& element : vector)
            list.append(toJS<T>(exec, globalObject, element));
        auto* array = JSC::constructArray(&exec, nullptr, &globalObject, list);
        return JSC::objectConstructorFreeze(&exec, array);
    }
};

// MARK: -
// MARK: Record type

namespace Detail {
    template<typename IDLStringType>
    struct IdentifierConverter;

    template<> struct IdentifierConverter<IDLDOMString> {
        static String convert(JSC::ExecState&, const JSC::Identifier& identifier)
        {
            return identifier.string();
        }
    };

    template<> struct IdentifierConverter<IDLByteString> {
        static String convert(JSC::ExecState& state, const JSC::Identifier& identifier)
        {
            return identifierToByteString(state, identifier);
        }
    };

    template<> struct IdentifierConverter<IDLUSVString> {
        static String convert(JSC::ExecState& state, const JSC::Identifier& identifier)
        {
            return identifierToUSVString(state, identifier);
        }
    };
}

template<typename K, typename V> struct Converter<IDLRecord<K, V>> : DefaultConverter<IDLRecord<K, V>> {
    using ReturnType = typename IDLRecord<K, V>::ImplementationType;
    using KeyType = typename K::ImplementationType;
    using ValueType = typename V::ImplementationType;

    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value)
    {
        auto& vm = state.vm();
        auto scope = DECLARE_THROW_SCOPE(vm);

        // 1. Let result be a new empty instance of record<K, V>.
        // 2. If Type(O) is Undefined or Null, return result.
        if (value.isUndefinedOrNull())
            return { };
        
        // 3. If Type(O) is not Object, throw a TypeError.
        if (!value.isObject()) {
            throwTypeError(&state, scope);
            return { };
        }
        
        JSC::JSObject* object = JSC::asObject(value);
    
        ReturnType result;
    
        // 4. Let keys be ? O.[[OwnPropertyKeys]]().
        JSC::PropertyNameArray keys(&vm, JSC::PropertyNameMode::Strings);
        object->getOwnPropertyNames(object, &state, keys, JSC::EnumerationMode());
        RETURN_IF_EXCEPTION(scope, { });

        // 5. Repeat, for each element key of keys in List order:
        for (auto& key : keys) {
            // 1. Let desc be ? O.[[GetOwnProperty]](key).
            JSC::PropertyDescriptor descriptor;
            bool didGetDescriptor = object->getOwnPropertyDescriptor(&state, key, descriptor);
            RETURN_IF_EXCEPTION(scope, { });

            if (!didGetDescriptor)
                continue;

            // 2. If desc is not undefined and desc.[[Enumerable]] is true:
            
            // FIXME: Do we need to check for enumerable / undefined, or is this handled by the default
            // enumeration mode?

            if (!descriptor.value().isUndefined() && descriptor.enumerable()) {
                // 1. Let typedKey be key converted to an IDL value of type K.
                auto typedKey = Detail::IdentifierConverter<K>::convert(state, key);

                // 2. Let value be ? Get(O, key).
                auto subValue = object->get(&state, key);
                RETURN_IF_EXCEPTION(scope, { });

                // 3. Let typedValue be value converted to an IDL value of type V.
                auto typedValue = Converter<V>::convert(state, subValue);
                RETURN_IF_EXCEPTION(scope, { });
                
                // 4. If typedKey is already a key in result, set its value to typedValue.
                // Note: This can happen when O is a proxy object.
                // 5. Otherwise, append to result a mapping (typedKey, typedValue).
                result.set(typedKey, typedValue);
            }
        }

        // 6. Return result.
        return result;
    }
};

template<typename K, typename V> struct JSConverter<IDLRecord<K, V>> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = true;

    template<typename ValueType>
    static JSC::JSValue convert(JSC::ExecState& state, JSDOMGlobalObject& globalObject, const HashMap<String, ValueType>& map)
    {
        auto& vm = state.vm();
    
        // 1. Let result be ! ObjectCreate(%ObjectPrototype%).
        auto result = constructEmptyObject(&state);
        
        // 2. Repeat, for each mapping (key, value) in D:
        for (const auto& keyValuePair : map) {
            // 1. Let esKey be key converted to an ECMAScript value.
            // Note, this step is not required, as we need the key to be
            // an Identifier, not a JSValue.

            // 2. Let esValue be value converted to an ECMAScript value.
            auto esValue = toJS<V>(state, globalObject, keyValuePair.value);

            // 3. Let created be ! CreateDataProperty(result, esKey, esValue).
            bool created = result->putDirect(vm, JSC::Identifier::fromString(&vm, keyValuePair.key), esValue);

            // 4. Assert: created is true.
            ASSERT_UNUSED(created, created);
        }

        // 3. Return result.
        return result;
    }
};

// MARK: -
// MARK: Dictionary type

template<typename T> struct Converter<IDLDictionary<T>> : DefaultConverter<IDLDictionary<T>> {
    using ReturnType = T;

    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value)
    {
        return convertDictionary<T>(state, value);
    }
};

// MARK: -
// MARK: Enumeration type

template<typename T> struct Converter<IDLEnumeration<T>> : DefaultConverter<IDLEnumeration<T>> {
    static T convert(JSC::ExecState& state, JSC::JSValue value)
    {
        return convertEnumeration<T>(state, value);
    }
};

template<typename T> struct JSConverter<IDLEnumeration<T>> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::ExecState& exec, T value)
    {
        return convertEnumerationToJS(exec, value);
    }
};

// MARK: -
// MARK: Callback function type

template<typename T> struct Converter<IDLCallbackFunction<T>> : DefaultConverter<IDLCallbackFunction<T>> {
    template<typename ExceptionThrower = DefaultExceptionThrower>
    static RefPtr<T> convert(JSC::ExecState& state, JSC::JSValue value, JSDOMGlobalObject& globalObject, ExceptionThrower&& exceptionThrower = ExceptionThrower())
    {
        JSC::VM& vm = state.vm();
        auto scope = DECLARE_THROW_SCOPE(vm);

        if (!value.isFunction()) {
            exceptionThrower(state, scope);
            return nullptr;
        }
        
        return T::create(JSC::asObject(value), &globalObject);
    }
};

template<typename T> struct JSConverter<IDLCallbackFunction<T>> {
    static constexpr bool needsState = false;
    static constexpr bool needsGlobalObject = false;

    template <typename U>
    static JSC::JSValue convert(const U& value)
    {
        return toJS(Detail::getPtrOrRef(value));
    }

    template<typename U>
    static JSC::JSValue convertNewlyCreated(U&& value)
    {
        return toJSNewlyCreated(std::forward<U>(value));
    }
};

// MARK: -
// MARK: Callback interface type

template<typename T> struct Converter<IDLCallbackInterface<T>> : DefaultConverter<IDLCallbackInterface<T>> {
    template<typename ExceptionThrower = DefaultExceptionThrower>
    static RefPtr<T> convert(JSC::ExecState& state, JSC::JSValue value, JSDOMGlobalObject& globalObject, ExceptionThrower&& exceptionThrower = ExceptionThrower())
    {
        JSC::VM& vm = state.vm();
        auto scope = DECLARE_THROW_SCOPE(vm);

        if (!value.isObject()) {
            exceptionThrower(state, scope);
            return nullptr;
        }

        return T::create(JSC::asObject(value), &globalObject);
    }
};

template<typename T> struct JSConverter<IDLCallbackInterface<T>> {
    static constexpr bool needsState = false;
    static constexpr bool needsGlobalObject = false;

    template <typename U>
    static JSC::JSValue convert(const U& value)
    {
        return toJS(Detail::getPtrOrRef(value));
    }

    template<typename U>
    static JSC::JSValue convertNewlyCreated(U&& value)
    {
        return toJSNewlyCreated(std::forward<U>(value));
    }
};

// MARK: -
// MARK: Union type

template<typename ReturnType, typename T, bool enabled>
struct ConditionalConverter;

template<typename ReturnType, typename T>
struct ConditionalConverter<ReturnType, T, true> {
    static std::optional<ReturnType> convert(JSC::ExecState& state, JSC::JSValue value)
    {
        return ReturnType(Converter<T>::convert(state, value));
    }
};

template<typename ReturnType, typename T>
struct ConditionalConverter<ReturnType, T, false> {
    static std::optional<ReturnType> convert(JSC::ExecState&, JSC::JSValue)
    {
        return std::nullopt;
    }
};

namespace Detail {
    template<typename List, bool condition>
    struct ConditionalFront;

    template<typename List>
    struct ConditionalFront<List, true>
    {
        using type = brigand::front<List>;
    };

    template<typename List>
    struct ConditionalFront<List, false>
    {
        using type = void;
    };
}

template<typename List, bool condition>
using ConditionalFront = typename Detail::ConditionalFront<List, condition>::type;

template<typename... T> struct Converter<IDLUnion<T...>> : DefaultConverter<IDLUnion<T...>> {
    using Type = IDLUnion<T...>;
    using TypeList = typename Type::TypeList;
    using ReturnType = typename Type::ImplementationType;

    using NumericTypeList = brigand::filter<TypeList, IsIDLNumber<brigand::_1>>;
    static constexpr size_t numberOfNumericTypes = brigand::size<NumericTypeList>::value;
    static_assert(numberOfNumericTypes == 0 || numberOfNumericTypes == 1, "There can be 0 or 1 numeric types in an IDLUnion.");
    using NumericType = ConditionalFront<NumericTypeList, numberOfNumericTypes != 0>;

    // FIXME: This should also check for IDLEnumeration<T>.
    using StringTypeList = brigand::filter<TypeList, std::is_base_of<IDLString, brigand::_1>>;
    static constexpr size_t numberOfStringTypes = brigand::size<StringTypeList>::value;
    static_assert(numberOfStringTypes == 0 || numberOfStringTypes == 1, "There can be 0 or 1 string types in an IDLUnion.");
    using StringType = ConditionalFront<StringTypeList, numberOfStringTypes != 0>;

    using SequenceTypeList = brigand::filter<TypeList, IsIDLSequence<brigand::_1>>;
    static constexpr size_t numberOfSequenceTypes = brigand::size<SequenceTypeList>::value;
    static_assert(numberOfSequenceTypes == 0 || numberOfSequenceTypes == 1, "There can be 0 or 1 sequence types in an IDLUnion.");
    using SequenceType = ConditionalFront<SequenceTypeList, numberOfSequenceTypes != 0>;

    using FrozenArrayTypeList = brigand::filter<TypeList, IsIDLFrozenArray<brigand::_1>>;
    static constexpr size_t numberOfFrozenArrayTypes = brigand::size<FrozenArrayTypeList>::value;
    static_assert(numberOfFrozenArrayTypes == 0 || numberOfFrozenArrayTypes == 1, "There can be 0 or 1 FrozenArray types in an IDLUnion.");
    using FrozenArrayType = ConditionalFront<FrozenArrayTypeList, numberOfFrozenArrayTypes != 0>;

    using DictionaryTypeList = brigand::filter<TypeList, IsIDLDictionary<brigand::_1>>;
    static constexpr size_t numberOfDictionaryTypes = brigand::size<DictionaryTypeList>::value;
    static_assert(numberOfDictionaryTypes == 0 || numberOfDictionaryTypes == 1, "There can be 0 or 1 dictionary types in an IDLUnion.");
    static constexpr bool hasDictionaryType = numberOfDictionaryTypes != 0;
    using DictionaryType = ConditionalFront<DictionaryTypeList, hasDictionaryType>;

    using RecordTypeList = brigand::filter<TypeList, IsIDLRecord<brigand::_1>>;
    static constexpr size_t numberOfRecordTypes = brigand::size<RecordTypeList>::value;
    static_assert(numberOfRecordTypes == 0 || numberOfRecordTypes == 1, "There can be 0 or 1 record types in an IDLUnion.");
    static constexpr bool hasRecordType = numberOfRecordTypes != 0;
    using RecordType = ConditionalFront<RecordTypeList, hasRecordType>;

    static constexpr bool hasObjectType = (numberOfSequenceTypes + numberOfFrozenArrayTypes + numberOfDictionaryTypes + numberOfRecordTypes) > 0;

    using InterfaceTypeList = brigand::filter<TypeList, IsIDLInterface<brigand::_1>>;

    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value)
    {
        auto scope = DECLARE_THROW_SCOPE(state.vm());

        // 1. If the union type includes a nullable type and V is null or undefined, then return the IDL value null.
        constexpr bool hasNullType = brigand::any<TypeList, std::is_same<IDLNull, brigand::_1>>::value;
        if (hasNullType) {
            if (value.isUndefinedOrNull())
                return std::move<WTF::CheckMoveParameter>(ConditionalConverter<ReturnType, IDLNull, hasNullType>::convert(state, value).value());
        }
        
        // 2. Let types be the flattened member types of the union type.
        // NOTE: Union is expected to be pre-flattented.
        
        // 3. If V is null or undefined then:
        if (hasDictionaryType || hasRecordType) {
            if (value.isUndefinedOrNull()) {
                //     1. If types includes a dictionary type, then return the result of converting V to that dictionary type.
                if (hasDictionaryType)
                    return std::move<WTF::CheckMoveParameter>(ConditionalConverter<ReturnType, DictionaryType, hasDictionaryType>::convert(state, value).value());
                
                //     2. If types includes a record type, then return the result of converting V to that record type.
                if (hasRecordType)
                    return std::move<WTF::CheckMoveParameter>(ConditionalConverter<ReturnType, RecordType, hasRecordType>::convert(state, value).value());
            }
        }

        // 4. If V is a platform object, then:
        //     1. If types includes an interface type that V implements, then return the IDL value that is a reference to the object V.
        //     2. If types includes object, then return the IDL value that is a reference to the object V.
        //         (FIXME: Add support for object and step 4.2)
        if (brigand::any<TypeList, IsIDLInterface<brigand::_1>>::value) {
            std::optional<ReturnType> returnValue;
            brigand::for_each<InterfaceTypeList>([&](auto&& type) {
                if (returnValue)
                    return;
                
                using Type = typename WTF::RemoveCVAndReference<decltype(type)>::type::type;
                using ImplementationType = typename Type::ImplementationType;
                using RawType = typename Type::RawType;
                using WrapperType = typename JSDOMWrapperConverterTraits<RawType>::WrapperClass;

                auto castedValue = WrapperType::toWrapped(value);
                if (!castedValue)
                    return;
                
                returnValue = ReturnType(ImplementationType(castedValue));
            });

            if (returnValue)
                return WTFMove(returnValue.value());
        }
        
        // FIXME: Add support for steps 5 - 10.

        // 11. If V is any kind of object, then:
        if (hasObjectType) {
            if (value.isCell()) {
                JSC::JSCell* cell = value.asCell();
                if (cell->isObject()) {
                    // FIXME: We should be able to optimize the following code by making use
                    // of the fact that we have proved that the value is an object. 
                
                    //     1. If types includes a sequence type, then:
                    //         1. Let method be the result of GetMethod(V, @@iterator).
                    //         2. ReturnIfAbrupt(method).
                    //         3. If method is not undefined, return the result of creating a
                    //            sequence of that type from V and method.        
                    constexpr bool hasSequenceType = numberOfSequenceTypes != 0;
                    if (hasSequenceType) {
                        bool hasIterator = hasIteratorMethod(state, value);
                        RETURN_IF_EXCEPTION(scope, ReturnType());
                        if (hasIterator)
                            return std::move<WTF::CheckMoveParameter>(ConditionalConverter<ReturnType, SequenceType, hasSequenceType>::convert(state, value).value());
                    }

                    //     2. If types includes a frozen array type, then:
                    //         1. Let method be the result of GetMethod(V, @@iterator).
                    //         2. ReturnIfAbrupt(method).
                    //         3. If method is not undefined, return the result of creating a
                    //            frozen array of that type from V and method.
                    constexpr bool hasFrozenArrayType = numberOfFrozenArrayTypes != 0;
                    if (hasFrozenArrayType) {
                        bool hasIterator = hasIteratorMethod(state, value);
                        RETURN_IF_EXCEPTION(scope, ReturnType());
                        if (hasIterator)
                            return std::move<WTF::CheckMoveParameter>(ConditionalConverter<ReturnType, FrozenArrayType, hasFrozenArrayType>::convert(state, value).value());
                    }

                    //     3. If types includes a dictionary type, then return the result of
                    //        converting V to that dictionary type.
                    if (hasDictionaryType)
                        return std::move<WTF::CheckMoveParameter>(ConditionalConverter<ReturnType, DictionaryType, hasDictionaryType>::convert(state, value).value());

                    //     4. If types includes a record type, then return the result of converting V to that record type.
                    if (hasRecordType)
                        return std::move<WTF::CheckMoveParameter>(ConditionalConverter<ReturnType, RecordType, hasRecordType>::convert(state, value).value());

                    //     5. If types includes a callback interface type, then return the result of converting V to that interface type.
                    //         (FIXME: Add support for callback interface type and step 12.5)
                    //     6. If types includes object, then return the IDL value that is a reference to the object V.
                    //         (FIXME: Add support for object and step 12.6)
                }
            }
        }

        // 12. If V is a Boolean value, then:
        //     1. If types includes a boolean, then return the result of converting V to boolean.
        constexpr bool hasBooleanType = brigand::any<TypeList, std::is_same<IDLBoolean, brigand::_1>>::value;
        if (hasBooleanType) {
            if (value.isBoolean())
                return std::move<WTF::CheckMoveParameter>(ConditionalConverter<ReturnType, IDLBoolean, hasBooleanType>::convert(state, value).value());
        }
        
        // 13. If V is a Number value, then:
        //     1. If types includes a numeric type, then return the result of converting V to that numeric type.
        constexpr bool hasNumericType = brigand::size<NumericTypeList>::value != 0;
        if (hasNumericType) {
            if (value.isNumber())
                return std::move<WTF::CheckMoveParameter>(ConditionalConverter<ReturnType, NumericType, hasNumericType>::convert(state, value).value());
        }
        
        // 14. If types includes a string type, then return the result of converting V to that type.
        constexpr bool hasStringType = brigand::size<StringTypeList>::value != 0;
        if (hasStringType)
            return std::move<WTF::CheckMoveParameter>(ConditionalConverter<ReturnType, StringType, hasStringType>::convert(state, value).value());

        // 15. If types includes a numeric type, then return the result of converting V to that numeric type.
        if (hasNumericType)
            return std::move<WTF::CheckMoveParameter>(ConditionalConverter<ReturnType, NumericType, hasNumericType>::convert(state, value).value());

        // 16. If types includes a boolean, then return the result of converting V to boolean.
        if (hasBooleanType)
            return std::move<WTF::CheckMoveParameter>(ConditionalConverter<ReturnType, IDLBoolean, hasBooleanType>::convert(state, value).value());

        // 17. Throw a TypeError.
        throwTypeError(&state, scope);
        return ReturnType();
    }
};

template<typename... T> struct JSConverter<IDLUnion<T...>> {
    using Type = IDLUnion<T...>;
    using TypeList = typename Type::TypeList;
    using ImplementationType = typename Type::ImplementationType;

    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = true;

    using Sequence = brigand::make_sequence<brigand::ptrdiff_t<0>, WTF::variant_size<ImplementationType>::value>;

    static JSC::JSValue convert(JSC::ExecState& state, JSDOMGlobalObject& globalObject, const ImplementationType& variant)
    {
        auto index = variant.index();

        std::optional<JSC::JSValue> returnValue;
        brigand::for_each<Sequence>([&](auto&& type) {
            using I = typename WTF::RemoveCVAndReference<decltype(type)>::type::type;
            if (I::value == index) {
                ASSERT(!returnValue);
                returnValue = toJS<brigand::at<TypeList, I>>(state, globalObject, WTF::get<I::value>(variant));
            }
        });

        ASSERT(returnValue);
        return returnValue.value();
    }
};

// MARK: -
// MARK: Date type

template<> struct Converter<IDLDate> : DefaultConverter<IDLDate> {
    static double convert(JSC::ExecState& state, JSC::JSValue value)
    {
        return valueToDate(&state, value);
    }
};

template<> struct JSConverter<IDLDate> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::ExecState& state, double value)
    {
        return jsDate(&state, value);
    }
};

// MARK: -
// MARK: SerializedScriptValue type

template<typename T> struct Converter<IDLSerializedScriptValue<T>> : DefaultConverter<IDLSerializedScriptValue<T>> {
    static RefPtr<T> convert(JSC::ExecState& state, JSC::JSValue value)
    {
        return T::create(state, value);
    }
};

template<typename T> struct JSConverter<IDLSerializedScriptValue<T>> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = true;

    static JSC::JSValue convert(JSC::ExecState& state, JSDOMGlobalObject& globalObject, RefPtr<T> value)
    {
        return value ? value->deserialize(state, &globalObject) : JSC::jsNull();
    }
};

// MARK: -
// MARK: Legacy dictionary type

template<typename T> struct Converter<IDLLegacyDictionary<T>> : DefaultConverter<IDLLegacyDictionary<T>> {
    using ReturnType = T;

    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value)
    {
        return T(&state, value);
    }
};

// MARK: -
// MARK: Event Listener type

template<typename T> struct Converter<IDLEventListener<T>> : DefaultConverter<IDLEventListener<T>> {
    using ReturnType = RefPtr<T>;

    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value, JSC::JSObject& thisObject)
    {
        auto scope = DECLARE_THROW_SCOPE(state.vm());

        auto listener = T::create(value, thisObject, false, currentWorld(&state));
        if (!listener)
            throwTypeError(&state, scope);
    
        return listener;
    }
};

// MARK: -
// MARK: XPathNSResolver type

template<typename T> struct Converter<IDLXPathNSResolver<T>> : DefaultConverter<IDLXPathNSResolver<T>> {
    using ReturnType = RefPtr<T>;
    using WrapperType = typename JSDOMWrapperConverterTraits<T>::WrapperClass;

    template<typename ExceptionThrower = DefaultExceptionThrower>
    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value, ExceptionThrower&& exceptionThrower = ExceptionThrower())
    {
        JSC::VM& vm = state.vm();
        auto scope = DECLARE_THROW_SCOPE(vm);
        ReturnType object = WrapperType::toWrapped(state, value);
        if (UNLIKELY(!object))
            exceptionThrower(state, scope);
        return object;
    }
};

template<typename T> struct JSConverter<IDLXPathNSResolver<T>> {
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

// MARK: -
// MARK: Support for variadic tail convertions

namespace Detail {
    template<typename IDLType>
    struct VariadicConverterBase;

    template<typename IDLType> 
    struct VariadicConverterBase {
        using Item = typename IDLType::ImplementationType;

        static std::optional<Item> convert(JSC::ExecState& state, JSC::JSValue value)
        {
            auto& vm = state.vm();
            auto scope = DECLARE_THROW_SCOPE(vm);

            auto result = Converter<IDLType>::convert(state, value);
            RETURN_IF_EXCEPTION(scope, std::nullopt);

            return WTFMove(result);
        }
    };

    template<typename T>
    struct VariadicConverterBase<IDLInterface<T>> {
        using Item = std::reference_wrapper<T>;

        static std::optional<Item> convert(JSC::ExecState& state, JSC::JSValue value)
        {
            auto* result = Converter<IDLInterface<T>>::convert(state, value);
            if (!result)
                return std::nullopt;
            return std::optional<Item>(*result);
        }
    };

    template<typename IDLType>
    struct VariadicConverter : VariadicConverterBase<IDLType> {
        using Item = typename VariadicConverterBase<IDLType>::Item;
        using Container = Vector<Item>;

        struct Result {
            size_t argumentIndex;
            std::optional<Container> arguments;
        };
    };
}

template<typename IDLType> typename Detail::VariadicConverter<IDLType>::Result convertVariadicArguments(JSC::ExecState& state, size_t startIndex)
{
    size_t length = state.argumentCount();
    if (startIndex > length)
        return { 0, std::nullopt };

    typename Detail::VariadicConverter<IDLType>::Container result;
    result.reserveInitialCapacity(length - startIndex);

    for (size_t i = startIndex; i < length; ++i) {
        auto value = Detail::VariadicConverter<IDLType>::convert(state, state.uncheckedArgument(i));
        if (!value)
            return { i, std::nullopt };
        result.uncheckedAppend(WTFMove(*value));
    }

    return { length, WTFMove(result) };
}

} // namespace WebCore
