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

#include <WebCore/RenderStyleConstants.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

// <'scroll-snap-align'> = [ none | start | end | center ]{1,2}
// https://drafts.csswg.org/css-scroll-snap-1/#propdef-scroll-snap-align
struct ScrollSnapAlign {
    constexpr ScrollSnapAlign(CSS::Keyword::None)
    {
    }
    constexpr ScrollSnapAlign(ScrollSnapAxisAlignType bothAxes)
        : blockAlign { bothAxes }
        , inlineAlign { bothAxes }
    {
    }
    constexpr ScrollSnapAlign(ScrollSnapAxisAlignType blockAlign, ScrollSnapAxisAlignType inlineAlign)
        : blockAlign { blockAlign }
        , inlineAlign { inlineAlign }
    {
    }

    bool isNone() const { return blockAlign == ScrollSnapAxisAlignType::None && inlineAlign == ScrollSnapAxisAlignType::None; }

    ScrollSnapAxisAlignType blockAlign { ScrollSnapAxisAlignType::None };
    ScrollSnapAxisAlignType inlineAlign { ScrollSnapAxisAlignType::None };

    constexpr bool operator==(const ScrollSnapAlign&) const = default;
};

template<size_t I> constexpr const auto& get(const ScrollSnapAlign& value)
{
    if constexpr (!I)
        return value.blockAlign;
    else if constexpr (I == 1)
        return value.inlineAlign;
}

// MARK: - Conversion

template<> struct CSSValueConversion<ScrollSnapAlign> { auto operator()(BuilderState&, const CSSValue&) -> ScrollSnapAlign; };

} // namespace Style
} // namespace WebCore

DEFINE_COALESCING_SPACE_SEPARATED_TUPLE_LIKE_CONFORMANCE(WebCore::Style::ScrollSnapAlign, 2)
