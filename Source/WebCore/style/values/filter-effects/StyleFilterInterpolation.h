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

#include <WebCore/AnimationUtilities.h>
#include <WebCore/CompositeOperation.h>
#include <algorithm>

namespace WebCore {
namespace Style {

// Generic implementation of interpolation for filter lists for use by both `Style::Filter` and `Style::AppleColorFilter`.
// https://drafts.fxtf.org/filter-effects/#interpolation-of-filters

template<typename FilterValue>
auto blendFilterValue(const FilterValue& from, const FilterValue& to, const RenderStyle& fromStyle, const RenderStyle& toStyle, const BlendingContext& context) -> FilterValue
{
    ASSERT(from.index() == to.index());

    return WTF::visit(WTF::makeVisitor(
        [&]<typename T>(const T& fromValue, const T& toValue) -> FilterValue {
            return Style::blend(fromValue, toValue, fromStyle, toStyle, context);
        },
        [](const auto&, const auto&) -> FilterValue {
            RELEASE_ASSERT_NOT_REACHED();
        }
    ), from.value, to.value);
}

template<typename FilterValue>
auto blendFilterValueFromOnly(const FilterValue& from, const RenderStyle& fromStyle, const RenderStyle& toStyle, const BlendingContext& context) -> FilterValue
{
    return WTF::visit(WTF::makeVisitor(
        [&]<CSSValueID C, typename T>(const FunctionNotation<C, T>& fromValue) -> FilterValue {
            return Style::blend(fromValue, FunctionNotation<C, T> { T::passthroughForInterpolation() }, fromStyle, toStyle, context);
        },
        [](const auto&) -> FilterValue {
            RELEASE_ASSERT_NOT_REACHED();
        }
    ), from.value);
}

template<typename FilterValue>
auto blendFilterValueToOnly(const FilterValue& to, const RenderStyle& fromStyle, const RenderStyle& toStyle, const BlendingContext& context) -> FilterValue
{
    return WTF::visit(WTF::makeVisitor(
        [&]<CSSValueID C, typename T>(const FunctionNotation<C, T>& toValue) -> FilterValue {
            return Style::blend(FunctionNotation<C, T> { T::passthroughForInterpolation() }, toValue, fromStyle, toStyle, context);
        },
        [](const auto&) -> FilterValue {
            RELEASE_ASSERT_NOT_REACHED();
        }
    ), to.value);
}

template<typename FilterList>
auto canBlendFilterLists(const FilterList& from, const FilterList& to, CompositeOperation compositeOperation) -> bool
{
    // Additive composition will always yield interpolation.
    if (compositeOperation == CompositeOperation::Add)
        return true;

    // Provided the two filter lists have a shared set of initial primitives, we will be able to interpolate.
    // Note that this means that if either list is empty, interpolation is supported.

    auto fromLength = from.size();
    auto toLength = to.size();
    auto minLength = std::min(fromLength, toLength);

    for (size_t i = 0; i < minLength; ++i) {
        if (from[i].index() != to[i].index())
            return false;
    }

    return true;
}

template<typename FilterList>
auto blendFilterLists(const FilterList& from, const FilterList& to, const RenderStyle& fromStyle, const RenderStyle& toStyle, const BlendingContext& context) -> FilterList
{
    using FilterValue = typename FilterList::value_type;

    if (context.compositeOperation == CompositeOperation::Add) {
        ASSERT(context.progress == 1.0);

        auto fromLength = from.size();
        auto toLength = to.size();

        return FilterList {
            FilterList::Container::createWithSizeFromGenerator(fromLength + toLength, [&](auto index) {
                if (index < fromLength)
                    return from[index];
                return to[index - fromLength];
            })
        };
    }

    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1.0);
        return context.progress ? to : from;
    }

    auto fromLength = from.size();
    auto toLength = to.size();
    auto maxLength = std::max(fromLength, toLength);

    return FilterList {
        FilterList::Container::createWithSizeFromGenerator(maxLength, [&](auto index) {
            std::optional<FilterValue> fromOp = (index < fromLength) ? std::make_optional(from[index]) : std::nullopt;
            std::optional<FilterValue> toOp = (index < toLength) ? std::make_optional(to[index]) : std::nullopt;

            if (fromOp && toOp)
                return blendFilterValue(*fromOp, *toOp, fromStyle, toStyle, context);
            if (fromOp)
                return blendFilterValueFromOnly(*fromOp, fromStyle, toStyle, context);
            return blendFilterValueToOnly(*toOp, fromStyle, toStyle, context);
        })
    };
}

} // namespace Style
} // namespace WebCore
