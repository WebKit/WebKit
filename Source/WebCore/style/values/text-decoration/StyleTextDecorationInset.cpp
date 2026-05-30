/*
 * Copyright (C) 2026 Alexsander Borges Damaceno <alexbdamac@gmail.com>. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StyleTextDecorationInset.h"

#include "AnimationUtilities.h"
#include "CSSKeywordValueInlines.h"
#include "StyleBuilderChecking.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleLengthWrapper+Blending.h"
#include "StyleLengthWrapper+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"

namespace WebCore {
namespace Style {

static constexpr float autoTextDecorationInset = 3.0f;

float TextDecorationInsetValue::resolve(const Style::ComputedStyle& style) const
{
    return WTF::switchOn(*this,
        [&](const Fixed& fixed) -> float {
            return Style::evaluate<float>(fixed, style.usedZoomForLength());
        },
        [&](const Calc& calc) -> float {
            return Style::evaluate<float>(calc, style.computedFontSize(), style.usedZoomForLength());
        },
        [&](const Percentage& percentage) -> float {
            return Style::evaluate<float>(percentage, style.computedFontSize());
        }
    );
}

std::pair<float, float> TextDecorationInset::resolve(const Style::ComputedStyle& style) const
{
    return WTF::switchOn(m_value,
        [](const CSS::Keyword::Auto&) -> std::pair<float, float> {
            return { 0, autoTextDecorationInset };
        },
        [&](const Value& value) -> std::pair<float, float> {
            return { value.first().resolve(style), value.second().resolve(style) };
        }
    );
}

auto CSSValueConversion<TextDecorationInset>::operator()(BuilderState& state, const CSSValue& value) -> TextDecorationInset
{
    if (isValueID(value, CSSValueAuto))
        return TextDecorationInset { CSS::Keyword::Auto { } };

    auto pair = requiredPairDowncast<CSSPrimitiveValue>(state, value);
    if (!pair)
        return TextDecorationInset { CSS::Keyword::Auto { } };

    auto start = toStyleFromCSSValue<TextDecorationInsetValue>(state, pair->first.get());
    auto end   = toStyleFromCSSValue<TextDecorationInsetValue>(state, pair->second.get());
    return TextDecorationInset { start, end };
}

// MARK: - Blending

auto Blending<TextDecorationInset>::canBlend(const TextDecorationInset& a, const TextDecorationInset& b, const Style::ComputedStyle& aStyle, const Style::ComputedStyle& bStyle) -> bool
{
    if (a.isAuto() || b.isAuto())
        return false;
    return a.resolve(aStyle) != b.resolve(bStyle);
}

auto Blending<TextDecorationInset>::blend(const TextDecorationInset& a, const TextDecorationInset& b, const Style::ComputedStyle& aStyle, const Style::ComputedStyle& bStyle, const BlendingContext& context) -> TextDecorationInset
{
    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1.0);
        return context.progress ? b : a;
    }

    auto [aStart, aEnd] = a.resolve(aStyle);
    auto [bStart, bEnd] = b.resolve(bStyle);
    TextDecorationInsetValue blendedStart { TextDecorationInsetValue::Fixed { WebCore::blend(aStart, bStart, context) } };
    TextDecorationInsetValue blendedEnd   { TextDecorationInsetValue::Fixed { WebCore::blend(aEnd,   bEnd,   context) } };
    return TextDecorationInset { blendedStart, blendedEnd };
}

} // namespace Style
} // namespace WebCore
