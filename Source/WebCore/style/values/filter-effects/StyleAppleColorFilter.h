/*
 * Copyright (C) 2024-2026 Samuel Weinig <sam@webkit.org>
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

#include <WebCore/StyleAppleInvertLightnessFunction.h>
#include <WebCore/StyleBrightnessFunction.h>
#include <WebCore/StyleContrastFunction.h>
#include <WebCore/StyleGrayscaleFunction.h>
#include <WebCore/StyleHueRotateFunction.h>
#include <WebCore/StyleInvertFunction.h>
#include <WebCore/StyleOpacityFunction.h>
#include <WebCore/StyleSaturateFunction.h>
#include <WebCore/StyleSepiaFunction.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

namespace CSS {
struct AppleColorFilter;
struct AppleColorFilterValue;
using AppleColorFilterValueList = SpaceSeparatedVector<AppleColorFilterValue>;
}

namespace Style {

// Any <apple-color-filter-function>.
// (Equivalent of https://drafts.fxtf.org/filter-effects/#typedef-filter-function)
using AppleColorFilterValueKind = Variant<
    AppleInvertLightnessFunction,
    BrightnessFunction,
    ContrastFunction,
    GrayscaleFunction,
    HueRotateFunction,
    InvertFunction,
    OpacityFunction,
    SaturateFunction,
    SepiaFunction
>;
struct AppleColorFilterValue {
    AppleColorFilterValueKind value;

    template<typename T>
        requires std::constructible_from<AppleColorFilterValueKind, T>
    AppleColorFilterValue(T&& value)
        : value(std::forward<T>(value))
    {
    }

    FORWARD_VARIANT_FUNCTIONS(AppleColorFilterValue, value)

    bool operator==(const AppleColorFilterValue&) const = default;
};
DEFINE_TYPE_MAPPING(CSS::AppleColorFilterValue, AppleColorFilterValue)

// <apple-color-filter-value-list> = [ <apple-color-filter-function> ]+
// (Equivalent of https://drafts.fxtf.org/filter-effects/#typedef-filter-value-list)
using AppleColorFilterValueList = SpaceSeparatedFixedVector<AppleColorFilterValue>;

// <'-apple-color-filter'> = none | <apple-color-filter-value-list>
// (Equivalent of https://drafts.fxtf.org/filter-effects/#propdef-filter)
struct AppleColorFilter : ListOrNone<AppleColorFilterValueList> {
    friend struct Blending<AppleColorFilter>;

    using ListOrNone<AppleColorFilterValueList>::ListOrNone;

    static const AppleColorFilter& none();

    bool transformColor(WebCore::Color&) const;
    bool inverseTransformColor(WebCore::Color&) const;

    template<typename>
    bool hasFilterOfType() const;
};
DEFINE_TYPE_MAPPING(CSS::AppleColorFilter, AppleColorFilter)

template<typename T> bool AppleColorFilter::hasFilterOfType() const
{
    return std::ranges::any_of(*this, [](auto& filterValue) { return WTF::holdsAlternative<T>(filterValue); });
}

// MARK: - Conversion

template<> struct ToCSS<AppleColorFilterValueList> { auto operator()(const AppleColorFilterValueList&, const RenderStyle&) -> CSS::AppleColorFilterValueList; };
template<> struct ToStyle<CSS::AppleColorFilterValueList> { auto operator()(const CSS::AppleColorFilterValueList&, const BuilderState&) -> AppleColorFilterValueList; };

template<> struct CSSValueConversion<AppleColorFilter> { auto operator()(BuilderState&, const CSSValue&) -> AppleColorFilter; };
template<> struct CSSValueCreation<AppleColorFilter> { auto operator()(CSSValuePool&, const RenderStyle&, const AppleColorFilter&) -> Ref<CSSValue>; };

// MARK: - Blending

template<> struct Blending<AppleColorFilter> {
    auto canBlend(const AppleColorFilter&, const AppleColorFilter&, CompositeOperation) -> bool;
    constexpr auto requiresInterpolationForAccumulativeIteration(const AppleColorFilter&, const AppleColorFilter&) -> bool { return true; }
    auto blend(const AppleColorFilter&, const AppleColorFilter&, const RenderStyle&, const RenderStyle&, const BlendingContext&) -> AppleColorFilter;
};

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::AppleColorFilterValue)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::AppleColorFilter)
