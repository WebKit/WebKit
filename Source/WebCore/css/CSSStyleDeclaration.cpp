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
#include "Document.h"
#include "Settings.h"
#include "StyledElement.h"
#include <variant>
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/StringParsingBuffer.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(CSSStyleDeclaration);

namespace {

enum class PropertyNamePrefix { None, Epub, WebKit };

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
    case 'e':
        if (matchesCSSPropertyNamePrefix(propertyName, "epub"))
            return PropertyNamePrefix::Epub;
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

static CSSPropertyID parseJavaScriptCSSPropertyName(const AtomString& propertyName)
{
    using CSSPropertyIDMap = HashMap<AtomString, CSSPropertyID>;
    static NeverDestroyed<CSSPropertyIDMap> propertyIDCache;

    auto* propertyNameString = propertyName.impl();
    if (!propertyNameString)
        return CSSPropertyInvalid;

    unsigned length = propertyNameString->length();
    if (!length)
        return CSSPropertyInvalid;

    if (auto id = propertyIDCache.get().get(propertyName))
        return id;

    constexpr size_t bufferSize = maxCSSPropertyNameLength;
    char buffer[bufferSize];
    char* bufferPtr = buffer;
    const char* name = bufferPtr;

    unsigned i = 0;
    switch (propertyNamePrefix(*propertyNameString)) {
    case PropertyNamePrefix::None:
        if (isASCIIUpper((*propertyNameString)[0]))
            return CSSPropertyInvalid;
        break;
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
        return CSSPropertyInvalid;

    for (; i < length; ++i) {
        UChar c = (*propertyNameString)[i];
        if (!c || !isASCII(c))
            return CSSPropertyInvalid; // illegal character
        if (isASCIIUpper(c)) {
            size_t bufferSizeLeft = stringEnd - bufferPtr;
            size_t propertySizeLeft = length - i + 1;
            if (propertySizeLeft > bufferSizeLeft)
                return CSSPropertyInvalid;
            *bufferPtr++ = '-';
            *bufferPtr++ = toASCIILowerUnchecked(c);
        } else
            *bufferPtr++ = c;
        ASSERT_WITH_SECURITY_IMPLICATION(bufferPtr < bufferEnd);
    }
    ASSERT_WITH_SECURITY_IMPLICATION(bufferPtr < bufferEnd);

    unsigned outputLength = bufferPtr - buffer;
    auto id = findCSSProperty(name, outputLength);
    // FIXME: Why aren't we memoizing CSS property names we fail to find?
    if (id != CSSPropertyInvalid)
        propertyIDCache.get().add(propertyName, id);
    return id;
}

}

CSSPropertyID CSSStyleDeclaration::getCSSPropertyIDFromJavaScriptPropertyName(const AtomString& propertyName)
{
    // FIXME: This exposes properties disabled by settings. Pass result of CSSStyleDeclaration::settings instead of null?
    Settings* settings = nullptr;
    auto property = parseJavaScriptCSSPropertyName(propertyName);
    return isExposed(property, settings) ? property : CSSPropertyInvalid;
}

const Settings* CSSStyleDeclaration::settings() const
{
    return parentElement() ? &parentElement()->document().settings() : nullptr;
}

enum class CSSPropertyLookupMode { ConvertUsingDashPrefix, ConvertUsingNoDashPrefix, NoConversion };

template<CSSPropertyLookupMode mode> static CSSPropertyID lookupCSSPropertyFromIDLAttribute(const AtomString& attribute)
{
    static NeverDestroyed<HashMap<AtomString, CSSPropertyID>> cache;

    if (auto id = cache.get().get(attribute))
        return id;

    char outputBuffer[maxCSSPropertyNameLength];
    char* outputBufferCurrent = outputBuffer;
    const char* outputBufferStart = outputBufferCurrent;

    if constexpr (mode == CSSPropertyLookupMode::ConvertUsingDashPrefix || mode == CSSPropertyLookupMode::ConvertUsingNoDashPrefix) {
        // Conversion is implementing the "IDL attribute to CSS property algorithm"
        // from https://drafts.csswg.org/cssom/#idl-attribute-to-css-property.

        if constexpr (mode == CSSPropertyLookupMode::ConvertUsingDashPrefix)
            *outputBufferCurrent++ = '-';

        readCharactersForParsing(attribute, [&](auto buffer) {
            while (buffer.hasCharactersRemaining()) {
                auto c = *buffer++;
                ASSERT_WITH_MESSAGE(isASCII(c), "Invalid property name: %s", attribute.string().utf8().data());
                if (isASCIIUpper(c)) {
                    *outputBufferCurrent++ = '-';
                    *outputBufferCurrent++ = toASCIILowerUnchecked(c);
                } else
                    *outputBufferCurrent++ = c;
            }
        });
    } else {
        readCharactersForParsing(attribute, [&](auto buffer) {
            while (buffer.hasCharactersRemaining()) {
                auto c = *buffer++;
                ASSERT_WITH_MESSAGE(c == '-' || isASCIILower(c), "Invalid property name: %s", attribute.string().utf8().data());
                *outputBufferCurrent++ = c;
            }
        });
    }

    auto id = findCSSProperty(outputBufferStart, outputBufferCurrent - outputBuffer);
    ASSERT_WITH_MESSAGE(id != CSSPropertyInvalid, "Invalid property name: %s", attribute.string().utf8().data());
    cache.get().add(attribute, id);
    return id;
}

String CSSStyleDeclaration::propertyValueForCamelCasedIDLAttribute(const AtomString& attribute)
{
    auto propertyID = lookupCSSPropertyFromIDLAttribute<CSSPropertyLookupMode::ConvertUsingNoDashPrefix>(attribute);
    ASSERT_WITH_MESSAGE(propertyID != CSSPropertyInvalid, "Invalid attribute: %s", attribute.string().utf8().data());
    return getPropertyValueInternal(propertyID);
}

ExceptionOr<void> CSSStyleDeclaration::setPropertyValueForCamelCasedIDLAttribute(const AtomString& attribute, const String& value)
{
    auto propertyID = lookupCSSPropertyFromIDLAttribute<CSSPropertyLookupMode::ConvertUsingNoDashPrefix>(attribute);
    ASSERT_WITH_MESSAGE(propertyID != CSSPropertyInvalid, "Invalid attribute: %s", attribute.string().utf8().data());
    return setPropertyInternal(propertyID, value, false);
}

String CSSStyleDeclaration::propertyValueForWebKitCasedIDLAttribute(const AtomString& attribute)
{
    auto propertyID = lookupCSSPropertyFromIDLAttribute<CSSPropertyLookupMode::ConvertUsingDashPrefix>(attribute);
    ASSERT_WITH_MESSAGE(propertyID != CSSPropertyInvalid, "Invalid attribute: %s", attribute.string().utf8().data());
    return getPropertyValueInternal(propertyID);
}

ExceptionOr<void> CSSStyleDeclaration::setPropertyValueForWebKitCasedIDLAttribute(const AtomString& attribute, const String& value)
{
    auto propertyID = lookupCSSPropertyFromIDLAttribute<CSSPropertyLookupMode::ConvertUsingDashPrefix>(attribute);
    ASSERT_WITH_MESSAGE(propertyID != CSSPropertyInvalid, "Invalid attribute: %s", attribute.string().utf8().data());
    return setPropertyInternal(propertyID, value, false);
}

String CSSStyleDeclaration::propertyValueForDashedIDLAttribute(const AtomString& attribute)
{
    auto propertyID = lookupCSSPropertyFromIDLAttribute<CSSPropertyLookupMode::NoConversion>(attribute);
    ASSERT_WITH_MESSAGE(propertyID != CSSPropertyInvalid, "Invalid attribute: %s", attribute.string().utf8().data());
    return getPropertyValueInternal(propertyID);
}

ExceptionOr<void> CSSStyleDeclaration::setPropertyValueForDashedIDLAttribute(const AtomString& attribute, const String& value)
{
    auto propertyID = lookupCSSPropertyFromIDLAttribute<CSSPropertyLookupMode::NoConversion>(attribute);
    ASSERT_WITH_MESSAGE(propertyID != CSSPropertyInvalid, "Invalid attribute: %s", attribute.string().utf8().data());
    return setPropertyInternal(propertyID, value, false);
}

String CSSStyleDeclaration::propertyValueForEpubCasedIDLAttribute(const AtomString& attribute)
{
    auto propertyID = lookupCSSPropertyFromIDLAttribute<CSSPropertyLookupMode::ConvertUsingDashPrefix>(attribute);
    ASSERT_WITH_MESSAGE(propertyID != CSSPropertyInvalid, "Invalid attribute: %s", attribute.string().utf8().data());
    return getPropertyValueInternal(propertyID);
}

ExceptionOr<void> CSSStyleDeclaration::setPropertyValueForEpubCasedIDLAttribute(const AtomString& attribute, const String& value)
{
    auto propertyID = lookupCSSPropertyFromIDLAttribute<CSSPropertyLookupMode::ConvertUsingDashPrefix>(attribute);
    ASSERT_WITH_MESSAGE(propertyID != CSSPropertyInvalid, "Invalid attribute: %s", attribute.string().utf8().data());
    return setPropertyInternal(propertyID, value, false);
}

String CSSStyleDeclaration::cssFloat()
{
    return getPropertyValueInternal(CSSPropertyFloat);
}

ExceptionOr<void> CSSStyleDeclaration::setCssFloat(const String& value)
{
    return setPropertyInternal(CSSPropertyFloat, value, false /* important */);
}

}
