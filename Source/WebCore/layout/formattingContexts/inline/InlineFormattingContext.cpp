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

#include "BlockLayoutState.h"
#include "FloatingContext.h"
#include "FontCascade.h"
#include "InlineDamage.h"
#include "InlineDisplayBox.h"
#include "InlineDisplayContentBuilder.h"
#include "InlineDisplayLineBuilder.h"
#include "InlineFormattingState.h"
#include "InlineItemsBuilder.h"
#include "InlineLineBox.h"
#include "InlineLineBoxBuilder.h"
#include "InlineLineTypes.h"
#include "InlineTextItem.h"
#include "LayoutBox.h"
#include "LayoutContext.h"
#include "LayoutElementBox.h"
#include "LayoutInitialContainingBlock.h"
#include "LayoutInlineTextBox.h"
#include "LayoutState.h"
#include "Logging.h"
#include "TextUtil.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(InlineFormattingContext);

InlineFormattingContext::InlineFormattingContext(const ElementBox& formattingContextRoot, InlineFormattingState& formattingState, const InlineDamage* lineDamage)
    : FormattingContext(formattingContextRoot, formattingState)
    , m_lineDamage(lineDamage)
    , m_inlineFormattingGeometry(*this)
    , m_inlineFormattingQuirks(*this)
{
}

static inline const Box* nextInlineLevelBoxToLayout(const Box& layoutBox, const ElementBox& stayWithin)
{
    // Atomic inline-level boxes and floats are opaque boxes meaning that they are
    // responsible for their own content (do not need to descend into their subtrees).
    // Only inline boxes may have relevant descendant content.
    if (layoutBox.isInlineBox()) {
        if (is<ElementBox>(layoutBox) && downcast<ElementBox>(layoutBox).hasInFlowOrFloatingChild()) {
            // Anonymous inline text boxes/line breaks can't have descendant content by definition.
            ASSERT(!layoutBox.isInlineTextBox() && !layoutBox.isLineBreakBox());
            return downcast<ElementBox>(layoutBox).firstInFlowOrFloatingChild();
        }
    }

    for (auto* nextInPreOrder = &layoutBox; nextInPreOrder && nextInPreOrder != &stayWithin; nextInPreOrder = &nextInPreOrder->parent()) {
        if (auto* nextSibling = nextInPreOrder->nextInFlowOrFloatingSibling())
            return nextSibling;
    }
    return nullptr;
}

