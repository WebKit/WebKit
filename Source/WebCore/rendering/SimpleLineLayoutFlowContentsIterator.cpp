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
#include "SimpleLineLayoutFlowContentsIterator.h"

#include "RenderBlockFlow.h"
#include "RenderChildIterator.h"
#include "RenderText.h"
#include "SimpleLineLayoutFlowContents.h"

namespace WebCore {
namespace SimpleLineLayout {

FlowContentsIterator::Style::Style(const RenderStyle& style)
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

FlowContentsIterator::FlowContentsIterator(const RenderBlockFlow& flow)
    : m_flowContents(flow)
    , m_lineBreakIterator((*m_flowContents.begin()).text, flow.style().locale())
    , m_style(flow.style())
{
}

FlowContentsIterator::TextFragment FlowContentsIterator::nextTextFragment(float xPosition)
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
    unsigned spaceCount = 0;
    unsigned startPosition = m_position;
    unsigned endPosition = findNextNonWhitespacePosition(startPosition, spaceCount);
    ASSERT(startPosition <= endPosition);
    if (endPosition > startPosition) {
        bool multipleWhitespace = startPosition + 1 < endPosition;
        bool isCollapsed = multipleWhitespace && m_style.collapseWhitespace;
        bool isBreakable = !isCollapsed && multipleWhitespace;
        float width = 0;
        if (isCollapsed)
            width = m_style.spaceWidth;
        else {
            unsigned length = endPosition - startPosition;
            width = length == spaceCount ? length * m_style.spaceWidth : textWidth(startPosition, endPosition, xPosition);
        }
        m_position = endPosition;
        return TextFragment(startPosition, endPosition, width, TextFragment::Whitespace, isCollapsed, isBreakable);
    }
    endPosition = findNextBreakablePosition(startPosition + 1);
    m_position = endPosition;
    return TextFragment(startPosition, endPosition, textWidth(startPosition, endPosition, xPosition), TextFragment::NonWhitespace, false, m_style.breakWordOnOverflow);
}

float FlowContentsIterator::textWidth(unsigned from, unsigned to, float xPosition) const
{
    const auto& fromSegment = m_flowContents.segmentForPosition(from);
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
        segment = &m_flowContents.segmentForPosition(fragmentEnd);
    };

    return textWidth;
}

template <typename CharacterType>
static unsigned nextBreakablePosition(LazyLineBreakIterator& lineBreakIterator, const FlowContents::Segment& segment, unsigned position)
{
    const auto* characters = segment.text.characters<CharacterType>();
    unsigned segmentLength = segment.end - segment.start;
    unsigned segmentPosition = position - segment.start;
    return nextBreakablePositionNonLoosely<CharacterType, NBSPBehavior::IgnoreNBSP>(lineBreakIterator, characters, segmentLength, segmentPosition);
}

unsigned FlowContentsIterator::findNextBreakablePosition(unsigned position) const
{
    while (!isEnd(position)) {
        auto& segment = m_flowContents.segmentForPosition(position);
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
static bool findNextNonWhitespace(const FlowContents::Segment& segment, const FlowContentsIterator::Style& style, unsigned& position, unsigned& spaceCount)
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

unsigned FlowContentsIterator::findNextNonWhitespacePosition(unsigned position, unsigned& spaceCount) const
{
    FlowContents::Iterator it(m_flowContents, m_flowContents.segmentIndexForPosition(position));
    for (auto end = m_flowContents.end(); it != end; ++it) {
        bool foundNonWhitespace = (*it).text.is8Bit() ? findNextNonWhitespace<LChar>(*it, m_style, position, spaceCount) : findNextNonWhitespace<UChar>(*it, m_style, position, spaceCount);
        if (foundNonWhitespace)
            break;
    }
    return position;
}

template <typename CharacterType>
float FlowContentsIterator::runWidth(const String& text, unsigned from, unsigned to, float xPosition) const
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
