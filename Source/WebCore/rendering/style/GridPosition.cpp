/*
 * Copyright (C) 2017 Igalia S.L.
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
#include "GridPosition.h"

#include <wtf/text/TextStream.h>

namespace WebCore {

std::optional<int> GridPosition::gMaxPositionForTesting;
static const int kGridMaxPosition = 1000000;

void GridPosition::setExplicitPosition(int position, const String& namedGridLine)
{
    m_type = GridPositionType::ExplicitPosition;
    setIntegerPosition(position);
    m_namedGridLine = namedGridLine;
}

void GridPosition::setAutoPosition()
{
    m_type = GridPositionType::AutoPosition;
    m_integerPosition = 0;
}

// 'span' values cannot be negative, yet we reuse the <integer> position which can
// be. This means that we have to convert the span position to an integer, losing
// some precision here. It shouldn't be an issue in practice though.
void GridPosition::setSpanPosition(int position, const String& namedGridLine)
{
    m_type = GridPositionType::SpanPosition;
    setIntegerPosition(position);
    m_namedGridLine = namedGridLine;
}

void GridPosition::setNamedGridArea(const String& namedGridArea)
{
    m_type = GridPositionType::NamedGridAreaPosition;
    m_namedGridLine = namedGridArea;
}

int GridPosition::integerPosition() const
{
    ASSERT(type() == GridPositionType::ExplicitPosition);
    return m_integerPosition;
}

String GridPosition::namedGridLine() const
{
    ASSERT(type() == GridPositionType::ExplicitPosition || type() == GridPositionType::SpanPosition || type() == GridPositionType::NamedGridAreaPosition);
    return m_namedGridLine;
}

int GridPosition::spanPosition() const
{
    ASSERT(type() == GridPositionType::SpanPosition);
    return m_integerPosition;
}

int GridPosition::max()
{
    return gMaxPositionForTesting.value_or(kGridMaxPosition);
}

int GridPosition::min()
{
    return -max();
}

void GridPosition::setMaxPositionForTesting(unsigned maxPosition)
{
    gMaxPositionForTesting = static_cast<int>(maxPosition);
}

TextStream& operator<<(TextStream& ts, const GridPosition& o)
{
    switch (o.type()) {
    case GridPositionType::AutoPosition:
        return ts << "auto";
    case GridPositionType::ExplicitPosition:
        return ts << o.namedGridLine() << " " << o.integerPosition();
    case GridPositionType::SpanPosition:
        return ts << "span" << " " << o.namedGridLine() << " " << o.integerPosition();
    case GridPositionType::NamedGridAreaPosition:
        return ts << o.namedGridLine();
    }
    return ts;
}

} // namespace WebCore
