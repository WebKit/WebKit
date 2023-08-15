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

#include "FloatingContext.h"
#include "FontCascade.h"
#include "InlineDamage.h"
#include "InlineDisplayBox.h"
#include "InlineDisplayContentBuilder.h"
#include "InlineDisplayLineBuilder.h"
#include "InlineFormattingState.h"
#include "InlineItemsBuilder.h"
#include "InlineLayoutState.h"
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
#include "RenderStyleInlines.h"
#include "TextOnlyLineBuilder.h"
#include "TextUtil.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(InlineFormattingContext);

InlineFormattingContext::InlineFormattingContext(const ElementBox& formattingContextRoot, InlineFormattingState& formattingState)
    : FormattingContext(formattingContextRoot, formattingState)
    , m_inlineFormattingGeometry(*this)
    , m_inlineFormattingQuirks(*this)
{
}

InlineLayoutResult InlineFormattingContext::layoutInFlowAndFloatContent(const ConstraintsForInlineContent& constraints, InlineLayoutState& inlineLayoutState, const InlineDamage* lineDamage)
{
    auto& floatingState = inlineLayoutState.parentBlockLayoutState().floatingState();
    if (!root().hasInFlowChild()) {
        // Float and/or out-of-flow only content does not support partial layout.
        ASSERT(!lineDamage);
        layoutFloatContentOnly(constraints, floatingState);
        return { { }, InlineLayoutResult::Range::Full };
    }

    auto& inlineFormattingState = formattingState();
    auto needsLayoutStartPosition = !lineDamage || !lineDamage->start() ? InlineItemPosition() : lineDamage->start()->inlineItemPosition;
    auto needsInlineItemsUpdate = inlineFormattingState.inlineItems().isEmpty() || lineDamage;
    if (needsInlineItemsUpdate) {
        // FIXME: This shoul go to invalidation.
        m_maximumIntrinsicWidthResultForSingleLine = { };
        InlineItemsBuilder { root(), inlineFormattingState }.build(needsLayoutStartPosition);
    }

    auto& inlineItems = inlineFormattingState.inlineItems();
    auto needsLayoutRange = InlineItemRange { needsLayoutStartPosition, { inlineItems.size(), 0 } };

    auto previousLine = [&]() -> std::optional<PreviousLine> {
        if (!needsLayoutStartPosition)
            return { };
        if (!lineDamage || !lineDamage->start())
            return { };
        auto lastLineIndex = lineDamage->start()->lineIndex - 1;
        // FIXME: We should be able to extract the last line information and provide it to layout as "previous line" (ends in line break and inline direction).
        return PreviousLine { lastLineIndex, { }, { }, { }, { } };
    };

    if (TextOnlyLineBuilder::isEligibleForSimplifiedTextOnlyInlineLayout(root(), formattingState(), &floatingState)) {
        auto simplifiedLineBuilder = TextOnlyLineBuilder { *this, constraints.horizontal(), inlineItems };
        return lineLayout(simplifiedLineBuilder, inlineItems, needsLayoutRange, previousLine(), constraints, inlineLayoutState, lineDamage);
    }
    auto lineBuilder = LineBuilder { *this, inlineLayoutState, floatingState, constraints.horizontal(), inlineItems };
    return lineLayout(lineBuilder, inlineItems, needsLayoutRange, previousLine(), constraints, inlineLayoutState, lineDamage);
}

void InlineFormattingContext::layoutOutOfFlowContent(const ConstraintsForInlineContent& constraints, InlineLayoutState& inlineLayoutState, const InlineDisplay::Content& inlineDisplayContent)
{
    // Collecting out-of-flow boxes happens during the in-flow phase.
    computeStaticPositionForOutOfFlowContent(formattingState().outOfFlowBoxes(), constraints, inlineDisplayContent, inlineLayoutState.parentBlockLayoutState().floatingState());
}

