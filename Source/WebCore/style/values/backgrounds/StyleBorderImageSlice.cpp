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
#include "StyleBorderImageSlice.h"

#include "AnimationUtilities.h"
#include "CSSBorderImageSliceValue.h"
#include "CSSPrimitiveValue.h"
#include "StyleBuilderChecking.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+CSSValueCreation.h"

namespace WebCore {
namespace Style {

using namespace CSS::Literals;

// MARK: - Conversion

static BorderImageSliceValue convertBorderImageSliceValue(BuilderState& state, const CSSValue& value)
{
    RefPtr primitiveValue = requiredDowncast<CSSPrimitiveValue>(state, value);
    if (!primitiveValue)
        return BorderImageSliceValue { 100_css_percentage };

    if (primitiveValue->isNumber())
        return toStyleFromCSSValue<BorderImageSliceValue::Number>(state, *primitiveValue);
    return toStyleFromCSSValue<BorderImageSliceValue::Percentage>(state, *primitiveValue);
}

auto CSSValueConversion<BorderImageSlice>::operator()(BuilderState& state, const CSSValue& value) -> BorderImageSlice
{
    if (RefPtr sliceValue = dynamicDowncast<CSSBorderImageSliceValue>(value)) {
        auto& slices = sliceValue->slices();
        return BorderImageSlice {
            .values = {
                convertBorderImageSliceValue(state, slices.top()),
                convertBorderImageSliceValue(state, slices.right()),
                convertBorderImageSliceValue(state, slices.bottom()),
                convertBorderImageSliceValue(state, slices.left()),
            },
            .fill = sliceValue->fill() ? std::make_optional(CSS::Keyword::Fill { }) : std::nullopt,
        };
    }

    // Values coming from CSS Typed OM may not have been converted to a CSSBorderImageSliceValue.
    auto sliceValue = convertBorderImageSliceValue(state, value);
    return BorderImageSlice {
        .values = {
            sliceValue,
            sliceValue,
            sliceValue,
            sliceValue,
        },
        .fill = std::nullopt
    };
}

auto CSSValueCreation<BorderImageSlice>::operator()(CSSValuePool& pool, const RenderStyle& style, const BorderImageSlice& value) -> Ref<CSSValue>
{
    return CSSBorderImageSliceValue::create({
        createCSSValue(pool, style, value.values.top()),
        createCSSValue(pool, style, value.values.right()),
        createCSSValue(pool, style, value.values.bottom()),
        createCSSValue(pool, style, value.values.left()),
    }, value.fill.has_value());
}

// MARK: - Blending

inline auto Blending<BorderImageSliceValue>::canBlend(const BorderImageSliceValue& a, const BorderImageSliceValue& b) -> bool
{
    return a.hasSameType(b);
}

inline auto Blending<BorderImageSliceValue>::requiresInterpolationForAccumulativeIteration(const BorderImageSliceValue& a, const BorderImageSliceValue& b) -> bool
{
    return !a.hasSameType(b);
}

inline auto Blending<BorderImageSliceValue>::blend(const BorderImageSliceValue& a, const BorderImageSliceValue& b, const BlendingContext& context) -> BorderImageSliceValue
{
    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1);
        return context.progress ? b : a;
    }

    return WTF::visit(WTF::makeVisitor(
        [&]<typename T>(const T& a, const T& b) -> BorderImageSliceValue {
            return Style::blend(a, b, context);
        },
        [&](const auto&, const auto&) -> BorderImageSliceValue {
            RELEASE_ASSERT_NOT_REACHED();
        }
    ), a.m_value, b.m_value);
}

auto Blending<BorderImageSlice>::canBlend(const BorderImageSlice& a, const BorderImageSlice& b) -> bool
{
    if (a.fill != b.fill)
        return false;

    return Style::canBlend(a.values.top(),    b.values.top())
        && Style::canBlend(a.values.right(),  b.values.right())
        && Style::canBlend(a.values.bottom(), b.values.bottom())
        && Style::canBlend(a.values.left(),   b.values.left());
}

auto Blending<BorderImageSlice>::requiresInterpolationForAccumulativeIteration(const BorderImageSlice& a, const BorderImageSlice& b) -> bool
{
    return Style::requiresInterpolationForAccumulativeIteration(a.values.top(),    b.values.top())
        && Style::requiresInterpolationForAccumulativeIteration(a.values.right(),  b.values.right())
        && Style::requiresInterpolationForAccumulativeIteration(a.values.bottom(), b.values.bottom())
        && Style::requiresInterpolationForAccumulativeIteration(a.values.left(),   b.values.left());
}

auto Blending<BorderImageSlice>::blend(const BorderImageSlice& a, const BorderImageSlice& b, const BlendingContext& context) -> BorderImageSlice
{
    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1);
        return context.progress ? b : a;
    }

    return BorderImageSlice {
        .values = {
            Style::blend(a.values.top(),     b.values.top(), context),
            Style::blend(a.values.right(),   b.values.right(), context),
            Style::blend(a.values.bottom(),  b.values.bottom(), context),
            Style::blend(a.values.left(),    b.values.left(), context),
        },
        .fill = (!context.progress || !context.isDiscrete ? a : b).fill,
    };
}

} // namespace Style
} // namespace WebCore
