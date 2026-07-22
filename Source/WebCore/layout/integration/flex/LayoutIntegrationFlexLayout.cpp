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
#include "RenderChildIterator.h"
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
    // Out-of-flow children are not flex items, but the container still establishes their static position.
    for (auto& child : childrenOfType<RenderBox>(flexBox())) {
        if (child.isOutOfFlowPositioned())
            prepareFlexItemForPositionedLayout(child);
    }

    // Build the flex items from the container's in-flow children in order-modified document order.
    FlexLayoutItems flexItems;
    for (auto& renderer : flexBox().flexItems()) {
        CheckedPtr flexItem = renderer.get();
        if (!flexItem)
            continue;
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
    // The first baseline comes from the visually-first flex line, and within it the item nearest that line's visual
    // start. flex-wrap: wrap-reverse makes the visually-first line the logically-last line; a reversed main axis
    // (row/column-reverse) puts the visual start at the logically-last item, so we scan the line in reverse.
    auto& flexItems = flexBox().flexItems();
    bool reverse = flexBox().style().flexDirection() == FlexDirection::RowReverse || flexBox().style().flexDirection() == FlexDirection::ColumnReverse;
    if (FlexFormattingUtils::isWrapReverse(flexBox()))
        return baselineFlexItemInLine(flexItems.size() - flexBox().m_numberOfFlexItemsOnLastLine, flexBox().m_numberOfFlexItemsOnLastLine, reverse);
    return baselineFlexItemInLine(0, flexBox().m_numberOfFlexItemsOnFirstLine, reverse);
}

const RenderBox* FlexLayout::flexItemForLastBaseline() const
{
    // The last baseline comes from the visually-last flex line, and within it the item nearest that line's visual
    // end (the opposite end to flexItemForFirstBaseline, hence the negated reverse). wrap-reverse makes the
    // visually-last line the logically-first line.
    auto& flexItems = flexBox().flexItems();
    bool reverse = !(flexBox().style().flexDirection() == FlexDirection::RowReverse || flexBox().style().flexDirection() == FlexDirection::ColumnReverse);
    if (FlexFormattingUtils::isWrapReverse(flexBox()))
        return baselineFlexItemInLine(0, flexBox().m_numberOfFlexItemsOnFirstLine, reverse);
    return baselineFlexItemInLine(flexItems.size() - flexBox().m_numberOfFlexItemsOnLastLine, flexBox().m_numberOfFlexItemsOnLastLine, reverse);
}

const RenderBox* FlexLayout::baselineFlexItemInLine(size_t lineStart, size_t itemCount, bool reverse) const
{
    // A flex line is the slice [lineStart, lineStart + itemCount) of the flex items (items are collected into lines
    // in order). Scanning it in the given direction, return the first item that participates in baseline alignment
    // (baseline self-alignment, the main axis is the item's inline axis, and no auto margins in the cross axis), or
    // the first item scanned when none participate.
    auto& flexItems = flexBox().flexItems();
    const RenderBox* fallback = nullptr;
    for (size_t i = 0; i < itemCount; ++i) {
        auto* flexItem = flexItems[lineStart + (reverse ? itemCount - 1 - i : i)].get();
        if (!flexItem)
            continue;
        if (!fallback)
            fallback = flexItem;
        auto position = FlexFormattingUtils::alignmentForFlexItem(flexBox(), *flexItem);
        if ((position == ItemPosition::Baseline || position == ItemPosition::LastBaseline)
            && FlexFormattingUtils::mainAxisIsFlexItemInlineAxis(flexBox(), *flexItem) && !FlexFormattingUtils::hasAutoMarginsInCrossAxis(flexBox(), *flexItem))
            return flexItem;
    }
    return fallback;
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