IntrinsicWidthConstraints InlineFormattingContext::computedIntrinsicSizes(const InlineDamage* lineDamage)
{
    auto& inlineFormattingState = formattingState();
    if (lineDamage)
        inlineFormattingState.resetIntrinsicWidthConstraints();

    if (auto intrinsicSizes = inlineFormattingState.intrinsicWidthConstraints())
        return *intrinsicSizes;

    auto needsLayoutStartPosition = !lineDamage || !lineDamage->start() ? InlineItemPosition() : lineDamage->start()->inlineItemPosition;
    auto needsInlineItemsUpdate = inlineFormattingState.inlineItems().isEmpty() || lineDamage;
    if (needsInlineItemsUpdate)
        InlineItemsBuilder { root(), inlineFormattingState }.build(needsLayoutStartPosition);

    auto computedIntrinsicValue = [&](auto intrinsicWidthMode, auto& inlineBuilder, MayCacheLayoutResult mayCacheLayoutResult = MayCacheLayoutResult::No) {
        inlineBuilder.setIntrinsicWidthMode(intrinsicWidthMode);
        return ceiledLayoutUnit(computedIntrinsicWidthForConstraint(intrinsicWidthMode, inlineBuilder, mayCacheLayoutResult));
    };

    auto intrinsicSizes = IntrinsicWidthConstraints { };
    if (TextOnlyLineBuilder::isEligibleForSimplifiedTextOnlyInlineLayout(root(), inlineFormattingState)) {
        auto simplifiedLineBuilder = TextOnlyLineBuilder { *this, { }, inlineFormattingState.inlineItems() };
        intrinsicSizes = { computedIntrinsicValue(IntrinsicWidthMode::Minimum, simplifiedLineBuilder), computedIntrinsicValue(IntrinsicWidthMode::Maximum, simplifiedLineBuilder, MayCacheLayoutResult::Yes) };
    } else {
        auto floatingState = FloatingState { root() };
        auto parentBlockLayoutState = BlockLayoutState { floatingState, { } };
        auto inlineLayoutState = InlineLayoutState { parentBlockLayoutState, { } };
        auto lineBuilder = LineBuilder { *this, inlineLayoutState, floatingState, { }, inlineFormattingState.inlineItems() };
        intrinsicSizes = { computedIntrinsicValue(IntrinsicWidthMode::Minimum, lineBuilder), computedIntrinsicValue(IntrinsicWidthMode::Maximum, lineBuilder) };
    }
    formattingState().setIntrinsicWidthConstraints(intrinsicSizes);
    return intrinsicSizes;
}

LayoutUnit InlineFormattingContext::maximumContentSize()
{
    auto& inlineFormattingState = formattingState();
    if (auto intrinsicWidthConstraints = inlineFormattingState.intrinsicWidthConstraints())
        return intrinsicWidthConstraints->maximum;

    if (inlineFormattingState.inlineItems().isEmpty())
        InlineItemsBuilder { root(), inlineFormattingState }.build({ });

    auto floatingState = FloatingState { root() };
    auto parentBlockLayoutState = BlockLayoutState { floatingState, { } };
    auto inlineLayoutState = InlineLayoutState { parentBlockLayoutState, { } };
    auto lineBuilder = LineBuilder { *this, inlineLayoutState, floatingState, { }, inlineFormattingState.inlineItems() };
    lineBuilder.setIntrinsicWidthMode(IntrinsicWidthMode::Maximum);

    return ceiledLayoutUnit(computedIntrinsicWidthForConstraint(IntrinsicWidthMode::Maximum, lineBuilder));
}

