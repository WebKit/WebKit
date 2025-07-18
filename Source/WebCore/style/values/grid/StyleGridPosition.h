/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013-2017 Igalia S.L.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "StyleGridPositionSide.h"
#include "StylePrimitiveNumeric.h"
#include "StyleValueTypes.h"

namespace WebCore {
namespace Style {

// <grid-line-explicit> = [ [ <integer [-∞,-1]> | <integer [1,∞]> ] && <custom-ident>? ]
struct GridPositionExplicit {
    // NOTE: We don't currently have an efficient way to represent disjoint numeric ranges so an unconstrained range is used here instead.
    using Position = Integer<>;

    Position position { 1 };
    CustomIdentifier name { nullAtom() };

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        if (name.value.isNull())
            return visitor(position);
        return visitor(SpaceSeparatedTuple { position, name });
    }

    bool operator==(const GridPositionExplicit&) const = default;
};

// <grid-line-span> = [ span && [ <integer [1,∞]> || <custom-ident>  ] ]
struct GridPositionSpan {
    using Position = Integer<CSS::Range{1,CSS::Range::infinity}>;

    Position position { 1 };
    CustomIdentifier name { nullAtom() };

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        if (name.value.isNull())
            return visitor(SpaceSeparatedTuple { CSS::Keyword::Span { }, position });
        if (position == 1)
            return visitor(SpaceSeparatedTuple { CSS::Keyword::Span { }, name });
        return visitor(SpaceSeparatedTuple { CSS::Keyword::Span { }, position, name });
    }

    bool operator==(const GridPositionSpan&) const = default;
};

// <grid-line> = auto | <custom-ident> | <grid-line-explicit> | <grid-line-span>
// https://drafts.csswg.org/css-grid/#typedef-grid-row-start-grid-line
// FIXME: The standard calls this type "grid-line". We should consider matching it.
struct GridPosition {
    using Explicit = GridPositionExplicit;
    using Span = GridPositionSpan;

    GridPosition(CSS::Keyword::Auto) { }
    WEBCORE_EXPORT GridPosition(Explicit&&);
    WEBCORE_EXPORT GridPosition(Span&&);
    GridPosition(CustomIdentifier&&);

    bool isAuto() const { return m_type == GridPositionType::Auto; }
    bool isExplicit() const { return m_type == GridPositionType::Explicit; }
    bool isSpan() const { return m_type == GridPositionType::Span; }
    bool isNamedGridArea() const { return m_type == GridPositionType::NamedGridArea; }

    WEBCORE_EXPORT int explicitPosition() const;
    WEBCORE_EXPORT int spanPosition() const;
    String namedGridLine() const;

    bool shouldBeResolvedAgainstOppositePosition() const { return isAuto() || isSpan(); }

    // Note that grid line 1 is internally represented by the index 0, that's why the max value for
    // a position is gridMaxTracks instead of gridMaxTracks + 1.
    static int max();
    static int min();

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        switch (m_type) {
        case GridPositionType::Auto:
            return visitor(CSS::Keyword::Auto { });
        case GridPositionType::Explicit:
            return visitor(Explicit { { m_integerPosition }, m_namedGridLine });
        case GridPositionType::Span:
            return visitor(Span { { m_integerPosition }, m_namedGridLine });
        case GridPositionType::NamedGridArea:
            return visitor(m_namedGridLine);
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    bool operator==(const GridPosition&) const = default;

    WEBCORE_EXPORT static void setMaxPositionForTesting(unsigned);

private:
    enum class GridPositionType : uint8_t {
        Auto,
        Explicit,
        Span,
        NamedGridArea
    };

    GridPositionType m_type { GridPositionType::Auto };
    int m_integerPosition { 1 };
    CustomIdentifier m_namedGridLine;
};

// MARK: - Conversion

template<> struct CSSValueConversion<GridPosition> { auto operator()(BuilderState&, const CSSValue&) -> GridPosition; };

// MARK: - Logging

WTF::TextStream& operator<<(WTF::TextStream&, const GridPosition&);

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::GridPosition)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::GridPosition::Explicit)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::GridPosition::Span)
