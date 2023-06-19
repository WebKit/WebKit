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
#include "FlexLayout.h"

#include "FlexRect.h"
#include <wtf/FixedVector.h>

namespace WebCore {
namespace Layout {

struct FlexBaseAndHypotheticalMainSize {
    LayoutUnit flexBase { 0.f };
    LayoutUnit hypotheticalMainSize { 0.f };
};

struct PositionAndMargins {
    LayoutUnit position;
    LayoutUnit marginStart;
    LayoutUnit marginEnd;
};

FlexLayout::FlexLayout(const ElementBox& flexContainer)
    : m_flexContainer(flexContainer)
{
}

FlexLayout::LogicalFlexItemRects FlexLayout::layout(const LogicalConstraints& logicalConstraints, const LogicalFlexItems& flexItems)
{
    // This follows https://www.w3.org/TR/css-flexbox-1/#layout-algorithm
    // 9.2. (#2) Determine the available main and cross space for the flex items
    computeAvailableMainAndCrossSpace(logicalConstraints);

    SizeList flexItemsMainSizeList(flexItems.size());
    SizeList flexItemsCrossSizeList(flexItems.size());
    LinesCrossSizeList flexLinesCrossSizeList;
    auto lineRanges = LineRanges { };

    auto performContentSizing = [&] {
        auto needsMainAxisLayout = true;

        while (needsMainAxisLayout) {
            auto performMainAxisSizing = [&] {
                // 9.2. (#3) Determine the flex base size and hypothetical main size of each item
                auto flexBaseAndHypotheticalMainSizeList = flexBaseAndHypotheticalMainSizeForFlexItems(logicalConstraints.mainAxis, flexItems);
                // 9.2. (#4) Determine the main size of the flex container
                auto flexContainerMainSize = this->flexContainerMainSize(logicalConstraints.mainAxis);
                // 9.3. (#5) Collect flex items into flex lines
                lineRanges = computeFlexLines(flexItems, flexContainerMainSize, flexBaseAndHypotheticalMainSizeList);
                // 9.3. (#6) Resolve the flexible lengths of all the flex items to find their used main size
                flexItemsMainSizeList = computeMainSizeForFlexItems(flexItems, lineRanges, flexContainerMainSize, flexBaseAndHypotheticalMainSizeList);
            };
            performMainAxisSizing();

            auto performCrossAxisSizing = [&] {
                // 9.4. (#7) Determine the hypothetical cross size of each item
                auto hypotheticalCrossSizeList = hypotheticalCrossSizeForFlexItems(flexItems);
                // 9.4. (#8) Calculate the cross size of each flex line
                flexLinesCrossSizeList = crossSizeForFlexLines(lineRanges, logicalConstraints.crossAxis, flexItems, hypotheticalCrossSizeList);
                // 9.4. (#9) Handle 'align-content: stretch
                stretchFlexLines(flexLinesCrossSizeList, lineRanges.size(), logicalConstraints.crossAxis);
                // 9.4. (#10) Collapse visibility:collapse items
                auto collapsedContentNeedsSecondLayout = collapseNonVisibleFlexItems();
                if (collapsedContentNeedsSecondLayout)
                    return;
                // 9.4. (#11) Determine the used cross size of each flex item
                flexItemsCrossSizeList = computeCrossSizeForFlexItems(flexItems, lineRanges, flexLinesCrossSizeList, hypotheticalCrossSizeList);
                needsMainAxisLayout = false;
            };
            performCrossAxisSizing();
        }
    };
    performContentSizing();

    auto mainPositionAndMargins = PositionAndMarginsList { flexItems.size() };
    auto crossPositionAndMargins = PositionAndMarginsList { flexItems.size() };
    auto linesCrossPositionList = LinesCrossPositionList { };

    auto performContentAlignment = [&] {
        // 9.5. (#12) Main-Axis Alignment
        mainPositionAndMargins = handleMainAxisAlignment(m_availableMainSpace, lineRanges, flexItems, flexItemsMainSizeList);
        // 9.6. (#13 - #16) Cross-Axis Alignment
        crossPositionAndMargins = handleCrossAxisAlignmentForFlexItems(flexItems, lineRanges, flexItemsCrossSizeList, flexLinesCrossSizeList);
        linesCrossPositionList = handleCrossAxisAlignmentForFlexLines(logicalConstraints.crossAxis, lineRanges, flexLinesCrossSizeList);
    };
    performContentAlignment();

    auto computeFlexItemRects = [&] {
        auto flexRects = LogicalFlexItemRects { flexItems.size() };
        for (size_t lineIndex = 0; lineIndex < lineRanges.size(); ++lineIndex) {
            auto lineRange = lineRanges[lineIndex];
            for (auto flexItemIndex = lineRange.begin(); flexItemIndex < lineRange.end(); ++flexItemIndex) {
                auto flexItemMainPosition = mainPositionAndMargins[flexItemIndex].position;
                auto flexItemCrossPosition = linesCrossPositionList[lineIndex] + crossPositionAndMargins[lineIndex].position;
                flexRects[flexItemIndex] = {
                    { flexItemMainPosition, flexItemCrossPosition, flexItemsMainSizeList[flexItemIndex], flexItemsCrossSizeList[flexItemIndex] },
                    { { mainPositionAndMargins[flexItemIndex].marginStart, mainPositionAndMargins[flexItemIndex].marginEnd }, { crossPositionAndMargins[flexItemIndex].marginStart, crossPositionAndMargins[flexItemIndex].marginEnd } }
                };
            }
        }
        return flexRects;
    };
    return computeFlexItemRects();
}

void FlexLayout::computeAvailableMainAndCrossSpace(const LogicalConstraints& logicalConstraints)
{
    UNUSED_PARAM(logicalConstraints);
}

FlexLayout::FlexBaseAndHypotheticalMainSizeList FlexLayout::flexBaseAndHypotheticalMainSizeForFlexItems(const LogicalConstraints::AxisGeometry& mainAxis, const LogicalFlexItems& flexItems) const
{
    auto flexBaseAndHypotheticalMainSizeList = FlexBaseAndHypotheticalMainSizeList { };
    for (auto& flexItem : flexItems) {
        auto flexBase = LayoutUnit { };
        // 3. Determine the flex base size and hypothetical main size of each item:
        auto computeFlexBase = [&] {
            // A. If the item has a definite used flex basis, that's the flex base size.
            if (auto definiteFlexBase = flexItem.mainAxis().definiteFlexBasis) {
                flexBase = *definiteFlexBase;
                return;
            }
            // B. If the flex item has...
            if (flexItem.hasAspectRatio() && flexItem.hasContentFlexBasis() && flexItem.crossAxis().definiteSize) {
                // The flex base size is calculated from its inner cross size and the flex item's intrinsic aspect ratio.
                ASSERT_NOT_IMPLEMENTED_YET();
                return;
            }
            // C. If the used flex basis is content or depends on its available space, and the flex container is being sized under
            //    a min-content or max-content constraint, size the item under that constraint
            auto flexBasisContentOrAvailableSpaceDependent = flexItem.hasContentFlexBasis() || flexItem.hasAvailableSpaceDependentFlexBasis();
            auto flexContainerHasMinMaxConstraints = mainAxis.minimumContentSize || mainAxis.maximumContentSize;
            if (flexBasisContentOrAvailableSpaceDependent && flexContainerHasMinMaxConstraints) {
                // Compute flex item's main size.
                ASSERT_NOT_IMPLEMENTED_YET();
                return;
            }
            // D. If the used flex basis is content or depends on its available space, the available main size is infinite,
            //    and the flex item's inline axis is parallel to the main axis, lay the item out using the rules for a box in an orthogonal flow.
            //    The flex base size is the item's max-content main size.
            if (flexBasisContentOrAvailableSpaceDependent && flexItem.isOrhogonal()) {
                // Lay the item out using the rules for a box in an orthogonal flow. The flex base size is the item's max-content main size.
                ASSERT_NOT_IMPLEMENTED_YET();
                return;
            }
            // E. Otherwise, size the item into the available space using its used flex basis in place of its main size, treating a value of content as max-content.
            ASSERT_NOT_IMPLEMENTED_YET();
        };
        computeFlexBase();

        auto hypotheticalMainSize = [&] {
            // The hypothetical main size is the item's flex base size clamped according to its used min and max main sizes (and flooring the content box size at zero).
            auto hypotheticalValue = flexBase;
            auto maximum = flexItem.mainAxis().maximumSize.value_or(hypotheticalValue);
            auto minimum = flexItem.mainAxis().minimumSize.value_or(hypotheticalValue);
            return std::max(maximum, std::min(minimum, hypotheticalValue));
        };
        flexBaseAndHypotheticalMainSizeList.append({ flexBase, hypotheticalMainSize() });
    }
    return flexBaseAndHypotheticalMainSizeList;
}

LayoutUnit FlexLayout::flexContainerMainSize(const LogicalConstraints::AxisGeometry& mainAxis) const
{
    UNUSED_PARAM(mainAxis);
    return { };
}

FlexLayout::LineRanges FlexLayout::computeFlexLines(const LogicalFlexItems& flexItems, LayoutUnit flexContainerMainSize, const FlexBaseAndHypotheticalMainSizeList& flexBaseAndHypotheticalMainSizeList) const
{
    // Collect flex items into flex lines:
    // If the flex container is single-line, collect all the flex items into a single flex line.
    // Otherwise, starting from the first uncollected item, collect consecutive items one by one until the first time that the next collected
    // item would not fit into the flex container's inner main size.
    // If the very first uncollected item wouldn't fit, collect just it into the line.
    // For this step, the size of a flex item is its outer hypothetical main size. (Note: This can be negative.)
    if (isSingleLineFlexContainer())
        return { 0, flexBaseAndHypotheticalMainSizeList.size() };

    auto lineRanges = LineRanges { };
    size_t lastWrapIndex = 0;
    auto flexItemsMainSize = LayoutUnit { };
    for (size_t flexItemIndex = 0; flexItemIndex < flexBaseAndHypotheticalMainSizeList.size(); ++flexItemIndex) {
        auto flexItemHypotheticalOuterMainSize = flexItems[flexItemIndex].mainAxis().margin() + flexBaseAndHypotheticalMainSizeList[flexItemIndex].hypotheticalMainSize;
        auto isFlexLineEmpty = flexItemIndex == lastWrapIndex;
        if (isFlexLineEmpty || flexItemsMainSize + flexItemHypotheticalOuterMainSize <= flexContainerMainSize) {
            flexItemsMainSize += flexItemHypotheticalOuterMainSize;
            continue;
        }
        lineRanges.append({ lastWrapIndex, flexItemIndex });
        flexItemsMainSize = flexItemHypotheticalOuterMainSize;
        lastWrapIndex = flexItemIndex;
    }
    lineRanges.append({ lastWrapIndex, flexBaseAndHypotheticalMainSizeList.size() });
    return lineRanges;
}

FlexLayout::SizeList FlexLayout::computeMainSizeForFlexItems(const LogicalFlexItems& flexItems, const LineRanges& lineRanges, LayoutUnit flexContainerMainSize, const FlexBaseAndHypotheticalMainSizeList& flexBaseAndHypotheticalMainSizeList) const
{
    SizeList mainSizeList(flexItems.size());
    Vector<bool> isInflexibleItemList(flexItems.size(), false);

    for (size_t lineIndex = 0; lineIndex < lineRanges.size(); ++lineIndex) {
        auto lineRange = lineRanges[lineIndex];
        auto nonFrozenSet = ListHashSet<size_t> { };

        // 1. Determine the used flex factor. Sum the outer hypothetical main sizes of all items on the line.
        //    If the sum is less than the flex container's inner main size, use the flex grow factor for the rest of this algorithm;
        //    otherwise, use the flex shrink factor.
        auto shouldUseFlexGrowFactor = [&] {
            auto hypotheticalOuterMainSizes = LayoutUnit { };
            for (auto flexItemIndex = lineRange.begin(); flexItemIndex < lineRange.end(); ++flexItemIndex)
                hypotheticalOuterMainSizes += (flexItems[flexItemIndex].mainAxis().margin() + flexBaseAndHypotheticalMainSizeList[flexItemIndex].hypotheticalMainSize);
            return hypotheticalOuterMainSizes < flexContainerMainSize;
        }();

        // 2. Size inflexible items. Freeze, setting its target main size to its hypothetical main size.
        //    any item that has a flex factor of zero
        //    if using the flex grow factor: any item that has a flex base size greater than its hypothetical main size
        //    if using the flex shrink factor: any item that has a flex base size smaller than its hypothetical main size
        for (auto flexItemIndex = lineRange.begin(); flexItemIndex < lineRange.end(); ++flexItemIndex) {
            auto shouldFreeze = [&] {
                if (!flexItems[flexItemIndex].growFactor() && !flexItems[flexItemIndex].shrinkFactor())
                    return true;
                auto flexBaseAndHypotheticalMainSize = flexBaseAndHypotheticalMainSizeList[flexItemIndex];
                if (shouldUseFlexGrowFactor && flexBaseAndHypotheticalMainSize.flexBase > flexBaseAndHypotheticalMainSize.hypotheticalMainSize)
                    return true;
                if (!shouldUseFlexGrowFactor && flexBaseAndHypotheticalMainSize.flexBase < flexBaseAndHypotheticalMainSize.hypotheticalMainSize)
                    return true;
                return false;
            };
            if (shouldFreeze()) {
                mainSizeList[flexItemIndex] = flexBaseAndHypotheticalMainSizeList[flexItemIndex].hypotheticalMainSize;
                isInflexibleItemList[flexItemIndex] = true;
                continue;
            }
            nonFrozenSet.add(flexItemIndex);
        }

        // 3. Calculate initial free space. Sum the outer sizes of all items on the line, and subtract this from the flex container's inner main size.
        //    For frozen items, use their outer target main size; for other items, use their outer flex base size.
        auto computedFreeSpace = [&] {
            auto lineContentMainSize = LayoutUnit { };
            for (auto flexItemIndex = lineRange.begin(); flexItemIndex < lineRange.end(); ++flexItemIndex) {
                auto flexItemOuterMainSize = flexItems[flexItemIndex].mainAxis().margin() + (nonFrozenSet.contains(flexItemIndex) ? flexBaseAndHypotheticalMainSizeList[flexItemIndex].flexBase : flexBaseAndHypotheticalMainSizeList[flexItemIndex].hypotheticalMainSize);
                lineContentMainSize += flexItemOuterMainSize;
            }
            return flexContainerMainSize - lineContentMainSize;
        };

        // 4. Loop:
        while (true) {
            // a. Check for flexible items. If all the flex items on the line are frozen, free space has been distributed; exit this loop.
            if (nonFrozenSet.isEmpty())
                break;

            // b. Calculate the remaining free space as for initial free space, above. If the sum of the unfrozen flex items' flex factors
            //    is less than one, multiply the initial free space by this sum. If the magnitude of this value is less than the magnitude of the
            //    remaining free space, use this as the remaining free space.
            auto freeSpace = computedFreeSpace();
            auto adjustFreeSpaceWithFlexFactors = [&] {
                auto totalFlexFactor = 0.f;
                for (auto nonFrozenIndex : nonFrozenSet)
                    totalFlexFactor += flexItems[nonFrozenIndex].growFactor() + flexItems[nonFrozenIndex].shrinkFactor();
                if (totalFlexFactor < 1)
                    freeSpace *= totalFlexFactor;
            };
            adjustFreeSpaceWithFlexFactors();

            auto minimumViolationList = Vector<size_t> { flexItems.size() };
            auto maximumViolationList = Vector<size_t> { flexItems.size() };
            // c. Distribute free space proportional to the flex factors.
            auto usedTotalFactor = 0.f;
            for (auto nonFrozenIndex : nonFrozenSet)
                usedTotalFactor += shouldUseFlexGrowFactor ? flexItems[nonFrozenIndex].growFactor() : flexItems[nonFrozenIndex].shrinkFactor() * flexBaseAndHypotheticalMainSizeList[nonFrozenIndex].flexBase;

            for (auto nonFrozenIndex : nonFrozenSet) {
                if (!usedTotalFactor) {
                    mainSizeList[nonFrozenIndex] = flexBaseAndHypotheticalMainSizeList[nonFrozenIndex].flexBase;
                    continue;
                }
                if (shouldUseFlexGrowFactor) {
                    // If using the flex grow factor
                    // Find the ratio of the item's flex grow factor to the sum of the flex grow factors of all unfrozen items on the line.
                    // Set the item's target main size to its flex base size plus a fraction of the remaining free space proportional to the ratio.
                    auto growFactor = flexItems[nonFrozenIndex].growFactor() / usedTotalFactor;
                    mainSizeList[nonFrozenIndex] = flexBaseAndHypotheticalMainSizeList[nonFrozenIndex].flexBase + freeSpace * growFactor;
                    continue;
                }
                // If using the flex shrink factor
                // For every unfrozen item on the line, multiply its flex shrink factor by its inner flex base size, and note this as its scaled flex shrink factor.
                // Find the ratio of the item's scaled flex shrink factor to the sum of the scaled flex shrink factors of all unfrozen items on the line.
                // Set the item's target main size to its flex base size minus a fraction of the absolute value of the remaining free space proportional to the ratio.
                // Note this may result in a negative inner main size; it will be corrected in the next step.
                auto flexBaseSize = flexBaseAndHypotheticalMainSizeList[nonFrozenIndex].flexBase;
                auto scaledShrinkFactor = flexItems[nonFrozenIndex].shrinkFactor() * flexBaseSize;
                auto shrinkFactor = scaledShrinkFactor / usedTotalFactor;
                mainSizeList[nonFrozenIndex] = flexBaseSize - std::abs(freeSpace * shrinkFactor);
            }

            // d. Fix min/max violations. Clamp each non-frozen item's target main size by its used min and max main sizes and floor
            //    its content-box size at zero. If the item's target main size was made smaller by this, it's a max violation.
            //    If the item's target main size was made larger by this, it's a min violation.
            auto totalViolation = LayoutUnit { };
            for (auto nonFrozenIndex : nonFrozenSet) {
                auto mainSize = mainSizeList[nonFrozenIndex];
                auto maximum = flexItems[nonFrozenIndex].mainAxis().maximumSize.value_or(mainSize);
                auto minimum = flexItems[nonFrozenIndex].mainAxis().minimumSize.value_or(mainSize);
                mainSize = std::max(maximum, std::min(minimum, mainSize));
                auto mainContentBoxSize = std::max(0_lu, mainSize - flexItems[nonFrozenIndex].mainAxis().borderAndPadding);
                if (mainContentBoxSize < mainSize)
                    maximumViolationList.append(nonFrozenIndex);
                else if (mainContentBoxSize > mainSize)
                    minimumViolationList.append(nonFrozenIndex);
                mainSizeList[nonFrozenIndex] = mainSize;
            }

            // e. Freeze over-flexed items. The total violation is the sum of the adjustments from the previous step
            //    ∑(clamped size - unclamped size). If the total violation is:
            //      Zero : Freeze all items.
            //      Positive: Freeze all the items with min violations.
            //      Negative: Freeze all the items with max violations.
            if (!totalViolation)
                nonFrozenSet.clear();
            else if (totalViolation > 0) {
                for (auto minimimViolationIndex : minimumViolationList)
                    nonFrozenSet.remove(minimimViolationIndex);
            } else {
                for (auto maximumViolationIndex : maximumViolationList)
                    nonFrozenSet.remove(maximumViolationIndex);
            }
        }
    }
    return mainSizeList;
}

FlexLayout::SizeList FlexLayout::hypotheticalCrossSizeForFlexItems(const LogicalFlexItems& flexItems) const
{
    UNUSED_PARAM(flexItems);
    return { };
}

FlexLayout::LinesCrossSizeList FlexLayout::crossSizeForFlexLines(const LineRanges& lineRanges, const LogicalConstraints::AxisGeometry& crossAxis, const LogicalFlexItems& flexItems, const SizeList& flexItemsHypotheticalCrossSizeList) const
{
    UNUSED_PARAM(lineRanges);
    UNUSED_PARAM(crossAxis);
    UNUSED_PARAM(flexItems);
    UNUSED_PARAM(flexItemsHypotheticalCrossSizeList);
    return { };
}

void FlexLayout::stretchFlexLines(LinesCrossSizeList& flexLinesCrossSizeList, size_t numberOfLines, const LogicalConstraints::AxisGeometry& crossAxis) const
{
    UNUSED_PARAM(flexLinesCrossSizeList);
    UNUSED_PARAM(numberOfLines);
    UNUSED_PARAM(crossAxis);
}

bool FlexLayout::collapseNonVisibleFlexItems()
{
    return false;
}

FlexLayout::SizeList FlexLayout::computeCrossSizeForFlexItems(const LogicalFlexItems& flexItems, const LineRanges& lineRanges, const LinesCrossSizeList& flexLinesCrossSizeList, const SizeList& flexItemsHypotheticalCrossSizeList) const
{
    UNUSED_PARAM(flexItems);
    UNUSED_PARAM(lineRanges);
    UNUSED_PARAM(flexLinesCrossSizeList);
    UNUSED_PARAM(flexItemsHypotheticalCrossSizeList);
    return { };
}

FlexLayout::PositionAndMarginsList FlexLayout::handleMainAxisAlignment(LayoutUnit availableMainSpace, const LineRanges& lineRanges, const LogicalFlexItems& flexItems, const SizeList& flexItemsMainSizeList) const
{
    UNUSED_PARAM(availableMainSpace);
    UNUSED_PARAM(lineRanges);
    UNUSED_PARAM(flexItems);
    UNUSED_PARAM(flexItemsMainSizeList);
    return { };
}

FlexLayout::PositionAndMarginsList FlexLayout::handleCrossAxisAlignmentForFlexItems(const LogicalFlexItems& flexItems, const LineRanges& lineRanges, const SizeList& flexItemsCrossSizeList, const LinesCrossSizeList& flexLinesCrossSizeList) const
{
    UNUSED_PARAM(flexItems);
    UNUSED_PARAM(lineRanges);
    UNUSED_PARAM(flexItemsCrossSizeList);
    UNUSED_PARAM(flexLinesCrossSizeList);
    return { };
}

FlexLayout::LinesCrossPositionList FlexLayout::handleCrossAxisAlignmentForFlexLines(const LogicalConstraints::AxisGeometry& crossAxis, const LineRanges& lineRanges, LinesCrossSizeList& flexLinesCrossSizeList) const
{
    UNUSED_PARAM(crossAxis);
    UNUSED_PARAM(lineRanges);
    UNUSED_PARAM(flexLinesCrossSizeList);
    return { };
}

}
}
