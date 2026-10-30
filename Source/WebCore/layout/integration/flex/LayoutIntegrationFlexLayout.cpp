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
    // The static position setStaticPositionForPositionedLayout computes after layout is this content-box corner plus
    // alignment offsets that need the box's size. Starting from the same corner means a box with no offset is already
    // at its static position and isn't laid out a second time.
    LayoutUnit staticInlinePosition = flexBox().startOffsetForContent();
    if (layer->staticInlinePosition() != staticInlinePosition) {
        layer->setStaticInlinePosition(staticInlinePosition);
        if (outOfFlowBox.style().hasStaticInlinePosition(flexBox().writingMode().isHorizontal()))
            outOfFlowBox.setChildNeedsLayout(MarkingBehavior::MarkOnlyThis);
    }

    LayoutUnit staticBlockPosition = flexBox().borderAndPaddingBefore();
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
        .minimumLineCount = FlexFormattingUtils::minimumLineCount(flexBox()),
        .isHorizontalFlow = FlexFormattingUtils::isHorizontalFlow(flexBox()),
        .isColumnFlow = FlexFormattingUtils::isColumnFlow(flexBox()),
        .isMultiline = FlexFormattingUtils::isMultiline(flexBox()),
        .isWrapReverse = FlexFormattingUtils::isWrapReverse(flexBox()),
        .isBalance = FlexFormattingUtils::isBalance(flexBox()),
        .isColumnOrRowReverse = utils.isColumnOrRowReverse(),
        .isLeftToRightFlow = utils.isLeftToRightFlow(),
        .crossAxisDirection = utils.crossAxisDirection(),
        .flowAwareBorderAndPaddingStart = utils.flowAwareBorderAndPaddingStart(),
        .flowAwareBorderAndPaddingEnd = utils.flowAwareBorderAndPaddingEnd(),
        .flowAwareBorderAndPaddingBefore = utils.flowAwareBorderAndPaddingBefore(),
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
    auto contentBoxLogicalWidth = flexBox().contentBoxLogicalWidth();
    for (auto& renderer : m_flexItems) {
        // buildFlexItemList has just collected these from the container's children, so none of them is gone.
        CheckedRef flexItem = *renderer;

        // Before running the flex algorithm, 'auto' has a margin of 0. Percentages resolve against the container's content box
        // width, even for margin-top and margin-bottom, and lengths against the item's own zoom.
        auto usedStyle = flexItem->usedStyle();
        auto margins = LayoutBoxExtent { usedStyle.marginTop(contentBoxLogicalWidth).value_or(0_lu), usedStyle.marginRight(contentBoxLogicalWidth).value_or(0_lu), usedStyle.marginBottom(contentBoxLogicalWidth).value_or(0_lu), usedStyle.marginLeft(contentBoxLogicalWidth).value_or(0_lu) };

        auto everHadLayout = flexItem->everHadLayout();
        if (flexItem->shouldInvalidateContentWidths())
            flexItem->invalidateContentLogicalWidths(MarkingBehavior::MarkOnlyThis);
        flexBox().updateBlockChildDirtyBitsBeforeLayout(relayoutChildren, flexItem.get());
        flexLayoutItems.append({ flexItem.get(), constraints.isHorizontalFlow, margins, everHadLayout, relayoutChildren == RelayoutChildren::Yes });
    }
    return flexLayoutItems;
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

    auto flexLayoutStateScope = SetForScope { m_flexLayoutState, FlexLayoutState { flexBox().hasDefiniteLogicalHeight() } };
    auto integrationUtils = FlexIntegrationUtils { flexBox(), *m_flexLayoutState, m_flexItemContentCache };
    m_flexLayoutResult = WebCore::FlexFormattingContext(flexBox(), integrationUtils, constraints, *m_flexLayoutState).layout(flexLayoutItems);
}

