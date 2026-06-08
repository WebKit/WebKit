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
#include "StyleWebKitBoxReflect.h"

#include "CSSKeywordValue.h"
#include "CSSWebkitBoxReflectValue.h"
#include "StyleBuilderChecking.h"
#include "StyleKeyword+Serialization.h"
#include "StyleLengthWrapper+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+CSSValueConversion.h"
#include "StylePrimitiveNumericTypes+CSSValueCreation.h"
#include "StylePrimitiveNumericTypes+Serialization.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

template<> struct ToCSS<WebkitBoxReflection::Direction> { auto operator()(const WebkitBoxReflection::Direction&, const RenderStyle&) -> CSS::WebkitBoxReflection::Direction; };
template<> struct ToStyle<CSS::WebkitBoxReflection::Direction> { auto operator()(const CSS::WebkitBoxReflection::Direction&, const BuilderState&) -> WebkitBoxReflection::Direction; };

template<> struct ToCSS<WebkitBoxReflection> { auto operator()(const WebkitBoxReflection&, const RenderStyle&) -> CSS::WebkitBoxReflection; };
template<> struct ToStyle<CSS::WebkitBoxReflection> { auto operator()(const CSS::WebkitBoxReflection&, const BuilderState&) -> WebkitBoxReflection; };

auto ToCSS<WebkitBoxReflection::Direction>::operator()(const WebkitBoxReflection::Direction& value, const RenderStyle&) -> CSS::WebkitBoxReflection::Direction
{
    switch (value) {
    case WebkitBoxReflection::Direction::Above: return CSS::Keyword::Above { };
    case WebkitBoxReflection::Direction::Below: return CSS::Keyword::Below { };
    case WebkitBoxReflection::Direction::Left:  return CSS::Keyword::Left { };
    case WebkitBoxReflection::Direction::Right: return CSS::Keyword::Right { };
    }
    RELEASE_ASSERT_NOT_REACHED();
}

auto ToStyle<CSS::WebkitBoxReflection::Direction>::operator()(const CSS::WebkitBoxReflection::Direction& value, const BuilderState&) -> WebkitBoxReflection::Direction
{
    return WTF::switchOn(value,
        [](CSS::Keyword::Above) { return WebkitBoxReflection::Direction::Above; },
        [](CSS::Keyword::Below) { return WebkitBoxReflection::Direction::Below; },
        [](CSS::Keyword::Left)  { return WebkitBoxReflection::Direction::Left; },
        [](CSS::Keyword::Right) { return WebkitBoxReflection::Direction::Right; }
    );
}

auto ToCSS<WebkitBoxReflection>::operator()(const WebkitBoxReflection& value, const RenderStyle& style) -> CSS::WebkitBoxReflection
{
    auto convertOffset = [](auto& offset, auto& style) {
        // FIXME: Support direct conversion from Style::LengthWrapperBase<LengthPercentage<...>> to CSS::LengthPercentage<...>.
        return offset.switchOnUsingSpecified(
            [&](const auto& specified) -> CSS::WebkitBoxReflection::Offset {
                return toCSS(specified, style);
            }
        );
    };

    return {
        .direction = toCSS(value.direction, style),
        .offset = convertOffset(value.offset, style),
        .mask = toCSS(value.mask, style),
    };
}

auto ToStyle<CSS::WebkitBoxReflection>::operator()(const CSS::WebkitBoxReflection& value, const BuilderState& state) -> WebkitBoxReflection
{
    auto convertOffset = [](auto& offset, auto& state) {
        // FIXME: Support direct conversion from CSS::LengthPercentage<...> to Style::LengthWrapperBase<LengthPercentage<...>>.
        return WebkitBoxReflection::Offset { toStyle(offset, state) };
    };

    return {
        .direction = toStyle(value.direction, state),
        .offset = convertOffset(value.offset, state),
        .mask = toStyle(value.mask, state, MaskBorderSliceOverride::AlwaysFill),
    };
}

auto CSSValueConversion<WebkitBoxReflect>::operator()(BuilderState& state, const CSSValue& value) -> WebkitBoxReflect
{
    using namespace CSS::Literals;

    if (auto* reflectValue = dynamicDowncast<CSSWebkitBoxReflectValue>(value))
        return toStyle(reflectValue->reflect(), state);

    // Values coming from CSS Typed OM may not have been converted to a CSSWebkitBoxReflectValue.

    RefPtr keywordValue = requiredDowncast<CSSKeywordValue>(state, value);
    if (!keywordValue)
        return CSS::Keyword::None { };

    switch (keywordValue->valueID()) {
    case CSSValueNone:
        return CSS::Keyword::None { };
    case CSSValueAbove:
        return WebkitBoxReflection { .direction = ReflectionDirection::Above, .offset = 0_css_px, .mask = MaskBorder { } };
    case CSSValueBelow:
        return WebkitBoxReflection { .direction = ReflectionDirection::Below, .offset = 0_css_px, .mask = MaskBorder { } };
    case CSSValueLeft:
        return WebkitBoxReflection { .direction = ReflectionDirection::Left,  .offset = 0_css_px, .mask = MaskBorder { } };
    case CSSValueRight:
        return WebkitBoxReflection { .direction = ReflectionDirection::Right, .offset = 0_css_px, .mask = MaskBorder { } };
    default:
        state.setCurrentPropertyInvalidAtComputedValueTime();
        return CSS::Keyword::None { };
    }
}

Ref<CSSValue> CSSValueCreation<WebkitBoxReflect>::operator()(CSSValuePool&, const RenderStyle& style, const WebkitBoxReflect& value)
{
    return CSSWebkitBoxReflectValue::create(toCSS(value, style));
}

// MARK: - Serialization

void Serialize<WebkitBoxReflection>::operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const WebkitBoxReflection& value)
{
    auto serializeMask = [&](auto& mask) {
        if (mask.source().isNone())
            serializationForCSS(builder, context, style, CSS::Keyword::None { });
        else
            serializationForCSS(builder, context, style, mask);
    };

    serializationForCSS(builder, context, style, value.direction);
    builder.append(' ');
    serializationForCSS(builder, context, style, value.offset);
    builder.append(' ');
    serializeMask(value.mask);
}

} // namespace Style
} // namespace WebCore
