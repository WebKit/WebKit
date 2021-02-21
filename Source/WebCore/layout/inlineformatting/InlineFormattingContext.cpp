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
#include "InlineLineBox.h"
#include "InlineLineRun.h"
#include "InlineTextItem.h"
#include "InvalidationState.h"
#include "LayoutBox.h"
#include "LayoutContainerBox.h"
#include "LayoutContext.h"
#include "LayoutInitialContainingBlock.h"
#include "LayoutInlineTextBox.h"
#include "LayoutLineBreakBox.h"
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
        } else if (layoutBox->isLineBreakBox()) {
            auto& boxGeometry = formattingState().boxGeometry(*layoutBox);
            boxGeometry.setHorizontalMargin({ });
            boxGeometry.setBorder({ });
            boxGeometry.setPadding({ });
            boxGeometry.setContentBoxWidth({ });
            boxGeometry.setVerticalMargin({ });
        } else if (layoutBox->isInlineBox()) {
            // Text wrapper boxes (anonymous inline level boxes) don't have box geometries (they only generate runs).
            if (!layoutBox->isInlineTextBox()) {
                // Inline boxes (<span>) can't get sized/positioned yet. At this point we can only compute their margins, borders and padding.
                computeBorderAndPadding(*layoutBox, constraints.horizontal);
                computeHorizontalMargin(*layoutBox, constraints.horizontal);
                formattingState().boxGeometry(*layoutBox).setVerticalMargin({ });
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

void InlineFormattingContext::lineLayoutForIntergration(InvalidationState& invalidationState, const ConstraintsForInFlowContent& constraints)
{
    invalidateFormattingState(invalidationState);
    collectInlineContentIfNeeded();
    auto& inlineItems = formattingState().inlineItems();
    lineLayout(inlineItems, { 0, inlineItems.size() }, constraints);
}

LayoutUnit InlineFormattingContext::usedContentHeight() const
{
    // 10.6.7 'Auto' heights for block formatting context roots

    // If it only has inline-level children, the height is the distance between the top of the topmost line box and the bottom of the bottommost line box.

    // In addition, if the element has any floating descendants whose bottom margin edge is below the element's bottom content edge,
    // then the height is increased to include those edges. Only floats that participate in this block formatting context are taken
    // into account, e.g., floats inside absolutely positioned descendants or other floats are not.
    auto& lines = formattingState().lines();
    // Even empty containers generate one line.
    ASSERT(!lines.isEmpty());
    auto top = LayoutUnit { lines.first().lineBoxLogicalRect().top() };
    auto bottom = LayoutUnit { lines.last().lineBoxLogicalRect().bottom() + formattingState().clearGapAfterLastLine() };

    auto floatingContext = FloatingContext { *this, formattingState().floatingState() };
    if (auto floatBottom = floatingContext.bottom()) {
        bottom = std::max(*floatBottom, bottom);
        top = std::min(*floatingContext.top(), top);
    }
    return bottom - top;
}

void InlineFormattingContext::lineLayout(InlineItems& inlineItems, LineBuilder::InlineItemRange needsLayoutRange, const ConstraintsForInFlowContent& constraints)
{
    auto& formattingState = this->formattingState();
    formattingState.lineRuns().reserveInitialCapacity(formattingState.inlineItems().size());
    InlineLayoutUnit lineLogicalTop = constraints.vertical.logicalTop;
    struct PreviousLine {
        LineBuilder::InlineItemRange range;
        size_t overflowContentLength { 0 };
        Optional<InlineLayoutUnit> overflowLogicalWidth;
    };
    Optional<PreviousLine> previousLine;
    auto& floatingState = formattingState.floatingState();
    auto floatingContext = FloatingContext { *this, floatingState };
    auto isFirstLine = formattingState.lines().isEmpty();

    auto lineBuilder = LineBuilder { *this, floatingState, constraints.horizontal, inlineItems };
    while (!needsLayoutRange.isEmpty()) {
        // Turn previous line's overflow content length into the next line's leading content partial length.
        // "sp[<-line break->]lit_content" -> overflow length: 11 -> leading partial content length: 11.
        auto partialLeadingContentLength = previousLine ? previousLine->overflowContentLength : 0;
        auto leadingLogicalWidth = previousLine ? previousLine->overflowLogicalWidth : WTF::nullopt;
        auto initialLineConstraints = InlineRect { lineLogicalTop, constraints.horizontal.logicalLeft, constraints.horizontal.logicalWidth, quirks().initialLineHeight() };
        auto lineContent = lineBuilder.layoutInlineContent(needsLayoutRange, partialLeadingContentLength, leadingLogicalWidth, initialLineConstraints, isFirstLine);
        auto lineLogicalRect = computeGeometryForLineContent(lineContent, constraints.horizontal);

        auto lineContentRange = lineContent.inlineItemRange;
        if (!lineContentRange.isEmpty()) {
            ASSERT(needsLayoutRange.start < lineContentRange.end);
            isFirstLine = false;
            lineLogicalTop = geometry().logicalTopForNextLine(lineContent, lineLogicalRect.bottom(), floatingContext);
            if (lineContent.isLastLineWithInlineContent) {
                // The final content height of this inline formatting context should include the cleared floats as well.
                formattingState.setClearGapAfterLastLine(lineLogicalTop - lineLogicalRect.bottom());
            }
            // When the trailing content is partial, we need to reuse the last InlineTextItem.
            auto lastInlineItemNeedsPartialLayout = lineContent.partialTrailingContentLength;
            if (lastInlineItemNeedsPartialLayout) {
                auto lineLayoutHasAdvanced = !previousLine
                    || lineContentRange.end > previousLine->range.end
                    || (previousLine->overflowContentLength && previousLine->overflowContentLength > lineContent.partialTrailingContentLength);
                if (!lineLayoutHasAdvanced) {
                    ASSERT_NOT_REACHED();
                    // Move over to the next run if we are stuck on this partial content (when the overflow content length remains the same).
                    // We certainly lose some content, but we would be busy looping otherwise.
                    lastInlineItemNeedsPartialLayout = false;
                }
            }
            needsLayoutRange.start = lastInlineItemNeedsPartialLayout ? lineContentRange.end - 1 : lineContentRange.end;
            previousLine = PreviousLine { lineContentRange, lineContent.partialTrailingContentLength, lineContent.overflowLogicalWidth };
            continue;
        }
        // Floats prevented us placing any content on the line.
        ASSERT(lineContent.runs.isEmpty());
        ASSERT(lineContent.hasIntrusiveFloat);
        // Move the next line below the intrusive float.
        auto floatConstraints = floatingContext.constraints(toLayoutUnit(lineLogicalTop), toLayoutUnit(lineLogicalRect.bottom()));
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
        if (layoutBox->isInlineTextBox()) {
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
            auto& boxGeometry = formattingState().boxGeometry(*formattingRoot);
            auto contentWidth = (availableWidth ? intrinsicWidths->maximum : intrinsicWidths->minimum) - boxGeometry.horizontalMarginBorderAndPadding();
            boxGeometry.setContentBoxWidth(contentWidth);
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
    auto lineBuilder = LineBuilder { *this, inlineItems };
    auto layoutRange = LineBuilder::InlineItemRange { 0 , inlineItems.size() };
    auto maximumLineWidth = InlineLayoutUnit { };
    auto maximumFloatWidth = LayoutUnit { };
    while (!layoutRange.isEmpty()) {
        auto intrinsicContent = lineBuilder.computedIntrinsicWidth(layoutRange, availableWidth);
        layoutRange.start = intrinsicContent.inlineItemRange.end;
        maximumLineWidth = std::max(maximumLineWidth, intrinsicContent.logicalWidth);
        // FIXME: Add support for clear.
        for (auto* floatBox : intrinsicContent.floats)
            maximumFloatWidth += geometryForBox(*floatBox).marginBoxWidth();
    }
    return maximumLineWidth + maximumFloatWidth;
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
    formattingState().boxGeometry(layoutBox).setHorizontalMargin({ computedHorizontalMargin.start.valueOr(0), computedHorizontalMargin.end.valueOr(0) });
}

void InlineFormattingContext::computeWidthAndMargin(const Box& layoutBox, const HorizontalConstraints& horizontalConstraints)
{
    auto compute = [&](Optional<LayoutUnit> usedWidth) {
        if (layoutBox.isFloatingPositioned())
            return geometry().floatingContentWidthAndMargin(layoutBox, horizontalConstraints, { usedWidth, { } });
        if (layoutBox.isInlineBlockBox())
            return geometry().inlineBlockContentWidthAndMargin(layoutBox, horizontalConstraints, { usedWidth, { } });
        if (layoutBox.isReplacedBox())
            return geometry().inlineReplacedContentWidthAndMargin(downcast<ReplacedBox>(layoutBox), horizontalConstraints, { }, { usedWidth, { } });
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

    auto& boxGeometry = formattingState().boxGeometry(layoutBox);
    boxGeometry.setContentBoxWidth(contentWidthAndMargin.contentWidth);
    boxGeometry.setHorizontalMargin({ contentWidthAndMargin.usedMargin.start, contentWidthAndMargin.usedMargin.end });
}

void InlineFormattingContext::computeHeightAndMargin(const Box& layoutBox, const HorizontalConstraints& horizontalConstraints)
{
    auto compute = [&](Optional<LayoutUnit> usedHeight) {
        if (layoutBox.isFloatingPositioned())
            return geometry().floatingContentHeightAndMargin(layoutBox, horizontalConstraints, { usedHeight });
        if (layoutBox.isInlineBlockBox())
            return geometry().inlineBlockContentHeightAndMargin(layoutBox, horizontalConstraints, { usedHeight });
        if (layoutBox.isReplacedBox())
            return geometry().inlineReplacedContentHeightAndMargin(downcast<ReplacedBox>(layoutBox), horizontalConstraints, { }, { usedHeight });
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
    auto& boxGeometry = formattingState().boxGeometry(layoutBox);
    boxGeometry.setContentBoxHeight(contentHeightAndMargin.contentHeight);
    boxGeometry.setVerticalMargin({ contentHeightAndMargin.nonCollapsedMargin.before, contentHeightAndMargin.nonCollapsedMargin.after });
}

void InlineFormattingContext::collectInlineContentIfNeeded()
{
    auto& formattingState = this->formattingState();
    if (!formattingState.inlineItems().isEmpty())
        return;
    // Traverse the tree and create inline items out of containers and leaf nodes. This essentially turns the tree inline structure into a flat one.
    // <span>text<span></span><img></span> -> [InlineBoxStart][InlineLevelBox][InlineBoxStart][InlineBoxEnd][InlineLevelBox][InlineBoxEnd]
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
            formattingState.addInlineItem({ layoutBox, InlineItem::Type::InlineBoxStart });
            auto& inlineBoxWithInlineContent = downcast<ContainerBox>(layoutBox);
            if (!inlineBoxWithInlineContent.hasInFlowOrFloatingChild())
                break;
            layoutQueue.append(inlineBoxWithInlineContent.firstInFlowOrFloatingChild());
        }

        while (!layoutQueue.isEmpty()) {
            auto& layoutBox = *layoutQueue.takeLast();
            if (is<LineBreakBox>(layoutBox)) {
                auto& lineBreakBox = downcast<LineBreakBox>(layoutBox);
                formattingState.addInlineItem({ layoutBox, lineBreakBox.isOptional() ? InlineItem::Type::WordBreakOpportunity : InlineItem::Type::HardLineBreak });
            } else if (layoutBox.isFloatingPositioned())
                formattingState.addInlineItem({ layoutBox, InlineItem::Type::Float });
            else if (layoutBox.isAtomicInlineLevelBox())
                formattingState.addInlineItem({ layoutBox, InlineItem::Type::Box });
            else if (layoutBox.isInlineTextBox()) {
                InlineTextItem::createAndAppendTextItems(formattingState.inlineItems(), downcast<InlineTextBox>(layoutBox));
            } else if (layoutBox.isInlineBox())
                formattingState.addInlineItem({ layoutBox, InlineItem::Type::InlineBoxEnd });
            else
                ASSERT_NOT_REACHED();

            if (auto* nextSibling = layoutBox.nextInFlowOrFloatingSibling()) {
                layoutQueue.append(nextSibling);
                break;
            }
        }
    }
}

InlineRect InlineFormattingContext::computeGeometryForLineContent(const LineBuilder::LineContent& lineContent, const HorizontalConstraints& horizontalConstraints)
{
    auto& formattingState = this->formattingState();
    auto geometry = this->geometry();

    formattingState.addLineBox(geometry.lineBoxForLineContent(lineContent));
    const auto& lineBox = formattingState.lineBoxes().last();
    auto lineIndex = formattingState.lines().size();
    auto& lineBoxLogicalRect = lineBox.logicalRect();
    if (!lineBox.hasContent()) {
        // Fast path for lines with no content e.g. <div><span></span><span></span></div> or <span><div></div></span> where we construct empty pre and post blocks.
        ASSERT(!lineBox.contentLogicalWidth() && !lineBoxLogicalRect.height());
        auto updateInlineBoxesGeometryIfApplicable = [&] {
            if (!lineBox.hasInlineBox())
                return;
            Vector<const Box*> layoutBoxList;
            // Collect the empty inline boxes that showed up first on this line.
            // Note that an inline box end on an empty line does not make the inline box taller.
            // (e.g. <div>text<span><br></span></div>) <- the <span> inline box is as tall as the line even though the </span> is after a <br> so technically is on the following (empty)line.
            for (auto& lineRun : lineContent.runs) {
                if (lineRun.isInlineBoxStart())
                    layoutBoxList.append(&lineRun.layoutBox());
            }
            for (auto* layoutBox : layoutBoxList) {
                auto& boxGeometry = formattingState.boxGeometry(*layoutBox);
                auto inlineBoxLogicalHeight = LayoutUnit::fromFloatCeil(lineBox.logicalBorderBoxForInlineBox(*layoutBox, boxGeometry).height());
                boxGeometry.setContentBoxHeight(inlineBoxLogicalHeight);
                boxGeometry.setContentBoxWidth({ });
                boxGeometry.setLogicalTopLeft(toLayoutPoint(lineBoxLogicalRect.topLeft()));
            }
        };
        updateInlineBoxesGeometryIfApplicable();
        formattingState.addLine({ lineBoxLogicalRect, { { }, { } }, { }, { }, { } });
        return lineBoxLogicalRect;
    }

    auto rootInlineBoxLogicalRect = lineBox.logicalRectForRootInlineBox();
    auto enclosingTopAndBottom = InlineLineGeometry::EnclosingTopAndBottom { rootInlineBoxLogicalRect.top(), rootInlineBoxLogicalRect.bottom() };
    HashSet<const Box*> inlineBoxStartSet;
    HashSet<const Box*> inlineBoxEndSet;

    auto constructLineRunsAndUpdateBoxGeometry = [&] {
        // Create the inline runs on the current line. This is mostly text and atomic inline runs.
        for (auto& lineRun : lineContent.runs) {
            // FIXME: We should not need to construct a line run for <br>.
            auto& layoutBox = lineRun.layoutBox();
            if (lineRun.isText()) {
                formattingState.addLineRun({ lineIndex, layoutBox, lineBox.logicalRectForTextRun(lineRun), lineRun.expansion(), lineRun.textContent() });
                continue;
            }
            if (lineRun.isLineBreak()) {
                if (layoutBox.isLineBreakBox()) {
                    // Only hard linebreaks have associated layout boxes.
                    auto lineBreakBoxRect = lineBox.logicalRectForLineBreakBox(layoutBox);
                    formattingState.addLineRun({ lineIndex, layoutBox, lineBreakBoxRect, lineRun.expansion(), { } });

                    auto& boxGeometry = formattingState.boxGeometry(layoutBox);
                    lineBreakBoxRect.moveBy(lineBoxLogicalRect.topLeft());
                    boxGeometry.setLogicalTopLeft(toLayoutPoint(lineBreakBoxRect.topLeft()));
                    boxGeometry.setContentBoxHeight(toLayoutUnit(lineBreakBoxRect.height()));
                } else 
                    formattingState.addLineRun({ lineIndex, layoutBox, lineBox.logicalRectForTextRun(lineRun), lineRun.expansion(), lineRun.textContent() });
                continue;
            }
            if (lineRun.isBox()) {
                ASSERT(layoutBox.isAtomicInlineLevelBox());
                auto& boxGeometry = formattingState.boxGeometry(layoutBox);
                auto logicalBorderBox = lineBox.logicalBorderBoxForAtomicInlineLevelBox(layoutBox, boxGeometry);
                formattingState.addLineRun({ lineIndex, layoutBox, logicalBorderBox, lineRun.expansion(), { } });

                auto borderBoxLogicalTopLeft = logicalBorderBox.topLeft();
                // Note that inline boxes are relative to the line and their top position can be negative.
                borderBoxLogicalTopLeft.moveBy(lineBoxLogicalRect.topLeft());
                if (layoutBox.isInFlowPositioned())
                    borderBoxLogicalTopLeft += geometry.inFlowPositionedPositionOffset(layoutBox, horizontalConstraints);
                // Atomic inline boxes are all set. Their margin/border/content box geometries are already computed. We just have to position them here.
                boxGeometry.setLogicalTopLeft(toLayoutPoint(borderBoxLogicalTopLeft));

                auto borderBoxTop = borderBoxLogicalTopLeft.y();
                auto borderBoxBottom = borderBoxTop + boxGeometry.borderBoxHeight();
                enclosingTopAndBottom.top = std::min(enclosingTopAndBottom.top, borderBoxTop);
                enclosingTopAndBottom.bottom = std::max(enclosingTopAndBottom.bottom, borderBoxBottom);
                continue;
            }
            if (lineRun.isInlineBoxStart()) {
                auto& boxGeometry = formattingState.boxGeometry(layoutBox);
                auto inlineBoxLogicalRect = lineBox.logicalBorderBoxForInlineBox(layoutBox, boxGeometry);
                formattingState.addLineRun({ lineIndex, layoutBox, inlineBoxLogicalRect, lineRun.expansion(), { } });
                inlineBoxStartSet.add(&layoutBox);
                enclosingTopAndBottom.top = std::min(enclosingTopAndBottom.top, inlineBoxLogicalRect.top());
                continue;
            }
            if (lineRun.isInlineBoxEnd()) {
                inlineBoxEndSet.add(&layoutBox);
                auto inlineBoxLogicalRect = lineBox.logicalBorderBoxForInlineBox(layoutBox, formattingState.boxGeometry(layoutBox));
                enclosingTopAndBottom.bottom = std::max(enclosingTopAndBottom.bottom, inlineBoxLogicalRect.bottom());
                continue;
            }
            ASSERT(lineRun.isWordBreakOpportunity());
        }
    };
    constructLineRunsAndUpdateBoxGeometry();

    auto updateBoxGeometryForInlineBoxes = [&] {
        // FIXME: We may want to keep around an inline box only set.
        if (!lineBox.hasInlineBox())
            return;
        // Grab the inline boxes (even those that don't have associated layout boxes on the current line due to line wrapping)
        // and update their geometries.
        for (auto& inlineLevelBox : lineBox.nonRootInlineLevelBoxes()) {
            if (!inlineLevelBox->isInlineBox())
                continue;
            auto& layoutBox = inlineLevelBox->layoutBox();
            auto& boxGeometry = formattingState.boxGeometry(layoutBox);
            // Inline boxes may or may not be wrapped and have runs on multiple lines (e.g. <span>first line<br>second line<br>third line</span>)
            auto inlineBoxBorderBox = lineBox.logicalBorderBoxForInlineBox(layoutBox, boxGeometry);
            auto inlineBoxSize = LayoutSize { LayoutUnit::fromFloatCeil(inlineBoxBorderBox.width()), LayoutUnit::fromFloatCeil(inlineBoxBorderBox.height()) };
            auto logicalRect = Rect { LayoutPoint { inlineBoxBorderBox.topLeft() }, inlineBoxSize };
            logicalRect.moveBy(LayoutPoint { lineBoxLogicalRect.topLeft() });
            if (inlineBoxStartSet.contains(&layoutBox)) {
                // This inline box showed up first on this line.
                boxGeometry.setLogicalTopLeft(logicalRect.topLeft());
                auto contentBoxHeight = logicalRect.height() - (boxGeometry.verticalBorder() + boxGeometry.verticalPadding().valueOr(0_lu));
                boxGeometry.setContentBoxHeight(contentBoxHeight);
                auto contentBoxWidth = logicalRect.width() - (boxGeometry.horizontalBorder() + boxGeometry.horizontalPadding().valueOr(0_lu));
                boxGeometry.setContentBoxWidth(contentBoxWidth);
                continue;
            }
            // Middle or end of the inline box. Let's stretch the box as needed.
            auto enclosingBorderBoxRect = BoxGeometry::borderBoxRect(boxGeometry);
            enclosingBorderBoxRect.expandToContain(logicalRect);
            boxGeometry.setLogicalLeft(enclosingBorderBoxRect.left());

            boxGeometry.setContentBoxHeight(enclosingBorderBoxRect.height() - (boxGeometry.verticalBorder() + boxGeometry.verticalPadding().valueOr(0_lu)));
            boxGeometry.setContentBoxWidth(enclosingBorderBoxRect.width() - (boxGeometry.horizontalBorder() + boxGeometry.horizontalPadding().valueOr(0_lu)));
        }
    };
    updateBoxGeometryForInlineBoxes();

    auto constructLineGeometry = [&] {
        formattingState.addLine({ lineBoxLogicalRect, enclosingTopAndBottom, lineBox.alignmentBaseline(), lineBox.horizontalAlignmentOffset().valueOr(InlineLayoutUnit { }), lineContent.contentLogicalWidth });
    };
    constructLineGeometry();

    return lineBoxLogicalRect;
}

void InlineFormattingContext::invalidateFormattingState(const InvalidationState&)
{
    // Find out what we need to invalidate. This is where we add some smarts to do partial line layout.
    // For now let's just clear the runs.
    formattingState().clearLineAndRuns();
    // FIXME: This is also where we would delete inline items if their content changed.
}

}
}

#endif
