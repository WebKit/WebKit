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
#include "StyleFontStyle.h"

#include "AnimationUtilities.h"
#include "CSSFontStyleWithAngleValue.h"
#include "CSSPropertyParserConsumer+Font.h"
#include "StyleBuilderChecking.h"
#include "StylePrimitiveKeyword+CSSValueCreation.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+Conversions.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto CSSValueConversion<FontStyle>::operator()(BuilderState& state, const CSSValue& value) -> FontStyle
{
    if (RefPtr fontStyleValue = dynamicDowncast<CSSFontStyleWithAngleValue>(value))
        return toStyle(fontStyleValue->obliqueAngle(), state);

    RefPtr primitiveValue = requiredDowncast<CSSPrimitiveValue>(state, value);
    if (!primitiveValue)
        return CSS::Keyword::Normal { };

    switch (auto valueID = primitiveValue->valueID(); valueID) {
    case CSSValueNormal:
        return CSS::Keyword::Normal { };
    case CSSValueItalic:
        return CSS::Keyword::Italic { };
    case CSSValueOblique:
        return CSS::Keyword::Oblique { };
    default:
        if (CSSPropertyParserHelpers::isSystemFontShorthand(valueID))
            return CSS::Keyword::Normal { };

        state.setCurrentPropertyInvalidAtComputedValueTime();
        return CSS::Keyword::Normal { };
    }
}

auto CSSValueCreation<FontStyle>::operator()(CSSValuePool& pool, const RenderStyle& style, const FontStyle& value) -> Ref<CSSValue>
{
    if (!value.platformSlope() || !*value.platformSlope())
        return createCSSValue(pool, style, CSS::Keyword::Normal { });

    if (*value.platformSlope() == italicValue()) {
        if (value.platformAxis() == FontStyleAxis::ital)
            return createCSSValue(pool, style, CSS::Keyword::Italic { });
        return createCSSValue(pool, style, CSS::Keyword::Oblique { });
    }

    return CSSFontStyleWithAngleValue::create(toCSS(FontStyle::Angle { static_cast<float>(*value.platformSlope()) }, style));
}

// MARK: - Blending

auto Blending<FontStyle>::canBlend(const FontStyle& a, const FontStyle& b) -> bool
{
    return a.platformAxis() == FontStyleAxis::slnt && b.platformAxis() == FontStyleAxis::slnt;
}

auto Blending<FontStyle>::blend(const FontStyle& a, const FontStyle& b, const BlendingContext& context) -> FontStyle
{
    if (context.isDiscrete)
        return context.progress < 0.5 ? a : b;

    if (!a.platformSlope() && !b.platformSlope())
        return CSS::Keyword::Normal { };

    return Style::blend(a.angle().value_or(FontStyle::Angle { 0 }), b.angle().value_or(FontStyle::Angle { 0 }), context);
}

} // namespace Style
} // namespace WebCore
