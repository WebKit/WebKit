/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "InlineLineBuilder.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "InlineFormattingContext.h"
#include "InlineSoftLineBreakItem.h"
#include "RuntimeEnabledFeatures.h"
#include "TextUtil.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Layout {

static inline bool isWhitespacePreserved(const RenderStyle& style)
{
    auto whitespace = style.whiteSpace();
    return whitespace == WhiteSpace::Pre || whitespace == WhiteSpace::PreWrap || whitespace == WhiteSpace::BreakSpaces;
}

struct HangingContent {
public:
    void reset();

    InlineLayoutUnit width() const { return m_width; }
    bool isConditional() const { return m_isConditional; }

    void setIsConditional() { m_isConditional = true; }
    void expand(InlineLayoutUnit width) { m_width += width; }

private:
    bool m_isConditional { false };
    InlineLayoutUnit m_width { 0 };
};

void HangingContent::reset()
{
    m_isConditional = false;
    m_width =  0;
}

LineBuilder::LineBuilder(const InlineFormattingContext& inlineFormattingContext, Optional<TextAlignMode> horizontalAlignment, IntrinsicSizing intrinsicSizing)
    : m_inlineFormattingContext(inlineFormattingContext)
    , m_trimmableTrailingContent(m_runs)
    , m_horizontalAlignment(horizontalAlignment)
    , m_isIntrinsicSizing(intrinsicSizing == IntrinsicSizing::Yes)
    , m_shouldIgnoreTrailingLetterSpacing(RuntimeEnabledFeatures::sharedFeatures().layoutFormattingContextIntegrationEnabled())
{
}

LineBuilder::~LineBuilder()
{
}

void LineBuilder::initialize(const Constraints& constraints)
{
    ASSERT(m_isIntrinsicSizing || constraints.heightAndBaseline);

    InlineLayoutUnit initialLineHeight = 0;
    InlineLayoutUnit initialBaseline = 0;
    if (constraints.heightAndBaseline) {
        m_initialStrut = constraints.heightAndBaseline->strut;
        initialLineHeight = constraints.heightAndBaseline->height;
        initialBaseline = constraints.heightAndBaseline->baseline;
    } else
        m_initialStrut = { };

    auto lineRect = Display::InlineRect { constraints.logicalTopLeft, 0_lu, initialLineHeight };
    auto ascentAndDescent = AscentAndDescent { initialBaseline, initialLineHeight - initialBaseline };
    m_lineBox = LineBox { lineRect, ascentAndDescent, initialBaseline };
    m_lineLogicalWidth = constraints.availableLogicalWidth;
    m_hasIntrusiveFloat = constraints.lineIsConstrainedByFloat;

    resetContent();
}

void LineBuilder::resetContent()
{
    m_lineBox.setLogicalWidth({ });
    m_lineBox.setIsConsideredEmpty();
    m_runs.clear();
    m_trimmableTrailingContent.reset();
    m_lineIsVisuallyEmptyBeforeTrimmableTrailingContent = { };
}

LineBuilder::RunList LineBuilder::close(IsLastLineWithInlineContent isLastLineWithInlineContent)
{
    // 1. Remove trimmable trailing content.
    // 2. Align merged runs both vertically and horizontally.
    removeTrailingTrimmableContent();
    visuallyCollapsePreWrapOverflowContent();
    if (m_isIntrinsicSizing)
        return WTFMove(m_runs);

    auto hangingContent = collectHangingContent(isLastLineWithInlineContent);
    adjustBaselineAndLineHeight();
    if (isVisuallyEmpty()) {
        m_lineBox.resetBaseline();
        m_lineBox.setLogicalHeight({ });
    }
    // Remove descent when all content is baseline aligned but none of them have descent.
    if (formattingContext().quirks().lineDescentNeedsCollapsing(m_runs)) {
        m_lineBox.shrinkVertically(m_lineBox.ascentAndDescent().descent);
        m_lineBox.resetDescent();
    }
    alignContentVertically();
    alignHorizontally(hangingContent, isLastLineWithInlineContent);
    return WTFMove(m_runs);
}

