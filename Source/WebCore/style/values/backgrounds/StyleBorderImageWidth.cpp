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
#include "StyleBorderImageWidth.h"

#include "AnimationUtilities.h"
#include "CSSBorderImageWidthValue.h"
#include "CSSPrimitiveValue.h"
#include "StyleBuilderChecking.h"
#include "StyleLengthWrapper+Blending.h"
#include "StyleLengthWrapper+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+CSSValueCreation.h"

namespace WebCore {
namespace Style {

using namespace CSS::Literals;

// MARK: - Conversion

static BorderImageWidthValue convertBorderImageWidthValue(BuilderState& state, const CSSValue& value)
{
    RefPtr primitiveValue = requiredDowncast<CSSPrimitiveValue>(state, value);
    if (!primitiveValue)
        return BorderImageWidthValue { 1_css_number };

    if (primitiveValue->valueID() == CSSValueAuto)
        return CSS::Keyword::Auto { };

    if (primitiveValue->isNumber())
        return toStyleFromCSSValue<BorderImageWidthValue::Number>(state, *primitiveValue);
    return toStyleFromCSSValue<BorderImageWidthValue::LengthPercentage>(state, *primitiveValue);
}

auto CSSValueConversion<BorderImageWidth>::operator()(BuilderState& state, const CSSValue& value) -> BorderImageWidth
{
    if (RefPtr widthValue = dynamicDowncast<CSSBorderImageWidthValue>(value)) {
        auto& widths = widthValue->widths();
        return BorderImageWidth {
            .values = {
                convertBorderImageWidthValue(state, widths.top()),
                convertBorderImageWidthValue(state, widths.right()),
                convertBorderImageWidthValue(state, widths.bottom()),
                convertBorderImageWidthValue(state, widths.left()),
            },
            .legacyWebkitBorderImage = widthValue->overridesBorderWidths(),
        };
    }

    // Values coming from CSS Typed OM may not have been converted to a CSSBorderImageWidthValue.
    auto widthValue = convertBorderImageWidthValue(state, value);
    return BorderImageWidth {
        .values = {
            widthValue,
            widthValue,
            widthValue,
            widthValue,
        },
        .legacyWebkitBorderImage = false
    };
}

auto CSSValueCreation<BorderImageWidth>::operator()(CSSValuePool& pool, const RenderStyle& style, const BorderImageWidth& value) -> Ref<CSSValue>
{
    return CSSBorderImageWidthValue::create({
        createCSSValue(pool, style, value.values.top()),
        createCSSValue(pool, style, value.values.right()),
        createCSSValue(pool, style, value.values.bottom()),
        createCSSValue(pool, style, value.values.left()),
    }, value.legacyWebkitBorderImage);
}

// MARK: - Blending

inline auto Blending<BorderImageWidthValue>::canBlend(const BorderImageWidthValue& a, const BorderImageWidthValue& b) -> bool
{
    if (a.hasSameType(b))
        return true;

    return (a.isLengthPercentage() || a.isNumber())
        && (b.isLengthPercentage() || b.isNumber())
        && a.isNumber() == b.isNumber();
}

inline auto Blending<BorderImageWidthValue>::requiresInterpolationForAccumulativeIteration(const BorderImageWidthValue& a, const BorderImageWidthValue& b) -> bool
{
    return a.isCalculated() || b.isCalculated() || !a.hasSameType(b);
}

inline auto Blending<BorderImageWidthValue>::blend(const BorderImageWidthValue& a, const BorderImageWidthValue& b, const BlendingContext& context) -> BorderImageWidthValue
{
    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1);
        return context.progress ? b : a;
    }

    return WTF::visit(WTF::makeVisitor(
        [&]<typename T>(const T& a, const T& b) -> BorderImageWidthValue {
            return Style::blend(a, b, context);
        },
        [&](const auto&, const auto&) -> BorderImageWidthValue {
            RELEASE_ASSERT_NOT_REACHED();
        }
    ), a.m_value, b.m_value);
}

auto Blending<BorderImageWidth>::canBlend(const BorderImageWidth& a, const BorderImageWidth& b) -> bool
{
    if (a.legacyWebkitBorderImage != b.legacyWebkitBorderImage)
        return false;

    return Style::canBlend(a.values.top(),    b.values.top())
        && Style::canBlend(a.values.right(),  b.values.right())
        && Style::canBlend(a.values.bottom(), b.values.bottom())
        && Style::canBlend(a.values.left(),   b.values.left());
}

auto Blending<BorderImageWidth>::requiresInterpolationForAccumulativeIteration(const BorderImageWidth& a, const BorderImageWidth& b) -> bool
{
    return Style::requiresInterpolationForAccumulativeIteration(a.values.top(),    b.values.top())
        && Style::requiresInterpolationForAccumulativeIteration(a.values.right(),  b.values.right())
        && Style::requiresInterpolationForAccumulativeIteration(a.values.bottom(), b.values.bottom())
        && Style::requiresInterpolationForAccumulativeIteration(a.values.left(),   b.values.left());
}

auto Blending<BorderImageWidth>::blend(const BorderImageWidth& a, const BorderImageWidth& b, const BlendingContext& context) -> BorderImageWidth
{
    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1);
        return context.progress ? b : a;
    }

    return BorderImageWidth {
        .values = {
            Style::blend(a.values.top(),     b.values.top(), context),
            Style::blend(a.values.right(),   b.values.right(), context),
            Style::blend(a.values.bottom(),  b.values.bottom(), context),
            Style::blend(a.values.left(),    b.values.left(), context),
        },
        .legacyWebkitBorderImage = (!context.progress || !context.isDiscrete ? a : b).legacyWebkitBorderImage,
    };
}

} // namespace Style
} // namespace WebCore