void InlineFormattingContext::layoutInFlowContent(const ConstraintsForInFlowContent& constraints)
{
    LOG_WITH_STREAM(FormattingContextLayout, stream << "[Start] -> inline formatting context -> formatting root(" << &root() << ")");
    ASSERT(root().hasInFlowOrFloatingChild());

    auto* layoutBox = root().firstInFlowOrFloatingChild();
    // 1. Visit each inline box and partially compute their geometry (margins, padding and borders).
    // 2. Collect the inline items (flatten the the layout tree) and place them on lines in bidirectional order. 
    while (layoutBox) {
        ASSERT(layoutBox->isInlineLevelBox() || layoutBox->isFloatingPositioned());

        if (layoutBox->isAtomicInlineLevelBox() || layoutBox->isFloatingPositioned()) {
            // Inline-blocks, inline-tables and replaced elements (img, video) can be sized but not yet positioned.
            if (is<ElementBox>(layoutBox) && layoutBox->establishesFormattingContext()) {
                ASSERT(layoutBox->isInlineBlockBox() || layoutBox->isInlineTableBox() || layoutBox->isFloatingPositioned());
                auto& formattingRoot = downcast<ElementBox>(*layoutBox);
                computeBorderAndPadding(formattingRoot, constraints.horizontal());
                computeWidthAndMargin(formattingRoot, constraints.horizontal());

                if (formattingRoot.hasChild()) {
                    auto formattingContext = LayoutContext::createFormattingContext(formattingRoot, layoutState());
                    if (formattingRoot.hasInFlowOrFloatingChild())
                        formattingContext->layoutInFlowContent(formattingGeometry().constraintsForInFlowContent(formattingRoot));
                    computeHeightAndMargin(formattingRoot, constraints.horizontal());
                    formattingContext->layoutOutOfFlowContent(formattingGeometry().constraintsForOutOfFlowContent(formattingRoot));
                } else
                    computeHeightAndMargin(formattingRoot, constraints.horizontal());
            } else {
                // Replaced and other type of leaf atomic inline boxes.
                computeBorderAndPadding(*layoutBox, constraints.horizontal());
                computeWidthAndMargin(*layoutBox, constraints.horizontal());
                computeHeightAndMargin(*layoutBox, constraints.horizontal());
            }
        } else if (layoutBox->isLineBreakBox()) {
            auto& boxGeometry = formattingState().boxGeometry(*layoutBox);
            boxGeometry.setHorizontalMargin({ });
            boxGeometry.setBorder({ });
            boxGeometry.setPadding({ });
            boxGeometry.setContentBoxWidth({ });
            boxGeometry.setVerticalMargin({ });
        } else if (layoutBox->isInlineBox()) {
            // Text wrapper boxes (anonymous inline level boxes) don't have box geometries (they only generate boxes).
            if (!layoutBox->isInlineTextBox()) {
                // Inline boxes (<span>) can't get sized/positioned yet. At this point we can only compute their margins, borders and padding.
                computeBorderAndPadding(*layoutBox, constraints.horizontal());
                computeHorizontalMargin(*layoutBox, constraints.horizontal());
                formattingState().boxGeometry(*layoutBox).setVerticalMargin({ });
            }
        } else
            ASSERT_NOT_REACHED();

        layoutBox = nextInlineLevelBoxToLayout(*layoutBox, root());
    }

    auto& inlineFormattingState = formattingState();
    inlineFormattingState.clearInlineItems();
    inlineFormattingState.boxes().clear();
    inlineFormattingState.lines().clear();
    auto needsLayoutStartPosition = !m_lineDamage || !m_lineDamage->contentPosition() ? InlineItemPosition() : m_lineDamage->contentPosition()->inlineItemPosition;
    auto needsInlineItemsUpdate = inlineFormattingState.inlineItems().isEmpty() || m_lineDamage;
    if (needsInlineItemsUpdate)
        InlineItemsBuilder { root(), inlineFormattingState }.build(needsLayoutStartPosition);
    // FIXME: Let the caller pass in the block layout state. 
    auto floatingState = FloatingState { layoutState(), FormattingContext::initialContainingBlock(root()) };
    auto blockLayoutState = BlockLayoutState { floatingState, { } };
    auto& inlineItems = inlineFormattingState.inlineItems();
    auto needsLayoutRange = InlineItemRange { needsLayoutStartPosition, { inlineItems.size(), 0 } };

    auto previousLine = [&]() -> std::optional<PreviousLine> {
        auto& displayLines = inlineFormattingState.lines();
        auto& displayBoxes = inlineFormattingState.boxes();
        if (displayLines.isEmpty() || displayBoxes.isEmpty())
            return { };
        auto& lastDisplayBox = displayBoxes.last();
        auto lastLineIndex = displayLines.size() - 1; 
        return PreviousLine { lastLineIndex, { }, lastDisplayBox.isLineBreak(), lastDisplayBox.style().direction(), { } };
    };
    lineLayout(inlineItems, needsLayoutRange, previousLine(), { constraints, { } }, blockLayoutState);
    computeStaticPositionForOutOfFlowContent(inlineFormattingState.outOfFlowBoxes(), { constraints.horizontal().logicalLeft, constraints.logicalTop() });
    InlineDisplayContentBuilder::computeIsFirstIsLastBoxForInlineContent(inlineFormattingState.boxes());
    LOG_WITH_STREAM(FormattingContextLayout, stream << "[End] -> inline formatting context -> formatting root(" << &root() << ")");
}