std::pair<LayoutUnit, LayoutUnit> FlexLayout::computeIntrinsicLogicalWidths()
{
    auto scrollbarWidth = flexBox().scrollbarLogicalWidth();

    if (flexBox().shouldApplySizeOrInlineSizeContainment()) {
        if (auto width = flexBox().explicitIntrinsicInnerLogicalWidth())
            return { width.value() + scrollbarWidth, width.value() + scrollbarWidth };
        return { scrollbarWidth, scrollbarWidth };
    }

    // FIXME: We're ignoring flex-basis here and we shouldn't. We can't start
    // honoring it though until the flex shorthand stops setting it to 0. See
    // https://bugs.webkit.org/show_bug.cgi?id=116117 and
    // https://crbug.com/240765.
    auto [legendMinWidth, legendMaxWidth] = flexBox().computeIntrinsicLogicalWidthsForFieldsetLegend();

    Vector<FlexFormattingContext::IntrinsicWidthContribution> contributions;
    for (CheckedPtr flexItem = flexBox().firstChildBox(); flexItem; flexItem = flexItem->nextSiblingBox()) {
        if (flexItem->isOutOfFlowPositioned() || flexItem->isExcludedFromNormalLayout())
            continue;

        LayoutUnit margin = flexBox().marginIntrinsicLogicalWidthForChild(*flexItem);
        auto [minContentInParentInlineAxis, maxContentInParentInlineAxis] = intrinsicLogicalWidthsForFlexItem(*flexItem);
        contributions.append({ minContentInParentInlineAxis + margin, maxContentInParentInlineAxis + margin });
    }

    auto [minimumContentSize, maximumContentSize] = FlexFormattingContext::minimumMaximumContentSize(flexBox(), contributions);
    // CSS Flexbox 9.9.2
    if (FlexFormattingUtils::isColumnFlow(flexBox()) && FlexFormattingUtils::isMultiline(flexBox())) {
        if (auto maximumContentSizeFromFlexLines = maximumContentSizeForMultilineColumn())
            maximumContentSize = *maximumContentSizeFromFlexLines;
    }
    maximumContentSize = std::max(minimumContentSize, maximumContentSize);

    // Due to negative margins, it is possible that we calculated a negative
    // intrinsic width. Make sure that we never return a negative width.
    minimumContentSize = std::max(0_lu, minimumContentSize);
    maximumContentSize = std::max(0_lu, maximumContentSize);

    return { std::max(minimumContentSize, legendMinWidth) + scrollbarWidth, std::max(maximumContentSize, legendMaxWidth) + scrollbarWidth };
}

std::pair<LayoutUnit, LayoutUnit> FlexLayout::intrinsicLogicalWidthsForFlexItem(RenderBox& flexItem) const
{
    if (!flexBox().writingMode().isOrthogonal(flexItem.writingMode()))
        return flexBox().computeChildIntrinsicLogicalWidths(flexItem);

    auto intrinsicBlockSize = flexItem.computeIntrinsicLogicalHeight();
    return { intrinsicBlockSize, intrinsicBlockSize };
}

// CSS Flexbox 9.9.2: the flex algorithm runs with the items at their max-content widths and content-based heights.
class FlexLayout::FlexItemsAtMaxContentWidthScope {
public:
    FlexItemsAtMaxContentWidthScope(FlexLayout&, const FlexLayoutItems&, const FlexFormattingContext::SizeList& flexItemsMaxContentWidths);
    ~FlexItemsAtMaxContentWidthScope();

    FlexLayoutState& flexLayoutState() LIFETIME_BOUND { return *m_flexLayout.m_flexLayoutState; }

private:
    struct OverridingSizes {
        std::optional<LayoutUnit> borderBoxLogicalWidth;
        std::optional<LayoutUnit> borderBoxLogicalHeight;
    };

    FlexLayout& m_flexLayout;
    const FlexLayoutItems& m_flexLayoutItems;
    Vector<OverridingSizes, 4> m_overridingSizes;
    SetForScope<std::optional<FlexLayoutState>> m_flexLayoutStateScope;
};

