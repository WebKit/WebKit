/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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

#include "CSSCustomIdent.h"
#include "CSSPrimitiveNumericTypes.h"

namespace WebCore {
namespace CSS {

// <grid-line-explicit> = [ <integer [-∞,-1]> | <integer [1,∞]> ] && <custom-ident>?
struct GridLineExplicit {
    Integer<> index;
    std::optional<CustomIdent> name;

    bool operator==(const GridLineExplicit&) const = default;
};
template<size_t I> const auto& get(const GridLineExplicit& value)
{
    if constexpr (!I)
        return value.index;
    else if constexpr (I == 1)
        return value.name;
}

// <grid-line-span> = span && [ <integer [1,∞]>@(default=1) || <custom-ident> ]
struct GridLineSpan {
    Integer<Positive> index;
    std::optional<CustomIdent> name;

    bool operator==(const GridLineSpan&) const = default;
};
template<size_t I> const auto& get(const GridLineSpan& value)
{
    if constexpr (!I)
        return value.index;
    else if constexpr (I == 1)
        return value.name;
}

// Custom serialization needed to add leading 'span' keyword and conditionally elide default `Integer<Positive>`.
template<> struct Serialize<GridLineSpan> { void operator()(StringBuilder&, const SerializationContext&, const GridLineSpan&); };

// <'grid-row-start'>/<'grid-column-start'>/<'grid-row-end'>/<'grid-column-end'>' = auto | <custom-ident> | <grid-line-explicit> | <grid-line-span>
// https://drafts.csswg.org/css-grid/#propdef-grid-row-start
// https://drafts.csswg.org/css-grid/#propdef-grid-column-start
// https://drafts.csswg.org/css-grid/#propdef-grid-row-end
// https://drafts.csswg.org/css-grid/#propdef-grid-column-end
struct GridLine {
    using Explicit = GridLineExplicit;
    using Span = GridLineSpan;

    GridLine(Keyword::Auto keyword) : m_value { keyword } { }
    GridLine(CustomIdent&& customIdent) : m_value { WTF::move(customIdent) } { }
    GridLine(Explicit&& gridLineExplicit) : m_value { WTF::move(gridLineExplicit) } { }
    GridLine(Span&& gridLineSpan) : m_value { WTF::move(gridLineSpan) } { }

    bool isAuto() const { return WTF::holdsAlternative<Keyword::Auto>(m_value); }
    bool isCustomIdent() const { return WTF::holdsAlternative<CustomIdent>(m_value); }
    bool isExplicit() const { return WTF::holdsAlternative<Explicit>(m_value); }
    bool isSpan() const { return WTF::holdsAlternative<Span>(m_value); }

    std::optional<CustomIdent> customIdent() const
    {
        return isCustomIdent() ? std::optional { std::get<CustomIdent>(m_value) } : std::nullopt;
    }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(m_value, std::forward<F>(f)...);
    }

    bool operator==(const GridLine&) const = default;

private:
    Variant<Keyword::Auto, CustomIdent, Explicit, Span> m_value;
};

// `GridLine` is special-cased to return a `CSSGridLineValue`.
template<> struct CSSValueCreation<GridLine> { Ref<CSSValue> operator()(CSSValuePool&, const GridLine&); };

} // namespace CSS
} // namespace WebCore

DEFINE_SPACE_SEPARATED_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::GridLineExplicit, 2)
DEFINE_SPACE_SEPARATED_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::GridLineSpan, 2)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::CSS::GridLine)
