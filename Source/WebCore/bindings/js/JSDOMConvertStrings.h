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
#include "TrustedType.h"
#include <JavaScriptCore/JSGlobalObject.h>

namespace WebCore {

class ScriptExecutionContext;

enum class ShouldConvertNullToEmptyString : bool { No, Yes };

WEBCORE_EXPORT String identifierToString(JSC::JSGlobalObject&, const JSC::Identifier&);

WEBCORE_EXPORT String identifierToByteString(JSC::JSGlobalObject&, const JSC::Identifier&);
WEBCORE_EXPORT ConversionResult<IDLByteString> valueToByteString(JSC::JSGlobalObject&, JSC::JSValue);
WEBCORE_EXPORT ConversionResult<IDLAtomStringAdaptor<IDLByteString>> valueToByteAtomString(JSC::JSGlobalObject&, JSC::JSValue);

WEBCORE_EXPORT String identifierToUSVString(JSC::JSGlobalObject&, const JSC::Identifier&);
WEBCORE_EXPORT ConversionResult<IDLUSVString> valueToUSVString(JSC::JSGlobalObject&, JSC::JSValue);
WEBCORE_EXPORT ConversionResult<IDLAtomStringAdaptor<IDLUSVString>> valueToUSVAtomString(JSC::JSGlobalObject&, JSC::JSValue);

ConversionResult<IDLDOMString> trustedTypeCompliantString(TrustedType, JSC::JSGlobalObject&, JSC::JSValue, const String& sink, ShouldConvertNullToEmptyString);

inline AtomString propertyNameToString(JSC::PropertyName propertyName)
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
    using Result = ConversionResult<IDLDOMString>;

    static Result convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value)
    {
        auto& vm = lexicalGlobalObject.vm();
        auto scope = DECLARE_THROW_SCOPE(vm);

        auto string = value.toWTFString(&lexicalGlobalObject);

        RETURN_IF_EXCEPTION(scope, Result::exception());

        return Result { WTFMove(string) };
    }
};

template<> struct JSConverter<IDLDOMString> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const String& value)
    {
        return JSC::jsStringWithCache(JSC::getVM(&lexicalGlobalObject), value);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const UncachedString& value)
    {
        return JSC::jsString(JSC::getVM(&lexicalGlobalObject), value.string);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const OwnedString& value)
    {
        return JSC::jsOwnedString(JSC::getVM(&lexicalGlobalObject), value.string);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const URL& value)
    {
        return JSC::jsOwnedString(JSC::getVM(&lexicalGlobalObject), value.string());
    }
};

template<> struct Converter<IDLByteString> : DefaultConverter<IDLByteString> {
    using Result = ConversionResult<IDLByteString>;

    static Result convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value)
    {
        return valueToByteString(lexicalGlobalObject, value);
    }
};

template<> struct JSConverter<IDLByteString> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const String& value)
    {
        return JSC::jsStringWithCache(JSC::getVM(&lexicalGlobalObject), value);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const UncachedString& value)
    {
        return JSC::jsString(JSC::getVM(&lexicalGlobalObject), value.string);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const OwnedString& value)
    {
        return JSC::jsOwnedString(JSC::getVM(&lexicalGlobalObject), value.string);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const URL& value)
    {
        return JSC::jsOwnedString(JSC::getVM(&lexicalGlobalObject), value.string());
    }
};

template<> struct Converter<IDLUSVString> : DefaultConverter<IDLUSVString> {
    using Result = ConversionResult<IDLUSVString>;

    static Result convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value)
    {
        return valueToUSVString(lexicalGlobalObject, value);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const URL& value)
    {
        return JSC::jsOwnedString(JSC::getVM(&lexicalGlobalObject), value.string());
    }
};

template<> struct JSConverter<IDLUSVString> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const String& value)
    {
        return JSC::jsStringWithCache(JSC::getVM(&lexicalGlobalObject), value);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const UncachedString& value)
    {
        return JSC::jsString(JSC::getVM(&lexicalGlobalObject), value.string);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const OwnedString& value)
    {
        return JSC::jsOwnedString(JSC::getVM(&lexicalGlobalObject), value.string);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const URL& value)
    {
        return JSC::jsOwnedString(JSC::getVM(&lexicalGlobalObject), value.string());
    }
};

// MARK: -
// MARK: String type adaptors

template<typename TrustedStringAdaptorIDL>
struct TrustedStringAdaptorTraits;

template<typename T> struct TrustedStringAdaptorTraits<IDLStringContextTrustedHTMLAdaptor<T>> {
    static constexpr auto trustedType = TrustedType::TrustedHTML;
    static constexpr auto shouldConvertNullToEmptyString = ShouldConvertNullToEmptyString::No;
};