FlexLayout::FlexItemsAtMaxContentWidthScope::FlexItemsAtMaxContentWidthScope(FlexLayout& flexLayout, const FlexLayoutItems& flexLayoutItems, const FlexFormattingContext::SizeList& flexItemsMaxContentWidths)
    : m_flexLayout(flexLayout)
    , m_flexLayoutItems(flexLayoutItems)
    , m_flexLayoutStateScope(flexLayout.m_flexLayoutState, FlexLayoutState { flexLayout.flexBox().hasDefiniteLogicalHeight() })
{
    ASSERT(flexLayoutItems.size() == flexItemsMaxContentWidths.size());
    m_overridingSizes.reserveInitialCapacity(flexLayoutItems.size());
    for (size_t index = 0; index < flexLayoutItems.size(); ++index) {
        auto& flexLayoutItem = flexLayoutItems[index];
        CheckedRef flexItem = flexLayoutItem.renderer;
        m_overridingSizes.append({ flexItem->overridingBorderBoxLogicalWidth(), flexItem->overridingBorderBoxLogicalHeight() });

        flexItem->clearOverridingBorderBoxLogicalHeight();
        if (!flexLayoutItem.mainAxisIsInlineAxis)
            flexItem->setOverridingBorderBoxLogicalWidth(flexItemsMaxContentWidths[index]);
    }
}

FlexLayout::FlexItemsAtMaxContentWidthScope::~FlexItemsAtMaxContentWidthScope()
{
    auto flexItemsNeedLayout = false;
    for (size_t index = 0; index < m_flexLayoutItems.size(); ++index) {
        CheckedRef flexItem = m_flexLayoutItems[index].renderer;
        auto& overridingSizes = m_overridingSizes[index];
        flexItem->clearOverridingSize();
        if (auto width = overridingSizes.borderBoxLogicalWidth)
            flexItem->setOverridingBorderBoxLogicalWidth(*width);
        if (auto height = overridingSizes.borderBoxLogicalHeight)
            flexItem->setOverridingBorderBoxLogicalHeight(*height);

        if (flexLayoutState().hasFlexItemCompletedLayout(flexItem)) {
            flexItem->setChildNeedsLayout(MarkingBehavior::MarkOnlyThis);
            flexItemsNeedLayout = true;
        }
    }
    if (!flexItemsNeedLayout)
        return;

    // Computing a box's intrinsic widths may not mark it for layout, but the items were just laid out at their max-content widths.
    // Its layout usually follows anyway; grid and flex parents can ask a laid-out container again.
    RenderObject::SetLayoutNeededForbiddenScope allowSetNeedsLayout(m_flexLayout.flexBox(), false);
    m_flexLayout.flexBox().setChildNeedsLayout();
}