void LineBuilder::alignContentVertically()
{
    ASSERT(!m_isIntrinsicSizing);
    auto scrollableOverflowRect = m_lineBox.logicalRect();
    for (auto& run : m_runs) {
        InlineLayoutUnit logicalTop = 0;
        auto& layoutBox = run.layoutBox();
        auto verticalAlign = layoutBox.style().verticalAlign();
        auto ascent = layoutBox.style().fontMetrics().ascent();

        switch (verticalAlign) {
        case VerticalAlign::Baseline:
            if (run.isLineBreak() || run.isText())
                logicalTop = baseline() - ascent;
            else if (run.isContainerStart()) {
                auto& boxGeometry = formattingContext().geometryForBox(layoutBox);
                logicalTop = baseline() - ascent - boxGeometry.borderTop() - boxGeometry.paddingTop().valueOr(0);
            } else if (layoutBox.isInlineBlockBox() && layoutBox.establishesInlineFormattingContext()) {
                auto& formattingState = layoutState().establishedInlineFormattingState(downcast<ContainerBox>(layoutBox));
                // Spec makes us generate at least one line -even if it is empty.
                auto inlineBlockBaseline = formattingState.displayInlineContent()->lineBoxes.last().baseline();
                // The inline-block's baseline offset is relative to its content box. Let's convert it relative to the margin box.
                //           _______________ <- margin box
                //          |
                //          |  ____________  <- border box
                //          | |
                //          | |  _________  <- content box
                //          | | |   ^
                //          | | |   |  <- baseline offset
                //          | | |   |
                //     text | | |   v text
                //     -----|-|-|---------- <- baseline
                //
                auto& boxGeometry = formattingContext().geometryForBox(layoutBox);
                auto baselineFromMarginBox = boxGeometry.marginBefore() + boxGeometry.borderTop() + boxGeometry.paddingTop().valueOr(0) + inlineBlockBaseline;
                logicalTop = baseline() - baselineFromMarginBox;
            } else {
                auto& boxGeometry = formattingContext().geometryForBox(layoutBox);
                logicalTop = baseline() - (boxGeometry.verticalBorder() + boxGeometry.verticalPadding().valueOr(0_lu) + run.logicalRect().height() + boxGeometry.marginAfter());
            }
            break;
        case VerticalAlign::Top:
            logicalTop = 0_lu;
            break;
        case VerticalAlign::Bottom:
            logicalTop = logicalBottom() - run.logicalRect().height();
            break;
        default:
            ASSERT_NOT_IMPLEMENTED_YET();
            break;
        }
        run.adjustLogicalTop(logicalTop);
        // Adjust scrollable overflow if the run overflows the line.
        scrollableOverflowRect.expandVerticallyToContain(run.logicalRect());
        // Convert runs from relative to the line top/left to the formatting root's border box top/left.
        run.moveVertically(this->logicalTop());
        run.moveHorizontally(this->logicalLeft());
    }
    m_lineBox.setScrollableOverflow(scrollableOverflowRect);
}

void LineBuilder::justifyRuns(InlineLayoutUnit availableWidth)
{
    ASSERT(availableWidth > 0);
    // Collect the expansion opportunity numbers and find the last run with content.
    auto expansionOpportunityCount = 0;
    Run* lastRunWithContent = nullptr;
    for (auto& run : m_runs) {
        expansionOpportunityCount += run.expansionOpportunityCount();
        if (run.isText() || run.isBox())
            lastRunWithContent = &run;
    }
    // Need to fix up the last run's trailing expansion.
    if (lastRunWithContent && lastRunWithContent->hasExpansionOpportunity()) {
        // Turn off the trailing bits first and add the forbid trailing expansion.
        auto leftExpansion = lastRunWithContent->expansionBehavior() & LeftExpansionMask;
        lastRunWithContent->setExpansionBehavior(leftExpansion | ForbidRightExpansion);
    }
    // Nothing to distribute?
    if (!expansionOpportunityCount)
        return;
    // Distribute the extra space.
    auto expansionToDistribute = availableWidth / expansionOpportunityCount;
    InlineLayoutUnit accumulatedExpansion = 0;
    for (auto& run : m_runs) {
        // Expand and moves runs by the accumulated expansion.
        if (!run.hasExpansionOpportunity()) {
            run.moveHorizontally(accumulatedExpansion);
            continue;
        }
        ASSERT(run.expansionOpportunityCount());
        auto computedExpansion = expansionToDistribute * run.expansionOpportunityCount();
        run.setComputedHorizontalExpansion(computedExpansion);
        run.moveHorizontally(accumulatedExpansion);
        accumulatedExpansion += computedExpansion;
    }
}

