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

        lineRuns.append(Run(uncommittedStart, uncommittedEnd, committedLogicalRight, committedLogicalRight + uncommittedWidth, false));
        // Move uncommitted to committed.
        committedWidth += uncommittedWidth;
        committedLogicalRight += committedWidth;
        committedTrailingWhitespaceWidth = uncomittedTrailingWhitespaceWidth;
        committedTrailingWhitespaceLength = uncomittedTrailingWhitespaceLength;

        uncommittedStart = uncommittedEnd;
        uncommittedWidth = 0;
        uncomittedTrailingWhitespaceWidth = 0;
        uncomittedTrailingWhitespaceLength = 0;
    }

    void addUncommitted(const FlowContents::TextFragment& fragment)
    {
        unsigned uncomittedFragmentLength = fragment.end - uncommittedEnd;
        uncommittedWidth += fragment.width;
        uncommittedEnd = fragment.end;
        position = uncommittedEnd;
        uncomittedTrailingWhitespaceWidth = fragment.type == FlowContents::TextFragment::Whitespace ? fragment.width : 0;
        uncomittedTrailingWhitespaceLength = fragment.type == FlowContents::TextFragment::Whitespace ? uncomittedFragmentLength  : 0;
    }

    void addUncommittedWhitespace(float whitespaceWidth)
    {
        addUncommitted(FlowContents::TextFragment(uncommittedEnd, uncommittedEnd + 1, whitespaceWidth, true));
    }

    void jumpTo(unsigned newPositon, float logicalRight)
    {
        position = newPositon;

        uncommittedStart = newPositon;
        uncommittedEnd = newPositon;
        uncommittedWidth = 0;
        committedLogicalRight = logicalRight;
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
        committedLogicalRight -= committedTrailingWhitespaceWidth;
        committedTrailingWhitespaceWidth = 0;
        committedTrailingWhitespaceLength = 0;
    }

    float availableWidth { 0 };
    float logicalLeftOffset { 0 };
    unsigned lineStartRunIndex { 0 }; // The run that the line starts with.
    unsigned position { 0 };

    unsigned uncommittedStart { 0 };
    unsigned uncommittedEnd { 0 };
    float uncommittedWidth { 0 };
    float committedWidth { 0 };

    float committedLogicalRight { 0 }; // Last committed X (coordinate) position.
    float committedTrailingWhitespaceWidth { 0 }; // Use this to remove trailing whitespace without re-mesuring the text.
    unsigned committedTrailingWhitespaceLength { 0 };

    FlowContents::TextFragment oveflowedFragment;

private:
    float uncomittedTrailingWhitespaceWidth { 0 };
    unsigned uncomittedTrailingWhitespaceLength { 0 };
};

static void removeTrailingWhitespace(LineState& lineState, Layout::RunVector& lineRuns, const FlowContents& flowContents)
{
    const auto& style = flowContents.style();
    bool preWrap = style.wrapLines && !style.collapseWhitespace;
    // Trailing whitespace gets removed when we either collapse whitespace or pre-wrap is present.
    if (!(style.collapseWhitespace || preWrap))
        return;

    ASSERT(lineRuns.size());
    Run& lastRun = lineRuns.last();

    unsigned lastPosition = lineState.position;
    bool trailingPreWrapWhitespaceNeedsToBeRemoved = false;
    if (preWrap) {
        // Special overflow pre-wrap fragment handling: Ignore the overflow whitespace fragment if we managed to fit at least one character on this line.
        // When the line is too short to fit one character (thought it still stays on the line) we keep the overflow whitespace content as it is.
        if (lineState.oveflowedFragment.type == FlowContents::TextFragment::Whitespace && !lineState.oveflowedFragment.isEmpty() && lineState.availableWidth >= lineState.committedWidth) {
            lineState.position = lineState.oveflowedFragment.end;
            lineState.oveflowedFragment = FlowContents::TextFragment();
        }
        // Remove whitespace, unless it's the only fragment on the line -so removing the whitesapce would produce an empty line.
        trailingPreWrapWhitespaceNeedsToBeRemoved = !lineState.hasWhitespaceOnly();
    }
    if (lineState.committedTrailingWhitespaceLength && (style.collapseWhitespace || trailingPreWrapWhitespaceNeedsToBeRemoved)) {
        lastRun.logicalRight -= lineState.committedTrailingWhitespaceWidth;
        lastRun.end -= lineState.committedTrailingWhitespaceLength;
        if (lastRun.start == lastRun.end)
            lineRuns.removeLast();
        lineState.removeTrailingWhitespace();
    }

    // If we skipped any whitespace and now the line end is a hard newline, skip the newline too as we are wrapping the line here already.
    if (lastPosition != lineState.position && !flowContents.isEnd(lineState.position) && flowContents.isLineBreak(lineState.position))
        ++lineState.position;
}

