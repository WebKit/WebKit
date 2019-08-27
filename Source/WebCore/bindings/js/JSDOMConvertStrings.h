/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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
#include "StringAdaptors.h"

namespace WebCore {

WEBCORE_EXPORT String identifierToByteString(JSC::ExecState&, const JSC::Identifier&);
WEBCORE_EXPORT String valueToByteString(JSC::ExecState&, JSC::JSValue);
WEBCORE_EXPORT String identifierToUSVString(JSC::ExecState&, const JSC::Identifier&);
WEBCORE_EXPORT String valueToUSVString(JSC::ExecState&, JSC::JSValue);

inline String propertyNameToString(JSC::PropertyName propertyName)
{
    ASSERT(!propertyName.isSymbol());
    return propertyName.uid() ? propertyName.uid() : propertyName.publicName();
}

inline AtomString propertyNameToAtomString(JSC::PropertyName propertyName)
{
    return AtomString(propertyName.uid() ? propertyName.uid() : propertyName.publicName());
}

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

    static JSC::JSValue convert(JSC::ExecState& state, const UncachedString& value)
    {
        return JSC::jsString(state.vm(), value.string);
    }

    static JSC::JSValue convert(JSC::ExecState& state, const OwnedString& value)
    {
        return JSC::jsOwnedString(state.vm(), value.string);
    }
};

template<> struct Converter<IDLByteString> : DefaultConverter<IDLByteString> {
    static String convert(JSC::ExecState& state, JSC::JSValue value)
    {
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

    static JSC::JSValue convert(JSC::ExecState& state, const UncachedString& value)
    {
        return JSC::jsString(state.vm(), value.string);
    }

    static JSC::JSValue convert(JSC::ExecState& state, const OwnedString& value)
    {
        return JSC::jsOwnedString(state.vm(), value.string);
    }
};

template<> struct Converter<IDLUSVString> : DefaultConverter<IDLUSVString> {
    static String convert(JSC::ExecState& state, JSC::JSValue value)
    {
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

    static JSC::JSValue convert(JSC::ExecState& state, const UncachedString& value)
    {
        return JSC::jsString(state.vm(), value.string);
    }

    static JSC::JSValue convert(JSC::ExecState& state, const OwnedString& value)
    {
        return JSC::jsOwnedString(state.vm(), value.string);
    }
};

// MARK: -
// MARK: String type adaptors

template<typename T> struct Converter<IDLTreatNullAsEmptyAdaptor<T>> : DefaultConverter<IDLTreatNullAsEmptyAdaptor<T>> {
    static String convert(JSC::ExecState& state, JSC::JSValue value)
    {
        if (value.isNull())
            return emptyString();
        return Converter<T>::convert(state, value);
    }
};

template<typename T>  struct JSConverter<IDLTreatNullAsEmptyAdaptor<T>> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::ExecState& state, const String& value)
    {
        return JSConverter<T>::convert(state, value);
    }
};

template<typename T> struct Converter<IDLAtomStringAdaptor<T>> : DefaultConverter<IDLAtomStringAdaptor<T>> {
    static AtomString convert(JSC::ExecState& state, JSC::JSValue value)
    {
        static_assert(std::is_same<T, IDLDOMString>::value, "This adaptor is only supported for IDLDOMString at the moment.");

        return value.toString(&state)->toAtomString(&state);
    }
};

template<typename T>  struct JSConverter<IDLAtomStringAdaptor<T>> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::ExecState& state, const AtomString& value)
    {
        static_assert(std::is_same<T, IDLDOMString>::value, "This adaptor is only supported for IDLDOMString at the moment.");

        return JSConverter<T>::convert(state, value);
    }
};

template<typename T> struct Converter<IDLRequiresExistingAtomStringAdaptor<T>> : DefaultConverter<IDLRequiresExistingAtomStringAdaptor<T>> {
    static AtomString convert(JSC::ExecState& state, JSC::JSValue value)
    {
        static_assert(std::is_same<T, IDLDOMString>::value, "This adaptor is only supported for IDLDOMString at the moment.");
    
        return AtomString(value.toString(&state)->toExistingAtomString(&state));
    }
};

template<typename T>  struct JSConverter<IDLRequiresExistingAtomStringAdaptor<T>> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::ExecState& state, const AtomString& value)
    {
        static_assert(std::is_same<T, IDLDOMString>::value, "This adaptor is only supported for IDLDOMString at the moment.");

        return JSConverter<T>::convert(state, value);
    }
};


} // namespace WebCore
