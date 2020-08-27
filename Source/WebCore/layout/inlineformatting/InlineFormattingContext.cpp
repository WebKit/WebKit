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
#include "InlineFormattingContext.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "FloatingContext.h"
#include "FontCascade.h"
#include "InlineFormattingState.h"
#include "InlineTextItem.h"
#include "InvalidationState.h"
#include "LayoutBox.h"
#include "LayoutContainerBox.h"
#include "LayoutContext.h"
#include "LayoutInitialContainingBlock.h"
#include "LayoutInlineTextBox.h"
#include "LayoutReplacedBox.h"
#include "LayoutState.h"
#include "Logging.h"
#include "RuntimeEnabledFeatures.h"
#include "TextUtil.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(InlineFormattingContext);

InlineFormattingContext::InlineFormattingContext(const ContainerBox& formattingContextRoot, InlineFormattingState& formattingState)
    : FormattingContext(formattingContextRoot, formattingState)
{
}

static inline const Box* nextInlineLevelBoxToLayout(const Box& layoutBox, const ContainerBox& stayWithin)
{
    // Atomic inline-level boxes and floats are opaque boxes meaning that they are
    // responsible for their own content (do not need to descend into their subtrees).
    // Only inline boxes may have relevant descendant content.
    if (layoutBox.isInlineBox()) {
        if (is<ContainerBox>(layoutBox) && downcast<ContainerBox>(layoutBox).hasInFlowOrFloatingChild()) {
            // Anonymous inline text boxes/line breaks can't have descendant content by definition.
            ASSERT(!layoutBox.isInlineTextBox() && !layoutBox.isLineBreakBox());
            return downcast<ContainerBox>(layoutBox).firstInFlowOrFloatingChild();
        }
    }

    for (auto* nextInPreOrder = &layoutBox; nextInPreOrder && nextInPreOrder != &stayWithin; nextInPreOrder = &nextInPreOrder->parent()) {
        if (auto* nextSibling = nextInPreOrder->nextInFlowOrFloatingSibling())
            return nextSibling;
    }
    return nullptr;
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

class LineContentAligner {
public:
    LineContentAligner(const InlineFormattingContext&, LineBox&, LineBuilder::RunList&, InlineLayoutUnit availableWidth);

    enum class IsLastLineWithInlineContent { No, Yes };
    void alignHorizontally(TextAlignMode, IsLastLineWithInlineContent);
    void alignVertically();
    void adjustBaselineAndLineHeight();

private:
    void justifyRuns(InlineLayoutUnit availableWidth);
    HangingContent collectHangingContent(IsLastLineWithInlineContent) const;
    InlineLayoutUnit runContentHeight(const LineBuilder::Run&) const;

    const InlineFormattingContext& formattingContext() const { return m_inlineFormattingContext; }
    LayoutState& layoutState() const { return formattingContext().layoutState(); }

    const InlineFormattingContext& m_inlineFormattingContext;
    LineBox& m_lineBox;
    LineBuilder::RunList& m_runs;
    InlineLayoutUnit m_availableWidth;
};

LineContentAligner::LineContentAligner(const InlineFormattingContext& inlineFormattingContext, LineBox& lineBox, LineBuilder::RunList& runs, InlineLayoutUnit availableWidth)
    : m_inlineFormattingContext(inlineFormattingContext)
    , m_lineBox(lineBox)
    , m_runs(runs)
    , m_availableWidth(availableWidth)
{
}

void LineContentAligner::alignHorizontally(TextAlignMode horizontalAlignment, IsLastLineWithInlineContent isLastLine)
{
    auto hangingContent = collectHangingContent(isLastLine);
    auto availableWidth = m_availableWidth + hangingContent.width();
    if (m_runs.isEmpty() || availableWidth <= 0)
        return;

    if (horizontalAlignment == TextAlignMode::Justify) {
        justifyRuns(availableWidth);
        return;
    }

    auto adjustmentForAlignment = [&] (auto availableWidth) -> Optional<InlineLayoutUnit> {
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

    auto adjustment = adjustmentForAlignment(availableWidth);
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

void LineContentAligner::alignVertically()
{
    auto scrollableOverflowRect = m_lineBox.logicalRect();
    for (auto& run : m_runs) {
        auto logicalTop = InlineLayoutUnit { };
        auto& layoutBox = run.layoutBox();
        auto verticalAlign = layoutBox.style().verticalAlign();
        auto ascent = layoutBox.style().fontMetrics().ascent();

        switch (verticalAlign) {
        case VerticalAlign::Baseline:
            if (run.isLineBreak() || run.isText())
                logicalTop = m_lineBox.alignmentBaseline() - ascent;
            else if (run.isContainerStart()) {
                auto& boxGeometry = formattingContext().geometryForBox(layoutBox);
                logicalTop = m_lineBox.alignmentBaseline() - ascent - boxGeometry.borderTop() - boxGeometry.paddingTop().valueOr(0);
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
                logicalTop = m_lineBox.alignmentBaseline() - baselineFromMarginBox;
            } else {
                auto& boxGeometry = formattingContext().geometryForBox(layoutBox);
                logicalTop = m_lineBox.alignmentBaseline() - (boxGeometry.verticalBorder() + boxGeometry.verticalPadding().valueOr(0_lu) + run.logicalRect().height() + boxGeometry.marginAfter());
            }
            break;
        case VerticalAlign::Top:
            logicalTop = 0_lu;
            break;
        case VerticalAlign::Bottom:
            logicalTop = m_lineBox.logicalBottom() - run.logicalRect().height();
            break;
        default:
            ASSERT_NOT_IMPLEMENTED_YET();
            break;
        }
        run.adjustLogicalTop(logicalTop);
        // Adjust scrollable overflow if the run overflows the line.
        scrollableOverflowRect.expandVerticallyToContain(run.logicalRect());
        // Convert runs from relative to the line top/left to the formatting root's border box top/left.
        run.moveVertically(m_lineBox.logicalTop());
        run.moveHorizontally(m_lineBox.logicalLeft());
    }
    m_lineBox.setScrollableOverflow(scrollableOverflowRect);
}

void LineContentAligner::justifyRuns(InlineLayoutUnit availableWidth)
{
    ASSERT(availableWidth > 0);
    // Collect the expansion opportunity numbers and find the last run with content.
    auto expansionOpportunityCount = 0;
    LineBuilder::Run* lastRunWithContent = nullptr;
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

void LineContentAligner::adjustBaselineAndLineHeight()
{
    unsigned inlineContainerNestingLevel = 0;
    auto hasSeenDirectTextOrLineBreak = false;
    for (auto& run : m_runs) {
        auto& layoutBox = run.layoutBox();
        auto& style = layoutBox.style();

        run.setLogicalHeight(runContentHeight(run));

        if (run.isText() || run.isLineBreak()) {
            if (inlineContainerNestingLevel) {
                // We've already adjusted the line height/baseline through the parent inline container. 
                continue;
            }
            if (hasSeenDirectTextOrLineBreak) {
                // e.g div>first text</div> or <div><span>nested<span>first direct text</div>.
                continue;
            }
            hasSeenDirectTextOrLineBreak = true;
            continue;
        }

        if (run.isContainerStart()) {
            ++inlineContainerNestingLevel;
            // Inline containers stretch the line by their font size.
            // Vertical margins, paddings and borders don't contribute to the line height.
            auto& fontMetrics = style.fontMetrics();
            if (style.verticalAlign() == VerticalAlign::Baseline) {
                auto halfLeading = LineBox::halfLeadingMetrics(fontMetrics, style.computedLineHeight());
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
                    m_lineBox.setAlignmentBaselineIfGreater(beforeHeight + lastLineBox.baseline());
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
                    m_lineBox.setAlignmentBaselineIfGreater(m_lineBox.alignmentBaseline() + (marginBoxHeight - lineLogicalHeight));
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

HangingContent LineContentAligner::collectHangingContent(IsLastLineWithInlineContent isLastLineWithInlineContent) const
{
    auto hangingContent = HangingContent { };
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

InlineLayoutUnit LineContentAligner::runContentHeight(const LineBuilder::Run& run) const
{
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

void InlineFormattingContext::layoutInFlowContent(InvalidationState& invalidationState, const ConstraintsForInFlowContent& constraints)
{
    LOG_WITH_STREAM(FormattingContextLayout, stream << "[Start] -> inline formatting context -> formatting root(" << &root() << ")");
    ASSERT(root().hasInFlowOrFloatingChild());

    invalidateFormattingState(invalidationState);
    auto* layoutBox = root().firstInFlowOrFloatingChild();
    // 1. Visit each inline box and partially compute their geometry (margins, padding and borders).
    // 2. Collect the inline items (flatten the the layout tree) and place them on lines in bidirectional order. 
    while (layoutBox) {
        ASSERT(layoutBox->isInlineLevelBox() || layoutBox->isFloatingPositioned());

        if (layoutBox->isAtomicInlineLevelBox() || layoutBox->isFloatingPositioned()) {
            // Inline-blocks, inline-tables and replaced elements (img, video) can be sized but not yet positioned.
            if (is<ContainerBox>(layoutBox) && layoutBox->establishesFormattingContext()) {
                ASSERT(layoutBox->isInlineBlockBox() || layoutBox->isInlineTableBox() || layoutBox->isFloatingPositioned());
                auto& formattingRoot = downcast<ContainerBox>(*layoutBox);
                computeBorderAndPadding(formattingRoot, constraints.horizontal);
                computeWidthAndMargin(formattingRoot, constraints.horizontal);

                if (formattingRoot.hasChild()) {
                    auto formattingContext = LayoutContext::createFormattingContext(formattingRoot, layoutState());
                    if (formattingRoot.hasInFlowOrFloatingChild())
                        formattingContext->layoutInFlowContent(invalidationState, geometry().constraintsForInFlowContent(formattingRoot));
                    computeHeightAndMargin(formattingRoot, constraints.horizontal);
                    formattingContext->layoutOutOfFlowContent(invalidationState, geometry().constraintsForOutOfFlowContent(formattingRoot));
                } else
                    computeHeightAndMargin(formattingRoot, constraints.horizontal);
            } else {
                // Replaced and other type of leaf atomic inline boxes.
                computeBorderAndPadding(*layoutBox, constraints.horizontal);
                computeWidthAndMargin(*layoutBox, constraints.horizontal);
                computeHeightAndMargin(*layoutBox, constraints.horizontal);
            }
        } else if (layoutBox->isInlineBox()) {
            // Text wrapper boxes (anonymous inline level boxes) and <br>s don't generate display boxes (only display runs).
            if (!layoutBox->isInlineTextBox() && !layoutBox->isLineBreakBox()) {
                // Inline boxes (<span>) can't get sized/positioned yet. At this point we can only compute their margins, borders and padding.
                computeBorderAndPadding(*layoutBox, constraints.horizontal);
                computeHorizontalMargin(*layoutBox, constraints.horizontal);
                formattingState().displayBox(*layoutBox).setVerticalMargin({ });
            }
        } else
            ASSERT_NOT_REACHED();

        layoutBox = nextInlineLevelBoxToLayout(*layoutBox, root());
    }

    collectInlineContentIfNeeded();

    auto& inlineItems = formattingState().inlineItems();
    lineLayout(inlineItems, { 0, inlineItems.size() }, constraints);
    LOG_WITH_STREAM(FormattingContextLayout, stream << "[End] -> inline formatting context -> formatting root(" << &root() << ")");
}

void InlineFormattingContext::lineLayout(InlineItems& inlineItems, LineLayoutContext::InlineItemRange needsLayoutRange, const ConstraintsForInFlowContent& constraints)
{
    auto lineLogicalTop = constraints.vertical.logicalTop;
    struct PreviousLine {
        LineLayoutContext::InlineItemRange range;
        Optional<unsigned> overflowContentLength;
    };
    Optional<PreviousLine> previousLine;
    auto line = LineBuilder { *this };
    auto lineLayoutContext = LineLayoutContext { *this, root(), inlineItems };

    while (!needsLayoutRange.isEmpty()) {
        auto lineConstraints = constraintsForLine(constraints.horizontal, lineLogicalTop);
        line.open(lineConstraints.availableLogicalWidth);
        line.setHasIntrusiveFloat(lineConstraints.lineIsConstrainedByFloat);
        // Turn previous line's overflow content length into the next line's leading content partial length.
        // "sp[<-line break->]lit_content" -> overflow length: 11 -> leading partial content length: 11.
        auto partialLeadingContentLength = previousLine ? previousLine->overflowContentLength : WTF::nullopt;
        auto lineContent = lineLayoutContext.layoutInlineContent(line, needsLayoutRange, partialLeadingContentLength);

        auto lineContentRange = lineContent.inlineItemRange;
        auto lineBox = LineBox { Display::InlineRect { lineConstraints.logicalTopLeft, line.contentLogicalWidth(), lineConstraints.lineHeight }, LineBox::halfLeadingMetrics(root().style().fontMetrics(), lineConstraints.lineHeight) };

        // FIXME: This is temporary and will eventually get merged to LineBox.
        auto alignLineContent = [&] {
            auto contentAligner = LineContentAligner { *this, lineBox, line.runs(), line.availableWidth() };
            contentAligner.adjustBaselineAndLineHeight();
            if (line.isVisuallyEmpty()) {
                lineBox.resetAlignmentBaseline();
                lineBox.setLogicalHeight({ });
            }
            // Remove descent when all content is baseline aligned but none of them have descent.
            if (quirks().lineDescentNeedsCollapsing(line.runs())) {
                lineBox.shrinkVertically(lineBox.ascentAndDescent().descent);
                lineBox.resetDescent();
            }
            contentAligner.alignVertically();

            auto isLastLineWithInlineContent = [&] {
                if (lineContentRange.end == needsLayoutRange.end)
                    return LineContentAligner::IsLastLineWithInlineContent::Yes;
                if (lineContent.partialContent)
                    return LineContentAligner::IsLastLineWithInlineContent::No;
                // Omit floats to see if this is the last line with inline content.
                for (auto i = needsLayoutRange.end; i--;) {
                    if (!inlineItems[i].isFloat())
                        return i == lineContentRange.end - 1 ? LineContentAligner::IsLastLineWithInlineContent::Yes : LineContentAligner::IsLastLineWithInlineContent::No;
                }
                // There has to be at least one non-float item.
                ASSERT_NOT_REACHED();
                return LineContentAligner::IsLastLineWithInlineContent::No;
            }();

            auto computedHorizontalAlignment = [&] {
                if (root().style().textAlign() != TextAlignMode::Justify)
                    return root().style().textAlign();
                // Text is justified according to the method specified by the text-justify property,
                // in order to exactly fill the line box. Unless otherwise specified by text-align-last,
                // the last line before a forced break or the end of the block is start-aligned.
                if (line.runs().last().isLineBreak() || isLastLineWithInlineContent == LineContentAligner::IsLastLineWithInlineContent::Yes)
                    return TextAlignMode::Start;
                return TextAlignMode::Justify;
            };
            contentAligner.alignHorizontally(computedHorizontalAlignment(), isLastLineWithInlineContent);
        };

        alignLineContent();
        setDisplayBoxesForLine(line, lineBox, lineContent, constraints.horizontal);

        if (!lineContentRange.isEmpty()) {
            ASSERT(needsLayoutRange.start < lineContentRange.end);
            lineLogicalTop = lineBox.logicalBottom();
            // When the trailing content is partial, we need to reuse the last InlineTextItem.
            auto lastInlineItemNeedsPartialLayout = lineContent.partialContent.hasValue();
            if (lastInlineItemNeedsPartialLayout) {
                ASSERT(lineContent.partialContent->overflowContentLength);
                auto lineLayoutHasAdvanced = !previousLine
                    || lineContentRange.end > previousLine->range.end
                    || (previousLine->overflowContentLength && *previousLine->overflowContentLength > lineContent.partialContent->overflowContentLength);
                if (!lineLayoutHasAdvanced) {
                    ASSERT_NOT_REACHED();
                    // Move over to the next run if we are stuck on this partial content (when the overflow content length remains the same).
                    // We certainly lose some content, but we would be busy looping otherwise.
                    lastInlineItemNeedsPartialLayout = false;
                }
            }
            needsLayoutRange.start = lastInlineItemNeedsPartialLayout ? lineContentRange.end - 1 : lineContentRange.end;
            previousLine = PreviousLine { lineContentRange, lineContent.partialContent ? makeOptional(lineContent.partialContent->overflowContentLength) : WTF::nullopt };
            continue;
        }
        // Floats prevented us placing any content on the line.
        ASSERT(line.runs().isEmpty());
        ASSERT(line.hasIntrusiveFloat());
        // Move the next line below the intrusive float.
        auto floatingContext = FloatingContext { root(), *this, formattingState().floatingState() };
        auto floatConstraints = floatingContext.constraints(lineLogicalTop, toLayoutUnit(lineBox.logicalBottom()));
        ASSERT(floatConstraints.left || floatConstraints.right);
        static auto inifitePoint = PointInContextRoot::max();
        // In case of left and right constraints, we need to pick the one that's closer to the current line.
        lineLogicalTop = std::min(floatConstraints.left.valueOr(inifitePoint).y, floatConstraints.right.valueOr(inifitePoint).y);
        ASSERT(lineLogicalTop < inifitePoint.y);
    }
}

FormattingContext::IntrinsicWidthConstraints InlineFormattingContext::computedIntrinsicWidthConstraints()
{
    auto& layoutState = this->layoutState();
    ASSERT(!formattingState().intrinsicWidthConstraints());

    if (!root().hasInFlowOrFloatingChild()) {
        auto constraints = geometry().constrainByMinMaxWidth(root(), { });
        formattingState().setIntrinsicWidthConstraints(constraints);
        return constraints;
    }

    Vector<const Box*> formattingContextRootList;
    auto horizontalConstraints = HorizontalConstraints { 0_lu, 0_lu };
    auto* layoutBox = root().firstInFlowOrFloatingChild();
    // In order to compute the max/min widths, we need to compute margins, borders and padding for certain inline boxes first.
    while (layoutBox) {
        if (layoutBox->isInlineTextBox() || layoutBox->isLineBreakBox()) {
            layoutBox = nextInlineLevelBoxToLayout(*layoutBox, root());
            continue;
        }
        if (layoutBox->isReplacedBox()) {
            computeBorderAndPadding(*layoutBox, horizontalConstraints);
            computeWidthAndMargin(*layoutBox, horizontalConstraints);
        } else if (layoutBox->isFloatingPositioned() || layoutBox->isAtomicInlineLevelBox()) {
            ASSERT(layoutBox->establishesFormattingContext());
            formattingContextRootList.append(layoutBox);

            computeBorderAndPadding(*layoutBox, horizontalConstraints);
            computeHorizontalMargin(*layoutBox, horizontalConstraints);
            computeIntrinsicWidthForFormattingRoot(*layoutBox);
        } else if (layoutBox->isInlineBox()) {
            computeBorderAndPadding(*layoutBox, horizontalConstraints);
            computeHorizontalMargin(*layoutBox, horizontalConstraints);
        } else
            ASSERT_NOT_REACHED();
        layoutBox = nextInlineLevelBoxToLayout(*layoutBox, root());
    }

    collectInlineContentIfNeeded();

    auto maximumLineWidth = [&](auto availableWidth) {
        // Switch to the min/max formatting root width values before formatting the lines.
        for (auto* formattingRoot : formattingContextRootList) {
            auto intrinsicWidths = layoutState.formattingStateForBox(*formattingRoot).intrinsicWidthConstraintsForBox(*formattingRoot);
            auto& displayBox = formattingState().displayBox(*formattingRoot);
            auto contentWidth = (availableWidth ? intrinsicWidths->maximum : intrinsicWidths->minimum) - displayBox.horizontalMarginBorderAndPadding();
            displayBox.setContentBoxWidth(contentWidth);
        }
        return computedIntrinsicWidthForConstraint(availableWidth);
    };

    auto minimumContentWidth = ceiledLayoutUnit(maximumLineWidth(0));
    auto maximumContentWidth = ceiledLayoutUnit(maximumLineWidth(maxInlineLayoutUnit()));
    auto constraints = geometry().constrainByMinMaxWidth(root(), { minimumContentWidth, maximumContentWidth });
    formattingState().setIntrinsicWidthConstraints(constraints);
    return constraints;
}

InlineLayoutUnit InlineFormattingContext::computedIntrinsicWidthForConstraint(InlineLayoutUnit availableWidth) const
{
    auto& inlineItems = formattingState().inlineItems();
    auto maximumLineWidth = InlineLayoutUnit { };
    auto line = LineBuilder { *this };
    auto lineLayoutContext = LineLayoutContext { *this, root(), inlineItems };
    auto layoutRange = LineLayoutContext::InlineItemRange { 0 , inlineItems.size() };
    while (!layoutRange.isEmpty()) {
        // Only the horiztonal available width is constrained when computing intrinsic width.
        line.open(availableWidth);
        auto lineContent = lineLayoutContext.layoutInlineContent(line, layoutRange, { });
        layoutRange.start = lineContent.inlineItemRange.end;
        // FIXME: Use line logical left and right to take floats into account.
        maximumLineWidth = std::max(maximumLineWidth, line.contentLogicalWidth());
    }
    return maximumLineWidth;
}

void InlineFormattingContext::computeIntrinsicWidthForFormattingRoot(const Box& formattingRoot)
{
    ASSERT(formattingRoot.establishesFormattingContext());
    auto constraints = IntrinsicWidthConstraints { };
    if (auto fixedWidth = geometry().fixedValue(formattingRoot.style().logicalWidth()))
        constraints = { *fixedWidth, *fixedWidth };
    else if (is<ContainerBox>(formattingRoot) && downcast<ContainerBox>(formattingRoot).hasInFlowOrFloatingChild())
        constraints = LayoutContext::createFormattingContext(downcast<ContainerBox>(formattingRoot), layoutState())->computedIntrinsicWidthConstraints();
    constraints = geometry().constrainByMinMaxWidth(formattingRoot, constraints);
    constraints.expand(geometryForBox(formattingRoot).horizontalMarginBorderAndPadding());
    formattingState().setIntrinsicWidthConstraintsForBox(formattingRoot, constraints);
}

void InlineFormattingContext::computeHorizontalMargin(const Box& layoutBox, const HorizontalConstraints& horizontalConstraints)
{
    auto computedHorizontalMargin = geometry().computedHorizontalMargin(layoutBox, horizontalConstraints);
    formattingState().displayBox(layoutBox).setHorizontalMargin({ computedHorizontalMargin.start.valueOr(0), computedHorizontalMargin.end.valueOr(0) });
}

void InlineFormattingContext::computeWidthAndMargin(const Box& layoutBox, const HorizontalConstraints& horizontalConstraints)
{
    auto compute = [&](Optional<LayoutUnit> usedWidth) {
        if (layoutBox.isFloatingPositioned())
            return geometry().floatingWidthAndMargin(layoutBox, horizontalConstraints, { usedWidth, { } });
        if (layoutBox.isInlineBlockBox())
            return geometry().inlineBlockWidthAndMargin(layoutBox, horizontalConstraints, { usedWidth, { } });
        if (layoutBox.isReplacedBox())
            return geometry().inlineReplacedWidthAndMargin(downcast<ReplacedBox>(layoutBox), horizontalConstraints, { }, { usedWidth, { } });
        ASSERT_NOT_REACHED();
        return ContentWidthAndMargin { };
    };

    auto contentWidthAndMargin = compute({ });

    auto availableWidth = horizontalConstraints.logicalWidth;
    if (auto maxWidth = geometry().computedMaxWidth(layoutBox, availableWidth)) {
        auto maxWidthAndMargin = compute(maxWidth);
        if (contentWidthAndMargin.contentWidth > maxWidthAndMargin.contentWidth)
            contentWidthAndMargin = maxWidthAndMargin;
    }

    auto minWidth = geometry().computedMinWidth(layoutBox, availableWidth).valueOr(0);
    auto minWidthAndMargin = compute(minWidth);
    if (contentWidthAndMargin.contentWidth < minWidthAndMargin.contentWidth)
        contentWidthAndMargin = minWidthAndMargin;

    auto& displayBox = formattingState().displayBox(layoutBox);
    displayBox.setContentBoxWidth(contentWidthAndMargin.contentWidth);
    displayBox.setHorizontalMargin({ contentWidthAndMargin.usedMargin.start, contentWidthAndMargin.usedMargin.end });
}

void InlineFormattingContext::computeHeightAndMargin(const Box& layoutBox, const HorizontalConstraints& horizontalConstraints)
{
    auto compute = [&](Optional<LayoutUnit> usedHeight) {
        if (layoutBox.isFloatingPositioned())
            return geometry().floatingHeightAndMargin(layoutBox, horizontalConstraints, { usedHeight });
        if (layoutBox.isInlineBlockBox())
            return geometry().inlineBlockHeightAndMargin(layoutBox, horizontalConstraints, { usedHeight });
        if (layoutBox.isReplacedBox())
            return geometry().inlineReplacedHeightAndMargin(downcast<ReplacedBox>(layoutBox), horizontalConstraints, { }, { usedHeight });
        ASSERT_NOT_REACHED();
        return ContentHeightAndMargin { };
    };

    auto contentHeightAndMargin = compute({ });
    if (auto maxHeight = geometry().computedMaxHeight(layoutBox)) {
        auto maxHeightAndMargin = compute(maxHeight);
        if (contentHeightAndMargin.contentHeight > maxHeightAndMargin.contentHeight)
            contentHeightAndMargin = maxHeightAndMargin;
    }

    if (auto minHeight = geometry().computedMinHeight(layoutBox)) {
        auto minHeightAndMargin = compute(minHeight);
        if (contentHeightAndMargin.contentHeight < minHeightAndMargin.contentHeight)
            contentHeightAndMargin = minHeightAndMargin;
    }
    auto& displayBox = formattingState().displayBox(layoutBox);
    displayBox.setContentBoxHeight(contentHeightAndMargin.contentHeight);
    displayBox.setVerticalMargin({ contentHeightAndMargin.nonCollapsedMargin.before, contentHeightAndMargin.nonCollapsedMargin.after });
}

void InlineFormattingContext::collectInlineContentIfNeeded()
{
    auto& formattingState = this->formattingState();
    if (!formattingState.inlineItems().isEmpty())
        return;
    // Traverse the tree and create inline items out of containers and leaf nodes. This essentially turns the tree inline structure into a flat one.
    // <span>text<span></span><img></span> -> [ContainerStart][InlineBox][ContainerStart][ContainerEnd][InlineBox][ContainerEnd]
    ASSERT(root().hasInFlowOrFloatingChild());
    LayoutQueue layoutQueue;
    layoutQueue.append(root().firstInFlowOrFloatingChild());
    while (!layoutQueue.isEmpty()) {
        while (true) {
            auto& layoutBox = *layoutQueue.last();
            auto isBoxWithInlineContent = layoutBox.isInlineBox() && !layoutBox.isInlineTextBox() && !layoutBox.isLineBreakBox();
            if (!isBoxWithInlineContent)
                break;
            // This is the start of an inline box (e.g. <span>).
            formattingState.addInlineItem({ layoutBox, InlineItem::Type::ContainerStart });
            auto& inlineBoxWithInlineContent = downcast<ContainerBox>(layoutBox);
            if (!inlineBoxWithInlineContent.hasInFlowOrFloatingChild())
                break;
            layoutQueue.append(inlineBoxWithInlineContent.firstInFlowOrFloatingChild());
        }

        while (!layoutQueue.isEmpty()) {
            auto& layoutBox = *layoutQueue.takeLast();
            if (layoutBox.isLineBreakBox()) {
                // FIXME: Treat <wbr> as a word break opportunity instead.
                formattingState.addInlineItem({ layoutBox, InlineItem::Type::HardLineBreak });
            } else if (layoutBox.isFloatingPositioned())
                formattingState.addInlineItem({ layoutBox, InlineItem::Type::Float });
            else if (layoutBox.isAtomicInlineLevelBox())
                formattingState.addInlineItem({ layoutBox, InlineItem::Type::Box });
            else if (layoutBox.isInlineTextBox()) {
                InlineTextItem::createAndAppendTextItems(formattingState.inlineItems(), downcast<InlineTextBox>(layoutBox));
            } else if (layoutBox.isInlineBox())
                formattingState.addInlineItem({ layoutBox, InlineItem::Type::ContainerEnd });
            else
                ASSERT_NOT_REACHED();

            if (auto* nextSibling = layoutBox.nextInFlowOrFloatingSibling()) {
                layoutQueue.append(nextSibling);
                break;
            }
        }
    }
}

InlineFormattingContext::LineConstraints InlineFormattingContext::constraintsForLine(const HorizontalConstraints& horizontalConstraints, InlineLayoutUnit lineLogicalTop)
{
    auto lineLogicalLeft = horizontalConstraints.logicalLeft;
    auto lineLogicalRight = lineLogicalLeft + horizontalConstraints.logicalWidth;
    auto initialLineHeight = quirks().initialLineHeight(root());
    auto lineIsConstrainedByFloat = false;

    auto floatingContext = FloatingContext { root(), *this, formattingState().floatingState() };
    // Check for intruding floats and adjust logical left/available width for this line accordingly.
    if (!floatingContext.isEmpty()) {
        // FIXME: Add support for variable line height, where the intrusive floats should be probed as the line height grows.
        auto floatConstraints = floatingContext.constraints(toLayoutUnit(lineLogicalTop), toLayoutUnit(lineLogicalTop + initialLineHeight));
        // Check if these constraints actually put limitation on the line.
        if (floatConstraints.left && floatConstraints.left->x <= lineLogicalLeft)
            floatConstraints.left = { };

        if (floatConstraints.right && floatConstraints.right->x >= lineLogicalRight)
            floatConstraints.right = { };

        lineIsConstrainedByFloat = floatConstraints.left || floatConstraints.right;

        if (floatConstraints.left && floatConstraints.right) {
            ASSERT(floatConstraints.left->x <= floatConstraints.right->x);
            lineLogicalRight = floatConstraints.right->x;
            lineLogicalLeft = floatConstraints.left->x;
        } else if (floatConstraints.left) {
            ASSERT(floatConstraints.left->x >= lineLogicalLeft);
            lineLogicalLeft = floatConstraints.left->x;
        } else if (floatConstraints.right) {
            ASSERT(floatConstraints.right->x >= lineLogicalLeft);
            lineLogicalRight = floatConstraints.right->x;
        }
    }

    auto computedTextIndent = [&] {
        // text-indent property specifies the indentation applied to lines of inline content in a block.
        // The indent is treated as a margin applied to the start edge of the line box.
        // Unless otherwise specified, only lines that are the first formatted line of an element are affected.
        // For example, the first line of an anonymous block box is only affected if it is the first child of its parent element.
        // FIXME: Add support for each-line.
        // [Integration] root()->parent() would normally produce a valid layout box.
        auto& root = this->root();
        auto isFormattingContextRootCandidateToTextIndent = !root.isAnonymous();
        if (root.isAnonymous()) {
            // Unless otherwise specified by the each-line and/or hanging keywords, only lines that are the first formatted line
            // of an element are affected.
            // For example, the first line of an anonymous block box is only affected if it is the first child of its parent element.
            isFormattingContextRootCandidateToTextIndent = RuntimeEnabledFeatures::sharedFeatures().layoutFormattingContextIntegrationEnabled()
                ? layoutState().isIntegratedRootBoxFirstChild()
                : root.parent().firstInFlowChild() == &root;
        }
        if (!isFormattingContextRootCandidateToTextIndent)
            return InlineLayoutUnit { };
        auto invertLineRange = false;
#if ENABLE(CSS3_TEXT)
        invertLineRange = root.style().textIndentType() == TextIndentType::Hanging;
#endif
        auto isFirstLine = formattingState().ensureDisplayInlineContent().lineBoxes.isEmpty();
        // text-indent: hanging inverts which lines are affected.
        // inverted line range -> all the lines except the first one.
        // !inverted line range -> first line gets the indent.
        auto shouldIndent = invertLineRange != isFirstLine;
        if (!shouldIndent)
            return InlineLayoutUnit { };
        return geometry().computedTextIndent(root, horizontalConstraints).valueOr(InlineLayoutUnit { });
    };
    lineLogicalLeft += computedTextIndent();
    return LineConstraints { { lineLogicalLeft, lineLogicalTop }, lineLogicalRight - lineLogicalLeft, initialLineHeight, lineIsConstrainedByFloat };
}

void InlineFormattingContext::setDisplayBoxesForLine(const LineBuilder& line, const LineBox& lineBox,  const LineLayoutContext::LineContent& lineContent, const HorizontalConstraints& horizontalConstraints)
{
    auto& formattingState = this->formattingState();

    if (!lineContent.floats.isEmpty()) {
        auto floatingContext = FloatingContext { root(), *this, formattingState.floatingState() };
        // Move floats to their final position.
        for (const auto& floatCandidate : lineContent.floats) {
            auto& floatBox = floatCandidate.item->layoutBox();
            auto& displayBox = formattingState.displayBox(floatBox);
            // Set static position first.
            auto verticalStaticPosition = floatCandidate.isIntrusive ? lineBox.logicalTop() : lineBox.logicalBottom();
            displayBox.setTopLeft({ lineBox.logicalLeft(), verticalStaticPosition });
            // Float it.
            displayBox.setTopLeft(floatingContext.positionForFloat(floatBox, horizontalConstraints));
            floatingContext.append(floatBox);
        }
    }

    auto initialContaingBlockSize = LayoutSize { };
    if (RuntimeEnabledFeatures::sharedFeatures().layoutFormattingContextIntegrationEnabled()) {
        // ICB is not the real ICB when lyoutFormattingContextIntegrationEnabled is on.
        initialContaingBlockSize = layoutState().viewportSize();
    } else
        initialContaingBlockSize = geometryForBox(root().initialContainingBlock(), EscapeReason::StrokeOverflowNeedsViewportGeometry).contentBox().size();
    auto& inlineContent = formattingState.ensureDisplayInlineContent();
    auto lineIndex = inlineContent.lineBoxes.size();
    auto lineInkOverflow = lineBox.scrollableOverflow();
    // Compute box final geometry.
    auto& lineRuns = line.runs();
    for (unsigned index = 0; index < lineRuns.size(); ++index) {
        auto& lineRun = lineRuns.at(index);
        auto& logicalRect = lineRun.logicalRect();
        auto& layoutBox = lineRun.layoutBox();

        // Add final display runs to state first.
        // Inline level containers (<span>) don't generate display runs and neither do completely collapsed runs.
        auto initiatesInlineRun = !lineRun.isContainerStart() && !lineRun.isContainerEnd();
        if (initiatesInlineRun) {
            auto computedInkOverflow = [&] {
                // FIXME: Add support for non-text ink overflow.
                if (!lineRun.isText())
                    return logicalRect;
                auto& style = lineRun.style();
                auto inkOverflow = logicalRect;
                auto strokeOverflow = std::ceil(style.computedStrokeWidth(ceiledIntSize(initialContaingBlockSize)));
                inkOverflow.inflate(strokeOverflow);

                auto letterSpacing = style.fontCascade().letterSpacing();
                if (letterSpacing < 0) {
                    // Last letter's negative spacing shrinks logical rect. Push it to ink overflow.
                    inkOverflow.expandHorizontally(-letterSpacing);
                }
                return inkOverflow;
            };
            auto inkOverflow = computedInkOverflow();
            lineInkOverflow.expandToContain(inkOverflow);
            inlineContent.runs.append({ lineIndex, lineRun.layoutBox(), logicalRect, inkOverflow, lineRun.expansion(), lineRun.textContent() });
        }

        if (lineRun.isText())
            continue;

        if (lineRun.isLineBreak()) {
            // FIXME: Since <br> and <wbr> runs have associated DOM elements, we might need to construct a display box here. 
            continue;
        }

        // Inline level box (replaced or inline-block)
        if (lineRun.isBox()) {
            auto topLeft = logicalRect.topLeft();
            if (layoutBox.isInFlowPositioned())
                topLeft += geometry().inFlowPositionedPositionOffset(layoutBox, horizontalConstraints);
            auto& displayBox = formattingState.displayBox(layoutBox);
            displayBox.setTopLeft(toLayoutPoint(topLeft));
            continue;
        }

        // Inline level container start (<span>)
        if (lineRun.isContainerStart()) {
            auto& displayBox = formattingState.displayBox(layoutBox);
            displayBox.setTopLeft(toLayoutPoint(logicalRect.topLeft()));
            continue;
        }

        // Inline level container end (</span>)
        if (lineRun.isContainerEnd()) {
            auto& displayBox = formattingState.displayBox(layoutBox);
            if (layoutBox.isInFlowPositioned()) {
                auto inflowOffset = geometry().inFlowPositionedPositionOffset(layoutBox, horizontalConstraints);
                displayBox.moveHorizontally(inflowOffset.width());
                displayBox.moveVertically(inflowOffset.height());
            }
            auto marginBoxWidth = logicalRect.left() - displayBox.left();
            auto contentBoxWidth = marginBoxWidth - (displayBox.marginStart() + displayBox.borderLeft() + displayBox.paddingLeft().valueOr(0));
            // FIXME fix it for multiline.
            displayBox.setContentBoxWidth(toLayoutUnit(contentBoxWidth));
            displayBox.setContentBoxHeight(toLayoutUnit(logicalRect.height()));
            continue;
        }

        ASSERT_NOT_REACHED();
    }
    // FIXME: This is where the logical to physical translate should happen.
    inlineContent.lineBoxes.append({ lineBox.logicalRect(), lineBox.scrollableOverflow(), lineInkOverflow, lineBox.alignmentBaseline() });
}

void InlineFormattingContext::invalidateFormattingState(const InvalidationState&)
{
    // Find out what we need to invalidate. This is where we add some smarts to do partial line layout.
    // For now let's just clear the runs.
    formattingState().clearDisplayInlineContent();
    // FIXME: This is also where we would delete inline items if their content changed.
}

}
}

#endif
