/*
 * Copyright (C) 2011 Nokia Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

// <'quote'> = auto | none | [ <string> <string> ]+
// https://www.w3.org/TR/css-content-3/#propdef-quotes
struct Quotes {
    using Data = SpaceSeparatedRefCountedFixedVector<String>;

    Quotes(CSS::Keyword::Auto keyword)
        : m_value { keyword }
    {
    }

    Quotes(CSS::Keyword::None keyword)
        : m_value { keyword }
    {
    }

    Quotes(Data&& data)
        : m_value { WTFMove(data) }
    {
    }

    bool isAuto() const { return WTF::holdsAlternative<CSS::Keyword::Auto>(m_value); }
    bool isNone() const { return WTF::holdsAlternative<CSS::Keyword::None>(m_value); }
    bool isQuotes() const { return WTF::holdsAlternative<Data>(m_value); }

    const String& openQuote(unsigned index) const;
    const String& closeQuote(unsigned index) const;

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(m_value, std::forward<F>(f)...);
    }

    bool operator==(const Quotes&) const = default;

private:
    Variant<CSS::Keyword::Auto, CSS::Keyword::None, Data> m_value;
};

// MARK: - Conversion

template<> struct CSSValueConversion<Quotes> { auto operator()(BuilderState&, const CSSValue&) -> Quotes; };

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::Quotes)