static bool mayExitFromPartialLayout(const InlineDamage& lineDamage, size_t lineIndex, const InlineDisplay::Boxes& newContent)
{
    if (lineDamage.start()->lineIndex == lineIndex) {
        // Never stop at the damaged line. Adding trailing overflowing content could easily produce the
        // same set of display boxes for the first damaged line.
        return false;
    }
    auto trailingContentFromPreviousLayout = lineDamage.trailingContentForLine(lineIndex);
    return trailingContentFromPreviousLayout ? (!newContent.isEmpty() && *trailingContentFromPreviousLayout == newContent.last()) : false;
}

InlineLayoutResult InlineFormattingContext::lineLayout(AbstractLineBuilder& lineBuilder, const InlineItems& inlineItems, InlineItemRange needsLayoutRange, std::optional<PreviousLine> previousLine, const ConstraintsForInlineContent& constraints, InlineLayoutState& inlineLayoutState, const InlineDamage* lineDamage)
{
    ASSERT(!needsLayoutRange.isEmpty());

    auto layoutResult = InlineLayoutResult { };
    if (!needsLayoutRange.start)
        layoutResult.displayContent.boxes.reserveInitialCapacity(inlineItems.size());

    auto isPartialLayout = lineDamage && lineDamage->start();
    if (!isPartialLayout && createDisplayContentForLineFromCachedContent(constraints, inlineLayoutState, layoutResult)) {
        ASSERT(!previousLine);
        layoutResult.range = InlineLayoutResult::Range::Full;
        return layoutResult;
    }

    auto floatingContext = FloatingContext { *this, inlineLayoutState.parentBlockLayoutState().floatingState() };
    auto lineLogicalTop = InlineLayoutUnit { constraints.logicalTop() };
    auto previousLineEnd = std::optional<InlineItemPosition> { };
    auto leadingInlineItemPosition = needsLayoutRange.start;
    while (true) {

        auto lineInitialRect = InlineRect { lineLogicalTop, constraints.horizontal().logicalLeft, constraints.horizontal().logicalWidth, formattingGeometry().initialLineHeight(!previousLine.has_value()) };
        auto lineInput = LineInput { { leadingInlineItemPosition, needsLayoutRange.end }, lineInitialRect };
        auto lineLayoutResult = lineBuilder.layoutInlineContent(lineInput, previousLine);

        auto lineIndex = previousLine ? (previousLine->lineIndex + 1) : 0lu;
        auto lineLogicalRect = createDisplayContentForLine(lineIndex, lineLayoutResult, constraints, inlineLayoutState, layoutResult.displayContent);

        if (auto firstLineGap = lineLayoutResult.lineGeometry.initialLetterClearGap) {
            ASSERT(!inlineLayoutState.clearGapBeforeFirstLine());
            inlineLayoutState.setClearGapBeforeFirstLine(*firstLineGap);
        }

        if (lineLayoutResult.isFirstLast.isLastLineWithInlineContent)
            inlineLayoutState.setClearGapAfterLastLine(formattingGeometry().logicalTopForNextLine(lineLayoutResult, lineLogicalRect, floatingContext) - lineLogicalRect.bottom());

        auto lineContentEnd = lineLayoutResult.inlineItemRange.end;
        leadingInlineItemPosition = InlineFormattingGeometry::leadingInlineItemPositionForNextLine(lineContentEnd, previousLineEnd, needsLayoutRange.end);
        auto isLastLine = leadingInlineItemPosition == needsLayoutRange.end && lineLayoutResult.floatContent.suspendedFloats.isEmpty();
        if (isLastLine) {
            layoutResult.range = !isPartialLayout ? InlineLayoutResult::Range::Full : InlineLayoutResult::Range::FullFromDamage;
            break;
        }
        if (isPartialLayout && mayExitFromPartialLayout(*lineDamage, lineIndex, layoutResult.displayContent.boxes)) {
            layoutResult.range = InlineLayoutResult::Range::PartialFromDamage;
            break;
        }

        lineLogicalTop = formattingGeometry().logicalTopForNextLine(lineLayoutResult, lineLogicalRect, floatingContext);
        previousLine = PreviousLine { lineIndex, lineLayoutResult.contentGeometry.trailingOverflowingContentWidth, !lineLayoutResult.inlineContent.isEmpty() && lineLayoutResult.inlineContent.last().isLineBreak(), lineLayoutResult.directionality.inlineBaseDirection, WTFMove(lineLayoutResult.floatContent.suspendedFloats) };
        previousLineEnd = lineContentEnd;
    }
    return layoutResult;
}

