/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
#include "StyleScrollPadding.h"

#include "ComputedStyleExtractor.h"
#include "LayoutRect.h"
#include "StyleBuilderConverter.h"
#include "StyleBuilderState.h"

namespace WebCore {
namespace Style {

LayoutUnit ScrollPaddingEdge::evaluate(LayoutUnit referenceLength) const
{
    switch (m_value.type()) {
    case LengthType::Fixed:
        return LayoutUnit(m_value.value());

    case LengthType::Percent:
        return LayoutUnit(static_cast<float>(referenceLength * m_value.percent() / 100.0f));

    case LengthType::Calculated:
        return LayoutUnit(m_value.nonNanCalculatedValue(referenceLength));

    case LengthType::Auto:
            return 0_lu;

    case LengthType::FillAvailable:
    case LengthType::Normal:
    case LengthType::Content:
    case LengthType::Relative:
    case LengthType::Intrinsic:
    case LengthType::MinIntrinsic:
    case LengthType::MinContent:
    case LengthType::MaxContent:
    case LengthType::FitContent:
    case LengthType::Undefined:
        break;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return 0_lu;
}

float ScrollPaddingEdge::evaluate(float referenceLength) const
{
    switch (m_value.type()) {
    case LengthType::Fixed:
        return m_value.value();

    case LengthType::Percent:
        return referenceLength * m_value.percent() / 100.0f;

    case LengthType::Calculated:
        return m_value.nonNanCalculatedValue(referenceLength);

    case LengthType::Auto:
            return 0;

    case LengthType::FillAvailable:
    case LengthType::Normal:
    case LengthType::Content:
    case LengthType::Relative:
    case LengthType::Intrinsic:
    case LengthType::MinIntrinsic:
    case LengthType::MinContent:
    case LengthType::MaxContent:
    case LengthType::FitContent:
    case LengthType::Undefined:
        break;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return 0;
}

Ref<CSSValue> ScrollPaddingEdge::toCSS(const RenderStyle& style) const
{
    return ComputedStyleExtractor::zoomAdjustedPixelValueForLength(m_value, style);
}

ScrollPaddingEdge scrollPaddingEdgeFromCSSValue(const CSSValue& value, BuilderState& state)
{
    if (value.valueID() == CSSValueAuto)
        return ScrollPaddingEdge { CSS::Keyword::Auto { } };
    return ScrollPaddingEdge { BuilderConverter::convertLength(state, value) };
}

LayoutBoxExtent extentForRect(const ScrollPadding& padding, const LayoutRect& rect)
{
    return LayoutBoxExtent {
        Style::evaluate(padding.top(), rect.height()),
        Style::evaluate(padding.right(), rect.width()),
        Style::evaluate(padding.bottom(), rect.height()),
        Style::evaluate(padding.left(), rect.width()),
    };
}

} // namespace Style
} // namespace WebCore
