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

InlineLayoutResult InlineFormattingContext::layoutInFlowAndFloatContent(const ConstraintsForInlineContent& constraints, InlineLayoutState& inlineLayoutState)
{
    if (!root().hasInFlowChild()) {
        // Float and/or out-of-flow only content does not support partial layout.
        ASSERT(!m_lineDamage);
        layoutFloatContentOnly(constraints, inlineLayoutState.parentBlockLayoutState().floatingState());
        return { { }, InlineLayoutResult::Range::Full };
    }

    auto& inlineFormattingState = formattingState();
    auto needsLayoutStartPosition = !m_lineDamage || !m_lineDamage->start() ? InlineItemPosition() : m_lineDamage->start()->inlineItemPosition;
    auto needsInlineItemsUpdate = inlineFormattingState.inlineItems().isEmpty() || m_lineDamage;
    if (needsInlineItemsUpdate)
        InlineItemsBuilder { root(), inlineFormattingState }.build(needsLayoutStartPosition);

    auto& inlineItems = inlineFormattingState.inlineItems();
    auto needsLayoutRange = InlineItemRange { needsLayoutStartPosition, { inlineItems.size(), 0 } };
    auto previousLine = [&]() -> std::optional<PreviousLine> {
        if (!needsLayoutStartPosition)
            return { };
        if (!m_lineDamage || !m_lineDamage->start())
            return { };
        auto lastLineIndex = m_lineDamage->start()->lineIndex - 1;
        // FIXME: We should be able to extract the last line information and provide it to layout as "previous line" (ends in line break and inline direction).
        return PreviousLine { lastLineIndex, { }, { }, { }, { } };
    };
    return lineLayout(inlineItems, needsLayoutRange, previousLine(), constraints, inlineLayoutState);
}

void InlineFormattingContext::layoutOutOfFlowContent(const ConstraintsForInlineContent& constraints, InlineLayoutState& inlineLayoutState, const InlineDisplay::Content& inlineDisplayContent)
{
    // Collecting out-of-flow boxes happens during the in-flow phase.
    computeStaticPositionForOutOfFlowContent(formattingState().outOfFlowBoxes(), constraints, inlineDisplayContent, inlineLayoutState.parentBlockLayoutState().floatingState());
}

IntrinsicWidthConstraints InlineFormattingContext::computedIntrinsicWidthConstraints()
{
    auto& inlineFormattingState = formattingState();
    if (m_lineDamage)
        inlineFormattingState.resetIntrinsicWidthConstraints();

    if (auto intrinsicWidthConstraints = inlineFormattingState.intrinsicWidthConstraints())
        return *intrinsicWidthConstraints;

    auto needsLayoutStartPosition = !m_lineDamage || !m_lineDamage->start() ? InlineItemPosition() : m_lineDamage->start()->inlineItemPosition;
    auto needsInlineItemsUpdate = inlineFormattingState.inlineItems().isEmpty() || m_lineDamage;
    if (needsInlineItemsUpdate)
        InlineItemsBuilder { root(), inlineFormattingState }.build(needsLayoutStartPosition);

    auto constraints = IntrinsicWidthConstraints {
        ceiledLayoutUnit(computedIntrinsicWidthForConstraint(IntrinsicWidthMode::Minimum)),
        ceiledLayoutUnit(computedIntrinsicWidthForConstraint(IntrinsicWidthMode::Maximum))
    };
    formattingState().setIntrinsicWidthConstraints(constraints);
    return constraints;
}

