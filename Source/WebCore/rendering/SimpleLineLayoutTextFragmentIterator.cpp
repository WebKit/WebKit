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
    , breakAnyWordOnOverflow(style.wordBreak() == BreakAllWordBreak && wrapLines)
    , breakFirstWordOnOverflow(breakAnyWordOnOverflow || (style.breakWords() && (wrapLines || preserveNewline)))
    , spaceWidth(font.width(TextRun(StringView(&space, 1))))
    , wordSpacing(font.wordSpacing())
    , tabWidth(collapseWhitespace ? 0 : style.tabSize())
    , locale(style.locale())
{
}

TextFragmentIterator::TextFragmentIterator(const RenderBlockFlow& flow)
    : m_flowContents(flow)
    , m_currentSegment(m_flowContents.begin())
    , m_lineBreakIterator(m_currentSegment->text, flow.style().locale())
    , m_style(flow.style())
{
}

TextFragmentIterator::TextFragment TextFragmentIterator::nextTextFragment(float xPosition)
{
    TextFragmentIterator::TextFragment nextFragment = findNextTextFragment(xPosition);
    m_atEndOfSegment = (m_currentSegment == m_flowContents.end()) || (m_position == m_currentSegment->end);
    return nextFragment;
}

TextFragmentIterator::TextFragment TextFragmentIterator::findNextTextFragment(float xPosition)
{
    // A fragment can either be
    // 1. line break when <br> is present or preserveNewline is on (not considered as whitespace) or
    // 2. whitespace (collasped, non-collapsed multi or single) or
    // 3. non-whitespace characters.
    // 4. content end.
    ASSERT(m_currentSegment != m_flowContents.end());
    unsigned startPosition = m_position;
    if (m_atEndOfSegment)
        ++m_currentSegment;

    if (m_currentSegment == m_flowContents.end())
        return TextFragment(startPosition, startPosition, 0, TextFragment::ContentEnd);
    if (isHardLineBreak(m_currentSegment))
        return TextFragment(startPosition, startPosition, 0, TextFragment::HardLineBreak);
    if (isSoftLineBreak(startPosition)) {
        unsigned endPosition = ++m_position;
        return TextFragment(startPosition, endPosition, 0, TextFragment::SoftLineBreak);
    }
    float width = 0;
    bool overlappingFragment = false;
    unsigned endPosition = skipToNextPosition(PositionType::NonWhitespace, startPosition, width, xPosition, overlappingFragment);
    unsigned segmentEndPosition = m_currentSegment->end;
    ASSERT(startPosition <= endPosition);
    if (startPosition < endPosition) {
        bool multipleWhitespace = startPosition + 1 < endPosition;
        bool isCollapsed = multipleWhitespace && m_style.collapseWhitespace;
        m_position = endPosition;
        return TextFragment(startPosition, endPosition, width, TextFragment::Whitespace, endPosition == segmentEndPosition, false, isCollapsed, m_style.collapseWhitespace);
    }
    endPosition = skipToNextPosition(PositionType::Breakable, startPosition, width, xPosition, overlappingFragment);
    m_position = endPosition;
    return TextFragment(startPosition, endPosition, width, TextFragment::NonWhitespace, endPosition == segmentEndPosition, overlappingFragment, false, false);
}

void TextFragmentIterator::revertToEndOfFragment(const TextFragment& fragment)
{
    ASSERT(m_position >= fragment.end());
    while (m_currentSegment->start > fragment.end())
        --m_currentSegment;
    // TODO: It reverts to the last fragment on the same position, but that's ok for now as we don't need to
    // differentiate multiple renderers on the same position.
    m_position = fragment.end();
    m_atEndOfSegment = false;
}

template <typename CharacterType>
unsigned TextFragmentIterator::nextBreakablePosition(const FlowContents::Segment& segment, unsigned startPosition)
{
    ASSERT(startPosition < segment.end);
    if (segment.text.impl() != m_lineBreakIterator.string().impl()) {
        const String& currentText = m_lineBreakIterator.string();
        unsigned textLength = currentText.length();
        UChar lastCharacter = textLength > 0 ? currentText[textLength - 1] : 0;
        UChar secondToLastCharacter = textLength > 1 ? currentText[textLength - 2] : 0;
        m_lineBreakIterator.setPriorContext(lastCharacter, secondToLastCharacter);
        m_lineBreakIterator.resetStringAndReleaseIterator(segment.text, m_style.locale, LineBreakIteratorModeUAX14);
    }
    const auto* characters = segment.text.characters<CharacterType>();
    unsigned segmentLength = segment.end - segment.start;
    unsigned segmentPosition = startPosition - segment.start;
    return segment.start + nextBreakablePositionNonLoosely<CharacterType, NBSPBehavior::IgnoreNBSP>(m_lineBreakIterator, characters, segmentLength, segmentPosition);
}

