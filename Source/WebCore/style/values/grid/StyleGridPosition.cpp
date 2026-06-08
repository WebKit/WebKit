/*
 * Copyright (C) 2017 Igalia S.L.
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

#include "config.h"
#include "StyleGridPosition.h"

#include "CSSCustomIdentValue.h"
#include "CSSGridLineValue.h"
#include "CSSKeywordValueInlines.h"
#include "StyleBuilderChecking.h"
#include "StyleKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include "StylePrimitiveNumericTypes+Logging.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

static std::optional<int> maxPositionForTesting;
static constexpr auto gridMaxPosition = 1000000;

static int NODELETE clampGridIntegerPosition(int integerPosition)
{
    return clampTo(integerPosition, GridPosition::min(), GridPosition::max());
}

void add(Hasher& hasher, const GridPosition& gridPosition)
{
    addArgs(hasher, gridPosition.m_type, gridPosition.m_integerPosition, gridPosition.m_namedGridLine);
}

GridPosition::GridPosition(GridPosition::Explicit&& explicitPosition)
    : m_type { GridPositionType::Explicit }
    , m_integerPosition { clampGridIntegerPosition(explicitPosition.position.value) }
    , m_namedGridLine { WTF::move(explicitPosition.name) }
{
}

GridPosition::GridPosition(GridPosition::Span&& spanPosition)
    : m_type { GridPositionType::Span }
    , m_integerPosition { clampGridIntegerPosition(spanPosition.position.value) }
    , m_namedGridLine { WTF::move(spanPosition.name) }
{
}

GridPosition::GridPosition(CustomIdent&& namedGridAreaPosition)
    : m_type { GridPositionType::NamedGridArea }
    , m_namedGridLine { WTF::move(namedGridAreaPosition.value) }
{
}

int GridPosition::explicitPosition() const
{
    ASSERT(m_type == GridPositionType::Explicit);
    return m_integerPosition;
}

int GridPosition::spanPosition() const
{
    ASSERT(m_type == GridPositionType::Span);
    return m_integerPosition;
}

const CustomIdent& GridPosition::namedGridLine() const
{
    ASSERT(m_type == GridPositionType::Explicit || m_type == GridPositionType::Span || m_type == GridPositionType::NamedGridArea);
    return m_namedGridLine;
}

int GridPosition::max()
{
    return maxPositionForTesting.value_or(gridMaxPosition);
}

int GridPosition::min()
{
    return -max();
}

void GridPosition::setMaxPositionForTesting(unsigned maxPosition)
{
    maxPositionForTesting = static_cast<int>(maxPosition);
}

// MARK: - Conversion

template<> struct ToCSS<GridPosition> { auto operator()(const GridPosition&, const RenderStyle&) -> CSS::GridLine; };
template<> struct ToStyle<CSS::GridLine> { auto operator()(const CSS::GridLine&, const BuilderState&) -> GridPosition; };

template<> struct ToCSS<GridPosition::Explicit> { auto operator()(const GridPosition::Explicit&, const RenderStyle&) -> CSS::GridLine::Explicit; };
template<> struct ToStyle<CSS::GridLine::Explicit> { auto operator()(const CSS::GridLine::Explicit&, const BuilderState&) -> GridPosition::Explicit; };

template<> struct ToCSS<GridPosition::Span> { auto operator()(const GridPosition::Span&, const RenderStyle&) -> CSS::GridLine::Span; };
template<> struct ToStyle<CSS::GridLine::Span> { auto operator()(const CSS::GridLine::Span&, const BuilderState&) -> GridPosition::Span; };

auto ToCSS<GridPosition>::operator()(const GridPosition& value, const RenderStyle& style) -> CSS::GridLine
{
    return WTF::switchOn(value,
        [&](CSS::Keyword::Auto keyword) -> CSS::GridLine {
            return keyword;
        },
        [&](const CustomIdent& customIdent) -> CSS::GridLine {
            return toCSS(customIdent, style);
        },
        [&](const GridPosition::Explicit& gridPositionExplicit) -> CSS::GridLine {
            return toCSS(gridPositionExplicit, style);
        },
        [&](const GridPosition::Span& gridPositionSpan) -> CSS::GridLine {
            return toCSS(gridPositionSpan, style);
        }
    );
}

auto ToStyle<CSS::GridLine>::operator()(const CSS::GridLine& value, const BuilderState& state) -> GridPosition
{
    return WTF::switchOn(value,
        [&](CSS::Keyword::Auto keyword) -> GridPosition {
            return keyword;
        },
        [&](const CSS::CustomIdent& customIdent) -> GridPosition {
            return toStyle(customIdent, state);
        },
        [&](const CSS::GridLineExplicit& gridLineExplicit) -> GridPosition {
            return toStyle(gridLineExplicit, state);
        },
        [&](const CSS::GridLineSpan& gridLineSpan) -> GridPosition {
            return toStyle(gridLineSpan, state);
        }
    );
}

auto ToCSS<GridPosition::Explicit>::operator()(const GridPosition::Explicit& value, const RenderStyle& style) -> CSS::GridLine::Explicit
{
    return CSS::GridLineExplicit {
        toCSS(value.position, style),
        !value.name.value.isNull() ? std::optional { toCSS(value.name, style) } : std::nullopt,
    };
}

auto ToStyle<CSS::GridLine::Explicit>::operator()(const CSS::GridLine::Explicit& value, const BuilderState& state) -> GridPosition::Explicit
{
    return GridPosition::Explicit {
        toStyle(value.index, state),
        toStyle(value.name, state).value_or(CustomIdent { nullAtom() }),
    };
}

auto ToCSS<GridPosition::Span>::operator()(const GridPosition::Span& value, const RenderStyle& style) -> CSS::GridLine::Span
{
    return CSS::GridLineSpan {
        toCSS(value.position, style),
        !value.name.value.isNull() ? std::optional { toCSS(value.name, style) } : std::nullopt,
    };
}

auto ToStyle<CSS::GridLine::Span>::operator()(const CSS::GridLine::Span& value, const BuilderState& state) -> GridPosition::Span
{
    return GridPosition::Span {
        toStyle(value.index, state),
        toStyle(value.name, state).value_or(CustomIdent { nullAtom() }),
    };
}

auto CSSValueConversion<GridPosition>::operator()(BuilderState& state, const CSSValue& value) -> GridPosition
{
    using namespace CSS::Literals;

    if (auto* keywordValue = dynamicDowncast<CSSKeywordValue>(value)) {
        switch (keywordValue->valueID()) {
        case CSSValueAuto:
            return CSS::Keyword::Auto { };
        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::Auto { };
        }
    }

    if (auto* customIdentValue = dynamicDowncast<CSSCustomIdentValue>(value))
        return toStyleFromCSSValue<CustomIdent>(state, *customIdentValue);

    RefPtr gridLineValue = requiredDowncast<CSSGridLineValue>(state, value);
    if (!gridLineValue)
        return CSS::Keyword::Auto { };

    return toStyle(gridLineValue->line(), state);
}

Ref<CSSValue> CSSValueCreation<GridPosition>::operator()(CSSValuePool& pool, const RenderStyle& style, const GridPosition& value)
{
    return CSS::createCSSValue(pool, toCSS(value, style));
}

} // namespace Style
} // namespace WebCore
