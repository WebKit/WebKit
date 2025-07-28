/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "StyleValueTypes.h"
#include <limits>

namespace WebCore {
namespace Style {

// <'-webkit-line-clamp'> = none | <percentage [0,∞]> | <integer [1,∞]>
// NOTE: There is no standard associated with this property.
struct WebkitLineClamp {
    using Percentage = Style::Percentage<CSS::Nonnegative>;
    using Integer = Style::Integer<CSS::Range{1,CSS::Range::infinity}>;

    constexpr WebkitLineClamp(CSS::Keyword::None keyword) : m_value { keyword } { }
    constexpr WebkitLineClamp(Percentage percentage) : m_value { percentage }  { }
    constexpr WebkitLineClamp(Integer integer) : m_value { integer }  { }

    constexpr bool isNone() const { return WTF::holdsAlternative<CSS::Keyword::None>(m_value); }
    constexpr bool isPercentage() const { return WTF::holdsAlternative<Percentage>(m_value); }
    constexpr bool isInteger() const { return WTF::holdsAlternative<Integer>(m_value); }

    std::optional<Percentage> tryPercentage() const { return isPercentage() ? std::make_optional(std::get<Percentage>(m_value)) : std::nullopt; }
    std::optional<Integer> tryInteger() const { return isInteger() ? std::make_optional(std::get<Integer>(m_value)) : std::nullopt; }

    template<typename U> bool holdsAlternative() const
    {
        return WTF::holdsAlternative<U>(m_value);
    }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(m_value, std::forward<F>(f)...);
    }

    unsigned valueForTextAutosizingHash() const
    {
        return WTF::switchOn(m_value,
            [](const CSS::Keyword::None&) { return std::numeric_limits<unsigned>::max(); },
            [](const auto& numeric) { return static_cast<unsigned>(numeric.value); }
        );
    }

    constexpr bool operator==(const WebkitLineClamp&) const = default;

private:
    Variant<CSS::Keyword::None, Percentage, Integer> m_value;
};

// MARK: - Conversion

template<> struct CSSValueConversion<WebkitLineClamp> { auto operator()(BuilderState&, const CSSValue&) -> WebkitLineClamp; };

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::WebkitLineClamp)
