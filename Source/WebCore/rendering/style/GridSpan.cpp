/**
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GridSpan.h"

#include "StyleGridPosition.h"

namespace WebCore {

GridSpan GridSpan::untranslatedDefiniteGridSpan(int startLine, int endLine)
{
    return GridSpan(startLine, endLine, UntranslatedDefinite);
}

GridSpan GridSpan::translatedDefiniteGridSpan(unsigned startLine, unsigned endLine)
{
    return GridSpan(startLine, endLine, TranslatedDefinite);
}

GridSpan GridSpan::indefiniteGridSpan()
{
    return GridSpan(0, 1, Indefinite);
}

GridSpan GridSpan::masonryAxisTranslatedDefiniteGridSpan()
{
    return GridSpan(0, 1, TranslatedDefinite);
}

unsigned GridSpan::integerSpan() const
{
    ASSERT(!isIndefinite());
    return m_endLine - m_startLine;
}

int GridSpan::untranslatedStartLine() const
{
    ASSERT(m_type == UntranslatedDefinite);
    return m_startLine;
}

int GridSpan::untranslatedEndLine() const
{
    ASSERT(m_type == UntranslatedDefinite);
    return m_endLine;
}

unsigned GridSpan::startLine() const
{
    ASSERT(isTranslatedDefinite());
    ASSERT(m_endLine >= 0);
    return m_startLine;
}

unsigned GridSpan::endLine() const
{
    ASSERT(isTranslatedDefinite());
    ASSERT(m_endLine > 0);
    return m_endLine;
}


GridSpan::GridSpanIterator GridSpan::begin() const
{
    ASSERT(isTranslatedDefinite());
    return m_startLine;
}

GridSpan::GridSpanIterator GridSpan::end() const
{
    ASSERT(isTranslatedDefinite());
    return m_endLine;
}

bool GridSpan::isTranslatedDefinite() const
{
    return m_type == TranslatedDefinite;
}

bool GridSpan::isIndefinite() const
{
    return m_type == Indefinite;
}

void GridSpan::translate(unsigned offset)
{
    ASSERT(m_type == UntranslatedDefinite);

    m_type = TranslatedDefinite;
    m_startLine += offset;
    m_endLine += offset;

    ASSERT(m_startLine >= 0);
    ASSERT(m_endLine > 0);
}

// Moves this span to be in the same coordinate space as |parent|.
// If reverse is specified, then swaps the direction to handle RTL/LTR changes.
void GridSpan::translateTo(const GridSpan& parent, bool reverse)
{
    ASSERT(m_type == TranslatedDefinite);
    ASSERT(parent.m_type == TranslatedDefinite);
    if (reverse) {
        int start = m_startLine;
        m_startLine = parent.endLine() - m_endLine;
        m_endLine = parent.endLine() - start;
    } else {
        m_startLine += parent.m_startLine;
        m_endLine += parent.m_startLine;
    }
}

void GridSpan::clamp(int max)
{
    ASSERT(m_type != Indefinite);
    m_startLine = std::max(m_startLine, 0);
    m_endLine = std::max(std::min(m_endLine, max), 1);
    if (m_startLine >= m_endLine)
        m_startLine = m_endLine - 1;
}

bool GridSpan::clamp(int min, int max)
{
    ASSERT(min < max);
    ASSERT(m_startLine < m_endLine);
    ASSERT(m_type != Indefinite);
    if (min >= m_endLine || max <= m_startLine)
        return false;
    m_startLine = std::max(m_startLine, min);
    m_endLine = std::min(m_endLine, max);
    ASSERT(m_startLine < m_endLine);
    return true;
}


GridSpan::GridSpan(int startLine, int endLine, GridSpanType type)
    : m_type(type)
{
#if ASSERT_ENABLED
    ASSERT(startLine < endLine);
    if (type == TranslatedDefinite) {
        ASSERT(startLine >= 0);
        ASSERT(endLine > 0);
    }
#endif

    m_startLine = std::max(Style::GridPosition::min(), std::min(startLine, Style::GridPosition::max() - 1));
    m_endLine = std::max(Style::GridPosition::min() + 1, std::min(endLine, Style::GridPosition::max()));
}

}