template<typename T> struct TrustedStringAdaptorTraits<IDLLegacyNullToEmptyStringStringContextTrustedHTMLAdaptor<T>> {
    static constexpr auto trustedType = TrustedType::TrustedHTML;
    static constexpr auto shouldConvertNullToEmptyString = ShouldConvertNullToEmptyString::Yes;
};

template<typename T> struct TrustedStringAdaptorTraits<IDLStringContextTrustedScriptAdaptor<T>> {
    static constexpr auto trustedType = TrustedType::TrustedScript;
    static constexpr auto shouldConvertNullToEmptyString = ShouldConvertNullToEmptyString::No;
};

template<typename T> struct TrustedStringAdaptorTraits<IDLLegacyNullToEmptyStringStringContextTrustedScriptAdaptor<T>> {
    static constexpr auto trustedType = TrustedType::TrustedScript;
    static constexpr auto shouldConvertNullToEmptyString = ShouldConvertNullToEmptyString::Yes;
};

template<typename T> struct TrustedStringAdaptorTraits<IDLStringContextTrustedScriptURLAdaptor<T>> {
    static constexpr auto trustedType = TrustedType::TrustedScriptURL;
    static constexpr auto shouldConvertNullToEmptyString = ShouldConvertNullToEmptyString::No;
};

template<typename T> struct TrustedStringAdaptorTraits<IDLLegacyNullToEmptyStringStringContextTrustedScriptURLAdaptor<T>> {
    static constexpr auto trustedType = TrustedType::TrustedScriptURL;
    static constexpr auto shouldConvertNullToEmptyString = ShouldConvertNullToEmptyString::Yes;
};

template<typename T> struct TrustedStringAdaptorTraits<IDLAtomStringStringContextTrustedHTMLAdaptor<T>> {
    static constexpr auto trustedType = TrustedType::TrustedHTML;
    static constexpr auto shouldConvertNullToEmptyString = ShouldConvertNullToEmptyString::No;
};

template<typename T> struct TrustedStringAdaptorTraits<IDLAtomStringStringContextTrustedScriptAdaptor<T>> {
    static constexpr auto trustedType = TrustedType::TrustedScript;
    static constexpr auto shouldConvertNullToEmptyString = ShouldConvertNullToEmptyString::No;
};

template<typename T> struct TrustedStringAdaptorTraits<IDLAtomStringStringContextTrustedScriptURLAdaptor<T>> {
    static constexpr auto trustedType = TrustedType::TrustedScriptURL;
    static constexpr auto shouldConvertNullToEmptyString = ShouldConvertNullToEmptyString::No;
};

template<typename IDL>
struct TrustedStringConverter : DefaultConverter<IDL> {
    using Result = ConversionResult<IDL>;
    using Traits = TrustedStringAdaptorTraits<IDL>;

    static Result convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value)
    {
        return Converter<IDL>::convert(lexicalGlobalObject, value, emptyString());
    }

    static Result convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value, const String& sink)
    {
        return trustedTypeCompliantString(Traits::trustedType, lexicalGlobalObject, value, sink, Traits::shouldConvertNullToEmptyString);
    }
};

template<typename InnerStringType>
struct JSStringAdaptorConverter  {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const String& value)
    {
        return JSConverter<InnerStringType>::convert(lexicalGlobalObject, value);
    }
};

template<typename InnerStringType>
struct JSAtomStringAdaptorConverter  {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const AtomString& value)
    {
        return JSConverter<InnerStringType>::convert(lexicalGlobalObject, value);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const URL& value)
    {
        return JSConverter<InnerStringType>::convert(lexicalGlobalObject, value.string());
    }
};

template<typename T> struct Converter<IDLStringContextTrustedHTMLAdaptor<T>>
    : TrustedStringConverter<IDLStringContextTrustedHTMLAdaptor<T>> { };
template<typename T> struct Converter<IDLLegacyNullToEmptyStringStringContextTrustedHTMLAdaptor<T>>
    : TrustedStringConverter<IDLLegacyNullToEmptyStringStringContextTrustedHTMLAdaptor<T>> { };
template<typename T> struct Converter<IDLStringContextTrustedScriptAdaptor<T>>
    : TrustedStringConverter<IDLStringContextTrustedScriptAdaptor<T>> { };
template<typename T> struct Converter<IDLLegacyNullToEmptyStringStringContextTrustedScriptAdaptor<T>>
    : TrustedStringConverter<IDLLegacyNullToEmptyStringStringContextTrustedScriptAdaptor<T>> { };
template<typename T> struct Converter<IDLStringContextTrustedScriptURLAdaptor<T>>
    : TrustedStringConverter<IDLStringContextTrustedScriptURLAdaptor<T>> { };
