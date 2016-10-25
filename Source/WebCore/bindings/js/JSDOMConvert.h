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

template<typename T, typename U = T> using EnableIfIntegralType = typename std::enable_if<IsIDLInteger<T>::value, typename Converter<U>::ReturnType>::type;
template<typename T, typename U = T> using EnableIfNotIntegralType = typename std::enable_if<!IsIDLInteger<T>::value, typename Converter<U>::ReturnType>::type;
template<typename T> EnableIfNotIntegralType<T> convert(JSC::ExecState&, JSC::JSValue);
template<typename T> EnableIfIntegralType<T> convert(JSC::ExecState&, JSC::JSValue, IntegerConversionConfiguration = NormalConversion);

// Specialized by generated code for IDL dictionary conversion.
template<typename T> T convertDictionary(JSC::ExecState&, JSC::JSValue);

// Specialized by generated code for IDL enumeration conversion.
template<typename T> Optional<T> parseEnumeration(JSC::ExecState&, JSC::JSValue);
template<typename T> T convertEnumeration(JSC::ExecState&, JSC::JSValue);
template<typename T> const char* expectedEnumerationValues();

template<typename T> inline EnableIfNotIntegralType<T> convert(JSC::ExecState& state, JSC::JSValue value)
{
    return Converter<T>::convert(state, value);
}

template<typename T> inline EnableIfIntegralType<T> convert(JSC::ExecState& state, JSC::JSValue value, IntegerConversionConfiguration configuration)
{
    return Converter<T>::convert(state, value, configuration);
}


// Conversion from Implementation -> JSValue
template<typename T> struct JSConverter;

template<typename T, typename U> inline JSC::JSValue toJS(U&&);
template<typename T, typename U> inline JSC::JSValue toJS(JSC::ExecState&, U&&);
template<typename T, typename U> inline JSC::JSValue toJS(JSC::ExecState&, JSDOMGlobalObject&, U&&);
template<typename T, typename U> inline JSC::JSValue toJS(JSC::ExecState&, JSC::ThrowScope&, ExceptionOr<U>&&);
template<typename T, typename U> inline JSC::JSValue toJS(JSC::ExecState&, JSDOMGlobalObject&, JSC::ThrowScope&, ExceptionOr<U>&&);

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


template<typename T> struct DefaultConverter {
    using ReturnType = typename T::ImplementationType;
};

// MARK: -
// MARK: Nullable type

template<typename T> struct Converter<IDLNullable<T>> : DefaultConverter<IDLNullable<T>> {
    using ReturnType = typename IDLNullable<T>::ImplementationType;
    
    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value)
    {
        // 1. If Type(V) is not Object, and the conversion to an IDL value is being performed
        // due to V being assigned to an attribute whose type is a nullable callback function
        // that is annotated with [TreatNonObjectAsNull], then return the IDL nullable type T?
        // value null.
        //
        // NOTE: Handled elsewhere.

        // 2. Otherwise, if V is null or undefined, then return the IDL nullable type T? value null.
        if (value.isUndefinedOrNull())
            return T::nullValue();

        // 3. Otherwise, return the result of converting V using the rules for the inner IDL type T.
        return Converter<T>::convert(state, value);
    }
};

template<typename T> struct JSConverter<IDLNullable<T>> {
    using ImplementationType = typename IDLNullable<T>::ImplementationType;

    static constexpr bool needsState = JSConverter<T>::needsState;
    static constexpr bool needsGlobalObject = JSConverter<T>::needsGlobalObject;

    static JSC::JSValue convert(const ImplementationType& value)
    {
        if (T::isNullValue(value))
            return JSC::jsNull();
        return JSConverter<T>::convert(T::extractValueFromNullable(value));
    }
    static JSC::JSValue convert(JSC::ExecState& state, const ImplementationType& value)
    {
        if (T::isNullValue(value))
            return JSC::jsNull();
        return JSConverter<T>::convert(state, T::extractValueFromNullable(value));
    }
    static JSC::JSValue convert(JSC::ExecState& state, JSDOMGlobalObject& globalObject, const ImplementationType& value)
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

    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value)
    {
        JSC::VM& vm = state.vm();
        auto scope = DECLARE_THROW_SCOPE(vm);
        ReturnType object = WrapperType::toWrapped(value);
        if (!object)
            throwTypeError(&state, scope);
        return object;
    }
};

