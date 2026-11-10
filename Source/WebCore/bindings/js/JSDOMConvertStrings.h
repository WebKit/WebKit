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

#include <JavaScriptCore/JSCJSValuePropertyInlines.h>
#include <JavaScriptCore/JSGlobalObject.h>
#include <JavaScriptCore/JSString.h>
#include <WebCore/IDLTypes.h>
#include <WebCore/JSDOMConvertBase.h>
#include <WebCore/StringAdaptors.h>
#include <WebCore/TrustedType.h>
#include <wtf/ValueOrReference.h>

namespace WebCore {

class ScriptExecutionContext;

WEBCORE_EXPORT RefPtr<StringImpl> identifierToString(JSC::JSGlobalObject&, const JSC::Identifier&);

WEBCORE_EXPORT RefPtr<StringImpl> identifierToByteString(JSC::JSGlobalObject&, const JSC::Identifier&);
WEBCORE_EXPORT ConversionResult<IDLByteString> valueToByteString(JSC::JSGlobalObject&, JSC::JSValue);
WEBCORE_EXPORT ConversionResult<IDLAtomStringAdaptor<IDLByteString>> valueToByteAtomString(JSC::JSGlobalObject&, JSC::JSValue);

WEBCORE_EXPORT RefPtr<StringImpl> identifierToUSVString(JSC::JSGlobalObject&, const JSC::Identifier&);
WEBCORE_EXPORT ConversionResult<IDLUSVString> valueToUSVString(JSC::JSGlobalObject&, JSC::JSValue);
WEBCORE_EXPORT ConversionResult<IDLAtomStringAdaptor<IDLUSVString>> valueToUSVAtomString(JSC::JSGlobalObject&, JSC::JSValue);

ConversionResult<IDLDOMString> trustedScriptCompliantString(JSC::JSGlobalObject&, JSC::JSValue, StringImpl* sink);

// Symbols are not atoms, so callers must not pass a symbol property name.
inline RefPtr<AtomStringImpl> propertyNameToString(JSC::PropertyName propertyName)
{
    ASSERT(!propertyName.isSymbol());
    return propertyName.publicName();
}

inline RefPtr<AtomStringImpl> propertyNameToAtomString(JSC::PropertyName propertyName)
{
    ASSERT(!propertyName.isSymbol());
    return propertyName.publicName();
}

// MARK: -
// MARK: String types

// Values that convert to RefPtr<StringImpl> but not to StringImpl*, such as ASCIILiteral.
template<typename T> concept ConvertibleToStringImplRefPtrOnly = std::is_convertible_v<T, RefPtr<StringImpl>> && !std::is_convertible_v<T, StringImpl*>;

template<> struct Converter<IDLDOMString> : DefaultConverter<IDLDOMString> {
    using Result = ConversionResult<IDLDOMString>;

    WEBCORE_EXPORT static Result convert(JSC::JSGlobalObject&, JSC::JSValue);
};

template<> struct JSConverter<IDLDOMString> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, StringImpl* value)
    {
        return JSC::jsStringWithCache(JSC::getVM(&lexicalGlobalObject), value);
    }

    template<WTF::StringImplClass T> static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const RefPtr<T>& value)
    {
        return convert(lexicalGlobalObject, value.get());
    }
    template<WTF::StringImplClass T> static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const ValueOrReference<RefPtr<T>>& value)
    {
        return convert(lexicalGlobalObject, value.get());
    }

    template<ConvertibleToStringImplRefPtrOnly T> static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, T&& value)
    {
        return convert(lexicalGlobalObject, RefPtr<StringImpl> { std::forward<T>(value) }.get());
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

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, StringImpl* value)
    {
        return JSC::jsStringWithCache(JSC::getVM(&lexicalGlobalObject), value);
    }

    template<ConvertibleToStringImplRefPtrOnly T> static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, T&& value)
    {
        return convert(lexicalGlobalObject, RefPtr<StringImpl> { std::forward<T>(value) }.get());
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

    template<WTF::StringImplClass T> static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const RefPtr<T>& value)
    {
        return convert(lexicalGlobalObject, value.get());
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

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, StringImpl* value)
    {
        return JSC::jsStringWithCache(JSC::getVM(&lexicalGlobalObject), value);
    }

    template<ConvertibleToStringImplRefPtrOnly T> static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, T&& value)
    {
        return convert(lexicalGlobalObject, RefPtr<StringImpl> { std::forward<T>(value) }.get());
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

    template<WTF::StringImplClass T> static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const RefPtr<T>& value)
    {
        return convert(lexicalGlobalObject, value.get());
    }
};

// MARK: -
// MARK: String type adaptors

