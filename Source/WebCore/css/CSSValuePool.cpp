/*
 * Copyright (C) 2011-2018 Apple Inc. All rights reserved.
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
#include "CSSValuePool.h"

#include "CSSParser.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSValueKeywords.h"
#include "CSSValueList.h"
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebCore {

CSSValuePool& CSSValuePool::singleton()
{
    ASSERT(isMainThread());
    static NeverDestroyed<CSSValuePool> pool;
    return pool;
}

CSSValuePool::CSSValuePool()
    : m_inheritedValue(CSSInheritedValue::create())
    , m_implicitInitialValue(CSSInitialValue::createImplicit())
    , m_explicitInitialValue(CSSInitialValue::createExplicit())
    , m_unsetValue(CSSUnsetValue::create())
    , m_revertValue(CSSRevertValue::create())
    , m_transparentColor(CSSPrimitiveValue::create(Color(Color::transparent)))
    , m_whiteColor(CSSPrimitiveValue::create(Color(Color::white)))
    , m_blackColor(CSSPrimitiveValue::create(Color(Color::black)))
{
    m_identifierValues.reserveInitialCapacity(numCSSValueKeywords);
    for (unsigned i = 0; i < numCSSValueKeywords; ++i)
        m_identifierValues.uncheckedAppend(CSSPrimitiveValue::create(static_cast<CSSValueID>(i)));

    m_pixelValues.reserveInitialCapacity(maximumCacheableIntegerValue + 1);
    m_percentValues.reserveInitialCapacity(maximumCacheableIntegerValue + 1);
    m_numberValues.reserveInitialCapacity(maximumCacheableIntegerValue + 1);
    for (unsigned i = 0; i < (maximumCacheableIntegerValue + 1); ++i) {
        m_pixelValues.uncheckedAppend(CSSPrimitiveValue::create(i, CSSUnitType::CSS_PX));
        m_percentValues.uncheckedAppend(CSSPrimitiveValue::create(i, CSSUnitType::CSS_PERCENTAGE));
        m_numberValues.uncheckedAppend(CSSPrimitiveValue::create(i, CSSUnitType::CSS_NUMBER));
    }
}

Ref<CSSPrimitiveValue> CSSValuePool::createIdentifierValue(CSSValueID ident)
{
    RELEASE_ASSERT(ident >= firstCSSValueKeyword && ident <= lastCSSValueKeyword);
    return m_identifierValues[ident].get();
}

Ref<CSSPrimitiveValue> CSSValuePool::createIdentifierValue(CSSPropertyID ident)
{
    return CSSPrimitiveValue::createIdentifier(ident);
}

Ref<CSSPrimitiveValue> CSSValuePool::createColorValue(const Color& color)
{
    // These are the empty and deleted values of the hash table.
    if (color == Color::transparent)
        return m_transparentColor.get();
    if (color == Color::white)
        return m_whiteColor.get();
    // Just because it is common.
    if (color == Color::black)
        return m_blackColor.get();

    // Remove one entry at random if the cache grows too large.
    // FIXME: Use TinyLRUCache instead?
    const int maximumColorCacheSize = 512;
    if (m_colorValueCache.size() >= maximumColorCacheSize)
        m_colorValueCache.remove(m_colorValueCache.random());

    return *m_colorValueCache.ensure(color, [&color] {
        return CSSPrimitiveValue::create(color);
    }).iterator->value;
}

Ref<CSSPrimitiveValue> CSSValuePool::createValue(double value, CSSUnitType type)
{
    ASSERT(std::isfinite(value));

    if (value < 0 || value > maximumCacheableIntegerValue)
        return CSSPrimitiveValue::create(value, type);

    int intValue = static_cast<int>(value);
    if (value != intValue)
        return CSSPrimitiveValue::create(value, type);

    switch (type) {
    case CSSUnitType::CSS_PX:
        return m_pixelValues[intValue].get();
    case CSSUnitType::CSS_PERCENTAGE:
        return m_percentValues[intValue].get();
    case CSSUnitType::CSS_NUMBER:
        return m_numberValues[intValue].get();
    default:
        return CSSPrimitiveValue::create(value, type);
    }
}

Ref<CSSPrimitiveValue> CSSValuePool::createFontFamilyValue(const String& familyName, FromSystemFontID fromSystemFontID)
{
    // Remove one entry at random if the cache grows too large.
    // FIXME: Use TinyLRUCache instead?
    const int maximumFontFamilyCacheSize = 128;
    if (m_fontFamilyValueCache.size() >= maximumFontFamilyCacheSize)
        m_fontFamilyValueCache.remove(m_fontFamilyValueCache.random());

    bool isFromSystemID = fromSystemFontID == FromSystemFontID::Yes;
    return *m_fontFamilyValueCache.ensure({ familyName, isFromSystemID }, [&familyName, isFromSystemID] {
        return CSSPrimitiveValue::create(CSSFontFamily { familyName, isFromSystemID });
    }).iterator->value;
}

RefPtr<CSSValueList> CSSValuePool::createFontFaceValue(const AtomString& string)
{
    // Remove one entry at random if the cache grows too large.
    // FIXME: Use TinyLRUCache instead?
    const int maximumFontFaceCacheSize = 128;
    if (m_fontFaceValueCache.size() >= maximumFontFaceCacheSize)
        m_fontFaceValueCache.remove(m_fontFaceValueCache.random());

    return m_fontFaceValueCache.ensure(string, [&string] () -> RefPtr<CSSValueList> {
        auto result = CSSParser::parseSingleValue(CSSPropertyFontFamily, string);
        if (!is<CSSValueList>(result))
            return nullptr;
        // FIXME: Make downcast work on RefPtr, remove the get() below, and save one reference count churn.
        return downcast<CSSValueList>(result.get());
    }).iterator->value;
}

void CSSValuePool::drain()
{
    m_colorValueCache.clear();
    m_fontFaceValueCache.clear();
    m_fontFamilyValueCache.clear();
}

}
