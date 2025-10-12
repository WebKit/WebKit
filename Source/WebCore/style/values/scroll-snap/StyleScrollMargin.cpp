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
#include "StyleBuilderState.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto CSSValueConversion<ScrollMarginEdge>::operator()(BuilderState& state, const CSSValue& value) -> ScrollMarginEdge
{
    return ScrollMarginEdge { toStyleFromCSSValue<Length<>>(state, value) };
}

// MARK: - Evaluation

auto Evaluation<ScrollMarginEdge, LayoutUnit>::operator()(const ScrollMarginEdge& edge, LayoutUnit, ZoomNeeded token) -> LayoutUnit
{
    return evaluate<LayoutUnit>(edge.m_value, token);
}

auto Evaluation<ScrollMarginEdge, LayoutUnit>::operator()(const ScrollMarginEdge& edge, ZoomNeeded token) -> LayoutUnit
{
    return evaluate<LayoutUnit>(edge.m_value, token);
}

auto Evaluation<ScrollMarginEdge, float>::operator()(const ScrollMarginEdge& edge, float, ZoomNeeded token) -> float
{
    return evaluate<float>(edge.m_value, token);
}

auto Evaluation<ScrollMarginEdge, float>::operator()(const ScrollMarginEdge& edge, ZoomNeeded token) -> float
{
    return evaluate<float>(edge.m_value, token);
}

// MARK: - Extent

LayoutBoxExtent extentForRect(const ScrollMarginBox& margin, const LayoutRect& rect)
{
    return LayoutBoxExtent {
        evaluate<LayoutUnit>(margin.top(), rect.height(), ZoomNeeded { }),
        evaluate<LayoutUnit>(margin.right(), rect.width(), ZoomNeeded { }),
        evaluate<LayoutUnit>(margin.bottom(), rect.height(), ZoomNeeded { }),
        evaluate<LayoutUnit>(margin.left(), rect.width(), ZoomNeeded { }),
    };
}

} // namespace Style
} // namespace WebCore
