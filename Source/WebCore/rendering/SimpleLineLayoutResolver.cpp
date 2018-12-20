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

String RunResolver::Run::textWithHyphen() const
{
    auto& run = m_iterator.simpleRun();
    ASSERT(run.hasHyphen);
    // Empty runs should not have hyphen.
    ASSERT(run.start < run.end);
    auto& segment = m_iterator.resolver().m_flowContents.segmentForRun(run.start, run.end);
    auto text = StringView(segment.text).substring(segment.toSegmentPosition(run.start), run.end - run.start);
    return makeString(text, m_iterator.resolver().flow().style().hyphenString());
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
    auto& run = m_iterator.simpleRun();
    ASSERT(run.start < run.end);
    auto& segment = m_iterator.resolver().m_flowContents.segmentForRun(run.start, run.end);
    // We currently split runs on segment boundaries (different RenderObject).
    ASSERT(run.end <= segment.end);
    return StringView(segment.text).substring(segment.toSegmentPosition(run.start), run.end - run.start);
}

const RenderObject& RunResolver::Run::renderer() const
{
    auto& run = m_iterator.simpleRun();
    // FlowContents cannot differentiate empty runs.
    ASSERT(run.start != run.end);
    return m_iterator.resolver().m_flowContents.segmentForRun(run.start, run.end).renderer;
}

unsigned RunResolver::Run::localStart() const
{
    auto& run = m_iterator.simpleRun();
    // FlowContents cannot differentiate empty runs.
    ASSERT(run.start != run.end);
    return m_iterator.resolver().m_flowContents.segmentForRun(run.start, run.end).toSegmentPosition(run.start);
}

