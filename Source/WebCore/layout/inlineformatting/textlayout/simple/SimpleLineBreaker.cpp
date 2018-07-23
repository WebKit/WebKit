/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "SimpleLineBreaker.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "FontCascade.h"
#include "Hyphenation.h"
#include "InlineFormattingContext.h"
#include "LayoutContext.h"
#include "RenderStyle.h"
#include "TextContentProvider.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(SimpleLineBreaker);

struct TextRunSplitPair {
    TextRun left;
    TextRun right;
};

SimpleLineBreaker::TextRunList::TextRunList(const Vector<TextRun>& textRuns)
    : m_textRuns(textRuns)
{
}

SimpleLineBreaker::Line::Line(Vector<LayoutRun>& layoutRuns)
    : m_layoutRuns(layoutRuns)
    , m_firstRunIndex(m_layoutRuns.size())
{
}

static inline unsigned adjustedEndPosition(const TextRun& textRun)
{
    if (textRun.isCollapsed())
        return textRun.start() + 1;
    return textRun.end();
}

void SimpleLineBreaker::Line::setTextAlign(TextAlignMode textAlign)
{
    m_style.textAlign = textAlign;
    m_collectExpansionOpportunities = textAlign == TextAlignMode::Justify;
}

float SimpleLineBreaker::Line::adjustedLeftForTextAlign(TextAlignMode textAlign) const
{
    float remainingWidth = availableWidth();
    float left = m_left;
    switch (textAlign) {
    case TextAlignMode::Left:
    case TextAlignMode::WebKitLeft:
    case TextAlignMode::Start:
        return left;
    case TextAlignMode::Right:
    case TextAlignMode::WebKitRight:
    case TextAlignMode::End:
        return left + std::max<float>(remainingWidth, 0);
    case TextAlignMode::Center:
    case TextAlignMode::WebKitCenter:
        return left + std::max<float>(remainingWidth / 2, 0);
    case TextAlignMode::Justify:
        ASSERT_NOT_REACHED();
        break;
    }
    ASSERT_NOT_REACHED();
    return left;
}

void SimpleLineBreaker::Line::justifyRuns()
{
    auto widthToDistribute = availableWidth();
    if (widthToDistribute <= 0)
        return;

    unsigned expansionOpportunities = 0;
    for (auto& expansionEntry : m_expansionOpportunityList)
        expansionOpportunities += expansionEntry.count;

    if (!expansionOpportunities)
        return;

    auto expansion = widthToDistribute / expansionOpportunities;
    float accumulatedExpansion = 0;
    unsigned runIndex = m_firstRunIndex;
    for (auto& expansionEntry : m_expansionOpportunityList) {
        if (runIndex >= m_layoutRuns.size()) {
            ASSERT_NOT_REACHED();
            return;
        }
        auto& layoutRun = m_layoutRuns.at(runIndex++);
        auto expansionForRun = expansionEntry.count * expansion;

        layoutRun.setExpansion(expansionEntry.behavior, expansionForRun);
        layoutRun.setLeft(layoutRun.left() + accumulatedExpansion);
        layoutRun.setRight(layoutRun.right() + accumulatedExpansion + expansionForRun);
        accumulatedExpansion += expansionForRun;
    }
}

void SimpleLineBreaker::Line::adjustRunsForTextAlign(bool lastLine)
{
    // Fallback to TextAlignMode::Left (START) alignment for non-collapsable content or for the last line before a forced break/the end of the block.
    auto textAlign = m_style.textAlign;
    if (textAlign == TextAlignMode::Justify && (!m_style.collapseWhitespace || lastLine))
        textAlign = TextAlignMode::Left;

    if (textAlign == TextAlignMode::Justify) {
        justifyRuns();
        return;
    }

    auto adjustedLeft = adjustedLeftForTextAlign(textAlign);
    if (adjustedLeft == m_left)
        return;

    for (auto i = m_firstRunIndex; i < m_layoutRuns.size(); ++i) {
        auto& layoutRun = m_layoutRuns.at(i);
        layoutRun.setLeft(layoutRun.left() + adjustedLeft);
        layoutRun.setRight(layoutRun.right() + adjustedLeft);
    }
}

