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
#include "StyleMinimumSize.h"

#include "CSSPrimitiveNumericTypes+CSSValueCreation.h"
#include "StylePreferredSize.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include "StyleSizing+Blending.h"
#include "StyleSizing+CSSValueConversions.h"
#include "StyleSizing+Evaluation.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

MinimumSize::MinimumSize(PreferredSize&& other)
    : m_value(WTFMove(other.m_value))
{
}

MinimumSize::MinimumSize(const PreferredSize& other)
    : m_value(other.m_value)
{
}

// MARK: - Conversion

MinimumSize CSSValueConversions<MinimumSize>::operator()(BuilderState& state, const CSSValue& value)
{
    return convertFromCSSValueForSizeType<MinimumSize>(state, value);
}

Ref<CSSValue> CSSValueCreation<MinimumSize>::operator()(const MinimumSize& value, const RenderStyle& style)
{
    return WTF::switchOn(value, [&](const auto& alternative) -> Ref<CSSValue> {
        return CSS::createCSSValue(Style::toCSS(alternative, style));
    });
}

// MARK: - Evaluation

double Evaluation<MinimumSize>::operator()(const MinimumSize& size, double referenceLength)
{
    return evaluateForSizeType(size, referenceLength);
}

float Evaluation<MinimumSize>::operator()(const MinimumSize& size, float referenceLength)
{
    return evaluateForSizeType(size, referenceLength);
}

LayoutUnit Evaluation<MinimumSize>::operator()(const MinimumSize& size, LayoutUnit referenceLength)
{
    return evaluateForSizeType(size, referenceLength);
}

LayoutUnit evaluateMinimum(const MinimumSize& size, LayoutUnit maximumValue)
{
    return evaluateMinimumForSizeType(size, maximumValue);
}

// MARK: - Blending

auto Blending<MinimumSize>::canBlend(const MinimumSize& from, const MinimumSize& to) -> bool
{
    return canBlendSizeType(from, to);
}

auto Blending<MinimumSize>::blend(const MinimumSize& from, const MinimumSize& to, const BlendingContext& context) -> MinimumSize
{
    return blendSizeType(from, to, context);
}

// MARK: - TextStream

TextStream& operator<<(TextStream& ts, MinimumSize size)
{
    WTF::switchOn(size,
        [&](const MinimumSize::Fixed& fixed) {
            ts << TextStream::FormatNumberRespectingIntegers(fixed.value) << "px";
        },
        [&](const MinimumSize::Percentage& percentage) {
            ts << TextStream::FormatNumberRespectingIntegers(percentage.value) << "%";
        },
        [&](const MinimumSize::Calc& calc) {
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
