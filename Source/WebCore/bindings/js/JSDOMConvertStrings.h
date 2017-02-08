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

namespace WebCore {

enum class StringConversionConfiguration { Normal, TreatNullAsEmptyString };

template<typename T> typename Converter<T>::ReturnType convert(JSC::ExecState&, JSC::JSValue, StringConversionConfiguration);

template<typename T> inline typename Converter<T>::ReturnType convert(JSC::ExecState& state, JSC::JSValue value, StringConversionConfiguration configuration)
{
    return Converter<T>::convert(state, value, configuration);
}

WEBCORE_EXPORT String identifierToByteString(JSC::ExecState&, const JSC::Identifier&);
WEBCORE_EXPORT String valueToByteString(JSC::ExecState&, JSC::JSValue);
WEBCORE_EXPORT String identifierToUSVString(JSC::ExecState&, const JSC::Identifier&);
WEBCORE_EXPORT String valueToUSVString(JSC::ExecState&, JSC::JSValue);

inline String propertyNameToString(JSC::PropertyName propertyName)
{
    ASSERT(!propertyName.isSymbol());
    return propertyName.uid() ? propertyName.uid() : propertyName.publicName();
}

inline AtomicString propertyNameToAtomicString(JSC::PropertyName propertyName)
{
    return AtomicString(propertyName.uid() ? propertyName.uid() : propertyName.publicName());
}

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

} // namespace WebCore
