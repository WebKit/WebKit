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

template<typename T> T convert(JSC::ExecState&, JSC::JSValue);
template<typename T> T convert(JSC::ExecState&, JSC::JSValue, ShouldAllowNonFinite);
template<typename T> T convert(JSC::ExecState&, JSC::JSValue, IntegerConversionConfiguration);

template<typename T> Optional<T> convertOptional(JSC::ExecState&, JSC::JSValue);
template<typename T, typename U> T convertOptional(JSC::ExecState&, JSC::JSValue, U&& defaultValue);

template<typename T> Optional<T> inline convertOptional(JSC::ExecState& state, JSC::JSValue value)
{
    return value.isUndefined() ? Optional<T>() : convert<T>(state, value);
}

template<typename T, typename U> inline T convertOptional(JSC::ExecState& state, JSC::JSValue value, U&& defaultValue)
{
    return value.isUndefined() ? std::forward<U>(defaultValue) : convert<T>(state, value);
}

template<> inline bool convert<bool>(JSC::ExecState& state, JSC::JSValue value)
{
    return value.toBoolean(&state);
}

template<> inline String convert<String>(JSC::ExecState& state, JSC::JSValue value)
{
    return value.toWTFString(&state);
}

template<typename T> inline T convert(JSC::ExecState& state, JSC::JSValue value, ShouldAllowNonFinite allowNonFinite)
{
    static_assert(std::is_same<T, float>::value || std::is_same<T, double>::value, "Should be called with converting to float or double");
    double number = value.toNumber(&state);
    if (allowNonFinite == ShouldAllowNonFinite::No && UNLIKELY(!std::isfinite(number)))
        throwNonFiniteTypeError(state);
    return static_cast<T>(number);
}

template<> inline Vector<String> convert<Vector<String>>(JSC::ExecState& state, JSC::JSValue value)
{
    // FIXME: The toNativeArray function doesn't throw a type error if the value is not an object. Is that really OK?
    return toNativeArray<String>(&state, value);
}

template<> inline int8_t convert(JSC::ExecState& state, JSC::JSValue value, IntegerConversionConfiguration configuration)
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

template<> inline uint8_t convert(JSC::ExecState& state, JSC::JSValue value, IntegerConversionConfiguration configuration)
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

template<> inline int16_t convert(JSC::ExecState& state, JSC::JSValue value, IntegerConversionConfiguration configuration)
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

template<> inline uint16_t convert(JSC::ExecState& state, JSC::JSValue value, IntegerConversionConfiguration configuration)
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

template<> inline int32_t convert(JSC::ExecState& state, JSC::JSValue value, IntegerConversionConfiguration configuration)
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

template<> inline uint32_t convert(JSC::ExecState& state, JSC::JSValue value, IntegerConversionConfiguration configuration)
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

template<> inline int64_t convert(JSC::ExecState& state, JSC::JSValue value, IntegerConversionConfiguration configuration)
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

template<> inline uint64_t convert(JSC::ExecState& state, JSC::JSValue value, IntegerConversionConfiguration configuration)
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

} // namespace WebCore