void LineBuilder::alignHorizontally(const HangingContent& hangingContent, IsLastLineWithInlineContent isLastLine)
{
    ASSERT(!m_isIntrinsicSizing);

    auto availableWidth = this->availableWidth() + hangingContent.width();
    if (m_runs.isEmpty() || availableWidth <= 0)
        return;

    auto computedHorizontalAlignment = [&] {
        ASSERT(m_horizontalAlignment);
        if (m_horizontalAlignment != TextAlignMode::Justify)
            return *m_horizontalAlignment;
        // Text is justified according to the method specified by the text-justify property,
        // in order to exactly fill the line box. Unless otherwise specified by text-align-last,
        // the last line before a forced break or the end of the block is start-aligned.
        if (m_runs.last().isLineBreak() || isLastLine == IsLastLineWithInlineContent::Yes)
            return TextAlignMode::Start;
        return TextAlignMode::Justify;
    }();

    if (computedHorizontalAlignment == TextAlignMode::Justify) {
        justifyRuns(availableWidth);
        return;
    }

    auto adjustmentForAlignment = [] (auto horizontalAlignment, auto availableWidth) -> Optional<InlineLayoutUnit> {
        switch (horizontalAlignment) {
        case TextAlignMode::Left:
        case TextAlignMode::WebKitLeft:
        case TextAlignMode::Start:
            return { };
        case TextAlignMode::Right:
        case TextAlignMode::WebKitRight:
        case TextAlignMode::End:
            return std::max<InlineLayoutUnit>(availableWidth, 0);
        case TextAlignMode::Center:
        case TextAlignMode::WebKitCenter:
            return std::max<InlineLayoutUnit>(availableWidth / 2, 0);
        case TextAlignMode::Justify:
            ASSERT_NOT_REACHED();
            break;
        }
        ASSERT_NOT_REACHED();
        return { };
    };

    auto adjustment = adjustmentForAlignment(computedHorizontalAlignment, availableWidth);
    if (!adjustment)
        return;
    // Horizontal alignment means that we not only adjust the runs but also make sure
    // that the line box is aligned as well
    // e.g. <div style="text-align: center; width: 100px;">centered text</div> : the line box will also be centered
    // as opposed to start at 0px all the way to [centered text] run's right edge.
    m_lineBox.moveHorizontally(*adjustment);
    for (auto& run : m_runs)
        run.moveHorizontally(*adjustment);
}

void LineBuilder::removeTrailingTrimmableContent()
{
    if (m_trimmableTrailingContent.isEmpty() || m_runs.isEmpty())
        return;

    // Complex line layout quirk: keep the trailing whitespace around when it is followed by a line break, unless the content overflows the line.
    if (RuntimeEnabledFeatures::sharedFeatures().layoutFormattingContextIntegrationEnabled()) {
        auto isTextAlignRight = [&] {
            ASSERT(m_horizontalAlignment);
            return m_horizontalAlignment == TextAlignMode::Right
                || m_horizontalAlignment == TextAlignMode::WebKitRight
                || m_horizontalAlignment == TextAlignMode::End;
            }();

        if (m_runs.last().isLineBreak() && availableWidth() >= 0 && !isTextAlignRight) {
            m_trimmableTrailingContent.reset();
            return;
        }
    }

    m_lineBox.shrinkHorizontally(m_trimmableTrailingContent.remove());
    // If we removed the first visible run on the line, we need to re-check the visibility status.
    if (m_lineIsVisuallyEmptyBeforeTrimmableTrailingContent) {
        // Just because the line was visually empty before the removed content, it does not necessarily mean it is still visually empty.
        // <span>  </span><span style="padding-left: 10px"></span>  <- non-empty
        auto lineIsVisuallyEmpty = [&] {
            for (auto& run : m_runs) {
                if (isVisuallyNonEmpty(run))
                    return false;
            }
            return true;
        };
        // We could only go from visually non empty -> to visually empty. Trimmed runs should never make the line visible.
        if (lineIsVisuallyEmpty())
            m_lineBox.setIsConsideredEmpty();
        m_lineIsVisuallyEmptyBeforeTrimmableTrailingContent = { };
    }
}

void LineBuilder::visuallyCollapsePreWrapOverflowContent()
{
    ASSERT(m_trimmableTrailingContent.isEmpty());
    // If white-space is set to pre-wrap, the UA must
    // ...
    // It may also visually collapse the character advance widths of any that would otherwise overflow.
    auto overflowWidth = -availableWidth();
    if (overflowWidth <= 0)
        return;
    // Let's just find the trailing pre-wrap whitespace content for now (e.g check if there are multiple trailing runs with
    // different set of white-space values and decide if the in-between pre-wrap content should be collapsed as well.)
    InlineLayoutUnit trimmedContentWidth = 0;
    for (auto& run : WTF::makeReversedRange(m_runs)) {
        if (run.style().whiteSpace() != WhiteSpace::PreWrap) {
            // We are only interested in pre-wrap trailing content.
            break;
        }
        auto preWrapVisuallyCollapsibleInlineItem = run.isContainerStart() || run.isContainerEnd() || run.hasTrailingWhitespace();
        if (!preWrapVisuallyCollapsibleInlineItem)
            break;
        ASSERT(!run.hasCollapsibleTrailingWhitespace());
        InlineLayoutUnit trimmableWidth = { };
        if (run.isText()) {
            // FIXME: We should always collapse the run at a glyph boundary as the spec indicates: "collapse the character advance widths of any that would otherwise overflow"
            // and the trimmed width should be capped at std::min(run.trailingWhitespaceWidth(), overflowWidth) for texgt runs. Both FF and Chrome agree.
            trimmableWidth = run.trailingWhitespaceWidth();
            run.visuallyCollapseTrailingWhitespace();
        } else {
            trimmableWidth = run.logicalWidth();
            run.shrinkHorizontally(trimmableWidth);
        }
        trimmedContentWidth += trimmableWidth;
        overflowWidth -= trimmableWidth;
        if (overflowWidth <= 0)
            break;
    }
    m_lineBox.shrinkHorizontally(trimmedContentWidth);
}