static bool expansionOpportunity(TextRun::Type current, TextRun::Type previous)
{
    return current == TextRun::Type::Whitespace || (current == TextRun::Type::NonWhitespace && previous == TextRun::Type::NonWhitespace);
}

static ExpansionBehavior expansionBehavior(bool isAtExpansionOpportunity)
{
    ExpansionBehavior expansionBehavior = AllowTrailingExpansion;
    expansionBehavior |= isAtExpansionOpportunity ? ForbidLeadingExpansion : AllowLeadingExpansion;
    return expansionBehavior;
}

void SimpleLineBreaker::Line::collectExpansionOpportunities(const TextRun& textRun, bool textRunCreatesNewLayoutRun)
{
    if (textRunCreatesNewLayoutRun) {
        // Create an entry for this new layout run.
        m_expansionOpportunityList.append({ });
    }

    if (!textRun.length())
        return;

    auto isAtExpansionOpportunity = expansionOpportunity(textRun.type(), m_lastTextRun ? m_lastTextRun->type() : TextRun::Type::Invalid);
    m_expansionOpportunityList.last().behavior = expansionBehavior(isAtExpansionOpportunity);
    if (isAtExpansionOpportunity)
        ++m_expansionOpportunityList.last().count;

    if (textRun.isNonWhitespace())
        m_lastNonWhitespaceExpansionOppportunity = m_expansionOpportunityList.last();
}

void SimpleLineBreaker::Line::closeLastRun()
{
    if (!m_layoutRuns.size())
        return;

    m_layoutRuns.last().setIsEndOfLine();

    // Forbid trailing expansion for the last run on line.
    if (!m_collectExpansionOpportunities || m_expansionOpportunityList.isEmpty())
        return;

    auto& lastExpansionEntry = m_expansionOpportunityList.last();
    auto expansionBehavior = lastExpansionEntry.behavior;
    // Remove allow and add forbid.
    expansionBehavior ^= AllowTrailingExpansion;
    expansionBehavior |= ForbidTrailingExpansion;
    lastExpansionEntry.behavior = expansionBehavior;
}

void SimpleLineBreaker::Line::append(const TextRun& textRun)
{
    auto start = textRun.start();
    auto end = adjustedEndPosition(textRun);
    auto previousLogicalRight = m_left + m_runsWidth;
    bool textRunCreatesNewLayoutRun = !m_lastTextRun || m_lastTextRun->isCollapsed();

    m_runsWidth += textRun.width();
    if (textRun.isNonWhitespace()) {
        m_trailingWhitespaceWidth = 0;
        m_lastNonWhitespaceTextRun = textRun;
    } else if (textRun.isWhitespace())
        m_trailingWhitespaceWidth += textRun.width();

    if (m_collectExpansionOpportunities)
        collectExpansionOpportunities(textRun, textRunCreatesNewLayoutRun);

    // New line needs new run.
    if (textRunCreatesNewLayoutRun)
        m_layoutRuns.append({ start, end, previousLogicalRight, previousLogicalRight + textRun.width(), textRun.hasHyphen() });
    else {
        auto& lastRun = m_layoutRuns.last();
        lastRun.setEnd(end);
        lastRun.setRight(lastRun.right() + textRun.width());
        if (textRun.hasHyphen())
            lastRun.setHasHyphen();
    }

    m_lastTextRun = textRun;
}

void SimpleLineBreaker::Line::collapseTrailingWhitespace()
{
    if (!m_lastTextRun || !m_lastTextRun->isWhitespace())
        return;

    if (!m_lastNonWhitespaceTextRun) {
        // This essentially becomes an empty line.
        reset();
        return;
    }

    m_runsWidth -= m_trailingWhitespaceWidth;
    m_lastTextRun = m_lastNonWhitespaceTextRun;

    while (m_trailingWhitespaceWidth) {
        auto& lastLayoutRun = m_layoutRuns.last();

        if (lastLayoutRun.end() <= m_lastNonWhitespaceTextRun->end())
            break;

        // Full remove.
        if (lastLayoutRun.start() >= m_lastNonWhitespaceTextRun->end()) {
            m_trailingWhitespaceWidth -= lastLayoutRun.width();
            m_layoutRuns.removeLast();
            continue;
        }
        // Partial remove.
        lastLayoutRun.setRight(lastLayoutRun.right() - m_trailingWhitespaceWidth);
        lastLayoutRun.setEnd(m_lastNonWhitespaceTextRun->end());
        m_trailingWhitespaceWidth = 0;
    }

    if (m_collectExpansionOpportunities) {
        ASSERT(m_lastNonWhitespaceExpansionOppportunity);
        ASSERT(!m_expansionOpportunityList.isEmpty());
        m_expansionOpportunityList.last() = *m_lastNonWhitespaceExpansionOppportunity;
    }
}

