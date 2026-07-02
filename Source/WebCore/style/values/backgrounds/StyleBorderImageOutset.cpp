/*
 * Copyright (C) 2025-2026 Samuel Weinig <sam@webkit.org>
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
#include "StyleBorderImageOutset.h"

#include "AnimationUtilities.h"
#include "CSSBorderImageOutsetValue.h"
#include "CSSPrimitiveValue.h"
#include "StyleBuilderChecking.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+CSSValueCreation.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

DEFINE_TYPE_MAPPING(CSS::BorderImageOutset::Value, BorderImageOutset::Value);

auto ToCSS<BorderImageOutset>::operator()(const BorderImageOutset& value, const Style::ComputedStyle& style) -> CSS::BorderImageOutset
{
    return { toCSS(value.values, style) };
}

auto ToStyle<CSS::BorderImageOutset>::operator()(const CSS::BorderImageOutset& value, const BuilderState& state) -> BorderImageOutset
{
    return { toStyle(value.values, state) };
}

auto CSSValueConversion<BorderImageOutset>::operator()(BuilderState& state, const CSSValue& value) -> BorderImageOutset
{
    using namespace CSS::Literals;

    if (RefPtr borderImageOutsetValue = dynamicDowncast<CSSBorderImageOutsetValue>(value))
        return toStyle(borderImageOutsetValue->outsets(), state);

    // Values coming from CSS Typed OM may not have been converted to a CSSBorderImageOutsetValue.
    RefPtr primitiveValue = requiredDowncast<CSSPrimitiveValue>(state, value);
    if (!primitiveValue)
        return BorderImageOutsetValue { 0_css_number };

    if (primitiveValue->isNumber())
        return toStyleFromCSSValue<BorderImageOutsetValue::Number>(state, *primitiveValue);
    return toStyleFromCSSValue<BorderImageOutsetValue::Length>(state, *primitiveValue);
}

auto CSSValueCreation<BorderImageOutset>::operator()(CSSValuePool&, const Style::ComputedStyle& style, const BorderImageOutset& value) -> Ref<CSSValue>
{
    return CSSBorderImageOutsetValue::create(toCSS(value, style));
}

// MARK: - Blending

inline auto Blending<BorderImageOutsetValue>::canBlend(const BorderImageOutsetValue& a, const BorderImageOutsetValue& b) -> bool
{
    return a.hasSameType(b);
}

inline auto Blending<BorderImageOutsetValue>::requiresInterpolationForAccumulativeIteration(const BorderImageOutsetValue& a, const BorderImageOutsetValue& b) -> bool
{
    return !a.hasSameType(b);
}

inline auto Blending<BorderImageOutsetValue>::blend(const BorderImageOutsetValue& a, const BorderImageOutsetValue& b, const BlendingContext& context) -> BorderImageOutsetValue
{
    using namespace CSS::Literals;

    if (!a.hasSameType(b))
        return BorderImageOutsetValue { 0_css_px };

    if (!context.progress && context.isReplace())
        return a;
    if (context.progress == 1 && context.isReplace())
        return b;

    return WTF::visit(WTF::makeVisitor(
        [&]<typename T>(const T& a, const T& b) -> BorderImageOutsetValue {
            return Style::blend(a, b, context);
        },
        [&](const auto&, const auto&) -> BorderImageOutsetValue {
            RELEASE_ASSERT_NOT_REACHED();
        }
    ), a.m_value, b.m_value);
}

auto Blending<BorderImageOutset>::canBlend(const BorderImageOutset& a, const BorderImageOutset& b) -> bool
{
    return Style::canBlend(a.values.top(),    b.values.top())
        && Style::canBlend(a.values.right(),  b.values.right())
        && Style::canBlend(a.values.bottom(), b.values.bottom())
        && Style::canBlend(a.values.left(),   b.values.left());
}

auto Blending<BorderImageOutset>::requiresInterpolationForAccumulativeIteration(const BorderImageOutset& a, const BorderImageOutset& b) -> bool
{
    return Style::requiresInterpolationForAccumulativeIteration(a.values.top(),    b.values.top())
        && Style::requiresInterpolationForAccumulativeIteration(a.values.right(),  b.values.right())
        && Style::requiresInterpolationForAccumulativeIteration(a.values.bottom(), b.values.bottom())
        && Style::requiresInterpolationForAccumulativeIteration(a.values.left(),   b.values.left());
}

auto Blending<BorderImageOutset>::blend(const BorderImageOutset& a, const BorderImageOutset& b, const BlendingContext& context) -> BorderImageOutset
{
    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1);
        return context.progress ? b : a;
    }

    return BorderImageOutset {
        Style::blend(a.values.top(),     b.values.top(), context),
        Style::blend(a.values.right(),   b.values.right(), context),
        Style::blend(a.values.bottom(),  b.values.bottom(), context),
        Style::blend(a.values.left(),    b.values.left(), context),
    };
}

} // namespace Style
} // namespace WebCore
