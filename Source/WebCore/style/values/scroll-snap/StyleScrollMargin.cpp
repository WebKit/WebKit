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
#include "StyleScrollMargin.h"

#include "LayoutRect.h"
#include "StyleBuilderConverter.h"
#include "StyleBuilderState.h"
#include "StyleExtractorConverter.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

LayoutUnit Evaluation<ScrollMarginEdge>::operator()(const ScrollMarginEdge& edge, LayoutUnit referenceLength)
{
    switch (edge.m_value.type()) {
    case LengthType::Fixed:
        return LayoutUnit(edge.m_value.value());

    case LengthType::Percent:
        return LayoutUnit(static_cast<float>(referenceLength * edge.m_value.percent() / 100.0f));

    case LengthType::Calculated:
        return LayoutUnit(edge.m_value.nonNanCalculatedValue(referenceLength));

    case LengthType::FillAvailable:
    case LengthType::Auto:
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

float Evaluation<ScrollMarginEdge>::operator()(const ScrollMarginEdge& edge, float referenceLength)
{
    switch (edge.m_value.type()) {
    case LengthType::Fixed:
        return edge.m_value.value();

    case LengthType::Percent:
        return referenceLength * edge.m_value.percent() / 100.0f;

    case LengthType::Calculated:
        return edge.m_value.nonNanCalculatedValue(referenceLength);

    case LengthType::FillAvailable:
    case LengthType::Auto:
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
    return 0.0f;
}

auto CSSValueConversion<ScrollMarginEdge>::operator()(BuilderState& state, const CSSValue& value) -> ScrollMarginEdge
{
    return ScrollMarginEdge { BuilderConverter::convertLength<ScrollMarginEdge::Fixed::range>(state, value) };
}

LayoutBoxExtent extentForRect(const ScrollMarginBox& margin, const LayoutRect& rect)
{
    return LayoutBoxExtent {
        Style::evaluate(margin.top(), rect.height()),
        Style::evaluate(margin.right(), rect.width()),
        Style::evaluate(margin.bottom(), rect.height()),
        Style::evaluate(margin.left(), rect.width()),
    };
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const ScrollMarginEdge& value)
{
    return ts << value.m_value;
}

} // namespace Style
} // namespace WebCore
