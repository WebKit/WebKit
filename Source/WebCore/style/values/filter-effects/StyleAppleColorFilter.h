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

#include <WebCore/FilterOperations.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {

namespace CSS {
struct AppleColorFilter;
struct AppleColorFilterValue;
}

namespace Style {

// Any <apple-color-filter-function>.
// (Equivalent of https://drafts.fxtf.org/filter-effects/#typedef-filter-function)
struct AppleColorFilterValue {
    AppleColorFilterValue(Ref<FilterOperation> value) : value { WTFMove(value) } { }

    const FilterOperation& get() const { return value.get(); }
    const FilterOperation& platform() const { return value.get(); }
    const FilterOperation* operator->() const { return value.ptr(); }

    bool operator==(const AppleColorFilterValue& other) const
    {
        return arePointingToEqualData(value, other.value);
    }

    Ref<FilterOperation> value;
};

// <apple-color-filter-value-list> = [ <apple-color-filter-function> | <url> ]+
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

    template<FilterOperation::Type Type>
    bool hasFilterOfType() const;
};

template<FilterOperation::Type type> bool AppleColorFilter::hasFilterOfType() const
{
    return std::ranges::any_of(*this, [](auto& op) { return op->type() == type; });
}

// MARK: - Conversion

template<> struct ToCSS<AppleColorFilterValue> { auto operator()(const AppleColorFilterValue&, const RenderStyle&) -> CSS::AppleColorFilterValue; };
template<> struct ToStyle<CSS::AppleColorFilterValue> { auto operator()(const CSS::AppleColorFilterValue&, const BuilderState&) -> AppleColorFilterValue; };

template<> struct ToCSS<AppleColorFilter> { auto operator()(const AppleColorFilter&, const RenderStyle&) -> CSS::AppleColorFilter; };
template<> struct ToStyle<CSS::AppleColorFilter> { auto operator()(const CSS::AppleColorFilter&, const BuilderState&) -> AppleColorFilter; };
template<> struct CSSValueConversion<AppleColorFilter> { auto operator()(BuilderState&, const CSSValue&) -> AppleColorFilter; };
template<> struct CSSValueCreation<AppleColorFilter> { auto operator()(CSSValuePool&, const RenderStyle&, const AppleColorFilter&) -> Ref<CSSValue>; };

// MARK: - Serialization

template<> struct Serialize<AppleColorFilter> { void operator()(StringBuilder&, const CSS::SerializationContext&, const RenderStyle&, const AppleColorFilter&); };

// MARK: - Blending

template<> struct Blending<AppleColorFilter> {
    auto canBlend(const AppleColorFilter&, const AppleColorFilter&, CompositeOperation) -> bool;
    constexpr auto requiresInterpolationForAccumulativeIteration(const AppleColorFilter&, const AppleColorFilter&) -> bool { return true; }
    auto blend(const AppleColorFilter&, const AppleColorFilter&, const BlendingContext&) -> AppleColorFilter;
};

// MARK: - Platform

template<> struct ToPlatform<AppleColorFilterValue> { auto operator()(const AppleColorFilterValue&) -> Ref<FilterOperation>; };
template<> struct ToPlatform<AppleColorFilter> { auto operator()(const AppleColorFilter&) -> FilterOperations; };

// MARK: - Logging

TextStream& operator<<(TextStream&, const AppleColorFilterValue&);

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::AppleColorFilter)
