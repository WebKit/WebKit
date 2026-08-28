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
#include "StyleTextDecorationInset.h"

#include "StyleBuilderChecking.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"

namespace WebCore {
namespace Style {

using namespace CSS::Literals;

bool TextDecorationInset::hasPercentage() const
{
    auto pair = tryValue();
    return pair && (pair->first().isPercentOrCalculated() || pair->second().isPercentOrCalculated());
}

bool TextDecorationInset::hasNegativePercentage() const
{
    auto pair = tryValue();
    if (!pair)
        return false;
    auto isNegativePercentage = [](const auto& endpoint) {
        return endpoint.isPercentOrCalculated() && endpoint.isPossiblyNegative();
    };
    return isNegativePercentage(pair->first()) || isNegativePercentage(pair->second());
}

float TextDecorationInset::resolvedStart(const Style::ComputedStyle& style, float autoValue, float percentageBasis) const
{
    if (auto pair = tryValue())
        return Style::evaluate<float>(pair->first(), percentageBasis, style.usedZoomForLength());
    return autoValue;
}

float TextDecorationInset::resolvedEnd(const Style::ComputedStyle& style, float autoValue, float percentageBasis) const
{
    if (auto pair = tryValue())
        return Style::evaluate<float>(pair->second(), percentageBasis, style.usedZoomForLength());
    return autoValue;
}

float TextDecorationInset::outwardExtent(const Style::ComputedStyle& style, float percentageBasis) const
{
    auto pair = tryValue();
    if (!pair)
        return 0.f; // 'auto' only ever trims inward.

    auto outwardExtent = [&](const auto& endpoint) {
        return std::max(0.f, -Style::evaluate<float>(endpoint, percentageBasis, style.usedZoomForLength()));
    };
    return std::max(outwardExtent(pair->first()), outwardExtent(pair->second()));
}

// MARK: - Conversion

auto CSSValueConversion<TextDecorationInsetPair>::operator()(BuilderState& state, const CSSValue& value) -> TextDecorationInsetPair
{
    using LengthPercentage = Style::LengthPercentage<>;

    if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        auto length = toStyleFromCSSValue<LengthPercentage>(state, *primitiveValue);
        return { length, length };
    }

    auto pair = requiredPairDowncast<CSSPrimitiveValue>(state, value);
    if (!pair)
        return { 0_css_px, 0_css_px };

    return {
        toStyleFromCSSValue<LengthPercentage>(state, pair->first),
        toStyleFromCSSValue<LengthPercentage>(state, pair->second),
    };
}

} // namespace Style
} // namespace WebCore