void InlineFormattingContext::layoutFloatContentOnly(const ConstraintsForInlineContent& constraints, FloatingState& floatingState)
{
    ASSERT(!root().hasInFlowChild());

    auto& inlineFormattingState = formattingState();
    auto floatingContext = FloatingContext { *this, floatingState };

    InlineItemsBuilder { root(), inlineFormattingState }.build({ });

    for (auto& inlineItem : inlineFormattingState.inlineItems()) {
        if (!inlineItem.isFloat()) {
            ASSERT_NOT_REACHED();
            continue;
        }
        auto& floatBox = inlineItem.layoutBox();
        auto& floatBoxGeometry = inlineFormattingState.boxGeometry(floatBox);
        auto staticPosition = LayoutPoint { constraints.horizontal().logicalLeft, constraints.logicalTop() };
        staticPosition.move(floatBoxGeometry.marginStart(), floatBoxGeometry.marginBefore());
        floatBoxGeometry.setLogicalTopLeft(staticPosition);

        auto floatBoxTopLeft = floatingContext.positionForFloat(floatBox, floatBoxGeometry, constraints.horizontal());
        floatBoxGeometry.setLogicalTopLeft(floatBoxTopLeft);
        floatingState.append(floatingContext.toFloatItem(floatBox, floatBoxGeometry));
    }
}

void InlineFormattingContext::computeStaticPositionForOutOfFlowContent(const FormattingState::OutOfFlowBoxList& outOfFlowBoxes, const ConstraintsForInFlowContent& constraints, const InlineDisplay::Content& displayContent, const FloatingState& floatingState)
{
    // This function computes the static position for out-of-flow content inside the inline formatting context.
    // As per spec, the static position of an out-of-flow box is computed as if the position was set to static.
    // However it does not mean that the out-of-flow box should be involved in the inline layout process.
    // Instead we figure out this static position after the inline layout by looking at the previous/next sibling (or parent) box's geometry and
    // place the out-of-flow box at the logical right position.
    auto& formattingGeometry = this->formattingGeometry();
    auto& formattingState = this->formattingState();
    auto floatingContext = FloatingContext { *this, floatingState };

    for (auto& outOfFlowBox : outOfFlowBoxes) {
        if (outOfFlowBox->style().isOriginalDisplayInlineType()) {
            formattingState.boxGeometry(outOfFlowBox).setLogicalTopLeft(formattingGeometry.staticPositionForOutOfFlowInlineLevelBox(outOfFlowBox, constraints, displayContent, floatingContext));
            continue;
        }
        formattingState.boxGeometry(outOfFlowBox).setLogicalTopLeft(formattingGeometry.staticPositionForOutOfFlowBlockLevelBox(outOfFlowBox, constraints, displayContent));
    }
}

