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

#include "InlineTextBoxStyle.h"
#include "RenderBlockFlow.h"
#include "RenderObject.h"
#include "SimpleLineLayoutFunctions.h"

namespace WebCore {
namespace SimpleLineLayout {

static FloatPoint linePosition(float logicalLeft, float logicalTop)
{
    return FloatPoint(logicalLeft, roundf(logicalTop));
}

static FloatSize lineSize(float logicalLeft, float logicalRight, float height)
{
    return FloatSize(logicalRight - logicalLeft, height);
}

RunResolver::Run::Run(const Iterator& iterator)
    : m_iterator(iterator)
{
}

FloatRect RunResolver::Run::rect() const
{
    auto& run = m_iterator.simpleRun();
    auto& resolver = m_iterator.resolver();
    float baseline = computeBaselinePosition();
    FloatPoint position = linePosition(run.logicalLeft, baseline - resolver.m_ascent);
    FloatSize size = lineSize(run.logicalLeft, run.logicalRight, resolver.m_ascent + resolver.m_descent + resolver.m_visualOverflowOffset);
    bool moveLineBreakToBaseline = false;
    if (run.start == run.end && m_iterator != resolver.begin() && m_iterator.inQuirksMode()) {
        auto previousRun = m_iterator;
        --previousRun;
        moveLineBreakToBaseline = !previousRun.simpleRun().isEndOfLine;
    }
    if (moveLineBreakToBaseline)
        return FloatRect(FloatPoint(position.x(), baseline), FloatSize(size.width(), std::max<float>(0, resolver.m_ascent - resolver.m_baseline.toFloat())));
    return FloatRect(position, size);
}

StringView RunResolver::Run::text() const
{
    auto& resolver = m_iterator.resolver();
    auto& run = m_iterator.simpleRun();
    ASSERT(run.start < run.end);
    auto& segment = resolver.m_flowContents.segmentForRun(run.start, run.end);
    // We currently split runs on segment boundaries (different RenderObject).
    ASSERT(run.end <= segment.end);
    if (segment.text.is8Bit())
        return StringView(segment.text.characters8(), segment.text.length()).substring(run.start - segment.start, run.end - run.start);
    return StringView(segment.text.characters16(), segment.text.length()).substring(run.start - segment.start, run.end - run.start);
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
    : m_flowRenderer(flow)
    , m_layout(layout)
    , m_flowContents(flow)
    , m_lineHeight(lineHeightFromFlow(flow))
    , m_baseline(baselineFromFlow(flow))
    , m_borderAndPaddingBefore(flow.borderAndPaddingBefore())
    , m_ascent(flow.style().fontCascade().fontMetrics().ascent())
    , m_descent(flow.style().fontCascade().fontMetrics().descent())
    , m_visualOverflowOffset(visualOverflowForDecorations(flow.style(), nullptr).bottom)
    , m_inQuirksMode(flow.document().inQuirksMode())
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

Range<RunResolver::Iterator> RunResolver::rangeForRenderer(const RenderObject& renderer) const
{
    if (begin() == end())
        return Range<Iterator>(end(), end());
    FlowContents::Iterator segment = m_flowContents.begin();
    auto run = begin();
    ASSERT(segment->start <= (*run).start());
    // Move run to the beginning of the segment.
    while (&segment->renderer != &renderer && run != end()) {
        if ((*run).start() == segment->start && (*run).end() == segment->end) {
            ++run;
            ++segment;
        } else if ((*run).start() < segment->end)
            ++run;
        else
            ++segment;
        ASSERT(segment != m_flowContents.end());
    }
    // Do we actually have a run for this renderer?
    // Collapsed whitespace with dedicated renderer could end up with no run at all.
    if (run == end() || (segment->start != segment->end && segment->end <= (*run).start()))
        return Range<Iterator>(end(), end());

    auto rangeBegin = run;
    // Move beyond the end of the segment.
    while (run != end() && (*run).start() < segment->end)
        ++run;
    // Special case when segment == run.
    if (run == rangeBegin)
        ++run;
    return Range<Iterator>(rangeBegin, run);
}

LineResolver::Iterator::Iterator(RunResolver::Iterator runIterator)
    : m_runIterator(runIterator)
{
}

const FloatRect LineResolver::Iterator::operator*() const
{
    unsigned currentLine = m_runIterator.lineIndex();
    auto it = m_runIterator;
    FloatRect rect = (*it).rect();
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
