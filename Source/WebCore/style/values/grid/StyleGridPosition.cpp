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

#include "CSSGridLineValue.h"
#include "CSSPrimitiveValue.h"
#include "StyleBuilderChecking.h"
#include "StylePrimitiveNumericTypes+Logging.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

static std::optional<int> maxPositionForTesting;
static constexpr auto gridMaxPosition = 1000000;

static int clampGridIntegerPosition(int integerPosition)
{
    return clampTo(integerPosition, GridPosition::min(), GridPosition::max());
}

GridPosition::GridPosition(GridPosition::Explicit&& explicitPosition)
    : m_type { GridPositionType::Explicit }
    , m_integerPosition { clampGridIntegerPosition(explicitPosition.position.value) }
    , m_namedGridLine { WTFMove(explicitPosition.name) }
{
}

GridPosition::GridPosition(GridPosition::Span&& spanPosition)
    : m_type { GridPositionType::Span }
    , m_integerPosition { clampGridIntegerPosition(spanPosition.position.value) }
    , m_namedGridLine { WTFMove(spanPosition.name) }
{
}

GridPosition::GridPosition(CustomIdentifier&& namedGridAreaPosition)
    : m_type { GridPositionType::NamedGridArea }
    , m_namedGridLine { WTFMove(namedGridAreaPosition.value) }
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

String GridPosition::namedGridLine() const
{
    ASSERT(m_type == GridPositionType::Explicit || m_type == GridPositionType::Span || m_type == GridPositionType::NamedGridArea);
    return m_namedGridLine.value;
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

auto CSSValueConversion<GridPosition>::operator()(BuilderState& state, const CSSValue& value) -> GridPosition
{
    if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        if (isValueID(*primitiveValue, CSSValueAuto))
            return CSS::Keyword::Auto { };

        if (primitiveValue->isCustomIdent())
            return CustomIdentifier { AtomString { primitiveValue->stringValue() } };

        state.setCurrentPropertyInvalidAtComputedValueTime();
        return CSS::Keyword::Auto { };
    }

    RefPtr gridLineValue = requiredDowncast<CSSGridLineValue>(state, value);
    if (!gridLineValue)
        return CSS::Keyword::Auto { };

    RefPtr uncheckedSpanValue = gridLineValue->spanValue();
    RefPtr uncheckedNumericValue = gridLineValue->numericValue();
    RefPtr uncheckedGridLineName = gridLineValue->gridLineName();

    auto gridLineNumber = uncheckedNumericValue && uncheckedNumericValue->isInteger() ? uncheckedNumericValue->resolveAsInteger(state.cssToLengthConversionData()) : 0;
    auto gridLineName = uncheckedGridLineName && uncheckedGridLineName->isCustomIdent() ? AtomString { uncheckedGridLineName->stringValue() } : nullAtom();

    if (isValueID(uncheckedSpanValue, CSSValueSpan))
        return GridPosition::Span { { gridLineNumber > 0 ? gridLineNumber : 1 }, CustomIdentifier { WTFMove(gridLineName) } };

    return GridPosition::Explicit { { gridLineNumber }, CustomIdentifier { WTFMove(gridLineName) } };
}

// MARK: - Logging

TextStream& operator<<(TextStream& ts, const GridPosition& value)
{
    WTF::switchOn(value,
        [&](const CSS::Keyword::Auto&) {
            ts << "auto"_s;
        },
        [&](const Style::GridPosition::Explicit& explicitPosition) {
            ts << explicitPosition.name << ' ' << explicitPosition.position;
        },
        [&](const Style::GridPosition::Span& spanPosition) {
            ts << "span"_s << ' ' << spanPosition.name << ' ' << spanPosition.position;
        },
        [&](const CustomIdentifier& namedGridAreaPosition) {
            ts << namedGridAreaPosition.value;
        }
    );
    return ts;
}

} // namespace Style
} // namespace WebCore
