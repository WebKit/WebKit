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
#include "StyleFlexBasis.h"

#include "CSSPrimitiveNumericTypes+CSSValueCreation.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include "StyleSizing+Blending.h"
#include "StyleSizing+CSSValueConversions.h"
#include "StyleSizing+Evaluation.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

// MARK: - From CSSValue

FlexBasis CSSValueConversions<FlexBasis>::operator()(const CSSValue& value, const BuilderState& state)
{
    return convertFromCSSValueForSizeType<FlexBasis>(value, state);
}

// MARK: - Evaluation

double Evaluation<FlexBasis>::operator()(const FlexBasis& size, double referenceLength)
{
    return evaluateForSizeType(size, referenceLength);
}

float Evaluation<FlexBasis>::operator()(const FlexBasis& size, float referenceLength)
{
    return evaluateForSizeType(size, referenceLength);
}

LayoutUnit Evaluation<FlexBasis>::operator()(const FlexBasis& size, LayoutUnit referenceLength)
{
    return evaluateForSizeType(size, referenceLength);
}

LayoutUnit evaluateMinimum(const FlexBasis& size, LayoutUnit maximumValue)
{
    return evaluateMinimumForSizeType(size, maximumValue);
}

// MARK: - Blending

auto Blending<FlexBasis>::canBlend(const FlexBasis& from, const FlexBasis& to) -> bool
{
    return canBlendSizeType(from, to);
}

auto Blending<FlexBasis>::blend(const FlexBasis& from, const FlexBasis& to, const BlendingContext& context) -> FlexBasis
{
    return blendSizeType(from, to, context);
}

// MARK: - TextStream

TextStream& operator<<(TextStream& ts, FlexBasis size)
{
    WTF::switchOn(size,
        [&](const FlexBasis::Fixed& fixed) {
            ts << TextStream::FormatNumberRespectingIntegers(fixed.value) << "px";
        },
        [&](const FlexBasis::Percentage& percentage) {
            ts << TextStream::FormatNumberRespectingIntegers(percentage.value) << "%";
        },
        [&](const FlexBasis::Calc& calc) {
            ts << calc.protectedCalculation();
        },
        [&]<CSSValueID Id>(const Constant<Id>& keyword) {
            ts << nameLiteralForSerialization(keyword.value);
        }
    );

    return ts;
}

} // namespace Style

namespace CSS {

// MARK: - To CSSValue

Ref<CSSValue> CSSValueCreation<Style::FlexBasis>::operator()(const Style::FlexBasis& value, const RenderStyle& style)
{
    return WTF::switchOn(value, [&](const auto& alternative) -> Ref<CSSValue> { return createCSSValue(Style::toCSS(alternative, style)); });
}

} // namespace CSS
} // namespace WebCore