HangingContent LineBuilder::collectHangingContent(IsLastLineWithInlineContent isLastLineWithInlineContent)
{
    auto hangingContent = HangingContent { };
    // Can't setup hanging content with removable trailing whitespace.
    ASSERT(m_trimmableTrailingContent.isEmpty());
    if (isLastLineWithInlineContent == IsLastLineWithInlineContent::Yes)
        hangingContent.setIsConditional();
    for (auto& run : WTF::makeReversedRange(m_runs)) {
        if (run.isContainerStart() || run.isContainerEnd())
            continue;
        if (run.isLineBreak()) {
            hangingContent.setIsConditional();
            continue;
        }
        if (!run.hasTrailingWhitespace())
            break;
        // Check if we have a preserved or hung whitespace.
        if (run.style().whiteSpace() != WhiteSpace::PreWrap)
            break;
        // This is either a normal or conditionally hanging trailing whitespace.
        hangingContent.expand(run.trailingWhitespaceWidth());
    }
    return hangingContent;
}

void LineBuilder::moveLogicalLeft(InlineLayoutUnit delta)
{
    if (!delta)
        return;
    ASSERT(delta > 0);
    m_lineBox.moveHorizontally(delta);
    m_lineLogicalWidth -= delta;
}

void LineBuilder::moveLogicalRight(InlineLayoutUnit delta)
{
    ASSERT(delta > 0);
    m_lineLogicalWidth -= delta;
}

void LineBuilder::append(const InlineItem& inlineItem, InlineLayoutUnit logicalWidth)
{
    appendWith(inlineItem, { logicalWidth, false });
}

void LineBuilder::appendPartialTrailingTextItem(const InlineTextItem& inlineTextItem, InlineLayoutUnit logicalWidth, bool needsHyphen)
{
    appendWith(inlineTextItem, { logicalWidth, needsHyphen });
}

void LineBuilder::appendWith(const InlineItem& inlineItem, const InlineRunDetails& inlineRunDetails)
{
    if (inlineItem.isText())
        appendTextContent(downcast<InlineTextItem>(inlineItem), inlineRunDetails.logicalWidth, inlineRunDetails.needsHyphen);
    else if (inlineItem.isLineBreak())
        appendLineBreak(inlineItem);
    else if (inlineItem.isContainerStart())
        appendInlineContainerStart(inlineItem, inlineRunDetails.logicalWidth);
    else if (inlineItem.isContainerEnd())
        appendInlineContainerEnd(inlineItem, inlineRunDetails.logicalWidth);
    else if (inlineItem.layoutBox().isReplacedBox())
        appendReplacedInlineBox(inlineItem, inlineRunDetails.logicalWidth);
    else if (inlineItem.isBox())
        appendNonReplacedInlineBox(inlineItem, inlineRunDetails.logicalWidth);
    else
        ASSERT_NOT_REACHED();

    // Check if this freshly appended content makes the line visually non-empty.
    if (m_lineBox.isConsideredEmpty() && !m_runs.isEmpty() && isVisuallyNonEmpty(m_runs.last()))
        m_lineBox.setIsConsideredNonEmpty();
}

void LineBuilder::appendNonBreakableSpace(const InlineItem& inlineItem, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth)
{
    m_runs.append({ inlineItem, logicalLeft, logicalWidth });
    m_lineBox.expandHorizontally(logicalWidth);
}

void LineBuilder::appendInlineContainerStart(const InlineItem& inlineItem, InlineLayoutUnit logicalWidth)
{
    // This is really just a placeholder to mark the start of the inline level container <span>.
    appendNonBreakableSpace(inlineItem, contentLogicalWidth(), logicalWidth);
}

void LineBuilder::appendInlineContainerEnd(const InlineItem& inlineItem, InlineLayoutUnit logicalWidth)
{
    // This is really just a placeholder to mark the end of the inline level container </span>.
    auto removeTrailingLetterSpacing = [&] {
        if (!m_trimmableTrailingContent.isTrailingRunPartiallyTrimmable())
            return;
        m_lineBox.shrinkHorizontally(m_trimmableTrailingContent.removePartiallyTrimmableContent());
    };
    // Prevent trailing letter-spacing from spilling out of the inline container.
    // https://drafts.csswg.org/css-text-3/#letter-spacing-property See example 21.
    removeTrailingLetterSpacing();
    appendNonBreakableSpace(inlineItem, contentLogicalRight(), logicalWidth);
}

