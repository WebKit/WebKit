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

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WebCore {
DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(CSSValuePool);

LazyNeverDestroyed<StaticCSSValuePool> staticCSSValuePool;

StaticCSSValuePool::StaticCSSValuePool()
{
    m_implicitInitialValue.construct(CSSValue::StaticCSSValue, CSSPrimitiveValue::ImplicitInitialValue);
    
    m_transparentColor.construct(CSSValue::StaticCSSValue, Color::transparentBlack);
    m_whiteColor.construct(CSSValue::StaticCSSValue, Color::white);
    m_blackColor.construct(CSSValue::StaticCSSValue, Color::black);

    for (auto keyword : allCSSValueKeywords())
        m_identifierValues[enumToUnderlyingType(keyword)].construct(CSSValue::StaticCSSValue, keyword);

    for (unsigned i = 0; i <= maximumCacheableIntegerValue; ++i) {
        m_pixelValues[i].construct(CSSValue::StaticCSSValue, i, CSSUnitType::CSS_PX);
        m_percentageValues[i].construct(CSSValue::StaticCSSValue, i, CSSUnitType::CSS_PERCENTAGE);
        m_numberValues[i].construct(CSSValue::StaticCSSValue, i, CSSUnitType::CSS_NUMBER);
    }
}

void StaticCSSValuePool::init()
{
    static std::once_flag onceKey;
    std::call_once(onceKey, []() {
        staticCSSValuePool.construct();
    });
}

CSSValuePool::CSSValuePool() = default;

// FIXME: This function needs a name that make it clear that this is not the one and only CSSValuePool, rather the one that can be used only on the main thread.
// FIXME: Consider a design where the value pool thread-local storage so callers don't have to deal directly with the value pool at all.
CSSValuePool& CSSValuePool::singleton()
{
    static MainThreadNeverDestroyed<CSSValuePool> pool;
    return pool;
}

Ref<CSSPrimitiveValue> CSSValuePool::createColorValue(const Color& color)
{
    // These are the empty and deleted values of the hash table.
    if (color == Color::transparentBlack)
        return staticCSSValuePool->m_transparentColor.get();
    if (color == Color::white)
        return staticCSSValuePool->m_whiteColor.get();
    // Just because it is common.
    if (color == Color::black)
        return staticCSSValuePool->m_blackColor.get();

    // Remove one entry at random if the cache grows too large.
    // FIXME: Use TinyLRUCache instead?
    const int maximumColorCacheSize = 512;
    if (m_colorValueCache.size() >= maximumColorCacheSize)
        m_colorValueCache.remove(m_colorValueCache.random());

    return m_colorValueCache.ensure(color, [&color] {
        return adoptRef(*new CSSPrimitiveValue(color));
    }).iterator->value;
}

Ref<CSSPrimitiveValue> CSSValuePool::createFontFamilyValue(const AtomString& familyName)
{
    // Remove one entry at random if the cache grows too large.
    // FIXME: Use TinyLRUCache instead?
    const int maximumFontFamilyCacheSize = 128;
    if (m_fontFamilyValueCache.size() >= maximumFontFamilyCacheSize)
        m_fontFamilyValueCache.remove(m_fontFamilyValueCache.random());

    return m_fontFamilyValueCache.ensure(familyName, [&familyName] {
        return CSSPrimitiveValue::createFontFamily(familyName);
    }).iterator->value;
}

RefPtr<CSSValueList> CSSValuePool::createFontFaceValue(const AtomString& string)
{
    // Remove one entry at random if the cache grows too large.
    // FIXME: Use TinyLRUCache instead?
    const int maximumFontFaceCacheSize = 128;
    if (m_fontFaceValueCache.size() >= maximumFontFaceCacheSize)
        m_fontFaceValueCache.remove(m_fontFaceValueCache.random());

    return m_fontFaceValueCache.ensure(string, [&string]() -> RefPtr<CSSValueList> {
        auto value = CSSParser::parseSingleValue(CSSPropertyFontFamily, string);
        return dynamicDowncast<CSSValueList>(value.get());
    }).iterator->value;
}

void CSSValuePool::drain()
{
    m_colorValueCache.clear();
    m_fontFaceValueCache.clear();
    m_fontFamilyValueCache.clear();
}

}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