template<typename TrustedStringAdaptorIDL>
struct TrustedStringAdaptorTraits;

template<typename T> struct TrustedStringAdaptorTraits<IDLStringContextTrustedScriptAdaptor<T>> {
    static constexpr auto trustedType = TrustedType::TrustedScript;
};

template<typename IDL>
struct TrustedStringConverter : DefaultConverter<IDL> {
    using Result = ConversionResult<IDL>;
    using Traits = TrustedStringAdaptorTraits<IDL>;

    static Result convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value)
    {
        return Converter<IDL>::convert(lexicalGlobalObject, value, emptyString());
    }

    static Result convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value, StringImpl* sink)
    {
        return trustedScriptCompliantString(lexicalGlobalObject, value, sink);
    }
};

template<typename InnerStringType>
struct JSStringAdaptorConverter  {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, StringImpl* value)
    {
        return JSConverter<InnerStringType>::convert(lexicalGlobalObject, value);
    }

    template<ConvertibleToStringImplRefPtrOnly T> static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, T&& value)
    {
        return JSConverter<InnerStringType>::convert(lexicalGlobalObject, std::forward<T>(value));
    }

    template<WTF::StringImplClass T> static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const RefPtr<T>& value)
    {
        return convert(lexicalGlobalObject, value.get());
    }
    template<WTF::StringImplClass T> static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const ValueOrReference<RefPtr<T>>& value)
    {
        return convert(lexicalGlobalObject, value.get());
    }
};

template<typename InnerStringType>
struct JSAtomStringAdaptorConverter  {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, AtomStringImpl* value)
    {
        return JSConverter<InnerStringType>::convert(lexicalGlobalObject, value);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const URL& value)
    {
        return JSConverter<InnerStringType>::convert(lexicalGlobalObject, value.string());
    }

    template<WTF::StringImplClass T> static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const RefPtr<T>& value)
    {
        return convert(lexicalGlobalObject, value.get());
    }
};

template<typename T> struct Converter<IDLStringContextTrustedScriptAdaptor<T>>
    : TrustedStringConverter<IDLStringContextTrustedScriptAdaptor<T>> { };

template<typename T> struct JSConverter<IDLLegacyNullToEmptyStringAdaptor<T>> : JSStringAdaptorConverter<T> { };
template<typename T> struct JSConverter<IDLStringContextTrustedScriptAdaptor<T>> : JSStringAdaptorConverter<T> { };
template<typename T> struct JSConverter<IDLLegacyNullToEmptyAtomStringAdaptor<T>> : JSAtomStringAdaptorConverter<T> { };

template<typename IDL> struct Converter<IDLLegacyNullToEmptyStringAdaptor<IDL>> : DefaultConverter<IDLLegacyNullToEmptyStringAdaptor<IDL>> {
    using Result = ConversionResult<IDLLegacyNullToEmptyStringAdaptor<IDL>>;

    static Result convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value)
    {
        if (value.isNull())
            return emptyString();
        return WebCore::convert<IDL>(lexicalGlobalObject, value);
    }
};

template<typename T> struct Converter<IDLLegacyNullToEmptyAtomStringAdaptor<T>> : DefaultConverter<IDLLegacyNullToEmptyAtomStringAdaptor<T>> {
    using Result = ConversionResult<IDLLegacyNullToEmptyAtomStringAdaptor<T>>;

    static Result convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value)
    {
        if (value.isNull())
            return emptyAtom();
        return WebCore::convert<IDLAtomStringAdaptor<T>>(lexicalGlobalObject, value);
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

        return Result { string.data };
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

template<typename IDL>  struct JSConverter<IDLAtomStringAdaptor<IDL>> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, AtomStringImpl* value)
    {
        return toJS<IDL>(lexicalGlobalObject, value);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, StringImpl* value)
    {
        return toJS<IDL>(lexicalGlobalObject, value);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const URL& value)
    {
        return toJS<IDL>(lexicalGlobalObject, value.string());
    }

    template<WTF::StringImplClass T> static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const RefPtr<T>& value)
    {
        return convert(lexicalGlobalObject, value.get());
    }
};

template<>  struct JSConverter<IDLAtomStringAdaptor<IDLUSVString>> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, AtomStringImpl* value)
    {
        return toJS<IDLUSVString>(lexicalGlobalObject, value);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, StringImpl* value)
    {
        return toJS<IDLUSVString>(lexicalGlobalObject, value);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const URL& value)
    {
        return toJS<IDLUSVString>(lexicalGlobalObject, value.string());
    }

    template<WTF::StringImplClass T> static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const RefPtr<T>& value)
    {
        return convert(lexicalGlobalObject, value.get());
    }
};