static void updateLineConstrains(const RenderBlockFlow& flow, float& availableWidth, float& logicalLeftOffset)
{
    LayoutUnit height = flow.logicalHeight();
    LayoutUnit logicalHeight = flow.minLineHeightForReplacedRenderer(false, 0);
    float logicalRightOffset = flow.logicalRightOffsetForLine(height, false, logicalHeight);
    logicalLeftOffset = flow.logicalLeftOffsetForLine(height, false, logicalHeight);
    availableWidth = std::max<float>(0, logicalRightOffset - logicalLeftOffset);
}

static LineState initializeNewLine(const LineState& previousLine, const RenderBlockFlow& flow, const FlowContents& flowContents, unsigned lineStartRunIndex)
{
    LineState lineState;
    lineState.jumpTo(previousLine.position, 0);
    lineState.lineStartRunIndex = lineStartRunIndex;
    updateLineConstrains(flow, lineState.availableWidth, lineState.logicalLeftOffset);
    // Skip leading whitespace if collapsing whitespace, unless there's an uncommitted fragment pushed from the previous line.
    // FIXME: Be smarter when the run from the previous line does not fit the current line. Right now, we just reprocess it.
    if (previousLine.oveflowedFragment.width) {
        if (lineState.fits(previousLine.oveflowedFragment.width))
            lineState.addUncommitted(previousLine.oveflowedFragment);
        else {
            // Start over with this fragment.
            lineState.jumpTo(previousLine.oveflowedFragment.start, 0);
        }
    } else {
        unsigned spaceCount = 0;
        lineState.jumpTo(flowContents.style().collapseWhitespace ? flowContents.findNextNonWhitespacePosition(previousLine.position, spaceCount) : previousLine.position, 0);
    }
    return lineState;
}