std::optional<LayoutUnit> FlexLayout::maximumContentSizeForMultilineColumn()
{
    ASSERT(FlexFormattingUtils::isColumnFlow(flexBox()) && FlexFormattingUtils::isMultiline(flexBox()));

    Vector<CheckedRef<RenderBox>> flexItems;
    for (CheckedPtr flexItem = flexBox().firstChildBox(); flexItem; flexItem = flexItem->nextSiblingBox()) {
        if (!flexItem->isOutOfFlowPositioned() && !flexItem->isExcludedFromNormalLayout())
            flexItems.append(*flexItem);
    }

    auto constraints = flexLayoutConstraints();
    if (!FlexFormattingUtils::maximumContentSizeNeedsFlexLines(constraints, flexItems))
        return { };

    std::stable_sort(flexItems.begin(), flexItems.end(), [](auto& a, auto& b) {
        return a->style().order().value < b->style().order().value;
    });

    FlexLayoutItems flexLayoutItems;
    FlexFormattingContext::SizeList flexItemsMaxContentWidths;
    for (auto& flexItem : flexItems) {
        // Against zero, like marginIntrinsicLogicalWidthForChild: percentages refer to the width being computed.
        auto usedStyle = flexItem->usedStyle();
        auto margins = LayoutBoxExtent { usedStyle.marginTop(0_lu).value_or(0_lu), usedStyle.marginRight(0_lu).value_or(0_lu), usedStyle.marginBottom(0_lu).value_or(0_lu), usedStyle.marginLeft(0_lu).value_or(0_lu) };
        flexLayoutItems.append({ flexItem.get(), constraints.isHorizontalFlow, margins, flexItem->everHadLayout(), true });
        flexItemsMaxContentWidths.append(intrinsicLogicalWidthsForFlexItem(flexItem).second);
    }

    auto flexItemsAtMaxContentWidthScope = FlexItemsAtMaxContentWidthScope { *this, flexLayoutItems, flexItemsMaxContentWidths };
    auto integrationUtils = FlexIntegrationUtils { flexBox(), flexItemsAtMaxContentWidthScope.flexLayoutState(), m_flexItemContentCache };
    auto flexFormattingContext = FlexFormattingContext { flexBox(), integrationUtils, constraints, flexItemsAtMaxContentWidthScope.flexLayoutState() };
    return flexFormattingContext.maximumContentSizeForMultilineColumn(flexLayoutItems, flexItemsMaxContentWidths);
}

std::optional<LayoutUnit> FlexLayout::firstLineBaseline() const
{
    if ((flexBox().isWritingModeRoot() && !flexBox().isFlexItem()) || !m_flexLayoutResult || !m_flexLayoutResult->numberOfFlexItemsOnFirstLine || flexBox().shouldApplyLayoutContainment())
        return { };

    CheckedPtr baselineFlexItem = flexItemForFirstBaseline();
    if (!baselineFlexItem)
        return { };
    return baselineFromFlexItem(*baselineFlexItem, ItemPosition::Baseline);
}

std::optional<LayoutUnit> FlexLayout::lastLineBaseline() const
{
    if (flexBox().isWritingModeRoot() || !m_flexLayoutResult || !m_flexLayoutResult->numberOfFlexItemsOnLastLine || flexBox().shouldApplyLayoutContainment())
        return { };

    CheckedPtr baselineFlexItem = flexItemForLastBaseline();
    if (!baselineFlexItem)
        return { };
    return baselineFromFlexItem(*baselineFlexItem, ItemPosition::LastBaseline);
}

// The container's first or last baseline, from the flex item flexItemForFirstBaseline or flexItemForLastBaseline picked.
LayoutUnit FlexLayout::baselineFromFlexItem(const RenderBox& baselineFlexItem, ItemPosition baselinePreference) const
{
    FlexFormattingUtils utils { flexBox() };
    auto baseline = std::optional<LayoutUnit> { };
    if (!FlexFormattingUtils::isColumnFlow(flexBox()) && !FlexFormattingUtils::mainAxisIsFlexItemInlineAxis(baselineFlexItem))
        baseline = utils.crossAxisExtentForFlexItem(baselineFlexItem);
    else if (FlexFormattingUtils::isColumnFlow(flexBox()) && FlexFormattingUtils::mainAxisIsFlexItemInlineAxis(baselineFlexItem))
        baseline = utils.mainAxisExtentForFlexItem(baselineFlexItem);
    else if (auto flexItemBaseline = baselinePreference == ItemPosition::LastBaseline ? baselineFlexItem.lastLineBaseline() : baselineFlexItem.firstLineBaseline())
        baseline = flexItemBaseline;
    else {
        // FIXME: We should pass |direction| into firstLineBoxBaseline and stop bailing out if we're a writing mode root.
        // This would also fix some cases where the flexbox is orthogonal to its container.
        auto direction = flexBox().isHorizontalWritingMode() ? BoxAxis::Horizontal : BoxAxis::Vertical;
        auto flexboxWritingMode = flexBox().style().writingMode();
        auto dominantBaseline = BaselineAlignment::dominantBaseline(flexboxWritingMode);
        baseline = BaselineAlignment::synthesizedBaseline(baselineFlexItem, dominantBaseline, flexboxWritingMode, direction, BaselineSynthesisEdge::BorderBox);
    }
    auto result = baselineFlexItem.logicalTop() + *baseline;
    // CSS Align §9.1: if a scroll container's baseline is outside its border edge, clamp to the border edge.
    if (FlexFormattingUtils::isHorizontalFlow(flexBox()) ? flexBox().isScrollContainerY() : flexBox().isScrollContainerX())
        return std::max(0_lu, std::min(result, flexBox().logicalHeight()));
    return result;
}

