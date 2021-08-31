/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "CSSStyleValue.h"

#if ENABLE(CSS_TYPED_OM)

#include "CSSParser.h"
#include "CSSPropertyParser.h"
#include "CSSUnitValue.h"
#include "CSSValueList.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/StringView.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(CSSStyleValue);

ExceptionOr<Vector<Ref<CSSStyleValue>>> CSSStyleValue::parseStyleValue(const String& cssProperty, const String& cssText, bool parseMultiple)
{
    // https://www.w3.org/TR/css-typed-om-1/#cssstylevalue
    
    String property;
    // 1. If property is not a custom property name string, set property to property ASCII lowercased.
    if (!isCustomPropertyName(property))
        property = cssProperty.convertToASCIILowercase();
    else
        property = cssProperty;
    
    
    // CSSPropertyID
    auto propertyID = cssPropertyID(property);

    // 2. If property is not a valid CSS property, throw a TypeError.
    if (propertyID == CSSPropertyInvalid)
        return Exception { TypeError, "Property String is not a valid CSS property."_s };
    
    auto styleDeclaration = MutableStyleProperties::create();
    
    constexpr bool important = true;
    auto parseResult = CSSParser::parseValue(styleDeclaration, propertyID, cssText, important, HTMLStandardMode);
    if (parseResult == CSSParser::ParseResult::Error)
        return Exception { SyntaxError, makeString(cssText, " cannot be parsed as a ", cssProperty)};

    auto cssValue = styleDeclaration->getPropertyCSSValue(propertyID);
    if (!cssValue)
        return Exception { SyntaxError, makeString(cssText, " cannot be parsed as a ", cssProperty)};

    Vector<Ref<CSSStyleValue>> results;

    if (is<CSSValueList>(cssValue.get())) {
        bool parsedFirst = false;
        for (auto& currentValue : downcast<CSSValueList>(*cssValue.get())) {
            if (!parseMultiple && parsedFirst)
                break;
            if (auto reifiedValue = CSSStyleValue::reifyValue(propertyID, currentValue.copyRef()))
                results.append(reifiedValue.releaseNonNull());
            parsedFirst = true;
        }
    } else {
        auto reifiedValue = CSSStyleValue::reifyValue(propertyID, cssValue.copyRef());
        if (reifiedValue)
            results.append(reifiedValue.releaseNonNull());
    }
    
    return results;
}

ExceptionOr<Ref<CSSStyleValue>> CSSStyleValue::parse(const String& property, const String& cssText)
{
    constexpr bool parseMultiple = false;
    auto parseResult = parseStyleValue(property, cssText, parseMultiple);
    if (parseResult.hasException())
        return parseResult.releaseException();
    
    auto returnValue = parseResult.releaseReturnValue();
    
    // Returned vector should not be empty. If parsing failed, an exception should be returned.
    if (returnValue.isEmpty())
        return Exception { SyntaxError, makeString(cssText, " cannot be parsed as a ", property) };

    return WTFMove(returnValue.at(0));
}

ExceptionOr<Vector<Ref<CSSStyleValue>>> CSSStyleValue::parseAll(const String& property, const String& cssText)
{
    constexpr bool parseMultiple = true;
    return parseStyleValue(property, cssText, parseMultiple);
}

Ref<CSSStyleValue> CSSStyleValue::create(RefPtr<CSSValue>&& cssValue, String&& property)
{
    return adoptRef(*new CSSStyleValue(WTFMove(cssValue), WTFMove(property)));
}

Ref<CSSStyleValue> CSSStyleValue::create()
{
    return adoptRef(*new CSSStyleValue());
}

CSSStyleValue::CSSStyleValue(RefPtr<CSSValue>&& cssValue, String&& property)
    : m_customPropertyName(WTFMove(property))
    , m_propertyValue(WTFMove(cssValue))
{
}

// Invokes static constructor of subclasses to reifyValues
RefPtr<CSSStyleValue> CSSStyleValue::reifyValue(CSSPropertyID, RefPtr<CSSValue>&&)
{
    // FIXME: Add Reification control flow. Returns nullptr if failed.
    return nullptr;
}

String CSSStyleValue::toString() const
{
    if (!m_propertyValue)
        return emptyString();
    
    return m_propertyValue->cssText();
}

} // namespace WebCore

#endif