template<typename T> struct Converter<IDLLegacyNullToEmptyStringStringContextTrustedScriptURLAdaptor<T>>
    : TrustedStringConverter<IDLLegacyNullToEmptyStringStringContextTrustedScriptURLAdaptor<T>> { };
template<typename T> struct Converter<IDLAtomStringStringContextTrustedHTMLAdaptor<T>>
    : TrustedStringConverter<IDLAtomStringStringContextTrustedHTMLAdaptor<T>> { };
template<typename T> struct Converter<IDLAtomStringStringContextTrustedScriptAdaptor<T>>
    : TrustedStringConverter<IDLAtomStringStringContextTrustedScriptAdaptor<T>> { };
template<typename T> struct Converter<IDLAtomStringStringContextTrustedScriptURLAdaptor<T>>
    : TrustedStringConverter<IDLAtomStringStringContextTrustedScriptURLAdaptor<T>> { };

template<typename T> struct JSConverter<IDLLegacyNullToEmptyStringAdaptor<T>> : JSStringAdaptorConverter<T> { };
template<typename T> struct JSConverter<IDLStringContextTrustedHTMLAdaptor<T>> : JSStringAdaptorConverter<T> { };
template<typename T> struct JSConverter<IDLLegacyNullToEmptyStringStringContextTrustedHTMLAdaptor<T>> : JSStringAdaptorConverter<T> { };
template<typename T> struct JSConverter<IDLStringContextTrustedScriptAdaptor<T>> : JSStringAdaptorConverter<T> { };
template<typename T> struct JSConverter<IDLLegacyNullToEmptyStringStringContextTrustedScriptAdaptor<T>> : JSStringAdaptorConverter<T> { };
template<typename T> struct JSConverter<IDLStringContextTrustedScriptURLAdaptor<T>> : JSStringAdaptorConverter<T> { };
template<typename T> struct JSConverter<IDLLegacyNullToEmptyStringStringContextTrustedScriptURLAdaptor<T>> : JSStringAdaptorConverter<T> { };
template<typename T> struct JSConverter<IDLAtomStringStringContextTrustedHTMLAdaptor<T>> : JSAtomStringAdaptorConverter<T> { };
template<typename T> struct JSConverter<IDLAtomStringStringContextTrustedScriptAdaptor<T>> : JSAtomStringAdaptorConverter<T> { };
template<typename T> struct JSConverter<IDLAtomStringStringContextTrustedScriptURLAdaptor<T>> : JSAtomStringAdaptorConverter<T> { };
template<typename T> struct JSConverter<IDLLegacyNullToEmptyAtomStringAdaptor<T>> : JSAtomStringAdaptorConverter<T> { };

template<typename T> struct Converter<IDLLegacyNullToEmptyStringAdaptor<T>> : DefaultConverter<IDLLegacyNullToEmptyStringAdaptor<T>> {
    using Result = ConversionResult<IDLLegacyNullToEmptyStringAdaptor<T>>;

    static Result convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value)
    {
        if (value.isNull())
            return emptyString();
        return Converter<T>::convert(lexicalGlobalObject, value);
    }
};

template<typename T> struct Converter<IDLLegacyNullToEmptyAtomStringAdaptor<T>> : DefaultConverter<IDLLegacyNullToEmptyAtomStringAdaptor<T>> {
    using Result = ConversionResult<IDLLegacyNullToEmptyAtomStringAdaptor<T>>;

    static Result convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value)
    {
        if (value.isNull())
            return emptyAtom();
        return Converter<IDLAtomStringAdaptor<T>>::convert(lexicalGlobalObject, value);
    }
};

template<typename T> struct Converter<IDLAtomStringAdaptor<T>> : DefaultConverter<IDLAtomStringAdaptor<T>> {
    using Result = ConversionResult<IDLAtomStringAdaptor<T>>;

    static Result convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value)
    {
        static_assert(std::is_same<T, IDLDOMString>::value, "This adaptor is only supported for IDLDOMString at the moment.");

        auto& vm = lexicalGlobalObject.vm();
        auto scope = DECLARE_THROW_SCOPE(vm);

        auto string = value.toString(&lexicalGlobalObject)->toAtomString(&lexicalGlobalObject);

        RETURN_IF_EXCEPTION(scope, Result::exception());

        return Result { WTFMove(string) };
    }
};

template<> struct Converter<IDLAtomStringAdaptor<IDLUSVString>> : DefaultConverter<IDLAtomStringAdaptor<IDLUSVString>> {
    using Result = ConversionResult<IDLAtomStringAdaptor<IDLUSVString>>;

    static Result convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value)
    {
        return valueToUSVAtomString(lexicalGlobalObject, value);
    }
};

template<> struct Converter<IDLAtomStringAdaptor<IDLByteString>> : DefaultConverter<IDLAtomStringAdaptor<IDLByteString>> {
    using Result = ConversionResult<IDLAtomStringAdaptor<IDLByteString>>;

    static Result convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value)
    {
        return valueToByteAtomString(lexicalGlobalObject, value);
    }
};

