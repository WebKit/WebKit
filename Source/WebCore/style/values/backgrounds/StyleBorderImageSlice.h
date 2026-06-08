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

#pragma once

#include <WebCore/StylePrimitiveNumeric.h>
#include <WebCore/StylePrimitiveNumericTypes.h>

namespace WebCore {

namespace CSS {
struct BorderImageSlice;
}

namespace Style {

// <'border-image-slice'> = [<number [0,∞]> | <percentage [0,∞]>]{1,4} && fill?
// https://drafts.csswg.org/css-backgrounds/#propdef-border-image-slice
struct BorderImageSlice {
    using Value = NumberOrPercentage<CSS::Nonnegative, CSS::Nonnegative, float>;
    using Edges = MinimallySerializingSpaceSeparatedRectEdges<Value>;

    Edges values { Value::Percentage { 100 } };
    std::optional<CSS::Keyword::Fill> fill { };

    BorderImageSlice(Edges values, std::optional<CSS::Keyword::Fill> fill = { })
        : values { WTF::move(values) }
        , fill { fill }
    {
    }
    BorderImageSlice(Value top, Value right, Value bottom, Value left, std::optional<CSS::Keyword::Fill> fill = { })
        : values { top, right, bottom, left }
        , fill { fill }
    {
    }
    BorderImageSlice(Value value, std::optional<CSS::Keyword::Fill> fill = { })
        : values { value }
        , fill { fill }
    {
    }
    BorderImageSlice(Value::Number number, std::optional<CSS::Keyword::Fill> fill = { })
        : values { number }
        , fill { fill }
    {
    }
    BorderImageSlice(CSS::ValueLiteral<CSS::NumberUnit::Number> literal, std::optional<CSS::Keyword::Fill> fill = { })
        : values { Value::Number { literal } }
        , fill { fill }
    {
    }
    BorderImageSlice(Value::Percentage percentage, std::optional<CSS::Keyword::Fill> fill = { })
        : values { percentage }
        , fill { fill }
    {
    }
    BorderImageSlice(CSS::ValueLiteral<CSS::PercentageUnit::Percentage> literal, std::optional<CSS::Keyword::Fill> fill = { })
        : values { Value::Percentage { literal } }
        , fill { fill }
    {
    }

    bool operator==(const BorderImageSlice&) const = default;
};
template<size_t I> const auto& get(const BorderImageSlice& value)
{
    if constexpr (!I)
        return value.values;
    else if constexpr (I == 1)
        return value.fill;
}

// MARK: - Conversion

template<> struct ToCSS<BorderImageSlice> { auto operator()(const BorderImageSlice&, const RenderStyle&) -> CSS::BorderImageSlice; };
template<> struct ToStyle<CSS::BorderImageSlice> { auto operator()(const CSS::BorderImageSlice&, const BuilderState&) -> BorderImageSlice; };

template<> struct CSSValueConversion<BorderImageSlice> { auto operator()(BuilderState&, const CSSValue&) -> BorderImageSlice; };
template<> struct CSSValueCreation<BorderImageSlice> { auto operator()(CSSValuePool&, const RenderStyle&, const BorderImageSlice&) -> Ref<CSSValue>; };

// MARK: - Blending

template<> struct Blending<BorderImageSlice> {
    auto canBlend(const BorderImageSlice&, const BorderImageSlice&) -> bool;
    auto requiresInterpolationForAccumulativeIteration(const BorderImageSlice&, const BorderImageSlice&) -> bool;
    auto blend(const BorderImageSlice&, const BorderImageSlice&, const BlendingContext&) -> BorderImageSlice;
};

} // namespace Style
} // namespace WebCore

DEFINE_SPACE_SEPARATED_TUPLE_LIKE_CONFORMANCE(WebCore::Style::BorderImageSlice, 2)
