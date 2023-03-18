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

#include "CSSFontFamilyValue.h"
#include "CSSParser.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSResolvedColorValue.h"
#include "CSSValueList.h"
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebCore {

CSSValuePool::CSSValuePool() = default;

// FIXME: This function needs a name that make it clear that this is not the one and only CSSValuePool, rather the one that can be used only on the main thread.
// FIXME: Consider a design where the value pool thread-local storage so callers don't have to deal directly with the value pool at all.
CSSValuePool& CSSValuePool::singleton()
{
    static MainThreadNeverDestroyed<CSSValuePool> pool;
    return pool;
}

Ref<CSSResolvedColorValue> CSSValuePool::createColorValue(const Color& color)
{
    if (auto value = CSSResolvedColorValue::tryCreateScalar(color))
        return value.releaseNonNull();

    // Remove one entry at random if the cache grows too large.
    // FIXME: Use TinyLRUCache instead?
    const int maximumColorCacheSize = 512;
    if (m_colorValueCache.size() >= maximumColorCacheSize)
        m_colorValueCache.remove(m_colorValueCache.random());

    return m_colorValueCache.ensure(color, [&color] {
        return adoptRef(*new CSSResolvedColorValue(Color { color }));
    }).iterator->value;
}

Ref<CSSFontFamilyValue> CSSValuePool::createFontFamilyValue(const AtomString& familyName)
{
    // Remove one entry at random if the cache grows too large.
    // FIXME: Use TinyLRUCache instead?
    const int maximumFontFamilyCacheSize = 128;
    if (m_fontFamilyValueCache.size() >= maximumFontFamilyCacheSize)
        m_fontFamilyValueCache.remove(m_fontFamilyValueCache.random());

    return m_fontFamilyValueCache.ensure(familyName, [&familyName] {
        return CSSFontFamilyValue::create(AtomString { familyName });
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
