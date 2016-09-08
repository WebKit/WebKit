/*

Copyright (C) 2016 Apple Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1.  Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
2.  Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#pragma once

#include "JSDOMBinding.h"

namespace WebCore {

enum class ShouldAllowNonFinite { No, Yes };

template<typename T, typename U = T> using EnableIfIntegralType = typename std::enable_if<std::is_integral<T>::value, U>::type;
template<typename T, typename U = T> using EnableIfFloatingPointType = typename std::enable_if<std::is_floating_point<T>::value, U>::type;

template<typename T, typename Enable = void> struct Converter;

template<typename T> T convert(JSC::ExecState&, JSC::JSValue);
template<typename T> EnableIfIntegralType<T> convert(JSC::ExecState&, JSC::JSValue, IntegerConversionConfiguration);
template<typename T> EnableIfFloatingPointType<T> convert(JSC::ExecState&, JSC::JSValue, ShouldAllowNonFinite);

template<typename T> typename Converter<T>::OptionalValue convertOptional(JSC::ExecState&, JSC::JSValue);
template<typename T, typename U> T convertOptional(JSC::ExecState&, JSC::JSValue, U&& defaultValue);

template<typename T> EnableIfIntegralType<T, Optional<T>> convertOptional(JSC::ExecState&, JSC::JSValue, IntegerConversionConfiguration);
template<typename T> EnableIfFloatingPointType<T, Optional<T>> convertOptional(JSC::ExecState&, JSC::JSValue, ShouldAllowNonFinite);
template<typename T, typename U> EnableIfIntegralType<T> convertOptional(JSC::ExecState&, JSC::JSValue, IntegerConversionConfiguration, U&& defaultValue);
template<typename T, typename U> EnableIfFloatingPointType<T> convertOptional(JSC::ExecState&, JSC::JSValue, ShouldAllowNonFinite, U&& defaultValue);

template<typename T> Optional<T> convertDictionary(JSC::ExecState&, JSC::JSValue);

enum class IsNullable { No, Yes };
template<typename T, typename JST> T* convertWrapperType(JSC::ExecState&, JSC::JSValue, IsNullable);

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

template<typename T> inline typename Converter<T>::OptionalValue convertOptional(JSC::ExecState& state, JSC::JSValue value)
{
    return value.isUndefined() ? typename Converter<T>::OptionalValue() : convert<T>(state, value);
}

template<typename T, typename U> inline T convertOptional(JSC::ExecState& state, JSC::JSValue value, U&& defaultValue)
{
    return value.isUndefined() ? std::forward<U>(defaultValue) : convert<T>(state, value);
}

template<typename T> inline EnableIfFloatingPointType<T, Optional<T>> convertOptional(JSC::ExecState& state, JSC::JSValue value, ShouldAllowNonFinite allow)
{
    return value.isUndefined() ? Optional<T>() : convert<T>(state, value, allow);
}

template<typename T, typename U> inline EnableIfFloatingPointType<T> convertOptional(JSC::ExecState& state, JSC::JSValue value, ShouldAllowNonFinite allow, U&& defaultValue)
{
    return value.isUndefined() ? std::forward<U>(defaultValue) : convert<T>(state, value, allow);
}

template<typename T> inline EnableIfIntegralType<T, Optional<T>> convertOptional(JSC::ExecState& state, JSC::JSValue value, IntegerConversionConfiguration configuration)
{
    return value.isUndefined() ? Optional<T>() : convert<T>(state, value, configuration);
}

template<typename T, typename U> inline EnableIfIntegralType<T> convertOptional(JSC::ExecState& state, JSC::JSValue value, IntegerConversionConfiguration configuration, U&& defaultValue)
{
    return value.isUndefined() ? std::forward<U>(defaultValue) : convert<T>(state, value, configuration);
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

} // namespace WebCore
