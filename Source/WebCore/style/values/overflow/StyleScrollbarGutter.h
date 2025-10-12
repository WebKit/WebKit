/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

// <'scrollbar-gutter'> = auto | stable && both-edges?
// https://www.w3.org/TR/css-overflow-3/#propdef-scrollbar-gutter
struct ScrollbarGutter {
    constexpr ScrollbarGutter(CSS::Keyword::Auto)
        : m_type { Type::Auto }
    {
    }

    constexpr ScrollbarGutter(CSS::Keyword::Stable)
        : m_type { Type::Stable }
    {
    }

    constexpr ScrollbarGutter(CSS::Keyword::Stable, CSS::Keyword::BothEdges)
        : m_type { Type::StableBothEdges }
    {
    }

    constexpr bool isAuto() const { return m_type == Type::Auto; }
    constexpr bool isStable() const { return m_type == Type::Stable; }
    constexpr bool isStableBothEdges() const { return m_type == Type::StableBothEdges; }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        switch (m_type) {
        case Type::Auto:
            return visitor(CSS::Keyword::Auto { });
        case Type::Stable:
            return visitor(CSS::Keyword::Stable { });
        case Type::StableBothEdges:
            return visitor(SpaceSeparatedTuple { CSS::Keyword::Stable { }, CSS::Keyword::BothEdges { } });
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    constexpr bool operator==(const ScrollbarGutter&) const = default;

private:
    enum Type : uint8_t { Auto, Stable, StableBothEdges };
    Type m_type;
};

// MARK: - Conversion

template<> struct CSSValueConversion<ScrollbarGutter> { auto operator()(BuilderState&, const CSSValue&) -> ScrollbarGutter; };

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::ScrollbarGutter)
