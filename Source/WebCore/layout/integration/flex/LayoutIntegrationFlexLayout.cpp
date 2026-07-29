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
#include "HitTestResult.h"
#include "RenderBlockInlines.h"
#include "RenderBoxInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderChildIterator.h"
#include "RenderElementStyleInlines.h"
#include "RenderFlexibleBox.h"
#include "RenderLayer.h"
#include "RenderObjectInlines.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleMarginTrim.h"

namespace WebCore {
namespace LayoutIntegration {

FlexLayout::FlexLayout(RenderFlexibleBox& flexBox)
    : m_flexBox(flexBox)
{
}

void FlexLayout::prepareOutOfFlowBoxForPositionedLayout(RenderBox& outOfFlowBox)
{
    ASSERT(outOfFlowBox.isOutOfFlowPositioned());
    outOfFlowBox.containingBlock()->addOutOfFlowBox(outOfFlowBox);
    CheckedPtr layer = outOfFlowBox.layer();
    FlexFormattingUtils utils { flexBox() };
    LayoutUnit staticInlinePosition = utils.flowAwareBorderStart() + utils.flowAwarePaddingStart();
    if (layer->staticInlinePosition() != staticInlinePosition) {
        layer->setStaticInlinePosition(staticInlinePosition);
        if (outOfFlowBox.style().hasStaticInlinePosition(flexBox().writingMode().isHorizontal()))
            outOfFlowBox.setChildNeedsLayout(MarkingBehavior::MarkOnlyThis);
    }

    LayoutUnit staticBlockPosition = utils.flowAwareBorderBefore() + utils.flowAwarePaddingBefore();
    if (layer->staticBlockPosition() != staticBlockPosition) {
        layer->setStaticBlockPosition(staticBlockPosition);
        if (outOfFlowBox.style().hasStaticBlockPosition(flexBox().writingMode().isHorizontal()))
            outOfFlowBox.setChildNeedsLayout(MarkingBehavior::MarkOnlyThis);
    }
}

FlexLayoutConstraints FlexLayout::flexLayoutConstraints() const
{
    FlexFormattingUtils utils { flexBox() };
    return {
        .style = flexBox().style(),
        .isHorizontalFlow = FlexFormattingUtils::isHorizontalFlow(flexBox()),
        .isColumnFlow = FlexFormattingUtils::isColumnFlow(flexBox()),
        .isMultiline = FlexFormattingUtils::isMultiline(flexBox()),
        .isWrapReverse = FlexFormattingUtils::isWrapReverse(flexBox()),
        .isColumnOrRowReverse = utils.isColumnOrRowReverse(),
        .isLeftToRightFlow = utils.isLeftToRightFlow(),
        .crossAxisDirection = utils.crossAxisDirection(),
        .flowAwareBorderInline = { utils.flowAwareBorderStart(), utils.flowAwareBorderEnd() },
        .flowAwareBorderBlock = { utils.flowAwareBorderBefore(), utils.flowAwareBorderAfter() },
        .flowAwarePaddingInline = { utils.flowAwarePaddingStart(), utils.flowAwarePaddingEnd() },
        .flowAwarePaddingBlock = { utils.flowAwarePaddingBefore(), utils.flowAwarePaddingAfter() },
        .mainAxisAvailableSpace = mainAxisAvailableSpace(),
        .mainAxisSizeForLengthResolution = FlexFormattingUtils::isColumnFlow(flexBox()) ? flexBox().availableLogicalHeight(AvailableLogicalHeightType::ExcludeMarginBorderPadding) : flexBox().contentBoxLogicalWidth(),
        .mainAxisBorderBoxExtent = utils.mainAxisExtent(),
        .crossAxisSizeForLengthResolution = flexBox().contentBoxLogicalWidth(),
        .mainAxisScrollbarExtent = utils.mainAxisScrollbarExtent(),
        .crossAxisScrollbarExtent = utils.crossAxisScrollbarExtent(),
    };
}

LayoutUnit FlexLayout::mainAxisAvailableSpace() const
{
    if (!FlexFormattingUtils::isColumnFlow(flexBox()))
        return flexBox().contentBoxLogicalWidth();

    auto logicalHeightIgnoringFlexBasisOverride = [&] {
        // The flex-basis override is for the parent flex's sizing of this item,
        // not for this container's own wrapping decisions. Temporarily clear it
        // so computeLogicalHeight sees the specified height.
        auto override = flexBox().overridingLogicalHeightForFlexBasisComputation();
        if (!override)
            return flexBox().computeLogicalHeight(LayoutUnit::max(), flexBox().logicalTop()).extent;

        flexBox().clearOverridingLogicalHeightForFlexBasisComputation();
        auto computedValues = flexBox().computeLogicalHeight(LayoutUnit::max(), flexBox().logicalTop());
        flexBox().setOverridingBorderBoxLogicalHeightForFlexBasisComputation(*override);
        return computedValues.extent;
    };
    auto logicalHeight = logicalHeightIgnoringFlexBasisOverride();
    return logicalHeight == LayoutUnit::max() ? logicalHeight : std::max(0_lu, logicalHeight - (flexBox().borderAndPaddingLogicalHeight() + flexBox().scrollbarLogicalHeight()));
}

FlexLayoutItems FlexLayout::buildFlexLayoutItems(RelayoutChildren relayoutChildren, const FlexLayoutConstraints& constraints)
{
    FlexLayoutItems flexLayoutItems;
    flexLayoutItems.reserveInitialCapacity(m_flexItems.size());
    for (auto& renderer : m_flexItems) {
        CheckedPtr flexItem = renderer.get();
        if (!flexItem)
            continue;

        // Before running the flex algorithm, 'auto' has a margin of 0.
        // Also, if we're not auto sizing, we don't do a layout that computes the start/end margins.
        auto flexItemMarginValue = [&](auto& margin) {
            // When resolving the margins, we use the content size for resolving percent and calc (for percents in calc expressions) margins.
            // Fortunately, percent margins are always computed with respect to the block's width, even for margin-top and margin-bottom.
            return Style::evaluateMinimum<LayoutUnit>(margin, flexBox().contentBoxLogicalWidth(), flexBox().style().usedZoomForLength());
        };
        if (constraints.isHorizontalFlow) {
            flexItem->setMarginLeft(flexItemMarginValue(flexItem->style().marginLeft()));
            flexItem->setMarginRight(flexItemMarginValue(flexItem->style().marginRight()));
        } else {
            flexItem->setMarginTop(flexItemMarginValue(flexItem->style().marginTop()));
            flexItem->setMarginBottom(flexItemMarginValue(flexItem->style().marginBottom()));
        }

        auto everHadLayout = flexItem->everHadLayout();
        if (everHadLayout && flexItem->hasTrimmedMargin(std::optional<Style::MarginTrimSide> { }))
            flexItem->clearTrimmedMarginsMarkings();
        if (flexItem->shouldInvalidateContentWidths())
            flexItem->invalidateContentLogicalWidths(MarkingBehavior::MarkOnlyThis);
        flexBox().updateBlockChildDirtyBitsBeforeLayout(relayoutChildren, *flexItem);
        flexLayoutItems.append({ *flexItem, constraints.isHorizontalFlow, everHadLayout, relayoutChildren == RelayoutChildren::Yes });
    }
    return flexLayoutItems;
}

// RenderFlexibleBox trims the first and last in-flow child's inline margins before the flex algorithm runs, so
// that they stay out of the container's intrinsic widths. Recompute that from style each layout rather than
// carrying it in a member the renderer has to remember to seed and clear.
FlexLayoutState::MarginTrimItems FlexLayout::marginTrimItemsBeforeFlexLayout() const
{
    auto marginTrim = flexBox().style().marginTrim();
    if (marginTrim.isNone())
        return { };

    auto trimsInlineStart = marginTrim.contains(Style::MarginTrimSide::InlineStart);
    auto trimsInlineEnd = marginTrim.contains(Style::MarginTrimSide::InlineEnd);
    if (!trimsInlineStart && !trimsInlineEnd)
        return { };

    // The items at the start and end of the container's single line of items, in the order the flex algorithm will
    // use: the lowest and highest used 'order' value, document order breaking ties. Scanning for them keeps this in
    // step with buildFlexItemList without having to build the sorted list before the container has been sized.
    CheckedPtr<RenderBox> firstFlexItem;
    CheckedPtr<RenderBox> lastFlexItem;
    for (CheckedRef child : childrenOfType<RenderBox>(flexBox())) {
        if (child->isOutOfFlowPositioned() || child->isExcludedFromNormalLayout())
            continue;
        auto order = child->style().order().value;
        if (!firstFlexItem || order < firstFlexItem->style().order().value)
            firstFlexItem = child.ptr();
        if (!lastFlexItem || order >= lastFlexItem->style().order().value)
            lastFlexItem = child.ptr();
    }
    if (!firstFlexItem)
        return { };

    auto marginTrimItems = FlexLayoutState::MarginTrimItems { };
    auto isRowsFlexbox = FlexFormattingUtils::isHorizontalFlow(flexBox());
    if (trimsInlineStart)
        isRowsFlexbox ? marginTrimItems.itemsAtFlexLineStart.add(*firstFlexItem) : marginTrimItems.itemsOnFirstFlexLine.add(*firstFlexItem);
    if (trimsInlineEnd)
        isRowsFlexbox ? marginTrimItems.itemsAtFlexLineEnd.add(*lastFlexItem) : marginTrimItems.itemsOnLastFlexLine.add(*lastFlexItem);
    return marginTrimItems;
}

bool FlexLayout::isFlexItemEligibleForMarginTrim(Style::MarginTrimSide marginTrimSide, const RenderBox& flexItem) const
{
    ASSERT(flexBox().style().marginTrim().contains(marginTrimSide));

    auto isTrimmed = [&](const FlexLayoutState::MarginTrimItems& marginTrimItems) {
        auto isMarginParallelWithMainAxis = FlexFormattingUtils::isHorizontalFlow(flexBox())
            ? marginTrimSide == Style::MarginTrimSide::BlockStart || marginTrimSide == Style::MarginTrimSide::BlockEnd
            : marginTrimSide == Style::MarginTrimSide::InlineStart || marginTrimSide == Style::MarginTrimSide::InlineEnd;
        auto isStartSide = marginTrimSide == Style::MarginTrimSide::BlockStart || marginTrimSide == Style::MarginTrimSide::InlineStart;
        if (isMarginParallelWithMainAxis)
            return isStartSide ? marginTrimItems.itemsOnFirstFlexLine.contains(flexItem) : marginTrimItems.itemsOnLastFlexLine.contains(flexItem);
        return isStartSide ? marginTrimItems.itemsAtFlexLineStart.contains(flexItem) : marginTrimItems.itemsAtFlexLineEnd.contains(flexItem);
    };

    // The flex algorithm owns the sets while it runs and is still filling them in -- an item asks this as it lays
    // out, which is mid-algorithm.
    if (m_flexLayoutState)
        return isTrimmed(m_flexLayoutState->marginTrimItems());

    // Between a style change and the layout that follows, all that holds is what the container trims up front,
    // which is also what keeps those margins out of the intrinsic widths computed in between.
    if (flexBox().needsLayout())
        return isTrimmed(marginTrimItemsBeforeFlexLayout());

    // Otherwise the last run of the algorithm has the answer, for the queries that arrive once it is done: the
    // scrollbar reconciliation relayout, and any later layout of an item on its own.
    if (!m_flexLayoutResult) {
        ASSERT_NOT_REACHED();
        return false;
    }
    return isTrimmed(m_flexLayoutResult->marginTrimItems);
}

void FlexLayout::buildFlexItemList()
{
    // The in-flow children are the flex items, collected in order-modified document order (a stable sort by the
    // used 'order' value keeps document order among equal values). This is rebuilt every layout and replaces the
    // order iterator. The list holds weak pointers because painting/hit-testing/baseline queries read it after
    // layout, when a child may have been removed.
    m_flexItems.clear();
    for (CheckedRef child : childrenOfType<RenderBox>(flexBox())) {
        // Out-of-flow children are not flex items, but the container still establishes their static position.
        if (child->isOutOfFlowPositioned()) {
            prepareOutOfFlowBoxForPositionedLayout(child);
            continue;
        }
        if (!child->isExcludedFromNormalLayout())
            m_flexItems.append(child.get());
    }
    std::stable_sort(m_flexItems.begin(), m_flexItems.end(), [](auto& a, auto& b) {
        return a->style().order().value < b->style().order().value;
    });
}

LayoutOptionalOutsets FlexLayout::adjustAllowedLayoutOverflow(LayoutOptionalOutsets allowance) const
{
    // How far content-alignment pushed the items past the container's content-box start edges. The flex algorithm
    // only computes the align-content overflow when it has lines to align, so treat "not computed" as no overflow.
    if (!m_flexLayoutResult) {
        ASSERT_NOT_REACHED();
        // The caller's writing-mode allowance is what holds without a flex contribution to add to it.
        return allowance;
    }

    auto alignContentStartOverflow = m_flexLayoutResult->alignContentStartOverflow.value_or(0_lu);
    auto justifyContentStartOverflow = m_flexLayoutResult->justifyContentStartOverflow;

    bool isColumnar = flexBox().style().isColumnFlexDirection();
    if (flexBox().isHorizontalWritingMode()) {
        allowance.top() = isColumnar ? justifyContentStartOverflow : alignContentStartOverflow;
        if (flexBox().writingMode().isInlineLeftToRight())
            allowance.left() = isColumnar ? alignContentStartOverflow : justifyContentStartOverflow;
        else
            allowance.right() = isColumnar ? alignContentStartOverflow : justifyContentStartOverflow;
    } else {
        allowance.left() = isColumnar ? justifyContentStartOverflow : alignContentStartOverflow;
        if (flexBox().writingMode().isInlineTopToBottom())
            allowance.top() = isColumnar ? alignContentStartOverflow : justifyContentStartOverflow;
        else
            allowance.bottom() = isColumnar ? alignContentStartOverflow : justifyContentStartOverflow;
    }

    return allowance;
}

void FlexLayout::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset, PaintInfo& paintInfoForFlexItem, bool usePrintRect)
{
    for (auto& renderer : m_flexItems) {
        CheckedPtr flexItem = renderer.get();
        if (flexItem && !flexBox().paintChild(*flexItem, paintInfo, paintOffset, paintInfoForFlexItem, usePrintRect, RenderBlock::PaintBlockType::PaintAsInlineBlock))
            return;
    }
}

bool FlexLayout::hitTest(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& adjustedLocation, HitTestAction hitTestAction)
{
    if (hitTestAction != HitTestAction::Foreground)
        return false;

    LayoutPoint scrolledOffset = flexBox().hasNonVisibleOverflow() ? adjustedLocation - toLayoutSize(flexBox().scrollPosition()) : adjustedLocation;

    auto hitTestChild = [&](RenderBox& child) {
        if (child.hasSelfPaintingLayer())
            return false;
        auto location = flexBox().flipForWritingModeForChild(child, scrolledOffset);
        if (!child.hitTest(request, result, locationInContainer, location))
            return false;
        flexBox().updateHitTestResult(result, flexBox().flipForWritingMode(toLayoutPoint(locationInContainer.point() - adjustedLocation)));
        return true;
    };

    // Hit-testing visits children front-to-back, i.e. the reverse of paint order.
    for (size_t i = m_flexItems.size(); i--;) {
        if (CheckedPtr flexItem = m_flexItems[i].get(); flexItem && hitTestChild(*flexItem))
            return true;
    }

    // A fieldset's legend is excluded from normal layout (placed in the border), so it is not a flex item and is
    // not in m_flexItems; hit-test it separately.
    if (CheckedPtr legend = flexBox().isFieldset() ? flexBox().findFieldsetLegend() : nullptr; legend && legend->isExcludedFromNormalLayout())
        return hitTestChild(*legend);

    return false;
}

void FlexLayout::layout(RelayoutChildren relayoutChildren)
{
    m_flexLayoutResult = { };
    buildFlexItemList();

    auto constraints = flexLayoutConstraints();
    auto flexLayoutItems = buildFlexLayoutItems(relayoutChildren, constraints);

    auto flexLayoutStateScope = SetForScope { m_flexLayoutState, FlexLayoutState { marginTrimItemsBeforeFlexLayout(), flexBox().hasDefiniteLogicalHeight() } };
    auto integrationUtils = FlexIntegrationUtils { flexBox(), *m_flexLayoutState, m_flexItemContentCache };
    m_flexLayoutResult = WebCore::FlexFormattingContext(flexBox(), integrationUtils, constraints, *m_flexLayoutState).layout(flexLayoutItems);
}

std::optional<LayoutUnit> FlexLayout::firstLineBaseline() const
{
    if ((flexBox().isWritingModeRoot() && !flexBox().isFlexItem()) || !m_flexLayoutResult || !m_flexLayoutResult->numberOfFlexItemsOnFirstLine || flexBox().shouldApplyLayoutContainment())
        return { };

    CheckedPtr baselineFlexItem = flexItemForFirstBaseline();
    if (!baselineFlexItem)
        return { };

    FlexFormattingUtils utils { flexBox() };
    auto baseline = std::optional<LayoutUnit> { };
    if (!FlexFormattingUtils::isColumnFlow(flexBox()) && !FlexFormattingUtils::mainAxisIsFlexItemInlineAxis(*baselineFlexItem))
        baseline = utils.crossAxisExtentForFlexItem(*baselineFlexItem);
    else if (FlexFormattingUtils::isColumnFlow(flexBox()) && FlexFormattingUtils::mainAxisIsFlexItemInlineAxis(*baselineFlexItem))
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
    if (flexBox().isWritingModeRoot() || !m_flexLayoutResult || !m_flexLayoutResult->numberOfFlexItemsOnLastLine || flexBox().shouldApplyLayoutContainment())
        return { };

    CheckedPtr baselineFlexItem = flexItemForLastBaseline();
    if (!baselineFlexItem)
        return { };

    FlexFormattingUtils utils { flexBox() };
    auto baseline = std::optional<LayoutUnit> { };
    if (!FlexFormattingUtils::isColumnFlow(flexBox()) && !FlexFormattingUtils::mainAxisIsFlexItemInlineAxis(*baselineFlexItem))
        baseline = utils.crossAxisExtentForFlexItem(*baselineFlexItem);
    else if (FlexFormattingUtils::isColumnFlow(flexBox()) && FlexFormattingUtils::mainAxisIsFlexItemInlineAxis(*baselineFlexItem))
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

CheckedPtr<const RenderBox> FlexLayout::flexItemForFirstBaseline() const
{
    if (!m_flexLayoutResult) {
        ASSERT_NOT_REACHED();
        return { };
    }

    // The first baseline comes from the visually-first flex line, and within it the item nearest that line's visual
    // start. flex-wrap: wrap-reverse makes the visually-first line the logically-last line; a reversed main axis
    // (row/column-reverse) puts the visual start at the logically-last item, so we scan the line in reverse.
    auto& flexItems = m_flexItems;
    bool reverse = flexBox().style().flexDirection() == FlexDirection::RowReverse || flexBox().style().flexDirection() == FlexDirection::ColumnReverse;
    if (FlexFormattingUtils::isWrapReverse(flexBox()))
        return baselineFlexItemInLine(flexItems.size() - m_flexLayoutResult->numberOfFlexItemsOnLastLine, m_flexLayoutResult->numberOfFlexItemsOnLastLine, reverse);
    return baselineFlexItemInLine(0, m_flexLayoutResult->numberOfFlexItemsOnFirstLine, reverse);
}

CheckedPtr<const RenderBox> FlexLayout::flexItemForLastBaseline() const
{
    if (!m_flexLayoutResult) {
        ASSERT_NOT_REACHED();
        return { };
    }

    // The last baseline comes from the visually-last flex line, and within it the item nearest that line's visual
    // end (the opposite end to flexItemForFirstBaseline, hence the negated reverse). wrap-reverse makes the
    // visually-last line the logically-first line.
    auto& flexItems = m_flexItems;
    bool reverse = !(flexBox().style().flexDirection() == FlexDirection::RowReverse || flexBox().style().flexDirection() == FlexDirection::ColumnReverse);
    if (FlexFormattingUtils::isWrapReverse(flexBox()))
        return baselineFlexItemInLine(0, m_flexLayoutResult->numberOfFlexItemsOnFirstLine, reverse);
    return baselineFlexItemInLine(flexItems.size() - m_flexLayoutResult->numberOfFlexItemsOnLastLine, m_flexLayoutResult->numberOfFlexItemsOnLastLine, reverse);
}

CheckedPtr<const RenderBox> FlexLayout::baselineFlexItemInLine(size_t lineStart, size_t itemCount, bool reverse) const
{
    // A flex line is the slice [lineStart, lineStart + itemCount) of the flex items (items are collected into lines
    // in order). Scanning it in the given direction, return the first item that participates in baseline alignment
    // (baseline self-alignment, the main axis is the item's inline axis, and no auto margins in the cross axis), or
    // the first item scanned when none participate.
    auto& flexItems = m_flexItems;
    CheckedPtr<const RenderBox> fallback = nullptr;
    for (size_t i = 0; i < itemCount; ++i) {
        CheckedPtr flexItem = flexItems[lineStart + (reverse ? itemCount - 1 - i : i)].get();
        if (!flexItem)
            continue;
        if (!fallback)
            fallback = flexItem;
        auto position = FlexFormattingUtils::alignmentForFlexItem(*flexItem);
        if ((position == ItemPosition::Baseline || position == ItemPosition::LastBaseline)
            && FlexFormattingUtils::mainAxisIsFlexItemInlineAxis(*flexItem) && !FlexFormattingUtils::hasAutoMarginsInCrossAxis(*flexItem))
            return flexItem;
    }
    return fallback;
}

LayoutUnit FlexLayout::staticMainAxisPositionForPositionedFlexItem(const RenderBox& flexItem)
{
    FlexFormattingUtils utils { flexBox() };
    auto flexItemMainExtent = utils.resolveMainAxisMarginExtentForFlexItem(flexItem) + utils.mainAxisExtentForFlexItem(flexItem);
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
    auto align = FlexFormattingUtils::alignmentForFlexItem(flexItem);
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

void FlexLayout::setFlexItemContentLogicalHeightFromLayout(const RenderBox& flexItem, LayoutUnit height)
{
    // Captures a flex item's content logical height mid-layout, before computeLogicalHeight
    // applies fixed/min/max or any overridingBorderBoxLogicalHeight set by the flex container for stretch alignment.
    // Reading logicalHeight() at the end of the flex item's layout would give the constrained/overridden value, not the content height the flex algorithm needs.
    auto canSetFlexItemContentLogicalHeight = !flexItem.isFloatingOrOutOfFlowPositioned() && !flexItem.shouldComputeLogicalHeightFromAspectRatio() && !is<RenderReplaced>(flexItem);
    if (!canSetFlexItemContentLogicalHeight)
        return;
    if (flexItem.overridingBorderBoxLogicalHeight())
        return;
    m_flexItemContentCache.setContentLogicalHeight(flexItem, height);
}

void FlexLayout::invalidateBlockAxisSizeForFlexItem(const RenderBox& flexItem)
{
    m_flexItemContentCache.clearBlockAxisSize(flexItem);
}

void FlexLayout::flexItemWillBeRemoved(const RenderBox& flexItem)
{
    m_flexItemContentCache.remove(flexItem);
}

std::optional<bool> FlexLayout::isFlexItemHeightDefiniteInLayoutPhase(const RenderBox& flexItem) const
{
    // A percentage resolved against a flex item resolves against the item's overriding logical height, so the
    // question is whether the flex algorithm has computed that height yet. Which step computes it depends on the item:
    // main-axis sizing does when the item's block axis is the container's main axis, cross-axis stretching does
    // otherwise.
    auto layoutPhase = this->layoutPhase();
    if (!layoutPhase)
        return { };

    switch (*layoutPhase) {
    case LayoutPhase::PreparingFlexItems:
    case LayoutPhase::ComputingFlexBaseSizes:
        // The algorithm has not sized anything yet -- PreparingFlexItems is the setup that collects the items and
        // measures the container's intrinsic widths. No flexed height exists.
        return false;
    case LayoutPhase::MainAxisItemSizing:
    case LayoutPhase::MainAxisAlignment:
        // Only the main size is definite, so the height is usable when the item's block axis is the main axis.
        // Multi-line column flow re-runs main-axis item sizing from the alignment step, once the container's main
        // size is known, so that phase lands here too.
        return !FlexFormattingUtils::mainAxisIsFlexItemInlineAxis(flexItem);
    case LayoutPhase::CrossAxisItemSizing:
    case LayoutPhase::CrossAxisAlignment:
        // Both axes are done, so the height is definite whichever axis it is.
        return true;
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

bool FlexLayout::hasDefiniteSizeForPercentResolution(const RenderBox& flexItem)
{
    if (FlexFormattingUtils::mainAxisIsFlexItemInlineAxis(flexItem))
        return FlexFormattingUtils::alignmentForFlexItem(flexItem) == ItemPosition::Stretch;

    // Flexbox 9.8 rule 2: definite flex-basis makes post-flexing main size definite.
    auto* flexLayoutState = m_flexLayoutState ? &*m_flexLayoutState : nullptr;
    if (FlexIntegrationUtils::flexItemMainSizeIsDefinite(flexItem, FlexFormattingUtils::flexBasisForFlexItem(flexItem), flexLayoutState))
        return true;

    // Flexbox 9.8 rule 1: definite container main size makes post-flexing sizes definite.
    return FlexIntegrationUtils::canResolvePercentAgainstContainerBlockSize(flexItem, RenderBox::UpdatePercentageHeightDescendants::Yes, flexLayoutState);
}

}
}
