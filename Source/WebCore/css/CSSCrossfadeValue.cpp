/*
 * Copyright (C) 2011-2021 Apple Inc.  All rights reserved.
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSCrossfadeValue.h"

#include "StyleBuilderState.h"
#include "StyleCrossfadeImage.h"

namespace WebCore {

inline CSSCrossfadeValue::CSSCrossfadeValue(Ref<CSSValue>&& fromValueOrNone, Ref<CSSValue>&& toValueOrNone, Ref<CSSPrimitiveValue>&& percentageValue, bool isPrefixed)
    : CSSValue { CrossfadeClass }
    , m_fromValueOrNone { WTFMove(fromValueOrNone) }
    , m_toValueOrNone { WTFMove(toValueOrNone) }
    , m_percentageValue { WTFMove(percentageValue) }
    , m_isPrefixed { isPrefixed }
{
}

Ref<CSSCrossfadeValue> CSSCrossfadeValue::create(Ref<CSSValue>&& fromValueOrNone, Ref<CSSValue>&& toValueOrNone, Ref<CSSPrimitiveValue>&& percentageValue, bool isPrefixed)
{
    return adoptRef(*new CSSCrossfadeValue(WTFMove(fromValueOrNone), WTFMove(toValueOrNone), WTFMove(percentageValue), isPrefixed));
}

CSSCrossfadeValue::~CSSCrossfadeValue() = default;

bool CSSCrossfadeValue::equals(const CSSCrossfadeValue& other) const
{
    return equalInputImages(other) && compareCSSValue(m_percentageValue, other.m_percentageValue);
}

bool CSSCrossfadeValue::equalInputImages(const CSSCrossfadeValue& other) const
{
    return compareCSSValue(m_fromValueOrNone, other.m_fromValueOrNone) && compareCSSValue(m_toValueOrNone, other.m_toValueOrNone);
}

String CSSCrossfadeValue::customCSSText() const
{
    return makeString(m_isPrefixed ? "-webkit-" : "", "cross-fade(", m_fromValueOrNone->cssText(), ", ", m_toValueOrNone->cssText(), ", ", m_percentageValue->cssText(), ')');
}

RefPtr<StyleImage> CSSCrossfadeValue::createStyleImage(Style::BuilderState& state) const
{
    return StyleCrossfadeImage::create(state.createStyleImage(m_fromValueOrNone), state.createStyleImage(m_toValueOrNone), m_percentageValue->doubleValue(), m_isPrefixed);
}

} // namespace WebCore
