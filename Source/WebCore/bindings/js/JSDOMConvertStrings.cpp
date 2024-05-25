/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004-2011, 2013, 2016 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Samuel Weinig <sam@webkit.org>
 *  Copyright (C) 2013 Michael Pruett <michael@68k.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "JSDOMConvertStrings.h"

#include "JSDOMGlobalObject.h"
#include "JSTrustedHTML.h"
#include "JSTrustedScript.h"
#include "JSTrustedScriptURL.h"
#include "ScriptExecutionContext.h"
#include <JavaScriptCore/HeapInlines.h>
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {
using namespace JSC;

String identifierToString(JSGlobalObject& lexicalGlobalObject, const Identifier& identifier)
{
    if (UNLIKELY(identifier.isSymbol())) {
        auto scope = DECLARE_THROW_SCOPE(lexicalGlobalObject.vm());
        throwTypeError(&lexicalGlobalObject, scope, SymbolCoercionError);
        return { };
    }

    return identifier.string();
}

static inline bool throwIfInvalidByteString(JSGlobalObject& lexicalGlobalObject, JSC::ThrowScope& scope, const String& string)
{
    if (UNLIKELY(!string.containsOnlyLatin1())) {
        throwTypeError(&lexicalGlobalObject, scope);
        return true;
    }
    return false;
}

String identifierToByteString(JSGlobalObject& lexicalGlobalObject, const Identifier& identifier)
{
    VM& vm = lexicalGlobalObject.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto string = identifierToString(lexicalGlobalObject, identifier);
    RETURN_IF_EXCEPTION(scope, { });
    if (UNLIKELY(throwIfInvalidByteString(lexicalGlobalObject, scope, string)))
        return { };
    return string;
}

ConversionResult<IDLByteString> valueToByteString(JSGlobalObject& lexicalGlobalObject, JSValue value)
{
    VM& vm = lexicalGlobalObject.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto string = value.toWTFString(&lexicalGlobalObject);
    RETURN_IF_EXCEPTION(scope, ConversionResult<IDLByteString>::exception());

    if (UNLIKELY(throwIfInvalidByteString(lexicalGlobalObject, scope, string)))
        return ConversionResult<IDLByteString>::exception();

    return { WTFMove(string) };
}

ConversionResult<IDLAtomStringAdaptor<IDLByteString>> valueToByteAtomString(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value)
{
    VM& vm = lexicalGlobalObject.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto string = value.toString(&lexicalGlobalObject)->toAtomString(&lexicalGlobalObject);
    RETURN_IF_EXCEPTION(scope, ConversionResult<IDLAtomStringAdaptor<IDLByteString>>::exception());

    if (UNLIKELY(throwIfInvalidByteString(lexicalGlobalObject, scope, string.string())))
        return ConversionResult<IDLAtomStringAdaptor<IDLByteString>>::exception();

    return string;
}

String identifierToUSVString(JSGlobalObject& lexicalGlobalObject, const Identifier& identifier)
{
    return replaceUnpairedSurrogatesWithReplacementCharacter(identifierToString(lexicalGlobalObject, identifier));
}

ConversionResult<IDLUSVString> valueToUSVString(JSGlobalObject& lexicalGlobalObject, JSValue value)
{
    VM& vm = lexicalGlobalObject.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto string = value.toWTFString(&lexicalGlobalObject);
    RETURN_IF_EXCEPTION(scope, ConversionResult<IDLUSVString>::exception());

    return { replaceUnpairedSurrogatesWithReplacementCharacter(WTFMove(string)) };
}

ConversionResult<IDLAtomStringAdaptor<IDLUSVString>> valueToUSVAtomString(JSGlobalObject& lexicalGlobalObject, JSValue value)
{
    VM& vm = lexicalGlobalObject.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto string = value.toString(&lexicalGlobalObject)->toAtomString(&lexicalGlobalObject);
    RETURN_IF_EXCEPTION(scope, ConversionResult<IDLAtomStringAdaptor<IDLUSVString>>::exception());

    return replaceUnpairedSurrogatesWithReplacementCharacter(WTFMove(string));
}

// https://w3c.github.io/trusted-types/dist/spec/#get-trusted-type-compliant-string-algorithm
ConversionResult<IDLDOMString> trustedTypeCompliantString(TrustedType expectedType, JSGlobalObject& global, JSValue input, const String& sink, ShouldConvertNullToEmptyString shouldConvertNullToEmptyString)
{
    VM& vm = global.vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    switch (expectedType) {
    case TrustedType::TrustedHTML:
        if (auto* trustedHTML = JSTrustedHTML::toWrapped(vm, input))
            return { trustedHTML->toString() };
        break;
    case TrustedType::TrustedScript:
        if (auto* trustedScript = JSTrustedScript::toWrapped(vm, input))
            return { trustedScript->toString() };
        break;
    case TrustedType::TrustedScriptURL:
        if (auto* trustedScriptURL = JSTrustedScriptURL::toWrapped(vm, input))
            return { trustedScriptURL->toString() };
        break;
    default:
        ASSERT_NOT_REACHED();
        return { String() };
    }

    RefPtr scriptExecutionContext = jsDynamicCast<JSDOMGlobalObject*>(&global)->scriptExecutionContext();
    if (!scriptExecutionContext) {
        ASSERT_NOT_REACHED();
        return { String() };
    }

    auto convertedValue = expectedType == TrustedType::TrustedScriptURL
        ? ConversionResult<IDLDOMString> { convert<IDLUSVString>(global, input) }
        : ConversionResult<IDLDOMString> { convert<IDLDOMString>(global, input) };
    if (UNLIKELY(convertedValue.hasException(throwScope)))
        return ConversionResultException { };

    if (input.isNull() && shouldConvertNullToEmptyString == ShouldConvertNullToEmptyString::Yes)
        convertedValue = ConversionResult<IDLDOMString> { String(emptyString()) };

    auto stringValueHolder = trustedTypeCompliantString(expectedType, *scriptExecutionContext, convertedValue.releaseReturnValue(), sink);
    if (stringValueHolder.hasException()) {
        propagateException(global, throwScope, stringValueHolder.releaseException());
        RETURN_IF_EXCEPTION(throwScope, ConversionResultException { });
    }

    auto stringValue = stringValueHolder.releaseReturnValue();

    if (stringValue.isNull() && shouldConvertNullToEmptyString == ShouldConvertNullToEmptyString::Yes)
        return { String(emptyString()) };

    return { WTFMove(stringValue) };
}

} // namespace WebCore
