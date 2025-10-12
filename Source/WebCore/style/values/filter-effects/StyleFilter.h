/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include <WebCore/FilterOperations.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {

namespace CSS {
struct Filter;
struct FilterValue;
}

namespace Style {

// Any <filter-function> or a reference to filter via <url>.
// https://drafts.fxtf.org/filter-effects/#typedef-filter-function
struct FilterValue {
    FilterValue(Ref<FilterOperation> value) : value { WTFMove(value) } { }

    const FilterOperation& get() const { return value.get(); }
    const FilterOperation& platform() const { return value.get(); }
    const FilterOperation* operator->() const { return value.ptr(); }

    bool operator==(const FilterValue& other) const
    {
        return arePointingToEqualData(value, other.value);
    }

    Ref<FilterOperation> value;
};

// <filter-value-list> = [ <filter-function> | <url> ]+
// https://drafts.fxtf.org/filter-effects/#typedef-filter-value-list
using FilterValueList = SpaceSeparatedFixedVector<FilterValue>;

// <'filter'> = none | <filter-value-list>
// https://drafts.fxtf.org/filter-effects/#propdef-filter
struct Filter : ListOrNone<FilterValueList> {
    friend struct Blending<Filter>;

    using ListOrNone<FilterValueList>::ListOrNone;

    bool hasFilterThatRequiresRepaintForCurrentColorChange() const;

    bool hasFilterThatAffectsOpacity() const;
    bool hasFilterThatMovesPixels() const;
    bool hasFilterThatShouldBeRestrictedBySecurityOrigin() const;

    template<FilterOperation::Type Type>
    bool hasFilterOfType() const;

    bool hasReferenceFilter() const;
    bool isReferenceFilter() const;

    IntOutsets outsets() const;
};

template<FilterOperation::Type type> bool Filter::hasFilterOfType() const
{
    return std::ranges::any_of(*this, [](auto& op) { return op->type() == type; });
}

// MARK: - Conversion

template<> struct ToCSS<FilterValue> { auto operator()(const FilterValue&, const RenderStyle&) -> CSS::FilterValue; };
template<> struct ToStyle<CSS::FilterValue> { auto operator()(const CSS::FilterValue&, const BuilderState&) -> FilterValue; };

template<> struct ToCSS<Filter> { auto operator()(const Filter&, const RenderStyle&) -> CSS::Filter; };
template<> struct ToStyle<CSS::Filter> { auto operator()(const CSS::Filter&, const BuilderState&) -> Filter; };
template<> struct CSSValueConversion<Filter> { auto operator()(BuilderState&, const CSSValue&) -> Filter; };
template<> struct CSSValueCreation<Filter> { auto operator()(CSSValuePool&, const RenderStyle&, const Filter&) -> Ref<CSSValue>; };

// MARK: - Serialization

template<> struct Serialize<Filter> { void operator()(StringBuilder&, const CSS::SerializationContext&, const RenderStyle&, const Filter&); };

// MARK: - Blending

template<> struct Blending<Filter> {
    auto canBlend(const Filter&, const Filter&, CompositeOperation) -> bool;
    constexpr auto requiresInterpolationForAccumulativeIteration(const Filter&, const Filter&) -> bool { return true; }
    auto blend(const Filter&, const Filter&, const BlendingContext&) -> Filter;
};

// MARK: - Platform

template<> struct ToPlatform<FilterValue> { auto operator()(const FilterValue&) -> Ref<FilterOperation>; };
template<> struct ToPlatform<Filter> { auto operator()(const Filter&) -> FilterOperations; };

// MARK: - Logging

TextStream& operator<<(TextStream&, const FilterValue&);

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::Filter)
