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

#include <WebCore/AnimationUtilities.h>
#include <WebCore/CompositeOperation.h>
#include <WebCore/FilterOperation.h>

namespace WebCore {
namespace Style {

// Generic implementation of interpolation for filter lists for use by both `Style::Filter` and `Style::AppleColorFilter`.
// https://drafts.fxtf.org/filter-effects/#interpolation-of-filters

template<typename FilterList>
auto canBlendFilterLists(const FilterList& from, const FilterList& to, CompositeOperation compositeOperation) -> bool
{
    auto hasReferenceFilter = [](const FilterList& list) {
        return std::ranges::any_of(list, [](const auto& value) { return value->type() == FilterOperation::Type::Reference; });
    };

    // We can't interpolate between lists if a reference filter is involved.
    if (hasReferenceFilter(from) || hasReferenceFilter(to))
        return false;

    // Additive and accumulative composition will always yield interpolation.
    if (compositeOperation != CompositeOperation::Replace)
        return true;

    // Provided the two filter lists have a shared set of initial primitives, we will be able to interpolate.
    // Note that this means that if either list is empty, interpolation is supported.

    auto fromLength = from.size();
    auto toLength = to.size();
    auto minLength = std::min(fromLength, toLength);

    for (size_t i = 0; i < minLength; ++i) {
        Ref fromOperation = from[i].platform();
        Ref toOperation = to[i].platform();

        if (fromOperation->type() != toOperation->type())
            return false;
    }

    return true;
}

template<typename FilterList>
auto blendFilterLists(const FilterList& from, const FilterList& to, const BlendingContext& context) -> FilterList
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
            RefPtr<FilterOperation> fromOp = (index < fromLength) ? from[index].value.ptr() : nullptr;
            RefPtr<FilterOperation> toOp = (index < toLength) ? to[index].value.ptr() : nullptr;

            RefPtr<FilterOperation> blendedOp;
            if (toOp)
                blendedOp = toOp->blend(fromOp.get(), context);
            else if (fromOp)
                blendedOp = fromOp->blend(nullptr, context, true);

            if (blendedOp)
                return FilterValue { blendedOp.releaseNonNull() };

            if (context.progress > 0.5) {
                if (toOp)
                    return FilterValue { toOp.releaseNonNull() };
                else
                    return FilterValue { PassthroughFilterOperation::create() };
            } else {
                if (fromOp)
                    return FilterValue { fromOp.releaseNonNull() };
                else
                    return FilterValue { PassthroughFilterOperation::create() };
            }
        })
    };
}

} // namespace Style
} // namespace WebCore