void SimpleLineBreaker::Line::reset()
{
    m_runsWidth = 0;
    m_firstRunIndex = m_layoutRuns.size();
    m_availableWidth = 0;
    m_trailingWhitespaceWidth  = 0;
    m_expansionOpportunityList.clear();
    m_lastNonWhitespaceExpansionOppportunity = { };
    m_lastTextRun = std::nullopt;
    m_lastNonWhitespaceTextRun = std::nullopt;
}

// FIXME: Use variable style based on the current text run.
SimpleLineBreaker::Style::Style(const RenderStyle& style)
    : font(style.fontCascade())
    , wrapLines(style.autoWrap())
    , breakAnyWordOnOverflow(style.wordBreak() == WordBreak::BreakAll && wrapLines)
    , breakFirstWordOnOverflow(breakAnyWordOnOverflow || (style.breakWords() && (wrapLines || style.preserveNewline())))
    , collapseWhitespace(style.collapseWhiteSpace())
    , preWrap(wrapLines && !collapseWhitespace)
    , preserveNewline(style.preserveNewline())
    , textAlign(style.textAlign())
    , shouldHyphenate(style.hyphens() == Hyphens::Auto && canHyphenate(style.locale()))
    , hyphenStringWidth(shouldHyphenate ? font.width(WebCore::TextRun(String(style.hyphenString()))) : 0)
    , hyphenLimitBefore(style.hyphenationLimitBefore() < 0 ? 2 : style.hyphenationLimitBefore())
    , hyphenLimitAfter(style.hyphenationLimitAfter() < 0 ? 2 : style.hyphenationLimitAfter())
    , locale(style.locale())
{
    if (style.hyphenationLimitLines() > -1)
        hyphenLimitLines = style.hyphenationLimitLines();
}

SimpleLineBreaker::SimpleLineBreaker(const Vector<TextRun>& textRuns, const TextContentProvider& contentProvider, LineConstraintList&& lineConstraintList, const RenderStyle& style)
    : m_contentProvider(contentProvider)
    , m_style(style)
    , m_textRunList(textRuns)
    , m_currentLine(m_layoutRuns)
    , m_lineConstraintList(WTFMove(lineConstraintList))
    , m_lineConstraintIterator(m_lineConstraintList)
{
    ASSERT(m_lineConstraintList.size());
    // Since the height of the content is undefined, the last vertical position in the constraint list should always be infinite.
    ASSERT(!m_lineConstraintList.last().verticalPosition);
}

Vector<LayoutRun> SimpleLineBreaker::runs()
{
    while (m_textRunList.current()) {
        handleLineStart();
        createRunsForLine();
        handleLineEnd();
    }

    return m_layoutRuns;
}

void SimpleLineBreaker::handleLineEnd()
{
    auto lineHasContent = m_currentLine.hasContent();
    if (lineHasContent) {
        ASSERT(m_layoutRuns.size());
        ++m_numberOfLines;
        m_currentLine.closeLastRun();

        auto lastLine = !m_textRunList.current();
        m_currentLine.adjustRunsForTextAlign(lastLine);
    }
    // Check if we need to disable hyphenation.
    if (m_style.hyphenLimitLines) {
        if (!lineHasContent || (m_layoutRuns.size() && !m_layoutRuns.last().hasHyphen()))
            m_numberOfPrecedingLinesWithHyphen = 0;
        else
            ++m_numberOfPrecedingLinesWithHyphen;
        m_hyphenationIsDisabled = m_numberOfPrecedingLinesWithHyphen >= *m_style.hyphenLimitLines;
    }

    m_previousLineHasNonForcedContent = lineHasContent && m_currentLine.availableWidth() >= 0;
    m_currentLine.reset();
}

