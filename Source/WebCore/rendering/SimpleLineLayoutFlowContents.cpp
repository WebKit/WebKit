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
#include "SimpleLineLayoutFlowContents.h"

#include "RenderBlockFlow.h"
#include "RenderChildIterator.h"
#include "RenderText.h"

namespace WebCore {
namespace SimpleLineLayout {

FlowContents::Style::Style(const RenderStyle& style)
    : font(style.font())
    , textAlign(style.textAlign())
    , collapseWhitespace(style.collapseWhiteSpace())
    , preserveNewline(style.preserveNewline())
    , wrapLines(style.autoWrap())
    , breakWordOnOverflow(style.overflowWrap() == BreakOverflowWrap && (wrapLines || preserveNewline))
    , spaceWidth(font.width(TextRun(&space, 1)))
    , tabWidth(collapseWhitespace ? 0 : style.tabSize())
    , locale(style.locale())
{
}

static Vector<FlowContents::Segment> initializeSegments(const RenderBlockFlow& flow)
{
    Vector<FlowContents::Segment, 8> segments;
    unsigned startPosition = 0;
    for (auto& textChild : childrenOfType<RenderText>(flow)) {
        unsigned textLength = textChild.text()->length();
        segments.append(FlowContents::Segment { startPosition, startPosition + textLength, textChild });
        startPosition += textLength;
    }
    return segments;
}

FlowContents::FlowContents(const RenderBlockFlow& flow)
    : m_style(flow.style())
    , m_segments(initializeSegments(flow))
    , m_lineBreakIterator(downcast<RenderText>(*flow.firstChild()).text(), flow.style().locale())
    , m_lastSegmentIndex(0)
{
}

unsigned FlowContents::findNextBreakablePosition(unsigned position) const
{
    String string = m_lineBreakIterator.string();
    unsigned breakablePosition = nextBreakablePositionNonLoosely<LChar, NBSPBehavior::IgnoreNBSP>(m_lineBreakIterator, string.characters8(), string.length(), position);
    if (appendNextRendererContentIfNeeded(breakablePosition))
        return findNextBreakablePosition(position);
    ASSERT(breakablePosition >= position);
    return breakablePosition;
}

unsigned FlowContents::findNextNonWhitespacePosition(unsigned position, unsigned& spaceCount) const
{
    unsigned nonWhitespacePosition = nextNonWhitespacePosition(position, spaceCount);
    if (appendNextRendererContentIfNeeded(nonWhitespacePosition))
        return findNextNonWhitespacePosition(position, spaceCount);
    ASSERT(nonWhitespacePosition >= position);
    return nonWhitespacePosition;
}

float FlowContents::textWidth(unsigned from, unsigned to, float xPosition) const
{
    auto& fromSegment = segmentForPosition(from);

    if ((m_style.font.isFixedPitch() && fromSegment.end >= to) || (from == fromSegment.start && to == fromSegment.end))
        return fromSegment.renderer.width(from - fromSegment.start, to - from, m_style.font, xPosition, nullptr, nullptr);

    auto* segment = &fromSegment;
    float textWidth = 0;
    unsigned fragmentEnd = 0;
    while (true) {
        fragmentEnd = std::min(to, segment->end);
        textWidth += runWidth(segment->renderer, from - segment->start, fragmentEnd - segment->start, xPosition + textWidth);
        if (fragmentEnd == to)
            break;
        from = fragmentEnd;
        segment = &segmentForPosition(fragmentEnd);
    };

    return textWidth;
}

const FlowContents::Segment& FlowContents::segmentForPositionSlow(unsigned position) const
{
    auto it = std::lower_bound(m_segments.begin(), m_segments.end(), position, [](const Segment& segment, unsigned position) {
        return segment.end <= position;
    });
    ASSERT(it != m_segments.end());
    m_lastSegmentIndex = it - m_segments.begin();
    return *it;
}

const FlowContents::Segment& FlowContents::segmentForRenderer(const RenderText& renderer) const
{
    for (auto& segment : m_segments) {
        if (&segment.renderer == &renderer)
            return segment;
    }
    ASSERT_NOT_REACHED();
    return m_segments.last();
}

bool FlowContents::appendNextRendererContentIfNeeded(unsigned position) const
{
    if (isEnd(position))
        return false;
    String string = m_lineBreakIterator.string();
    if (position < string.length())
        return false;

    // Content needs to be requested sequentially.
    ASSERT(position == string.length());
    auto& segment = segmentForPosition(position);

    m_lineBreakIterator.resetStringAndReleaseIterator(string + String(segment.renderer.text()), m_style.locale, LineBreakIteratorModeUAX14);
    return true;
}

unsigned FlowContents::nextNonWhitespacePosition(unsigned position, unsigned& spaceCount) const
{
    String string = m_lineBreakIterator.string();
    unsigned length = string.length();
    const LChar* text = string.characters8();
    for (; position < length; ++position) {
        bool isSpace = text[position] == ' ';
        if (!(isSpace || text[position] == '\t' || (!m_style.preserveNewline && text[position] == '\n')))
            return position;
        if (isSpace)
            ++spaceCount;
    }
    return length;
}

float FlowContents::runWidth(const RenderText& renderer, unsigned from, unsigned to, float xPosition) const
{
    ASSERT(from < to);
    String string = renderer.text();
    bool measureWithEndSpace = m_style.collapseWhitespace && to < string.length() && string[to] == ' ';
    if (measureWithEndSpace)
        ++to;
    TextRun run(string.characters8() + from, to - from);
    run.setXPos(xPosition);
    run.setTabSize(!!m_style.tabWidth, m_style.tabWidth);
    float width = m_style.font.width(run);
    if (measureWithEndSpace)
        width -= m_style.spaceWidth;
    return width;
}

}
}