template<> struct JSConverter<IDLAtomStringAdaptor<IDLByteString>> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, AtomStringImpl* value)
    {
        return toJS<IDLByteString>(lexicalGlobalObject, value);
    }

    template<WTF::StringImplClass T> static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const RefPtr<T>& value)
    {
        return convert(lexicalGlobalObject, value.get());
    }
};

template<typename IDL> struct Converter<IDLRequiresExistingAtomStringAdaptor<IDL>> : DefaultConverter<IDLRequiresExistingAtomStringAdaptor<IDL>> {
    static RefPtr<AtomStringImpl> convert(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value)
    {
        static_assert(std::is_same<IDL, IDLDOMString>::value, "This adaptor is only supported for IDLDOMString at the moment.");

        return value.toString(&lexicalGlobalObject)->toExistingAtomString(&lexicalGlobalObject).data;
    }
};

template<typename IDL> struct JSConverter<IDLRequiresExistingAtomStringAdaptor<IDL>> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, AtomStringImpl* value)
    {
        static_assert(std::is_same<IDL, IDLDOMString>::value, "This adaptor is only supported for IDLDOMString at the moment.");

        return toJS<IDL>(lexicalGlobalObject, value);
    }

    template<WTF::StringImplClass T> static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const RefPtr<T>& value)
    {
        return convert(lexicalGlobalObject, value.get());
    }
};


template<> struct JSConverter<IDLNullable<IDLDOMString>> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, StringImpl* value)
    {
        if (!value)
            return JSC::jsNull();
        return toJS<IDLDOMString>(lexicalGlobalObject, value);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const UncachedString& value)
    {
        if (!value.string)
            return JSC::jsNull();
        return toJS<IDLDOMString>(lexicalGlobalObject, value);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const OwnedString& value)
    {
        if (!value.string)
            return JSC::jsNull();
        return toJS<IDLDOMString>(lexicalGlobalObject, value);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const URL& value)
    {
        if (value.isNull())
            return JSC::jsNull();
        return toJS<IDLDOMString>(lexicalGlobalObject, value);
    }

    template<WTF::StringImplClass T> static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const RefPtr<T>& value)
    {
        return convert(lexicalGlobalObject, value.get());
    }
};

template<> struct JSConverter<IDLNullable<IDLByteString>> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, StringImpl* value)
    {
        if (!value)
            return JSC::jsNull();
        return toJS<IDLByteString>(lexicalGlobalObject, value);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const UncachedString& value)
    {
        if (!value.string)
            return JSC::jsNull();
        return toJS<IDLByteString>(lexicalGlobalObject, value);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const OwnedString& value)
    {
        if (!value.string)
            return JSC::jsNull();
        return toJS<IDLByteString>(lexicalGlobalObject, value);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const URL& value)
    {
        if (value.isNull())
            return JSC::jsNull();
        return toJS<IDLByteString>(lexicalGlobalObject, value);
    }

    template<WTF::StringImplClass T> static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const RefPtr<T>& value)
    {
        return convert(lexicalGlobalObject, value.get());
    }
};

template<> struct JSConverter<IDLNullable<IDLUSVString>> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, StringImpl* value)
    {
        if (!value)
            return JSC::jsNull();
        return toJS<IDLUSVString>(lexicalGlobalObject, value);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const UncachedString& value)
    {
        if (!value.string)
            return JSC::jsNull();
        return toJS<IDLUSVString>(lexicalGlobalObject, value);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const OwnedString& value)
    {
        if (!value.string)
            return JSC::jsNull();
        return toJS<IDLUSVString>(lexicalGlobalObject, value);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const URL& value)
    {
        if (value.isNull())
            return JSC::jsNull();
        return toJS<IDLUSVString>(lexicalGlobalObject, value);
    }

    template<WTF::StringImplClass T> static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const RefPtr<T>& value)
    {
        return convert(lexicalGlobalObject, value.get());
    }
};

template<> struct JSConverter<IDLNullable<IDLAtomStringAdaptor<IDLDOMString>>> {
    static constexpr bool needsState = true;
    static constexpr bool needsGlobalObject = false;

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, StringImpl* value)
    {
        if (!value)
            return JSC::jsNull();
        return toJS<IDLDOMString>(lexicalGlobalObject, value);
    }

    static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, AtomStringImpl* value)
    {
        if (!value)
            return JSC::jsNull();
        return toJS<IDLDOMString>(lexicalGlobalObject, value);
    }

    template<WTF::StringImplClass T> static JSC::JSValue convert(JSC::JSGlobalObject& lexicalGlobalObject, const RefPtr<T>& value)
    {
        return convert(lexicalGlobalObject, value.get());
    }
};

} // namespace WebCore