void SimpleLineBreaker::handleLineStart()
{
    auto lineConstraint = this->lineConstraint(verticalPosition());
    m_currentLine.setLeft(lineConstraint.left);
    m_currentLine.setTextAlign(m_style.textAlign);
    m_currentLine.setCollapseWhitespace(m_style.collapseWhitespace);
    m_currentLine.setAvailableWidth(lineConstraint.right - lineConstraint.left);
}

static bool isTextAlignRight(TextAlignMode textAlign)
{
    return textAlign == TextAlignMode::Right || textAlign == TextAlignMode::WebKitRight;
}

void SimpleLineBreaker::createRunsForLine()
{
    collapseLeadingWhitespace();

    while (auto textRun = m_textRunList.current()) {

        if (!textRun->isLineBreak() && (!wrapContentOnOverflow() || m_currentLine.availableWidth() >= textRun->width())) {
            m_currentLine.append(*textRun);
            ++m_textRunList;
            continue;
        }

        // This run is either a linebreak or it does not fit the current line.
        if (textRun->isLineBreak()) {
            // Add the new line only if there's nothing on the line. (otherwise the extra new line character would show up at the end of the content.)
            if (textRun->isHardLineBreak() || !m_currentLine.hasContent()) {
                if (isTextAlignRight(m_style.textAlign))
                    m_currentLine.collapseTrailingWhitespace();
                m_currentLine.append(*textRun);
            }
            ++m_textRunList;
            break;
        }

        // Overflow wrapping behaviour:
        // 1. Whitesapce collapse on: whitespace is skipped. Jump to next line.
        // 2. Whitespace collapse off: whitespace is wrapped.
        // 3. First, non-whitespace run is either wrapped or kept on the line. (depends on overflow-wrap)
        // 4. Non-whitespace run when there's already another run on the line either gets wrapped (word-break: break-all) or gets pushed to the next line.

        // Whitespace run.
        if (textRun->isWhitespace()) {
            // Process collapsed whitespace again for the next line.
            if (textRun->isCollapsed())
                break;
            // Try to split the whitespace; left part stays on this line, right is pushed to next line.
            if (!splitTextRun(*textRun))
                ++m_textRunList;
            break;
        }

        // Non-whitespace run. (!style.wrapLines: bug138102(preserve existing behavior)
        if (((!m_currentLine.hasContent() && m_style.breakFirstWordOnOverflow) || m_style.breakAnyWordOnOverflow) || !m_style.wrapLines) {
            // Try to split the run; left side stays on this line, right side is pushed to next line.
            if (!splitTextRun(*textRun))
                ++m_textRunList;
            break;
        }

        ASSERT(textRun->isNonWhitespace());
        // Find out if this non-whitespace fragment has a hyphen where we can break.
        if (m_style.shouldHyphenate && !m_hyphenationIsDisabled) {
            if (!splitTextRun(*textRun)) {
                ++m_textRunList;
                break;
            }
        }

        // Non-breakable non-whitespace first run. Add it to the current line. -it overflows though.
        if (!m_currentLine.hasContent()) {
            handleOverflownRun();
            break;
        }
        // Non-breakable non-whitespace run when there's already content on the line. Process it on the next line.
        break;
    }

    collapseTrailingWhitespace();
}

void SimpleLineBreaker::handleOverflownRun()
{
    auto textRun = m_textRunList.current();
    ASSERT(textRun);

    m_currentLine.append(*textRun);
    ++m_textRunList;

    // If the forced run is followed by a line break, we need to process it on this line, otherwise the next line would start with a line break (double line break).
    // "foobarlongtext<br>foobar" should produce
    // foobarlongtext
    // foobar
    // and not
    // foobarlongtext
    //
    // foobar

    // First check for collapsable whitespace.
    textRun = m_textRunList.current();
    if (!textRun)
        return;

    if (textRun->isWhitespace() && m_style.collapseWhitespace) {
        ++m_textRunList;
        textRun = m_textRunList.current();
        if (!textRun)
            return;
    }

    if (textRun->isHardLineBreak()) {
        // <br> always produces a run. (required by testing output)
        m_currentLine.append(*textRun);
        ++m_textRunList;
        return;
    }

    if (textRun->isSoftLineBreak() && !m_style.preserveNewline)
        ++m_textRunList;
}

