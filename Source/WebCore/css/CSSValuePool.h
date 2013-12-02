/*
 * Copyright (C) 2011, 2012 Apple Inc. All rights reserved.
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

#ifndef CSSValuePool_h
#define CSSValuePool_h

#include "CSSInheritedValue.h"
#include "CSSInitialValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include <wtf/HashMap.h>
#include <wtf/RefPtr.h>
#include <wtf/text/AtomicStringHash.h>

namespace WebCore {

class CSSValueList;

class CSSValuePool {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PassRefPtr<CSSValueList> createFontFaceValue(const AtomicString&);
    PassRef<CSSPrimitiveValue> createFontFamilyValue(const String&);
    PassRef<CSSInheritedValue> createInheritedValue() { return m_inheritedValue.get(); }
    PassRef<CSSInitialValue> createImplicitInitialValue() { return m_implicitInitialValue.get(); }
    PassRef<CSSInitialValue> createExplicitInitialValue() { return m_explicitInitialValue.get(); }
    PassRef<CSSPrimitiveValue> createIdentifierValue(CSSValueID identifier);
    PassRef<CSSPrimitiveValue> createIdentifierValue(CSSPropertyID identifier);
    PassRef<CSSPrimitiveValue> createColorValue(unsigned rgbValue);
    PassRef<CSSPrimitiveValue> createValue(double value, CSSPrimitiveValue::UnitTypes);
    PassRef<CSSPrimitiveValue> createValue(const String& value, CSSPrimitiveValue::UnitTypes type) { return CSSPrimitiveValue::create(value, type); }
    PassRef<CSSPrimitiveValue> createValue(const Length& value, const RenderStyle* style) { return CSSPrimitiveValue::create(value, style); }
    PassRef<CSSPrimitiveValue> createValue(const LengthSize& value) { return CSSPrimitiveValue::create(value); }
    template<typename T> static PassRef<CSSPrimitiveValue> createValue(T value) { return CSSPrimitiveValue::create(value); }

    void drain();

private:
    CSSValuePool();

    Ref<CSSInheritedValue> m_inheritedValue;
    Ref<CSSInitialValue> m_implicitInitialValue;
    Ref<CSSInitialValue> m_explicitInitialValue;

    RefPtr<CSSPrimitiveValue> m_identifierValueCache[numCSSValueKeywords];

    typedef HashMap<unsigned, RefPtr<CSSPrimitiveValue>> ColorValueCache;
    ColorValueCache m_colorValueCache;
    Ref<CSSPrimitiveValue> m_colorTransparent;
    Ref<CSSPrimitiveValue> m_colorWhite;
    Ref<CSSPrimitiveValue> m_colorBlack;

    static const int maximumCacheableIntegerValue = 255;

    RefPtr<CSSPrimitiveValue> m_pixelValueCache[maximumCacheableIntegerValue + 1];
    RefPtr<CSSPrimitiveValue> m_percentValueCache[maximumCacheableIntegerValue + 1];
    RefPtr<CSSPrimitiveValue> m_numberValueCache[maximumCacheableIntegerValue + 1];

    typedef HashMap<AtomicString, RefPtr<CSSValueList>> FontFaceValueCache;
    FontFaceValueCache m_fontFaceValueCache;

    typedef HashMap<String, RefPtr<CSSPrimitiveValue>> FontFamilyValueCache;
    FontFamilyValueCache m_fontFamilyValueCache;

    friend CSSValuePool& cssValuePool();
};

CSSValuePool& cssValuePool() PURE_FUNCTION;

}

#endif