void InlineFormattingContext::layoutInFlowContentForIntegration(const ConstraintsForInFlowContent& constraints, BlockLayoutState& blockLayoutState)
{
    auto& inlineFormattingState = formattingState();
    auto needsLayoutStartPosition = !m_lineDamage || !m_lineDamage->contentPosition() ? InlineItemPosition() : m_lineDamage->contentPosition()->inlineItemPosition;
    auto needsInlineItemsUpdate = inlineFormattingState.inlineItems().isEmpty() || m_lineDamage;
    if (needsInlineItemsUpdate)
        InlineItemsBuilder { root(), inlineFormattingState }.build(needsLayoutStartPosition);

    auto& inlineItems = inlineFormattingState.inlineItems();
    auto needsLayoutRange = InlineItemRange { needsLayoutStartPosition, { inlineItems.size(), 0 } };
    if (!needsLayoutRange.start)
        inlineFormattingState.boxes().reserveInitialCapacity(inlineItems.size());
    auto previousLine = [&]() -> std::optional<PreviousLine> {
        if (!needsLayoutStartPosition)
            return { };
        if (!m_lineDamage || !m_lineDamage->contentPosition())
            return { };
        auto lastLineIndex = m_lineDamage->contentPosition()->lineIndex - 1;
        // FIXME: We should be able to extract the last line information and provide it to layout as "previous line" (ends in line break and inline direction).
        return PreviousLine { lastLineIndex, { }, { }, { }, { } };
    };

    auto inlineConstraints = downcast<ConstraintsForInlineContent>(constraints);
    lineLayout(inlineItems, needsLayoutRange, previousLine(), inlineConstraints, blockLayoutState);
    computeStaticPositionForOutOfFlowContent(inlineFormattingState.outOfFlowBoxes(), { inlineConstraints.horizontal().logicalLeft, inlineConstraints.logicalTop() });
    InlineDisplayContentBuilder::computeIsFirstIsLastBoxForInlineContent(inlineFormattingState.boxes());
}

IntrinsicWidthConstraints InlineFormattingContext::computedIntrinsicWidthConstraintsForIntegration()
{
    if (formattingState().intrinsicWidthConstraints())
        return *formattingState().intrinsicWidthConstraints();

    auto needsLayoutStartPosition = !m_lineDamage || !m_lineDamage->contentPosition() ? InlineItemPosition() : m_lineDamage->contentPosition()->inlineItemPosition;
    auto needsInlineItemsUpdate = formattingState().inlineItems().isEmpty() || m_lineDamage;
    if (needsInlineItemsUpdate)
        InlineItemsBuilder { root(), formattingState() }.build(needsLayoutStartPosition);

    auto constraints = IntrinsicWidthConstraints {
        ceiledLayoutUnit(computedIntrinsicWidthForConstraint(IntrinsicWidthMode::Minimum)),
        ceiledLayoutUnit(computedIntrinsicWidthForConstraint(IntrinsicWidthMode::Maximum))
    };
    formattingState().setIntrinsicWidthConstraints(constraints);
    return constraints;
}

LayoutUnit InlineFormattingContext::usedContentHeight() const
{
    auto& lines = formattingState().lines();
    // Even empty content generates a line.
    ASSERT(!lines.isEmpty());
    auto top = LayoutUnit { lines.first().top() };
    auto bottom = LayoutUnit { lines.last().bottom() + formattingState().clearGapAfterLastLine() };
    return bottom - top;
}

static InlineItemPosition leadingInlineItemPositionForNextLine(InlineItemPosition lineContentEnd, std::optional<InlineItemPosition> previousLineTrailingInlineItemPosition, InlineItemPosition layoutRangeEnd)
{
    if (!previousLineTrailingInlineItemPosition)
        return lineContentEnd;
    if (previousLineTrailingInlineItemPosition->index < lineContentEnd.index || (previousLineTrailingInlineItemPosition->index == lineContentEnd.index && previousLineTrailingInlineItemPosition->offset < lineContentEnd.offset)) {
        // Either full or partial advancing.
        return lineContentEnd;
    }
    if (previousLineTrailingInlineItemPosition->index == lineContentEnd.index && !previousLineTrailingInlineItemPosition->offset && !lineContentEnd.offset) {
        // Can't mangage to put any content on line (most likely due to floats). Note that this only applies to full content.
        return lineContentEnd;
    }
    // This looks like a partial content and we are stuck. Let's force-move over to the next inline item.
    // We certainly lose some content, but we would be busy looping otherwise.
    ASSERT_NOT_REACHED();
    return { std::min(lineContentEnd.index + 1, layoutRangeEnd.index), { } };
}