InlineLayoutUnit InlineFormattingContext::computedIntrinsicWidthForConstraint(IntrinsicWidthMode intrinsicWidthMode, AbstractLineBuilder& lineBuilder, MayCacheLayoutResult mayCacheLayoutResult)
{
    auto horizontalConstraints = HorizontalConstraints { };
    if (intrinsicWidthMode == IntrinsicWidthMode::Maximum)
        horizontalConstraints.logicalWidth = maxInlineLayoutUnit();
    auto& inlineItems = formattingState().inlineItems();
    auto layoutRange = InlineItemRange { 0 , inlineItems.size() };
    auto maximumContentWidth = InlineLayoutUnit { };
    auto previousLineEnd = std::optional<InlineItemPosition> { };
    auto previousLine = std::optional<PreviousLine> { };
    auto lineIndex = 0lu;

    while (true) {
        auto lineLayoutResult = lineBuilder.layoutInlineContent({ layoutRange, { 0.f, 0.f, horizontalConstraints.logicalWidth, 0.f } }, previousLine);
        auto floatContentWidth = [&] {
            auto leftWidth = LayoutUnit { };
            auto rightWidth = LayoutUnit { };
            for (auto& floatItem : lineLayoutResult.floatContent.placedFloats) {
                mayCacheLayoutResult = MayCacheLayoutResult::No;
                auto marginBoxRect = BoxGeometry::marginBoxRect(floatItem.boxGeometry());
                if (floatItem.isLeftPositioned())
                    leftWidth = std::max(leftWidth, marginBoxRect.right());
                else
                    rightWidth = std::max(rightWidth, horizontalConstraints.logicalWidth - marginBoxRect.left());
            }
            return InlineLayoutUnit { leftWidth + rightWidth };
        };
        maximumContentWidth = std::max(maximumContentWidth, lineLayoutResult.lineGeometry.logicalTopLeft.x() + lineLayoutResult.contentGeometry.logicalWidth + floatContentWidth());

        layoutRange.start = InlineFormattingGeometry::leadingInlineItemPositionForNextLine(lineLayoutResult.inlineItemRange.end, previousLineEnd, layoutRange.end);
        if (layoutRange.isEmpty()) {
            auto shouldCacheLineBreakingResultForSubsequentLayout = !lineIndex && mayCacheLayoutResult == MayCacheLayoutResult::Yes;
            if (shouldCacheLineBreakingResultForSubsequentLayout)
                m_maximumIntrinsicWidthResultForSingleLine = LineBreakingResultForConstraint { ceiledLayoutUnit(maximumContentWidth), WTFMove(lineLayoutResult) };
            break;
        }

        previousLineEnd = layoutRange.start;
        previousLine = PreviousLine { lineIndex++, lineLayoutResult.contentGeometry.trailingOverflowingContentWidth, { }, { }, WTFMove(lineLayoutResult.floatContent.suspendedFloats) };
    }
    return maximumContentWidth;
}

static LineEndingEllipsisPolicy lineEndingEllipsisPolicy(const RenderStyle& rootStyle, size_t numberOfLines, std::optional<size_t> numberOfVisibleLinesAllowed)
{
    // We may have passed the line-clamp line with overflow visible.
    if (numberOfVisibleLinesAllowed && numberOfLines < *numberOfVisibleLinesAllowed) {
        // If the next call to layoutInlineContent() won't produce a line with content (e.g. only floats), we'll end up here again.
        auto shouldApplyClampWhenApplicable = *numberOfVisibleLinesAllowed - numberOfLines == 1;
        if (shouldApplyClampWhenApplicable)
            return LineEndingEllipsisPolicy::WhenContentOverflowsInBlockDirection;
    }
    // Truncation is in effect when the block container has overflow other than visible.
    if (rootStyle.overflowX() != Overflow::Visible && rootStyle.textOverflow() == TextOverflow::Ellipsis)
        return LineEndingEllipsisPolicy::WhenContentOverflowsInInlineDirection;
    return LineEndingEllipsisPolicy::No;
}