void SimpleLineBreaker::collapseLeadingWhitespace()
{
    auto textRun = m_textRunList.current();
    if (!textRun || !textRun->isWhitespace())
        return;

    // Special overflow pre-wrap whitespace/newline handling for overflow whitespace: skip the leading whitespace/newline (even when style says not-collapsible)
    // if we managed to put some content on the previous line.
    if (m_textRunList.isCurrentOverridden()) {
        auto collapseWhitespace = m_style.collapseWhitespace || m_style.preWrap;
        if (collapseWhitespace && m_previousLineHasNonForcedContent) {
            ++m_textRunList;
            // If collapsing the whitespace puts us on a newline, skip the soft newline too as we already wrapped the line.
            textRun = m_textRunList.current();
            // <br> always produces a run. (required by testing output).
            if (!textRun || textRun->isHardLineBreak())
                return;
            auto collapseSoftLineBreak = textRun->isSoftLineBreak() && (!m_style.preserveNewline || m_style.preWrap);
            if (collapseSoftLineBreak) {
                ++m_textRunList;
                textRun = m_textRunList.current();
            }
        }
    }

    // Collapse leading whitespace if the style says so.
    if (!m_style.collapseWhitespace)
        return;

    if (!textRun || !textRun->isWhitespace())
        return;

    ++m_textRunList;
}

void SimpleLineBreaker::collapseTrailingWhitespace()
{
    if (!m_currentLine.hasTrailingWhitespace())
        return;

    // Remove collapsed whitespace, or non-collapsed pre-wrap whitespace, unless it's the only content on the line -so removing the whitesapce would produce an empty line.
    bool collapseWhitespace = m_style.collapseWhitespace || m_style.preWrap;
    if (!collapseWhitespace)
        return;

    // pre-wrap?
    if (m_currentLine.isWhitespaceOnly() && m_style.preWrap)
        return;

    m_currentLine.collapseTrailingWhitespace();
}

std::optional<ContentPosition> SimpleLineBreaker::hyphenPositionBefore(const TextRun& textRun, ContentPosition before) const
{
    // Enough characters before the split position?
    if (before <= textRun.start() + m_style.hyphenLimitBefore)
        return { };

    // Adjust before to accommodate the limit-after value (this is the last potential hyphen location).
    before = std::min(before, textRun.end() - m_style.hyphenLimitAfter + 1);

    auto hyphenLocation = m_contentProvider.hyphenPositionBefore(textRun.start(), textRun.end(), before);
    if (!hyphenLocation)
        return { };

    ASSERT(hyphenLocation >= textRun.start() && hyphenLocation <= textRun.end());
    // Check if there are enough characters before and after the hyphen.
    if (*hyphenLocation < textRun.start() + m_style.hyphenLimitBefore || m_style.hyphenLimitAfter > (textRun.end() - *hyphenLocation))
        return { };

    return hyphenLocation;
}

std::optional<ContentPosition> SimpleLineBreaker::adjustSplitPositionWithHyphenation(const TextRun& textRun, ContentPosition splitPosition, float leftSideWidth) const
{
    ASSERT(textRun.isNonWhitespace());

    // Use hyphen?
    if (!m_style.shouldHyphenate || m_hyphenationIsDisabled)
        return { };

    // Check if there are enough characters in the run.
    auto runLength = textRun.length();
    if (m_style.hyphenLimitBefore >= runLength || m_style.hyphenLimitAfter >= runLength || m_style.hyphenLimitBefore + m_style.hyphenLimitAfter > runLength)
        return { };

    // FIXME: This is a workaround for webkit.org/b/169613. See maxPrefixWidth computation in tryHyphenating().
    // It does not work properly with non-collapsed leading tabs when font is enlarged.
    auto adjustedAvailableWidth = m_currentLine.availableWidth() - m_style.hyphenStringWidth;
    if (m_currentLine.hasContent())
        adjustedAvailableWidth += m_style.font.spaceWidth();

    if (!enoughWidthForHyphenation(adjustedAvailableWidth, m_style.font.pixelSize()))
        return { };

    // Find the split position where hyphen surely fits (we might be able to fit the hyphen at the split position).
    auto left = textRun.start();
    auto right = splitPosition;
    while (leftSideWidth + m_style.hyphenStringWidth > m_currentLine.availableWidth()) {
        if (--right <= left)
            return { }; // No space for hyphen.
        // FIXME: for correctness (kerning) we should instead measure the left side.
        leftSideWidth -= m_contentProvider.width(right, right + 1, 0);
    }

    // Find out if there's an actual hyphen opportinity at this position (or before).
    return hyphenPositionBefore(textRun, right + 1);
}

