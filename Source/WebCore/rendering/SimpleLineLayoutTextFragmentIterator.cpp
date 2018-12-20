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

#include "FontCascade.h"
#include "Hyphenation.h"
#include "RenderBlockFlow.h"
#include "RenderChildIterator.h"
#include "SimpleLineLayoutFlowContents.h"

namespace WebCore {
namespace SimpleLineLayout {

TextFragmentIterator::Style::Style(const RenderStyle& style)
    : font(style.fontCascade())
    , textAlign(style.textAlign())
    , hasKerningOrLigatures(font.enableKerning() || font.requiresShaping())
    , collapseWhitespace(style.collapseWhiteSpace())
    , preserveNewline(style.preserveNewline())
    , wrapLines(style.autoWrap())
    , breakAnyWordOnOverflow(style.wordBreak() == WordBreak::BreakAll && wrapLines)
    , breakFirstWordOnOverflow(breakAnyWordOnOverflow || (style.breakWords() && (wrapLines || preserveNewline)))
    , breakNBSP(wrapLines && style.nbspMode() == NBSPMode::Space)
    , keepAllWordsForCJK(style.wordBreak() == WordBreak::KeepAll)
    , wordSpacing(font.wordSpacing())
    , tabWidth(collapseWhitespace ? 0 : style.tabSize())
    , shouldHyphenate(style.hyphens() == Hyphens::Auto && canHyphenate(style.locale()))
    , hyphenStringWidth(shouldHyphenate ? font.width(TextRun(String(style.hyphenString()))) : 0)
    , hyphenLimitBefore(style.hyphenationLimitBefore() < 0 ? 2 : style.hyphenationLimitBefore())
    , hyphenLimitAfter(style.hyphenationLimitAfter() < 0 ? 2 : style.hyphenationLimitAfter())
    , locale(style.locale())
{
    if (style.hyphenationLimitLines() > -1)
        hyphenLimitLines = style.hyphenationLimitLines();
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

static inline unsigned nextBreakablePositionInSegment(LazyLineBreakIterator& lineBreakIterator, unsigned startPosition, bool breakNBSP, bool keepAllWordsForCJK)
{
    if (keepAllWordsForCJK) {
        if (breakNBSP)
            return nextBreakablePositionKeepingAllWords(lineBreakIterator, startPosition);
        return nextBreakablePositionKeepingAllWordsIgnoringNBSP(lineBreakIterator, startPosition);
    }

    if (lineBreakIterator.mode() == LineBreakIteratorMode::Default) {
        if (breakNBSP)
            return WebCore::nextBreakablePosition(lineBreakIterator, startPosition);
        return nextBreakablePositionIgnoringNBSP(lineBreakIterator, startPosition);
    }

    if (breakNBSP)
        return nextBreakablePositionWithoutShortcut(lineBreakIterator, startPosition);
    return nextBreakablePositionIgnoringNBSPWithoutShortcut(lineBreakIterator, startPosition);
}

unsigned TextFragmentIterator::nextBreakablePosition(const FlowContents::Segment& segment, unsigned startPosition)
{
    ASSERT(startPosition < segment.end);
    StringView currentText = m_lineBreakIterator.stringView();
    StringView segmentText = StringView(segment.text);
    if (segmentText != currentText) {
        unsigned textLength = currentText.length();
        UChar lastCharacter = textLength > 0 ? currentText[textLength - 1] : 0;
        UChar secondToLastCharacter = textLength > 1 ? currentText[textLength - 2] : 0;
        m_lineBreakIterator.setPriorContext(lastCharacter, secondToLastCharacter);
        m_lineBreakIterator.resetStringAndReleaseIterator(segment.text, m_style.locale, LineBreakIteratorMode::Default);
    }
    return segment.toRenderPosition(nextBreakablePositionInSegment(m_lineBreakIterator, segment.toSegmentPosition(startPosition), m_style.breakNBSP, m_style.keepAllWordsForCJK));
}

unsigned TextFragmentIterator::nextNonWhitespacePosition(const FlowContents::Segment& segment, unsigned startPosition)
{
    ASSERT(startPosition < segment.end);
    unsigned position = startPosition;
    for (; position < segment.end; ++position) {
        auto character = segment.text[segment.toSegmentPosition(position)];
        bool isWhitespace = character == ' ' || character == '\t' || (!m_style.preserveNewline && character == '\n');
        if (!isWhitespace)
            return position;
    }
    return position;
}

Optional<unsigned> TextFragmentIterator::lastHyphenPosition(const TextFragmentIterator::TextFragment& run, unsigned before) const
{
    ASSERT(run.start() < before);
    auto& segment = *m_currentSegment;
    ASSERT(segment.start <= before && before <= segment.end);
    ASSERT(is<RenderText>(segment.renderer));
    if (!m_style.shouldHyphenate || run.type() != TextFragment::NonWhitespace)
        return WTF::nullopt;
    // Check if there are enough characters in the run.
    unsigned runLength = run.end() - run.start();
    if (m_style.hyphenLimitBefore >= runLength || m_style.hyphenLimitAfter >= runLength || m_style.hyphenLimitBefore + m_style.hyphenLimitAfter > runLength)
        return WTF::nullopt;
    auto runStart = segment.toSegmentPosition(run.start());
    auto beforeIndex = segment.toSegmentPosition(before) - runStart;
    if (beforeIndex <= m_style.hyphenLimitBefore)
        return WTF::nullopt;
    // Adjust before index to accommodate the limit-after value (this is the last potential hyphen location).
    beforeIndex = std::min(beforeIndex, runLength - m_style.hyphenLimitAfter + 1);
    auto substringForHyphenation = StringView(segment.text).substring(runStart, run.end() - run.start());
    auto hyphenLocation = lastHyphenLocation(substringForHyphenation, beforeIndex, m_style.locale);
    // Check if there are enough characters before and after the hyphen.
    if (hyphenLocation && hyphenLocation >= m_style.hyphenLimitBefore && m_style.hyphenLimitAfter <= (runLength - hyphenLocation))
        return segment.toRenderPosition(hyphenLocation + runStart);
    return WTF::nullopt;
}

unsigned TextFragmentIterator::skipToNextPosition(PositionType positionType, unsigned startPosition, float& width, float xPosition, bool& overlappingFragment)
{
    overlappingFragment = false;
    unsigned currentPosition = startPosition;
    unsigned nextPosition = currentPosition;
    // Collapsed whitespace has constant width. Do not measure it.
    if (positionType == NonWhitespace)
        nextPosition = nextNonWhitespacePosition(*m_currentSegment, currentPosition);
    else if (positionType == Breakable) {
        nextPosition = nextBreakablePosition(*m_currentSegment, currentPosition);
        // nextBreakablePosition returns the same position for certain characters such as hyphens. Call next again with modified position unless we are at the end of the segment.
        bool skipCurrentPosition = nextPosition == currentPosition;
        if (skipCurrentPosition) {
            // When we are skipping the last character in the segment, just move to the end of the segment and we'll check the next segment whether it is an overlapping fragment.
            ASSERT(currentPosition < m_currentSegment->end);
            if (currentPosition == m_currentSegment->end - 1)
                nextPosition = m_currentSegment->end;
            else
                nextPosition = nextBreakablePosition(*m_currentSegment, currentPosition + 1);
        }
        // We need to know whether the word actually finishes at the end of this renderer or not.
        if (nextPosition == m_currentSegment->end) {
            const auto nextSegment = m_currentSegment + 1;
            if (nextSegment != m_flowContents.end() && !isHardLineBreak(nextSegment))
                overlappingFragment = nextPosition < nextBreakablePosition(*nextSegment, nextPosition);
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
        width = m_style.font.spaceWidth() + m_style.wordSpacing;
    return nextPosition;
}

float TextFragmentIterator::textWidth(unsigned from, unsigned to, float xPosition) const
{
    auto& segment = *m_currentSegment;
    ASSERT(segment.start <= from && from <= segment.end && segment.start <= to && to <= segment.end);
    ASSERT(is<RenderText>(segment.renderer));
    if (!m_style.font.size() || from == to)
        return 0;

    unsigned segmentFrom = segment.toSegmentPosition(from);
    unsigned segmentTo = segment.toSegmentPosition(to);
    if (m_style.font.isFixedPitch())
        return downcast<RenderText>(segment.renderer).width(segmentFrom, segmentTo - segmentFrom, m_style.font, xPosition, nullptr, nullptr);

    bool measureWithEndSpace = m_style.hasKerningOrLigatures && m_style.collapseWhitespace
        && segmentTo < segment.text.length() && segment.text[segmentTo] == ' ';
    if (measureWithEndSpace)
        ++segmentTo;
    float width = 0;
    if (segment.canUseSimplifiedTextMeasuring)
        width = m_style.font.widthForSimpleText(StringView(segment.text).substring(segmentFrom, segmentTo - segmentFrom));
    else {
        TextRun run(StringView(segment.text).substring(segmentFrom, segmentTo - segmentFrom), xPosition);
        if (m_style.tabWidth)
            run.setTabSize(true, m_style.tabWidth);
        width = m_style.font.width(run);
    }
    if (measureWithEndSpace)
        width -= (m_style.font.spaceWidth() + m_style.wordSpacing);
    return std::max<float>(0, width);
}

}
}
