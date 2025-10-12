/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#if ENABLE(TEXT_AUTOSIZING)

#include <WebCore/StylePrimitiveNumericTypes.h>

namespace WebCore {
namespace Style {

// <'-webkit-text-size-adjust'> = auto | none | <percentage [0,∞]>
// https://drafts.csswg.org/css-size-adjust/#propdef-text-size-adjust
struct TextSizeAdjust {
    constexpr TextSizeAdjust(CSS::Keyword::None)
        : m_value(None)
    {
    }

    constexpr TextSizeAdjust(CSS::Keyword::Auto)
        : m_value(Auto)
    {
    }

    constexpr TextSizeAdjust(Percentage<CSS::Nonnegative, float> value)
        : m_value(value.value)
    {
        ASSERT_UNDER_CONSTEXPR_CONTEXT(m_value >= 0);
    }

    constexpr float percentage() const { ASSERT_UNDER_CONSTEXPR_CONTEXT(m_value >= 0); return m_value; }
    constexpr float multiplier() const { ASSERT_UNDER_CONSTEXPR_CONTEXT(m_value >= 0); return m_value / 100; }

    constexpr bool isAuto() const { return m_value == Auto; }
    constexpr bool isNone() const { return m_value == None; }
    constexpr bool isPercentage() const { return m_value >= 0; }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        if (isAuto())
            return visitor(CSS::Keyword::Auto { });
        if (isNone())
            return visitor(CSS::Keyword::None { });
        ASSERT(isPercentage());
        return visitor(Percentage<CSS::Nonnegative, float> { m_value });
    }

    constexpr bool operator==(const TextSizeAdjust&) const = default;

private:
    static constexpr float Auto = -1;
    static constexpr float None = -2;

    float m_value;
};

// MARK: - Conversion

template<> struct CSSValueConversion<TextSizeAdjust> { auto operator()(BuilderState&, const CSSValue&) -> TextSizeAdjust; };

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::TextSizeAdjust)

#endif // ENABLE(TEXT_AUTOSIZING)
