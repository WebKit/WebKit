/*
 * Copyright (C) 2022-2023 Apple Inc. All rights reserved.
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

#include "CSSValueKeywords.h"
#include <wtf/text/MakeString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

CSSBackgroundRepeatValue::CSSBackgroundRepeatValue(CSSValueID xValue, CSSValueID yValue)
    : CSSValue(BackgroundRepeatClass)
    , m_xValue(xValue)
    , m_yValue(yValue)
{
}

Ref<CSSBackgroundRepeatValue> CSSBackgroundRepeatValue::create(CSSValueID repeatXValue, CSSValueID repeatYValue)
{
    return adoptRef(*new CSSBackgroundRepeatValue(repeatXValue, repeatYValue));
}

template<typename Maker> decltype(auto) CSSBackgroundRepeatValue::serialize(Maker&& maker) const
{
    // background-repeat/mask-repeat behave a little like a shorthand, but `repeat no-repeat` is transformed to `repeat-x`.
    if (m_xValue != m_yValue) {
        if (m_xValue == CSSValueRepeat && m_yValue == CSSValueNoRepeat)
            return maker(nameString(CSSValueRepeatX));
        if (m_xValue == CSSValueNoRepeat && m_yValue == CSSValueRepeat)
            return maker(nameString(CSSValueRepeatY));
        return maker(nameString(m_xValue), ' ', nameString(m_yValue));
    }
    return maker(nameString(m_xValue));
}

String CSSBackgroundRepeatValue::customCSSText() const
{
    return serialize(SerializeUsingMakeString { });
}

void CSSBackgroundRepeatValue::customCSSText(StringBuilder& builder) const
{
    serialize(SerializeUsingStringBuilder { builder });
}

bool CSSBackgroundRepeatValue::equals(const CSSBackgroundRepeatValue& other) const
{
    return m_xValue == other.m_xValue && m_yValue == other.m_yValue;
}

} // namespace WebCore
