/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "SimpleLineLayoutResolver.h"

#include "RenderBlockFlow.h"
#include "RenderText.h"
#include "SimpleLineLayoutFunctions.h"

namespace WebCore {
namespace SimpleLineLayout {

RunResolver::Run::Run(const Iterator& iterator)
    : m_iterator(iterator)
{
}

LayoutRect RunResolver::Run::rect() const
{
    auto& resolver = m_iterator.resolver();
    auto& run = m_iterator.simpleRun();

    float baselinePosition = resolver.m_lineHeight * m_iterator.lineIndex() + resolver.m_baseline;
    LayoutPoint linePosition(LayoutUnit::fromFloatFloor(run.logicalLeft), roundToInt(baselinePosition - resolver.m_ascent + resolver.m_borderAndPaddingBefore));
    LayoutSize lineSize(LayoutUnit::fromFloatCeil(run.logicalRight) - LayoutUnit::fromFloatFloor(run.logicalLeft), resolver.m_ascent + resolver.m_descent);
    return LayoutRect(linePosition, lineSize);
}

FloatPoint RunResolver::Run::baseline() const
{
    auto& resolver = m_iterator.resolver();
    auto& run = m_iterator.simpleRun();

    float baselinePosition = resolver.m_lineHeight * m_iterator.lineIndex() + resolver.m_baseline;
    return FloatPoint(run.logicalLeft, roundToInt(baselinePosition + resolver.m_borderAndPaddingBefore));
}

StringView RunResolver::Run::text() const
{
    auto& resolver = m_iterator.resolver();
    auto& run = m_iterator.simpleRun();
    return StringView(resolver.m_string).substring(run.start, run.end - run.start);
}

RunResolver::Iterator::Iterator(const RunResolver& resolver, unsigned runIndex, unsigned lineIndex)
    : m_resolver(resolver)
    , m_runIndex(runIndex)
    , m_lineIndex(lineIndex)
{
}

RunResolver::Iterator& RunResolver::Iterator::advance()
{
    if (simpleRun().isEndOfLine)
        ++m_lineIndex;
    ++m_runIndex;
    return *this;
}

RunResolver::Iterator& RunResolver::Iterator::advanceLines(unsigned lineCount)
{
    unsigned runCount = m_resolver.m_layout.runCount();
    if (runCount == m_resolver.m_layout.lineCount()) {
        m_runIndex = std::min(runCount, m_runIndex + lineCount);
        m_lineIndex = m_runIndex;
        return *this;
    }
    unsigned target = m_lineIndex + lineCount;
    while (m_lineIndex < target && m_runIndex < runCount)
        advance();

    return *this;
}

RunResolver::RunResolver(const RenderBlockFlow& flow, const Layout& layout)
    : m_layout(layout)
    , m_string(downcast<RenderText>(*flow.firstChild()).text())
    , m_lineHeight(lineHeightFromFlow(flow))
    , m_baseline(baselineFromFlow(flow))
    , m_borderAndPaddingBefore(flow.borderAndPaddingBefore())
    , m_ascent(flow.style().font().fontMetrics().ascent())
    , m_descent(flow.style().font().fontMetrics().descent())
{
}

unsigned RunResolver::lineIndexForHeight(LayoutUnit height, IndexType type) const
{
    ASSERT(m_lineHeight);
    float y = height - m_borderAndPaddingBefore;
    // Lines may overlap, adjust to get the first or last line at this height.
    if (type == IndexType::First)
        y += m_lineHeight - (m_baseline + m_descent);
    else
        y -= m_baseline - m_ascent;
    y = std::max<float>(y, 0);
    return std::min<unsigned>(y / m_lineHeight, m_layout.lineCount() - 1);
}

Range<RunResolver::Iterator> RunResolver::rangeForRect(const LayoutRect& rect) const
{ 
    if (!m_lineHeight)
        return Range<Iterator>(begin(), end());

    unsigned firstLine = lineIndexForHeight(rect.y(), IndexType::First);
    unsigned lastLine = std::max(firstLine, lineIndexForHeight(rect.maxY(), IndexType::Last));

    auto rangeBegin = begin().advanceLines(firstLine);
    if (rangeBegin == end())
        return Range<Iterator>(end(), end());
    auto rangeEnd = rangeBegin;
    ASSERT(lastLine >= firstLine);
    rangeEnd.advanceLines(lastLine - firstLine + 1);
    return Range<Iterator>(rangeBegin, rangeEnd);
}

LineResolver::Iterator::Iterator(RunResolver::Iterator runIterator)
    : m_runIterator(runIterator)
{
}

const LayoutRect LineResolver::Iterator::operator*() const
{
    unsigned currentLine = m_runIterator.lineIndex();
    auto it = m_runIterator;
    LayoutRect rect = (*it).rect();
    while (it.advance().lineIndex() == currentLine)
        rect.unite((*it).rect());

    return rect;
}

LineResolver::LineResolver(const RenderBlockFlow& flow, const Layout& layout)
    : m_runResolver(flow, layout)
{
}

}
}
