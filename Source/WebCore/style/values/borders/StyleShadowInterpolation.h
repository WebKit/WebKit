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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/AnimationUtilities.h>
#include <WebCore/StyleShadow.h>

namespace WebCore {
namespace Style {

// Generic implementation of interpolation for shadow lists for use by both `Style::BoxShadow` and `Style::TextShadow`.
// https://www.w3.org/TR/web-animations-1/#animating-shadow-lists

template<typename ShadowsType, typename MatchingShadows>
struct ShadowInterpolation {
    using ShadowListType = typename ShadowsType::List;
    using ShadowType = typename ShadowListType::value_type;

    static bool canInterpolate(const ShadowsType& fromShadows, const ShadowsType& toShadows, CompositeOperation compositeOperation)
    {
        if (compositeOperation != CompositeOperation::Replace)
            return true;

        // The only scenario where we can't interpolate is if specified items don't have the same shadow style.

        auto fromLength = fromShadows.size();
        auto toLength = toShadows.size();

        // FIXME: Something like LLVM ADT's zip_shortest (https://llvm.org/doxygen/structllvm_1_1detail_1_1zip__shortest.html) would allow this to be done without indexing:
        //
        // return std::ranges::all_of(
        //     zip_shortest(fromShadows | std::views::reverse, toShadows | std::views::reverse),
        //     [](const auto& pair) {
        //         return shadowStyle(std::get<0>(pair)) == shadowStyle(std::get<1>(pair));
        //     }
        // );

        size_t minLength = std::min(fromLength, toLength);
        for (size_t i = 0; i < minLength; ++i) {
            auto fromIndex = fromLength - i - 1;
            auto toIndex = toLength - i - 1;
            if (shadowStyle(fromShadows[fromIndex]) != shadowStyle(toShadows[toIndex]))
                return false;
        }
        return true;
    }

    static auto interpolate(const ShadowsType& fromShadows, const ShadowsType& toShadows, const RenderStyle& fromStyle, const RenderStyle& toStyle, const BlendingContext& context) -> ShadowsType
    {
        if (context.isDiscrete) {
            ASSERT(!context.progress || context.progress == 1.0);
            return context.progress ? toShadows : fromShadows;
        }

        auto fromLength = fromShadows.size();
        auto toLength = toShadows.size();

        if (!fromLength && !toLength)
            return CSS::Keyword::None { };
        if (fromLength == toLength)
            return blendMatchedShadowLists(fromShadows, toShadows, fromLength, fromStyle, toStyle, context);
        return blendMismatchedShadowLists(fromShadows, toShadows, fromLength, toLength, fromStyle, toStyle, context);
    }

    static auto addShadowLists(const ShadowsType& fromShadows, const ShadowsType& toShadows, size_t fromLength, size_t toLength) -> ShadowsType
    {
        return ShadowListType::createWithSizeFromGenerator(fromLength + toLength, [&](auto index) -> ShadowType {
            if (index < toLength)
                return toShadows[index];
            return fromShadows[index - toLength];
        });
    }

    static auto blendMatchedShadowLists(const ShadowsType& fromShadows, const ShadowsType& toShadows, size_t length, const RenderStyle& fromStyle, const RenderStyle& toStyle, const BlendingContext& context) -> ShadowsType
    {
        // `from` or `to` might be empty in which case we don't want to do additivity, but do replace instead.
        if (!fromShadows.isNone() && !toShadows.isNone() && context.compositeOperation == CompositeOperation::Add)
            return addShadowLists(fromShadows, toShadows, length, length);

        return ShadowListType::createWithSizeFromGenerator(length, [&](auto index) -> ShadowType {
            return Style::blend(fromShadows[index], toShadows[index], fromStyle, toStyle, context);
        });
    }

    static auto blendMismatchedShadowLists(const ShadowsType& fromShadows, const ShadowsType& toShadows, size_t fromLength, size_t toLength, const RenderStyle& fromStyle, const RenderStyle& toStyle, const BlendingContext& context) -> ShadowsType
    {
        if (!fromShadows.isNone() && !toShadows.isNone() && context.compositeOperation != CompositeOperation::Replace)
            return addShadowLists(fromShadows, toShadows, fromLength, toLength);

        auto maxLength = std::max(fromLength, toLength);
        return ShadowListType::createWithSizeFromGenerator(maxLength, [&](auto index) -> ShadowType {
            auto indexFromEnd = maxLength - index - 1;
            bool hasFrom = indexFromEnd < fromLength;
            bool hasTo = indexFromEnd < toLength;

            if (hasFrom && hasTo) {
                const auto& fromShadow = fromShadows[index - (maxLength - fromLength)];
                const auto& toShadow = toShadows[index - (maxLength - toLength)];
                return Style::blend(fromShadow, toShadow, fromStyle, toStyle, context);
            } else if (hasFrom) {
                const auto& fromShadow = fromShadows[index - (maxLength - fromLength)];
                const auto& toShadow = MatchingShadows::shadowForInterpolation(fromShadow);
                return Style::blend(fromShadow, toShadow, fromStyle, toStyle, context);
            } else if (hasTo) {
                const auto& toShadow = toShadows[index - (maxLength - toLength)];
                const auto& fromShadow = MatchingShadows::shadowForInterpolation(toShadow);
                return Style::blend(fromShadow, toShadow, fromStyle, toStyle, context);
            }

            RELEASE_ASSERT_NOT_REACHED();
        });
    }
};

} // namespace Style
} // namespace WebCore