template<typename T> struct JSConverter<IDLInterface<T>> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = true;

    template<typename U>
    static JSC::JSValue convert(JSC::ExecState& exec, JSDOMGlobalObject& globalObject, const U& value)
    {
        return toJS(&exec, &globalObject, WTF::getPtr(value));
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
    static int8_t convert(JSC::ExecState& state, JSC::JSValue value, IntegerConversionConfiguration configuration = NormalConversion)
    {
        switch (configuration) {
        case NormalConversion:
            break;
        case EnforceRange:
            return toInt8EnforceRange(state, value);
        case Clamp:
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
    static uint8_t convert(JSC::ExecState& state, JSC::JSValue value, IntegerConversionConfiguration configuration = NormalConversion)
    {
        switch (configuration) {
        case NormalConversion:
            break;
        case EnforceRange:
            return toUInt8EnforceRange(state, value);
        case Clamp:
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
    static int16_t convert(JSC::ExecState& state, JSC::JSValue value, IntegerConversionConfiguration configuration = NormalConversion)
    {
        switch (configuration) {
        case NormalConversion:
            break;
        case EnforceRange:
            return toInt16EnforceRange(state, value);
        case Clamp:
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
    static uint16_t convert(JSC::ExecState& state, JSC::JSValue value, IntegerConversionConfiguration configuration = NormalConversion)
    {
        switch (configuration) {
        case NormalConversion:
            break;
        case EnforceRange:
            return toUInt16EnforceRange(state, value);
        case Clamp:
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
    static int32_t convert(JSC::ExecState& state, JSC::JSValue value, IntegerConversionConfiguration configuration = NormalConversion)
    {
        switch (configuration) {
        case NormalConversion:
            break;
        case EnforceRange:
            return toInt32EnforceRange(state, value);
        case Clamp:
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
    static uint32_t convert(JSC::ExecState& state, JSC::JSValue value, IntegerConversionConfiguration configuration = NormalConversion)
    {
        switch (configuration) {
        case NormalConversion:
            break;
        case EnforceRange:
            return toUInt32EnforceRange(state, value);
        case Clamp:
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
    static int64_t convert(JSC::ExecState& state, JSC::JSValue value, IntegerConversionConfiguration configuration = NormalConversion)
    {
        if (value.isInt32())
            return value.asInt32();

        switch (configuration) {
        case NormalConversion:
            break;
        case EnforceRange:
            return toInt64EnforceRange(state, value);
        case Clamp:
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
    static uint64_t convert(JSC::ExecState& state, JSC::JSValue value, IntegerConversionConfiguration configuration = NormalConversion)
    {
        if (value.isUInt32())
            return value.asUInt32();

        switch (configuration) {
        case NormalConversion:
            break;
        case EnforceRange:
            return toUInt64EnforceRange(state, value);
        case Clamp:
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
    static String convert(JSC::ExecState& state, JSC::JSValue value)
    {
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

template<> struct Converter<IDLUSVString> : DefaultConverter<IDLUSVString> {
    static String convert(JSC::ExecState& state, JSC::JSValue value)
    {
        return valueToUSVString(&state, value);
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
// MARK: Union type

template<typename ReturnType, typename T, bool enabled>
struct ConditionalConverter;

template<typename ReturnType, typename T>
struct ConditionalConverter<ReturnType, T, true> {
    static Optional<ReturnType> convert(JSC::ExecState& state, JSC::JSValue value)
    {
        return ReturnType(Converter<T>::convert(state, value));
    }
};

template<typename ReturnType, typename T>
struct ConditionalConverter<ReturnType, T, false> {
    static Optional<ReturnType> convert(JSC::ExecState&, JSC::JSValue)
    {
        return Nullopt;
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
    using DictionaryType = ConditionalFront<DictionaryTypeList, numberOfDictionaryTypes != 0>;

    static constexpr bool hasObjectType = (numberOfSequenceTypes + numberOfFrozenArrayTypes + numberOfDictionaryTypes) > 0;

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
        
        // 3. If V is null or undefined, and types includes a dictionary type, then return the result of converting V to that dictionary type.
        constexpr bool hasDictionaryType = numberOfDictionaryTypes != 0;
        if (hasDictionaryType) {
            if (value.isUndefinedOrNull())
                return std::move<WTF::CheckMoveParameter>(ConditionalConverter<ReturnType, DictionaryType, hasDictionaryType>::convert(state, value).value());
        }

        // 4. If V is a platform object, then:
        //     1. If types includes an interface type that V implements, then return the IDL value that is a reference to the object V.
        //     2. If types includes object, then return the IDL value that is a reference to the object V.
        //         (FIXME: Add support for object and step 4.2)
        if (brigand::any<TypeList, IsIDLInterface<brigand::_1>>::value) {
            Optional<ReturnType> returnValue;
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
        
        // FIXME: Add support for steps 5 - 11.

        // 12. If V is any kind of object except for a native RegExp object, then:
        if (hasObjectType) {
            if (value.isCell()) {
                JSC::JSCell* cell = value.asCell();
                if (cell->isObject() && cell->type() != JSC::RegExpObjectType) {
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
                    //         (FIXME: Add support for record types and step 12.4)
                    //     5. If types includes a callback interface type, then return the result of converting V to that interface type.
                    //         (FIXME: Add support for callback interface type and step 12.5)
                    //     6. If types includes object, then return the IDL value that is a reference to the object V.
                    //         (FIXME: Add support for object and step 12.6)
                }
            }
        }

        // 13. If V is a Boolean value, then:
        //     1. If types includes a boolean, then return the result of converting V to boolean.
        constexpr bool hasBooleanType = brigand::any<TypeList, std::is_same<IDLBoolean, brigand::_1>>::value;
        if (hasBooleanType) {
            if (value.isBoolean())
                return std::move<WTF::CheckMoveParameter>(ConditionalConverter<ReturnType, IDLBoolean, hasBooleanType>::convert(state, value).value());
        }
        
        // 14. If V is a Number value, then:
        //     1. If types includes a numeric type, then return the result of converting V to that numeric type.
        constexpr bool hasNumericType = brigand::size<NumericTypeList>::value != 0;
        if (hasNumericType) {
            if (value.isNumber())
                return std::move<WTF::CheckMoveParameter>(ConditionalConverter<ReturnType, NumericType, hasNumericType>::convert(state, value).value());
        }
        
        // 15. If types includes a string type, then return the result of converting V to that type.
        constexpr bool hasStringType = brigand::size<StringTypeList>::value != 0;
        if (hasStringType)
            return std::move<WTF::CheckMoveParameter>(ConditionalConverter<ReturnType, StringType, hasStringType>::convert(state, value).value());

        // 16. If types includes a numeric type, then return the result of converting V to that numeric type.
        if (hasNumericType)
            return std::move<WTF::CheckMoveParameter>(ConditionalConverter<ReturnType, NumericType, hasNumericType>::convert(state, value).value());

        // 17. If types includes a boolean, then return the result of converting V to boolean.
        if (hasBooleanType)
            return std::move<WTF::CheckMoveParameter>(ConditionalConverter<ReturnType, IDLBoolean, hasBooleanType>::convert(state, value).value());

        // 18. Throw a TypeError.
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

        Optional<JSC::JSValue> returnValue;
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
// MARK: BufferSource type

template<> struct Converter<IDLBufferSource> : DefaultConverter<IDLBufferSource> {
    using ReturnType = BufferSource;

    static ReturnType convert(JSC::ExecState& state, JSC::JSValue value)
    {
        JSC::VM& vm = state.vm();
        auto scope = DECLARE_THROW_SCOPE(vm);

        if (JSC::ArrayBuffer* buffer = JSC::toArrayBuffer(value))
            return { static_cast<uint8_t*>(buffer->data()), buffer->byteLength() };
        if (RefPtr<JSC::ArrayBufferView> bufferView = toArrayBufferView(value))
            return { static_cast<uint8_t*>(bufferView->baseAddress()), bufferView->byteLength() };

        throwTypeError(&state, scope, ASCIILiteral("Only ArrayBuffer and ArrayBufferView objects can be passed as BufferSource arguments"));
        return { nullptr, 0 };
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
// MARK: Support for variadic tail convertions

namespace Detail {
    template<typename IDLType>
    struct VariadicConverterBase;

    template<typename IDLType> 
    struct VariadicConverterBase {
        using Item = typename IDLType::ImplementationType;

        static Optional<Item> convert(JSC::ExecState& state, JSC::JSValue value)
        {
            auto& vm = state.vm();
            auto scope = DECLARE_THROW_SCOPE(vm);

            auto result = Converter<IDLType>::convert(state, value);
            RETURN_IF_EXCEPTION(scope, Nullopt);

            return WTFMove(result);
        }
    };

    template<typename T>
    struct VariadicConverterBase<IDLInterface<T>> {
        using Item = std::reference_wrapper<T>;

        static Optional<Item> convert(JSC::ExecState& state, JSC::JSValue value)
        {
            auto* result = Converter<IDLInterface<T>>::convert(state, value);
            if (!result)
                return Nullopt;
            return Optional<Item>(*result);
        }
    };

    template<typename IDLType>
    struct VariadicConverter : VariadicConverterBase<IDLType> {
        using Item = typename VariadicConverterBase<IDLType>::Item;
        using Container = Vector<Item>;

        struct Result {
            size_t argumentIndex;
            Optional<Container> arguments;
        };
    };
}

template<typename IDLType> typename Detail::VariadicConverter<IDLType>::Result convertVariadicArguments(JSC::ExecState& state, size_t startIndex)
{
    size_t length = state.argumentCount();
    if (startIndex > length)
        return { 0, Nullopt };

    typename Detail::VariadicConverter<IDLType>::Container result;
    result.reserveInitialCapacity(length - startIndex);

    for (size_t i = startIndex; i < length; ++i) {
        auto value = Detail::VariadicConverter<IDLType>::convert(state, state.uncheckedArgument(i));
        if (!value)
            return { i, Nullopt };
        result.uncheckedAppend(WTFMove(*value));
    }

    return { length, WTFMove(result) };
}

} // namespace WebCore