InlineRect InlineFormattingContext::createDisplayContentForLine(size_t lineIndex, const LineLayoutResult& lineLayoutResult, const ConstraintsForInlineContent& constraints, const InlineLayoutState& inlineLayoutState, InlineDisplay::Content& displayContent)
{
    auto numberOfVisibleLinesAllowed = [&] () -> std::optional<size_t> {
        if (auto lineClamp = inlineLayoutState.parentBlockLayoutState().lineClamp())
            return lineClamp->maximumLineCount > lineClamp->currentLineCount ? lineClamp->maximumLineCount - lineClamp->currentLineCount : 0;
        return { };
    }();

    auto ellipsisPolicy = lineEndingEllipsisPolicy(root().style(), lineIndex, numberOfVisibleLinesAllowed);
    auto lineIsFullyTruncatedInBlockDirection = numberOfVisibleLinesAllowed && lineIndex + 1 > *numberOfVisibleLinesAllowed;
    auto lineBox = LineBoxBuilder { *this, inlineLayoutState, lineLayoutResult }.build(lineIndex);
    auto displayLine = InlineDisplayLineBuilder { *this }.build(lineLayoutResult, lineBox, constraints, lineIsFullyTruncatedInBlockDirection);
    auto boxes = InlineDisplayContentBuilder { *this, formattingState(), displayLine, lineIndex }.build(lineLayoutResult, lineBox);
    if (auto ellipsisRect = InlineDisplayLineBuilder::trailingEllipsisVisualRectAfterTruncation(ellipsisPolicy, displayLine, boxes, lineLayoutResult.isFirstLast.isLastLineWithInlineContent))
        displayLine.setEllipsisVisualRect(*ellipsisRect);

    displayContent.boxes.appendVector(WTFMove(boxes));
    displayContent.lines.append(displayLine);
    auto updateBoxGeometryForPlacedFloats = [&] {
        for (auto& floatItem : lineLayoutResult.floatContent.placedFloats) {
            if (!floatItem.layoutBox()) {
                ASSERT_NOT_REACHED();
                // We should not be placing intrusive floats coming from parent BFC.
                continue;
            }
            auto& boxGeometry = formattingState().boxGeometry(*floatItem.layoutBox());
            boxGeometry.setLogicalTopLeft(BoxGeometry::borderBoxTopLeft(floatItem.boxGeometry()));
        }
    };
    updateBoxGeometryForPlacedFloats();

    return InlineFormattingGeometry::flipVisualRectToLogicalForWritingMode(displayContent.lines.last().lineBoxRect(), root().style().writingMode());
}

void InlineFormattingContext::resetGeometryForClampedContent(const InlineItemRange& needsDisplayContentRange, const LineLayoutResult::SuspendedFloatList& suspendedFloats, LayoutPoint topleft)
{
    if (needsDisplayContentRange.isEmpty() && suspendedFloats.isEmpty())
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

bool InlineFormattingContext::createDisplayContentForLineFromCachedContent(const ConstraintsForInlineContent& constraints, const InlineLayoutState& inlineLayoutState, InlineLayoutResult& layoutResult)
{
    if (!m_maximumIntrinsicWidthResultForSingleLine)
        return false;
    if (m_maximumIntrinsicWidthResultForSingleLine->constraint > constraints.horizontal().logicalWidth) {
        m_maximumIntrinsicWidthResultForSingleLine = { };
        return false;
    }
    if (!inlineLayoutState.parentBlockLayoutState().floatingState().isEmpty()) {
        m_maximumIntrinsicWidthResultForSingleLine = { };
        return false;
    }
    auto& lineBreakingResult = m_maximumIntrinsicWidthResultForSingleLine->result;
    lineBreakingResult.lineGeometry.logicalTopLeft = { constraints.horizontal().logicalLeft, constraints.logicalTop() };
    lineBreakingResult.lineGeometry.logicalWidth = constraints.horizontal().logicalWidth;
    lineBreakingResult.contentGeometry.logicalLeft = InlineFormattingGeometry::horizontalAlignmentOffset(root().style(), lineBreakingResult.contentGeometry.logicalWidth, lineBreakingResult.lineGeometry.logicalWidth, lineBreakingResult.hangingContent.logicalWidth, lineBreakingResult.inlineContent, true);
    createDisplayContentForLine(0, lineBreakingResult, constraints, inlineLayoutState, layoutResult.displayContent);
    return true;
}

}
}

