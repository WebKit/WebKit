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
    : font(style.fontCascade())
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
        segments.append(FlowContents::Segment { startPosition, startPosition + textLength, textChild.text(), textChild });
        startPosition += textLength;
    }
    return segments;
}

FlowContents::FlowContents(const RenderBlockFlow& flow)
    : m_style(flow.style())
    , m_segments(initializeSegments(flow))
    , m_lineBreakIterator(m_segments[0].text, flow.style().locale())
    , m_lastSegmentIndex(0)
{
}

FlowContents::TextFragment FlowContents::nextTextFragment(float xPosition)
{
    // A fragment can either be
    // 1. new line character when preserveNewline is on (not considered as whitespace) or
    // 2. whitespace (collasped, non-collapsed multi or single) or
    // 3. non-whitespace characters.
    // 4. empty, indicating content end.
    TextFragment fragment;
    fragment.start = m_position;
    if (isEnd(fragment.start)) {
        fragment.end = fragment.start;
        return fragment;
    }
    if (isLineBreak(fragment.start)) {
        fragment.type = TextFragment::LineBreak;
        fragment.end = fragment.start + 1;
        m_position = fragment.end;
        return fragment;
    }

    unsigned spaceCount = 0;
    unsigned whitespaceEnd = findNextNonWhitespacePosition(fragment.start, spaceCount);
    ASSERT(fragment.start <= whitespaceEnd);
    if (fragment.start != whitespaceEnd) {
        fragment.type = TextFragment::Whitespace;
        fragment.end = whitespaceEnd;
        bool multipleWhitespace = fragment.start + 1 < fragment.end;
        fragment.isCollapsed = multipleWhitespace && m_style.collapseWhitespace;
        fragment.isBreakable = !fragment.isCollapsed && multipleWhitespace;
        if (fragment.isCollapsed)
            fragment.width = m_style.spaceWidth;
        else {
            unsigned fragmentLength = fragment.end - fragment.start;
            if (fragmentLength == spaceCount)
                fragment.width = fragmentLength * m_style.spaceWidth;
            else
                fragment.width = textWidth(fragment.start, fragment.end, xPosition);
        }
        m_position = fragment.end;
        return fragment;
    }

    fragment.type = TextFragment::NonWhitespace;
    fragment.isBreakable = m_style.breakWordOnOverflow;
    fragment.end = findNextBreakablePosition(fragment.start + 1);
    fragment.width = textWidth(fragment.start, fragment.end, xPosition);
    m_position = fragment.end;
    return fragment;
}

template <typename CharacterType>
static unsigned nextBreakablePosition(LazyLineBreakIterator& lineBreakIterator, const FlowContents::Segment& segment, unsigned position)
{
    const auto* characters = segment.text.characters<CharacterType>();
    unsigned segmentLength = segment.end - segment.start;
    unsigned segmentPosition = position - segment.start;
    return nextBreakablePositionNonLoosely<CharacterType, NBSPBehavior::IgnoreNBSP>(lineBreakIterator, characters, segmentLength, segmentPosition);
}

unsigned FlowContents::findNextBreakablePosition(unsigned position) const
{
    while (!isEnd(position)) {
        auto& segment = segmentForPosition(position);
        if (segment.text.impl() != m_lineBreakIterator.string().impl()) {
            UChar lastCharacter = segment.start > 0 ? characterAt(segment.start - 1) : 0;
            UChar secondToLastCharacter = segment.start > 1 ? characterAt(segment.start - 2) : 0;
            m_lineBreakIterator.setPriorContext(lastCharacter, secondToLastCharacter);
            m_lineBreakIterator.resetStringAndReleaseIterator(segment.text, m_style.locale, LineBreakIteratorModeUAX14);
        }

        unsigned breakable = segment.text.is8Bit() ? nextBreakablePosition<LChar>(m_lineBreakIterator, segment, position) : nextBreakablePosition<UChar>(m_lineBreakIterator, segment, position);
        position = segment.start + breakable;
        if (position < segment.end)
            break;
    }
    return position;
}

template <typename CharacterType>
static bool findNextNonWhitespace(const FlowContents::Segment& segment, const FlowContents::Style& style, unsigned& position, unsigned& spaceCount)
{
    const auto* text = segment.text.characters<CharacterType>();
    for (; position < segment.end; ++position) {
        auto character = text[position - segment.start];
        bool isSpace = character == ' ';
        bool isWhitespace = isSpace || character == '\t' || (!style.preserveNewline && character == '\n');
        if (!isWhitespace)
            return true;
        if (isSpace)
            ++spaceCount;
    }
    return false;
}

unsigned FlowContents::findNextNonWhitespacePosition(unsigned position, unsigned& spaceCount) const
{
    for (unsigned i = segmentIndexForPosition(position); i < m_segments.size(); ++i) {
        bool foundNonWhitespace = m_segments[i].text.is8Bit() ? findNextNonWhitespace<LChar>(m_segments[i], m_style, position, spaceCount) :
            findNextNonWhitespace<UChar>(m_segments[i], m_style, position, spaceCount);
        if (foundNonWhitespace)
            break;
    }
    return position;
}

float FlowContents::textWidth(unsigned from, unsigned to, float xPosition) const
{
    const auto& fromSegment = segmentForPosition(from);

    if ((m_style.font.isFixedPitch() && fromSegment.end >= to) || (from == fromSegment.start && to == fromSegment.end))
        return fromSegment.renderer.width(from - fromSegment.start, to - from, m_style.font, xPosition, nullptr, nullptr);

    const auto* segment = &fromSegment;
    float textWidth = 0;
    unsigned fragmentEnd = 0;
    while (true) {
        fragmentEnd = std::min(to, segment->end);
        textWidth += segment->text.is8Bit() ? runWidth<LChar>(segment->text, from - segment->start, fragmentEnd - segment->start, xPosition + textWidth) :
            runWidth<UChar>(segment->text, from - segment->start, fragmentEnd - segment->start, xPosition + textWidth);
        if (fragmentEnd == to)
            break;
        from = fragmentEnd;
        segment = &segmentForPosition(fragmentEnd);
    };

    return textWidth;
}

unsigned FlowContents::segmentIndexForPositionSlow(unsigned position) const
{
    auto it = std::lower_bound(m_segments.begin(), m_segments.end(), position, [](const Segment& segment, unsigned position) {
        return segment.end <= position;
    });
    ASSERT(it != m_segments.end());
    auto index = it - m_segments.begin();
    m_lastSegmentIndex = index;
    return index;
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

template <typename CharacterType>
float FlowContents::runWidth(const String& text, unsigned from, unsigned to, float xPosition) const
{
    ASSERT(from < to);
    bool measureWithEndSpace = m_style.collapseWhitespace && to < text.length() && text[to] == ' ';
    if (measureWithEndSpace)
        ++to;
    TextRun run(text.characters<CharacterType>() + from, to - from);
    run.setXPos(xPosition);
    run.setTabSize(!!m_style.tabWidth, m_style.tabWidth);
    float width = m_style.font.width(run);
    if (measureWithEndSpace)
        width -= m_style.spaceWidth;
    return width;
}

}
}
