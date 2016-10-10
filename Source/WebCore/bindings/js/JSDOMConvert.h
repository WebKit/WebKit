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
#include "JSDOMBinding.h"

namespace WebCore {

enum class ShouldAllowNonFinite { No, Yes };

template<typename T, typename U = T> using EnableIfIntegralType = typename std::enable_if<std::is_integral<T>::value, U>::type;
template<typename T, typename U = T> using EnableIfFloatingPointType = typename std::enable_if<std::is_floating_point<T>::value, U>::type;

template<typename T, typename Enable = void> struct Converter;

template<typename T> T convert(JSC::ExecState&, JSC::JSValue);
template<typename T> EnableIfIntegralType<T> convert(JSC::ExecState&, JSC::JSValue, IntegerConversionConfiguration);
template<typename T> EnableIfFloatingPointType<T> convert(JSC::ExecState&, JSC::JSValue, ShouldAllowNonFinite);

template<typename T> Optional<T> convertDictionary(JSC::ExecState&, JSC::JSValue);

// Used for IDL enumerations.
template<typename T> Optional<T> parse(JSC::ExecState&, JSC::JSValue);
template<typename T> const char* expectedEnumerationValues();

enum class IsNullable { No, Yes };
template<typename T, typename JST> T* convertWrapperType(JSC::ExecState&, JSC::JSValue, IsNullable);
template<typename T, typename JST, typename VectorType> VectorType convertWrapperTypeSequence(JSC::ExecState&, JSC::JSValue);

// This is where the implementation of the things declared above begins:

template<typename T> T convert(JSC::ExecState& state, JSC::JSValue value)
{
    return Converter<T>::convert(state, value);
}

template<typename T> inline EnableIfIntegralType<T> convert(JSC::ExecState& state, JSC::JSValue value, IntegerConversionConfiguration configuration)
{
    return Converter<T>::convert(state, value, configuration);
}

template<typename T> inline EnableIfFloatingPointType<T> convert(JSC::ExecState& state, JSC::JSValue value, ShouldAllowNonFinite allow)
{
    return Converter<T>::convert(state, value, allow);
}

template<typename T, typename JST> inline T* convertWrapperType(JSC::ExecState& state, JSC::JSValue value, IsNullable isNullable)
{
    JSC::VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    T* object = JST::toWrapped(value);
    if (!object && (isNullable == IsNullable::No || !value.isUndefinedOrNull()))
        throwTypeError(&state, scope);
    return object;
}

template<typename T, typename JST, typename VectorType> inline VectorType convertWrapperTypeSequence(JSC::ExecState& state, JSC::JSValue value)
{
    return toRefPtrNativeArray<T, JST, VectorType>(state, value);
}

template<typename T> struct DefaultConverter {
    using OptionalValue = Optional<T>;
};

template<typename T, typename Enable> struct Converter : DefaultConverter<T> {
};

template<> struct Converter<bool> : DefaultConverter<bool> {
    static bool convert(JSC::ExecState& state, JSC::JSValue value)
    {
        return value.toBoolean(&state);
    }
};

template<> struct Converter<String> : DefaultConverter<String> {
    using OptionalValue = String; // Use null string to mean an optional value was not present.
    static String convert(JSC::ExecState& state, JSC::JSValue value)
    {
        return value.toWTFString(&state);
    }
};

template<> struct Converter<IDLDOMString> : DefaultConverter<String> {
    using OptionalValue = String; // Use null string to mean an optional value was not present.
    static String convert(JSC::ExecState& state, JSC::JSValue value)
    {
        return value.toWTFString(&state);
    }
};

template<> struct Converter<IDLUSVString> : DefaultConverter<String> {
    using OptionalValue = String; // Use null string to mean an optional value was not present.
    static String convert(JSC::ExecState& state, JSC::JSValue value)
    {
        return valueToUSVString(&state, value);
    }
};

template<typename T> struct Converter<IDLInterface<T>> : DefaultConverter<T*> {
    static T* convert(JSC::ExecState& state, JSC::JSValue value)
    {
        return convertWrapperType<T, typename JSDOMWrapperConverterTraits<T>::WrapperClass>(state, value, IsNullable::No);
    }
};

template<> struct Converter<JSC::JSValue> : DefaultConverter<JSC::JSValue> {
    using OptionalValue = JSC::JSValue; // Use jsUndefined() to mean an optional value was not present.
    static JSC::JSValue convert(JSC::ExecState&, JSC::JSValue value)
    {
        return value;
    }
};

template<typename T> struct Converter<Vector<T>> : DefaultConverter<Vector<T>> {
    static Vector<T> convert(JSC::ExecState& state, JSC::JSValue value)
    {
        return toNativeArray<T>(state, value);
    }
};

template<> struct Converter<int8_t> : DefaultConverter<int8_t> {
    static int8_t convert(JSC::ExecState& state, JSC::JSValue value, IntegerConversionConfiguration configuration)
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

template<> struct Converter<uint8_t> : DefaultConverter<uint8_t> {
    static uint8_t convert(JSC::ExecState& state, JSC::JSValue value, IntegerConversionConfiguration configuration)
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

template<> struct Converter<int16_t> : DefaultConverter<int16_t> {
    static int16_t convert(JSC::ExecState& state, JSC::JSValue value, IntegerConversionConfiguration configuration)
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

template<> struct Converter<uint16_t> : DefaultConverter<uint16_t> {
    static uint16_t convert(JSC::ExecState& state, JSC::JSValue value, IntegerConversionConfiguration configuration)
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

template<> struct Converter<int32_t> : DefaultConverter<int32_t> {
    static int32_t convert(JSC::ExecState& state, JSC::JSValue value, IntegerConversionConfiguration configuration)
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

template<> struct Converter<uint32_t> : DefaultConverter<uint32_t> {
    static uint32_t convert(JSC::ExecState& state, JSC::JSValue value, IntegerConversionConfiguration configuration)
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

template<> struct Converter<int64_t> : DefaultConverter<int64_t> {
    static int64_t convert(JSC::ExecState& state, JSC::JSValue value, IntegerConversionConfiguration configuration)
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

template<> struct Converter<uint64_t> : DefaultConverter<uint64_t> {
    static uint64_t convert(JSC::ExecState& state, JSC::JSValue value, IntegerConversionConfiguration configuration)
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

template<typename T> struct Converter<T, typename std::enable_if<std::is_floating_point<T>::value>::type> : DefaultConverter<T> {
    static T convert(JSC::ExecState& state, JSC::JSValue value, ShouldAllowNonFinite allow)
    {
        JSC::VM& vm = state.vm();
        auto scope = DECLARE_THROW_SCOPE(vm);
        double number = value.toNumber(&state);
        if (allow == ShouldAllowNonFinite::No && UNLIKELY(!std::isfinite(number)))
            throwNonFiniteTypeError(state, scope);
        return static_cast<T>(number);
    }
};

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

template<typename... T>
struct Converter<IDLUnion<T...>> : DefaultConverter<typename IDLUnion<T...>::ImplementationType>
{
    using Type = IDLUnion<T...>;
    using TypeList = typename Type::TypeList;
    using ReturnType = typename Type::ImplementationType;

    using DictionaryTypeList = brigand::find<TypeList, IsIDLDictionary<brigand::_1>>;
    using DictionaryType = ConditionalFront<DictionaryTypeList, brigand::size<DictionaryTypeList>::value != 0>;
    static_assert(brigand::size<DictionaryTypeList>::value == 0 || brigand::size<DictionaryTypeList>::value == 1, "There can be 0 or 1 dictionary types in an IDLUnion.");

    using NumericTypeList = brigand::find<TypeList, IsIDLNumber<brigand::_1>>;
    using NumericType = ConditionalFront<NumericTypeList, brigand::size<NumericTypeList>::value != 0>;
    static_assert(brigand::size<NumericTypeList>::value == 0 || brigand::size<NumericTypeList>::value == 1, "There can be 0 or 1 numeric types in an IDLUnion.");

    using StringTypeList = brigand::find<TypeList, std::is_base_of<IDLString, brigand::_1>>;
    using StringType = ConditionalFront<StringTypeList, brigand::size<StringTypeList>::value != 0>;
    static_assert(brigand::size<StringTypeList>::value == 0 || brigand::size<StringTypeList>::value == 1, "There can be 0 or 1 string types in an IDLUnion.");

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
        constexpr bool hasDictionaryType = brigand::size<DictionaryTypeList>::value != 0;
        if (hasDictionaryType) {
            if (value.isUndefinedOrNull())
                return std::move<WTF::CheckMoveParameter>(ConditionalConverter<ReturnType, DictionaryType, hasDictionaryType>::convert(state, value).value());
        }

        // 4. If V is a platform object, then:
        //     1. If types includes an interface type that V implements, then return the IDL value that is a reference to the object V.
        //     2. If types includes object, then return the IDL value that is a reference to the object V.
        //         (FIXME: Add support for object and step 4.2)
        if (brigand::any<TypeList, IsIDLInterface<brigand::_1>>::value) {
            if (isJSDOMWrapperType(value)) {
                Optional<ReturnType> returnValue;
                brigand::for_each<InterfaceTypeList>([&](auto&& type) {
                    if (returnValue)
                        return;
                    
                    using ImplementationType = typename WTF::RemoveCVAndReference<decltype(type)>::type::type::RawType;
                    using WrapperType = typename JSDOMWrapperConverterTraits<ImplementationType>::WrapperClass;

                    auto* castedValue = JSC::jsDynamicCast<WrapperType*>(value);
                    if (!castedValue)
                        return;
                    
                    returnValue = ReturnType(castedValue->wrapped());
                });
                ASSERT(returnValue);

                return WTFMove(returnValue.value());
            }
        }
        
        // FIXME: Add support for steps 5 - 12.
        
        // 13. If V is a Boolean value, then:
        //     1. If types includes a boolean, then return the result of converting V to boolean.
        constexpr bool hasBooleanType = brigand::any<TypeList, std::is_same<IDLBoolean, brigand::_1>>::value;
        if (hasBooleanType) {
            if (value.isBoolean())
                return std::move<WTF::CheckMoveParameter>(ConditionalConverter<ReturnType, bool, hasBooleanType>::convert(state, value).value());
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
            return std::move<WTF::CheckMoveParameter>(ConditionalConverter<ReturnType, bool, hasBooleanType>::convert(state, value).value());

        // 18. Throw a TypeError.
        throwTypeError(&state, scope);
        return ReturnType();
    }
};

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

        return result;
    }
};

template<typename T>
struct VariadicConverterBase<IDLInterface<T>> {
    using Item = typename IDLInterface<T>::ImplementationType;

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

template<typename IDLType> typename VariadicConverter<IDLType>::Result convertVariadicArguments(JSC::ExecState& state, size_t startIndex)
{
    size_t length = state.argumentCount();
    if (startIndex > length)
        return { 0, Nullopt };

    typename VariadicConverter<IDLType>::Container result;
    result.reserveInitialCapacity(length - startIndex);

    for (size_t i = startIndex; i < length; ++i) {
        auto value = VariadicConverter<IDLType>::convert(state, state.uncheckedArgument(i));
        if (!value)
            return { i, Nullopt };
        result.uncheckedAppend(WTFMove(*value));
    }

    return { length, WTFMove(result) };
}

} // namespace WebCore