template <typename CharacterType>
unsigned TextFragmentIterator::nextNonWhitespacePosition(const FlowContents::Segment& segment, unsigned startPosition)
{
    ASSERT(startPosition < segment.end);
    const auto* text = segment.text.characters<CharacterType>();
    unsigned position = startPosition;
    for (; position < segment.end; ++position) {
        auto character = text[position - segment.start];
        bool isWhitespace = character == ' ' || character == '\t' || (!m_style.preserveNewline && character == '\n');
        if (!isWhitespace)
            return position;
    }
    return position;
}

float TextFragmentIterator::textWidth(unsigned from, unsigned to, float xPosition) const
{
    auto& segment = *m_currentSegment;
    ASSERT(segment.start <= from && from <= segment.end && segment.start <= to && to <= segment.end);
    ASSERT(is<RenderText>(segment.renderer));
    if (m_style.font.isFixedPitch() || (from == segment.start && to == segment.end))
        return downcast<RenderText>(segment.renderer).width(from - segment.start, to - from, m_style.font, xPosition, nullptr, nullptr);
    return segment.text.is8Bit() ? runWidth<LChar>(segment, from, to, xPosition) : runWidth<UChar>(segment, from, to, xPosition);
}

unsigned TextFragmentIterator::skipToNextPosition(PositionType positionType, unsigned startPosition, float& width, float xPosition, bool& overlappingFragment)
{
    overlappingFragment = false;
    unsigned currentPosition = startPosition;
    unsigned nextPosition = currentPosition;
    // Collapsed whitespace has constant width. Do not measure it.
    if (positionType == NonWhitespace)
        nextPosition = m_currentSegment->text.is8Bit() ? nextNonWhitespacePosition<LChar>(*m_currentSegment, currentPosition) : nextNonWhitespacePosition<UChar>(*m_currentSegment, currentPosition);
    else if (positionType == Breakable) {
        nextPosition = m_currentSegment->text.is8Bit() ? nextBreakablePosition<LChar>(*m_currentSegment, currentPosition) : nextBreakablePosition<UChar>(*m_currentSegment, currentPosition);
        // nextBreakablePosition returns the same position for certain characters such as hyphens. Call next again with modified position unless we are at the end of the segment.
        bool skipCurrentPosition = nextPosition == currentPosition;
        if (skipCurrentPosition) {
            // When we are skipping the last character in the segment, just move to the end of the segment and we'll check the next segment whether it is an overlapping fragment.
            ASSERT(currentPosition < m_currentSegment->end);
            if (currentPosition == m_currentSegment->end - 1)
                nextPosition = m_currentSegment->end;
            else
                nextPosition = m_currentSegment->text.is8Bit() ? nextBreakablePosition<LChar>(*m_currentSegment, currentPosition + 1) : nextBreakablePosition<UChar>(*m_currentSegment, currentPosition + 1);
        }
        // We need to know whether the word actually finishes at the end of this renderer or not.
        if (nextPosition == m_currentSegment->end) {
            const auto nextSegment = m_currentSegment + 1;
            if (nextSegment != m_flowContents.end() && !isHardLineBreak(nextSegment))
                overlappingFragment = nextPosition < (nextSegment->text.is8Bit() ? nextBreakablePosition<LChar>(*nextSegment, nextPosition) : nextBreakablePosition<UChar>(*nextSegment, nextPosition));
        }
    }
    width = 0;
    if (nextPosition == currentPosition)
        return currentPosition;
    // Both non-collapsed whitespace and non-whitespace runs need to be measured.
    bool measureText = positionType != NonWhitespace || !m_style.collapseWhitespace;
    if (measureText)
        width = this->textWidth(currentPosition, nextPosition, xPosition);
    else if (startPosition < nextPosition)
        width = m_style.spaceWidth + m_style.wordSpacing;
    return nextPosition;
}

template <typename CharacterType>
float TextFragmentIterator::runWidth(const FlowContents::Segment& segment, unsigned startPosition, unsigned endPosition, float xPosition) const
{
    ASSERT(startPosition <= endPosition);
    if (startPosition == endPosition)
        return 0;
    unsigned segmentFrom = startPosition - segment.start;
    unsigned segmentTo = endPosition - segment.start;
    bool measureWithEndSpace = m_style.collapseWhitespace && segmentTo < segment.text.length() && segment.text[segmentTo] == ' ';
    if (measureWithEndSpace)
        ++segmentTo;
    TextRun run(StringView(segment.text).substring(segmentFrom, segmentTo - segmentFrom));
    run.setXPos(xPosition);
    run.setTabSize(!!m_style.tabWidth, m_style.tabWidth);
    float width = m_style.font.width(run);
    if (measureWithEndSpace)
        width -= (m_style.spaceWidth + m_style.wordSpacing);
    return std::max<float>(0, width);
}

}
}
