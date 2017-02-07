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
#include "JSDOMExceptionHandling.h"
#include <runtime/JSCJSValueInlines.h>

namespace WebCore {

enum class IntegerConversionConfiguration { Normal, EnforceRange, Clamp };

template<typename T> typename Converter<T>::ReturnType convert(JSC::ExecState&, JSC::JSValue, IntegerConversionConfiguration);

template<typename T> inline typename Converter<T>::ReturnType convert(JSC::ExecState& state, JSC::JSValue value, IntegerConversionConfiguration configuration)
{
    return Converter<T>::convert(state, value, configuration);
}

// The following functions convert values to integers as per the WebIDL specification.
// The conversion fails if the value cannot be converted to a number or, if EnforceRange is specified,
// the value is outside the range of the destination integer type.

WEBCORE_EXPORT int8_t toInt8EnforceRange(JSC::ExecState&, JSC::JSValue);
WEBCORE_EXPORT uint8_t toUInt8EnforceRange(JSC::ExecState&, JSC::JSValue);
WEBCORE_EXPORT int16_t toInt16EnforceRange(JSC::ExecState&, JSC::JSValue);
WEBCORE_EXPORT uint16_t toUInt16EnforceRange(JSC::ExecState&, JSC::JSValue);
WEBCORE_EXPORT int32_t toInt32EnforceRange(JSC::ExecState&, JSC::JSValue);
WEBCORE_EXPORT uint32_t toUInt32EnforceRange(JSC::ExecState&, JSC::JSValue);
WEBCORE_EXPORT int64_t toInt64EnforceRange(JSC::ExecState&, JSC::JSValue);
WEBCORE_EXPORT uint64_t toUInt64EnforceRange(JSC::ExecState&, JSC::JSValue);

WEBCORE_EXPORT int8_t toInt8Clamp(JSC::ExecState&, JSC::JSValue);
WEBCORE_EXPORT uint8_t toUInt8Clamp(JSC::ExecState&, JSC::JSValue);
WEBCORE_EXPORT int16_t toInt16Clamp(JSC::ExecState&, JSC::JSValue);
WEBCORE_EXPORT uint16_t toUInt16Clamp(JSC::ExecState&, JSC::JSValue);
WEBCORE_EXPORT int32_t toInt32Clamp(JSC::ExecState&, JSC::JSValue);
WEBCORE_EXPORT uint32_t toUInt32Clamp(JSC::ExecState&, JSC::JSValue);
WEBCORE_EXPORT int64_t toInt64Clamp(JSC::ExecState&, JSC::JSValue);
WEBCORE_EXPORT uint64_t toUInt64Clamp(JSC::ExecState&, JSC::JSValue);

WEBCORE_EXPORT int8_t toInt8(JSC::ExecState&, JSC::JSValue);
WEBCORE_EXPORT uint8_t toUInt8(JSC::ExecState&, JSC::JSValue);
WEBCORE_EXPORT int16_t toInt16(JSC::ExecState&, JSC::JSValue);
WEBCORE_EXPORT uint16_t toUInt16(JSC::ExecState&, JSC::JSValue);
WEBCORE_EXPORT int64_t toInt64(JSC::ExecState&, JSC::JSValue);
WEBCORE_EXPORT uint64_t toUInt64(JSC::ExecState&, JSC::JSValue);


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
    static inline int32_t convert(JSC::ExecState&, JSC::ThrowScope&, double number)
    {
        return JSC::toInt32(number);
    }

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

    static inline float convert(JSC::ExecState& state, JSC::ThrowScope& scope, double number)
    {
        if (UNLIKELY(!std::isfinite(number)))
            throwNonFiniteTypeError(state, scope);
        return static_cast<float>(number);
    }

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
    static inline float convert(JSC::ExecState&, JSC::ThrowScope&, double number)
    {
        return static_cast<float>(number);
    }

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
    static inline double convert(JSC::ExecState& state, JSC::ThrowScope& scope, double number)
    {
        if (UNLIKELY(!std::isfinite(number)))
            throwNonFiniteTypeError(state, scope);
        return number;
    }

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
    static inline double convert(JSC::ExecState&, JSC::ThrowScope&, double number)
    {
        return number;
    }

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

} // namespace WebCore
