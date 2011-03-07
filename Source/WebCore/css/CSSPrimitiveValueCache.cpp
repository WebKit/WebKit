/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#include <config.h>
#include "CSSPrimitiveValueCache.h"

namespace WebCore {

CSSPrimitiveValueCache::CSSPrimitiveValueCache()
    : m_colorTransparent(CSSPrimitiveValue::createColor(Color::transparent))
    , m_colorWhite(CSSPrimitiveValue::createColor(Color::white))
    , m_colorBlack(CSSPrimitiveValue::createColor(Color::black))
{
}

CSSPrimitiveValueCache::~CSSPrimitiveValueCache()
{
}

PassRefPtr<CSSPrimitiveValue> CSSPrimitiveValueCache::createIdentifierValue(int ident)
{
    if (ident < 0 || ident >= numCSSValueKeywords)
        return CSSPrimitiveValue::createIdentifier(ident);

    RefPtr<CSSPrimitiveValue> primitiveValue = m_identifierValueCache[ident];
    if (!primitiveValue) {
        primitiveValue = CSSPrimitiveValue::createIdentifier(ident);
        m_identifierValueCache[ident] = primitiveValue;
    }
    return primitiveValue.release();
}

PassRefPtr<CSSPrimitiveValue> CSSPrimitiveValueCache::createColorValue(unsigned rgbValue)
{
    // These are the empty and deleted values of the hash table.
    if (rgbValue == Color::transparent)
        return m_colorTransparent;
    if (rgbValue == Color::white)
        return m_colorWhite;
    // Just because it is common.
    if (rgbValue == Color::black)
        return m_colorBlack;    

    RefPtr<CSSPrimitiveValue> primitiveValue = m_colorValueCache.get(rgbValue);
    if (primitiveValue)
        return primitiveValue.release();
    primitiveValue = CSSPrimitiveValue::createColor(rgbValue);
    // Just wipe out the cache and start rebuilding if it gets too big.
    const int maxColorCacheSize = 512;
    if (m_colorValueCache.size() >= maxColorCacheSize)
        m_colorValueCache.clear();
    m_colorValueCache.add(rgbValue, primitiveValue);
    
    return primitiveValue.release();
}

PassRefPtr<CSSPrimitiveValue> CSSPrimitiveValueCache::createValue(double value, CSSPrimitiveValue::UnitTypes type)
{
    if (type > maxCachedUnitType || value < 0 || value >= cachedIntegerCount)
        return CSSPrimitiveValue::create(value, type);

    int intValue = static_cast<int>(value);
    if (value != intValue)
        return CSSPrimitiveValue::create(value, type);
    RefPtr<CSSPrimitiveValue> primitiveValue = m_integerValueCache[intValue][type];
    if (!primitiveValue) {
        primitiveValue = CSSPrimitiveValue::create(value, type);
        m_integerValueCache[intValue][type] = primitiveValue;
    }
    return primitiveValue.release();
}

}
