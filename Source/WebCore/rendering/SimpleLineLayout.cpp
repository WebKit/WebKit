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
#include "RenderStyle.h"
#include "RenderText.h"
#include "RenderTextControl.h"
#include "RenderView.h"
#include "Settings.h"
#include "SimpleLineLayoutFlowContents.h"
#include "SimpleLineLayoutFlowContentsIterator.h"
#include "SimpleLineLayoutFunctions.h"
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
    // This currently covers <blockflow>#text</blockflow> and mutiple (sibling) RenderText cases.
    // The <blockflow><inline>#text</inline></blockflow> case is also popular and should be relatively easy to cover.
    for (const auto& renderer : childrenOfType<RenderObject>(flow)) {
        if (!is<RenderText>(renderer))
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
    if (style.hasTextCombine())
        return false;
    if (style.backgroundClip() == TextFillBox)
        return false;
    if (style.borderFit() == BorderFitLines)
        return false;
    if (style.lineBreak() != LineBreakAuto)
        return false;

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

struct LineState {
    void commitAndCreateRun(Layout::RunVector& lineRuns)
    {
        if (uncommittedStart == uncommittedEnd)
            return;

        lineRuns.append(Run(uncommittedStart, uncommittedEnd, committedWidth, committedWidth + uncommittedWidth, false));
        // Move uncommitted to committed.
        committedWidth += uncommittedWidth;
        committedTrailingWhitespaceWidth = uncomittedTrailingWhitespaceWidth;
        committedTrailingWhitespaceLength = uncomittedTrailingWhitespaceLength;
        if (!m_firstCharacterFits)
            m_firstCharacterFits = uncommittedStart + 1 > uncommittedEnd || committedWidth <= availableWidth;

        uncommittedStart = uncommittedEnd;
        uncommittedWidth = 0;
        uncomittedTrailingWhitespaceWidth = 0;
        uncomittedTrailingWhitespaceLength = 0;
        m_newUncommittedSegment = true;
    }

    void addUncommitted(const FlowContentsIterator::TextFragment& fragment)
    {
        // Start a new uncommitted segment.
        if (m_newUncommittedSegment) {
            uncommittedStart = fragment.start;
            m_newUncommittedSegment = false;
        }
        uncommittedWidth += fragment.width;
        uncommittedEnd = fragment.end;
        uncomittedTrailingWhitespaceWidth = fragment.type == FlowContentsIterator::TextFragment::Whitespace ? fragment.width : 0;
        uncomittedTrailingWhitespaceLength = fragment.type == FlowContentsIterator::TextFragment::Whitespace ? fragment.end - fragment.start  : 0;
    }

    void addUncommittedWhitespace(float whitespaceWidth)
    {
        addUncommitted(FlowContentsIterator::TextFragment(uncommittedEnd, uncommittedEnd + 1, whitespaceWidth, true));
    }

    bool hasWhitespaceOnly() const
    {
        return committedTrailingWhitespaceWidth && committedWidth == committedTrailingWhitespaceWidth;
    }

    float width() const
    {
        return committedWidth + uncommittedWidth;
    }

    bool fits(float extra) const
    {
        return availableWidth >= width() + extra;
    }

    void removeTrailingWhitespace()
    {
        committedWidth -= committedTrailingWhitespaceWidth;
        committedTrailingWhitespaceWidth = 0;
        committedTrailingWhitespaceLength = 0;
    }

    float availableWidth { 0 };
    float logicalLeftOffset { 0 };

    unsigned uncommittedStart { 0 };
    unsigned uncommittedEnd { 0 };
    float uncommittedWidth { 0 };
    float committedWidth { 0 };
    float committedTrailingWhitespaceWidth { 0 }; // Use this to remove trailing whitespace without re-mesuring the text.
    unsigned committedTrailingWhitespaceLength { 0 };
    // Having one character on the line does not necessarily mean it actually fits.
    // First character of the first fragment might be forced on to the current line even if it does not fit.
    bool m_firstCharacterFits { false };
    bool m_newUncommittedSegment { true };

    FlowContentsIterator::TextFragment overflowedFragment;

private:
    float uncomittedTrailingWhitespaceWidth { 0 };
    unsigned uncomittedTrailingWhitespaceLength { 0 };
};

static bool preWrap(const FlowContentsIterator::Style& style)
{
    return style.wrapLines && !style.collapseWhitespace;
}
    
static void removeTrailingWhitespace(LineState& lineState, Layout::RunVector& lineRuns, const FlowContentsIterator& flowContentsIterator)
{
    if (!lineState.committedTrailingWhitespaceLength)
        return;
    
    // Remove collapsed whitespace, or non-collapsed pre-wrap whitespace, unless it's the only content on the line -so removing the whitesapce would produce an empty line.
    const auto& style = flowContentsIterator.style();
    bool collapseWhitespace = style.collapseWhitespace | preWrap(style);
    if (!collapseWhitespace)
        return;

    if (preWrap(style) && lineState.hasWhitespaceOnly())
        return;

    ASSERT(lineRuns.size());
    Run& lastRun = lineRuns.last();
    lastRun.logicalRight -= lineState.committedTrailingWhitespaceWidth;
    lastRun.end -= lineState.committedTrailingWhitespaceLength;
    if (lastRun.start == lastRun.end)
        lineRuns.removeLast();
    lineState.removeTrailingWhitespace();
}

static void updateLineConstrains(const RenderBlockFlow& flow, float& availableWidth, float& logicalLeftOffset)
{
    LayoutUnit height = flow.logicalHeight();
    LayoutUnit logicalHeight = flow.minLineHeightForReplacedRenderer(false, 0);
    float logicalRightOffset = flow.logicalRightOffsetForLine(height, false, logicalHeight);
    logicalLeftOffset = flow.logicalLeftOffsetForLine(height, false, logicalHeight);
    availableWidth = std::max<float>(0, logicalRightOffset - logicalLeftOffset);
}

static FlowContentsIterator::TextFragment splitFragmentToFitLine(FlowContentsIterator::TextFragment& fragmentToSplit, float availableWidth, bool keepAtLeastOneCharacter, const FlowContentsIterator& flowContentsIterator)
{
    // Fast path for single char fragments.
    if (fragmentToSplit.start + 1 == fragmentToSplit.end) {
        if (keepAtLeastOneCharacter)
            return FlowContentsIterator::TextFragment();

        FlowContentsIterator::TextFragment fragmentForNextLine(fragmentToSplit);
        fragmentToSplit.end = fragmentToSplit.start;
        fragmentToSplit.width = 0;
        return fragmentForNextLine;
    }
    // Simple binary search to find out what fits the current line.
    // FIXME: add surrogate pair support.
    unsigned left = fragmentToSplit.start;
    unsigned right = fragmentToSplit.end - 1; // We can ignore the last character. It surely does not fit.
    float width = 0;
    while (left < right) {
        unsigned middle = (left + right) / 2;
        width = flowContentsIterator.textWidth(fragmentToSplit.start, middle + 1, 0);
        if (availableWidth > width)
            left = middle + 1;
        else if (availableWidth < width)
            right = middle;
        else {
            right = middle + 1;
            break;
        }
    }

    if (keepAtLeastOneCharacter && right == fragmentToSplit.start)
        ++right;
    FlowContentsIterator::TextFragment fragmentForNextLine(fragmentToSplit);
    fragmentToSplit.end = right;
    fragmentToSplit.width = fragmentToSplit.isEmpty() ? 0 : flowContentsIterator.textWidth(fragmentToSplit.start, fragmentToSplit.end, 0);

    fragmentForNextLine.start = fragmentToSplit.end;
    fragmentForNextLine.width -= fragmentToSplit.width;
    return fragmentForNextLine;
}

static FlowContentsIterator::TextFragment firstFragment(FlowContentsIterator& flowContentsIterator, const LineState& previousLine)
{
    // Handle overflowed fragment from previous line.
    FlowContentsIterator::TextFragment firstFragment = previousLine.overflowedFragment;
    const auto& style = flowContentsIterator.style();

    if (firstFragment.isEmpty())
        firstFragment = flowContentsIterator.nextTextFragment();
    else {
        // Special overflow pre-wrap whitespace handling: ignore the overflowed whitespace if we managed to fit at least one character on the previous line.
        // When the line is too short to fit one character (thought it still stays on the line) we continue with the overflow whitespace content on this line.
        if (firstFragment.type == FlowContentsIterator::TextFragment::Whitespace && preWrap(style) && previousLine.m_firstCharacterFits) {
            firstFragment = flowContentsIterator.nextTextFragment();
            // If skipping the whitespace puts us on a hard newline, skip the newline too as we already wrapped the line.
            if (firstFragment.type == FlowContentsIterator::TextFragment::LineBreak)
                firstFragment = flowContentsIterator.nextTextFragment();
        }
    }

    // Check if we need to skip the leading whitespace.
    if (style.collapseWhitespace && firstFragment.type == FlowContentsIterator::TextFragment::Whitespace)
        firstFragment = flowContentsIterator.nextTextFragment();
    return firstFragment;
}

static bool createLineRuns(LineState& line, const LineState& previousLine, Layout::RunVector& lineRuns, FlowContentsIterator& flowContentsIterator)
{
    const auto& style = flowContentsIterator.style();
    bool lineCanBeWrapped = style.wrapLines || style.breakWordOnOverflow;
    auto fragment = firstFragment(flowContentsIterator, previousLine);
    while (!fragment.isEmpty()) {
        // Hard linebreak.
        if (fragment.type == FlowContentsIterator::TextFragment::LineBreak) {
            // Add the new line fragment only if there's nothing on the line. (otherwise the extra new line character would show up at the end of the content.)
            if (!line.width())
                line.addUncommitted(fragment);
            break;
        }
        if (lineCanBeWrapped && !line.fits(fragment.width)) {
            // Overflow wrapping behaviour:
            // 1. Whitesapce collapse on: whitespace is skipped. Jump to next line.
            // 2. Whitespace collapse off: whitespace is wrapped.
            // 3. First, non-whitespace fragment is either wrapped or kept on the line. (depends on overflow-wrap)
            // 4. Non-whitespace fragment when there's already another fragment on the line gets pushed to the next line.
            bool emptyLine = !line.width();
            // Whitespace fragment.
            if (fragment.type == FlowContentsIterator::TextFragment::Whitespace) {
                if (!style.collapseWhitespace) {
                    // Split the fragment; (modified)fragment stays on this line, overflowedFragment is pushed to next line.
                    line.overflowedFragment = splitFragmentToFitLine(fragment, line.availableWidth - line.width(), emptyLine, flowContentsIterator);
                    line.addUncommitted(fragment);
                }
                // When whitespace collapse is on, whitespace that doesn't fit is simply skipped.
                break;
            }
            // Non-whitespace fragment. (!style.wrapLines: bug138102(preserve existing behavior)
            if ((emptyLine && style.breakWordOnOverflow) || !style.wrapLines) {
                // Split the fragment; (modified)fragment stays on this line, overflowedFragment is pushed to next line.
                line.overflowedFragment = splitFragmentToFitLine(fragment, line.availableWidth - line.width(), emptyLine, flowContentsIterator);
                line.addUncommitted(fragment);
                break;
            }
            // Non-breakable non-whitespace first fragment. Add it to the current line. -it overflows though.
            if (emptyLine) {
                line.addUncommitted(fragment);
                break;
            }
            // Non-breakable non-whitespace fragment when there's already content on the line. Push it to the next line.
            line.overflowedFragment = fragment;
            break;
        }
        // When the current fragment is collapsed whitespace, we need to create a run for what we've processed so far.
        if (fragment.isCollapsed) {
            // One trailing whitespace to preserve.
            line.addUncommittedWhitespace(style.spaceWidth);
            line.commitAndCreateRun(lineRuns);
        } else
            line.addUncommitted(fragment);
        // Find the next text fragment.
        fragment = flowContentsIterator.nextTextFragment(line.width());
    }
    line.commitAndCreateRun(lineRuns);
    return fragment.isEmpty() && line.overflowedFragment.isEmpty();
}

static void closeLineEndingAndAdjustRuns(LineState& line, Layout::RunVector& runs, unsigned previousRunCount, unsigned& lineCount, const FlowContentsIterator& flowContentsIterator)
{
    if (previousRunCount == runs.size())
        return;
    ASSERT(runs.size());
    removeTrailingWhitespace(line, runs, flowContentsIterator);
    if (!runs.size())
        return;
    // Adjust runs' position by taking line's alignment into account.
    if (float lineLogicalLeft = computeLineLeft(flowContentsIterator.style().textAlign, line.availableWidth, line.committedWidth, line.logicalLeftOffset)) {
        for (unsigned i = previousRunCount; i < runs.size(); ++i) {
            runs[i].logicalLeft += lineLogicalLeft;
            runs[i].logicalRight += lineLogicalLeft;
        }
    }
    runs.last().isEndOfLine = true;
    ++lineCount;
}

static void splitRunsAtRendererBoundary(Layout::RunVector& lineRuns, const FlowContentsIterator& flowContentsIterator)
{
    // FIXME: We should probably split during run construction instead of as a separate pass.
    if (lineRuns.isEmpty())
        return;
    unsigned runIndex = 0;
    do {
        const Run& run = lineRuns.at(runIndex);
        ASSERT(run.start != run.end);
        auto& startSegment = flowContentsIterator.segmentForPosition(run.start);
        if (run.end <= startSegment.end)
            continue;
        // This run overlaps multiple renderers. Split it up.
        // Split run at the renderer's boundary and create a new run for the left side, while use the current run as the right side.
        float logicalRightOfLeftRun = run.logicalLeft + flowContentsIterator.textWidth(run.start, startSegment.end, run.logicalLeft);
        lineRuns.insert(runIndex, Run(run.start, startSegment.end, run.logicalLeft, logicalRightOfLeftRun, false));
        Run& rightSideRun = lineRuns.at(runIndex + 1);
        rightSideRun.start = startSegment.end;
        rightSideRun.logicalLeft = logicalRightOfLeftRun;
    } while (++runIndex < lineRuns.size());
}

static void createTextRuns(Layout::RunVector& runs, RenderBlockFlow& flow, unsigned& lineCount)
{
    LayoutUnit borderAndPaddingBefore = flow.borderAndPaddingBefore();
    LayoutUnit lineHeight = lineHeightFromFlow(flow);
    LineState line;
    bool isEndOfContent = false;
    FlowContentsIterator flowContentsIterator = FlowContentsIterator(flow);

    do {
        flow.setLogicalHeight(lineHeight * lineCount + borderAndPaddingBefore);
        LineState previousLine = line;
        unsigned previousRunCount = runs.size();
        line = LineState();
        updateLineConstrains(flow, line.availableWidth, line.logicalLeftOffset);
        isEndOfContent = createLineRuns(line, previousLine, runs, flowContentsIterator);
        closeLineEndingAndAdjustRuns(line, runs, previousRunCount, lineCount, flowContentsIterator);
    } while (!isEndOfContent);

    if (flow.firstChild() != flow.lastChild())
        splitRunsAtRendererBoundary(runs, flowContentsIterator);
    ASSERT(!line.uncommittedWidth);
}

std::unique_ptr<Layout> create(RenderBlockFlow& flow)
{
    unsigned lineCount = 0;
    Layout::RunVector runs;

    createTextRuns(runs, flow, lineCount);
    for (auto& renderer : childrenOfType<RenderObject>(flow)) {
        ASSERT(is<RenderText>(renderer));
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