CheckedPtr<const RenderBox> FlexLayout::flexItemForFirstBaseline() const
{
    ASSERT(m_flexLayoutResult);

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
    ASSERT(m_flexLayoutResult);

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
    // An out-of-flow item has no baseline-sharing group, so baseline values use their fallback alignment: start for first baseline and end for last baseline.
    if (align == ItemPosition::LastBaseline)
        align = ItemPosition::FlexEnd;
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
    // Not while computing the intrinsic widths; see computeBlockAxisContentSizeForFlexItem.
    if (flexBox().layoutContext().isComputingIntrinsicLogicalWidthFor(flexBox()))
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
    // A percentage (or stretch) flex-basis is definite only when the container's block size is, which is rule 1 below,
    // or when it is used as content and the aspect-ratio makes that definite, which is checked after rule 1.
    auto* flexLayoutState = m_flexLayoutState ? &*m_flexLayoutState : nullptr;
    auto flexBasis = FlexFormattingUtils::flexBasisForFlexItem(flexItem);
    if (!flexBasis.isPercentOrCalculated() && !flexBasis.isStretch() && FlexIntegrationUtils::flexItemMainSizeIsDefinite(flexItem, flexBasis, flexLayoutState))
        return true;

    // Flexbox 9.8 rule 1: definite container main size makes post-flexing sizes definite.
    if (FlexIntegrationUtils::canResolvePercentAgainstContainerBlockSize(flexItem, RenderBox::UpdatePercentageHeightDescendants::Yes, flexLayoutState))
        return true;

    // At this point, the container main size is assumed to be indefinite.
    ASSERT(!FlexIntegrationUtils::isFlexBoxBlockSizeDefiniteForFlexItem(flexItem, flexLayoutState).value_or(false));

    // Flexbox 9.8 rule 2 + Flexbox 9.2.3.B: check if a non-fixed flexBasis can also be definite.
    // A flex item has a definite inline size if it has a preferred aspect ratio, a used flex basis of 'content', and a definite cross size.
    // https://drafts.csswg.org/css-flexbox/#algo-main-item
    bool hasPreferredAspectRatio = FlexFormattingUtils::flexItemHasAspectRatio(flexItem) && flexItem.preferredAspectRatioAsSize().aspectRatioDouble() > 0;
    // https://drafts.csswg.org/css-flexbox/#flex-basis-property
    // If a value would resolve to auto for width, it instead resolves to content for flex-basis.
    // If a value would resolve against a containing block with an indefinite size, the used value for flex-basis is content.
    bool usedFlexBasisIsContent = flexBasis.isAuto() || flexBasis.isPercentOrCalculated() || flexBasis.isContent();

    auto& crossSize = flexItem.style().logicalWidth();
    // A percentage or stretch cross size is unknown while the container computes its intrinsic widths.
    bool hasDefiniteCrossSize = crossSize.isFixed()
        || ((crossSize.isPercentOrCalculated() || crossSize.isStretch()) && !flexBox().layoutContext().isComputingIntrinsicLogicalWidthFor(flexBox()))
        || FlexFormattingUtils::hasDefiniteCrossSizeForFlexItem(flexItem);

    return usedFlexBasisIsContent && hasPreferredAspectRatio && hasDefiniteCrossSize;
}

}
}