void InlineFormattingContext::lineLayout(InlineItems& inlineItems, const InlineItemRange& needsLayoutRange, std::optional<PreviousLine> previousLine, const ConstraintsForInlineContent& constraints, BlockLayoutState& blockLayoutState)
{
    ASSERT(!needsLayoutRange.isEmpty());

    auto floatingContext = FloatingContext { *this, blockLayoutState.floatingState() };
    auto lineLogicalTop = InlineLayoutUnit { constraints.logicalTop() };
    auto previousLineEnd = std::optional<InlineItemPosition> { };
    auto leadingInlineItemPosition = needsLayoutRange.start;

    auto lineBuilder = LineBuilder { *this, blockLayoutState, constraints.horizontal(), inlineItems };
    while (true) {

        auto lineInitialRect = InlineRect { lineLogicalTop, constraints.horizontal().logicalLeft, constraints.horizontal().logicalWidth, formattingGeometry().initialLineHeight(!previousLine.has_value()) };
        auto lineContent = lineBuilder.layoutInlineContent({ { leadingInlineItemPosition, needsLayoutRange.end }, lineInitialRect }, previousLine);

        auto lineIndex = previousLine ? (previousLine->lineIndex + 1) : 0lu;
        auto lineLogicalRect = createDisplayContentForLine(lineIndex, lineContent, constraints, blockLayoutState);
        if (lineContent.isLastLineWithInlineContent)
            formattingState().setClearGapAfterLastLine(formattingGeometry().logicalTopForNextLine(lineContent, lineLogicalRect, floatingContext) - lineLogicalRect.bottom());

        auto lineContentEnd = lineContent.committedRange.end;
        leadingInlineItemPosition = leadingInlineItemPositionForNextLine(lineContentEnd, previousLineEnd, needsLayoutRange.end);
        auto isLastLine = leadingInlineItemPosition == needsLayoutRange.end && lineContent.overflowingFloats.isEmpty();
        if (isLastLine)
            break;

        lineLogicalTop = formattingGeometry().logicalTopForNextLine(lineContent, lineLogicalRect, floatingContext);
        previousLine = PreviousLine { lineIndex, lineContent.trailingOverflowingContentWidth, !lineContent.runs.isEmpty() && lineContent.runs.last().isLineBreak(), lineContent.inlineBaseDirection, WTFMove(lineContent.overflowingFloats) };
        previousLineEnd = lineContentEnd;
    }
}

void InlineFormattingContext::computeStaticPositionForOutOfFlowContent(const FormattingState::OutOfFlowBoxList& outOfFlowBoxes, LayoutPoint contentBoxTopLeft)
{
    // This function computes the static position for out-of-flow content inside the inline formatting context.
    // As per spec, the static position of an out-of-flow box is computed as if the position was set to static.
    // However it does not mean that the out-of-flow box should be involved in the inline layout process.
    // Instead we figure out this static position after the inline layout by looking at the previous/next sibling (or parent) box's geometry and
    // place the out-of-flow box at the logical right position.
    auto& formattingGeometry = this->formattingGeometry();
    auto& formattingState = this->formattingState();

    for (auto& outOfFlowBox : outOfFlowBoxes) {
        if (outOfFlowBox->style().isOriginalDisplayInlineType()) {
            formattingState.boxGeometry(outOfFlowBox).setLogicalTopLeft(formattingGeometry.staticPositionForOutOfFlowInlineLevelBox(outOfFlowBox, contentBoxTopLeft));
            continue;
        }
        formattingState.boxGeometry(outOfFlowBox).setLogicalTopLeft(formattingGeometry.staticPositionForOutOfFlowBlockLevelBox(outOfFlowBox, contentBoxTopLeft));
    }
}