template<typename T>  struct JSConverter<IDLAtomStringAdaptor<T>> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const AtomString& value)
    {
        return JSConverter<T>::convert(lexicalGlobalObject, value);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const String& value)
    {
        return JSConverter<T>::convert(lexicalGlobalObject, value);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const URL& value)
    {
        return JSConverter<T>::convert(lexicalGlobalObject, value.string());
    }
};

template<>  struct JSConverter<IDLAtomStringAdaptor<IDLUSVString>> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const AtomString& value)
    {
        return JSConverter<IDLUSVString>::convert(lexicalGlobalObject, value.string());
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const String& value)
    {
        return JSConverter<IDLUSVString>::convert(lexicalGlobalObject, value);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const URL& value)
    {
        return JSConverter<IDLUSVString>::convert(lexicalGlobalObject, value.string());
    }
};

template<> struct JSConverter<IDLAtomStringAdaptor<IDLByteString>> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const AtomString& value)
    {
        return JSConverter<IDLByteString>::convert(lexicalGlobalObject, value.string());
    }
};

template<typename T> struct Converter<IDLRequiresExistingAtomStringAdaptor<T>> : DefaultConverter<IDLRequiresExistingAtomStringAdaptor<T>> {
    static AtomString convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value)
    {
        static_assert(std::is_same<T, IDLDOMString>::value, "This adaptor is only supported for IDLDOMString at the moment.");

        return value.toString(&lexicalGlobalObject)->toExistingAtomString(&lexicalGlobalObject);
    }
};

template<typename T> struct JSConverter<IDLRequiresExistingAtomStringAdaptor<T>> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const AtomString& value)
    {
        static_assert(std::is_same<T, IDLDOMString>::value, "This adaptor is only supported for IDLDOMString at the moment.");

        return JSConverter<T>::convert(lexicalGlobalObject, value);
    }
};


template<> struct JSConverter<IDLNullable<IDLDOMString>> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const String& value)
    {
        if (value.isNull())
            return JSC::jsNull();
        return JSConverter<IDLDOMString>::convert(lexicalGlobalObject, value);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const UncachedString& value)
    {
        if (value.string.isNull())
            return JSC::jsNull();
        return JSConverter<IDLDOMString>::convert(lexicalGlobalObject, value);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const OwnedString& value)
    {
        if (value.string.isNull())
            return JSC::jsNull();
        return JSConverter<IDLDOMString>::convert(lexicalGlobalObject, value);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const URL& value)
    {
        if (value.isNull())
            return JSC::jsNull();
        return JSConverter<IDLDOMString>::convert(lexicalGlobalObject, value);
    }
};

template<> struct JSConverter<IDLNullable<IDLByteString>> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const String& value)
    {
        if (value.isNull())
            return JSC::jsNull();
        return JSConverter<IDLByteString>::convert(lexicalGlobalObject, value);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const UncachedString& value)
    {
        if (value.string.isNull())
            return JSC::jsNull();
        return JSConverter<IDLByteString>::convert(lexicalGlobalObject, value);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const OwnedString& value)
    {
        if (value.string.isNull())
            return JSC::jsNull();
        return JSConverter<IDLByteString>::convert(lexicalGlobalObject, value);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const URL& value)
    {
        if (value.isNull())
            return JSC::jsNull();
        return JSConverter<IDLByteString>::convert(lexicalGlobalObject, value);
    }
};

template<> struct JSConverter<IDLNullable<IDLUSVString>> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const String& value)
    {
        if (value.isNull())
            return JSC::jsNull();
        return JSConverter<IDLUSVString>::convert(lexicalGlobalObject, value);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const UncachedString& value)
    {
        if (value.string.isNull())
            return JSC::jsNull();
        return JSConverter<IDLUSVString>::convert(lexicalGlobalObject, value);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const OwnedString& value)
    {
        if (value.string.isNull())
            return JSC::jsNull();
        return JSConverter<IDLUSVString>::convert(lexicalGlobalObject, value);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const URL& value)
    {
        if (value.isNull())
            return JSC::jsNull();
        return JSConverter<IDLUSVString>::convert(lexicalGlobalObject, value);
    }
};

template<> struct JSConverter<IDLNullable<IDLAtomStringAdaptor<IDLDOMString>>> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const String& value)
    {
        if (value.isNull())
            return JSC::jsNull();
        return JSConverter<IDLDOMString>::convert(lexicalGlobalObject, value);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const AtomString& value)
    {
        if (value.isNull())
            return JSC::jsNull();
        return JSConverter<IDLDOMString>::convert(lexicalGlobalObject, value.string());
    }
};

} // namespace WebCore
