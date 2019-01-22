/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "config.h"
#include "CSSStyleDeclaration.h"

#include "CSSPropertyNames.h"
#include "CSSPropertyParser.h"
#include "DeprecatedGlobalSettings.h"
#include "HashTools.h"
#include "RuntimeEnabledFeatures.h"

namespace WebCore {

namespace {

enum class PropertyNamePrefix {
    None, Epub, CSS, Pixel, Pos, WebKit,
#if ENABLE(LEGACY_CSS_VENDOR_PREFIXES)
    Apple, KHTML,
#endif
};

template<size_t prefixCStringLength>
static inline bool matchesCSSPropertyNamePrefix(const StringImpl& propertyName, const char (&prefix)[prefixCStringLength])
{
    size_t prefixLength = prefixCStringLength - 1;

    ASSERT(toASCIILower(propertyName[0]) == prefix[0]);
    const size_t offset = 1;

#ifndef NDEBUG
    for (size_t i = 0; i < prefixLength; ++i)
        ASSERT(isASCIILower(prefix[i]));
    ASSERT(!prefix[prefixLength]);
    ASSERT(propertyName.length());
#endif

    // The prefix within the property name must be followed by a capital letter.
    // Other characters in the prefix within the property name must be lowercase.
    if (propertyName.length() < prefixLength + 1)
        return false;

    for (size_t i = offset; i < prefixLength; ++i) {
        if (propertyName[i] != prefix[i])
            return false;
    }

    if (!isASCIIUpper(propertyName[prefixLength]))
        return false;

    return true;
}

static PropertyNamePrefix propertyNamePrefix(const StringImpl& propertyName)
{
    ASSERT(propertyName.length());

    // First character of the prefix within the property name may be upper or lowercase.
    UChar firstChar = toASCIILower(propertyName[0]);
    switch (firstChar) {
#if ENABLE(LEGACY_CSS_VENDOR_PREFIXES)
    case 'a':
        if (RuntimeEnabledFeatures::sharedFeatures().legacyCSSVendorPrefixesEnabled() && matchesCSSPropertyNamePrefix(propertyName, "apple"))
            return PropertyNamePrefix::Apple;
        break;
#endif
    case 'c':
        if (matchesCSSPropertyNamePrefix(propertyName, "css"))
            return PropertyNamePrefix::CSS;
        break;
#if ENABLE(LEGACY_CSS_VENDOR_PREFIXES)
    case 'k':
        if (RuntimeEnabledFeatures::sharedFeatures().legacyCSSVendorPrefixesEnabled() && matchesCSSPropertyNamePrefix(propertyName, "khtml"))
            return PropertyNamePrefix::KHTML;
        break;
#endif
    case 'e':
        if (matchesCSSPropertyNamePrefix(propertyName, "epub"))
            return PropertyNamePrefix::Epub;
        break;
    case 'p':
        if (matchesCSSPropertyNamePrefix(propertyName, "pos"))
            return PropertyNamePrefix::Pos;
        if (matchesCSSPropertyNamePrefix(propertyName, "pixel"))
            return PropertyNamePrefix::Pixel;
        break;
    case 'w':
        if (matchesCSSPropertyNamePrefix(propertyName, "webkit"))
            return PropertyNamePrefix::WebKit;
        break;
    default:
        break;
    }
    return PropertyNamePrefix::None;
}

static inline void writeWebKitPrefix(char*& buffer)
{
    *buffer++ = '-';
    *buffer++ = 'w';
    *buffer++ = 'e';
    *buffer++ = 'b';
    *buffer++ = 'k';
    *buffer++ = 'i';
    *buffer++ = 't';
    *buffer++ = '-';
}

static inline void writeEpubPrefix(char*& buffer)
{
    *buffer++ = '-';
    *buffer++ = 'e';
    *buffer++ = 'p';
    *buffer++ = 'u';
    *buffer++ = 'b';
    *buffer++ = '-';
}

struct CSSPropertyInfo {
    CSSPropertyID propertyID;
    bool hadPixelOrPosPrefix;
};

static CSSPropertyInfo parseJavaScriptCSSPropertyName(const AtomicString& propertyName)
{
    using CSSPropertyInfoMap = HashMap<String, CSSPropertyInfo>;
    static NeverDestroyed<CSSPropertyInfoMap> propertyInfoCache;

    CSSPropertyInfo propertyInfo = { CSSPropertyInvalid, false };

    auto* propertyNameString = propertyName.impl();
    if (!propertyNameString)
        return propertyInfo;
    unsigned length = propertyNameString->length();
    if (!length)
        return propertyInfo;

    propertyInfo = propertyInfoCache.get().get(propertyNameString);
    if (propertyInfo.propertyID)
        return propertyInfo;

    bool hadPixelOrPosPrefix = false;

    constexpr size_t bufferSize = maxCSSPropertyNameLength + 1;
    char buffer[bufferSize];
    char* bufferPtr = buffer;
    const char* name = bufferPtr;

    unsigned i = 0;
    // Prefixes CSS, Pixel, Pos are ignored.
    // Prefixes Apple, KHTML and Webkit are transposed to "-webkit-".
    // The prefix "Epub" becomes "-epub-".
    switch (propertyNamePrefix(*propertyNameString)) {
    case PropertyNamePrefix::None:
        if (isASCIIUpper((*propertyNameString)[0]))
            return propertyInfo;
        break;
    case PropertyNamePrefix::CSS:
        i += 3;
        break;
    case PropertyNamePrefix::Pixel:
        i += 5;
        hadPixelOrPosPrefix = true;
        break;
    case PropertyNamePrefix::Pos:
        i += 3;
        hadPixelOrPosPrefix = true;
        break;
#if ENABLE(LEGACY_CSS_VENDOR_PREFIXES)
    case PropertyNamePrefix::Apple:
    case PropertyNamePrefix::KHTML:
        ASSERT(RuntimeEnabledFeatures::sharedFeatures().legacyCSSVendorPrefixesEnabled());
        writeWebKitPrefix(bufferPtr);
        i += 5;
        break;
#endif
    case PropertyNamePrefix::Epub:
        writeEpubPrefix(bufferPtr);
        i += 4;
        break;
    case PropertyNamePrefix::WebKit:
        writeWebKitPrefix(bufferPtr);
        i += 6;
        break;
    }

    *bufferPtr++ = toASCIILower((*propertyNameString)[i++]);

    char* bufferEnd = buffer + bufferSize;
    char* stringEnd = bufferEnd - 1;
    size_t bufferSizeLeft = stringEnd - bufferPtr;
    size_t propertySizeLeft = length - i;
    if (propertySizeLeft > bufferSizeLeft)
        return propertyInfo;

    for (; i < length; ++i) {
        UChar c = (*propertyNameString)[i];
        if (!c || !isASCII(c))
            return propertyInfo; // illegal character
        if (isASCIIUpper(c)) {
            size_t bufferSizeLeft = stringEnd - bufferPtr;
            size_t propertySizeLeft = length - i + 1;
            if (propertySizeLeft > bufferSizeLeft)
                return propertyInfo;
            *bufferPtr++ = '-';
            *bufferPtr++ = toASCIILowerUnchecked(c);
        } else
            *bufferPtr++ = c;
        ASSERT_WITH_SECURITY_IMPLICATION(bufferPtr < bufferEnd);
    }
    ASSERT_WITH_SECURITY_IMPLICATION(bufferPtr < bufferEnd);
    *bufferPtr = '\0';

    unsigned outputLength = bufferPtr - buffer;
#if PLATFORM(IOS_FAMILY)
    cssPropertyNameIOSAliasing(buffer, name, outputLength);
#endif

    auto* hashTableEntry = findProperty(name, outputLength);
    if (auto propertyID = hashTableEntry ? hashTableEntry->id : 0) {
        auto id = static_cast<CSSPropertyID>(propertyID);
        if (isEnabledCSSProperty(id)) {
            propertyInfo.hadPixelOrPosPrefix = hadPixelOrPosPrefix;
            propertyInfo.propertyID = id;
            propertyInfoCache.get().add(propertyNameString, propertyInfo);
        }
    }
    return propertyInfo;
}

}

CSSPropertyID CSSStyleDeclaration::getCSSPropertyIDFromJavaScriptPropertyName(const AtomicString& propertyName)
{
    return parseJavaScriptCSSPropertyName(propertyName).propertyID;
}

Optional<Variant<String, double>> CSSStyleDeclaration::namedItem(const AtomicString& propertyName)
{
    auto propertyInfo = parseJavaScriptCSSPropertyName(propertyName);
    if (!propertyInfo.propertyID)
        return WTF::nullopt;

    auto value = getPropertyCSSValueInternal(propertyInfo.propertyID);
    if (!value) {
        // If the property is a shorthand property (such as "padding"), it can only be accessed using getPropertyValue.
        return Variant<String, double> { getPropertyValueInternal(propertyInfo.propertyID) };
    }
    
    if (propertyInfo.hadPixelOrPosPrefix && is<CSSPrimitiveValue>(*value)) {
        // Call this version of the getter so that, e.g., pixelTop returns top as a number
        // in pixel units and posTop should does the same _if_ this is a positioned element.
        // FIXME: If not a positioned element, MSIE documentation says posTop should return 0; this rule is not implemented.
        return Variant<String, double> { downcast<CSSPrimitiveValue>(*value).floatValue(CSSPrimitiveValue::CSS_PX) };
    }

    return Variant<String, double> { value->cssText() };
}

ExceptionOr<void> CSSStyleDeclaration::setNamedItem(const AtomicString& propertyName, String value, bool& propertySupported)
{
    auto propertyInfo = parseJavaScriptCSSPropertyName(propertyName);
    if (!propertyInfo.propertyID) {
        propertySupported = false;
        return { };
    }

    propertySupported = true;

    if (propertyInfo.hadPixelOrPosPrefix)
        value.append("px");

    bool important = false;
    if (DeprecatedGlobalSettings::shouldRespectPriorityInCSSAttributeSetters()) {
        auto importantIndex = value.findIgnoringASCIICase("!important");
        if (importantIndex && importantIndex != notFound) {
            important = true;
            value = value.left(importantIndex - 1);
        }
    }

    auto setPropertyInternalResult = setPropertyInternal(propertyInfo.propertyID, value, important);
    if (setPropertyInternalResult.hasException())
        return setPropertyInternalResult.releaseException();

    return { };
}

Vector<AtomicString> CSSStyleDeclaration::supportedPropertyNames() const
{
    static unsigned numNames = 0;
    static const AtomicString* const cssPropertyNames = [] {
        String names[numCSSProperties];
        for (int i = 0; i < numCSSProperties; ++i) {
            CSSPropertyID id = static_cast<CSSPropertyID>(firstCSSProperty + i);
            if (isEnabledCSSProperty(id))
                names[numNames++] = getJSPropertyName(id);
        }
        std::sort(&names[0], &names[numNames], WTF::codePointCompareLessThan);
        auto* identifiers = new AtomicString[numNames];
        for (unsigned i = 0; i < numNames; ++i)
            identifiers[i] = names[i];
        return identifiers;
    }();

    Vector<AtomicString> result;
    result.reserveInitialCapacity(numNames);

    for (unsigned i = 0; i < numNames; ++i)
        result.uncheckedAppend(cssPropertyNames[i]);

    return result;
}

String CSSStyleDeclaration::cssFloat()
{
    return getPropertyValueInternal(CSSPropertyFloat);
}

ExceptionOr<void> CSSStyleDeclaration::setCssFloat(const String& value)
{
    auto result = setPropertyInternal(CSSPropertyFloat, value, false /* important */);
    if (result.hasException())
        return result.releaseException();
    return { };
}

}