IntrinsicWidthConstraints InlineFormattingContext::computedIntrinsicWidthConstraints()
{
    auto& formattingState = this->formattingState();
    if (formattingState.intrinsicWidthConstraints())
        return *formattingState.intrinsicWidthConstraints();

    if (!root().hasInFlowOrFloatingChild()) {
        auto constraints = formattingGeometry().constrainByMinMaxWidth(root(), { });
        formattingState.setIntrinsicWidthConstraints(constraints);
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

    auto needsLayoutStartPosition = !m_lineDamage || !m_lineDamage->contentPosition() ? InlineItemPosition() : m_lineDamage->contentPosition()->inlineItemPosition;
    auto needsInlineItemsUpdate = formattingState.inlineItems().isEmpty() || m_lineDamage;
    if (needsInlineItemsUpdate)
        InlineItemsBuilder { root(), formattingState }.build(needsLayoutStartPosition);

    auto maximumLineWidth = [&](auto intrinsicWidthMode) {
        // Switch to the min/max formatting root width values before formatting the lines.
        for (auto* formattingRoot : formattingContextRootList) {
            auto intrinsicWidths = formattingState.intrinsicWidthConstraintsForBox(*formattingRoot);
            auto& boxGeometry = formattingState.boxGeometry(*formattingRoot);
            auto contentWidth = (intrinsicWidthMode == IntrinsicWidthMode::Maximum ? intrinsicWidths->maximum : intrinsicWidths->minimum) - boxGeometry.horizontalMarginBorderAndPadding();
            boxGeometry.setContentBoxWidth(contentWidth);
        }
        return computedIntrinsicWidthForConstraint(intrinsicWidthMode);
    };

    auto minimumContentWidth = ceiledLayoutUnit(maximumLineWidth(IntrinsicWidthMode::Minimum));
    auto maximumContentWidth = ceiledLayoutUnit(maximumLineWidth(IntrinsicWidthMode::Maximum));
    auto constraints = formattingGeometry().constrainByMinMaxWidth(root(), { minimumContentWidth, maximumContentWidth });
    formattingState.setIntrinsicWidthConstraints(constraints);
    return constraints;
}

InlineLayoutUnit InlineFormattingContext::computedIntrinsicWidthForConstraint(IntrinsicWidthMode intrinsicWidthMode) const
{
    auto& inlineItems = formattingState().inlineItems();
    auto lineBuilder = LineBuilder { *this, inlineItems, intrinsicWidthMode };
    auto layoutRange = InlineItemRange { 0 , inlineItems.size() };
    auto maximumLineWidth = InlineLayoutUnit { };
    auto maximumFloatWidth = LayoutUnit { };
    auto previousLineEnd = std::optional<InlineItemPosition> { };
    auto previousLine = std::optional<PreviousLine> { };
    auto lineIndex = 0lu;

    while (!layoutRange.isEmpty()) {
        auto intrinsicContent = lineBuilder.computedIntrinsicWidth(layoutRange, previousLine);
        maximumLineWidth = std::max(maximumLineWidth, intrinsicContent.contentLogicalWidth);

        layoutRange.start = leadingInlineItemPositionForNextLine(intrinsicContent.committedRange.end, previousLineEnd, layoutRange.end);
        previousLineEnd = layoutRange.start;
        previousLine = PreviousLine { lineIndex++, intrinsicContent.trailingOverflowingContentWidth, { }, { }, { } };

        // FIXME: Add support for clear.
        for (auto* inlineFloatItem : intrinsicContent.placedFloats)
            maximumFloatWidth += geometryForBox(inlineFloatItem->layoutBox()).marginBoxWidth();
    }
    return maximumLineWidth + maximumFloatWidth;
}

void InlineFormattingContext::computeIntrinsicWidthForFormattingRoot(const Box& formattingRoot)
{
    ASSERT(formattingRoot.establishesFormattingContext());
    auto constraints = IntrinsicWidthConstraints { };
    if (auto fixedWidth = formattingGeometry().fixedValue(formattingRoot.style().logicalWidth()))
        constraints = { *fixedWidth, *fixedWidth };
    else {
        auto hasInflowOrFloatingContent = is<ElementBox>(formattingRoot) && downcast<ElementBox>(formattingRoot).hasInFlowOrFloatingChild();
        // The intrinsic sizes of the size containment box are determined as if the element had no content.
        auto shouldIgnoreChildContent = formattingRoot.isSizeContainmentBox();
        if (hasInflowOrFloatingContent && !shouldIgnoreChildContent)
            constraints = LayoutContext::createFormattingContext(downcast<ElementBox>(formattingRoot), layoutState())->computedIntrinsicWidthConstraints();
    }
    constraints = formattingGeometry().constrainByMinMaxWidth(formattingRoot, constraints);
    constraints.expand(geometryForBox(formattingRoot).horizontalMarginBorderAndPadding());
    formattingState().setIntrinsicWidthConstraintsForBox(formattingRoot, constraints);
}

void InlineFormattingContext::computeHorizontalMargin(const Box& layoutBox, const HorizontalConstraints& horizontalConstraints)
{
    auto computedHorizontalMargin = formattingGeometry().computedHorizontalMargin(layoutBox, horizontalConstraints);
    formattingState().boxGeometry(layoutBox).setHorizontalMargin({ computedHorizontalMargin.start.value_or(0), computedHorizontalMargin.end.value_or(0) });
}

void InlineFormattingContext::computeWidthAndMargin(const Box& layoutBox, const HorizontalConstraints& horizontalConstraints)
{
    auto compute = [&](std::optional<LayoutUnit> usedWidth) {
        if (layoutBox.isFloatingPositioned())
            return formattingGeometry().floatingContentWidthAndMargin(layoutBox, horizontalConstraints, { usedWidth, { } });
        if (layoutBox.isInlineBlockBox())
            return formattingGeometry().inlineBlockContentWidthAndMargin(layoutBox, horizontalConstraints, { usedWidth, { } });
        if (layoutBox.isReplacedBox())
            return formattingGeometry().inlineReplacedContentWidthAndMargin(downcast<ElementBox>(layoutBox), horizontalConstraints, { }, { usedWidth, { } });
        ASSERT_NOT_REACHED();
        return ContentWidthAndMargin { };
    };

    auto contentWidthAndMargin = compute({ });

    auto availableWidth = horizontalConstraints.logicalWidth;
    if (auto maxWidth = formattingGeometry().computedMaxWidth(layoutBox, availableWidth)) {
        auto maxWidthAndMargin = compute(maxWidth);
        if (contentWidthAndMargin.contentWidth > maxWidthAndMargin.contentWidth)
            contentWidthAndMargin = maxWidthAndMargin;
    }

    auto minWidth = formattingGeometry().computedMinWidth(layoutBox, availableWidth).value_or(0);
    auto minWidthAndMargin = compute(minWidth);
    if (contentWidthAndMargin.contentWidth < minWidthAndMargin.contentWidth)
        contentWidthAndMargin = minWidthAndMargin;

    auto& boxGeometry = formattingState().boxGeometry(layoutBox);
    boxGeometry.setContentBoxWidth(contentWidthAndMargin.contentWidth);
    boxGeometry.setHorizontalMargin({ contentWidthAndMargin.usedMargin.start, contentWidthAndMargin.usedMargin.end });
}

void InlineFormattingContext::computeHeightAndMargin(const Box& layoutBox, const HorizontalConstraints& horizontalConstraints)
{
    auto compute = [&](std::optional<LayoutUnit> usedHeight) {
        if (layoutBox.isFloatingPositioned())
            return formattingGeometry().floatingContentHeightAndMargin(layoutBox, horizontalConstraints, { usedHeight });
        if (layoutBox.isInlineBlockBox())
            return formattingGeometry().inlineBlockContentHeightAndMargin(layoutBox, horizontalConstraints, { usedHeight });
        if (layoutBox.isReplacedBox())
            return formattingGeometry().inlineReplacedContentHeightAndMargin(downcast<ElementBox>(layoutBox), horizontalConstraints, { }, { usedHeight });
        ASSERT_NOT_REACHED();
        return ContentHeightAndMargin { };
    };

    auto contentHeightAndMargin = compute({ });
    if (auto maxHeight = formattingGeometry().computedMaxHeight(layoutBox)) {
        auto maxHeightAndMargin = compute(maxHeight);
        if (contentHeightAndMargin.contentHeight > maxHeightAndMargin.contentHeight)
            contentHeightAndMargin = maxHeightAndMargin;
    }

    if (auto minHeight = formattingGeometry().computedMinHeight(layoutBox)) {
        auto minHeightAndMargin = compute(minHeight);
        if (contentHeightAndMargin.contentHeight < minHeightAndMargin.contentHeight)
            contentHeightAndMargin = minHeightAndMargin;
    }
    auto& boxGeometry = formattingState().boxGeometry(layoutBox);
    boxGeometry.setContentBoxHeight(contentHeightAndMargin.contentHeight);
    boxGeometry.setVerticalMargin({ contentHeightAndMargin.nonCollapsedMargin.before, contentHeightAndMargin.nonCollapsedMargin.after });
}

static LineEndingEllipsisPolicy lineEndingEllipsisPolicy(const RenderStyle& rootStyle, size_t numberOfLines, std::optional<size_t> maximumNumberOfVisibleLines)
{
    // We may have passed the line-clamp line with overflow visible.
    if (maximumNumberOfVisibleLines && numberOfLines < *maximumNumberOfVisibleLines) {
        // If the next call to layoutInlineContent() won't produce a line with content (e.g. only floats), we'll end up here again.
        auto shouldApplyClampWhenApplicable = *maximumNumberOfVisibleLines - numberOfLines == 1;
        if (shouldApplyClampWhenApplicable)
            return LineEndingEllipsisPolicy::WhenContentOverflowsInBlockDirection;
    }
    // Truncation is in effect when the block container has overflow other than visible.
    if (rootStyle.overflowX() == Overflow::Hidden && rootStyle.textOverflow() == TextOverflow::Ellipsis)
        return LineEndingEllipsisPolicy::WhenContentOverflowsInInlineDirection;
    return LineEndingEllipsisPolicy::No;
}

InlineRect InlineFormattingContext::createDisplayContentForLine(size_t lineIndex, const LineBuilder::LineContent& lineContent, const ConstraintsForInlineContent& constraints, const BlockLayoutState& blockLayoutState)
{
    auto maximumNumberOfVisibleLinesForThisInlineContent = [&] () -> std::optional<size_t> {
        if (auto lineClamp = blockLayoutState.lineClamp())
            return lineClamp->maximumNumberOfLines - lineClamp->numberOfVisibleLines;
        return { };
    }();

    auto& formattingState = this->formattingState();
    auto lineBox = LineBoxBuilder { *this, lineContent, blockLayoutState }.build(lineIndex);
    auto displayLine = InlineDisplayLineBuilder { *this }.build(lineContent, lineBox, constraints);
    auto boxes = InlineDisplayContentBuilder { *this, formattingState }.build(lineContent, lineBox, displayLine, lineIndex);
    auto ellipsisPolicy = lineEndingEllipsisPolicy(root().style(), lineIndex, maximumNumberOfVisibleLinesForThisInlineContent);
    if (auto ellipsisRect = InlineDisplayLineBuilder::trailingEllipsisVisualRectAfterTruncation(ellipsisPolicy, displayLine, boxes, lineContent.isLastLineWithInlineContent))
        displayLine.setEllipsisVisualRect(*ellipsisRect);

    formattingState.addBoxes(WTFMove(boxes));
    formattingState.addLine(displayLine);

    return InlineFormattingGeometry::flipVisualRectToLogicalForWritingMode(formattingState.lines().last().lineBoxRect(), root().style().writingMode());
}

void InlineFormattingContext::resetGeometryForClampedContent(const InlineItemRange& needsDisplayContentRange, const LineBuilder::FloatList& overflowingFloats, LayoutPoint topleft)
{
    if (needsDisplayContentRange.isEmpty() && overflowingFloats.isEmpty())
        return;

    auto& inlineItems = formattingState().inlineItems();
    for (size_t index = needsDisplayContentRange.startIndex(); index < needsDisplayContentRange.endIndex(); ++index) {
        auto& inlineItem = inlineItems[index];
        auto hasBoxGeometry = inlineItem.isBox() || inlineItem.isFloat() || inlineItem.isHardLineBreak() || inlineItem.isInlineBoxStart();
        if (!hasBoxGeometry)
            continue;
        auto& boxGeometry = formattingState().boxGeometry(inlineItem.layoutBox());
        boxGeometry.setLogicalTopLeft(topleft);
        boxGeometry.setContentBoxHeight({ });
        boxGeometry.setContentBoxWidth({ });
    }
}

}
}

