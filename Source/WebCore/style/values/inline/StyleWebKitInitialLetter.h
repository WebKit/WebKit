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

#include <WebCore/StylePrimitiveNumericTypes.h>

namespace WebCore {
namespace Style {

// <'-webkit-initial-letter'> = normal | <number [0,∞]>{1,2}@(default=previous)
// NOTE: There is a standard `initial-letter` property with a different grammar that is not yet implemented.
// https://drafts.csswg.org/css-inline/#propdef-initial-letter
struct WebkitInitialLetter {
    constexpr WebkitInitialLetter(CSS::Keyword::Normal)
    {
    }

    constexpr WebkitInitialLetter(Number<CSS::Nonnegative, float> height)
        : m_value { height, height }
    {
    }

    constexpr WebkitInitialLetter(Number<CSS::Nonnegative, float> height, Number<CSS::Nonnegative, float> drop)
        : m_value { height, drop }
    {
    }

    constexpr bool isNormal() const { return m_value.first().isZero() && m_value.second().isZero(); }

    constexpr float height() const { return m_value.first().value; }
    constexpr float drop() const { return m_value.second().value; }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        if (isNormal())
            return visitor(CSS::Keyword::Normal { });
        return visitor(m_value);
    }

    bool operator==(const WebkitInitialLetter&) const = default;

private:
    MinimallySerializingSpaceSeparatedPair<Number<CSS::Nonnegative, float>> m_value { 0, 0 };
};

// MARK: - Conversion

template<> struct CSSValueConversion<WebkitInitialLetter> { auto operator()(BuilderState&, const CSSValue&) -> WebkitInitialLetter; };

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::WebkitInitialLetter)
