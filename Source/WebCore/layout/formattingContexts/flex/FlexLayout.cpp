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

#include "FlexFormattingContext.h"
#include "FlexFormattingUtils.h"
#include "FlexRect.h"
#include "InlineFormattingContext.h"
#include "RenderStyleSetters.h"
#include "StyleContentAlignmentData.h"
#include "StyleSelfAlignmentData.h"
#include <wtf/FixedVector.h>
#include <wtf/ListHashSet.h>

namespace WebCore {
namespace Layout {

struct FlexBaseAndHypotheticalMainSize {
    LayoutUnit flexBase { 0.f };
    LayoutUnit hypotheticalMainSize { 0.f };
};

struct PositionAndMargins {
    LayoutUnit margin() const { return marginStart + marginEnd; }

    LayoutUnit position;
    LayoutUnit marginStart;
    LayoutUnit marginEnd;
};

FlexLayout::FlexLayout(FlexFormattingContext& flexFormattingContext)
    : m_flexFormattingContext(flexFormattingContext)
{
}

FlexLayout::LogicalFlexItemRects FlexLayout::layout(const ConstraintsForFlexContent& flexContainerConstraints, const LogicalFlexItems& flexItems)
{
    // This follows https://www.w3.org/TR/css-flexbox-1/#layout-algorithm
    SizeList flexItemsMainSizeList(flexItems.size());
    SizeList flexItemsCrossSizeList(flexItems.size());
    LinesCrossSizeList flexLinesCrossSizeList;
    auto lineRanges = LineRanges { };

    auto performContentSizing = [&] {
        auto needsMainAxisLayout = true;

        while (needsMainAxisLayout) {
            auto performMainAxisSizing = [&] {
                // 9.2. (#3) Determine the flex base size and hypothetical main size of each item
                auto flexBaseAndHypotheticalMainSizeList = flexBaseAndHypotheticalMainSizeForFlexItems(flexItems, flexContainerConstraints.isSizedUnderMinMax());
                // 9.2. (#4) Determine the main size of the flex container
                auto flexContainerInnerMainSize = this->flexContainerInnerMainSize(flexContainerConstraints.mainAxis());
                // 9.3. (#5) Collect flex items into flex lines
                lineRanges = computeFlexLines(flexItems, flexContainerInnerMainSize, flexBaseAndHypotheticalMainSizeList);
                // 9.3. (#6) Resolve the flexible lengths of all the flex items to find their used main size
                flexItemsMainSizeList = computeMainSizeForFlexItems(flexItems, lineRanges, flexContainerInnerMainSize, flexBaseAndHypotheticalMainSizeList);
            };
            performMainAxisSizing();

            auto performCrossAxisSizing = [&] {
                // 9.4. (#7) Determine the hypothetical cross size of each item
                auto hypotheticalCrossSizeList = hypotheticalCrossSizeForFlexItems(flexItems, flexItemsMainSizeList);
                // 9.4. (#8) Calculate the cross size of each flex line
                flexLinesCrossSizeList = crossSizeForFlexLines(lineRanges, flexContainerConstraints.crossAxis(), flexItems, hypotheticalCrossSizeList);
                // 9.4. (#9) Handle 'align-content: stretch
                stretchFlexLines(flexLinesCrossSizeList, lineRanges.size(), flexContainerConstraints.crossAxis().availableSize);
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
        mainPositionAndMargins = handleMainAxisAlignment(flexContainerInnerMainSize(flexContainerConstraints.mainAxis()), lineRanges, flexItems, flexItemsMainSizeList);
        // 9.6. (#13 - #16) Cross-Axis Alignment
        crossPositionAndMargins = handleCrossAxisAlignmentForFlexItems(flexItems, lineRanges, flexItemsCrossSizeList, flexLinesCrossSizeList);
        linesCrossPositionList = handleCrossAxisAlignmentForFlexLines(flexContainerConstraints.crossAxis().availableSize, lineRanges, flexLinesCrossSizeList);
    };
    performContentAlignment();

    auto computeFlexItemRects = [&] {
        auto flexRects = LogicalFlexItemRects { flexItems.size() };
        auto mainAxisGapValue = FlexFormattingUtils::mainAxisGapValue(flexContainer(), flexContainerConstraints.mainAxis().availableSize.value_or(0_lu));
        auto crossAxisGapValue = FlexFormattingUtils::crossAxisGapValue(flexContainer(), flexContainerConstraints.crossAxis().availableSize.value_or(0_lu));
        for (size_t lineIndex = 0; lineIndex < lineRanges.size(); ++lineIndex) {
            auto lineRange = lineRanges[lineIndex];
            for (auto flexItemIndex = lineRange.begin(); flexItemIndex < lineRange.end(); ++flexItemIndex) {
                auto flexItemMainPosition = mainPositionAndMargins[flexItemIndex].position + (flexItemIndex - lineRange.begin()) * mainAxisGapValue;
                auto flexItemCrossPosition = linesCrossPositionList[lineIndex] + crossPositionAndMargins[flexItemIndex].position + lineIndex * crossAxisGapValue;

                flexRects[flexItemIndex] = {
                    { flexItemMainPosition, flexItemCrossPosition, flexItemsMainSizeList[flexItemIndex], flexItemsCrossSizeList[flexItemIndex] },
                    { mainPositionAndMargins[flexItemIndex].marginStart, mainPositionAndMargins[flexItemIndex].marginEnd }, { crossPositionAndMargins[flexItemIndex].marginStart, crossPositionAndMargins[flexItemIndex].marginEnd }
                };
            }
        }
        return flexRects;
    };
    return computeFlexItemRects();
}

FlexLayout::FlexBaseAndHypotheticalMainSizeList FlexLayout::flexBaseAndHypotheticalMainSizeForFlexItems(const LogicalFlexItems& flexItems, bool isSizedUnderMinMaxConstraints) const
{
    auto flexBaseAndHypotheticalMainSizeList = FlexBaseAndHypotheticalMainSizeList { };
    for (auto& flexItem : flexItems) {
        // 3. Determine the flex base size and hypothetical main size of each item:
        auto computedFlexBase = [&]() -> LayoutUnit {
            // A. If the item has a definite used flex basis, that's the flex base size.
            if (auto definiteFlexBase = flexItem.mainAxis().definiteFlexBasis)
                return *definiteFlexBase;
            // B. If the flex item has...
            if (flexItem.hasAspectRatio() && flexItem.hasContentFlexBasis() && flexItem.crossAxis().definiteSize) {
                // The flex base size is calculated from its inner cross size and the flex item's intrinsic aspect ratio.
                ASSERT_NOT_IMPLEMENTED_YET();
                return { };
            }
            // C. If the used flex basis is content or depends on its available space, and the flex container is being sized under
            //    a min-content or max-content constraint, size the item under that constraint
            auto flexBasisContentOrAvailableSpaceDependent = flexItem.hasContentFlexBasis() || flexItem.hasAvailableSpaceDependentFlexBasis();
            if (flexBasisContentOrAvailableSpaceDependent && isSizedUnderMinMaxConstraints) {
                // Compute flex item's main size.
                ASSERT_NOT_IMPLEMENTED_YET();
                return { };
            }
            // D. If the used flex basis is content or depends on its available space, the available main size is infinite,
            //    and the flex item's inline axis is parallel to the main axis, lay the item out using the rules for a box in an orthogonal flow.
            //    The flex base size is the item's max-content main size.
            if (flexBasisContentOrAvailableSpaceDependent && flexItem.isOrhogonal()) {
                // Lay the item out using the rules for a box in an orthogonal flow. The flex base size is the item's max-content main size.
                ASSERT_NOT_IMPLEMENTED_YET();
                return { };
            }
            // E Otherwise, size the item into the available space using its used flex basis in place of its main size,
            // treating a value of content as max-content. If a cross size is needed to determine the main size (e.g. when the flex item’s main size
            // is in its block axis) and the flex item’s cross size is auto and not definite, in this calculation use fit-content as the flex item’s cross size.
            // The flex base size is the item’s resulting main size.
            if (flexItem.isOrhogonal() && !flexItem.crossAxis().definiteSize) {
                ASSERT_NOT_IMPLEMENTED_YET();
                return { };
            }
            return formattingUtils().usedMaxContentSizeInMainAxis(flexItem);
        };
        auto flexBaseSize = computedFlexBase();
        // The hypothetical main size is the item's flex base size clamped according to its used min and max main sizes (and flooring the content box size at zero).
        auto hypotheticalMainSize = std::max(formattingUtils().usedMinimumSizeInMainAxis(flexItem), flexBaseSize);
        if (auto usedMaximumMainSize = formattingUtils().usedMaximumSizeInMainAxis(flexItem))
            hypotheticalMainSize = std::min(*usedMaximumMainSize, hypotheticalMainSize);
        flexBaseAndHypotheticalMainSizeList.append({ flexBaseSize, hypotheticalMainSize });
    }
    return flexBaseAndHypotheticalMainSizeList;
}

LayoutUnit FlexLayout::flexContainerInnerMainSize(const ConstraintsForFlexContent::AxisGeometry& mainAxisGeometry) const
{
    // 4. Determine the main size of the flex container using the rules of the formatting context in which it participates.
    // FIXME: above.
    return mainAxisGeometry.availableSize.value_or(LayoutUnit::max());
}

static LayoutUnit outerMainSize(const LogicalFlexItem& flexItem, LayoutUnit mainSize, std::optional<LayoutUnit> usedMargin = { })
{
    auto outerMainSize = usedMargin.value_or(flexItem.mainAxis().margin());
    if (flexItem.isContentBoxBased())
        outerMainSize += flexItem.mainAxis().borderAndPadding;
    outerMainSize += mainSize;
    return outerMainSize;
}

static LayoutUnit outerCrossSize(const LogicalFlexItem& flexItem, LayoutUnit crossSize, std::optional<LayoutUnit> usedMargin = { })
{
    auto outerCrossSize = usedMargin.value_or(flexItem.crossAxis().margin());
    if (flexItem.isContentBoxBased())
        outerCrossSize += flexItem.crossAxis().borderAndPadding;
    outerCrossSize += crossSize;
    return outerCrossSize;
}

FlexLayout::LineRanges FlexLayout::computeFlexLines(const LogicalFlexItems& flexItems, LayoutUnit flexContainerInnerMainSize, const FlexBaseAndHypotheticalMainSizeList& flexBaseAndHypotheticalMainSizeList) const
{
    // Collect flex items into flex lines:
    // If the flex container is single-line, collect all the flex items into a single flex line.
    // Otherwise, starting from the first uncollected item, collect consecutive items one by one until the first time that the next collected
    // item would not fit into the flex container's inner main size.
    // If the very first uncollected item wouldn't fit, collect just it into the line.
    // For this step, the size of a flex item is its outer hypothetical main size. (Note: This can be negative.)
    if (isSingleLineFlexContainer())
        return { { 0, flexBaseAndHypotheticalMainSizeList.size() } };

    auto mainAxisGapValue = FlexFormattingUtils::mainAxisGapValue(flexContainer(), flexContainerInnerMainSize);
    auto lineRanges = LineRanges { };
    size_t lastWrapIndex = 0;
    auto flexItemsMainSize = LayoutUnit { };
    for (size_t flexItemIndex = 0; flexItemIndex < flexBaseAndHypotheticalMainSizeList.size(); ++flexItemIndex) {
        auto flexItemHypotheticalOuterMainSize = outerMainSize(flexItems[flexItemIndex], flexBaseAndHypotheticalMainSizeList[flexItemIndex].hypotheticalMainSize);
        auto numberOfFlexItemsOnLine = flexItemIndex - lastWrapIndex;
        auto mainAxisGapSize = mainAxisGapValue * numberOfFlexItemsOnLine;
        if (!numberOfFlexItemsOnLine || flexItemsMainSize + flexItemHypotheticalOuterMainSize + mainAxisGapSize <= flexContainerInnerMainSize) {
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

FlexLayout::SizeList FlexLayout::computeMainSizeForFlexItems(const LogicalFlexItems& flexItems, const LineRanges& lineRanges, LayoutUnit flexContainerInnerMainSize, const FlexBaseAndHypotheticalMainSizeList& flexBaseAndHypotheticalMainSizeList) const
{
    SizeList mainSizeList(flexItems.size());
    Vector<bool> isInflexibleItemList(flexItems.size(), false);

    for (size_t lineIndex = 0; lineIndex < lineRanges.size(); ++lineIndex) {
        auto lineRange = lineRanges[lineIndex];
        auto nonFrozenSet = ListHashSet<size_t> { };
        auto availableMainSpaceForLineContent = mainAxisAvailableSpaceForItemAlignment(flexContainerInnerMainSize, lineRange.distance());

        // 1. Determine the used flex factor. Sum the outer hypothetical main sizes of all items on the line.
        //    If the sum is less than the flex container's inner main size, use the flex grow factor for the rest of this algorithm;
        //    otherwise, use the flex shrink factor.
        auto shouldUseFlexGrowFactor = [&] {
            auto hypotheticalOuterMainSizes = LayoutUnit { };
            for (auto flexItemIndex = lineRange.begin(); flexItemIndex < lineRange.end(); ++flexItemIndex) {
                auto flexItemHypotheticalOuterMainSize = outerMainSize(flexItems[flexItemIndex], flexBaseAndHypotheticalMainSizeList[flexItemIndex].hypotheticalMainSize);
                hypotheticalOuterMainSizes += flexItemHypotheticalOuterMainSize;
            }
            return hypotheticalOuterMainSizes < availableMainSpaceForLineContent;
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
                auto flexItemOuterMainSize = outerMainSize(flexItems[flexItemIndex], nonFrozenSet.contains(flexItemIndex) ? flexBaseAndHypotheticalMainSizeList[flexItemIndex].flexBase : mainSizeList[flexItemIndex]);
                lineContentMainSize += flexItemOuterMainSize;
            }
            return availableMainSpaceForLineContent - lineContentMainSize;
        };
        auto initialFreeSpace = computedFreeSpace();

        auto minimumViolationList = Vector<size_t> { };
        auto maximumViolationList = Vector<size_t> { };
        minimumViolationList.reserveInitialCapacity(flexItems.size());
        maximumViolationList.reserveInitialCapacity(flexItems.size());
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
                    totalFlexFactor += shouldUseFlexGrowFactor ? flexItems[nonFrozenIndex].growFactor() : flexItems[nonFrozenIndex].shrinkFactor();
                if (totalFlexFactor < 1) {
                    auto freeSpaceCandidate = LayoutUnit { initialFreeSpace * totalFlexFactor };
                    if (freeSpaceCandidate.abs() < freeSpace.abs())
                        freeSpace = freeSpaceCandidate;
                }
            };
            adjustFreeSpaceWithFlexFactors();

            // c. Distribute free space proportional to the flex factors.
            auto usedTotalFactor = 0.f;
            for (auto nonFrozenIndex : nonFrozenSet)
                usedTotalFactor += shouldUseFlexGrowFactor ? flexItems[nonFrozenIndex].growFactor() : flexItems[nonFrozenIndex].shrinkFactor() * flexBaseAndHypotheticalMainSizeList[nonFrozenIndex].flexBase;

            for (auto nonFrozenIndex : nonFrozenSet) {
                if (!usedTotalFactor || std::isinf(usedTotalFactor)) {
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
            minimumViolationList.shrink(0);
            maximumViolationList.shrink(0);
            for (auto nonFrozenIndex : nonFrozenSet) {
                auto unclampedMainSize = mainSizeList[nonFrozenIndex];
                auto& flexItem = flexItems[nonFrozenIndex];
                auto clampedMainSize = std::max(formattingUtils().usedMinimumSizeInMainAxis(flexItem), unclampedMainSize);
                if (auto usedMaximumMainSize = formattingUtils().usedMaximumSizeInMainAxis(flexItem))
                    clampedMainSize = std::min(*usedMaximumMainSize, clampedMainSize);
                // FIXME: ...and floor its content-box size at zero
                totalViolation += (clampedMainSize - unclampedMainSize);
                if (clampedMainSize < unclampedMainSize)
                    maximumViolationList.append(nonFrozenIndex);
                else if (clampedMainSize > unclampedMainSize)
                    minimumViolationList.append(nonFrozenIndex);
                mainSizeList[nonFrozenIndex] = clampedMainSize;
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

FlexLayout::SizeList FlexLayout::hypotheticalCrossSizeForFlexItems(const LogicalFlexItems& flexItems, const SizeList& flexItemsMainSizeList)
{
    SizeList hypotheticalCrossSizeList(flexItems.size());
    for (size_t flexItemIndex = 0; flexItemIndex < flexItems.size(); ++flexItemIndex)
        hypotheticalCrossSizeList[flexItemIndex] = formattingUtils().usedSizeInCrossAxis(flexItems[flexItemIndex], flexItemsMainSizeList[flexItemIndex]);
    return hypotheticalCrossSizeList;
}

FlexLayout::LinesCrossSizeList FlexLayout::crossSizeForFlexLines(const LineRanges& lineRanges, const ConstraintsForFlexContent::AxisGeometry& crossAxis, const LogicalFlexItems& flexItems, const SizeList& flexItemsHypotheticalCrossSizeList) const
{
    LinesCrossSizeList flexLinesCrossSizeList(lineRanges.size());
    // If the flex container is single-line and has a definite cross size, the cross size of the flex line is the flex container's inner cross size.
    if (isSingleLineFlexContainer() && crossAxis.availableSize) {
        ASSERT(flexLinesCrossSizeList.size() == 1);
        flexLinesCrossSizeList[0] = *crossAxis.availableSize;
        return flexLinesCrossSizeList;
    }

    for (size_t lineIndex = 0; lineIndex < lineRanges.size(); ++lineIndex) {
        auto maximumAscent = LayoutUnit { };
        auto maximumDescent = LayoutUnit { };
        auto maximumHypotheticalOuterCrossSize = LayoutUnit { };
        for (size_t flexItemIndex = lineRanges[lineIndex].begin(); flexItemIndex < lineRanges[lineIndex].end(); ++flexItemIndex) {
            // Collect all the flex items whose inline-axis is parallel to the main-axis, whose align-self is baseline, and whose cross-axis margins are both non-auto.
            auto& flexItem = flexItems[flexItemIndex];
            if (!flexItem.isOrhogonal() && flexItem.style().alignSelf().position() == ItemPosition::Baseline && flexItem.crossAxis().hasNonAutoMargins()) {
                // Find the largest of the distances between each item's baseline and its hypothetical outer cross-start edge,
                // and the largest of the distances between each item's baseline and its hypothetical outer cross-end edge, and sum these two values.
                maximumAscent = std::max(maximumAscent, flexItem.crossAxis().ascent);
                maximumDescent = std::max(maximumDescent, flexItem.crossAxis().descent);
                continue;
            }
            // Among all the items not collected by the previous step, find the largest outer hypothetical cross size.
            auto flexItemOuterCrossSize = outerCrossSize(flexItem, flexItemsHypotheticalCrossSizeList[flexItemIndex]);
            maximumHypotheticalOuterCrossSize = std::max(maximumHypotheticalOuterCrossSize, flexItemOuterCrossSize);
        }
        // The used cross-size of the flex line is the largest of the numbers found in the previous two steps and zero.
        // If the flex container is single-line, then clamp the line's cross-size to be within the container's computed min and max cross sizes.
        flexLinesCrossSizeList[lineIndex] = std::max(maximumHypotheticalOuterCrossSize, maximumAscent + maximumDescent);
        if (isSingleLineFlexContainer()) {
            auto minimumCrossSize = crossAxis.minimumSize.value_or(flexLinesCrossSizeList[lineIndex]);
            auto maximumCrossSize = crossAxis.maximumSize.value_or(LayoutUnit::max());
            flexLinesCrossSizeList[lineIndex] = std::min(maximumCrossSize, std::max(minimumCrossSize, flexLinesCrossSizeList[lineIndex]));
        }
    }
    return flexLinesCrossSizeList;
}

void FlexLayout::stretchFlexLines(LinesCrossSizeList& flexLinesCrossSizeList, size_t numberOfLines, std::optional<LayoutUnit> crossAxisAvailableSpace) const
{
    // Handle 'align-content: stretch'.
    // If the flex container has a definite cross size, align-content is stretch, and the sum of the flex lines' cross sizes is less than the flex container's inner cross size,
    // increase the cross size of each flex line by equal amounts such that the sum of their cross sizes exactly equals the flex container's inner cross size.
    auto linesMayStretch = [&] {
        auto alignContent = flexContainerStyle().alignContent();
        if (alignContent.distribution() == ContentDistribution::Stretch)
            return true;
        return alignContent.distribution() == ContentDistribution::Default && alignContent.position() == ContentPosition::Normal;
    };
    if (!linesMayStretch() || !crossAxisAvailableSpace)
        return;

    auto linesCrossSize = [&] {
        auto size = LayoutUnit { };
        for (size_t lineIndex = 0; lineIndex < flexLinesCrossSizeList.size(); ++lineIndex)
            size += flexLinesCrossSizeList[lineIndex];
        return size;
    }();
    auto flexContainerUsedCrossSize = crossAxisAvailableSpaceForLineSizingAndAlignment(*crossAxisAvailableSpace, numberOfLines);
    if (flexContainerUsedCrossSize <= linesCrossSize)
        return;

    auto extraSpace = (flexContainerUsedCrossSize - linesCrossSize) / numberOfLines;
    for (size_t lineIndex = 0; lineIndex < flexLinesCrossSizeList.size(); ++lineIndex)
        flexLinesCrossSizeList[lineIndex] += extraSpace;
}

bool FlexLayout::collapseNonVisibleFlexItems()
{
    // Collapse visibility:collapse items. If any flex items have visibility: collapse,
    // note the cross size of the line they're in as the item's strut size, and restart layout from the beginning.
    // FIXME: Not supported yet.
    return false;
}

FlexLayout::SizeList FlexLayout::computeCrossSizeForFlexItems(const LogicalFlexItems& flexItems, const LineRanges& lineRanges, const LinesCrossSizeList& flexLinesCrossSizeList, const SizeList& flexItemsHypotheticalCrossSizeList) const
{
    SizeList crossSizeList(flexItems.size());
    for (size_t lineIndex = 0; lineIndex < lineRanges.size(); ++lineIndex) {
        for (auto flexItemIndex = lineRanges[lineIndex].begin(); flexItemIndex < lineRanges[lineIndex].end(); ++flexItemIndex) {
            auto& flexItem = flexItems[flexItemIndex];
            auto& crossAxis = flexItem.crossAxis();
            auto& flexItemAlignSelf = flexItem.style().alignSelf();
            auto alignValue = flexItemAlignSelf.position() != ItemPosition::Auto ? flexItemAlignSelf.position() : flexContainerStyle().alignItems().position();
            // If a flex item has align-self: stretch, its computed cross size property is auto, and neither of its cross-axis margins are auto, the used outer cross size is the used cross size of its flex line,
            // clamped according to the item's used min and max cross sizes. Otherwise, the used cross size is the item's hypothetical cross size.
            if ((alignValue == ItemPosition::Stretch || alignValue == ItemPosition::Normal) && crossAxis.hasSizeAuto && crossAxis.hasNonAutoMargins()) {
                auto stretchedInnerCrossSize = [&] {
                    auto stretchedInnerCrossSize = flexLinesCrossSizeList[lineIndex] - flexItems[flexItemIndex].crossAxis().margin();
                    if (flexItem.isContentBoxBased())
                        stretchedInnerCrossSize -= flexItem.crossAxis().borderAndPadding;
                    auto maximum = flexItem.crossAxis().maximumSize.value_or(stretchedInnerCrossSize);
                    auto minimum = flexItem.crossAxis().minimumSize.value_or(stretchedInnerCrossSize);
                    return std::min(maximum, std::max(minimum, stretchedInnerCrossSize));
                };
                crossSizeList[flexItemIndex] = stretchedInnerCrossSize();
                // FIXME: This requires re-layout to get percentage-sized descendants updated.
            } else
                crossSizeList[flexItemIndex] = flexItemsHypotheticalCrossSizeList[flexItemIndex];
        }
    }
    return crossSizeList;
}

FlexLayout::PositionAndMarginsList FlexLayout::handleMainAxisAlignment(LayoutUnit availableMainSpace, const LineRanges& lineRanges, const LogicalFlexItems& flexItems, const SizeList& flexItemsMainSizeList) const
{
    // Distribute any remaining free space. For each flex line:
    auto mainPositionAndMargins = PositionAndMarginsList { flexItems.size() };

    for (auto lineRange : lineRanges) {
        auto lineContentOuterMainSize = LayoutUnit { };
        auto availableMainSpaceForLineContent = mainAxisAvailableSpaceForItemAlignment(availableMainSpace, lineRange.distance());

        auto resolveMarginAuto = [&] {
            // 1. If the remaining free space is positive and at least one main-axis margin on this line is auto, distribute the free space equally among these margins.
            //    Otherwise, set all auto margins to zero.
            auto flexItemsWithMarginAuto = Vector<size_t> { };
            size_t autoMarginCount = 0;

            for (auto flexItemIndex = lineRange.begin(); flexItemIndex < lineRange.end(); ++flexItemIndex) {
                auto& flexItem = flexItems[flexItemIndex];
                auto marginStart = flexItem.mainAxis().marginStart;
                auto marginEnd = flexItem.mainAxis().marginEnd;

                if (!marginStart || !marginEnd) {
                    flexItemsWithMarginAuto.append(flexItemIndex);
                    if (!marginStart)
                        ++autoMarginCount;
                    if (!marginEnd)
                        ++autoMarginCount;
                }
                mainPositionAndMargins[flexItemIndex].marginStart = marginStart.value_or(0_lu);
                mainPositionAndMargins[flexItemIndex].marginEnd = marginEnd.value_or(0_lu);
                lineContentOuterMainSize += outerMainSize(flexItem, flexItemsMainSizeList[flexItemIndex], mainPositionAndMargins[flexItemIndex].margin());
            }

            auto spaceToDistrubute = availableMainSpaceForLineContent - lineContentOuterMainSize;
            if (!autoMarginCount || spaceToDistrubute <= 0)
                return;

            lineContentOuterMainSize = availableMainSpaceForLineContent;
            auto extraMarginSpace = spaceToDistrubute / autoMarginCount;

            for (auto flexItemIndex : flexItemsWithMarginAuto) {
                auto& flexItem = flexItems[flexItemIndex];

                if (!flexItem.mainAxis().marginStart)
                    mainPositionAndMargins[flexItemIndex].marginStart = extraMarginSpace;
                if (!flexItem.mainAxis().marginEnd)
                    mainPositionAndMargins[flexItemIndex].marginEnd = extraMarginSpace;
            }
        };
        resolveMarginAuto();

        auto& justifyContentValue = flexContainerStyle().justifyContent();
        auto justifyContentDistribution = justifyContentValue.distribution();
        auto justifyContentPosition = justifyContentValue.position();

        auto setFallbackValuesIfApplicable = [&] {
            auto itemCount = lineRange.distance();
            auto hasOverflow = lineContentOuterMainSize > availableMainSpaceForLineContent;
            if (!hasOverflow && itemCount > 1)
                return;

            switch (justifyContentDistribution) {
            case ContentDistribution::SpaceBetween:
                justifyContentPosition = hasOverflow ? ContentPosition::Start : ContentPosition::FlexStart;
                break;
            case ContentDistribution::SpaceEvenly:
            case ContentDistribution::SpaceAround:
                justifyContentPosition = hasOverflow ? ContentPosition::Start : ContentPosition::Center;
                break;
            default:
                break;
            }
            justifyContentPosition = justifyContentValue.overflow() == OverflowAlignment::Safe && hasOverflow ? ContentPosition::Start : justifyContentPosition;
            justifyContentDistribution = ContentDistribution::Default;
        };
        setFallbackValuesIfApplicable();

        auto resolveNonLogicalValues = [&] {
            if (justifyContentPosition != ContentPosition::Right && justifyContentPosition != ContentPosition::Left)
                return;
            if (!FlexFormattingUtils::isMainAxisParallelWithLeftRightAxis(flexContainer())) {
                // If the property's axis is not parallel with either left<->right axis, this value behaves as start (https://drafts.csswg.org/css-align/#positional-values)
                justifyContentPosition = ContentPosition::Start;
                return;
            }
            auto leftRightNeedsFlipping = FlexFormattingUtils::isInlineDirectionRTL(flexContainer());
            justifyContentPosition = justifyContentPosition == ContentPosition::Left ? (!leftRightNeedsFlipping ? ContentPosition::Start : ContentPosition::End) : (!leftRightNeedsFlipping ? ContentPosition::End : ContentPosition::Start);
        };
        resolveNonLogicalValues();

        auto justifyContent = [&] {
            // 2. Align the items along the main-axis per justify-content.
            auto initialOffset = [&] {
                // ContentDistribution::Default handles fallback to justifyContentPosition
                if (justifyContentDistribution != ContentDistribution::Default) {
                    switch (justifyContentDistribution) {
                    case ContentDistribution::SpaceBetween:
                        return LayoutUnit { };
                    case ContentDistribution::SpaceAround:
                        return (availableMainSpaceForLineContent - lineContentOuterMainSize) / lineRange.distance() / 2;
                    case ContentDistribution::SpaceEvenly:
                        return (availableMainSpaceForLineContent - lineContentOuterMainSize) / (lineRange.distance() + 1);
                    default:
                        ASSERT_NOT_IMPLEMENTED_YET();
                        break;
                    }
                }

                switch (justifyContentPosition) {
                case ContentPosition::Normal:
                case ContentPosition::FlexStart:
                    return LayoutUnit { };
                case ContentPosition::FlexEnd:
                    return availableMainSpaceForLineContent - lineContentOuterMainSize;
                case ContentPosition::Center:
                    return availableMainSpaceForLineContent / 2 - lineContentOuterMainSize / 2;
                case ContentPosition::Start:
                    if (FlexFormattingUtils::isMainReversedToContentDirection(flexContainer()))
                        return availableMainSpaceForLineContent - lineContentOuterMainSize;
                    return LayoutUnit { };
                case ContentPosition::End:
                    if (FlexFormattingUtils::isMainReversedToContentDirection(flexContainer()))
                        return LayoutUnit { };
                    return availableMainSpaceForLineContent - lineContentOuterMainSize;
                default:
                    ASSERT_NOT_IMPLEMENTED_YET();
                    break;
                }
                ASSERT_NOT_REACHED();
                return LayoutUnit { };
            };

            auto gapBetweenItems = [&] {
                switch (justifyContentDistribution) {
                case ContentDistribution::Default:
                    return LayoutUnit { };
                case ContentDistribution::SpaceBetween:
                    return std::max(0_lu, availableMainSpaceForLineContent - lineContentOuterMainSize) / (lineRange.distance() - 1);
                case ContentDistribution::SpaceAround:
                    return std::max(0_lu, availableMainSpaceForLineContent - lineContentOuterMainSize) / lineRange.distance();
                case ContentDistribution::SpaceEvenly:
                    return std::max(0_lu, availableMainSpaceForLineContent - lineContentOuterMainSize) / (lineRange.distance() + 1);
                default:
                    ASSERT_NOT_IMPLEMENTED_YET();
                    break;
                }
                ASSERT_NOT_REACHED();
                return LayoutUnit { };
            };

            auto flexItemOuterEnd = [&](auto flexItemIndex) {
                auto flexIteOuterMainSize = outerMainSize(flexItems[flexItemIndex], flexItemsMainSizeList[flexItemIndex], mainPositionAndMargins[flexItemIndex].margin());
                // Note that position here means border box position.
                auto flexItemEnd = flexIteOuterMainSize - mainPositionAndMargins[flexItemIndex].marginStart;
                return mainPositionAndMargins[flexItemIndex].position + flexItemEnd;
            };

            auto startIndex = lineRange.begin();
            mainPositionAndMargins[startIndex].position = initialOffset() + mainPositionAndMargins[startIndex].marginStart;
            auto previousFlexItemOuterEnd = flexItemOuterEnd(startIndex);
            auto gap = gapBetweenItems();
            for (auto index = startIndex + 1; index < lineRange.end(); ++index) {
                mainPositionAndMargins[index].position = previousFlexItemOuterEnd + gap + mainPositionAndMargins[index].marginStart;
                previousFlexItemOuterEnd = flexItemOuterEnd(index);
            }
        };
        justifyContent();
    }
    return mainPositionAndMargins;
}

FlexLayout::PositionAndMarginsList FlexLayout::handleCrossAxisAlignmentForFlexItems(const LogicalFlexItems& flexItems, const LineRanges& lineRanges, const SizeList& flexItemsCrossSizeList, const LinesCrossSizeList& flexLinesCrossSizeList) const
{
    auto crossPositionAndMargins = PositionAndMarginsList { flexItems.size() };

    for (size_t lineIndex = 0; lineIndex < lineRanges.size(); ++lineIndex) {
        auto lineRange = lineRanges[lineIndex];

        auto resolveMarginAuto = [&] {
            for (auto flexItemIndex = lineRange.begin(); flexItemIndex < lineRange.end(); ++flexItemIndex) {
                auto& flexItem = flexItems[flexItemIndex];
                auto marginStart = flexItem.crossAxis().marginStart;
                auto marginEnd = flexItem.crossAxis().marginEnd;

                // Resolve cross-axis auto margins. If a flex item has auto cross-axis margins:
                if (!marginStart || !marginEnd) {
                    auto flexItemOuterCrossSize = outerCrossSize(flexItem, flexItemsCrossSizeList[flexItemIndex]);
                    auto extraCrossSpace = flexLinesCrossSizeList[lineIndex] - flexItemOuterCrossSize;
                    // If its outer cross size (treating those auto margins as zero) is less than the cross size of its flex line, distribute
                    // the difference in those sizes equally to the auto margins.
                    // Otherwise, if the block-start or inline-start margin (whichever is in the cross axis) is auto, set it to zero.
                    // Set the opposite margin so that the outer cross size of the item equals the cross size of its flex line.
                    if (extraCrossSpace > 0) {
                        if (!marginStart && !marginEnd) {
                            marginStart = extraCrossSpace / 2;
                            marginEnd = extraCrossSpace / 2;
                        } else if (!marginStart)
                            marginStart = extraCrossSpace;
                        else
                            marginEnd = extraCrossSpace;
                    } else {
                        auto marginCrossSpace = flexLinesCrossSizeList[lineIndex] - flexItemOuterCrossSize;
                        auto setMargins = [&](auto startValue, auto endValue) {
                            marginStart = startValue;
                            marginEnd = endValue;
                        };
                        !marginStart ? setMargins(0_lu, marginCrossSpace) : setMargins(marginCrossSpace, 0_lu);
                    }
                }
                crossPositionAndMargins[flexItemIndex].marginStart = *marginStart;
                crossPositionAndMargins[flexItemIndex].marginEnd = *marginEnd;
            }
        };
        resolveMarginAuto();

        auto alignSelf = [&] {
            // Align all flex items along the cross-axis per align-self, if neither of the item's cross-axis margins are auto.
            for (auto flexItemIndex = lineRange.begin(); flexItemIndex < lineRange.end(); ++flexItemIndex) {
                auto& flexItem = flexItems[flexItemIndex];
                auto flexItemOuterCrossSize = outerCrossSize(flexItem, flexItemsCrossSizeList[flexItemIndex], crossPositionAndMargins[flexItemIndex].margin());
                auto flexItemOuterCrossPosition = LayoutUnit { };

                auto& flexItemAlign = flexItem.style().alignSelf().position() != ItemPosition::Auto ? flexItem.style().alignSelf() : flexContainerStyle().alignItems();
                auto alignSelfPosition = flexItemAlign.position();
                auto setFallbackValuesIfApplicable = [&] {
                    if (flexItemOuterCrossSize > flexLinesCrossSizeList[lineIndex] && flexItemAlign.overflow() == OverflowAlignment::Safe)
                        alignSelfPosition = ItemPosition::FlexStart;
                };
                setFallbackValuesIfApplicable();

                switch (alignSelfPosition) {
                case ItemPosition::Stretch:
                case ItemPosition::Normal:
                    // This is taken care of at 9.4.11 see computeCrossSizeForFlexItems.
                    flexItemOuterCrossPosition = { };
                    break;
                case ItemPosition::Center:
                    flexItemOuterCrossPosition = flexLinesCrossSizeList[lineIndex] / 2 - flexItemOuterCrossSize  / 2;
                    break;
                case ItemPosition::Start:
                case ItemPosition::SelfStart:
                case ItemPosition::FlexStart:
                    flexItemOuterCrossPosition = { };
                    break;
                case ItemPosition::End:
                case ItemPosition::SelfEnd:
                case ItemPosition::FlexEnd:
                    flexItemOuterCrossPosition = flexLinesCrossSizeList[lineIndex] - flexItemOuterCrossSize;
                    break;
                default:
                    ASSERT_NOT_IMPLEMENTED_YET();
                    break;
                }
                crossPositionAndMargins[flexItemIndex].position = flexItemOuterCrossPosition + crossPositionAndMargins[flexItemIndex].marginStart;
            }
        };
        alignSelf();
    }
    return crossPositionAndMargins;
}

FlexLayout::LinesCrossPositionList FlexLayout::handleCrossAxisAlignmentForFlexLines(std::optional<LayoutUnit> crossAxisAvailableSpace, const LineRanges& lineRanges, LinesCrossSizeList& flexLinesCrossSizeList) const
{
    // If the cross size property is a definite size, use that, clamped by the used min and max cross sizes of the flex container.
    // Otherwise, use the sum of the flex lines' cross sizes, clamped by the used min and max cross sizes of the flex container.
    auto flexLinesCrossSize = [&] {
        auto linesCrossSize = LayoutUnit { };
        for (auto crossSize : flexLinesCrossSizeList)
            linesCrossSize += crossSize;
        return linesCrossSize;
    }();
    auto isSingleLineFlexContainer = this->isSingleLineFlexContainer() || lineRanges.size() == 1;
    auto flexContainerUsedCrossSize = crossAxisAvailableSpaceForLineSizingAndAlignment(crossAxisAvailableSpace.value_or(0_lu), lineRanges.size());

    // Align all flex lines per align-content.
    auto distributableCrossSpace = flexContainerUsedCrossSize - flexLinesCrossSize;
    auto initialOffset = [&]() -> LayoutUnit {
        auto& alignContentValue = flexContainerStyle().alignContent();
        auto alignContentPosition = alignContentValue.position();
        auto alignContentDistribution = alignContentValue.distribution();

        auto setFallbackValuesIfApplicable = [&] {
            auto hasOverflow = distributableCrossSpace < 0;
            if (!hasOverflow && !isSingleLineFlexContainer)
                return;
            switch (alignContentDistribution) {
            case ContentDistribution::SpaceBetween:
                alignContentPosition = hasOverflow ? ContentPosition::Start : ContentPosition::FlexStart;
                break;
            case ContentDistribution::SpaceEvenly:
            case ContentDistribution::SpaceAround:
                alignContentPosition = hasOverflow ? ContentPosition::Start : ContentPosition::Center;
                break;
            case ContentDistribution::Stretch:
                alignContentPosition = ContentPosition::FlexStart;
                break;
            default:
                break;
            }
            alignContentPosition = alignContentValue.overflow() == OverflowAlignment::Safe && hasOverflow ? ContentPosition::Start : alignContentPosition;
            alignContentDistribution = ContentDistribution::Default;
        };
        setFallbackValuesIfApplicable();

        auto flipStartEndContentPositionIfApplicable = [&] {
            auto isWrapReversed = FlexFormattingUtils::areFlexLinesReversedInCrossAxis(flexContainer());
            if (alignContentPosition == ContentPosition::Start)
                alignContentPosition = isWrapReversed ? ContentPosition::FlexEnd : ContentPosition::FlexStart;
            else if (alignContentPosition == ContentPosition::End)
                alignContentPosition = isWrapReversed ? ContentPosition::FlexStart : ContentPosition::FlexEnd;
        };
        flipStartEndContentPositionIfApplicable();

        switch (alignContentPosition) {
        case ContentPosition::FlexStart:
            return { };
        case ContentPosition::Center:
            return flexContainerUsedCrossSize / 2 - flexLinesCrossSize / 2;
        case ContentPosition::FlexEnd:
            return flexContainerUsedCrossSize - flexLinesCrossSize;
        default:
            switch (alignContentDistribution) {
            case ContentDistribution::Default:
                return { };
            case ContentDistribution::SpaceBetween:
            case ContentDistribution::Stretch:
                return { };
            case ContentDistribution::SpaceAround:
                return distributableCrossSpace / lineRanges.size() / 2;
            case ContentDistribution::SpaceEvenly:
                return distributableCrossSpace / (lineRanges.size() + 1);
            default:
                ASSERT_NOT_REACHED();
                return { };
            }
        }
    };

    LinesCrossPositionList linesCrossPositionList(lineRanges.size());
    linesCrossPositionList[0] = initialOffset();
    if (isSingleLineFlexContainer)
        return linesCrossPositionList;

    auto gap = [&]() -> LayoutUnit {
        if (distributableCrossSpace <= 0)
            return { };
        switch (flexContainerStyle().alignContent().distribution()) {
        case ContentDistribution::SpaceBetween:
            return distributableCrossSpace / (lineRanges.size() - 1);
        case ContentDistribution::SpaceAround:
            return distributableCrossSpace / lineRanges.size();
        case ContentDistribution::Stretch: {
            // Lines stretch to take up the remaining space. If the leftover free-space is negative,
            // this value is identical to flex-start. Otherwise, the free-space is split equally between all of the lines,
            // increasing their cross size.
            auto extraCrossSpaceForEachLine = distributableCrossSpace / flexLinesCrossSizeList.size();
            for (size_t lineIndex = 0; lineIndex < flexLinesCrossSizeList.size(); ++lineIndex)
                flexLinesCrossSizeList[lineIndex] += extraCrossSpaceForEachLine;
            return { };
        }
        case ContentDistribution::SpaceEvenly:
            return distributableCrossSpace / (lineRanges.size() + 1);
        default:
            return { };
        }
    }();
    for (size_t lineIndex = 1; lineIndex < lineRanges.size(); ++lineIndex)
        linesCrossPositionList[lineIndex] = (linesCrossPositionList[lineIndex - 1] + flexLinesCrossSizeList[lineIndex - 1]) + gap;
    return linesCrossPositionList;
}

LayoutUnit FlexLayout::mainAxisAvailableSpaceForItemAlignment(LayoutUnit mainAxisAvailableSpace, size_t numberOfFlexItems) const
{
    if (numberOfFlexItems == 1)
        return mainAxisAvailableSpace;
    return mainAxisAvailableSpace - (FlexFormattingUtils::mainAxisGapValue(flexContainer(), mainAxisAvailableSpace) * (numberOfFlexItems - 1));
}

LayoutUnit FlexLayout::crossAxisAvailableSpaceForLineSizingAndAlignment(LayoutUnit crossAxisAvailableSpace, size_t numberOfFlexLines) const
{
    if (numberOfFlexLines == 1)
        return crossAxisAvailableSpace;
    return crossAxisAvailableSpace - (FlexFormattingUtils::crossAxisGapValue(flexContainer(), crossAxisAvailableSpace) * (numberOfFlexLines - 1));
}

const ElementBox& FlexLayout::flexContainer() const
{
    return m_flexFormattingContext.root();
}

const FlexFormattingContext& FlexLayout::formattingContext() const
{
    return m_flexFormattingContext;
}

const FlexFormattingUtils& FlexLayout::formattingUtils() const
{
    return formattingContext().formattingUtils();
}

}
}
