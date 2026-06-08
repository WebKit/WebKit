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
#include "StyleMaskBorderWidth.h"

#include "AnimationUtilities.h"
#include "CSSMaskBorderWidthValue.h"
#include "CSSKeywordValueInlines.h"
#include "CSSPrimitiveValue.h"
#include "StyleBuilderChecking.h"
#include "StyleLengthWrapper+Blending.h"
#include "StyleLengthWrapper+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+CSSValueCreation.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

template<> struct ToCSS<MaskBorderWidth::Value> { auto operator()(const MaskBorderWidth::Value&, const RenderStyle&) -> CSS::MaskBorderWidth::Value; };
template<> struct ToStyle<CSS::MaskBorderWidth::Value> { auto operator()(const CSS::MaskBorderWidth::Value&, const BuilderState&) -> MaskBorderWidth::Value; };

auto ToCSS<MaskBorderWidth::Value>::operator()(const MaskBorderWidth::Value& value, const RenderStyle& style) -> CSS::MaskBorderWidth::Value
{
    return WTF::switchOn(value,
        [&](const CSS::Keyword::Auto& keyword) -> CSS::MaskBorderWidth::Value {
            return keyword;
        },
        [&](const MaskBorderWidth::Value::LengthPercentage& lengthPercentage) -> CSS::MaskBorderWidth::Value {
            // FIXME: Support direct conversion from Style::LengthWrapperBase<LengthPercentage<...>> to CSS::LengthPercentage<...>.
            return lengthPercentage.switchOnUsingSpecified(
                [&](const auto& specified) -> CSS::MaskBorderWidth::Value {
                    return toCSS(specified, style);
                }
            );
        },
        [&](const MaskBorderWidth::Value::Number& number) -> CSS::MaskBorderWidth::Value {
            return toCSS(number, style);
        }
    );
}

auto ToStyle<CSS::MaskBorderWidth::Value>::operator()(const CSS::MaskBorderWidth::Value& value, const BuilderState& state) -> MaskBorderWidth::Value
{
    return WTF::switchOn(value,
        [&](const CSS::Keyword::Auto& keyword) -> MaskBorderWidth::Value {
            return keyword;
        },
        [&](const CSS::MaskBorderWidth::Value::LengthPercentage& lengthPercentage) -> MaskBorderWidth::Value {
            // FIXME: Support direct conversion from CSS::LengthPercentage<...> to Style::LengthWrapperBase<LengthPercentage<...>>.
            return MaskBorderWidthValueLength { toStyle(lengthPercentage, state) };
        },
        [&](const CSS::MaskBorderWidth::Value::Number& number) -> MaskBorderWidth::Value {
            return toStyle(number, state);
        }
    );
}

auto ToCSS<MaskBorderWidth>::operator()(const MaskBorderWidth& value, const RenderStyle& style) -> CSS::MaskBorderWidth
{
    return { toCSS(value.values, style) };
}

auto ToStyle<CSS::MaskBorderWidth>::operator()(const CSS::MaskBorderWidth& value, const BuilderState& state) -> MaskBorderWidth
{
    return { toStyle(value.values, state) };
}

auto CSSValueConversion<MaskBorderWidth>::operator()(BuilderState& state, const CSSValue& value) -> MaskBorderWidth
{
    if (RefPtr widthValue = dynamicDowncast<CSSMaskBorderWidthValue>(value))
        return toStyle(widthValue->widths(), state);

    // Values coming from CSS Typed OM may not have been converted to a CSSMaskBorderWidthValue.
    if (isValueID(value, CSSValueAuto))
        return CSS::Keyword::Auto { };

    RefPtr primitiveValue = requiredDowncast<CSSPrimitiveValue>(state, value);
    if (!primitiveValue)
        return CSS::Keyword::Auto { };

    if (primitiveValue->isNumber())
        return toStyleFromCSSValue<MaskBorderWidthValue::Number>(state, *primitiveValue);
    return toStyleFromCSSValue<MaskBorderWidthValue::LengthPercentage>(state, *primitiveValue);
}

auto CSSValueCreation<MaskBorderWidth>::operator()(CSSValuePool&, const RenderStyle& style, const MaskBorderWidth& value) -> Ref<CSSValue>
{
    return CSSMaskBorderWidthValue::create(toCSS(value, style));
}

// MARK: - Blending

inline auto Blending<MaskBorderWidthValue>::canBlend(const MaskBorderWidthValue& a, const MaskBorderWidthValue& b) -> bool
{
    if (a.hasSameType(b))
        return true;

    return (a.isLengthPercentage() || a.isNumber())
        && (b.isLengthPercentage() || b.isNumber())
        && a.isNumber() == b.isNumber();
}

inline auto Blending<MaskBorderWidthValue>::requiresInterpolationForAccumulativeIteration(const MaskBorderWidthValue& a, const MaskBorderWidthValue& b) -> bool
{
    return a.isCalculated() || b.isCalculated() || !a.hasSameType(b);
}

inline auto Blending<MaskBorderWidthValue>::blend(const MaskBorderWidthValue& a, const MaskBorderWidthValue& b, const BlendingContext& context) -> MaskBorderWidthValue
{
    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1);
        return context.progress ? b : a;
    }

    return WTF::visit(WTF::makeVisitor(
        [&]<typename T>(const T& a, const T& b) -> MaskBorderWidthValue {
            return Style::blend(a, b, context);
        },
        [&](const auto&, const auto&) -> MaskBorderWidthValue {
            RELEASE_ASSERT_NOT_REACHED();
        }
    ), a.m_value, b.m_value);
}

auto Blending<MaskBorderWidth>::canBlend(const MaskBorderWidth& a, const MaskBorderWidth& b) -> bool
{
    return Style::canBlend(a.values.top(),    b.values.top())
        && Style::canBlend(a.values.right(),  b.values.right())
        && Style::canBlend(a.values.bottom(), b.values.bottom())
        && Style::canBlend(a.values.left(),   b.values.left());
}

auto Blending<MaskBorderWidth>::requiresInterpolationForAccumulativeIteration(const MaskBorderWidth& a, const MaskBorderWidth& b) -> bool
{
    return Style::requiresInterpolationForAccumulativeIteration(a.values.top(),    b.values.top())
        && Style::requiresInterpolationForAccumulativeIteration(a.values.right(),  b.values.right())
        && Style::requiresInterpolationForAccumulativeIteration(a.values.bottom(), b.values.bottom())
        && Style::requiresInterpolationForAccumulativeIteration(a.values.left(),   b.values.left());
}

auto Blending<MaskBorderWidth>::blend(const MaskBorderWidth& a, const MaskBorderWidth& b, const BlendingContext& context) -> MaskBorderWidth
{
    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1);
        return context.progress ? b : a;
    }

    return MaskBorderWidth {
        Style::blend(a.values.top(),     b.values.top(), context),
        Style::blend(a.values.right(),   b.values.right(), context),
        Style::blend(a.values.bottom(),  b.values.bottom(), context),
        Style::blend(a.values.left(),    b.values.left(), context),
    };
}

} // namespace Style
} // namespace WebCore