bool SimpleLineBreaker::splitTextRun(const TextRun& textRun)
{
    // Single character handling.
    if (textRun.start() + 1 == textRun.end()) {
        // Keep at least one character on empty lines.
        if (!m_currentLine.hasContent()) {
            m_currentLine.append(textRun);
            return false;
        }
        m_textRunList.overrideCurrent(textRun);
        return true;
    }

    auto splitPair = split(textRun, m_currentLine.availableWidth());
    m_currentLine.append(splitPair.left);
    m_textRunList.overrideCurrent(splitPair.right);
    return true;
}

TextRunSplitPair SimpleLineBreaker::split(const TextRun& textRun, float leftSideMaximumWidth) const
{
    // Preserve the left width for the final split position so that we don't need to remeasure the left side again.
    float leftSideWidth = 0;
    auto left = textRun.start();

    // Pathological case of (extremely)long string and narrow lines.
    // Adjust the range so that we can pick a reasonable midpoint.
    auto averageCharacterWidth = textRun.width() / textRun.length();
    auto right = std::min<unsigned>(left + (2 * leftSideMaximumWidth / averageCharacterWidth), textRun.end() - 1);
    while (left < right) {
        auto middle = (left + right) / 2;
        auto width = m_contentProvider.width(textRun.start(), middle + 1, 0);
        if (width < leftSideMaximumWidth) {
            left = middle + 1;
            leftSideWidth = width;
        } else if (width > leftSideMaximumWidth)
            right = middle;
        else {
            right = middle + 1;
            leftSideWidth = width;
            break;
        }
    }

    // Keep at least one character on empty lines.
    bool splitHasHypen = false;
    left = textRun.start();
    if (left >= right && !m_currentLine.hasContent()) {
        right = left + 1;
        leftSideWidth = m_contentProvider.width(left, right, 0);
    } else if (textRun.isNonWhitespace()) {
        if (auto hyphenPosition = adjustSplitPositionWithHyphenation(textRun, right, leftSideWidth)) {
            splitHasHypen = true;
            right = *hyphenPosition;
            leftSideWidth = m_contentProvider.width(left, right, 0) + m_style.hyphenStringWidth;
        }
    }

    auto rightSideWidth = m_contentProvider.width(right, textRun.end(), 0);
    if (textRun.isNonWhitespace())
        return { splitHasHypen ? TextRun::createNonWhitespaceRunWithHyphen(left, right, leftSideWidth) : TextRun::createNonWhitespaceRun(left, right, leftSideWidth),
            TextRun::createNonWhitespaceRun(right, textRun.end(), rightSideWidth) };

    // We never split collapsed whitespace.
    ASSERT(textRun.isWhitespace());
    ASSERT(!textRun.isCollapsed());
    return { TextRun::createWhitespaceRun(left, right, leftSideWidth, false), TextRun::createWhitespaceRun(right, textRun.end(), rightSideWidth, false) };
}

SimpleLineBreaker::LineConstraint SimpleLineBreaker::lineConstraint(float verticalPosition)
{
    auto* lineConstraint = m_lineConstraintIterator.current();
    if (!lineConstraint) {
        ASSERT_NOT_REACHED();
        return { };
    }

    while (lineConstraint->verticalPosition && verticalPosition > *lineConstraint->verticalPosition) {
        lineConstraint = (++m_lineConstraintIterator).current();
        if (!lineConstraint) {
            // The vertical position of the last entry in the constraint list is supposed to be infinite.
            ASSERT_NOT_REACHED();
            return { };
        }
    }

    return *lineConstraint;
}

}
}
#endif