void LineBuilder::appendTextContent(const InlineTextItem& inlineTextItem, InlineLayoutUnit logicalWidth, bool needsHyphen)
{
    auto willCollapseCompletely = [&] {
        if (!inlineTextItem.isCollapsible())
            return false;
        if (inlineTextItem.isEmptyContent())
            return true;
        // Check if the last item is collapsed as well.
        for (auto& run : WTF::makeReversedRange(m_runs)) {
            if (run.isBox())
                return false;
            // https://drafts.csswg.org/css-text-3/#white-space-phase-1
            // Any collapsible space immediately following another collapsible space—even one outside the boundary of the inline containing that space,
            // provided both spaces are within the same inline formatting context—is collapsed to have zero advance width.
            // : "<span>  </span> " <- the trailing whitespace collapses completely.
            // Not that when the inline container has preserve whitespace style, "<span style="white-space: pre">  </span> " <- this whitespace stays around.
            if (run.isText())
                return run.hasCollapsibleTrailingWhitespace();
            ASSERT(run.isContainerStart() || run.isContainerEnd());
        }
        // Leading whitespace.
        return !isWhitespacePreserved(inlineTextItem.style());
    };

    if (willCollapseCompletely())
        return;

    auto inlineTextItemNeedsNewRun = true;
    if (!m_runs.isEmpty()) {
        auto& lastRun = m_runs.last();
        inlineTextItemNeedsNewRun = lastRun.hasCollapsedTrailingWhitespace() || !lastRun.isText() || &lastRun.layoutBox() != &inlineTextItem.layoutBox();
        if (!inlineTextItemNeedsNewRun) {
            lastRun.expand(inlineTextItem, logicalWidth);
            if (needsHyphen) {
                ASSERT(!lastRun.textContent()->needsHyphen());
                lastRun.setNeedsHyphen();
            }
        }
    }
    if (inlineTextItemNeedsNewRun)
        m_runs.append({ inlineTextItem, contentLogicalWidth(), logicalWidth, needsHyphen });

    m_lineBox.expandHorizontally(logicalWidth);

    // Set the trailing trimmable content.
    if (inlineTextItem.isWhitespace() && !TextUtil::shouldPreserveTrailingWhitespace(inlineTextItem.style())) {
        m_trimmableTrailingContent.addFullyTrimmableContent(m_runs.size() - 1, logicalWidth);
        // If we ever trim this content, we need to know if the line visibility state needs to be recomputed.
        if (m_trimmableTrailingContent.isEmpty())
            m_lineIsVisuallyEmptyBeforeTrimmableTrailingContent = isVisuallyEmpty();
        return;
    }
    // Any non-whitespace, no-trimmable content resets the existing trimmable.
    m_trimmableTrailingContent.reset();
    if (!m_shouldIgnoreTrailingLetterSpacing && !inlineTextItem.isWhitespace() && inlineTextItem.style().letterSpacing() > 0)
        m_trimmableTrailingContent.addPartiallyTrimmableContent(m_runs.size() - 1, logicalWidth);
}

void LineBuilder::appendNonReplacedInlineBox(const InlineItem& inlineItem, InlineLayoutUnit logicalWidth)
{
    auto& layoutBox = inlineItem.layoutBox();
    auto& boxGeometry = formattingContext().geometryForBox(layoutBox);
    auto horizontalMargin = boxGeometry.horizontalMargin();
    m_runs.append({ inlineItem, contentLogicalWidth() + horizontalMargin.start, logicalWidth });
    m_lineBox.expandHorizontally(logicalWidth + horizontalMargin.start + horizontalMargin.end);
    m_trimmableTrailingContent.reset();
}

void LineBuilder::appendReplacedInlineBox(const InlineItem& inlineItem, InlineLayoutUnit logicalWidth)
{
    ASSERT(inlineItem.layoutBox().isReplacedBox());
    // FIXME: Surely replaced boxes behave differently.
    appendNonReplacedInlineBox(inlineItem, logicalWidth);
}

void LineBuilder::appendLineBreak(const InlineItem& inlineItem)
{
    if (inlineItem.isHardLineBreak())
        return m_runs.append({ inlineItem, contentLogicalWidth(), 0_lu });
    // Soft line breaks (preserved new line characters) require inline text boxes for compatibility reasons.
    ASSERT(inlineItem.isSoftLineBreak());
    m_runs.append({ downcast<InlineSoftLineBreakItem>(inlineItem), contentLogicalWidth() });
}