static FlowContents::TextFragment splitFragmentToFitLine(FlowContents::TextFragment& fragmentToSplit, float availableWidth, bool keepAtLeastOneCharacter, const FlowContents& flowContents)
{
    // Fast path for single char fragments.
    if (fragmentToSplit.start + 1 == fragmentToSplit.end) {
        if (keepAtLeastOneCharacter)
            return FlowContents::TextFragment();

        FlowContents::TextFragment fragmentForNextLine(fragmentToSplit);
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
        width = flowContents.textWidth(fragmentToSplit.start, middle + 1, 0);
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
    FlowContents::TextFragment fragmentForNextLine(fragmentToSplit);
    fragmentToSplit.end = right;
    fragmentToSplit.width = fragmentToSplit.isEmpty() ? 0 : flowContents.textWidth(fragmentToSplit.start, fragmentToSplit.end, 0);

    fragmentForNextLine.start = fragmentToSplit.end;
    fragmentForNextLine.width -= fragmentToSplit.width;
    return fragmentForNextLine;
}

static bool createLineRuns(LineState& lineState, Layout::RunVector& lineRuns, const FlowContents& flowContents)
{
    const auto& style = flowContents.style();
    bool lineCanBeWrapped = style.wrapLines || style.breakWordOnOverflow;
    while (!flowContents.isEnd(lineState.position)) {
        // Find the next text fragment. Start from the end of the previous fragment -current line end.
        FlowContents::TextFragment fragment = flowContents.nextTextFragment(lineState.position, lineState.width());
        if ((lineCanBeWrapped && !lineState.fits(fragment.width)) || fragment.type == FlowContents::TextFragment::LineBreak) {
            // Overflow wrapping behaviour:
            // 1. Newline character: wraps the line unless it's treated as whitespace.
            // 2. Whitesapce collapse on: whitespace is skipped.
            // 3. Whitespace collapse off: whitespace is wrapped.
            // 4. First, non-whitespace fragment is either wrapped or kept on the line. (depends on overflow-wrap)
            // 5. Non-whitespace fragment when there's already another fragment on the line gets pushed to the next line.
            bool isFirstFragment = !lineState.width();
            if (fragment.type == FlowContents::TextFragment::LineBreak) {
                if (isFirstFragment)
                    lineState.addUncommitted(fragment);
                else {
                    // No need to add the new line fragment if there's already content on the line. We are about to close this line anyway.
                    ++lineState.position;
                }
            } else if (style.collapseWhitespace && fragment.type == FlowContents::TextFragment::Whitespace) {
                // Whitespace collapse is on: whitespace that doesn't fit is simply skipped.
                lineState.position = fragment.end;
            } else if (fragment.type == FlowContents::TextFragment::Whitespace || ((isFirstFragment && style.breakWordOnOverflow) || !style.wrapLines)) { // !style.wrapLines: bug138102(preserve existing behavior)
                // Whitespace collapse is off or non-whitespace content. split the fragment; (modified)fragment -> this lineState, oveflowedFragment -> next line.
                // When this is the only (first) fragment, the first character stays on the line, even if it does not fit.
                lineState.oveflowedFragment = splitFragmentToFitLine(fragment, lineState.availableWidth - lineState.width(), isFirstFragment, flowContents);
                if (!fragment.isEmpty()) {
                    // Whitespace fragments can get pushed entirely to the next line.
                    lineState.addUncommitted(fragment);
                }
            } else if (isFirstFragment) {
                // Non-breakable non-whitespace first fragment. Add it to the current line. -it overflows though.
                lineState.addUncommitted(fragment);
            } else {
                // Non-breakable non-whitespace fragment when there's already a fragment on the line. Push it to the next line.
                lineState.oveflowedFragment = fragment;
            }
            break;
        }
        // When the current fragment is collapsed whitespace, we need to create a run for what we've processed so far.
        if (fragment.isCollapsed) {
            // One trailing whitespace to preserve.
            lineState.addUncommittedWhitespace(style.spaceWidth);
            lineState.commitAndCreateRun(lineRuns);
            // And skip the collapsed whitespace.
            lineState.jumpTo(fragment.end, lineState.width() + fragment.width - style.spaceWidth);
        } else
            lineState.addUncommitted(fragment);
    }
    lineState.commitAndCreateRun(lineRuns);
    return flowContents.isEnd(lineState.position) && lineState.oveflowedFragment.isEmpty();
}

static void closeLineEndingAndAdjustRuns(LineState& lineState, Layout::RunVector& lineRuns, unsigned& lineCount, const FlowContents& flowContents)
{
    if (lineState.lineStartRunIndex == lineRuns.size())
        return;

    ASSERT(lineRuns.size());
    removeTrailingWhitespace(lineState, lineRuns, flowContents);
    // Adjust runs' position by taking line's alignment into account.
    if (float lineLogicalLeft = computeLineLeft(flowContents.style().textAlign, lineState.availableWidth, lineState.committedWidth, lineState.logicalLeftOffset)) {
        for (unsigned i = lineState.lineStartRunIndex; i < lineRuns.size(); ++i) {
            lineRuns[i].logicalLeft += lineLogicalLeft;
            lineRuns[i].logicalRight += lineLogicalLeft;
        }
    }
    lineRuns.last().isEndOfLine = true;
    lineState.committedWidth = 0;
    lineState.committedLogicalRight = 0;
    ++lineCount;
}

static void splitRunsAtRendererBoundary(Layout::RunVector& lineRuns, const FlowContents& flowContents)
{
    // FIXME: We should probably split during run construction instead of as a separate pass.
    if (lineRuns.isEmpty())
        return;
    if (flowContents.hasOneSegment())
        return;

    unsigned runIndex = 0;
    do {
        const Run& run = lineRuns.at(runIndex);
        ASSERT(run.start != run.end);
        auto& startSegment = flowContents.segmentForPosition(run.start);
        if (run.end <= startSegment.end)
            continue;
        // This run overlaps multiple renderers. Split it up.
        // Split run at the renderer's boundary and create a new run for the left side, while use the current run as the right side.
        float logicalRightOfLeftRun = run.logicalLeft + flowContents.textWidth(run.start, startSegment.end, run.logicalLeft);
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
    LineState lineState;
    bool isEndOfContent = false;
    FlowContents flowContents = FlowContents(flow);

    do {
        flow.setLogicalHeight(lineHeight * lineCount + borderAndPaddingBefore);
        lineState = initializeNewLine(lineState, flow, flowContents, runs.size());
        isEndOfContent = createLineRuns(lineState, runs, flowContents);
        closeLineEndingAndAdjustRuns(lineState, runs, lineCount, flowContents);
    } while (!isEndOfContent);

    splitRunsAtRendererBoundary(runs, flowContents);
    ASSERT(!lineState.uncommittedWidth);
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
