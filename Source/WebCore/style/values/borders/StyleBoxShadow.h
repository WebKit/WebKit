/*
 * Copyright (C) 2024-2025 Samuel Weinig <sam@webkit.org>
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

#include "CSSBoxShadow.h"
#include "CSSPrimitiveNumeric.h"
#include "StyleColor.h"
#include "StylePrimitiveNumericTypes.h"
#include "StyleShadow.h"

namespace WebCore {
namespace Style {

struct BoxShadow {
    Color color;
    SpaceSeparatedPoint<Length<>> location;
    Length<CSS::Nonnegative> blur;
    Length<> spread;
    std::optional<CSS::Keyword::Inset> inset;
    bool isWebkitBoxShadow;

    bool operator==(const BoxShadow&) const = default;
};

template<size_t I> const auto& get(const BoxShadow& value)
{
    if constexpr (!I)
        return value.color;
    else if constexpr (I == 1)
        return value.location;
    else if constexpr (I == 2)
        return value.blur;
    else if constexpr (I == 3)
        return value.spread;
    else if constexpr (I == 4)
        return value.inset;
}

// <box-shadow-list> = <single-box-shadow>#
using BoxShadowList = ShadowList<BoxShadow>;

// <'box-shadow'> = none | <box-shadow-list>
// https://www.w3.org/TR/css-backgrounds-3/#propdef-box-shadow
using BoxShadows = Shadows<BoxShadow>;

// MARK: - Conversions

template<> struct ToCSS<BoxShadow> { auto operator()(const BoxShadow&, const RenderStyle&) -> CSS::BoxShadow; };
template<> struct ToStyle<CSS::BoxShadow> { auto operator()(const CSS::BoxShadow&, const BuilderState&) -> BoxShadow; };

// `BoxShadowList` is special-cased to return a `CSSBoxShadowPropertyValue`.
template<> struct CSSValueCreation<BoxShadowList> { Ref<CSSValue> operator()(CSSValuePool&, const RenderStyle&, const BoxShadowList&); };
template<> struct CSSValueConversion<BoxShadows> { auto operator()(BuilderState&, const CSSValue&) -> BoxShadows; };

// MARK: - Serialization

template<> struct Serialize<BoxShadowList> { void operator()(StringBuilder&, const CSS::SerializationContext&, const RenderStyle&, const BoxShadowList&); };

// MARK: - Blending

template<> struct Blending<BoxShadow> {
    auto blend(const BoxShadow&, const BoxShadow&, const RenderStyle&, const RenderStyle&, const BlendingContext&) -> BoxShadow;
};

// MARK: - Shadow-specific Interfaces

inline ShadowStyle shadowStyle(const BoxShadow& shadow)
{
    return shadow.inset.has_value() ? ShadowStyle::Inset : ShadowStyle::Normal;
}

inline bool isInset(const BoxShadow& shadow)
{
    return shadow.inset.has_value();
}

inline LayoutUnit paintingSpread(const BoxShadow& shadow)
{
    return LayoutUnit { shadow.spread.value };
}

} // namespace Style
} // namespace WebCore

DEFINE_SPACE_SEPARATED_TUPLE_LIKE_CONFORMANCE(WebCore::Style::BoxShadow, 5)
template<> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::Style::BoxShadows> = true;
