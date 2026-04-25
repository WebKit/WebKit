/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#pragma once

#include <WebCore/StyleBlurFunction.h>
#include <WebCore/StyleBrightnessFunction.h>
#include <WebCore/StyleContrastFunction.h>
#include <WebCore/StyleDropShadowFunction.h>
#include <WebCore/StyleFilterReference.h>
#include <WebCore/StyleGrayscaleFunction.h>
#include <WebCore/StyleHueRotateFunction.h>
#include <WebCore/StyleInvertFunction.h>
#include <WebCore/StyleOpacityFunction.h>
#include <WebCore/StyleSaturateFunction.h>
#include <WebCore/StyleSepiaFunction.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

class FilterOperations;

namespace CSS {
struct Filter;
struct FilterValue;
using FilterValueList = SpaceSeparatedVector<FilterValue>;
}

namespace Style {

struct ZoomFactor;

// Any <filter-function> or a reference to filter via <url>.
// https://drafts.fxtf.org/filter-effects/#typedef-filter-function
using FilterValueKind = Variant<
    BlurFunction,
    BrightnessFunction,
    ContrastFunction,
    DropShadowFunction,
    GrayscaleFunction,
    HueRotateFunction,
    InvertFunction,
    OpacityFunction,
    SaturateFunction,
    SepiaFunction,
    FilterReference
>;
struct FilterValue {
    FilterValueKind value;

    template<typename T>
        requires std::constructible_from<FilterValueKind, T>
    FilterValue(T&& value)
        : value(std::forward<T>(value))
    {
    }

    FORWARD_VARIANT_FUNCTIONS(FilterValue, value)

    bool operator==(const FilterValue&) const = default;
};
DEFINE_TYPE_MAPPING(CSS::FilterValue, FilterValue)

// <filter-value-list> = [ <filter-function> | <url> ]+
// https://drafts.fxtf.org/filter-effects/#typedef-filter-value-list
using FilterValueList = SpaceSeparatedFixedVector<FilterValue>;

// <'filter'> = none | <filter-value-list>
// https://drafts.fxtf.org/filter-effects/#propdef-filter
struct Filter : ListOrNone<FilterValueList> {
    friend struct Blending<Filter>;

    using ListOrNone<FilterValueList>::ListOrNone;

    // True if any filter requires a repaint when `currentColor` changes.
    bool hasFilterThatRequiresRepaintForCurrentColorChange() const;
    // True if any filter can cause the the alpha channel of any pixel to change.
    bool hasFilterThatAffectsOpacity() const;
    // True if any filter can cause the value of one pixel to affect the value of another pixel, such as blur.
    bool hasFilterThatMovesPixels() const;
    // True if any filter should not be allowed to work on content that is not available from this security origin.
    bool hasFilterThatShouldBeRestrictedBySecurityOrigin() const;

    template<typename>
    bool hasFilterOfType() const;

    bool hasReferenceFilter() const;
    bool NODELETE isReferenceFilter() const;

    IntOutsets calculateOutsets(ZoomFactor) const;
};
DEFINE_TYPE_MAPPING(CSS::Filter, Filter)

template<typename T> bool Filter::hasFilterOfType() const
{
    return std::ranges::any_of(*this, [](auto& filterValue) { return WTF::holdsAlternative<T>(filterValue); });
}

// MARK: - Conversion

template<> struct ToCSS<FilterValueList> { auto operator()(const FilterValueList&, const RenderStyle&) -> CSS::FilterValueList; };
template<> struct ToStyle<CSS::FilterValueList> { auto operator()(const CSS::FilterValueList&, const BuilderState&) -> FilterValueList; };

template<> struct CSSValueConversion<Filter> { auto operator()(BuilderState&, const CSSValue&) -> Filter; };
template<> struct CSSValueCreation<Filter> { auto operator()(CSSValuePool&, const RenderStyle&, const Filter&) -> Ref<CSSValue>; };

// MARK: - Blending

template<> struct Blending<Filter> {
    auto canBlend(const Filter&, const Filter&, CompositeOperation) -> bool;
    constexpr auto requiresInterpolationForAccumulativeIteration(const Filter&, const Filter&) -> bool { return true; }
    auto blend(const Filter&, const Filter&, const RenderStyle&, const RenderStyle&, const BlendingContext&) -> Filter;
};

// MARK: - Platform

template<> struct ToPlatform<FilterValue> { auto operator()(const FilterValue&, const RenderStyle&) -> Ref<FilterOperation>; };
template<> struct ToPlatform<Filter> { auto operator()(const Filter&, const RenderStyle&) -> FilterOperations; };

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::FilterValue)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::Filter)