unsigned RunResolver::Run::localEnd() const
{
    auto& run = m_iterator.simpleRun();
    // FlowContents cannot differentiate empty runs.
    ASSERT(run.start != run.end);
    return m_iterator.resolver().m_flowContents.segmentForRun(run.start, run.end).toSegmentPosition(run.end);
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

unsigned RunResolver::adjustLineIndexForStruts(LayoutUnit y, IndexType type, unsigned lineIndexCandidate) const
{
    auto& struts = m_layout.struts();
    // We need to offset the lineIndex with line struts when there's an actual strut before the candidate.
    auto& strut = struts.first();
    if (strut.lineBreak >= lineIndexCandidate)
        return lineIndexCandidate;
    unsigned strutIndex = 0;
    Optional<unsigned> lastIndexCandidate;
    auto top = strut.lineBreak * m_lineHeight;
    auto lineHeightWithOverflow = m_lineHeight;
    // If font is larger than the line height (glyphs overflow), use the font size when checking line boundaries.
    if (m_ascent + m_descent > m_lineHeight) {
        lineHeightWithOverflow = m_ascent + m_descent;
        top += m_baseline - m_ascent;
    }
    auto bottom = top + lineHeightWithOverflow;
    for (auto lineIndex = strut.lineBreak; lineIndex < m_layout.lineCount(); ++lineIndex) {
        float strutOffset = 0;
        if (strutIndex < struts.size() && struts.at(strutIndex).lineBreak == lineIndex)
            strutOffset = struts.at(strutIndex++).offset;
        bottom = top + strutOffset + lineHeightWithOverflow;
        if (y >= top && y < bottom) {
            if (type == IndexType::First)
                return lineIndex;
            lastIndexCandidate = lineIndex;
        } else if (lastIndexCandidate)
            return *lastIndexCandidate;
        top += m_lineHeight + strutOffset;
    }
    if (lastIndexCandidate || y >= bottom)
        return m_layout.lineCount() - 1;
    // We missed the line.
    ASSERT_NOT_REACHED();
    return lineIndexCandidate;
}

unsigned RunResolver::lineIndexForHeight(LayoutUnit height, IndexType type) const
{
    ASSERT(m_lineHeight);
    float y = height - m_borderAndPaddingBefore;
    // Lines may overlap, adjust to get the first or last line at this height.
    auto adjustedY = y;
    if (type == IndexType::First)
        adjustedY += m_lineHeight - (m_baseline + m_descent);
    else
        adjustedY -= m_baseline - m_ascent;
    adjustedY = std::max<float>(adjustedY, 0);
    auto lineIndexCandidate =  std::min<unsigned>(adjustedY / m_lineHeight, m_layout.lineCount() - 1);
    if (m_layout.hasLineStruts())
        return adjustLineIndexForStruts(y, type, lineIndexCandidate);
    return lineIndexCandidate;
}

WTF::IteratorRange<RunResolver::Iterator> RunResolver::rangeForRect(const LayoutRect& rect) const
{ 
    if (!m_lineHeight)
        return { begin(), end() };

    unsigned firstLine = lineIndexForHeight(rect.y(), IndexType::First);
    unsigned lastLine = std::max(firstLine, lineIndexForHeight(rect.maxY(), IndexType::Last));
    auto rangeBegin = begin().advanceLines(firstLine);
    if (rangeBegin == end())
        return { end(), end() };
    auto rangeEnd = rangeBegin;
    ASSERT(lastLine >= firstLine);
    rangeEnd.advanceLines(lastLine - firstLine + 1);
    return { rangeBegin, rangeEnd };
}

WTF::IteratorRange<RunResolver::Iterator> RunResolver::rangeForLine(unsigned lineIndex) const
{
    auto rangeBegin = begin().advanceLines(lineIndex);
    if (rangeBegin == end())
        return { end(), end() };
    auto rangeEnd = rangeBegin;
    rangeEnd.advanceLines(1);
    return { rangeBegin, rangeEnd };
}

WTF::IteratorRange<RunResolver::Iterator> RunResolver::rangeForRenderer(const RenderObject& renderer) const
{
    if (begin() == end())
        return { end(), end() };
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
        return { end(), end() };

    auto rangeBegin = run;
    // Move beyond the end of the segment.
    while (run != end() && (*run).start() < segment->end)
        ++run;
    // Special case when segment == run.
    if (run == rangeBegin)
        ++run;
    return { rangeBegin, run };
}

RunResolver::Iterator RunResolver::runForPoint(const LayoutPoint& point) const
{
    if (!m_lineHeight)
        return end();
    if (begin() == end())
        return end();
    unsigned lineIndex = lineIndexForHeight(point.y(), IndexType::Last);
    auto x = point.x() - m_borderAndPaddingBefore;
    auto it = begin();
    it.advanceLines(lineIndex);
    // Point is at the left side of the first run on this line.
    if ((*it).logicalLeft() > x)
        return it;
    // Advance to the first candidate run on this line.
    while (it != end() && (*it).logicalRight() < x && lineIndex == it.lineIndex())
        ++it;
    // We jumped to the next line so the point is at the right side of the previous line.
    if (it.lineIndex() > lineIndex)
        return --it;
    // Now we have a candidate run.
    // Find the last run that still contains this point (taking overlapping runs with odd word spacing values into account).
    while (it != end() && (*it).logicalLeft() <= x && lineIndex == it.lineIndex())
        ++it;
    return --it;
}

WTF::IteratorRange<RunResolver::Iterator> RunResolver::rangeForRendererWithOffsets(const RenderObject& renderer, unsigned startOffset, unsigned endOffset) const
{
    ASSERT(startOffset <= endOffset);
    auto range = rangeForRenderer(renderer);
    auto it = range.begin();
    // Advance to the firt run with the start offset inside.
    while (it != range.end() && (*it).end() <= startOffset)
        ++it;
    if (it == range.end())
        return { end(), end() };
    auto rangeBegin = it;
    // Special case empty ranges that start at the edge of the run. Apparently normal line layout include those.
    if (endOffset == startOffset && (*it).start() == endOffset)
        return { rangeBegin, ++it };
    // Advance beyond the last run with the end offset.
    while (it != range.end() && (*it).start() < endOffset)
        ++it;
    return { rangeBegin, it };
}

LineResolver::Iterator::Iterator(RunResolver::Iterator runIterator)
    : m_runIterator(runIterator)
{
}

FloatRect LineResolver::Iterator::operator*() const
{
    unsigned currentLine = m_runIterator.lineIndex();
    auto it = m_runIterator;
    FloatRect rect = (*it).rect();
    while (it.advance().lineIndex() == currentLine)
        rect.unite((*it).rect());
    return rect;
}

const RenderObject& LineResolver::Iterator::renderer() const
{
    // FIXME: This works as long as we've got only one renderer per line.
    auto run = *m_runIterator;
    return m_runIterator.resolver().flowContents().segmentForRun(run.start(), run.end()).renderer;
}

LineResolver::LineResolver(const RunResolver& runResolver)
    : m_runResolver(runResolver)
{
}

}
}
