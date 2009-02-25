/*
 * Copyright (C) 2007-2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSStyleDeclaration.h"

#include "CSSValue.h"
#include "CSSPrimitiveValue.h"
#include "EventTarget.h"

#include "V8Binding.h"
#include "V8CustomBinding.h"
#include "V8Proxy.h"

#include <wtf/ASCIICType.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

// FIXME: Next two functions look lifted verbatim from JSCSSStyleDeclarationCustom. Please remove duplication.

// Check for a CSS prefix.
// Passed prefix is all lowercase.
// First character of the prefix within the property name may be upper or lowercase.
// Other characters in the prefix within the property name must be lowercase.
// The prefix within the property name must be followed by a capital letter.
static bool hasCSSPropertyNamePrefix(const String& propertyName, const char* prefix)
{
#ifndef NDEBUG
    ASSERT(*prefix);
    for (const char* p = prefix; *p; ++p)
        ASSERT(WTF::isASCIILower(*p));
    ASSERT(propertyName.length());
#endif

    if (WTF::toASCIILower(propertyName[0]) != prefix[0])
        return false;

    unsigned length = propertyName.length();
    for (unsigned i = 1; i < length; ++i) {
        if (!prefix[i])
            return WTF::isASCIIUpper(propertyName[i]);
        if (propertyName[i] != prefix[i])
            return false;
    }
    return false;
}

// When getting properties on CSSStyleDeclarations, the name used from
// Javascript and the actual name of the property are not the same, so
// we have to do the following translation.  The translation turns upper
// case characters into lower case characters and inserts dashes to
// separate words.
//
// Example: 'backgroundPositionY' -> 'background-position-y'
//
// Also, certain prefixes such as 'pos', 'css-' and 'pixel-' are stripped
// and the hadPixelOrPosPrefix out parameter is used to indicate whether or
// not the property name was prefixed with 'pos-' or 'pixel-'.
static String cssPropertyName(const String& propertyName, bool& hadPixelOrPosPrefix)
{
    if (hadPixelOrPosPrefix)
        hadPixelOrPosPrefix = false;

    unsigned length = propertyName.length();
    if (!length)
        return String();

    Vector<UChar> name;
    name.reserveCapacity(length);

    unsigned i = 0;

    if (hasCSSPropertyNamePrefix(propertyName, "css"))
        i += 3;
    else if (hasCSSPropertyNamePrefix(propertyName, "pixel")) {
        i += 5;
        if (hadPixelOrPosPrefix)
            hadPixelOrPosPrefix = true;
    } else if (hasCSSPropertyNamePrefix(propertyName, "pos")) {
        i += 3;
        if (hadPixelOrPosPrefix)
            hadPixelOrPosPrefix = true;
    } else if (hasCSSPropertyNamePrefix(propertyName, "webkit")
            || hasCSSPropertyNamePrefix(propertyName, "khtml")
            || hasCSSPropertyNamePrefix(propertyName, "apple"))
        name.append('-');
    else {
        if (WTF::isASCIIUpper(propertyName[0]))
            return String();
    }

    name.append(WTF::toASCIILower(propertyName[i++]));

    for (; i < length; ++i) {
        UChar c = propertyName[i];
        if (!WTF::isASCIIUpper(c))
            name.append(c);
        else {
            name.append('-');
            name.append(WTF::toASCIILower(c));
        }
    }

    return String::adopt(name);
}

NAMED_PROPERTY_GETTER(CSSStyleDeclaration)
{
    INC_STATS("DOM.CSSStyleDeclaration.NamedPropertyGetter");
    // First look for API defined attributes on the style declaration object.
    if (info.Holder()->HasRealNamedCallbackProperty(name))
        return notHandledByInterceptor();

    // Search the style declaration.
    CSSStyleDeclaration* imp = V8Proxy::ToNativeObject<CSSStyleDeclaration>(V8ClassIndex::CSSSTYLEDECLARATION, info.Holder());

    bool hadPixelOrPosPrefix;
    String propertyName = cssPropertyName(toWebCoreString(name), hadPixelOrPosPrefix);

    // Do not handle non-property names.
    if (!CSSStyleDeclaration::isPropertyName(propertyName))
        return notHandledByInterceptor();


    RefPtr<CSSValue> cssValue = imp->getPropertyCSSValue(propertyName);
    if (cssValue) {
        if (hadPixelOrPosPrefix && cssValue->cssValueType() == CSSValue::CSS_PRIMITIVE_VALUE)
            return v8::Number::New(static_cast<CSSPrimitiveValue*>(cssValue.get())->getFloatValue(CSSPrimitiveValue::CSS_PX));
        return v8StringOrNull(cssValue->cssText());
    }

    String result = imp->getPropertyValue(propertyName);
    if (result.isNull())
        result = "";  // convert null to empty string.

    // The 'filter' attribute is made undetectable in KJS/WebKit
    // to avoid confusion with IE's filter extension.
    if (propertyName == "filter")
        return v8UndetectableString(result);

    return v8String(result);
}

NAMED_PROPERTY_SETTER(CSSStyleDeclaration)
{
    INC_STATS("DOM.CSSStyleDeclaration.NamedPropertySetter");
    CSSStyleDeclaration* imp = V8Proxy::ToNativeObject<CSSStyleDeclaration>(V8ClassIndex::CSSSTYLEDECLARATION, info.Holder());

    bool hadPixelOrPosPrefix;
    String prop = cssPropertyName(toWebCoreString(name), hadPixelOrPosPrefix);
    if (!CSSStyleDeclaration::isPropertyName(prop))
        return notHandledByInterceptor();

    String propertyValue = valueToStringWithNullCheck(value);
    if (hadPixelOrPosPrefix)
        propertyValue.append("px");

    ExceptionCode ec = 0;
    imp->setProperty(prop, propertyValue, ec);

    if (ec)
        throwError(ec);

    return value;
}

} // namespace WebCore
