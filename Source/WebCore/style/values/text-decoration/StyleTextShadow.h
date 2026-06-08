/*
 * Copyright (C) 2024-2025 Samuel Weinig <sam@webkit.org>
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/CSSTextShadow.h>
#include <WebCore/StyleColor.h>
#include <WebCore/StylePrimitiveNumericTypes.h>
#include <WebCore/StyleShadow.h>

namespace WebCore {
namespace Style {

struct TextShadow {
    Color color;
    SpaceSeparatedPoint<Length<CSS::AllUnzoomed>> location;
    Length<CSS::NonnegativeUnzoomed> blur;

    bool operator==(const TextShadow&) const = default;
};

template<size_t I> const auto& get(const TextShadow& value)
{
    if constexpr (!I)
        return value.color;
    else if constexpr (I == 1)
        return value.location;
    else if constexpr (I == 2)
        return value.blur;
}

// <text-shadow-list> = <single-text-shadow>#
using TextShadowList = ShadowList<TextShadow>;

// <'text-shadow'> = none | <text-list>
// https://www.w3.org/TR/css-text-decor-3/#propdef-text-shadow
using TextShadows = Shadows<TextShadow>;

// MARK: - Conversions

template<> struct ToCSS<TextShadow> { auto operator()(const TextShadow&, const RenderStyle&) -> CSS::TextShadow; };
template<> struct ToStyle<CSS::TextShadow> { auto operator()(const CSS::TextShadow&, const BuilderState&) -> TextShadow; };

// `TextShadowList` is special-cased to return a `CSSTextShadowPropertyValue`.
template<> struct CSSValueCreation<TextShadowList> { Ref<CSSValue> operator()(CSSValuePool&, const RenderStyle&, const TextShadowList&); };
template<> struct CSSValueConversion<TextShadows> { auto operator()(BuilderState&, const CSSValue&) -> TextShadows; };

// MARK: - Serialization

template<> struct Serialize<TextShadowList> { void operator()(StringBuilder&, const CSS::SerializationContext&, const RenderStyle&, const TextShadowList&); };

// MARK: - Blending

template<> struct Blending<TextShadow> {
    auto blend(const TextShadow&, const TextShadow&, const RenderStyle&, const RenderStyle&, const BlendingContext&) -> TextShadow;
};

template<> struct Blending<TextShadows> {
    auto canBlend(const TextShadows&, const TextShadows&, CompositeOperation) -> bool;
    constexpr auto requiresInterpolationForAccumulativeIteration(const TextShadows&, const TextShadows&) -> bool { return true; }
    auto blend(const TextShadows&, const TextShadows&, const RenderStyle&, const RenderStyle&, const BlendingContext&) -> TextShadows;
};

// MARK: - Shadow-specific Interfaces

constexpr ShadowStyle shadowStyle(const TextShadow&)
{
    return ShadowStyle::Normal;
}

constexpr bool isInset(const TextShadow&)
{
    return false;
}

constexpr LayoutUnit paintingSpread(const TextShadow&, const Style::ZoomFactor&)
{
    return LayoutUnit();
}

} // namespace Style
} // namespace WebCore

DEFINE_SPACE_SEPARATED_TUPLE_LIKE_CONFORMANCE(WebCore::Style::TextShadow, 3)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::TextShadows)
