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

#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

// <'grid-auto-flow'> = normal | [ row | column ] || dense
// https://drafts.csswg.org/css-grid-1/#grid-auto-flow-property
struct GridAutoFlow {
    enum class Direction : uint8_t { Normal, Row, Column };
    enum class Packing : bool { Dense, Sparse };

    constexpr GridAutoFlow(CSS::Keyword::Normal) { }
    constexpr GridAutoFlow(CSS::Keyword::Row) : m_direction { static_cast<uint8_t>(Direction::Row) } { }
    constexpr GridAutoFlow(CSS::Keyword::Row, CSS::Keyword::Dense) : m_direction { static_cast<uint8_t>(Direction::Row) }, m_packing { static_cast<uint8_t>(Packing::Dense) } { }
    constexpr GridAutoFlow(CSS::Keyword::Column) : m_direction { static_cast<uint8_t>(Direction::Column) } { }
    constexpr GridAutoFlow(CSS::Keyword::Column, CSS::Keyword::Dense) : m_direction { static_cast<uint8_t>(Direction::Column) }, m_packing { static_cast<uint8_t>(Packing::Dense) } { }
    constexpr GridAutoFlow(CSS::Keyword::Dense) : m_packing { static_cast<uint8_t>(Packing::Dense) } { }

    constexpr Direction direction() const { return static_cast<Direction>(m_direction); }
    constexpr Packing packing() const { return static_cast<Packing>(m_packing); }

    constexpr bool isRow() const { return direction() == Direction::Row; }
    constexpr bool isColumn() const { return direction() == Direction::Column; }
    constexpr bool isDense() const { return packing() == Packing::Dense; }
    constexpr bool isSparse() const { return packing() == Packing::Sparse; }

    void setDirection(Direction direction) { m_direction = static_cast<uint8_t>(direction); }

    template<typename... F> decltype(auto) switchOn(F&&...) const;

    constexpr bool operator==(const GridAutoFlow&) const = default;

private:
    PREFERRED_TYPE(Direction) uint8_t m_direction : 2 { static_cast<uint8_t>(Direction::Normal) };
    PREFERRED_TYPE(Packing) uint8_t m_packing : 1 { static_cast<uint8_t>(Packing::Sparse) };
};

template<typename... F> decltype(auto) GridAutoFlow::switchOn(F&&... f) const
{
    auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

    switch (direction()) {
    case Direction::Column:
        switch (packing()) {
        case Packing::Dense:
            return visitor(SpaceSeparatedTuple { CSS::Keyword::Column { }, CSS::Keyword::Dense { } });
        case Packing::Sparse:
            return visitor(CSS::Keyword::Column { });
        }
    case Direction::Row:
        switch (packing()) {
        case Packing::Dense:
            return visitor(SpaceSeparatedTuple { CSS::Keyword::Row { }, CSS::Keyword::Dense { } });
        case Packing::Sparse:
            return visitor(CSS::Keyword::Row { });
        }
    case Direction::Normal:
        switch (packing()) {
        case Packing::Dense:
            return visitor(CSS::Keyword::Dense { });
        case Packing::Sparse:
            return visitor(CSS::Keyword::Normal { });
        }
    }
}

// MARK: - Conversion

template<> struct CSSValueConversion<GridAutoFlow> { auto operator()(BuilderState&, const CSSValue&) -> GridAutoFlow; };

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::GridAutoFlow)
