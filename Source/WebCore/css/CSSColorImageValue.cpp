/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "CSSColorImageValue.h"

#include "StyleBuilderState.h"
#include "StyleColor.h"
#include "StyleColorImage.h"

namespace WebCore {

Ref<CSSColorImageValue> CSSColorImageValue::create(CSS::Color&& color)
{
    return adoptRef(*new CSSColorImageValue(WTF::move(color)));
}

CSSColorImageValue::CSSColorImageValue(CSS::Color&& color)
    : CSSValue { ClassType::ColorImage }
    , m_color { WTF::move(color) }
{
}

CSSColorImageValue::~CSSColorImageValue() = default;

String CSSColorImageValue::customCSSText(const CSS::SerializationContext& context) const
{
    return makeString("image("_s, CSS::serializationForCSS(context, m_color), ')');
}

bool CSSColorImageValue::equals(const CSSColorImageValue& other) const
{
    return m_color == other.m_color;
}

RefPtr<Style::Image> CSSColorImageValue::createStyleImage(const Style::BuilderState& state) const
{
    return Style::ColorImage::create(Style::toStyle(m_color, state));
}

IterationStatus CSSColorImageValue::customVisitChildren(NOESCAPE const Function<IterationStatus(CSSValue&)>& func) const
{
    return CSS::visitCSSValueChildren(func, m_color);
}

} // namespace WebCore
