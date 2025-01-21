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

#include "CSSScrollPaddingEdgeValue.h"
#include "LayoutRect.h"
#include "StyleBuilderState.h"
#include "StylePrimitiveNumericOrKeyword+CSSValueConversions.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"

namespace WebCore {
namespace Style {

ScrollPaddingEdge CSSValueConversions<ScrollPaddingEdge>::operator()(const CSSValue& value, const BuilderState& state)
{
    if (RefPtr edge = dynamicDowncast<CSSScrollPaddingEdgeValue>(value))
        return toStyle(edge->edge(), state);

    return { convertFromCSSValue<ScrollPaddingEdge::Wrapped>(value, state) };
}

double Evaluation<ScrollPaddingEdge>::operator()(const ScrollPaddingEdge& edge, double referenceLength)
{
    return WTF::switchOn(edge.value,
        [&](const CSS::Keyword::Auto&) -> double {
            return 0;
        },
        [&](const auto& length) -> double {
            return evaluate(length, referenceLength);
        }
    );
}

float Evaluation<ScrollPaddingEdge>::operator()(const ScrollPaddingEdge& edge, float referenceLength)
{
    return WTF::switchOn(edge.value,
        [&](const CSS::Keyword::Auto&) -> float {
            return 0;
        },
        [&](const auto& length) -> float {
            return evaluate(length, referenceLength);
        }
    );
}

LayoutUnit Evaluation<ScrollPaddingEdge>::operator()(const ScrollPaddingEdge& edge, LayoutUnit referenceLength)
{
    return WTF::switchOn(edge.value,
        [&](const CSS::Keyword::Auto&) -> LayoutUnit {
            return 0_lu;
        },
        [&](const auto& length) -> LayoutUnit {
            return evaluate(length, referenceLength);
        }
    );
}

LayoutBoxExtent extentForRect(const ScrollPadding& padding, const LayoutRect& rect)
{
    return LayoutBoxExtent {
        evaluate(padding.top(), rect.height()),
        evaluate(padding.right(), rect.width()),
        evaluate(padding.bottom(), rect.height()),
        evaluate(padding.left(), rect.width())
    };
}

} // namespace Style
} // namespace WebCore
