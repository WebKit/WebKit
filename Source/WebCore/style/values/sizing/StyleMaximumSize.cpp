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
#include "StyleMaximumSize.h"

#include "CSSPrimitiveNumericTypes+CSSValueCreation.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include "StyleSizing+Blending.h"
#include "StyleSizing+CSSValueConversions.h"
#include "StyleSizing+Evaluation.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

// MARK: - Conversion

MaximumSize CSSValueConversions<MaximumSize>::operator()(BuilderState& state, const CSSValue& value)
{
    return convertFromCSSValueForSizeType<MaximumSize>(state, value);
}

Ref<CSSValue> CSSValueCreation<MaximumSize>::operator()(const MaximumSize& value, const RenderStyle& style)
{
    return WTF::switchOn(value, [&](const auto& alternative) -> Ref<CSSValue> {
        return CSS::createCSSValue(Style::toCSS(alternative, style));
    });
}

// MARK: - Evaluation

double Evaluation<MaximumSize>::operator()(const MaximumSize& size, double referenceLength)
{
    return evaluateForSizeType(size, referenceLength);
}

float Evaluation<MaximumSize>::operator()(const MaximumSize& size, float referenceLength)
{
    return evaluateForSizeType(size, referenceLength);
}

LayoutUnit Evaluation<MaximumSize>::operator()(const MaximumSize& size, LayoutUnit referenceLength)
{
    return evaluateForSizeType(size, referenceLength);
}

LayoutUnit evaluateMinimum(const MaximumSize& size, LayoutUnit maximumValue)
{
    return evaluateMinimumForSizeType(size, maximumValue);
}

// MARK: - Blending

auto Blending<MaximumSize>::canBlend(const MaximumSize& from, const MaximumSize& to) -> bool
{
    return canBlendSizeType(from, to);
}

auto Blending<MaximumSize>::blend(const MaximumSize& from, const MaximumSize& to, const BlendingContext& context) -> MaximumSize
{
    return blendSizeType(from, to, context);
}

// MARK: - TextStream

TextStream& operator<<(TextStream& ts, MaximumSize size)
{
    WTF::switchOn(size,
        [&](const MaximumSize::Fixed& fixed) {
            ts << TextStream::FormatNumberRespectingIntegers(fixed.value) << "px";
        },
        [&](const MaximumSize::Percentage& percentage) {
            ts << TextStream::FormatNumberRespectingIntegers(percentage.value) << "%";
        },
        [&](const MaximumSize::Calc& calc) {
            ts << calc.protectedCalculation();
        },
        [&]<CSSValueID Id>(const Constant<Id>& keyword) {
            ts << nameLiteralForSerialization(keyword.value);
        }
    );

    return ts;
}

} // namespace Style
} // namespace WebCore