void LineBuilder::adjustBaselineAndLineHeight()
{
    unsigned inlineContainerNestingLevel = 0;
    auto hasSeenDirectTextOrLineBreak = false;
    for (auto& run : m_runs) {
        auto& layoutBox = run.layoutBox();
        auto& style = layoutBox.style();

        run.setLogicalHeight(runContentHeight(run));

        if (run.isText() || run.isLineBreak()) {
            // For text content we set the baseline either through the initial strut (set by the formatting context root) or
            // through the inline container (start). Normally the text content itself does not stretch the line.
            if (!m_initialStrut) {
                // We are in standards mode where the baseline and line height are explict.
                continue;
            }
            if (inlineContainerNestingLevel) {
                // We've already adjusted the line height/baseline through the parent inline container. 
                continue;
            }
            if (hasSeenDirectTextOrLineBreak) {
                // e.g div>first text</div> or <div><span>nested<span>first direct text</div>.
                continue;
            }
            hasSeenDirectTextOrLineBreak = true;
            // We are in quirks mode where the font-metrics might change the line line height/baseline and this is the first text content on the line
            // outside of an inline container.
            m_lineBox.setAscentIfGreater(m_initialStrut->ascent);
            m_lineBox.setDescentIfGreater(m_initialStrut->descent);
            m_lineBox.setLogicalHeightIfGreater(m_initialStrut->height());
            continue;
        }

        if (run.isContainerStart()) {
            ++inlineContainerNestingLevel;
            // Inline containers stretch the line by their font size.
            // Vertical margins, padding and borders don't contribute to the line height.
            auto& fontMetrics = style.fontMetrics();
            if (style.verticalAlign() == VerticalAlign::Baseline) {
                auto halfLeading = halfLeadingMetrics(fontMetrics, style.computedLineHeight());
                // Both halfleading ascent and descent could be negative (tall font vs. small line-height value)
                if (halfLeading.descent > 0)
                    m_lineBox.setDescentIfGreater(halfLeading.descent);
                if (halfLeading.ascent > 0)
                    m_lineBox.setAscentIfGreater(halfLeading.ascent);
                m_lineBox.setLogicalHeightIfGreater(m_lineBox.ascentAndDescent().height());
            } else
                m_lineBox.setLogicalHeightIfGreater(fontMetrics.height());
            continue;
        }

        if (run.isContainerEnd()) {
            // The line's baseline and height have already been adjusted at ContainerStart.
            ASSERT(inlineContainerNestingLevel);
            --inlineContainerNestingLevel;
            continue;
        }

        if (run.isBox()) {
            auto& boxGeometry = formattingContext().geometryForBox(layoutBox);
            auto marginBoxHeight = boxGeometry.marginBoxHeight();

            switch (style.verticalAlign()) {
            case VerticalAlign::Baseline: {
                if (layoutBox.isInlineBlockBox() && layoutBox.establishesInlineFormattingContext()) {
                    // Inline-blocks with inline content always have baselines.
                    auto& formattingState = layoutState().establishedInlineFormattingState(downcast<ContainerBox>(layoutBox));
                    // There has to be at least one line -even if it is empty.
                    auto& lastLineBox = formattingState.displayInlineContent()->lineBoxes.last();
                    auto beforeHeight = boxGeometry.marginBefore() + boxGeometry.borderTop() + boxGeometry.paddingTop().valueOr(0);
                    m_lineBox.setBaselineIfGreater(beforeHeight + lastLineBox.baseline());
                    m_lineBox.setLogicalHeightIfGreater(marginBoxHeight);
                } else {
                    // Non inline-block boxes sit on the baseline (including their bottom margin).
                    m_lineBox.setAscentIfGreater(marginBoxHeight);
                    // Ignore negative descent (yes, negative descent is a thing).
                    m_lineBox.setLogicalHeightIfGreater(marginBoxHeight + std::max<InlineLayoutUnit>(0, m_lineBox.ascentAndDescent().descent));
                }
                break;
            }
            case VerticalAlign::Top:
                // Top align content never changes the baseline, it only pushes the bottom of the line further down.
                m_lineBox.setLogicalHeightIfGreater(marginBoxHeight);
                break;
            case VerticalAlign::Bottom: {
                // Bottom aligned, tall content pushes the baseline further down from the line top.
                auto lineLogicalHeight = m_lineBox.logicalHeight();
                if (marginBoxHeight > lineLogicalHeight) {
                    m_lineBox.setLogicalHeightIfGreater(marginBoxHeight);
                    m_lineBox.setBaselineIfGreater(m_lineBox.baseline() + (marginBoxHeight - lineLogicalHeight));
                }
                break;
            }
            default:
                ASSERT_NOT_IMPLEMENTED_YET();
                break;
            }
            continue;
        }
    }
}

InlineLayoutUnit LineBuilder::runContentHeight(const Run& run) const
{
    ASSERT(!m_isIntrinsicSizing);
    auto& fontMetrics = run.style().fontMetrics();
    if (run.isText() || run.isLineBreak())
        return fontMetrics.height();

    if (run.isContainerStart() || run.isContainerEnd())
        return fontMetrics.height();

    auto& layoutBox = run.layoutBox();
    auto& boxGeometry = formattingContext().geometryForBox(layoutBox);
    if (layoutBox.isReplacedBox() || layoutBox.isFloatingPositioned())
        return boxGeometry.contentBoxHeight();

    // Non-replaced inline box (e.g. inline-block). It looks a bit misleading but their margin box is considered the content height here.
    return boxGeometry.marginBoxHeight();
}

