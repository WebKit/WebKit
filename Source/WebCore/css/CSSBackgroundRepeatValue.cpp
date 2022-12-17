/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "CSSBackgroundRepeatValue.h"

#include "Rect.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

CSSBackgroundRepeatValue::CSSBackgroundRepeatValue(Ref<CSSPrimitiveValue>&& repeatXValue, Ref<CSSPrimitiveValue>&& repeatYValue)
    : CSSValue(BackgroundRepeatClass)
    , m_xValue(WTFMove(repeatXValue))
    , m_yValue(WTFMove(repeatYValue))
{
}

CSSBackgroundRepeatValue::CSSBackgroundRepeatValue(CSSValueID repeatXValue, CSSValueID repeatYValue)
    : CSSValue(BackgroundRepeatClass)
    , m_xValue(CSSValuePool::singleton().createIdentifierValue(repeatXValue))
    , m_yValue(CSSValuePool::singleton().createIdentifierValue(repeatYValue))
{
}

String CSSBackgroundRepeatValue::customCSSText() const
{
    // background-repeat/mask-repeat behave a little like a shorthand, but `repeat no-repeat` is transformed to `repeat-x`.
    if (!compareCSSValue(m_xValue, m_yValue)) {
        if (m_xValue->valueID() == CSSValueRepeat && m_yValue->valueID() == CSSValueNoRepeat)
            return nameString(CSSValueRepeatX);
        if (m_xValue->valueID() == CSSValueNoRepeat && m_yValue->valueID() == CSSValueRepeat)
            return nameString(CSSValueRepeatY);
        return makeString(m_xValue->cssText(), ' ', m_yValue->cssText());
    }
    return m_xValue->cssText();
}

bool CSSBackgroundRepeatValue::equals(const CSSBackgroundRepeatValue& other) const
{
    return compareCSSValue(m_xValue, other.m_xValue) && compareCSSValue(m_yValue, other.m_yValue);
}

} // namespace WebCore
