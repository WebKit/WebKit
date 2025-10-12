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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StyleTextDecorationThickness.h"

#include "AnimationUtilities.h"
#include "CSSPrimitiveValue.h"
#include "FontMetrics.h"
#include "RenderStyleInlines.h"
#include "StyleBuilderChecking.h"
#include "StyleLengthWrapper+Blending.h"
#include "StyleLengthWrapper+CSSValueConversion.h"

namespace WebCore {
namespace Style {

static constexpr float textDecorationBaseFontSize = 16;

float TextDecorationThickness::resolve(const RenderStyle& style) const
{
    return WTF::switchOn(m_value,
        [&](const CSS::Keyword::Auto&) {
            return style.computedFontSize() / textDecorationBaseFontSize;
        },
        [&](const CSS::Keyword::FromFont&) {
            return style.metricsOfPrimaryFont().underlineThickness().value_or(0);
        },
        [&](const TextDecorationThicknessLength& length) {
            return Style::evaluate<float>(length, style.computedFontSize(), Style::ZoomNeeded { });
        }
    );
}

float TextDecorationThickness::resolve(float fontSize, const FontMetrics& metrics) const
{
    return WTF::switchOn(m_value,
        [&](const CSS::Keyword::Auto&) {
            return fontSize / textDecorationBaseFontSize;
        },
        [&](const CSS::Keyword::FromFont&) {
            return metrics.underlineThickness().value_or(0);
        },
        [&](const TextDecorationThicknessLength& length) {
            return Style::evaluate<float>(length, fontSize, Style::ZoomNeeded { });
        }
    );
}

// MARK: - Conversion

auto CSSValueConversion<TextDecorationThickness>::operator()(BuilderState& state, const CSSValue& value) -> TextDecorationThickness
{
    RefPtr primitiveValue = requiredDowncast<CSSPrimitiveValue>(state, value);
    if (!primitiveValue)
        return CSS::Keyword::Auto { };

    if (primitiveValue->isValueID()) {
        switch (primitiveValue->valueID()) {
        case CSSValueAuto:
            return CSS::Keyword::Auto { };
        case CSSValueFromFont:
            return CSS::Keyword::FromFont { };
        default:
            break;
        }

        state.setCurrentPropertyInvalidAtComputedValueTime();
        return CSS::Keyword::Auto { };
    }

    return toStyleFromCSSValue<TextDecorationThicknessLength>(state, *primitiveValue);
}

// MARK: - Blending

auto Blending<TextDecorationThickness>::canBlend(const TextDecorationThickness& a, const TextDecorationThickness& b, const RenderStyle& aStyle, const RenderStyle& bStyle) -> bool
{
    if (a.isAuto() || b.isAuto())
        return false;
    return a.resolve(aStyle) != b.resolve(bStyle);
}

auto Blending<TextDecorationThickness>::blend(const TextDecorationThickness& a, const TextDecorationThickness& b, const RenderStyle& aStyle, const RenderStyle& bStyle, const BlendingContext& context) -> TextDecorationThickness
{
    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1.0);
        return context.progress ? b : a;
    }

    return TextDecorationThicknessLength { TextDecorationThicknessLength::Fixed { WebCore::blend(a.resolve(aStyle), b.resolve(bStyle), context) } };
}

} // namespace Style
} // namespace WebCore
