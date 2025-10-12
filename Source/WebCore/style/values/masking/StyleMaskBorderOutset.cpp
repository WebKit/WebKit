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
#include "StyleMaskBorderOutset.h"

#include "AnimationUtilities.h"
#include "CSSPrimitiveValue.h"
#include "CSSQuadValue.h"
#include "StyleBuilderChecking.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+CSSValueCreation.h"

namespace WebCore {
namespace Style {

using namespace CSS::Literals;

// MARK: - Conversion

static MaskBorderOutsetValue convertMaskBorderOutsetValue(BuilderState& state, const CSSValue& value)
{
    RefPtr primitiveValue = requiredDowncast<CSSPrimitiveValue>(state, value);
    if (!primitiveValue)
        return MaskBorderOutsetValue { 0_css_number };

    if (primitiveValue->isNumber())
        return toStyleFromCSSValue<MaskBorderOutsetValue::Number>(state, *primitiveValue);
    return toStyleFromCSSValue<MaskBorderOutsetValue::Length>(state, *primitiveValue);
}

auto CSSValueConversion<MaskBorderOutset>::operator()(BuilderState& state, const CSSValue& value) -> MaskBorderOutset
{
    if (RefPtr quadValue = dynamicDowncast<CSSQuadValue>(value)) {
        auto& quad = quadValue->quad();
        return MaskBorderOutset {
            .values = {
                convertMaskBorderOutsetValue(state, quad.top()),
                convertMaskBorderOutsetValue(state, quad.right()),
                convertMaskBorderOutsetValue(state, quad.bottom()),
                convertMaskBorderOutsetValue(state, quad.left()),
            }
        };
    }

    // Values coming from CSS Typed OM may not have been converted to a Quad.
    auto outsetValue = convertMaskBorderOutsetValue(state, value);
    return MaskBorderOutset {
        .values = {
            outsetValue,
            outsetValue,
            outsetValue,
            outsetValue,
        }
    };
}

auto CSSValueCreation<MaskBorderOutset>::operator()(CSSValuePool& pool, const RenderStyle& style, const MaskBorderOutset& value) -> Ref<CSSValue>
{
    return CSSQuadValue::create({
        createCSSValue(pool, style, value.values.top()),
        createCSSValue(pool, style, value.values.right()),
        createCSSValue(pool, style, value.values.bottom()),
        createCSSValue(pool, style, value.values.left()),
    });
}

// MARK: - Blending

inline auto Blending<MaskBorderOutsetValue>::canBlend(const MaskBorderOutsetValue& a, const MaskBorderOutsetValue& b) -> bool
{
    return a.hasSameType(b);
}

inline auto Blending<MaskBorderOutsetValue>::requiresInterpolationForAccumulativeIteration(const MaskBorderOutsetValue& a, const MaskBorderOutsetValue& b) -> bool
{
    return !a.hasSameType(b);
}

inline auto Blending<MaskBorderOutsetValue>::blend(const MaskBorderOutsetValue& a, const MaskBorderOutsetValue& b, const BlendingContext& context) -> MaskBorderOutsetValue
{
    if (!a.hasSameType(b))
        return MaskBorderOutsetValue { 0_css_px };

    if (!context.progress && context.isReplace())
        return a;
    if (context.progress == 1 && context.isReplace())
        return b;

    return WTF::visit(WTF::makeVisitor(
        [&]<typename T>(const T& a, const T& b) -> MaskBorderOutsetValue {
            return Style::blend(a, b, context);
        },
        [&](const auto&, const auto&) -> MaskBorderOutsetValue {
            RELEASE_ASSERT_NOT_REACHED();
        }
    ), a.m_value, b.m_value);
}

auto Blending<MaskBorderOutset>::canBlend(const MaskBorderOutset& a, const MaskBorderOutset& b) -> bool
{
    return Style::canBlend(a.values.top(),    b.values.top())
        && Style::canBlend(a.values.right(),  b.values.right())
        && Style::canBlend(a.values.bottom(), b.values.bottom())
        && Style::canBlend(a.values.left(),   b.values.left());
}

auto Blending<MaskBorderOutset>::requiresInterpolationForAccumulativeIteration(const MaskBorderOutset& a, const MaskBorderOutset& b) -> bool
{
    return Style::requiresInterpolationForAccumulativeIteration(a.values.top(),    b.values.top())
        && Style::requiresInterpolationForAccumulativeIteration(a.values.right(),  b.values.right())
        && Style::requiresInterpolationForAccumulativeIteration(a.values.bottom(), b.values.bottom())
        && Style::requiresInterpolationForAccumulativeIteration(a.values.left(),   b.values.left());
}

auto Blending<MaskBorderOutset>::blend(const MaskBorderOutset& a, const MaskBorderOutset& b, const BlendingContext& context) -> MaskBorderOutset
{
    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1);
        return context.progress ? b : a;
    }

    return MaskBorderOutset {
        .values = {
            Style::blend(a.values.top(),     b.values.top(), context),
            Style::blend(a.values.right(),   b.values.right(), context),
            Style::blend(a.values.bottom(),  b.values.bottom(), context),
            Style::blend(a.values.left(),    b.values.left(), context),
        }
    };
}

} // namespace Style
} // namespace WebCore
