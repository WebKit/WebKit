/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "SimpleLineLayoutTextFragmentIterator.h"

#include "RenderBlockFlow.h"
#include "RenderChildIterator.h"
#include "SimpleLineLayoutFlowContents.h"

namespace WebCore {
namespace SimpleLineLayout {

TextFragmentIterator::Style::Style(const RenderStyle& style)
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

TextFragmentIterator::TextFragmentIterator(const RenderBlockFlow& flow)
    : m_flowContents(flow)
    , m_lineBreakIterator((*m_flowContents.begin()).text, flow.style().locale())
    , m_style(flow.style())
{
}

TextFragmentIterator::TextFragment TextFragmentIterator::nextTextFragment(float xPosition)
{
    // A fragment can either be
    // 1. new line character when preserveNewline is on (not considered as whitespace) or
    // 2. whitespace (collasped, non-collapsed multi or single) or
    // 3. non-whitespace characters.
    // 4. empty, indicating content end.
    if (isEnd(m_position))
        return TextFragment(m_position, m_position, 0, TextFragment::ContentEnd);
    if (isLineBreak(m_position)) {
        TextFragment fragment(m_position, m_position + 1, 0, TextFragment::LineBreak);
        ++m_position;
        return fragment;
    }
    unsigned startPosition = m_position;
    unsigned endPosition = skipToNextPosition(PositionType::NonWhitespace, startPosition);
    ASSERT(startPosition <= endPosition);
    if (endPosition > startPosition) {
        bool multipleWhitespace = startPosition + 1 < endPosition;
        bool isCollapsed = multipleWhitespace && m_style.collapseWhitespace;
        bool isBreakable = !isCollapsed && multipleWhitespace;
        float width = isCollapsed ? m_style.spaceWidth : textWidth(startPosition, endPosition, xPosition);
        m_position = endPosition;
        return TextFragment(startPosition, endPosition, width, TextFragment::Whitespace, isCollapsed, isBreakable);
    }
    endPosition = skipToNextPosition(PositionType::Breakable, startPosition + 1);
    m_position = endPosition;
    return TextFragment(startPosition, endPosition, textWidth(startPosition, endPosition, xPosition), TextFragment::NonWhitespace, false, m_style.breakWordOnOverflow);
}

float TextFragmentIterator::textWidth(unsigned from, unsigned to, float xPosition) const
{
    const auto& fromSegment = m_flowContents.segmentForPosition(from);
    ASSERT(is<RenderText>(fromSegment.renderer));
    if ((m_style.font.isFixedPitch() && fromSegment.end >= to) || (from == fromSegment.start && to == fromSegment.end))
        return downcast<RenderText>(fromSegment.renderer).width(from - fromSegment.start, to - from, m_style.font, xPosition, nullptr, nullptr);

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
        segment = &m_flowContents.segmentForPosition(fragmentEnd);
    };

    return textWidth;
}

template <typename CharacterType>
static unsigned nextBreakablePosition(LazyLineBreakIterator& lineBreakIterator, const FlowContents::Segment& segment, unsigned startPosition)
{
    return nextBreakablePositionNonLoosely<CharacterType, NBSPBehavior::IgnoreNBSP>(lineBreakIterator, segment.text.characters<CharacterType>(), segment.end - segment.start, startPosition);
}

template <typename CharacterType>
static unsigned nextNonWhitespacePosition(const FlowContents::Segment& segment, unsigned startPosition, const TextFragmentIterator::Style& style)
{
    const auto* text = segment.text.characters<CharacterType>();
    unsigned position = startPosition;
    unsigned length = segment.end - segment.start;
    for (; position < length; ++position) {
        auto character = text[position];
        bool isWhitespace = character == ' ' || character == '\t' || (!style.preserveNewline && character == '\n');
        if (!isWhitespace)
            return position;
    }
    return position;
}

unsigned TextFragmentIterator::skipToNextPosition(PositionType positionType, unsigned startPosition) const
{
    if (isEnd(startPosition))
        return startPosition;

    unsigned currentPosition = startPosition;
    FlowContents::Iterator it(m_flowContents, m_flowContents.segmentIndexForPosition(currentPosition));
    for (auto end = m_flowContents.end(); it != end; ++it) {
        auto& segment = *it;
        unsigned currentPositonRelativeToSegment = currentPosition - segment.start;
        unsigned nextPositionRelativeToSegment = 0;
        if (positionType == NonWhitespace) {
            nextPositionRelativeToSegment = segment.text.is8Bit() ? nextNonWhitespacePosition<LChar>(segment, currentPositonRelativeToSegment, m_style) :
                nextNonWhitespacePosition<UChar>(segment, currentPositonRelativeToSegment, m_style);
        } else if (positionType == Breakable) {
            if (segment.text.impl() != m_lineBreakIterator.string().impl()) {
                UChar lastCharacter = segment.start > 0 ? characterAt(segment.start - 1) : 0;
                UChar secondToLastCharacter = segment.start > 1 ? characterAt(segment.start - 2) : 0;
                m_lineBreakIterator.setPriorContext(lastCharacter, secondToLastCharacter);
                m_lineBreakIterator.resetStringAndReleaseIterator(segment.text, m_style.locale, LineBreakIteratorModeUAX14);
            }
            nextPositionRelativeToSegment = segment.text.is8Bit() ? nextBreakablePosition<LChar>(m_lineBreakIterator, segment, currentPositonRelativeToSegment) :
                nextBreakablePosition<UChar>(m_lineBreakIterator, segment, currentPositonRelativeToSegment);
        } else
            ASSERT_NOT_REACHED();
        currentPosition = segment.start + nextPositionRelativeToSegment;
        if (currentPosition < segment.end)
            break;
    }
    return currentPosition;
}

template <typename CharacterType>
float TextFragmentIterator::runWidth(const String& text, unsigned from, unsigned to, float xPosition) const
{
    ASSERT(from <= to);
    if (from == to)
        return 0;
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
