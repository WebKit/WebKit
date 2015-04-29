/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "SimpleLineLayout.h"

#include "FontCache.h"
#include "Frame.h"
#include "GraphicsContext.h"
#include "HTMLTextFormControlElement.h"
#include "HitTestLocation.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "InlineTextBox.h"
#include "LineWidth.h"
#include "PaintInfo.h"
#include "RenderBlockFlow.h"
#include "RenderChildIterator.h"
#include "RenderLineBreak.h"
#include "RenderStyle.h"
#include "RenderText.h"
#include "RenderTextControl.h"
#include "RenderView.h"
#include "Settings.h"
#include "SimpleLineLayoutFlowContents.h"
#include "SimpleLineLayoutFunctions.h"
#include "SimpleLineLayoutTextFragmentIterator.h"
#include "Text.h"
#include "TextPaintStyle.h"

namespace WebCore {
namespace SimpleLineLayout {

template <typename CharacterType>
static bool canUseForText(const CharacterType* text, unsigned length, const Font& font)
{
    // FIXME: <textarea maxlength=0> generates empty text node.
    if (!length)
        return false;
    for (unsigned i = 0; i < length; ++i) {
        UChar character = text[i];
        if (character == ' ')
            continue;

        // These would be easy to support.
        if (character == noBreakSpace)
            return false;
        if (character == softHyphen)
            return false;

        UCharDirection direction = u_charDirection(character);
        if (direction == U_RIGHT_TO_LEFT || direction == U_RIGHT_TO_LEFT_ARABIC
            || direction == U_RIGHT_TO_LEFT_EMBEDDING || direction == U_RIGHT_TO_LEFT_OVERRIDE
            || direction == U_LEFT_TO_RIGHT_EMBEDDING || direction == U_LEFT_TO_RIGHT_OVERRIDE
            || direction == U_POP_DIRECTIONAL_FORMAT || direction == U_BOUNDARY_NEUTRAL)
            return false;

        if (!font.glyphForCharacter(character))
            return false;
    }
    return true;
}

static bool canUseForText(const RenderText& textRenderer, const Font& font)
{
    if (textRenderer.is8Bit())
        return canUseForText(textRenderer.characters8(), textRenderer.textLength(), font);
    return canUseForText(textRenderer.characters16(), textRenderer.textLength(), font);
}

bool canUseFor(const RenderBlockFlow& flow)
{
    if (!flow.frame().settings().simpleLineLayoutEnabled())
        return false;
    if (!flow.firstChild())
        return false;
    // This currently covers <blockflow>#text</blockflow>, <blockflow>#text<br></blockflow> and mutiple (sibling) RenderText cases.
    // The <blockflow><inline>#text</inline></blockflow> case is also popular and should be relatively easy to cover.
    for (const auto& renderer : childrenOfType<RenderObject>(flow)) {
        if (is<RenderText>(renderer))
            continue;
        if (is<RenderLineBreak>(renderer) && !downcast<RenderLineBreak>(renderer).isWBR() && renderer.style().clear() == CNONE)
            continue;
        return false;
    }
    if (!flow.isHorizontalWritingMode())
        return false;
    if (flow.flowThreadState() != RenderObject::NotInsideFlowThread)
        return false;
    // Printing does pagination without a flow thread.
    if (flow.document().paginated())
        return false;
    if (flow.hasOutline())
        return false;
    if (flow.isRubyText() || flow.isRubyBase())
        return false;
    if (flow.parent()->isDeprecatedFlexibleBox())
        return false;
    // FIXME: Implementation of wrap=hard looks into lineboxes.
    if (flow.parent()->isTextArea() && flow.parent()->element()->fastHasAttribute(HTMLNames::wrapAttr))
        return false;
    // FIXME: Placeholders do something strange.
    if (is<RenderTextControl>(*flow.parent()) && downcast<RenderTextControl>(*flow.parent()).textFormControlElement().placeholderElement())
        return false;
    const RenderStyle& style = flow.style();
    if (style.textDecorationsInEffect() != TextDecorationNone)
        return false;
    if (style.textAlign() == JUSTIFY)
        return false;
    // Non-visible overflow should be pretty easy to support.
    if (style.overflowX() != OVISIBLE || style.overflowY() != OVISIBLE)
        return false;
    if (!style.textIndent().isZero())
        return false;
    if (!style.wordSpacing().isZero() || style.letterSpacing())
        return false;
    if (!style.isLeftToRightDirection())
        return false;
    if (style.lineBoxContain() != RenderStyle::initialLineBoxContain())
        return false;
    if (style.writingMode() != TopToBottomWritingMode)
        return false;
    if (style.lineBreak() != LineBreakAuto)
        return false;
    if (style.wordBreak() != NormalWordBreak)
        return false;
    if (style.unicodeBidi() != UBNormal || style.rtlOrdering() != LogicalOrder)
        return false;
    if (style.lineAlign() != LineAlignNone || style.lineSnap() != LineSnapNone)
        return false;
    if (style.hyphens() == HyphensAuto)
        return false;
    if (style.textEmphasisFill() != TextEmphasisFillFilled || style.textEmphasisMark() != TextEmphasisMarkNone)
        return false;
    if (style.textShadow())
        return false;
    if (style.textOverflow() || (flow.isAnonymousBlock() && flow.parent()->style().textOverflow()))
        return false;
    if (style.hasPseudoStyle(FIRST_LINE) || style.hasPseudoStyle(FIRST_LETTER))
        return false;
    else if (flow.isAnonymous() && flow.firstLineBlock())
        return false;
    if (style.hasTextCombine())
        return false;
    if (style.backgroundClip() == TextFillBox)
        return false;
    if (style.borderFit() == BorderFitLines)
        return false;
    if (style.lineBreak() != LineBreakAuto)
        return false;
#if ENABLE(CSS_TRAILING_WORD)
    if (style.trailingWord() != TrailingWord::Auto)
        return false;
#endif

    // We can't use the code path if any lines would need to be shifted below floats. This is because we don't keep per-line y coordinates.
    if (flow.containsFloats()) {
        float minimumWidthNeeded = std::numeric_limits<float>::max();
        for (const auto& textRenderer : childrenOfType<RenderText>(flow)) {
            minimumWidthNeeded = std::min(minimumWidthNeeded, textRenderer.minLogicalWidth());

            for (auto& floatingObject : *flow.floatingObjectSet()) {
                ASSERT(floatingObject);
#if ENABLE(CSS_SHAPES)
                // if a float has a shape, we cannot tell if content will need to be shifted until after we lay it out,
                // since the amount of space is not uniform for the height of the float.
                if (floatingObject->renderer().shapeOutsideInfo())
                    return false;
#endif
                float availableWidth = flow.availableLogicalWidthForLine(floatingObject->y(), false);
                if (availableWidth < minimumWidthNeeded)
                    return false;
            }
        }
    }
    if (style.fontCascade().primaryFont().isSVGFont())
        return false;
    // We assume that all lines have metrics based purely on the primary font.
    auto& primaryFont = style.fontCascade().primaryFont();
    if (primaryFont.isLoading())
        return false;
    for (const auto& textRenderer : childrenOfType<RenderText>(flow)) {
        if (textRenderer.isCombineText() || textRenderer.isCounter() || textRenderer.isQuote() || textRenderer.isTextFragment()
            || textRenderer.isSVGInlineText())
            return false;
        if (style.fontCascade().codePath(TextRun(textRenderer.text())) != FontCascade::Simple)
            return false;
        if (!canUseForText(textRenderer, primaryFont))
            return false;
    }
    return true;
}

static float computeLineLeft(ETextAlign textAlign, float availableWidth, float committedWidth, float logicalLeftOffset)
{
    float remainingWidth = availableWidth - committedWidth;
    float left = logicalLeftOffset;
    switch (textAlign) {
    case LEFT:
    case WEBKIT_LEFT:
    case TASTART:
        return left;
    case RIGHT:
    case WEBKIT_RIGHT:
    case TAEND:
        return left + std::max<float>(remainingWidth, 0);
    case CENTER:
    case WEBKIT_CENTER:
        return left + std::max<float>(remainingWidth / 2, 0);
    case JUSTIFY:
        ASSERT_NOT_REACHED();
        break;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

static void revertRuns(Layout::RunVector& runs, unsigned length, float width)
{
    while (length) {
        ASSERT(runs.size());
        Run& lastRun = runs.last();
        unsigned lastRunLength = lastRun.end - lastRun.start;
        if (lastRunLength > length) {
            lastRun.logicalRight -= width;
            lastRun.end -= length;
            break;
        }
        length -= lastRunLength;
        width -= (lastRun.logicalRight - lastRun.logicalLeft);
        runs.removeLast();
    }
}

class LineState {
public:
    void setAvailableWidth(float width) { m_availableWidth = width; }
    void setCollapedWhitespaceWidth(float width) { m_collapsedWhitespaceWidth = width; }
    void setLogicalLeftOffset(float offset) { m_logicalLeftOffset = offset; }
    void setOverflowedFragment(const TextFragmentIterator::TextFragment& fragment) { m_overflowedFragment = fragment; }

    float availableWidth() const { return m_availableWidth; }
    float logicalLeftOffset() const { return m_logicalLeftOffset; }
    const TextFragmentIterator::TextFragment& overflowedFragment() const { return m_overflowedFragment; }
    bool hasTrailingWhitespace() const { return m_trailingWhitespaceLength; }
    TextFragmentIterator::TextFragment lastFragment() const { return m_fragments.last(); }
    bool isWhitespaceOnly() const { return m_trailingWhitespaceWidth && m_runsWidth == m_trailingWhitespaceWidth; }
    bool fits(float extra) const { return m_availableWidth >= m_runsWidth + extra; }
    bool firstCharacterFits() const { return m_firstCharacterFits; }
    float width() const { return m_runsWidth; }
    bool isEmpty() const
    {
        if (!m_fragments.size())
            return true;
        if (!m_lastCompleteFragment.isEmpty())
            return false;
        return m_fragments.last().overlapsToNextRenderer();
    }

    void appendFragmentAndCreateRunIfNeeded(const TextFragmentIterator::TextFragment& fragment, Layout::RunVector& runs)
    {
        // Adjust end position while collapsing.
        unsigned endPosition = fragment.isCollapsed() ? fragment.start() + 1 : fragment.end();
        // New line needs new run.
        if (!m_runsWidth)
            runs.append(Run(fragment.start(), endPosition, m_runsWidth, m_runsWidth + fragment.width(), false));
        else {
            const auto& lastFragment = m_fragments.last();
            // Advance last completed fragment when the previous fragment is all set (including multiple parts across renderers)
            if ((lastFragment.type() != fragment.type()) || !lastFragment.overlapsToNextRenderer())
                m_lastCompleteFragment = lastFragment;
            // Collapse neighbouring whitespace, if they are across multiple renderers and are not collapsed yet.
            if (lastFragment.isCollapsible() && fragment.isCollapsible()) {
                ASSERT(lastFragment.isLastInRenderer());
                if (!lastFragment.isCollapsed()) {
                    // Line width needs to be reset so that now it takes collapsing into consideration.
                    m_runsWidth -= (lastFragment.width() - m_collapsedWhitespaceWidth);
                }
                // This fragment is collapsed completely. No run is needed.
                return;
            }
            if (lastFragment.isLastInRenderer() || lastFragment.isCollapsed())
                runs.append(Run(fragment.start(), endPosition, m_runsWidth, m_runsWidth + fragment.width(), false));
            else {
                Run& lastRun = runs.last();
                lastRun.end = endPosition;
                lastRun.logicalRight += fragment.width();
            }
        }
        m_fragments.append(fragment);
        m_runsWidth += fragment.width();

        if (fragment.type() == TextFragmentIterator::TextFragment::Whitespace) {
            m_trailingWhitespaceLength += endPosition - fragment.start();
            m_trailingWhitespaceWidth += fragment.width();
        } else {
            m_trailingWhitespaceLength = 0;
            m_trailingWhitespaceWidth = 0;
        }

        if (!m_firstCharacterFits)
            m_firstCharacterFits = fragment.start() + 1 > endPosition || m_runsWidth <= m_availableWidth;
    }

    TextFragmentIterator::TextFragment revertToLastCompleteFragment(Layout::RunVector& runs)
    {
        ASSERT(m_fragments.size());
        unsigned revertLength = 0;
        float revertWidth = 0;
        while (m_fragments.size()) {
            const auto& current = m_fragments.last();
            if (current == m_lastCompleteFragment)
                break;
            revertLength += current.end() - current.start();
            revertWidth += current.width();
            m_fragments.removeLast();
        }
        m_runsWidth -= revertWidth;
        if (revertLength)
            revertRuns(runs, revertLength, revertWidth);
        return m_lastCompleteFragment;
    }

    void removeTrailingWhitespace(Layout::RunVector& runs)
    {
        if (!m_trailingWhitespaceLength)
            return;
        revertRuns(runs, m_trailingWhitespaceLength, m_trailingWhitespaceWidth);
        m_runsWidth -= m_trailingWhitespaceWidth;
        ASSERT(m_fragments.last().type() == TextFragmentIterator::TextFragment::Whitespace);
        while (m_fragments.size()) {
            const auto& current = m_fragments.last();
            if (current.type() != TextFragmentIterator::TextFragment::Whitespace)
                break;
#if !ASSERT_DISABLED
            m_trailingWhitespaceLength -= (current.isCollapsed() ? 1 : current.end() - current.start());
            m_trailingWhitespaceWidth -= current.width();
#endif
            m_fragments.removeLast();
        }
#if !ASSERT_DISABLED
        ASSERT(!m_trailingWhitespaceLength);
        ASSERT(!m_trailingWhitespaceWidth);
#endif
        m_trailingWhitespaceLength = 0;
        m_trailingWhitespaceWidth = 0;
    }

private:
    float m_availableWidth { 0 };
    float m_logicalLeftOffset { 0 };
    TextFragmentIterator::TextFragment m_overflowedFragment;
    float m_runsWidth { 0 };
    TextFragmentIterator::TextFragment m_lastCompleteFragment;
    float m_trailingWhitespaceWidth { 0 }; // Use this to remove trailing whitespace without re-mesuring the text.
    unsigned m_trailingWhitespaceLength { 0 };
    float m_collapsedWhitespaceWidth { 0 };
    // Having one character on the line does not necessarily mean it actually fits.
    // First character of the first fragment might be forced on to the current line even if it does not fit.
    bool m_firstCharacterFits { false };
    Vector<TextFragmentIterator::TextFragment> m_fragments;
};

class FragmentForwardIterator : public std::iterator<std::forward_iterator_tag, unsigned> {
public:
    FragmentForwardIterator(unsigned fragmentIndex)
        : m_fragmentIndex(fragmentIndex)
    {
    }

    FragmentForwardIterator& operator++()
    {
        ++m_fragmentIndex;
        return *this;
    }

    bool operator!=(const FragmentForwardIterator& other) const { return m_fragmentIndex != other.m_fragmentIndex; }
    unsigned operator*() const { return m_fragmentIndex; }

private:
    unsigned m_fragmentIndex { 0 };
};

static FragmentForwardIterator begin(const TextFragmentIterator::TextFragment& fragment)  { return FragmentForwardIterator(fragment.start()); }
static FragmentForwardIterator end(const TextFragmentIterator::TextFragment& fragment)  { return FragmentForwardIterator(fragment.end()); }

static bool preWrap(const TextFragmentIterator::Style& style)
{
    return style.wrapLines && !style.collapseWhitespace;
}
    
static void removeTrailingWhitespace(LineState& lineState, Layout::RunVector& runs, const TextFragmentIterator& textFragmentIterator)
{
    if (!lineState.hasTrailingWhitespace())
        return;

    // Remove collapsed whitespace, or non-collapsed pre-wrap whitespace, unless it's the only content on the line -so removing the whitesapce would produce an empty line.
    const auto& style = textFragmentIterator.style();
    bool collapseWhitespace = style.collapseWhitespace | preWrap(style);
    if (!collapseWhitespace)
        return;

    if (preWrap(style) && lineState.isWhitespaceOnly())
        return;

    lineState.removeTrailingWhitespace(runs);
}

static void updateLineConstrains(const RenderBlockFlow& flow, LineState& line)
{
    LayoutUnit height = flow.logicalHeight();
    LayoutUnit logicalHeight = flow.minLineHeightForReplacedRenderer(false, 0);
    float logicalRightOffset = flow.logicalRightOffsetForLine(height, false, logicalHeight);
    line.setLogicalLeftOffset(flow.logicalLeftOffsetForLine(height, false, logicalHeight));
    line.setAvailableWidth(std::max<float>(0, logicalRightOffset - line.logicalLeftOffset()));
}

static TextFragmentIterator::TextFragment splitFragmentToFitLine(TextFragmentIterator::TextFragment& fragmentToSplit, float availableWidth, bool keepAtLeastOneCharacter, const TextFragmentIterator& textFragmentIterator)
{
    // FIXME: add surrogate pair support.
    unsigned start = fragmentToSplit.start();
    auto it = std::upper_bound(begin(fragmentToSplit), end(fragmentToSplit), availableWidth, [&textFragmentIterator, start](float availableWidth, unsigned index) {
        // FIXME: use the actual left position of the line (instead of 0) to calculated width. It might give false width for tab characters.
        return availableWidth < textFragmentIterator.textWidth(start, index + 1, 0);
    });
    unsigned splitPosition = (*it);
    if (keepAtLeastOneCharacter && splitPosition == fragmentToSplit.start())
        ++splitPosition;
    return fragmentToSplit.split(splitPosition, textFragmentIterator);
}

enum PreWrapLineBreakRule { Preserve, Ignore };

static TextFragmentIterator::TextFragment consumeLineBreakIfNeeded(const TextFragmentIterator::TextFragment& fragment, TextFragmentIterator& textFragmentIterator, LineState& line, Layout::RunVector& runs,
    PreWrapLineBreakRule preWrapLineBreakRule = PreWrapLineBreakRule::Preserve)
{
    if (!fragment.isLineBreak())
        return fragment;

    if (preWrap(textFragmentIterator.style()) && preWrapLineBreakRule != PreWrapLineBreakRule::Ignore)
        return fragment;

    // <br> always produces a run. (required by testing output)
    if (fragment.type() == TextFragmentIterator::TextFragment::HardLineBreak)
        line.appendFragmentAndCreateRunIfNeeded(fragment, runs);
    return textFragmentIterator.nextTextFragment();
}

static TextFragmentIterator::TextFragment skipWhitespaceIfNeeded(const TextFragmentIterator::TextFragment& fragment, TextFragmentIterator& textFragmentIterator)
{
    if (!textFragmentIterator.style().collapseWhitespace)
        return fragment;

    TextFragmentIterator::TextFragment firstNonWhitespaceFragment = fragment;
    while (firstNonWhitespaceFragment.type() == TextFragmentIterator::TextFragment::Whitespace)
        firstNonWhitespaceFragment = textFragmentIterator.nextTextFragment();
    return firstNonWhitespaceFragment;
}

static TextFragmentIterator::TextFragment firstFragment(TextFragmentIterator& textFragmentIterator, LineState& currentLine, const LineState& previousLine, Layout::RunVector& runs)
{
    // Handle overflowed fragment from previous line.
    TextFragmentIterator::TextFragment firstFragment(previousLine.overflowedFragment());

    if (firstFragment.isEmpty())
        firstFragment = textFragmentIterator.nextTextFragment();
    else if (firstFragment.type() == TextFragmentIterator::TextFragment::Whitespace && preWrap(textFragmentIterator.style()) && previousLine.firstCharacterFits()) {
        // Special overflow pre-wrap whitespace handling: skip the overflowed whitespace (even when style says not-collapsible) if we managed to fit at least one character on the previous line.
        firstFragment = textFragmentIterator.nextTextFragment();
        // If skipping the whitespace puts us on a newline, skip the newline too as we already wrapped the line.
        firstFragment = consumeLineBreakIfNeeded(firstFragment, textFragmentIterator, currentLine, runs, PreWrapLineBreakRule::Ignore);
    }
    return skipWhitespaceIfNeeded(firstFragment, textFragmentIterator);
}

static void forceFragmentToLine(LineState& line, TextFragmentIterator& textFragmentIterator, Layout::RunVector& runs, const TextFragmentIterator::TextFragment& fragment)
{
    line.appendFragmentAndCreateRunIfNeeded(fragment, runs);
    // Check if there are more fragments to add to the current line.
    auto nextFragment = textFragmentIterator.nextTextFragment();
    if (fragment.overlapsToNextRenderer()) {
        while (true) {
            if (nextFragment.type() != fragment.type())
                break;
            line.appendFragmentAndCreateRunIfNeeded(nextFragment, runs);
            // Does it overlap to the next segment?
            if (!nextFragment.overlapsToNextRenderer())
                return;
            nextFragment = textFragmentIterator.nextTextFragment();
        }
    }
    // When the forced fragment is followed by either whitespace and/or line break, consume them too, otherwise we end up with an extra whitespace and/or line break.
    nextFragment = skipWhitespaceIfNeeded(nextFragment, textFragmentIterator);
    nextFragment = consumeLineBreakIfNeeded(nextFragment, textFragmentIterator, line, runs);
    line.setOverflowedFragment(nextFragment);
}

static bool createLineRuns(LineState& line, const LineState& previousLine, Layout::RunVector& runs, TextFragmentIterator& textFragmentIterator)
{
    const auto& style = textFragmentIterator.style();
    line.setCollapedWhitespaceWidth(style.spaceWidth);
    bool lineCanBeWrapped = style.wrapLines || style.breakWordOnOverflow;
    auto fragment = firstFragment(textFragmentIterator, line, previousLine, runs);
    while (fragment.type() != TextFragmentIterator::TextFragment::ContentEnd) {
        // Hard linebreak.
        if (fragment.isLineBreak()) {
            // Add the new line fragment only if there's nothing on the line. (otherwise the extra new line character would show up at the end of the content.)
            if (line.isEmpty() || fragment.type() == TextFragmentIterator::TextFragment::HardLineBreak) {
                if (style.textAlign == RIGHT || style.textAlign == WEBKIT_RIGHT)
                    line.removeTrailingWhitespace(runs);
                line.appendFragmentAndCreateRunIfNeeded(fragment, runs);
            }
            break;
        }
        if (lineCanBeWrapped && !line.fits(fragment.width())) {
            // Overflow wrapping behaviour:
            // 1. Whitesapce collapse on: whitespace is skipped. Jump to next line.
            // 2. Whitespace collapse off: whitespace is wrapped.
            // 3. First, non-whitespace fragment is either wrapped or kept on the line. (depends on overflow-wrap)
            // 4. Non-whitespace fragment when there's already another fragment on the line gets pushed to the next line.
            bool emptyLine = line.isEmpty();
            // Whitespace fragment.
            if (fragment.type() == TextFragmentIterator::TextFragment::Whitespace) {
                if (!style.collapseWhitespace) {
                    // Split the fragment; (modified)fragment stays on this line, overflowedFragment is pushed to next line.
                    line.setOverflowedFragment(splitFragmentToFitLine(fragment, line.availableWidth() - line.width(), emptyLine, textFragmentIterator));
                    line.appendFragmentAndCreateRunIfNeeded(fragment, runs);
                }
                // When whitespace collapse is on, whitespace that doesn't fit is simply skipped.
                break;
            }
            // Non-whitespace fragment. (!style.wrapLines: bug138102(preserve existing behavior)
            if ((emptyLine && style.breakWordOnOverflow) || !style.wrapLines) {
                // Split the fragment; (modified)fragment stays on this line, overflowedFragment is pushed to next line.
                line.setOverflowedFragment(splitFragmentToFitLine(fragment, line.availableWidth() - line.width(), emptyLine, textFragmentIterator));
                line.appendFragmentAndCreateRunIfNeeded(fragment, runs);
                break;
            }
            // Non-breakable non-whitespace first fragment. Add it to the current line. -it overflows though.
            ASSERT(fragment.type() == TextFragmentIterator::TextFragment::NonWhitespace);
            if (emptyLine) {
                forceFragmentToLine(line, textFragmentIterator, runs, fragment);
                break;
            }
            // Non-breakable non-whitespace fragment when there's already content on the line. Push it to the next line.
            if (line.lastFragment().overlapsToNextRenderer()) {
                // Check if this fragment is a continuation of a previous segment. In such cases, we need to remove them all.
                const auto& lastCompleteFragment = line.revertToLastCompleteFragment(runs);
                textFragmentIterator.revertToEndOfFragment(lastCompleteFragment);
                break;
            }
            line.setOverflowedFragment(fragment);
            break;
        }
        line.appendFragmentAndCreateRunIfNeeded(fragment, runs);
        // Find the next text fragment.
        fragment = textFragmentIterator.nextTextFragment(line.width());
    }
    return (fragment.type() == TextFragmentIterator::TextFragment::ContentEnd && line.overflowedFragment().isEmpty()) || line.overflowedFragment().type() == TextFragmentIterator::TextFragment::ContentEnd;
}

static void closeLineEndingAndAdjustRuns(LineState& line, Layout::RunVector& runs, unsigned previousRunCount, unsigned& lineCount, const TextFragmentIterator& textFragmentIterator)
{
    if (previousRunCount == runs.size())
        return;
    ASSERT(runs.size());
    removeTrailingWhitespace(line, runs, textFragmentIterator);
    if (!runs.size())
        return;
    // Adjust runs' position by taking line's alignment into account.
    if (float lineLogicalLeft = computeLineLeft(textFragmentIterator.style().textAlign, line.availableWidth(), line.width(), line.logicalLeftOffset())) {
        for (unsigned i = previousRunCount; i < runs.size(); ++i) {
            runs[i].logicalLeft += lineLogicalLeft;
            runs[i].logicalRight += lineLogicalLeft;
        }
    }
    runs.last().isEndOfLine = true;
    ++lineCount;
}

static void createTextRuns(Layout::RunVector& runs, RenderBlockFlow& flow, unsigned& lineCount)
{
    LayoutUnit borderAndPaddingBefore = flow.borderAndPaddingBefore();
    LayoutUnit lineHeight = lineHeightFromFlow(flow);
    LineState line;
    bool isEndOfContent = false;
    TextFragmentIterator textFragmentIterator = TextFragmentIterator(flow);

    do {
        flow.setLogicalHeight(lineHeight * lineCount + borderAndPaddingBefore);
        LineState previousLine = line;
        unsigned previousRunCount = runs.size();
        line = LineState();
        updateLineConstrains(flow, line);
        isEndOfContent = createLineRuns(line, previousLine, runs, textFragmentIterator);
        closeLineEndingAndAdjustRuns(line, runs, previousRunCount, lineCount, textFragmentIterator);
    } while (!isEndOfContent);
}

std::unique_ptr<Layout> create(RenderBlockFlow& flow)
{
    unsigned lineCount = 0;
    Layout::RunVector runs;

    createTextRuns(runs, flow, lineCount);
    for (auto& renderer : childrenOfType<RenderObject>(flow)) {
        ASSERT(is<RenderText>(renderer) || is<RenderLineBreak>(renderer));
        renderer.clearNeedsLayout();
    }
    return Layout::create(runs, lineCount);
}

std::unique_ptr<Layout> Layout::create(const RunVector& runVector, unsigned lineCount)
{
    void* slot = WTF::fastMalloc(sizeof(Layout) + sizeof(Run) * runVector.size());
    return std::unique_ptr<Layout>(new (NotNull, slot) Layout(runVector, lineCount));
}

Layout::Layout(const RunVector& runVector, unsigned lineCount)
    : m_lineCount(lineCount)
    , m_runCount(runVector.size())
{
    memcpy(m_runs, runVector.data(), m_runCount * sizeof(Run));
}

}
}