LayoutUnit InlineFormattingContext::maximumContentSize()
{
    auto& inlineFormattingState = formattingState();
    if (auto intrinsicWidthConstraints = inlineFormattingState.intrinsicWidthConstraints())
        return intrinsicWidthConstraints->maximum;

    if (inlineFormattingState.inlineItems().isEmpty())
        InlineItemsBuilder { root(), inlineFormattingState }.build({ });

    return ceiledLayoutUnit(computedIntrinsicWidthForConstraint(IntrinsicWidthMode::Maximum));
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

InlineLayoutResult InlineFormattingContext::lineLayout(const InlineItems& inlineItems, InlineItemRange needsLayoutRange, std::optional<PreviousLine> previousLine, const ConstraintsForInlineContent& constraints, InlineLayoutState& inlineLayoutState)
{
    ASSERT(!needsLayoutRange.isEmpty());

    auto layoutResult = InlineLayoutResult { };
    if (!needsLayoutRange.start)
        layoutResult.displayContent.boxes.reserveInitialCapacity(inlineItems.size());

    auto isPartialLayout = m_lineDamage && m_lineDamage->start();
    auto& floatingState = inlineLayoutState.parentBlockLayoutState().floatingState();
    auto floatingContext = FloatingContext { *this, floatingState };
    auto lineLogicalTop = InlineLayoutUnit { constraints.logicalTop() };
    auto previousLineEnd = std::optional<InlineItemPosition> { };
    auto leadingInlineItemPosition = needsLayoutRange.start;

    auto lineBuilder = LineBuilder { *this, inlineLayoutState, floatingState, constraints.horizontal(), inlineItems };
    while (true) {

        auto lineInitialRect = InlineRect { lineLogicalTop, constraints.horizontal().logicalLeft, constraints.horizontal().logicalWidth, formattingGeometry().initialLineHeight(!previousLine.has_value()) };
        auto lineLayoutResult = lineBuilder.layoutInlineContent({ { leadingInlineItemPosition, needsLayoutRange.end }, lineInitialRect }, previousLine);

        auto lineIndex = previousLine ? (previousLine->lineIndex + 1) : 0lu;
        auto lineLogicalRect = createDisplayContentForLine(lineIndex, lineLayoutResult, constraints, inlineLayoutState, layoutResult.displayContent);

        if (auto firstLineGap = lineLayoutResult.lineGeometry.initialLetterClearGap) {
            ASSERT(!inlineLayoutState.clearGapBeforeFirstLine());
            inlineLayoutState.setClearGapBeforeFirstLine(*firstLineGap);
        }

        if (lineLayoutResult.isFirstLast.isLastLineWithInlineContent)
            inlineLayoutState.setClearGapAfterLastLine(formattingGeometry().logicalTopForNextLine(lineLayoutResult, lineLogicalRect, floatingContext) - lineLogicalRect.bottom());

        auto lineContentEnd = lineLayoutResult.inlineItemRange.end;
        leadingInlineItemPosition = leadingInlineItemPositionForNextLine(lineContentEnd, previousLineEnd, needsLayoutRange.end);
        auto isLastLine = leadingInlineItemPosition == needsLayoutRange.end && lineLayoutResult.floatContent.suspendedFloats.isEmpty();
        if (isLastLine) {
            layoutResult.range = !isPartialLayout ? InlineLayoutResult::Range::Full : InlineLayoutResult::Range::FullFromDamage;
            break;
        }
        if (isPartialLayout && mayExitFromPartialLayout(*m_lineDamage, lineIndex, layoutResult.displayContent.boxes)) {
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

InlineLayoutUnit InlineFormattingContext::computedIntrinsicWidthForConstraint(IntrinsicWidthMode intrinsicWidthMode) const
{
    auto floatingState = FloatingState { root() };
    auto parentBlockLayoutState = BlockLayoutState { floatingState, { } };
    auto inlineLayoutState = InlineLayoutState { parentBlockLayoutState, { } };
    auto horizontalConstraints = HorizontalConstraints { };
    if (intrinsicWidthMode == IntrinsicWidthMode::Maximum)
        horizontalConstraints.logicalWidth = maxInlineLayoutUnit();
    auto& inlineItems = formattingState().inlineItems();
    auto lineBuilder = LineBuilder { *this, inlineLayoutState, floatingState, horizontalConstraints, inlineItems, intrinsicWidthMode };
    auto layoutRange = InlineItemRange { 0 , inlineItems.size() };
    auto maximumContentWidth = InlineLayoutUnit { };
    auto previousLineEnd = std::optional<InlineItemPosition> { };
    auto previousLine = std::optional<PreviousLine> { };
    auto lineIndex = 0lu;

    while (!layoutRange.isEmpty()) {
        auto lineLayoutResult = lineBuilder.layoutInlineContent({ layoutRange, { 0.f, 0.f, horizontalConstraints.logicalWidth, 0.f } }, previousLine);
        auto floatContentWidth = [&] {
            auto leftWidth = LayoutUnit { };
            auto rightWidth = LayoutUnit { };
            for (auto& floatItem : lineLayoutResult.floatContent.placedFloats) {
                auto marginBoxRect = BoxGeometry::marginBoxRect(floatItem.boxGeometry());
                if (floatItem.isLeftPositioned())
                    leftWidth = std::max(leftWidth, marginBoxRect.right());
                else
                    rightWidth = std::max(rightWidth, horizontalConstraints.logicalWidth - marginBoxRect.left());
            }
            return InlineLayoutUnit { leftWidth + rightWidth };
        };
        maximumContentWidth = std::max(maximumContentWidth, lineLayoutResult.lineGeometry.logicalTopLeft.x() + lineLayoutResult.contentGeometry.logicalWidth + floatContentWidth());

        layoutRange.start = leadingInlineItemPositionForNextLine(lineLayoutResult.inlineItemRange.end, previousLineEnd, layoutRange.end);
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

InlineRect InlineFormattingContext::createDisplayContentForLine(size_t lineIndex, const LineBuilder::LayoutResult& lineLayoutResult, const ConstraintsForInlineContent& constraints, const InlineLayoutState& inlineLayoutState, InlineDisplay::Content& displayContent)
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

void InlineFormattingContext::resetGeometryForClampedContent(const InlineItemRange& needsDisplayContentRange, const LineBuilder::SuspendedFloatList& suspendedFloats, LayoutPoint topleft)
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

}
}

