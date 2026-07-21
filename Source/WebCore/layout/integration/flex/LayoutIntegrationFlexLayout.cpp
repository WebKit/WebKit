/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "LayoutIntegrationFlexLayout.h"

#include "FlexFormattingUtils.h"
#include "RenderBlockInlines.h"
#include "RenderBoxInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderElementStyleInlines.h"
#include "RenderFlexibleBox.h"
#include "RenderLayer.h"
#include "RenderObjectInlines.h"
#include "StyleComputedStyle+GettersInlines.h"

namespace WebCore {
namespace LayoutIntegration {

FlexLayout::FlexLayout(RenderFlexibleBox& flexBox)
    : m_flexBox(flexBox)
{
}

void FlexLayout::prepareFlexItemForPositionedLayout(RenderBox& flexItem)
{
    ASSERT(flexItem.isOutOfFlowPositioned());
    flexItem.containingBlock()->addOutOfFlowBox(flexItem);
    CheckedPtr layer = flexItem.layer();
    FlexFormattingUtils utils { flexBox() };
    LayoutUnit staticInlinePosition = utils.flowAwareBorderStart() + utils.flowAwarePaddingStart();
    if (layer->staticInlinePosition() != staticInlinePosition) {
        layer->setStaticInlinePosition(staticInlinePosition);
        if (flexItem.style().hasStaticInlinePosition(flexBox().writingMode().isHorizontal()))
            flexItem.setChildNeedsLayout(MarkingBehavior::MarkOnlyThis);
    }

    LayoutUnit staticBlockPosition = utils.flowAwareBorderBefore() + utils.flowAwarePaddingBefore();
    if (layer->staticBlockPosition() != staticBlockPosition) {
        layer->setStaticBlockPosition(staticBlockPosition);
        if (flexItem.style().hasStaticBlockPosition(flexBox().writingMode().isHorizontal()))
            flexItem.setChildNeedsLayout(MarkingBehavior::MarkOnlyThis);
    }
}

FlexLayoutItems FlexLayout::collectFlexItems(RelayoutChildren relayoutChildren)
{
    // Build the flex container's flex items in order-modified document order, skipping out-of-flow children (which are not flex items).
    FlexLayoutItems flexItems;
    auto& orderIterator = flexBox().m_orderIterator;
    for (CheckedPtr flexItem = orderIterator.first(); flexItem; flexItem = orderIterator.next()) {
        if (orderIterator.shouldSkipChild(*flexItem)) {
            if (flexItem->isOutOfFlowPositioned())
                prepareFlexItemForPositionedLayout(*flexItem);
            continue;
        }
        auto everHadLayout = flexItem->everHadLayout();
        if (CheckedPtr flexibleBox = dynamicDowncast<RenderFlexibleBox>(flexItem.get()))
            flexibleBox->resetHasDefiniteHeight();
        if (everHadLayout && flexItem->hasTrimmedMargin(std::optional<Style::MarginTrimSide> { }))
            flexItem->clearTrimmedMarginsMarkings();
        if (flexItem->shouldInvalidateContentWidths())
            flexItem->invalidateContentLogicalWidths(MarkingBehavior::MarkOnlyThis);
        flexBox().updateBlockChildDirtyBitsBeforeLayout(relayoutChildren, *flexItem);
        flexItems.append({ *flexItem, everHadLayout, relayoutChildren == RelayoutChildren::Yes });
    }
    return flexItems;
}

void FlexLayout::layout(RelayoutChildren relayoutChildren)
{
    auto flexItems = collectFlexItems(relayoutChildren);
    if (flexItems.isEmpty()) {
        flexBox().updateFlexContainerLogicalHeight(0_lu);
        return;
    }

    auto flexLayoutResult = WebCore::FlexFormattingContext(flexBox()).layout(flexItems);
    if (flexLayoutResult.alignContentStartOverflow)
        flexBox().m_alignContentStartOverflow = *flexLayoutResult.alignContentStartOverflow;
    flexBox().m_justifyContentStartOverflow = flexLayoutResult.justifyContentStartOverflow;
    flexBox().m_numberOfFlexItemsOnFirstLine = flexLayoutResult.numberOfFlexItemsOnFirstLine;
    flexBox().m_numberOfFlexItemsOnLastLine = flexLayoutResult.numberOfFlexItemsOnLastLine;
}

std::optional<LayoutUnit> FlexLayout::firstLineBaseline() const
{
    if ((flexBox().isWritingModeRoot() && !flexBox().isFlexItem()) || !flexBox().m_numberOfFlexItemsOnFirstLine || flexBox().shouldApplyLayoutContainment())
        return { };

    CheckedPtr baselineFlexItem = flexItemForFirstBaseline();
    if (!baselineFlexItem)
        return { };

    FlexFormattingUtils utils { flexBox() };
    auto baseline = std::optional<LayoutUnit> { };
    if (!FlexFormattingUtils::isColumnFlow(flexBox()) && !FlexFormattingUtils::mainAxisIsFlexItemInlineAxis(flexBox(), *baselineFlexItem))
        baseline = utils.crossAxisExtentForFlexItem(*baselineFlexItem);
    else if (FlexFormattingUtils::isColumnFlow(flexBox()) && FlexFormattingUtils::mainAxisIsFlexItemInlineAxis(flexBox(), *baselineFlexItem))
        baseline = utils.mainAxisExtentForFlexItem(*baselineFlexItem);
    else if (auto firstLineBaseline = baselineFlexItem->firstLineBaseline())
        baseline = firstLineBaseline;
    else {
        // FIXME: We should pass |direction| into firstLineBoxBaseline and stop bailing out if we're a writing mode root.
        // This would also fix some cases where the flexbox is orthogonal to its container.
        auto direction = flexBox().isHorizontalWritingMode() ? LineDirection::Horizontal : LineDirection::Vertical;
        auto flexboxWritingMode = flexBox().style().writingMode();
        auto dominantBaseline = BaselineAlignment::dominantBaseline(flexboxWritingMode);
        baseline = BaselineAlignment::synthesizedBaseline(*baselineFlexItem, dominantBaseline, flexboxWritingMode, direction, BaselineSynthesisEdge::BorderBox);
    }
    auto result = baselineFlexItem->logicalTop() + *baseline;
    // CSS Align §9.1: if a scroll container's baseline is outside its border edge, clamp to the border edge.
    if (FlexFormattingUtils::isHorizontalFlow(flexBox()) ? flexBox().isScrollContainerY() : flexBox().isScrollContainerX())
        return std::max(0_lu, std::min(result, flexBox().logicalHeight()));
    return result;
}

std::optional<LayoutUnit> FlexLayout::lastLineBaseline() const
{
    if (flexBox().isWritingModeRoot() || !flexBox().m_numberOfFlexItemsOnLastLine || flexBox().shouldApplyLayoutContainment())
        return { };

    CheckedPtr baselineFlexItem = flexItemForLastBaseline();
    if (!baselineFlexItem)
        return { };

    FlexFormattingUtils utils { flexBox() };
    auto baseline = std::optional<LayoutUnit> { };
    if (!FlexFormattingUtils::isColumnFlow(flexBox()) && !FlexFormattingUtils::mainAxisIsFlexItemInlineAxis(flexBox(), *baselineFlexItem))
        baseline = utils.crossAxisExtentForFlexItem(*baselineFlexItem);
    else if (FlexFormattingUtils::isColumnFlow(flexBox()) && FlexFormattingUtils::mainAxisIsFlexItemInlineAxis(flexBox(), *baselineFlexItem))
        baseline = utils.mainAxisExtentForFlexItem(*baselineFlexItem);
    else if (auto lastLineBaseline = baselineFlexItem->lastLineBaseline())
        baseline = lastLineBaseline;
    else {
        // FIXME: We should pass |direction| into firstLineBoxBaseline and stop bailing out if we're a writing mode root.
        // This would also fix some cases where the flexbox is orthogonal to its container.
        auto direction = flexBox().isHorizontalWritingMode() ? LineDirection::Horizontal : LineDirection::Vertical;
        auto flexboxWritingMode = flexBox().style().writingMode();
        auto dominantBaseline = BaselineAlignment::dominantBaseline(flexboxWritingMode);
        baseline = BaselineAlignment::synthesizedBaseline(*baselineFlexItem, dominantBaseline, flexboxWritingMode, direction, BaselineSynthesisEdge::BorderBox);
    }
    auto result = baselineFlexItem->logicalTop() + *baseline;
    // CSS Align §9.1: if a scroll container's baseline is outside its border edge, clamp to the border edge.
    if (FlexFormattingUtils::isHorizontalFlow(flexBox()) ? flexBox().isScrollContainerY() : flexBox().isScrollContainerX())
        return std::max(0_lu, std::min(result, flexBox().logicalHeight()));
    return result;
}

const RenderBox* FlexLayout::flexItemForFirstBaseline() const
{
    // Looking for baseline flex candidate on visually first line.
    auto useLastLine = FlexFormattingUtils::isWrapReverse(flexBox());
    auto useLastItem = flexBox().style().flexDirection() == FlexDirection::RowReverse || flexBox().style().flexDirection() == FlexDirection::ColumnReverse;

    if (!useLastLine) {
        if (!useLastItem) {
            // Logically (and visually) first item on logically (and visually) first line.
            return firstBaselineCandidateOnLine(flexBox().m_orderIterator, flexBox().m_numberOfFlexItemsOnFirstLine);
        }
        // Logically last (but visually first) item on logically (and visually) first line.
        return lastBaselineCandidateOnLine(flexBox().m_orderIterator, flexBox().m_numberOfFlexItemsOnFirstLine);
    }

    if (!useLastItem) {
        // Logically (and visually) first item on logically last (but visually first) line.
        return lastBaselineCandidateOnLine(flexBox().m_orderIterator.reverse(), flexBox().m_numberOfFlexItemsOnLastLine);
    }
    // Logically last (but visually first) item on logically last (but visually first) line.
    return firstBaselineCandidateOnLine(flexBox().m_orderIterator.reverse(), flexBox().m_numberOfFlexItemsOnLastLine);
}

const RenderBox* FlexLayout::flexItemForLastBaseline() const
{
    // Looking for baseline flex candidate on visually last line.
    auto useLastLine = FlexFormattingUtils::isWrapReverse(flexBox());
    auto useLastItem = flexBox().style().flexDirection() == FlexDirection::RowReverse || flexBox().style().flexDirection() == FlexDirection::ColumnReverse;

    if (!useLastLine) {
        if (!useLastItem) {
            // Logically (and visually) last item on logically (and visually) last line.
            return firstBaselineCandidateOnLine(flexBox().m_orderIterator.reverse(), flexBox().m_numberOfFlexItemsOnLastLine);
        }
        // Logically first (but visually last) item  on logically (and visually) last line.
        return lastBaselineCandidateOnLine(flexBox().m_orderIterator.reverse(), flexBox().m_numberOfFlexItemsOnLastLine);
    }

    if (!useLastItem) {
        // Logically (and visually) last item on logically first (but visually last) line.
        return lastBaselineCandidateOnLine(flexBox().m_orderIterator, flexBox().m_numberOfFlexItemsOnFirstLine);
    }
    // Logically first (but visually last) item on logically last (but visually first) line.
    return firstBaselineCandidateOnLine(flexBox().m_orderIterator, flexBox().m_numberOfFlexItemsOnFirstLine);
}

const RenderBox* FlexLayout::firstBaselineCandidateOnLine(OrderIterator flexItemIterator, size_t numberOfItemsOnLine) const
{
    // Note that "first" here means in iterator order and not logical flex order (caller can pass in reversed order).
    size_t index = 0;
    const RenderBox* baselineFlexItem = nullptr;
    for (auto* flexItem = flexItemIterator.first(); flexItem; flexItem = flexItemIterator.next()) {
        if (flexItemIterator.shouldSkipChild(*flexItem))
            continue;
        auto flexItemPosition = FlexFormattingUtils::alignmentForFlexItem(flexBox(), *flexItem);
        if ((flexItemPosition == ItemPosition::Baseline || flexItemPosition == ItemPosition::LastBaseline)
            && FlexFormattingUtils::mainAxisIsFlexItemInlineAxis(flexBox(), *flexItem) && !FlexFormattingUtils::hasAutoMarginsInCrossAxis(flexBox(), *flexItem))
            return flexItem;
        if (!baselineFlexItem)
            baselineFlexItem = flexItem;
        if (++index == numberOfItemsOnLine)
            return baselineFlexItem;
    }
    return nullptr;
}

const RenderBox* FlexLayout::lastBaselineCandidateOnLine(OrderIterator flexItemIterator, size_t numberOfItemsOnLine) const
{
    // Note that "last" here means in iterator order and not logical flex order (caller can pass in reversed order).
    size_t index = 0;
    RenderBox* baselineFlexItem = nullptr;
    for (auto* flexItem = flexItemIterator.first(); flexItem; flexItem = flexItemIterator.next()) {
        if (flexItemIterator.shouldSkipChild(*flexItem))
            continue;
        auto flexItemPosition = FlexFormattingUtils::alignmentForFlexItem(flexBox(), *flexItem);
        if ((flexItemPosition == ItemPosition::Baseline || flexItemPosition == ItemPosition::LastBaseline)
            && FlexFormattingUtils::mainAxisIsFlexItemInlineAxis(flexBox(), *flexItem) && !FlexFormattingUtils::hasAutoMarginsInCrossAxis(flexBox(), *flexItem))
            baselineFlexItem = flexItem;
        if (++index == numberOfItemsOnLine)
            return baselineFlexItem ? baselineFlexItem : flexItem;
    }
    return nullptr;
}

LayoutUnit FlexLayout::staticMainAxisPositionForPositionedFlexItem(const RenderBox& flexItem)
{
    FlexFormattingUtils utils { flexBox() };
    auto flexItemMainExtent = utils.mainAxisMarginExtentForFlexItem(flexItem) + utils.mainAxisExtentForFlexItem(flexItem);
    auto mainAxisContentSize = FlexFormattingUtils::isColumnFlow(flexBox()) ? flexBox().contentBoxLogicalHeight() : flexBox().contentBoxLogicalWidth();
    auto availableSpace = mainAxisContentSize - flexItemMainExtent;
    auto isReverse = utils.isColumnOrRowReverse();
    LayoutUnit offset = FlexFormattingUtils::initialJustifyContentOffset(flexBox().style(), availableSpace, { }, isReverse);
    if (isReverse)
        offset = availableSpace - offset;
    return offset;
}

LayoutUnit FlexLayout::staticCrossAxisPositionForPositionedFlexItem(const RenderBox& flexItem)
{
    FlexFormattingUtils utils { flexBox() };
    auto availableSpace = utils.availableAlignmentSpaceForFlexItem(FlexFormattingUtils::crossAxisContentExtent(flexBox()), flexItem, utils.crossAxisExtentForFlexItem(flexItem));
    auto safety = utils.overflowAlignmentForFlexItem(flexItem);
    auto align = FlexFormattingUtils::alignmentForFlexItem(flexBox(), flexItem);
    if (availableSpace < 0 && safety == OverflowAlignment::Safe)
        align = ItemPosition::FlexStart;
    return FlexFormattingUtils::alignmentOffset(availableSpace, align, { }, { }, FlexFormattingUtils::isWrapReverse(flexBox()));
}

LayoutUnit FlexLayout::staticInlinePositionForPositionedFlexItem(const RenderBox& flexItem)
{
    return flexBox().startOffsetForContent() + (FlexFormattingUtils::isColumnFlow(flexBox()) ? staticCrossAxisPositionForPositionedFlexItem(flexItem) : staticMainAxisPositionForPositionedFlexItem(flexItem));
}

LayoutUnit FlexLayout::staticBlockPositionForPositionedFlexItem(const RenderBox& flexItem)
{
    return flexBox().borderAndPaddingBefore() + (FlexFormattingUtils::isColumnFlow(flexBox()) ? staticMainAxisPositionForPositionedFlexItem(flexItem) : staticCrossAxisPositionForPositionedFlexItem(flexItem));
}

bool FlexLayout::setStaticPositionForPositionedLayout(const RenderBox& flexItem)
{
    bool positionChanged = false;
    CheckedPtr layer = flexItem.layer();
    if (flexItem.style().hasStaticInlinePosition(flexBox().writingMode().isHorizontal())) {
        LayoutUnit inlinePosition = staticInlinePositionForPositionedFlexItem(flexItem);
        if (layer->staticInlinePosition() != inlinePosition) {
            layer->setStaticInlinePosition(inlinePosition);
            positionChanged = true;
        }
    }
    if (flexItem.style().hasStaticBlockPosition(flexBox().writingMode().isHorizontal())) {
        LayoutUnit blockPosition = staticBlockPositionForPositionedFlexItem(flexItem);
        if (layer->staticBlockPosition() != blockPosition) {
            layer->setStaticBlockPosition(blockPosition);
            positionChanged = true;
        }
    }
    return positionChanged;
}

}
}
