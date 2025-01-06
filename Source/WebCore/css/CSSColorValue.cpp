/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSColorValue.h"

#include "CSSPrimitiveValue.h"

namespace WebCore {

Ref<CSSColorValue> CSSColorValue::create(CSS::Color color)
{
    return adoptRef(*new CSSColorValue(WTFMove(color)));
}

Ref<CSSColorValue> CSSColorValue::create(WebCore::Color color)
{
    return adoptRef(*new CSSColorValue(WTFMove(color)));
}

CSSColorValue::CSSColorValue(CSS::Color color)
    : CSSValue(ClassType::Color)
    , m_color(WTFMove(color))
{
}

CSSColorValue::CSSColorValue(WebCore::Color color)
    : CSSColorValue(CSS::Color { CSS::ResolvedColor { WTFMove(color) } })
{
}

CSSColorValue::CSSColorValue(StaticCSSValueTag, WebCore::Color color)
    : CSSColorValue(WTFMove(color))
{
    makeStatic();
}

WebCore::Color CSSColorValue::absoluteColor(const CSSValue& value)
{
    if (RefPtr color = dynamicDowncast<CSSColorValue>(value))
        return color->color().absoluteColor();

    if (auto valueID = value.valueID(); CSS::isAbsoluteColorKeyword(valueID))
        return CSS::colorFromAbsoluteKeyword(valueID);
    return { };
}

String CSSColorValue::customCSSText() const
{
    return CSS::serializationForCSS(m_color);
}

bool CSSColorValue::equals(const CSSColorValue& other) const
{
    return m_color == other.m_color;
}

IterationStatus CSSColorValue::customVisitChildren(const Function<IterationStatus(CSSValue&)>& func) const
{
    return CSS::visitCSSValueChildren(func, m_color);
}

} // namespace WebCore