bool LineBuilder::isVisuallyNonEmpty(const Run& run) const
{
    if (run.isText())
        return true;

    if (run.isLineBreak())
        return true;

    // Note that this does not check whether the inline container has content. It simply checks if the container itself is considered non-empty.
    if (run.isContainerStart() || run.isContainerEnd()) {
        if (!run.logicalWidth())
            return false;
        // Margin does not make the container visually non-empty. Check if it has border or padding.
        auto& boxGeometry = formattingContext().geometryForBox(run.layoutBox());
        if (run.isContainerStart())
            return boxGeometry.borderLeft() || (boxGeometry.paddingLeft() && boxGeometry.paddingLeft().value());
        return boxGeometry.borderRight() || (boxGeometry.paddingRight() && boxGeometry.paddingRight().value());
    }

    if (run.isBox()) {
        if (run.layoutBox().isReplacedBox())
            return true;
        ASSERT(run.layoutBox().isInlineBlockBox() || run.layoutBox().isInlineTableBox());
        if (!run.logicalWidth())
            return false;
        if (m_isIntrinsicSizing || formattingContext().geometryForBox(run.layoutBox()).height())
            return true;
        return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}

AscentAndDescent LineBuilder::halfLeadingMetrics(const FontMetrics& fontMetrics, InlineLayoutUnit lineLogicalHeight)
{
    auto ascent = fontMetrics.ascent();
    auto descent = fontMetrics.descent();
    // 10.8.1 Leading and half-leading
    auto halfLeading = (lineLogicalHeight - (ascent + descent)) / 2;
    // Inline tree height is all integer based.
    auto adjustedAscent = std::max<InlineLayoutUnit>(floorf(ascent + halfLeading), 0);
    auto adjustedDescent = std::max<InlineLayoutUnit>(ceilf(descent + halfLeading), 0);
    return { adjustedAscent, adjustedDescent };
}

LayoutState& LineBuilder::layoutState() const
{ 
    return formattingContext().layoutState();
}

const InlineFormattingContext& LineBuilder::formattingContext() const
{
    return m_inlineFormattingContext;
}

LineBuilder::TrimmableTrailingContent::TrimmableTrailingContent(RunList& runs)
    : m_runs(runs)
{
}

void LineBuilder::TrimmableTrailingContent::addFullyTrimmableContent(size_t runIndex, InlineLayoutUnit trimmableWidth)
{
    // Any subsequent trimmable whitespace should collapse to zero advanced width and ignored at ::appendTextContent().
    ASSERT(!m_hasFullyTrimmableContent);
    m_fullyTrimmableWidth = trimmableWidth;
    // Note that just becasue the trimmable width is 0 (font-size: 0px), it does not mean we don't have a trimmable trailing content.
    m_hasFullyTrimmableContent = true;
    m_firstRunIndex = m_firstRunIndex.valueOr(runIndex);
}

void LineBuilder::TrimmableTrailingContent::addPartiallyTrimmableContent(size_t runIndex, InlineLayoutUnit trimmableWidth)
{
    // Do not add trimmable letter spacing after a fully trimmable whitesapce.
    ASSERT(!m_firstRunIndex);
    ASSERT(!m_hasFullyTrimmableContent);
    ASSERT(!m_partiallyTrimmableWidth);
    ASSERT(trimmableWidth);
    m_partiallyTrimmableWidth = trimmableWidth;
    m_firstRunIndex = runIndex;
}

InlineLayoutUnit LineBuilder::TrimmableTrailingContent::remove()
{
    // Remove trimmable trailing content and move all the subsequent trailing runs.
    // <span> </span><span></span>
    // [trailing whitespace][container end][container start][container end]
    // Trim the whitespace run and move the trailing inline container runs to the logical left.
    ASSERT(!isEmpty());
    auto& trimmableRun = m_runs[*m_firstRunIndex];
    ASSERT(trimmableRun.isText());

    if (m_hasFullyTrimmableContent)
        trimmableRun.removeTrailingWhitespace();
    if (m_partiallyTrimmableWidth)
        trimmableRun.removeTrailingLetterSpacing();

    auto trimmableWidth = width();
    for (auto index = *m_firstRunIndex + 1; index < m_runs.size(); ++index) {
        auto& run = m_runs[index];
        ASSERT(run.isContainerStart() || run.isContainerEnd() || run.isLineBreak());
        run.moveHorizontally(-trimmableWidth);
    }
    reset();
    return trimmableWidth;
}

InlineLayoutUnit LineBuilder::TrimmableTrailingContent::removePartiallyTrimmableContent()
{
    // Partially trimmable content is always gated by a fully trimmable content.
    // We can't just trim spacing in the middle.
    ASSERT(!m_fullyTrimmableWidth);
    return remove();
}

LineBuilder::Run::Run(const InlineItem& inlineItem, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth)
    : m_type(inlineItem.type())
    , m_layoutBox(&inlineItem.layoutBox())
    , m_logicalRect({ 0, logicalLeft, logicalWidth, 0 })
{
}

LineBuilder::Run::Run(const InlineSoftLineBreakItem& softLineBreakItem, InlineLayoutUnit logicalLeft)
    : m_type(softLineBreakItem.type())
    , m_layoutBox(&softLineBreakItem.layoutBox())
    , m_logicalRect({ 0, logicalLeft, 0, 0 })
    , m_textContent({ softLineBreakItem.position(), 1, softLineBreakItem.inlineTextBox().content(), false })
{
}

LineBuilder::Run::Run(const InlineTextItem& inlineTextItem, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth, bool needsHyphen)
    : m_type(InlineItem::Type::Text)
    , m_layoutBox(&inlineTextItem.layoutBox())
    , m_logicalRect({ 0, logicalLeft, logicalWidth, 0 })
    , m_trailingWhitespaceType(trailingWhitespaceType(inlineTextItem))
    , m_textContent({ inlineTextItem.start(), m_trailingWhitespaceType == TrailingWhitespace::Collapsed ? 1 : inlineTextItem.length(), inlineTextItem.inlineTextBox().content(), needsHyphen })
{
    if (m_trailingWhitespaceType != TrailingWhitespace::None) {
        m_trailingWhitespaceWidth = logicalWidth;
        if (!isWhitespacePreserved(inlineTextItem.style()))
            m_expansionOpportunityCount = 1;
    }
}

void LineBuilder::Run::expand(const InlineTextItem& inlineTextItem, InlineLayoutUnit logicalWidth)
{
    // FIXME: This is a very simple expansion merge. We should eventually switch over to FontCascade::expansionOpportunityCount.
    ASSERT(!hasCollapsedTrailingWhitespace());
    ASSERT(isText() && inlineTextItem.isText());
    ASSERT(m_layoutBox == &inlineTextItem.layoutBox());

    m_logicalRect.expandHorizontally(logicalWidth);
    m_trailingWhitespaceType = trailingWhitespaceType(inlineTextItem);

    if (m_trailingWhitespaceType == TrailingWhitespace::None) {
        m_trailingWhitespaceWidth = { };
        setExpansionBehavior(AllowLeftExpansion | AllowRightExpansion);
        m_textContent->expand(inlineTextItem.length());
        return;
    }
    m_trailingWhitespaceWidth += logicalWidth;
    if (!isWhitespacePreserved(inlineTextItem.style()))
        ++m_expansionOpportunityCount;
    setExpansionBehavior(DefaultExpansion);
    m_textContent->expand(m_trailingWhitespaceType == TrailingWhitespace::Collapsed ? 1 : inlineTextItem.length());
}

bool LineBuilder::Run::hasTrailingLetterSpacing() const
{
    // Complex line layout does not keep track of trailing letter spacing.
    if (RuntimeEnabledFeatures::sharedFeatures().layoutFormattingContextIntegrationEnabled())
        return false;
    return !hasTrailingWhitespace() && style().letterSpacing() > 0;
}

InlineLayoutUnit LineBuilder::Run::trailingLetterSpacing() const
{
    if (!hasTrailingLetterSpacing())
        return 0_lu;
    return InlineLayoutUnit { style().letterSpacing() };
}

void LineBuilder::Run::removeTrailingLetterSpacing()
{
    ASSERT(hasTrailingLetterSpacing());
    shrinkHorizontally(trailingLetterSpacing());
    ASSERT(logicalWidth() > 0 || (!logicalWidth() && style().letterSpacing() >= intMaxForLayoutUnit));
}

void LineBuilder::Run::removeTrailingWhitespace()
{
    // According to https://www.w3.org/TR/css-text-3/#white-space-property matrix
    // Trimmable whitespace is always collapsable so the length of the trailing trimmable whitespace is always 1 (or non-existent).
    ASSERT(m_textContent->length());
    constexpr size_t trailingTrimmableContentLength = 1;
    m_textContent->shrink(trailingTrimmableContentLength);
    visuallyCollapseTrailingWhitespace();
}

void LineBuilder::Run::visuallyCollapseTrailingWhitespace()
{
    // This is just a visual adjustment, the text length should remain the same.
    shrinkHorizontally(m_trailingWhitespaceWidth);
    m_trailingWhitespaceWidth = { };
    m_trailingWhitespaceType = TrailingWhitespace::None;

    if (!isWhitespacePreserved(style())) {
        ASSERT(m_expansionOpportunityCount);
        m_expansionOpportunityCount--;
    }
    setExpansionBehavior(AllowLeftExpansion | AllowRightExpansion);
}

void LineBuilder::Run::setExpansionBehavior(ExpansionBehavior expansionBehavior)
{
    ASSERT(isText());
    m_expansion.behavior = expansionBehavior;
}

inline ExpansionBehavior LineBuilder::Run::expansionBehavior() const
{
    ASSERT(isText());
    return m_expansion.behavior;
}

void LineBuilder::Run::setComputedHorizontalExpansion(InlineLayoutUnit logicalExpansion)
{
    ASSERT(isText());
    ASSERT(hasExpansionOpportunity());
    m_logicalRect.expandHorizontally(logicalExpansion);
    m_expansion.horizontalExpansion = logicalExpansion;
}

}
}

#endif
