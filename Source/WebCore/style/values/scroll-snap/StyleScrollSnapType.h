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

// <'scroll-snap-type'> = none | [ x | y | block | inline | both ] [ mandatory | proximity ]?@(default=proximity)
// https://drafts.csswg.org/css-scroll-snap-1/#propdef-scroll-snap-type
struct ScrollSnapType {
    struct ScrollSnapContainer {
        ScrollSnapAxis axis;
        ScrollSnapStrictness strictness;

        constexpr bool operator==(const ScrollSnapContainer&) const = default;
    };

    constexpr ScrollSnapType(CSS::Keyword::None) { }
    constexpr ScrollSnapType(ScrollSnapAxis axis, ScrollSnapStrictness strictness = ScrollSnapStrictness::Proximity)
        : m_value { ScrollSnapContainer { axis, strictness } }
    {
    }

    constexpr bool isNone() const { return !m_value; }
    constexpr bool isContainer() const { return !!m_value; }
    constexpr std::optional<ScrollSnapContainer> tryContainer() const { return m_value; }

    template<typename... F> constexpr decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        if (isNone())
            return visitor(CSS::Keyword::None { });

        switch (m_value->strictness) {
        case ScrollSnapStrictness::Proximity:
            return visitor(m_value->axis);
        case ScrollSnapStrictness::Mandatory:
            return visitor(SpaceSeparatedTuple { m_value->axis, CSS::Keyword::Mandatory { } });
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    constexpr bool operator==(const ScrollSnapType&) const = default;

private:
    std::optional<ScrollSnapContainer> m_value;
};

// MARK: - Conversion

template<> struct CSSValueConversion<ScrollSnapType> { auto operator()(BuilderState&, const CSSValue&) -> ScrollSnapType; };

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::ScrollSnapType)
