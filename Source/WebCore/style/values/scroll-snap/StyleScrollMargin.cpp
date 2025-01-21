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

#include "CSSScrollMarginEdgeValue.h"
#include "LayoutRect.h"
#include "StyleBuilderState.h"
#include "StylePrimitiveNumericTypes+CSSValueConversions.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"

namespace WebCore {
namespace Style {

ScrollMarginEdge CSSValueConversions<ScrollMarginEdge>::operator()(const CSSValue& value, const BuilderState& state)
{
    if (RefPtr edge = dynamicDowncast<CSSScrollMarginEdgeValue>(value))
        return toStyle(edge->edge(), state);

    return { convertFromCSSValue<ScrollMarginEdge::Wrapped>(value, state) };
}

LayoutBoxExtent extentForRect(const ScrollMargin& margin, const LayoutRect& rect)
{
    return LayoutBoxExtent {
        Style::evaluate(margin.top(), rect.height()),
        Style::evaluate(margin.right(), rect.width()),
        Style::evaluate(margin.bottom(), rect.height()),
        Style::evaluate(margin.left(), rect.width())
    };
}

} // namespace Style
} // namespace WebCore
