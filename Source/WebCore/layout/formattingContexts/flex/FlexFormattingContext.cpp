/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "FlexFormattingContext.h"

#include "FlexFormattingUtils.h"
#include "FlexIntegrationUtils.h"
#include "FlexLayoutState.h"
#include "InspectorInstrumentation.h"
#include "RenderBoxInlines.h"
#include "RenderFlexibleBox.h"
#include "RenderLayer.h"
#include "RenderObjectInlines.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "WritingMode.h"

#include <wtf/OrderedHashSet.h>

namespace WebCore {

static LayoutUnit constrainSizeByMinMax(LayoutUnit size, std::pair<LayoutUnit, LayoutUnit> minMaxSizes)
{
    return std::max(minMaxSizes.first, std::min(size, minMaxSizes.second));
}

FlexFormattingContext::FlexFormattingContext(LayoutIntegration::FlexIntegrationUtils& integration, const FlexLayoutConstraints& constraints)
    : m_flexBox(integration.flexBox())
    , m_integrationUtils(integration)
    , m_flexFormattingUtils(integration.flexBox())
    , m_constraints(constraints)
{
}

FlexFormattingContext::Result FlexFormattingContext::layout(FlexLayoutItems& flexItems)
{
    ASSERT(!flexItems.isEmpty());

    FlexLines flexLines;
    SizeList flexItemsMainSizeList;
    LinesCrossSizeList flexLinesCrossSizeList;

    InspectorInstrumentation::flexibleBoxRendererBeganLayout(m_flexBox);

    // 9.2. Determine the flex base size and hypothetical main size of each item RenderFlexibleBox collected.
    auto flexBaseAndHypotheticalMainSizeList = computeFlexBaseAndHypotheticalMainSizes(flexItems);

    auto performContentSizing = [&] {
        // 9.3. (#5) Collect the flex items into flex lines.
        flexLines = computeFlexLines(flexItems, flexBaseAndHypotheticalMainSizeList.span());
        // 9.3. (#6) Resolve the flexible lengths to find the used main size of each item.
        flexItemsMainSizeList = computeMainSizeForFlexItems(flexItems, flexLines, flexBaseAndHypotheticalMainSizeList.span());
        trimCrossAxisMarginsForFlexItems(flexItems, flexLines);
        layoutFlexItems(flexItems, flexItemsMainSizeList.span());
        // 9.4. (#7) Determine the hypothetical cross size of each item.
        auto flexItemsHypotheticalCrossSizeList = hypotheticalCrossSizeForFlexItems(flexItems);
        // 9.4. (#8) Calculate the cross size of each flex line.
        flexLinesCrossSizeList = crossSizeForFlexLines(flexLines, flexItems, flexItemsHypotheticalCrossSizeList);
    };
    performContentSizing();

    LayoutUnit crossAxisStartEdge;
    LinesCrossPositionList flexLinesCrossPositionList;
    FlexContainerUsedExtents flexContainerUsedExtents;
    PositionList flexItemsPositionList;
    SizeList flexItemsCrossSizeList;

    auto performContentAlignment = [&] {
        // Stack the flex lines along the cross axis (their cross sizes are known); the block extent this returns is
        // what finalizes the container's logical height for row flow below.
        auto flexContentBlockExtent = LayoutUnit { };
        flexLinesCrossPositionList = computeFlexLineCrossPositions(flexLines, flexLinesCrossSizeList, flexContentBlockExtent);
        // 9.5. (#12) Main-Axis Alignment.
        auto columnMainContentExtent = LayoutUnit { };
        flexItemsPositionList = handleMainAxisAlignment(flexLines, flexItems, flexItemsMainSizeList, flexLinesCrossPositionList, columnMainContentExtent);
        setFlexItemCountsForFirstAndLastLine(flexLines);
        // 9.6. (#15) Finalize the container's logical height from the block extent computeFlexLineCrossPositions built
        // (row flow) or the main content extent handleMainAxisAlignment built while placing the items (column flow):
        // apply the empty-line minimum, then resolve it against the container's own specified/min/max height and box-sizing.
        flexContainerUsedExtents = integrationUtils().updateFlexContainerLogicalHeight(m_constraints.isColumnFlow ? columnMainContentExtent : flexContentBlockExtent);
        // Multi-line column flex only knows its main size now, so re-resolve the flexible lengths of any lines that were left short.
        distributeMainAxisFreeSpaceForMultilineColumnIfNeeded(flexLines, flexItems, flexBaseAndHypotheticalMainSizeList.span(), flexItemsMainSizeList, flexItemsPositionList, flexLinesCrossPositionList, flexContainerUsedExtents.blockContentBox);
        // 9.5. (#12) For column-reverse, reposition each line's items from the finalized container main-axis end.
        reverseColumnLinesFromContainerMainEndIfNeeded(flexLines, flexItems, flexItemsMainSizeList, flexItemsPositionList, flexLinesCrossPositionList, flexContainerUsedExtents.blockContentBox, flexContainerUsedExtents.blockBorderBox);
        // Cross-Axis Alignment: with the container's cross size now final, run the remaining cross-axis steps
        // (§9.4 #9 and #11, §9.6 #13, #14 and #16) here rather than in spec-number order. First record where the
        // lines start on the cross axis, for the wrap-reverse flip in computeFlexItemRects.
        crossAxisStartEdge = flexLinesCrossPositionList.isEmpty() ? 0_lu : flexLinesCrossPositionList[0];
        // If we have a single line flexbox, the line height is all the available space. For flex-direction: row,
        // this means we need to use the height, so we do this after calling updateLogicalHeight.
        if (!m_constraints.isMultiline && !flexLinesCrossSizeList.isEmpty())
            flexLinesCrossSizeList[0] = flexContainerUsedExtents.crossContentBox;
        // 9.4. (#9) Handle 'align-content: stretch' and 9.6. (#16) align all flex lines per align-content.
        handleCrossAxisAlignmentForFlexLines(flexLines, flexItemsPositionList, flexLinesCrossPositionList, flexLinesCrossSizeList, flexContainerUsedExtents.crossContentBox);
        // 9.4. (#11) Determine the used cross size of each flex item.
        flexItemsCrossSizeList = computeCrossSizeForFlexItems(flexLines, flexItems, flexLinesCrossSizeList, flexContainerUsedExtents.crossContentBox);
        // 9.6. (#13 - #14) Resolve cross-axis auto margins and align each item per align-self.
        handleCrossAxisAlignmentForFlexItems(flexLines, flexItems, flexItemsCrossSizeList, flexLinesCrossSizeList, flexItemsPositionList);
    };
    performContentAlignment();

    computeFlexItemRects(flexLines, flexItems, flexItemsPositionList, flexLinesCrossPositionList, flexLinesCrossSizeList, flexItemsCrossSizeList, crossAxisStartEdge, flexContainerUsedExtents.crossContentBox, flexContainerUsedExtents.crossBorderBox);
    return m_result;
}

FlexFormattingContext::FlexBaseAndHypotheticalMainSizeList FlexFormattingContext::computeFlexBaseAndHypotheticalMainSizes(FlexLayoutItems& flexItems)
{
    layoutState().setPhase(FlexLayoutState::Phase::ComputingFlexBaseSizes);

    FlexBaseAndHypotheticalMainSizeList flexBaseAndHypotheticalMainSizeList(flexItems.size());
    for (size_t index = 0; index < flexItems.size(); ++index) {
        auto& flexItem = flexItems[index];
        auto flexBase = flexBaseSizeForFlexItem(flexItem);
        if (!flexItem.mainAxisIsInlineAxis) {
            // flexBaseSizeForFlexItem just laid out an orthogonal item, so its physical margins are now resolved.
            CheckedRef renderer = flexItem.renderer;
            flexItem.mainAxisMargin = m_constraints.isHorizontalFlow ? renderer->horizontalMarginExtent() : renderer->verticalMarginExtent();
        }
        auto minMaxMainSizes = minMaxMainSizesForFlexItem(flexItem);
        // The hypothetical main size is the item's flex base size clamped according to its used min and max main sizes.
        flexBaseAndHypotheticalMainSizeList[index] = { flexBase, std::max(minMaxMainSizes.first, std::min(flexBase, minMaxMainSizes.second)), minMaxMainSizes };
        // FIXME: Figure out if we can do this outside of the loop.
        layoutState().resetFlexBoxBlockSizeDefiniteness();
    }
    return flexBaseAndHypotheticalMainSizeList;
}

FlexFormattingContext::FlexLines FlexFormattingContext::computeFlexLines(FlexLayoutItems& flexItems, std::span<const FlexBaseAndHypotheticalMainSize> flexBaseAndHypotheticalMainSizeList)
{
    layoutState().setPhase(FlexLayoutState::Phase::CollectingLines);
    // 9.3. (#5) Collect flex items into flex lines: a single-line container collects all items into one line; a
    // multi-line container collects consecutive items until the next item's outer hypothetical main size would not
    // fit in the inner main size. The first uncollected item is always collected, even if it does not fit.
    auto mainAxisAvailableSpace = m_constraints.mainAxisAvailableSpace;
    auto gapBetweenItems = flexFormattingUtils().computeGap(FlexFormattingUtils::GapType::BetweenItems);
    FlexLines flexLines;
    size_t nextIndex = 0;
    while (nextIndex < flexItems.size()) {
        auto lineStartIndex = nextIndex;
        LayoutUnit sumFlexBaseSize;
        LayoutUnit sumHypotheticalMainSize;
        // Trim the main-axis margin of the item at the start of the flex line.
        if (flexFormattingUtils().shouldTrimMainAxisMarginStart())
            trimMainAxisMarginStart(flexItems[nextIndex]);
        for (; nextIndex < flexItems.size(); ++nextIndex) {
            const auto& flexLayoutItem = flexItems[nextIndex];
            ASSERT(!flexLayoutItem.renderer->isOutOfFlowPositioned());
            if (m_constraints.isMultiline && (sumHypotheticalMainSize + flexLayoutItem.hypotheticalMainAxisMarginBoxSize(flexBaseAndHypotheticalMainSizeList[nextIndex].hypotheticalMainSize) > mainAxisAvailableSpace && !canFitItemWithTrimmedMarginEnd(flexLayoutItem, flexBaseAndHypotheticalMainSizeList[nextIndex].hypotheticalMainSize, sumHypotheticalMainSize, mainAxisAvailableSpace)) && nextIndex > lineStartIndex)
                break;
            sumFlexBaseSize += flexLayoutItem.flexBaseMarginBoxSize(flexBaseAndHypotheticalMainSizeList[nextIndex].flexBase) + gapBetweenItems;
            sumHypotheticalMainSize += flexLayoutItem.hypotheticalMainAxisMarginBoxSize(flexBaseAndHypotheticalMainSizeList[nextIndex].hypotheticalMainSize) + gapBetweenItems;
        }

        // We added a gap after every item but there shouldn't be one after the last item, so subtract it here. Note
        // that sums might be negative here due to negative margins in flex items.
        sumHypotheticalMainSize -= gapBetweenItems;
        sumFlexBaseSize -= gapBetweenItems;

        // Trim the main-axis margin of the item at the end of the flex line.
        if (flexFormattingUtils().shouldTrimMainAxisMarginEnd()) {
            auto& lastItem = flexItems[nextIndex - 1];
            removeMarginEndFromFlexSizes(lastItem, sumFlexBaseSize, sumHypotheticalMainSize);
            trimMainAxisMarginEnd(lastItem);
        }

        flexLines.ranges.append({ lineStartIndex, nextIndex });
        flexLines.hypotheticalMainSizes.append(sumHypotheticalMainSize);
    }

    for (auto lineRange : flexLines.ranges)
        InspectorInstrumentation::flexibleBoxRendererWrappedToNextLine(m_flexBox, lineRange.end());

    return flexLines;
}

FlexFormattingContext::SizeList FlexFormattingContext::computeMainSizeForFlexItems(FlexLayoutItems& flexItems, const FlexLines& flexLines, std::span<const FlexBaseAndHypotheticalMainSize> flexBaseAndHypotheticalMainSizeList)
{
    layoutState().setPhase(FlexLayoutState::Phase::ResolvingFlexibleLengths);

    SizeList flexItemsMainSizeList(flexItems.size());
    for (size_t lineIndex = 0; lineIndex < flexLines.ranges.size(); ++lineIndex) {
        auto lineRange = flexLines.ranges[lineIndex];
        auto lineItems = flexItems.mutableSpan().subspan(lineRange.begin(), lineRange.distance());
        auto lineFlexBaseAndHypotheticalMainSizeList = flexBaseAndHypotheticalMainSizeList.subspan(lineRange.begin(), lineRange.distance());
        auto lineFlexItemsMainSizeList = flexItemsMainSizeList.mutableSpan().subspan(lineRange.begin(), lineRange.distance());
        auto flexContainerInnerMainSize = m_constraints.isColumnFlow ? flexFormattingUtils().columnInnerMainSize(flexLines.hypotheticalMainSizes[lineIndex]) : m_constraints.mainAxisAvailableSpace;
        resolveFlexibleLengthsForLineItems(lineItems, lineFlexBaseAndHypotheticalMainSizeList, lineFlexItemsMainSizeList, flexContainerInnerMainSize);
    }
    return flexItemsMainSizeList;
}

void FlexFormattingContext::resolveFlexibleLengthsForLineItems(std::span<FlexLayoutItem> lineItems, std::span<const FlexBaseAndHypotheticalMainSize> lineFlexBaseAndHypotheticalMainSizeList, std::span<LayoutUnit> flexItemsMainSizeList, LayoutUnit flexContainerInnerMainSize)
{
    auto nonFrozenSet = OrderedHashSet<size_t> { };
    auto availableMainSpaceForLineContent = mainAxisAvailableSpaceForItemAlignment(flexContainerInnerMainSize, lineItems.size());

    // The outer main size of an item is its content-box main size plus its main-axis border, padding and margin.
    auto outerMainSize = [&](size_t index, LayoutUnit mainSize) {
        return mainSize + lineItems[index].mainAxisBorderAndPadding + lineItems[index].mainAxisMargin;
    };

    // 9.7 (1) Determine the used flex factor: if the summed outer hypothetical main sizes are less than the flex
    // container's inner main size, use the flex grow factor for the rest of the algorithm, otherwise flex shrink.
    auto shouldUseFlexGrowFactor = [&] {
        auto hypotheticalOuterMainSizes = LayoutUnit { };
        for (size_t index = 0; index < lineItems.size(); ++index)
            hypotheticalOuterMainSizes += outerMainSize(index, lineFlexBaseAndHypotheticalMainSizeList[index].hypotheticalMainSize);
        return hypotheticalOuterMainSizes < availableMainSpaceForLineContent;
    }();

    // 9.7 (2) Size inflexible items: freeze, at its hypothetical main size, any item with a flex factor of zero, and
    // -- when growing -- any item whose flex base size is greater than its hypothetical main size, or -- when
    // shrinking -- smaller than it.
    for (size_t index = 0; index < lineItems.size(); ++index) {
        ASSERT(!lineItems[index].renderer->isOutOfFlowPositioned());
        auto& sizing = lineFlexBaseAndHypotheticalMainSizeList[index];
        auto shouldFreeze = [&] {
            if (!lineItems[index].style().flexGrow().value && !lineItems[index].style().flexShrink().value)
                return true;
            if (shouldUseFlexGrowFactor && sizing.flexBase > sizing.hypotheticalMainSize)
                return true;
            if (!shouldUseFlexGrowFactor && sizing.flexBase < sizing.hypotheticalMainSize)
                return true;
            return false;
        };
        if (shouldFreeze()) {
            flexItemsMainSizeList[index] = sizing.hypotheticalMainSize;
            continue;
        }
        nonFrozenSet.add(index);
    }

    // 9.7 (3) Calculate the initial free space: the container's inner main size minus the summed outer sizes of its
    // items, using the outer target main size for frozen items and the outer flex base size for the rest.
    auto computedFreeSpace = [&] {
        auto lineContentMainSize = LayoutUnit { };
        for (size_t index = 0; index < lineItems.size(); ++index)
            lineContentMainSize += outerMainSize(index, nonFrozenSet.contains(index) ? lineFlexBaseAndHypotheticalMainSizeList[index].flexBase : flexItemsMainSizeList[index]);
        return availableMainSpaceForLineContent - lineContentMainSize;
    };
    auto initialFreeSpace = computedFreeSpace();

    Vector<size_t> minimumViolationList;
    Vector<size_t> maximumViolationList;

    // 9.7 (4) Loop: while unfrozen items remain, distribute the remaining free space over them by their flex factors,
    // clamp each to its used min and max main sizes, and freeze the items whose clamp introduced a violation.
    while (true) {
        if (nonFrozenSet.isEmpty())
            break;

        auto remainingFreeSpace = computedFreeSpace();

        // If the unfrozen items' flex factors sum to less than one, multiply the initial free space by that sum and
        // use it as the remaining free space when its magnitude is smaller.
        auto totalFlexFactor = 0.0;
        for (auto index : nonFrozenSet)
            totalFlexFactor += shouldUseFlexGrowFactor ? lineItems[index].style().flexGrow().value : lineItems[index].style().flexShrink().value;
        if (totalFlexFactor < 1) {
            LayoutUnit fractional(initialFreeSpace * totalFlexFactor);
            if (fractional.abs() < remainingFreeSpace.abs())
                remainingFreeSpace = fractional;
        }

        // Distribute the free space proportional to the flex factors (weighting the shrink factor by the flex base
        // size). Round each item's share the way RenderBox layout does before adding it to the flex base.
        auto usedTotalFlexFactor = 0.0;
        for (auto index : nonFrozenSet)
            usedTotalFlexFactor += shouldUseFlexGrowFactor ? lineItems[index].style().flexGrow().value : lineItems[index].style().flexShrink().value * lineFlexBaseAndHypotheticalMainSizeList[index].flexBase;
        for (auto index : nonFrozenSet) {
            auto& flexItemStyle = lineItems[index].style();
            double extraSpace = 0;
            if (remainingFreeSpace > 0 && shouldUseFlexGrowFactor && usedTotalFlexFactor > 0 && std::isfinite(usedTotalFlexFactor))
                extraSpace = remainingFreeSpace * (flexItemStyle.flexGrow().value / usedTotalFlexFactor);
            else if (remainingFreeSpace < 0 && !shouldUseFlexGrowFactor && usedTotalFlexFactor > 0 && std::isfinite(usedTotalFlexFactor) && !flexItemStyle.flexShrink().isZero())
                extraSpace = remainingFreeSpace * flexItemStyle.flexShrink().value * lineFlexBaseAndHypotheticalMainSizeList[index].flexBase / usedTotalFlexFactor;
            auto flexItemSize = lineFlexBaseAndHypotheticalMainSizeList[index].flexBase;
            if (std::isfinite(extraSpace))
                flexItemSize += LayoutUnit::fromFloatRound(extraSpace);
            flexItemsMainSizeList[index] = flexItemSize;
        }

        // Fix min/max violations: clamp each unfrozen item by its used min and max main sizes; a size made larger is
        // a min violation, made smaller a max violation.
        auto totalViolation = LayoutUnit { };
        minimumViolationList.shrink(0);
        maximumViolationList.shrink(0);
        for (auto index : nonFrozenSet) {
            auto unclampedMainSize = flexItemsMainSizeList[index];
            auto clampedMainSize = constrainSizeByMinMax(unclampedMainSize, lineFlexBaseAndHypotheticalMainSizeList[index].minMaxMainSizes);
            ASSERT(clampedMainSize >= 0);
            totalViolation += clampedMainSize - unclampedMainSize;
            if (clampedMainSize < unclampedMainSize)
                maximumViolationList.append(index);
            else if (clampedMainSize > unclampedMainSize)
                minimumViolationList.append(index);
            flexItemsMainSizeList[index] = clampedMainSize;
        }

        // Freeze over-flexed items: zero total violation freezes all remaining items, a positive one freezes the min
        // violations, a negative one the max violations.
        if (!totalViolation)
            nonFrozenSet.clear();
        else if (totalViolation > 0) {
            for (auto index : minimumViolationList)
                nonFrozenSet.remove(index);
        } else {
            for (auto index : maximumViolationList)
                nonFrozenSet.remove(index);
        }
    }
}

LayoutUnit FlexFormattingContext::mainAxisAvailableSpaceForItemAlignment(LayoutUnit mainAxisAvailableSpace, size_t numberOfFlexItems) const
{
    if (numberOfFlexItems == 1)
        return mainAxisAvailableSpace;
    return mainAxisAvailableSpace - (numberOfFlexItems - 1) * flexFormattingUtils().computeGap(FlexFormattingUtils::GapType::BetweenItems);
}

LayoutUnit FlexFormattingContext::crossAxisAvailableSpaceForLineSizingAndAlignment(LayoutUnit crossAxisAvailableSpace, size_t numberOfFlexLines) const
{
    if (numberOfFlexLines == 1)
        return crossAxisAvailableSpace;
    return crossAxisAvailableSpace - (numberOfFlexLines - 1) * flexFormattingUtils().computeGap(FlexFormattingUtils::GapType::BetweenLines);
}

void FlexFormattingContext::distributeMainAxisFreeSpaceForMultilineColumnIfNeeded(const FlexLines& flexLines, FlexLayoutItems& flexItems, std::span<const FlexBaseAndHypotheticalMainSize> flexBaseAndHypotheticalMainSizeList, SizeList& flexItemsMainSizeList, PositionList& flexItemsPositionList, const LinesCrossPositionList& flexLinesCrossPositionList, LayoutUnit containerMainBlockContentExtent)
{
    // In multi-line column flex, the container's main size (height) is only known
    // after all lines are laid out. Lines whose items had flex-grow may not have
    // received enough space because the container height wasn't final during the
    // per-line pass. Re-resolve and relayout those lines now.
    if (!m_constraints.isMultiline || !m_constraints.isColumnFlow)
        return;

    auto containerMainInnerSize = containerMainBlockContentExtent;
    for (size_t lineIndex = 0; lineIndex < flexLines.ranges.size(); ++lineIndex) {
        auto lineRange = flexLines.ranges[lineIndex];
        auto lineItems = flexItems.mutableSpan().subspan(lineRange.begin(), lineRange.distance());
        auto lineFlexBaseAndHypotheticalMainSizeList = flexBaseAndHypotheticalMainSizeList.subspan(lineRange.begin(), lineRange.distance());
        auto lineFlexItemsMainSizeList = flexItemsMainSizeList.mutableSpan().subspan(lineRange.begin(), lineRange.distance());
        auto linePositions = flexItemsPositionList.mutableSpan().subspan(lineRange.begin(), lineRange.distance());

        auto lineContentMainSize = LayoutUnit { };
        for (size_t index = 0; index < lineItems.size(); ++index)
            lineContentMainSize += lineItems[index].flexedMarginBoxSize(lineFlexItemsMainSizeList[index]);
        if (lineContentMainSize >= mainAxisAvailableSpaceForItemAlignment(containerMainInnerSize, lineItems.size()))
            continue;

        resolveFlexibleLengthsForLineItems(lineItems, lineFlexBaseAndHypotheticalMainSizeList, lineFlexItemsMainSizeList, containerMainInnerSize);

        auto remainingFreeSpace = mainAxisAvailableSpaceForItemAlignment(containerMainInnerSize, lineItems.size());
        for (size_t index = 0; index < lineItems.size(); ++index)
            remainingFreeSpace -= lineItems[index].flexedMarginBoxSize(lineFlexItemsMainSizeList[index]);

        layoutFlexItemsWithMainSizes(lineItems, lineFlexItemsMainSizeList);
        placeFlexItems(flexLinesCrossPositionList[lineIndex], lineItems, linePositions, remainingFreeSpace);
    }
}

void FlexFormattingContext::trimCrossAxisMarginsForFlexItems(FlexLayoutItems& flexItems, const FlexLines& flexLines)
{
    // Cross axis margins are only trimmed on the first and last flex line.
    auto shouldTrimStart = flexFormattingUtils().shouldTrimCrossAxisMarginStart();
    auto shouldTrimEnd = flexFormattingUtils().shouldTrimCrossAxisMarginEnd();
    if (!shouldTrimStart && !shouldTrimEnd)
        return;

    for (size_t lineIndex = 0; lineIndex < flexLines.ranges.size(); ++lineIndex) {
        auto shouldTrimCrossAxisStart = shouldTrimStart && !lineIndex;
        auto shouldTrimCrossAxisEnd = shouldTrimEnd && lineIndex == flexLines.ranges.size() - 1;
        if (!shouldTrimCrossAxisStart && !shouldTrimCrossAxisEnd)
            continue;

        auto lineRange = flexLines.ranges[lineIndex];
        for (auto& flexLayoutItem : flexItems.mutableSpan().subspan(lineRange.begin(), lineRange.distance())) {
            if (shouldTrimCrossAxisStart)
                trimCrossAxisMarginStart(flexLayoutItem);
            if (shouldTrimCrossAxisEnd)
                trimCrossAxisMarginEnd(flexLayoutItem);
        }
    }
}

void FlexFormattingContext::layoutFlexItems(FlexLayoutItems& flexItems, std::span<const LayoutUnit> flexItemsMainSizeList)
{
    // The MainAxisItemSizing phase begins and ends here: every item is laid out at its resolved main size, so only the items' main-axis sizes are final at this point.
    layoutState().setPhase(FlexLayoutState::Phase::MainAxisItemSizing);
    layoutFlexItemsWithMainSizes(flexItems.mutableSpan(), flexItemsMainSizeList);
}

void FlexFormattingContext::layoutFlexItemsWithMainSizes(std::span<FlexLayoutItem> flexLayoutItems, std::span<const LayoutUnit> flexItemsMainSizeList)
{
    for (size_t index = 0; index < flexLayoutItems.size(); ++index)
        integrationUtils().layoutFlexItemWithMainSize(flexLayoutItems[index], flexItemsMainSizeList[index]);
}

FlexFormattingContext::SizeList FlexFormattingContext::hypotheticalCrossSizeForFlexItems(const FlexLayoutItems& flexItems)
{
    layoutState().setPhase(FlexLayoutState::Phase::CrossSizing);

    // 9.4. (#7) The hypothetical cross size of each item is the cross size it would have at its used main size.
    SizeList flexItemsHypotheticalCrossSizeList(flexItems.size());
    for (size_t flexItemIndex = 0; flexItemIndex < flexItems.size(); ++flexItemIndex) {
        auto& flexItem = flexItems[flexItemIndex];
        auto crossAxisIntrinsicExtentForFlexItem = flexItem.mainAxisIsInlineAxis ? flexItemIntrinsicLogicalHeight(flexItem) : flexItemIntrinsicLogicalWidth(flexItem);
        flexItemsHypotheticalCrossSizeList[flexItemIndex] = crossAxisIntrinsicExtentForFlexItem;
    }
    return flexItemsHypotheticalCrossSizeList;
}

FlexFormattingContext::LinesCrossSizeList FlexFormattingContext::crossSizeForFlexLines(const FlexLines& flexLines, const FlexLayoutItems& flexItems, const SizeList& flexItemsHypotheticalCrossSizeList)
{
    // 9.4. (#8) The used cross size of each flex line is the largest of: the summed baseline ascent and descent of
    // its baseline-aligned items, the largest outer hypothetical cross size of the remaining items, and zero. (The
    // single-line container with a definite cross size uses the inner cross size; the caller applies that.)
    LinesCrossSizeList flexLinesCrossSizeList(flexLines.ranges.size());
    for (size_t lineIndex = 0; lineIndex < flexLines.ranges.size(); ++lineIndex) {
        auto lineRange = flexLines.ranges[lineIndex];

        LayoutUnit maxFlexItemCrossAxisExtent;
        LayoutUnit maxAscent;
        LayoutUnit maxDescent = LayoutUnit::min();
        LayoutUnit lastBaselineMaxAscent;
        for (auto flexItemIndex = lineRange.begin(); flexItemIndex < lineRange.end(); ++flexItemIndex) {
            auto& flexLayoutItem = flexItems[flexItemIndex];
            ASSERT(!flexLayoutItem.renderer->isOutOfFlowPositioned());

            LayoutUnit flexItemCrossAxisMarginBoxExtent;
            auto alignment = flexFormattingUtils().alignmentForFlexItem(flexLayoutItem);
            if ((alignment == ItemPosition::Baseline || alignment == ItemPosition::LastBaseline) && !flexFormattingUtils().hasAutoMarginsInCrossAxis(flexLayoutItem)) {
                LayoutUnit ascent = flexFormattingUtils().marginBoxAscentForFlexItem(flexLayoutItem, flexFormattingUtils().crossAxisExtentForFlexItem(flexLayoutItem));
                LayoutUnit descent = (flexFormattingUtils().crossAxisMarginExtentForFlexItem(flexLayoutItem) + flexFormattingUtils().crossAxisExtentForFlexItem(flexLayoutItem)) - ascent;
                maxDescent = std::max(maxDescent, descent);
                if (alignment == ItemPosition::Baseline) {
                    maxAscent = std::max(maxAscent, ascent);
                    flexItemCrossAxisMarginBoxExtent = maxAscent + maxDescent;
                } else {
                    lastBaselineMaxAscent = std::max(lastBaselineMaxAscent, ascent);
                    flexItemCrossAxisMarginBoxExtent = lastBaselineMaxAscent + maxDescent;
                }
            } else
                flexItemCrossAxisMarginBoxExtent = flexItemsHypotheticalCrossSizeList[flexItemIndex] + flexFormattingUtils().crossAxisMarginExtentForFlexItem(flexLayoutItem);

            maxFlexItemCrossAxisExtent = std::max(maxFlexItemCrossAxisExtent, flexItemCrossAxisMarginBoxExtent);
        }
        flexLinesCrossSizeList[lineIndex] = maxFlexItemCrossAxisExtent;
    }
    return flexLinesCrossSizeList;
}

FlexFormattingContext::LinesCrossPositionList FlexFormattingContext::computeFlexLineCrossPositions(const FlexLines& flexLines, const LinesCrossSizeList& flexLinesCrossSizeList, LayoutUnit& flexContentBlockExtent)
{
    // Stack the flex lines along the cross axis, recording each line's position, and return the flex content's
    // block-axis extent (line cross sizes + inter-line gaps + cross-axis scrollbar). RenderFlexibleBox resolves
    // this into the container's logical height in updateFlexContainerLogicalHeight. Column flow's block axis is
    // its main axis, sized later while placing the items, so nothing is returned there.
    LinesCrossPositionList flexLinesCrossPositionList(flexLines.ranges.size());
    auto contentStart = m_constraints.flowAwareBorderBlock.first + m_constraints.flowAwarePaddingBlock.first;
    auto crossAxisOffset = contentStart;
    for (size_t lineIndex = 0; lineIndex < flexLines.ranges.size(); ++lineIndex) {
        flexLinesCrossPositionList[lineIndex] = crossAxisOffset;
        crossAxisOffset += flexLinesCrossSizeList[lineIndex];
    }
    if (m_constraints.isColumnFlow)
        return flexLinesCrossPositionList;

    auto totalGapBetweenLines = flexLines.ranges.size() > 1 ? flexFormattingUtils().computeGap(FlexFormattingUtils::GapType::BetweenLines) * (flexLines.ranges.size() - 1) : 0_lu;
    flexContentBlockExtent = (crossAxisOffset - contentStart) + totalGapBetweenLines + m_constraints.crossAxisScrollbarExtent;
    return flexLinesCrossPositionList;
}

FlexFormattingContext::PositionList FlexFormattingContext::handleMainAxisAlignment(const FlexLines& flexLines, FlexLayoutItems& flexItems, const SizeList& flexItemsMainSizeList, const LinesCrossPositionList& flexLinesCrossPositionList, LayoutUnit& columnMainContentExtent)
{
    layoutState().setPhase(FlexLayoutState::Phase::MainAxisAlignment);

    PositionList flexItemsPositionList(flexItems.size());
    columnMainContentExtent = 0_lu;
    for (size_t lineIndex = 0; lineIndex < flexLines.ranges.size(); ++lineIndex) {
        auto lineRange = flexLines.ranges[lineIndex];
        auto containerMainInnerSize = m_constraints.isColumnFlow ? flexFormattingUtils().columnInnerMainSize(flexLines.hypotheticalMainSizes[lineIndex]) : m_constraints.mainAxisAvailableSpace;

        // The remaining free space is the space available to the line's items (its inner main size less inter-item
        // gaps) minus their used outer main sizes.
        // (The 0..1 flex-factor adjustment means we recompute it here rather than trust the resolve step's leftover.)
        auto remainingFreeSpace = mainAxisAvailableSpaceForItemAlignment(containerMainInnerSize, lineRange.distance());
        for (auto flexItemIndex = lineRange.begin(); flexItemIndex < lineRange.end(); ++flexItemIndex) {
            ASSERT(!flexItems[flexItemIndex].renderer->isOutOfFlowPositioned());
            remainingFreeSpace -= flexItemsMainSizeList[flexItemIndex] + flexItems[flexItemIndex].mainAxisBorderAndPadding + flexItems[flexItemIndex].mainAxisMargin;
        }

        auto lineItems = flexItems.mutableSpan().subspan(lineRange.begin(), lineRange.distance());
        auto linePositions = flexItemsPositionList.mutableSpan().subspan(lineRange.begin(), lineRange.distance());
        auto mainContentExtent = placeFlexItems(flexLinesCrossPositionList[lineIndex], lineItems, linePositions, remainingFreeSpace);
        columnMainContentExtent = std::max(columnMainContentExtent, mainContentExtent);
    }
    return flexItemsPositionList;
}

FlexFormattingContext::SizeList FlexFormattingContext::computeCrossSizeForFlexItems(const FlexLines& flexLines, FlexLayoutItems& flexItems, const LinesCrossSizeList& flexLinesCrossSizeList, LayoutUnit crossContentExtent)
{
    layoutState().setPhase(FlexLayoutState::Phase::CrossAxisItemSizing);
    // Stretching items to their line's cross size relays them out, so both their main and cross sizes become final.
    SizeList flexItemsCrossSizeList(flexItems.size());
    for (size_t lineIndex = 0; lineIndex < flexLines.ranges.size(); ++lineIndex) {
        auto lineRange = flexLines.ranges[lineIndex];
        for (auto flexItemIndex = lineRange.begin(); flexItemIndex < lineRange.end(); ++flexItemIndex) {
            auto& flexLayoutItem = flexItems[flexItemIndex];
            ASSERT(!flexLayoutItem.renderer->isOutOfFlowPositioned());
            // If a flex item has align-self: stretch, its computed cross size property is auto, and neither of its cross-axis margins are auto, the used outer cross size is the used cross size
            // of its flex line, clamped according to the item's used min and max cross sizes. Otherwise, the used cross size is the item's hypothetical cross size.
            if (flexFormattingUtils().alignmentForFlexItem(flexLayoutItem) == ItemPosition::Stretch && !flexFormattingUtils().hasAutoMarginsInCrossAxis(flexLayoutItem))
                flexItemsCrossSizeList[flexItemIndex] = applyStretchAlignmentToFlexItem(flexLayoutItem, flexLinesCrossSizeList[lineIndex], crossContentExtent);
            else
                flexItemsCrossSizeList[flexItemIndex] = flexFormattingUtils().crossAxisExtentForFlexItem(flexLayoutItem);
        }
    }
    return flexItemsCrossSizeList;
}

void FlexFormattingContext::handleCrossAxisAlignmentForFlexLines(const FlexLines& flexLines, PositionList& flexItemsPositionList, LinesCrossPositionList& flexLinesCrossPositionList, LinesCrossSizeList& flexLinesCrossSizeList, LayoutUnit crossContentExtent)
{
    // 9.6. (#16) Align the flex lines within the flex container per align-content, and (#9) grow the lines to fill
    // the container for align-content: stretch. A single-line container has nothing to align.
    if (flexLines.ranges.isEmpty() || !m_constraints.isMultiline)
        return;

    auto alignedContent = m_constraints.style->alignContent().resolve(FlexFormattingUtils::contentAlignmentNormalBehavior());
    auto position = alignedContent.position();
    auto distribution = alignedContent.distribution();
    auto safety = alignedContent.overflow();

    bool isWrapReverse = m_constraints.isWrapReverse;

    auto gapBetweenLines = flexFormattingUtils().computeGap(FlexFormattingUtils::GapType::BetweenLines);
    if (position == ContentPosition::FlexStart && !gapBetweenLines && safety != OverflowAlignment::Safe && !isWrapReverse)
        return;

    size_t numLines = flexLines.ranges.size();
    LayoutUnit availableCrossAxisSpace = crossAxisAvailableSpaceForLineSizingAndAlignment(crossContentExtent, numLines);
    for (size_t i = 0; i < numLines; ++i)
        availableCrossAxisSpace -= flexLinesCrossSizeList[i];

    m_result.alignContentStartOverflow = FlexFormattingUtils::contentAlignmentStartOverflow(availableCrossAxisSpace, position, distribution, safety, isWrapReverse);
    LayoutUnit lineOffset = FlexFormattingUtils::initialAlignContentOffset(availableCrossAxisSpace, position, distribution, safety, numLines, isWrapReverse);
    for (unsigned lineNumber = 0; lineNumber < numLines; ++lineNumber) {
        flexLinesCrossPositionList[lineNumber] += lineOffset;
        // Fold this line's align-content offset into each of its items' cross-axis position.
        auto lineRange = flexLines.ranges[lineNumber];
        for (auto flexItemIndex = lineRange.begin(); flexItemIndex < lineRange.end(); ++flexItemIndex)
            flexItemsPositionList[flexItemIndex].move(0_lu, lineOffset);

        if (distribution == ContentDistribution::Stretch && availableCrossAxisSpace > 0)
            flexLinesCrossSizeList[lineNumber] += availableCrossAxisSpace / static_cast<unsigned>(numLines);

        lineOffset += FlexFormattingUtils::alignContentSpaceBetweenFlexItems(availableCrossAxisSpace, distribution, numLines) + gapBetweenLines;
    }
}

void FlexFormattingContext::handleCrossAxisAlignmentForFlexItems(const FlexLines& flexLines, FlexLayoutItems& flexItems, const SizeList& flexItemsCrossSizeList, const LinesCrossSizeList& flexLinesCrossSizeList, PositionList& flexItemsPositionList)
{
    layoutState().setPhase(FlexLayoutState::Phase::CrossAxisAlignment);
    // 9.6. (#13, #14) For each item resolve its cross-axis auto margins, then -- when neither cross-axis margin is
    // auto -- align it within its line per align-self (baseline-aligned items go through performBaselineAlignment).
    Vector<LayoutUnit> flexItemsCrossOffsetList(flexItems.size());
    for (size_t lineIndex = 0; lineIndex < flexLines.ranges.size(); ++lineIndex) {
        auto lineRange = flexLines.ranges[lineIndex];
        LayoutUnit lineCrossAxisExtent = flexLinesCrossSizeList[lineIndex];

        performBaselineAlignment(lineRange, flexItems, flexItemsCrossOffsetList, flexItemsCrossSizeList, lineCrossAxisExtent);

        for (auto flexItemIndex = lineRange.begin(); flexItemIndex < lineRange.end(); ++flexItemIndex) {
            auto& flexLayoutItem = flexItems[flexItemIndex];
            ASSERT(!flexLayoutItem.renderer->isOutOfFlowPositioned());

            auto safety = flexFormattingUtils().overflowAlignmentForFlexItem(flexLayoutItem);
            auto position = flexFormattingUtils().alignmentForFlexItem(flexLayoutItem);
            if (updateAutoMarginsInCrossAxis(flexLayoutItem, flexItemsCrossOffsetList[flexItemIndex], std::max(0_lu, flexFormattingUtils().availableAlignmentSpaceForFlexItem(lineCrossAxisExtent, flexLayoutItem, flexItemsCrossSizeList[flexItemIndex]))) || position == ItemPosition::Baseline || position == ItemPosition::LastBaseline)
                continue;

            LayoutUnit availableSpace = flexFormattingUtils().availableAlignmentSpaceForFlexItem(lineCrossAxisExtent, flexLayoutItem, flexItemsCrossSizeList[flexItemIndex]);
            if (availableSpace < 0 && safety == OverflowAlignment::Safe)
                position = ItemPosition::FlexStart; // See Start == FlexStart assumption in flexFormattingUtils().alignmentForFlexItem().
            LayoutUnit offset = FlexFormattingUtils::alignmentOffset(availableSpace, position, { }, { }, m_constraints.isWrapReverse);
            flexItemsCrossOffsetList[flexItemIndex] += offset;
        }
    }
    // Fold each item's cross-axis alignment offset (align-self / auto-margin / baseline) into its position.
    for (size_t flexItemIndex = 0; flexItemIndex < flexItems.size(); ++flexItemIndex)
        flexItemsPositionList[flexItemIndex].move(0_lu, flexItemsCrossOffsetList[flexItemIndex]);
}

void FlexFormattingContext::performBaselineAlignment(WTF::Range<size_t> lineRange, FlexLayoutItems& flexItems, Vector<LayoutUnit>& flexItemsCrossOffsetList, const SizeList& flexItemsCrossSizeList, LayoutUnit lineCrossAxisExtent)
{
    // 9.6. (#14) Align each baseline-aligned item (align-self: baseline / last baseline) so its baseline sits on
    // its baseline-sharing group's shared baseline within the flex line.
    bool containerHasWrapReverse = m_constraints.isWrapReverse;

    auto flexItemWritingModeForBaselineAlignment = [&](const FlexLayoutItem& flexLayoutItem) {
        CheckedRef flexItem = flexLayoutItem.renderer;
        if (flexFormattingUtils().mainAxisIsFlexItemInlineAxis(flexLayoutItem))
            return flexItem->style().writingMode();

        auto alignmentContextAxis = m_constraints.style->isRowFlexDirection() ? LogicalBoxAxis::Inline : LogicalBoxAxis::Block;
        return BaselineAlignment::usedWritingModeForBaselineAlignment(alignmentContextAxis, m_constraints.style->writingMode(), flexItem->writingMode());
    };

    auto shouldAdjustItemTowardsCrossAxisEnd = [&](const FlowDirection& flexItemBlockFlowDirection, ItemPosition alignment) {
        ASSERT(alignment == ItemPosition::Baseline || alignment == ItemPosition::LastBaseline);

        // The direction in which we are aligning (i.e. direction of the cross axis) must be parallel with the direction of the flex item's used writing mode
        ASSERT_IMPLIES(m_constraints.crossAxisDirection == RenderFlexibleBox::Direction::TopToBottom || m_constraints.crossAxisDirection == RenderFlexibleBox::Direction::BottomToTop, flexItemBlockFlowDirection == RenderFlexibleBox::Direction::TopToBottom || flexItemBlockFlowDirection == RenderFlexibleBox::Direction::BottomToTop);
        ASSERT_IMPLIES(m_constraints.crossAxisDirection == RenderFlexibleBox::Direction::LeftToRight || m_constraints.crossAxisDirection == RenderFlexibleBox::Direction::RightToLeft, flexItemBlockFlowDirection == RenderFlexibleBox::Direction::LeftToRight || flexItemBlockFlowDirection == RenderFlexibleBox::Direction::RightToLeft);

        // For first baseline aligned items, if its block direction is the opposite of
        // the cross axis direction, then that means its fallback alignment (safe self-start)
        // is in the direction of the end of the cross axis
        //
        // For last baseline aligned items, if its block direction is in the same direction as
        // the cross axis direction, then that means its fallback alignment (safe self-end) is
        // in the direction of the end of the cross axis
        if (alignment == ItemPosition::Baseline)
            return m_constraints.crossAxisDirection != flexItemBlockFlowDirection;
        return m_constraints.crossAxisDirection == flexItemBlockFlowDirection;
    };

    // Build the baseline sharing groups for this line: first- and last-baseline items whose cross-axis margins are both non-auto.
    std::optional<BaselineAlignmentState> baselineAlignmentState;
    BaselineSharingGroups baselineSharingGroups;
    for (auto itemIndex = lineRange.begin(); itemIndex < lineRange.end(); ++itemIndex) {
        auto& flexLayoutItem = flexItems[itemIndex];
        CheckedRef flexItem = flexLayoutItem.renderer;
        auto alignment = flexFormattingUtils().alignmentForFlexItem(flexLayoutItem);
        if ((alignment != ItemPosition::Baseline && alignment != ItemPosition::LastBaseline) || flexFormattingUtils().hasAutoMarginsInCrossAxis(flexLayoutItem))
            continue;
        if (!baselineAlignmentState) {
            auto alignmentContextAxis = m_constraints.style->isRowFlexDirection() ? LogicalBoxAxis::Inline : LogicalBoxAxis::Block;
            baselineAlignmentState = BaselineAlignmentState { alignmentContextAxis, m_constraints.style->writingMode() };
        }
        auto baselineSharingGroupIndex = baselineAlignmentState->sharedGroupIndex(flexItem->writingMode(), alignment);
        if (baselineSharingGroupIndex == baselineSharingGroups.size())
            baselineSharingGroups.append({ });
        auto& group = baselineSharingGroups[baselineSharingGroupIndex];
        group.maxAscent = std::max(group.maxAscent, flexFormattingUtils().marginBoxAscentForFlexItem(flexLayoutItem, flexItemsCrossSizeList[itemIndex]));
        group.items.append(itemIndex);
    }

    for (auto& baselineSharingGroup : baselineSharingGroups) {
        LayoutUnit minMarginAfterBaseline = LayoutUnit::max();
        for (auto itemIndex : baselineSharingGroup.items) {
            auto& flexLayoutItem = flexItems[itemIndex];
            auto position = flexFormattingUtils().alignmentForFlexItem(flexLayoutItem);
            ASSERT(position == ItemPosition::Baseline || position == ItemPosition::LastBaseline);
            auto offset = FlexFormattingUtils::alignmentOffset(flexFormattingUtils().availableAlignmentSpaceForFlexItem(lineCrossAxisExtent, flexLayoutItem, flexItemsCrossSizeList[itemIndex]), position, flexFormattingUtils().marginBoxAscentForFlexItem(flexLayoutItem, flexItemsCrossSizeList[itemIndex]), baselineSharingGroup.maxAscent, containerHasWrapReverse);
            flexItemsCrossOffsetList[itemIndex] += offset;

            if (shouldAdjustItemTowardsCrossAxisEnd(flexItemWritingModeForBaselineAlignment(flexLayoutItem).blockDirection(), position))
                minMarginAfterBaseline = std::min(minMarginAfterBaseline, flexFormattingUtils().availableAlignmentSpaceForFlexItem(lineCrossAxisExtent, flexLayoutItem, flexItemsCrossSizeList[itemIndex]) - offset);
        }
        // css-align-3 9.3 part 3:
        // Position the aligned baseline-sharing group within the alignment container according to its
        // fallback alignment. The fallback alignment of a baseline-sharing group is the fallback alignment
        // of its items as resolved to physical directions.
        if (minMarginAfterBaseline) {
            for (auto itemIndex : baselineSharingGroup.items) {
                auto& flexLayoutItem = flexItems[itemIndex];
                if (shouldAdjustItemTowardsCrossAxisEnd(flexItemWritingModeForBaselineAlignment(flexLayoutItem).blockDirection(), flexFormattingUtils().alignmentForFlexItem(flexLayoutItem)) && !flexFormattingUtils().hasAutoMarginsInCrossAxis(flexLayoutItem))
                    flexItemsCrossOffsetList[itemIndex] += minMarginAfterBaseline;
            }
        }
    }
}

void FlexFormattingContext::computeFlexItemRects(const FlexLines& flexLines, FlexLayoutItems& flexItems, const PositionList& flexItemsPositionList, const LinesCrossPositionList& flexLinesCrossPositionList, const LinesCrossSizeList& flexLinesCrossSizeList, const SizeList& flexItemsCrossSizeList, LayoutUnit crossAxisStartEdge, LayoutUnit crossContentExtent, LayoutUnit crossExtent)
{
    // 9.6. Place each flex item at its final flow-aware location, applying the wrap-reverse and rtl-column
    // cross-axis flips, and write it to the renderer.
    bool isRightToLeftColumn = !m_constraints.style->writingMode().isLogicalLeftInlineStart() && m_constraints.isColumnFlow;
    for (size_t lineIndex = 0; lineIndex < flexLines.ranges.size(); ++lineIndex) {
        auto lineRange = flexLines.ranges[lineIndex];
        for (auto flexItemIndex = lineRange.begin(); flexItemIndex < lineRange.end(); ++flexItemIndex) {
            auto location = flexItemsPositionList[flexItemIndex];
            if (m_constraints.isWrapReverse) {
                auto originalOffset = flexLinesCrossPositionList[lineIndex] - crossAxisStartEdge;
                location.move(0_lu, (crossContentExtent - originalOffset - flexLinesCrossSizeList[lineIndex]) - originalOffset);
            }
            if (isRightToLeftColumn) {
                // For vertical flows, setFlexItemGeometry will transpose x and
                // y, so using the y axis for a column cross axis extent is correct.
                location.setY(crossExtent - flexItemsCrossSizeList[flexItemIndex] - location.y());
                if (!m_constraints.style->writingMode().isHorizontal())
                    location.move(LayoutSize(0, -m_constraints.crossAxisScrollbarExtent));
            }
            integrationUtils().setFlexItemGeometry(flexItems[flexItemIndex], location, m_constraints.isHorizontalFlow);
        }
    }
}

LayoutUnit FlexFormattingContext::placeFlexItems(LayoutUnit crossAxisOffset, std::span<FlexLayoutItem> flexLayoutItems, std::span<LayoutPoint> positions, LayoutUnit availableFreeSpace)
{
    // 9.5. (#12) Position the items along the main axis: first give any positive free space to main-axis auto
    // margins, then place each item per justify-content (the initial offset plus the spacing between items).
    LayoutUnit autoMarginOffset = autoMarginOffsetInMainAxis(flexLayoutItems, availableFreeSpace);
    LayoutUnit mainAxisOffset = m_constraints.flowAwareBorderInline.first + m_constraints.flowAwarePaddingInline.first;
    mainAxisOffset += FlexFormattingUtils::initialJustifyContentOffset(m_constraints.style, availableFreeSpace, flexLayoutItems.size(), m_constraints.isColumnOrRowReverse);
    if (m_constraints.style->flexDirection() == FlexDirection::RowReverse)
        mainAxisOffset += m_constraints.mainAxisScrollbarExtent;

    if (availableFreeSpace < 0) {
        auto resolvedJustifyContent = m_constraints.style->justifyContent().resolve(FlexFormattingUtils::contentAlignmentNormalBehavior());
        auto distribution = resolvedJustifyContent.distribution();
        auto safety = resolvedJustifyContent.overflow();
        auto position = FlexFormattingUtils::resolveLeftRightAlignment(resolvedJustifyContent.position(), resolvedJustifyContent, m_constraints.style, m_constraints.isColumnOrRowReverse);
        LayoutUnit overflow = FlexFormattingUtils::contentAlignmentStartOverflow(availableFreeSpace, position, distribution, safety, m_constraints.isColumnOrRowReverse);
        m_result.justifyContentStartOverflow = std::max(m_result.justifyContentStartOverflow, overflow);
    }

    LayoutUnit totalMainExtent = m_constraints.mainAxisBorderBoxExtent;

    auto resolvedJustifyContent = m_constraints.style->justifyContent().resolve(FlexFormattingUtils::contentAlignmentNormalBehavior());
    auto distribution = resolvedJustifyContent.distribution();
    bool shouldFlipMainAxis = !m_constraints.isColumnFlow && !m_constraints.isLeftToRightFlow;
    auto gapBetweenItems = flexFormattingUtils().computeGap(FlexFormattingUtils::GapType::BetweenItems);
    for (size_t i = 0; i < flexLayoutItems.size(); ++i) {
        auto& flexLayoutItem = flexLayoutItems[i];
        CheckedRef flexItem = flexLayoutItem.renderer;

        ASSERT(!flexItem->isOutOfFlowPositioned());

        updateAutoMarginsInMainAxis(flexItem, autoMarginOffset);

        mainAxisOffset += flexFormattingUtils().flowAwareMarginStartForFlexItem(flexLayoutItem);

        LayoutUnit flexItemMainExtent = flexFormattingUtils().mainAxisExtentForFlexItem(flexLayoutItem);
        // In an RTL column situation, this will apply the margin-right/margin-end
        // on the left. This will be fixed later by the rtl-column flip in computeFlexItemRects.
        auto leadingScrollbarSize = m_constraints.style->writingMode().isInlineFlipped() && m_constraints.style->writingMode().isVertical() ? m_constraints.mainAxisScrollbarExtent : LayoutUnit();
        LayoutPoint location(shouldFlipMainAxis ? totalMainExtent - mainAxisOffset - flexItemMainExtent - leadingScrollbarSize : mainAxisOffset, crossAxisOffset + flexFormattingUtils().flowAwareMarginBeforeForFlexItem(flexLayoutItem));
        positions[i] = location;
        integrationUtils().setFlexItemGeometry(flexLayoutItems[i], positions[i], m_constraints.isHorizontalFlow);
        mainAxisOffset += flexItemMainExtent + flexFormattingUtils().flowAwareMarginEndForFlexItem(flexLayoutItem);

        if (i != flexLayoutItems.size() - 1) {
            // The last item does not get extra space added.
            mainAxisOffset += FlexFormattingUtils::justifyContentSpaceBetweenFlexItems(availableFreeSpace, distribution, flexLayoutItems.size()) + gapBetweenItems;
        }

        // FIXME: Deal with pagination.
    }

    // For column flow, return the line's main-axis content extent (its main content size plus the cross-axis
    // scrollbar); handleMainAxisAlignment folds the largest line's extent into the container's logical height at the
    // 9.6 finalize instead of growing the container here. Row flow builds its block extent elsewhere, so return 0.
    if (!m_constraints.isColumnFlow)
        return { };

    return mainAxisOffset - (m_constraints.flowAwareBorderInline.first + m_constraints.flowAwarePaddingInline.first) + m_constraints.mainAxisScrollbarExtent;
}

void FlexFormattingContext::reverseColumnLinesFromContainerMainEndIfNeeded(const FlexLines& flexLines, FlexLayoutItems& flexItems, const SizeList& flexItemsMainSizeList, PositionList& flexItemsPositionList, const LinesCrossPositionList& flexLinesCrossPositionList, LayoutUnit containerMainBlockContentExtent, LayoutUnit containerMainBorderBoxExtent)
{
    // The container's main size is settled by now (9.2 determines it before 9.5 places the items), so every line
    // is reversed against the one finalized height rather than growing and reading it back per line.
    if (m_constraints.style->flexDirection() != FlexDirection::ColumnReverse)
        return;

    for (size_t lineIndex = 0; lineIndex < flexLines.ranges.size(); ++lineIndex) {
        auto lineRange = flexLines.ranges[lineIndex];
        auto lineItems = flexItems.mutableSpan().subspan(lineRange.begin(), lineRange.distance());
        auto linePositions = flexItemsPositionList.mutableSpan().subspan(lineRange.begin(), lineRange.distance());
        auto remainingFreeSpace = mainAxisAvailableSpaceForItemAlignment(containerMainBlockContentExtent, lineItems.size());
        for (size_t index = 0; index < lineItems.size(); ++index)
            remainingFreeSpace -= lineItems[index].flexedMarginBoxSize(flexItemsMainSizeList[lineRange.begin() + index]);
        layoutColumnReverse(lineItems, linePositions, flexLinesCrossPositionList[lineIndex], remainingFreeSpace, containerMainBorderBoxExtent);
    }
}

void FlexFormattingContext::layoutColumnReverse(std::span<FlexLayoutItem> flexLayoutItems, std::span<LayoutPoint> positions, LayoutUnit crossAxisOffset, LayoutUnit availableFreeSpace, LayoutUnit columnMainBorderBoxExtent)
{
    // This is similar to the logic in placeFlexItems, except we place
    // the children starting from the end of the flexbox. We also don't need to
    // layout anything since we're just moving the children to a new position.
    // The flex container's main size (its logical height) has been finalized by now, so it is passed in
    // rather than read back off the container (css-flexbox-1 9.2 determines the container main size before
    // 9.5 main-axis alignment places the items).
    LayoutUnit mainAxisOffset = columnMainBorderBoxExtent - m_constraints.flowAwareBorderInline.second - m_constraints.flowAwarePaddingInline.second;
    mainAxisOffset -= FlexFormattingUtils::initialJustifyContentOffset(m_constraints.style, availableFreeSpace, flexLayoutItems.size(), m_constraints.isColumnOrRowReverse);
    mainAxisOffset -= m_constraints.mainAxisScrollbarExtent;

    auto distribution = m_constraints.style->justifyContent().resolve(FlexFormattingUtils::contentAlignmentNormalBehavior()).distribution();
    auto gapBetweenItems = flexFormattingUtils().computeGap(FlexFormattingUtils::GapType::BetweenItems);

    for (size_t i = 0; i < flexLayoutItems.size(); ++i) {
        auto& flexLayoutItem = flexLayoutItems[i];
        ASSERT(!flexLayoutItem.renderer->isOutOfFlowPositioned());
        mainAxisOffset -= flexFormattingUtils().mainAxisExtentForFlexItem(flexLayoutItem) + flexFormattingUtils().flowAwareMarginEndForFlexItem(flexLayoutItem);
        positions[i] = LayoutPoint(mainAxisOffset, crossAxisOffset + flexFormattingUtils().flowAwareMarginBeforeForFlexItem(flexLayoutItem));
        integrationUtils().setFlexItemGeometry(flexLayoutItems[i], positions[i], m_constraints.isHorizontalFlow);
        mainAxisOffset -= flexFormattingUtils().flowAwareMarginStartForFlexItem(flexLayoutItem);

        if (i != flexLayoutItems.size() - 1) {
            // The last item does not get extra space added.
            mainAxisOffset -= FlexFormattingUtils::justifyContentSpaceBetweenFlexItems(availableFreeSpace, distribution, flexLayoutItems.size()) + gapBetweenItems;
        }
    }
}

void FlexFormattingContext::setFlexItemCountsForFirstAndLastLine(const FlexLines& flexLines)
{
    if (flexLines.ranges.isEmpty())
        return;

    auto isWrapReverse = m_constraints.isWrapReverse;
    auto firstLineItemsCountInOriginalOrder = flexLines.ranges.first().distance();
    auto lastLineItemsCountInOriginalOrder = flexLines.ranges.last().distance();

    m_result.numberOfFlexItemsOnFirstLine = !isWrapReverse ? firstLineItemsCountInOriginalOrder : lastLineItemsCountInOriginalOrder;
    m_result.numberOfFlexItemsOnLastLine = !isWrapReverse ? lastLineItemsCountInOriginalOrder : firstLineItemsCountInOriginalOrder;
}

LayoutUnit FlexFormattingContext::flexBaseSizeForFlexItem(const FlexLayoutItem& flexLayoutItem)
{
    auto flexBasis = flexFormattingUtils().flexBasisForFlexItem(flexLayoutItem);
    auto scoped = LayoutIntegration::ScopedFlexBasisAsFlexItemMainSize { flexLayoutItem, flexBasis.tryPreferredSize().value_or(Style::PreferredSize { CSS::Keyword::MaxContent { } }) };
    // FIXME: While we are supposed to ignore min/max here, the cached
    // The cached block-axis size entry may hold a min/max-constrained size.
    auto blockAxisContentSize = ensureBlockAxisContentSizeForFlexItemIfNeeded(flexLayoutItem);

    // A. If the item has a definite used flex basis, that's the flex base size.
    if (integrationUtils().flexItemMainSizeIsDefinite(flexLayoutItem, flexBasis))
        return std::max(0_lu, integrationUtils().computeMainAxisExtentForFlexItem(flexLayoutItem, flexBasis, m_constraints.mainAxisSizeForLengthResolution).value());

    // B. If the flex item has a preferred aspect ratio, a used flex basis of content, and a definite cross size,
    // the flex base size is calculated from its used cross size and the flex item's aspect ratio.
    if (flexItemHasComputableAspectRatioAndCrossSizeIsConsideredDefinite(flexLayoutItem)) {
        auto& crossSizeLength = flexFormattingUtils().preferredCrossSizeLengthForFlexItem(flexLayoutItem);
        return adjustFlexItemSizeForAspectRatioCrossAxisMinAndMax(flexLayoutItem, computeMainSizeFromAspectRatioUsing(flexLayoutItem, crossSizeLength));
    }

    // FIXME: C. If the used flex basis is content or depends on its available space, and the flex container is being
    // sized under a min-content or max-content constraint, size the item under that constraint.
    // FIXME: D. If the used flex basis is content or depends on its available space, the available main size is
    // infinite, and the flex item's inline axis is parallel to the main axis, lay the item out using the rules for a
    // box in an orthogonal flow. The flex base size is the item's max-content main size.

    // E. Otherwise, size the item into the available space using its used flex basis in place of its main size,
    // treating a value of content as max-content. The flex base size is the item's resulting main size.
    if (!flexLayoutItem.mainAxisIsInlineAxis) {
        ASSERT(!flexLayoutItem.renderer->needsLayout());
        ASSERT(blockAxisContentSize);
        return blockAxisContentSize.value_or(0_lu);
    }

    CheckedRef flexItem = flexLayoutItem.renderer;
    auto mainAxisExtent = integrationUtils().maxContentMainAxisContributionForFlexItem(flexLayoutItem);
    auto mainAxisBorderAndPadding = m_constraints.isHorizontalFlow ? flexItem->horizontalBorderAndPaddingExtent() : flexItem->verticalBorderAndPaddingExtent();
    return mainAxisExtent - mainAxisBorderAndPadding;
}

std::optional<LayoutUnit> FlexFormattingContext::ensureBlockAxisContentSizeForFlexItemIfNeeded(const FlexLayoutItem& flexLayoutItem)
{
    // Laying the item out, reusing the previously cached size, and caching the new one are all render-tree work; ask RenderFlexibleBox.
    auto flexBaseSizeNeedsBlockAxisContentSize = [&] {
        if (flexLayoutItem.mainAxisIsInlineAxis)
            return false;

        auto flexBasis = flexFormattingUtils().flexBasisForFlexItem(flexLayoutItem);
        auto minSize = flexFormattingUtils().minMainSizeLengthForFlexItem(flexLayoutItem);
        auto maxSize = flexFormattingUtils().maxMainSizeLengthForFlexItem(flexLayoutItem);
        // FIXME: we must run flexItemMainSizeIsDefinite() because it might end up calling computePercentageLogicalHeight()
        // which has some side effects like calling addPercentHeightDescendant() for example so it is not possible to skip
        // the call for example by moving it to the end of the conditional expression. This is error-prone and we should
        // refactor computePercentageLogicalHeight() at some point so that it only computes stuff without those side effects.
        if (!integrationUtils().flexItemMainSizeIsDefinite(flexLayoutItem, flexBasis) || minSize.isIntrinsic() || maxSize.isIntrinsic())
            return true;

        if (flexFormattingUtils().useContentBasedMinimumSize(flexLayoutItem))
            return true;

        return false;
    };

    if (flexBaseSizeNeedsBlockAxisContentSize())
        return integrationUtils().computeBlockAxisContentSizeForFlexItem(flexLayoutItem);
    return { };
}

std::pair<LayoutUnit, LayoutUnit> FlexFormattingContext::minMaxMainSizesForFlexItem(const FlexLayoutItem& flexLayoutItem)
{
    auto maxExtent = computeUsedMaxMainSize(flexLayoutItem);
    auto resolvedMax = maxExtent.value_or(LayoutUnit::max());

    // useContentBasedMinimumSize covers both auto-equivalent cases: min:auto with
    // non-scrollable overflow (§ 4.5) and block-axis intrinsic keywords (CSS Sizing
    // 3 § 5.2 makes those behave like auto, regardless of overflow).
    if (flexFormattingUtils().useContentBasedMinimumSize(flexLayoutItem))
        return { computeContentBasedMinMainSize(flexLayoutItem, maxExtent), resolvedMax };

    auto min = flexFormattingUtils().minMainSizeLengthForFlexItem(flexLayoutItem);
    if (!min.isAuto())
        return { computeUsedNonAutoMinMainSize(flexLayoutItem, min), resolvedMax };

    // min:auto on a scroll container — spec says the automatic minimum size is zero.
    return { 0_lu, resolvedMax };
}

std::optional<LayoutUnit> FlexFormattingContext::computeUsedMaxMainSize(const FlexLayoutItem& flexLayoutItem)
{
    auto max = flexFormattingUtils().maxMainSizeLengthForFlexItem(flexLayoutItem);
    if (max.isSpecified())
        return integrationUtils().computeMainAxisExtentForFlexItem(flexLayoutItem, max, m_constraints.mainAxisSizeForLengthResolution);
    if (max.isIntrinsicOrStretch())
        return integrationUtils().computeMainAxisExtentForFlexItemWithCrossAxisOverride(flexLayoutItem, max, m_constraints.mainAxisSizeForLengthResolution);
    return { };
}

LayoutUnit FlexFormattingContext::computeUsedNonAutoMinMainSize(const FlexLayoutItem& flexLayoutItem, const Style::MinimumSize& min)
{
    // https://drafts.csswg.org/css-flexbox/#main-size-property
    // Resolves the used min main size for every case except min:auto. Three values
    // route here: a specified length/percentage, the stretch keyword, and intrinsic
    // keywords (min-content/max-content/fit-content) on the inline axis. Per CSS
    // Sizing 3 § 5.2, intrinsic keywords on the block axis behave like auto, so
    // those go through computeContentBasedMinMainSize instead.
    auto minExtent = [&] {
        if (min.isIntrinsicOrStretch())
            return integrationUtils().computeMainAxisExtentForFlexItemWithCrossAxisOverride(flexLayoutItem, min, m_constraints.mainAxisSizeForLengthResolution).value_or(0_lu);
        return integrationUtils().computeMainAxisExtentForFlexItem(flexLayoutItem, min, m_constraints.mainAxisSizeForLengthResolution).value_or(0_lu);
    }();

    // We must never return a min size smaller than the min preferred size for tables.
    if (flexLayoutItem.renderer->isRenderTable() && flexLayoutItem.mainAxisIsInlineAxis)
        minExtent = std::max(minExtent, integrationUtils().minContentMainAxisContributionForFlexItem(flexLayoutItem));
    return minExtent;
}

LayoutUnit FlexFormattingContext::computeContentBasedMinMainSize(const FlexLayoutItem& flexLayoutItem, std::optional<LayoutUnit> maxExtent)
{
    CheckedRef flexItem = flexLayoutItem.renderer;
    // The content-based minimum size (https://drafts.csswg.org/css-flexbox/#min-size-auto): for a non-replaced item
    // the larger, and for a replaced item the smaller, of the content size suggestion and the transferred size
    // suggestion, capped by the specified size suggestion, and clamped by the definite maximum main size.
    // FIXME: If the min value is expected to be valid here, we need to come up with a non optional version of computeMainAxisExtentForFlexItem and
    // ensure it's valid through the virtual calls of computeSizingKeywordLogicalContentHeightUsing.
    LayoutUnit contentSize;
    auto& flexItemCrossSizeLength = flexFormattingUtils().preferredCrossSizeLengthForFlexItem(flexLayoutItem);

    // Content size suggestion: the min-content size in the main axis, clamped (when the item has a preferred
    // aspect ratio) through the aspect ratio.
    bool canComputeSizeThroughAspectRatio = flexFormattingUtils().flexItemHasComputableAspectRatio(flexLayoutItem) && flexItemCrossSizeIsDefinite(flexLayoutItem, flexItemCrossSizeLength);
    if (canComputeSizeThroughAspectRatio)
        contentSize = computeMainSizeFromAspectRatioUsing(flexLayoutItem, flexItemCrossSizeLength);

    if (!canComputeSizeThroughAspectRatio || !flexItem->isRenderReplaced()) {
        auto minContentSize = integrationUtils().computeMainAxisExtentForFlexItemWithCrossAxisOverride(flexLayoutItem, Style::MinimumSize { CSS::Keyword::MinContent { } }, m_constraints.mainAxisSizeForLengthResolution).value_or(0_lu);
        contentSize = std::max(contentSize, minContentSize);
    }

    if (FlexFormattingUtils::flexItemHasAspectRatio(flexLayoutItem))
        contentSize = adjustFlexItemSizeForAspectRatioCrossAxisMinAndMax(flexLayoutItem, contentSize);

    contentSize = std::max(0_lu, contentSize);
    ASSERT(contentSize >= 0);
    contentSize = std::min(contentSize, maxExtent.value_or(contentSize));

    // Specified size suggestion: if the item's preferred main size is definite, cap the result by that size.
    auto mainSize = flexFormattingUtils().preferredMainSizeLengthForFlexItem(flexLayoutItem);
    if (integrationUtils().flexItemMainSizeIsDefinite(flexLayoutItem, mainSize)) {
        auto resolvedMainSize = integrationUtils().computeMainAxisExtentForFlexItem(flexLayoutItem, mainSize, m_constraints.mainAxisSizeForLengthResolution).value_or(0);
        ASSERT(resolvedMainSize >= 0);
        auto specifiedSize = std::min(resolvedMainSize, maxExtent.value_or(resolvedMainSize));
        return std::min(specifiedSize, contentSize);
    }

    // Transferred size suggestion: a replaced item's definite cross size converted through its aspect ratio.
    if (flexItem->isRenderReplaced() && flexItemHasComputableAspectRatioAndCrossSizeIsConsideredDefinite(flexLayoutItem)) {
        auto transferredSize = computeMainSizeFromAspectRatioUsing(flexLayoutItem, flexItemCrossSizeLength);
        transferredSize = adjustFlexItemSizeForAspectRatioCrossAxisMinAndMax(flexLayoutItem, transferredSize);
        return std::min(transferredSize, contentSize);
    }

    return contentSize;
}

// FIXME: computeMainSizeFromAspectRatioUsing may need to return an std::optional<LayoutUnit> in the future
// rather than returning indefinite sizes as 0/-1.
template<typename SizeType>
LayoutUnit FlexFormattingContext::computeMainSizeFromAspectRatioUsing(const FlexLayoutItem& flexLayoutItem, const SizeType& crossSizeLength) const
{
    CheckedRef flexItem = flexLayoutItem.renderer;
    CheckedRef style = flexLayoutItem.style();
    ASSERT(FlexFormattingUtils::flexItemHasAspectRatio(flexLayoutItem));
    auto flexItemCrossAxisBorderAndPadding = flexLayoutItem.crossAxisBorderAndPadding;

    // All paths return border-box cross size.
    auto crossSizeOptional = WTF::switchOn(crossSizeLength,
        [&](const SizeType::Fixed& fixedCrossSizeLength) -> std::optional<LayoutUnit> {
            auto value = LayoutUnit { fixedCrossSizeLength.resolveZoom(style->usedZoomForLength()) };
            if (style->boxSizing() == BoxSizing::ContentBox)
                value += flexItemCrossAxisBorderAndPadding;
            return value;
        },
        [&](const SizeType::Percentage& percentageCrossSizeLength) -> std::optional<LayoutUnit> {
            return flexLayoutItem.mainAxisIsInlineAxis
                ? flexItem->computePercentageLogicalHeight(SizeType { percentageCrossSizeLength })
                : integrationUtils().adjustBorderBoxLogicalWidthForBoxSizing(Style::evaluate<LayoutUnit>(percentageCrossSizeLength, m_constraints.crossAxisSizeForLengthResolution));
        },
        [&](const SizeType::Calc& calcCrossSizeLength) -> std::optional<LayoutUnit> {
            return flexLayoutItem.mainAxisIsInlineAxis
                ? flexItem->computePercentageLogicalHeight(calcCrossSizeLength)
                : integrationUtils().adjustBorderBoxLogicalWidthForBoxSizing(Style::evaluate<LayoutUnit>(calcCrossSizeLength, m_constraints.crossAxisSizeForLengthResolution, style->usedZoomForLength()));
        },
        [&](const CSS::Keyword::Auto&) -> std::optional<LayoutUnit> {
            ASSERT(flexFormattingUtils().hasDefiniteCrossSizeForFlexItem(flexLayoutItem));
            return flexFormattingUtils().innerCrossSizeForFlexItem(flexLayoutItem);
        },
        [&](const CSS::Keyword::Stretch&) -> std::optional<LayoutUnit> {
            // Resolve stretch against the flex container's cross-axis definite size.
            return flexFormattingUtils().innerCrossSizeForFlexItem(flexLayoutItem);
        },
        [&](const CSS::Keyword::WebkitFillAvailable&) -> std::optional<LayoutUnit> {
            return flexFormattingUtils().innerCrossSizeForFlexItem(flexLayoutItem);
        },
        [&](const auto&) -> std::optional<LayoutUnit> {
            ASSERT_NOT_REACHED();
            return { };
        }
    );
    if (!crossSizeOptional)
        return 0_lu;

    auto crossSize = *crossSizeOptional;
    auto preferredAspectRatio = flexFormattingUtils().preferredAspectRatioForFlexItem(flexLayoutItem);

    auto useCSSAspectRatio = style->aspectRatio().isRatio() || (style->aspectRatio().isAutoAndRatio() && flexItem->intrinsicSize().isEmpty());
    if (!useCSSAspectRatio) {
        // Intrinsic aspect ratio (e.g. from <img>). The sizing calculations that floor
        // the content box size at zero when applying box-sizing are also ignored.
        // https://drafts.csswg.org/css-flexbox/#algo-main-item.
        crossSize -= flexItemCrossAxisBorderAndPadding;
        return std::max(0_lu, LayoutUnit { crossSize * preferredAspectRatio });
    }

    auto boxSizingForAspectRatio = style->boxSizingForAspectRatio();
    if (boxSizingForAspectRatio == BoxSizing::ContentBox) {
        // Ratio applies to content dimensions. Convert border-box cross size to content-box.
        crossSize -= flexItemCrossAxisBorderAndPadding;
        return std::max(0_lu, LayoutUnit { crossSize * preferredAspectRatio });
    }

    // Ratio applies to border-box dimensions. Compute border-box main size,
    // then subtract main-axis border+padding to return content-box.
    ASSERT(style->boxSizing() == BoxSizing::BorderBox);
    auto flexItemMainAxisBorderAndPadding = m_constraints.isHorizontalFlow ? flexItem->horizontalBorderAndPaddingExtent() : flexItem->verticalBorderAndPaddingExtent();
    return std::max(0_lu, LayoutUnit { crossSize * preferredAspectRatio } - flexItemMainAxisBorderAndPadding);
}

LayoutUnit FlexFormattingContext::adjustFlexItemSizeForAspectRatioCrossAxisMinAndMax(const FlexLayoutItem& flexLayoutItem, LayoutUnit flexItemSize)
{
    auto& crossMin = flexFormattingUtils().minCrossSizeLengthForFlexItem(flexLayoutItem);
    auto& crossMax = flexFormattingUtils().maxCrossSizeLengthForFlexItem(flexLayoutItem);

    if (flexItemCrossSizeIsDefinite(flexLayoutItem, crossMax)) {
        LayoutUnit maxValue = computeMainSizeFromAspectRatioUsing(flexLayoutItem, crossMax);
        flexItemSize = std::min(maxValue, flexItemSize);
    }

    if (flexItemCrossSizeIsDefinite(flexLayoutItem, crossMin)) {
        LayoutUnit minValue = computeMainSizeFromAspectRatioUsing(flexLayoutItem, crossMin);
        flexItemSize = std::max(minValue, flexItemSize);
    }

    return flexItemSize;
}

LayoutUnit FlexFormattingContext::flexItemIntrinsicLogicalHeight(const FlexLayoutItem& flexLayoutItem) const
{
    CheckedRef flexItem = flexLayoutItem.renderer;
    // This should only be called if the logical height is the cross size
    ASSERT(flexLayoutItem.mainAxisIsInlineAxis);
    if (flexFormattingUtils().needToStretchFlexItemLogicalHeight(flexLayoutItem)) {
        LayoutUnit flexItemContentHeight = integrationUtils().flexItemContentLogicalHeight(flexLayoutItem);
        LayoutUnit flexItemLogicalHeight = flexItemContentHeight + flexItem->scrollbarLogicalHeight() + flexItem->borderAndPaddingLogicalHeight();
        return flexItem->constrainLogicalHeightByMinMax(flexItemLogicalHeight, flexItemContentHeight);
    }
    return flexItem->logicalHeight();
}

LayoutUnit FlexFormattingContext::flexItemIntrinsicLogicalWidth(const FlexLayoutItem& flexLayoutItem)
{
    CheckedRef flexItem = flexLayoutItem.renderer;
    // This should only be called if the logical width is the cross size
    ASSERT(!flexLayoutItem.mainAxisIsInlineAxis);
    if (flexItemCrossSizeIsDefinite(flexLayoutItem, flexLayoutItem.style().logicalWidth()))
        return flexItem->logicalWidth();

    RenderBox::LogicalExtentComputedValues values;
    {
        auto cleanOverridingWidthScope = LayoutIntegration::OverridingSizesScope { flexItem, LayoutIntegration::OverridingSizesScope::Axis::Inline };
        flexItem->computeLogicalWidth(values);
    }
    return values.extent;
}

template<typename SizeType> bool FlexFormattingContext::flexItemCrossSizeIsDefinite(const FlexLayoutItem& flexLayoutItem, const SizeType& size)
{
    CheckedRef flexItem = flexLayoutItem.renderer;
    if constexpr (!std::same_as<SizeType, Style::MaximumSize>) {
        if (size.isAuto())
            return false;
    }

    // Stretch is definite in the same cases as percentages, i.e. when the
    // container's cross size is definite. We use a dummy percentage for stretch
    // since computePercentageLogicalHeight evaluates the value as a percentage.
    auto crossSizeIsDefinite = [&](const auto& sizeForPercentageComputation) {
        if (!flexLayoutItem.mainAxisIsInlineAxis || layoutState().isFlexBoxBlockSizeDefinite())
            return true;
        if (layoutState().isFlexBoxBlockSizeIndefinite())
            return false;
        bool definite = bool(flexItem->computePercentageLogicalHeight(sizeForPercentageComputation));
        layoutState().setFlexBoxBlockSizeIsDefinite(definite);
        return definite;
    };

    if (size.isPercentOrCalculated())
        return crossSizeIsDefinite(size);

    if (size.isStretch())
        return crossSizeIsDefinite(Style::PreferredSize { 0_css_percentage });

    // FIXME: Support other intrinsic sizes (min-content, max-content, fit-content) here.
    // Requires updating computeMainSizeFromAspectRatioUsing.
    return size.isFixed();
}

bool FlexFormattingContext::flexItemHasComputableAspectRatioAndCrossSizeIsConsideredDefinite(const FlexLayoutItem& flexLayoutItem)
{
    return flexFormattingUtils().flexItemHasComputableAspectRatio(flexLayoutItem)
        && (flexItemCrossSizeIsDefinite(flexLayoutItem, flexFormattingUtils().preferredCrossSizeLengthForFlexItem(flexLayoutItem)) || flexFormattingUtils().hasDefiniteCrossSizeForFlexItem(flexLayoutItem));
}

void FlexFormattingContext::trimMainAxisMarginStart(FlexLayoutItem& flexLayoutItem)
{
    CheckedRef renderer = flexLayoutItem.renderer;
    auto horizontalFlow = m_constraints.isHorizontalFlow;
    flexLayoutItem.mainAxisMargin -= horizontalFlow
        ? renderer->marginStart(m_constraints.style->writingMode())
        : renderer->marginBefore(m_constraints.style->writingMode());
    if (horizontalFlow)
        integrationUtils().setTrimmedMarginForChild(flexLayoutItem, Style::MarginTrimSide::InlineStart);
    else
        integrationUtils().setTrimmedMarginForChild(flexLayoutItem, Style::MarginTrimSide::BlockStart);
    integrationUtils().addItemAtFlexLineStart(flexLayoutItem);
}

void FlexFormattingContext::trimMainAxisMarginEnd(FlexLayoutItem& flexLayoutItem)
{
    CheckedRef renderer = flexLayoutItem.renderer;
    auto horizontalFlow = m_constraints.isHorizontalFlow;
    flexLayoutItem.mainAxisMargin -= horizontalFlow
        ? renderer->marginEnd(m_constraints.style->writingMode())
        : renderer->marginAfter(m_constraints.style->writingMode());
    if (horizontalFlow)
        integrationUtils().setTrimmedMarginForChild(flexLayoutItem, Style::MarginTrimSide::InlineEnd);
    else
        integrationUtils().setTrimmedMarginForChild(flexLayoutItem, Style::MarginTrimSide::BlockEnd);
    integrationUtils().addItemAtFlexLineEnd(flexLayoutItem);
}

void FlexFormattingContext::trimCrossAxisMarginStart(const FlexLayoutItem& flexLayoutItem)
{
    if (m_constraints.isHorizontalFlow)
        integrationUtils().setTrimmedMarginForChild(flexLayoutItem, Style::MarginTrimSide::BlockStart);
    else
        integrationUtils().setTrimmedMarginForChild(flexLayoutItem, Style::MarginTrimSide::InlineStart);
    integrationUtils().addItemOnFirstFlexLine(flexLayoutItem);
}

void FlexFormattingContext::trimCrossAxisMarginEnd(const FlexLayoutItem& flexLayoutItem)
{
    if (m_constraints.isHorizontalFlow)
        integrationUtils().setTrimmedMarginForChild(flexLayoutItem, Style::MarginTrimSide::BlockEnd);
    else
        integrationUtils().setTrimmedMarginForChild(flexLayoutItem, Style::MarginTrimSide::InlineEnd);
    integrationUtils().addItemOnLastFlexLine(flexLayoutItem);
}

bool FlexFormattingContext::canFitItemWithTrimmedMarginEnd(const FlexLayoutItem& flexLayoutItem, LayoutUnit hypotheticalMainContentSize, LayoutUnit sumHypotheticalMainSize, LayoutUnit mainAxisAvailableSpace) const
{
    auto marginTrim = m_constraints.style->marginTrim();
    if ((m_constraints.isHorizontalFlow && marginTrim.contains(Style::MarginTrimSide::InlineEnd)) || (m_constraints.isColumnFlow && marginTrim.contains(Style::MarginTrimSide::BlockEnd)))
        return sumHypotheticalMainSize + flexLayoutItem.hypotheticalMainAxisMarginBoxSize(hypotheticalMainContentSize) - flexFormattingUtils().flowAwareMarginEndForFlexItem(flexLayoutItem) <= mainAxisAvailableSpace;
    return false;
}

void FlexFormattingContext::removeMarginEndFromFlexSizes(FlexLayoutItem& flexLayoutItem, LayoutUnit& sumFlexBaseSize, LayoutUnit& sumHypotheticalMainSize) const
{
    LayoutUnit margin;
    CheckedRef renderer = flexLayoutItem.renderer;
    if (m_constraints.isHorizontalFlow)
        margin = renderer->marginEnd(m_constraints.style->writingMode());
    else
        margin = renderer->marginAfter(m_constraints.style->writingMode());
    sumFlexBaseSize -= margin;
    sumHypotheticalMainSize -= margin;
}

LayoutUnit FlexFormattingContext::autoMarginOffsetInMainAxis(std::span<const FlexLayoutItem> flexLayoutItems, LayoutUnit& availableFreeSpace)
{
    // 9.5. (#12) If the remaining free space is positive and at least one main-axis margin on the line is auto,
    // distribute the free space equally among those auto margins; otherwise they resolve to zero.
    if (availableFreeSpace <= 0_lu)
        return 0_lu;

    int numberOfAutoMargins = 0;
    bool isHorizontal = m_constraints.isHorizontalFlow;
    for (auto& flexLayoutItem : flexLayoutItems) {
        auto& flexItemStyle = flexLayoutItem.style();
        ASSERT(!flexLayoutItem.renderer->isOutOfFlowPositioned());
        if (isHorizontal) {
            if (flexItemStyle.marginLeft().isAuto())
                ++numberOfAutoMargins;
            if (flexItemStyle.marginRight().isAuto())
                ++numberOfAutoMargins;
        } else {
            if (flexItemStyle.marginTop().isAuto())
                ++numberOfAutoMargins;
            if (flexItemStyle.marginBottom().isAuto())
                ++numberOfAutoMargins;
        }
    }
    if (!numberOfAutoMargins)
        return 0_lu;

    LayoutUnit sizeOfAutoMargin = availableFreeSpace / numberOfAutoMargins;
    availableFreeSpace = 0_lu;
    return sizeOfAutoMargin;
}

void FlexFormattingContext::updateAutoMarginsInMainAxis(RenderBox& flexItem, LayoutUnit autoMarginOffset)
{
    ASSERT(autoMarginOffset >= 0_lu);

    if (m_constraints.isHorizontalFlow) {
        if (flexItem.style().marginLeft().isAuto())
            flexItem.setMarginLeft(autoMarginOffset);
        if (flexItem.style().marginRight().isAuto())
            flexItem.setMarginRight(autoMarginOffset);
    } else {
        if (flexItem.style().marginTop().isAuto())
            flexItem.setMarginTop(autoMarginOffset);
        if (flexItem.style().marginBottom().isAuto())
            flexItem.setMarginBottom(autoMarginOffset);
    }
}

bool FlexFormattingContext::updateAutoMarginsInCrossAxis(FlexLayoutItem& flexLayoutItem, LayoutUnit& crossOffset, LayoutUnit availableAlignmentSpace)
{
    // 9.6. (#13) Resolve cross-axis auto margins: if both cross-axis margins are auto, split the free space
    // between them; if only one is auto, give it all the free space so the item's outer cross size fills the line.
    auto& flexItem = flexLayoutItem.renderer.get();
    ASSERT(!flexItem.isOutOfFlowPositioned());
    ASSERT(availableAlignmentSpace >= 0_lu);

    bool isHorizontal = m_constraints.isHorizontalFlow;
    auto& style = flexLayoutItem.style();
    auto& topOrLeft = isHorizontal ? style.marginTop() : style.marginLeft();
    auto& bottomOrRight = isHorizontal ? style.marginBottom() : style.marginRight();
    if (topOrLeft.isAuto() && bottomOrRight.isAuto()) {
        crossOffset += availableAlignmentSpace / 2;
        if (isHorizontal) {
            flexItem.setMarginTop(availableAlignmentSpace / 2);
            flexItem.setMarginBottom(availableAlignmentSpace / 2);
        } else {
            flexItem.setMarginLeft(availableAlignmentSpace / 2);
            flexItem.setMarginRight(availableAlignmentSpace / 2);
        }
        return true;
    }
    bool shouldAdjustTopOrLeft = true;
    if (m_constraints.isColumnFlow && flexItem.writingMode().isInlineFlipped()) {
        // For column flows, only make this adjustment if topOrLeft corresponds to
        // the "before" margin, so that the rtl-column flip in computeFlexItemRects
        // will do the right thing.
        shouldAdjustTopOrLeft = false;
    }
    if (!m_constraints.isColumnFlow && flexItem.writingMode().isBlockFlipped()) {
        // If we are a flipped writing mode, we need to adjust the opposite side.
        // This is only needed for row flows because this only affects the
        // block-direction axis.
        shouldAdjustTopOrLeft = false;
    }

    if (topOrLeft.isAuto()) {
        if (shouldAdjustTopOrLeft)
            crossOffset += availableAlignmentSpace;

        if (isHorizontal)
            flexItem.setMarginTop(availableAlignmentSpace);
        else
            flexItem.setMarginLeft(availableAlignmentSpace);
        return true;
    }

    if (bottomOrRight.isAuto()) {
        if (!shouldAdjustTopOrLeft)
            crossOffset += availableAlignmentSpace;

        if (isHorizontal)
            flexItem.setMarginBottom(availableAlignmentSpace);
        else
            flexItem.setMarginRight(availableAlignmentSpace);
        return true;
    }
    return false;
}

LayoutUnit FlexFormattingContext::applyStretchAlignmentToFlexItem(const FlexLayoutItem& flexLayoutItem, LayoutUnit lineCrossAxisExtent, LayoutUnit crossContentExtent)
{
    CheckedRef flexItem = flexLayoutItem.renderer;
    CheckedRef style = flexLayoutItem.style();
    // 9.4. (#11) A stretched item's used cross size is its flex line's cross size minus the item's cross-axis
    // margins, clamped by the item's used min and max cross sizes. (An item with a definite cross size is not
    // stretched, but still gets the stretch min/max clamp via applyStretchMinMaxCrossSize.)
    if (flexLayoutItem.mainAxisIsInlineAxis) {
        // Cross axis is block axis (height).
        if (!style->logicalHeight().isAuto() && !style->logicalHeight().isStretch())
            return applyStretchMinMaxCrossSize(flexLayoutItem, lineCrossAxisExtent, LogicalBoxAxis::Block, crossContentExtent);

        auto stretchedLogicalHeight = std::max(flexItem->borderAndPaddingLogicalHeight(),
            lineCrossAxisExtent - flexFormattingUtils().crossAxisMarginExtentForFlexItem(flexLayoutItem));
        ASSERT(!flexItem->needsLayout());
        auto blockSize = flexItem->constrainLogicalHeightByMinMax(stretchedLogicalHeight, integrationUtils().flexItemContentLogicalHeight(flexLayoutItem));

        // FIXME: Can avoid laying out here in some cases. See https://webkit.org/b/87905.
        auto flexItemNeedsLayout = [&] {
            if (blockSize != flexItem->logicalHeight())
                return true;
            // Have to force another relayout even though the child is sized correctly,
            // because its descendants are not sized correctly yet.
            // The previous layout of the child was done without an override height set.
            return layoutState().hasFlexItemCompletedLayout(flexItem) && integrationUtils().flexItemHasPercentHeightDescendants(flexLayoutItem);
        };
        if (flexItemNeedsLayout())
            integrationUtils().applyStretchedLogicalHeightToFlexItem(flexLayoutItem, blockSize);
        else {
            // This sets the used height for the flex item, making its height definite.
            integrationUtils().setFlexItemOverridingBorderBoxLogicalHeight(flexLayoutItem, blockSize);
        }
        return blockSize;
    }

    // Cross axis is inline axis (width).
    if (!style->logicalWidth().isAuto() && !style->logicalWidth().isStretch())
        return applyStretchMinMaxCrossSize(flexLayoutItem, lineCrossAxisExtent, LogicalBoxAxis::Inline, crossContentExtent);

    auto flexItemWidth = std::max(0_lu, lineCrossAxisExtent - flexFormattingUtils().crossAxisMarginExtentForFlexItem(flexLayoutItem));
    flexItemWidth = flexItem->constrainLogicalWidthByMinMax(flexItemWidth, crossContentExtent, m_flexBox);

    if (flexItemWidth != flexItem->logicalWidth())
        integrationUtils().layoutFlexItemForStretchedCrossSize(flexLayoutItem, flexItemWidth, LogicalBoxAxis::Inline);
    return flexItemWidth;
}

LayoutUnit FlexFormattingContext::applyStretchMinMaxCrossSize(const FlexLayoutItem& flexLayoutItem, LayoutUnit lineCrossAxisExtent, LogicalBoxAxis crossAxis, LayoutUnit crossContentExtent)
{
    CheckedRef flexItem = flexLayoutItem.renderer;
    // Clamp an item that has a definite cross size by its used min and max cross sizes, resolving a 'stretch'
    // keyword on either against the flex line's cross size (part of 9.4 #11).
    bool isBlockAxis = crossAxis == LogicalBoxAxis::Block;
    CheckedRef style = flexLayoutItem.style();
    auto& min = isBlockAxis ? style->logicalMinHeight() : style->logicalMinWidth();
    auto& max = isBlockAxis ? style->logicalMaxHeight() : style->logicalMaxWidth();
    bool minIsStretch = min.isStretch();
    bool maxIsStretch = !max.isNone() && max.isStretch();
    if (!minIsStretch && !maxIsStretch)
        return flexFormattingUtils().crossAxisExtentForFlexItem(flexLayoutItem);

    // The block-axis floor ensures the stretched size never goes below border+padding,
    // matching the behavior in applyStretchAlignmentToFlexItem.
    auto stretchValue = std::max(isBlockAxis ? flexItem->borderAndPaddingLogicalHeight() : 0_lu,
        lineCrossAxisExtent - flexFormattingUtils().crossAxisMarginExtentForFlexItem(flexLayoutItem));

    auto computeBlockSize = [&](const auto& size, LayoutUnit fallback) {
        return flexItem->computeLogicalHeightUsing(size, std::nullopt).value_or(fallback);
    };
    auto computeInlineSize = [&](const auto& size) {
        return flexItem->computeLogicalWidthUsing(size, crossContentExtent, m_flexBox);
    };

    // Compute the specified cross-size, unclamped by stretch min/max.
    // We cannot use the current laid-out size because the initial layout
    // resolves stretch against the container, not the flex line.
    auto specifiedSize = isBlockAxis ? computeBlockSize(style->logicalHeight(), flexItem->logicalHeight()) : computeInlineSize(style->logicalWidth());

    // Resolve each constraint: stretch resolves to the line cross size,
    // non-stretch constraints are computed normally.
    auto effectiveMax = [&] {
        if (maxIsStretch)
            return stretchValue;
        if (max.isNone())
            return LayoutUnit::max();
        return isBlockAxis ? computeBlockSize(max, LayoutUnit::max()) : computeInlineSize(max);
    }();

    // FIXME: The auto minimum does not account for aspect-ratio automatic
    // minimums, which are computed in constrainLogicalHeightByMinMax.
    auto effectiveMin = [&] {
        if (minIsStretch)
            return stretchValue;
        return isBlockAxis ? computeBlockSize(min, 0_lu) : computeInlineSize(min);
    }();

    auto newSize = std::max(std::min(specifiedSize, effectiveMax), effectiveMin);

    auto currentSize = isBlockAxis ? flexItem->logicalHeight() : flexItem->logicalWidth();
    if (newSize != currentSize)
        integrationUtils().layoutFlexItemForStretchedCrossSize(flexLayoutItem, newSize, crossAxis);
    return newSize;
}

const FlexFormattingUtils& FlexFormattingContext::flexFormattingUtils() const
{
    return m_flexFormattingUtils;
}

FlexLayoutState& FlexFormattingContext::layoutState() const
{
    return integrationUtils().flexLayoutState();
}

FlexLayoutItem::FlexLayoutItem(RenderBox& flexItem, bool flexContainerIsHorizontalFlow, bool everHadLayout, bool shouldInvalidateChildContent)
    : renderer(flexItem)
    , mainAxisBorderAndPadding(flexContainerIsHorizontalFlow ? flexItem.horizontalBorderAndPaddingExtent() : flexItem.verticalBorderAndPaddingExtent())
    , mainAxisMargin(flexContainerIsHorizontalFlow ? flexItem.horizontalMarginExtent() : flexItem.verticalMarginExtent())
    , crossAxisBorderAndPadding(flexContainerIsHorizontalFlow ? flexItem.verticalBorderAndPaddingExtent() : flexItem.horizontalBorderAndPaddingExtent())
    , mainAxisIsInlineAxis(flexContainerIsHorizontalFlow == flexItem.isHorizontalWritingMode())
    , everHadLayout(everHadLayout)
    , shouldInvalidateChildContent(shouldInvalidateChildContent)
{
    ASSERT(!flexItem.isOutOfFlowPositioned());
}

LayoutUnit FlexLayoutItem::hypotheticalMainAxisMarginBoxSize(LayoutUnit hypotheticalMainContentSize) const
{
    return hypotheticalMainContentSize + mainAxisBorderAndPadding + mainAxisMargin;
}

LayoutUnit FlexLayoutItem::flexBaseMarginBoxSize(LayoutUnit flexBaseContentSize) const
{
    return flexBaseContentSize + mainAxisBorderAndPadding + mainAxisMargin;
}

LayoutUnit FlexLayoutItem::flexedMarginBoxSize(LayoutUnit mainSize) const
{
    return mainSize + mainAxisBorderAndPadding + mainAxisMargin;
}

const Style::ComputedStyle& FlexLayoutItem::style() const
{
    return renderer->style();
}

} // namespace WebCore
