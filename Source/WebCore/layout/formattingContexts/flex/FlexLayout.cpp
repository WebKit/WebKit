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
#include "FlexFormattingGeometry.h"
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

FlexLayout::FlexLayout(const FlexFormattingContext& flexFormattingContext)
    : m_flexFormattingContext(flexFormattingContext)
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
                auto hypotheticalCrossSizeList = hypotheticalCrossSizeForFlexItems(flexItems, flexItemsMainSizeList);
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
    auto computedFinalSize = [&](auto& candidateSizes) {
        // For each dimension, if that dimension of the flex container's content box is a definite size, use that;
        // if that dimension of the flex container is being sized under a min or max-content constraint, the available space in that dimension is that constraint;
        // otherwise, subtract the flex container's margin, border, and padding from the space available to the flex container in that dimension and use that value.
        if (candidateSizes.definiteSize)
            return *candidateSizes.definiteSize;
        if (candidateSizes.minimumContentSize)
            return *candidateSizes.minimumContentSize;
        if (candidateSizes.maximumContentSize)
            return *candidateSizes.maximumContentSize;
        return candidateSizes.availableSize;
    };
    m_availableMainSpace = computedFinalSize(logicalConstraints.mainAxis);
    m_availableCrossSpace = computedFinalSize(logicalConstraints.crossAxis);
}

LayoutUnit FlexLayout::maxContentForFlexItem(const LogicalFlexItem& flexItem) const
{
    // 9.2.3 E Otherwise, size the item into the available space using its used flex basis in place of its main size,
    // treating a value of content as max-content. If a cross size is needed to determine the main size (e.g. when the flex item’s main size
    // is in its block axis) and the flex item’s cross size is auto and not definite, in this calculation use fit-content as the flex item’s cross size.
    // The flex base size is the item’s resulting main size.
    auto& flexItemBox = flexItem.layoutBox();
    if (!flexItemBox.establishesInlineFormattingContext()) {
        ASSERT_NOT_IMPLEMENTED_YET();
        return { };
    }

    if (flexItem.isOrhogonal() && !flexItem.crossAxis().definiteSize) {
        ASSERT_NOT_IMPLEMENTED_YET();
        return { };
    }
    auto& inlineFormattingState = flexFormattingContext().layoutState().ensureInlineFormattingState(flexItemBox);
    return InlineFormattingContext { flexItemBox, inlineFormattingState, { } }.maximumContentSize();
}

FlexLayout::FlexBaseAndHypotheticalMainSizeList FlexLayout::flexBaseAndHypotheticalMainSizeForFlexItems(const LogicalConstraints::AxisGeometry& mainAxis, const LogicalFlexItems& flexItems) const
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
            auto flexContainerHasMinMaxConstraints = mainAxis.minimumContentSize || mainAxis.maximumContentSize;
            if (flexBasisContentOrAvailableSpaceDependent && flexContainerHasMinMaxConstraints) {
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
            // E. Otherwise, size the item into the available space using its used flex basis in place of its main size, treating a value of content as max-content.
            auto usedMainSize = maxContentForFlexItem(flexItem);
            if (!flexItem.isContentBoxBased())
                usedMainSize += flexItem.mainAxis().borderAndPadding;
            return usedMainSize;
        };
        auto flexBaseSize = computedFlexBase();
        // The hypothetical main size is the item's flex base size clamped according to its used min and max main sizes (and flooring the content box size at zero).
        auto hypotheticalMainSize = std::min(flexItem.mainAxis().maximumUsedSize, std::max(flexItem.mainAxis().minimumUsedSize, flexBaseSize));
        flexBaseAndHypotheticalMainSizeList.append({ flexBaseSize, hypotheticalMainSize });
    }
    return flexBaseAndHypotheticalMainSizeList;
}

LayoutUnit FlexLayout::flexContainerMainSize(const LogicalConstraints::AxisGeometry& mainAxis) const
{
    // 4. Determine the main size of the flex container using the rules of the formatting context in which it participates.
    //    For this computation, auto margins on flex items are treated as 0.
    // FIXME: above.
    UNUSED_PARAM(mainAxis);
    return m_availableMainSpace;
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

FlexLayout::LineRanges FlexLayout::computeFlexLines(const LogicalFlexItems& flexItems, LayoutUnit flexContainerMainSize, const FlexBaseAndHypotheticalMainSizeList& flexBaseAndHypotheticalMainSizeList) const
{
    // Collect flex items into flex lines:
    // If the flex container is single-line, collect all the flex items into a single flex line.
    // Otherwise, starting from the first uncollected item, collect consecutive items one by one until the first time that the next collected
    // item would not fit into the flex container's inner main size.
    // If the very first uncollected item wouldn't fit, collect just it into the line.
    // For this step, the size of a flex item is its outer hypothetical main size. (Note: This can be negative.)
    if (isSingleLineFlexContainer())
        return { { 0, flexBaseAndHypotheticalMainSizeList.size() } };

    auto lineRanges = LineRanges { };
    size_t lastWrapIndex = 0;
    auto flexItemsMainSize = LayoutUnit { };
    for (size_t flexItemIndex = 0; flexItemIndex < flexBaseAndHypotheticalMainSizeList.size(); ++flexItemIndex) {
        auto flexItemHypotheticalOuterMainSize = outerMainSize(flexItems[flexItemIndex], flexBaseAndHypotheticalMainSizeList[flexItemIndex].hypotheticalMainSize);
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
            for (auto flexItemIndex = lineRange.begin(); flexItemIndex < lineRange.end(); ++flexItemIndex) {
                auto flexItemHypotheticalOuterMainSize = outerMainSize(flexItems[flexItemIndex], flexBaseAndHypotheticalMainSizeList[flexItemIndex].hypotheticalMainSize);
                hypotheticalOuterMainSizes += flexItemHypotheticalOuterMainSize;
            }
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
                auto flexItemOuterMainSize = outerMainSize(flexItems[flexItemIndex], nonFrozenSet.contains(flexItemIndex) ? flexBaseAndHypotheticalMainSizeList[flexItemIndex].flexBase : mainSizeList[flexItemIndex]);
                lineContentMainSize += flexItemOuterMainSize;
            }
            return flexContainerMainSize - lineContentMainSize;
        };

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
                    totalFlexFactor += flexItems[nonFrozenIndex].growFactor() + flexItems[nonFrozenIndex].shrinkFactor();
                if (totalFlexFactor < 1)
                    freeSpace *= totalFlexFactor;
            };
            adjustFreeSpaceWithFlexFactors();

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
            minimumViolationList.resize(0);
            maximumViolationList.resize(0);
            for (auto nonFrozenIndex : nonFrozenSet) {
                auto unclampedMainSize = mainSizeList[nonFrozenIndex];
                auto& flexItem = flexItems[nonFrozenIndex];
                auto clampedMainSize = std::min(flexItem.mainAxis().maximumUsedSize, std::max(flexItem.mainAxis().minimumUsedSize, unclampedMainSize));
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

FlexLayout::SizeList FlexLayout::hypotheticalCrossSizeForFlexItems(const LogicalFlexItems& flexItems, const SizeList& flexItemsMainSizeList) const
{
    UNUSED_PARAM(flexItemsMainSizeList);
    // FIXME: This is where layout is called on flex items.
    SizeList hypotheticalCrossSizeList(flexItems.size());
    for (size_t flexItemIndex = 0; flexItemIndex < flexItems.size(); ++flexItemIndex) {
        auto& flexItem = flexItems[flexItemIndex];

        if (auto definiteSize = flexItems[flexItemIndex].crossAxis().definiteSize) {
            hypotheticalCrossSizeList[flexItemIndex] = *definiteSize;
            continue;
        }
        auto& flexItemBox = flexItem.layoutBox();
        auto crossSizeAfterPerformingLayout = [&]() -> LayoutUnit {
            if (!flexItemBox.establishesInlineFormattingContext()) {
                ASSERT_NOT_IMPLEMENTED_YET();
                return { };
            }
            // FIXME: Let it run through integration codepath.
            auto floatingState = FloatingState { flexItemBox };
            auto parentBlockLayoutState = BlockLayoutState { floatingState };
            auto inlineLayoutState = InlineLayoutState { parentBlockLayoutState, { } };
            auto& inlineFormattingState = flexFormattingContext().layoutState().ensureInlineFormattingState(flexItemBox);
            auto inlineFormattingContext = InlineFormattingContext { flexItemBox, inlineFormattingState, { } };
            auto constraintsForInFlowContent = ConstraintsForInFlowContent { HorizontalConstraints { { }, flexItemsMainSizeList[flexItemIndex] }, { } };
            auto layoutResult = inlineFormattingContext.layoutInFlowAndFloatContent({ constraintsForInFlowContent, { } }, inlineLayoutState);
            return LayoutUnit { layoutResult.displayContent.lines.last().lineBoxLogicalRect().maxY() };
        };
        auto usedCrossSize = crossSizeAfterPerformingLayout();
        if (!flexItem.isContentBoxBased())
            usedCrossSize += flexItem.crossAxis().borderAndPadding;
        hypotheticalCrossSizeList[flexItemIndex] = usedCrossSize;
    }
    return hypotheticalCrossSizeList;
}

FlexLayout::LinesCrossSizeList FlexLayout::crossSizeForFlexLines(const LineRanges& lineRanges, const LogicalConstraints::AxisGeometry& crossAxis, const LogicalFlexItems& flexItems, const SizeList& flexItemsHypotheticalCrossSizeList) const
{
    LinesCrossSizeList flexLinesCrossSizeList(lineRanges.size());
    // If the flex container is single-line and has a definite cross size, the cross size of the flex line is the flex container's inner cross size.
    if (isSingleLineFlexContainer() && crossAxis.definiteSize) {
        ASSERT(flexLinesCrossSizeList.size() == 1);
        flexLinesCrossSizeList[0] = *crossAxis.definiteSize;
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
            auto maximumCrossSize = crossAxis.maximumSize.value_or(flexLinesCrossSizeList[lineIndex]);
            flexLinesCrossSizeList[lineIndex] = std::min(maximumCrossSize, std::max(minimumCrossSize, flexLinesCrossSizeList[lineIndex]));
        }
    }
    return flexLinesCrossSizeList;
}

void FlexLayout::stretchFlexLines(LinesCrossSizeList& flexLinesCrossSizeList, size_t numberOfLines, const LogicalConstraints::AxisGeometry& crossAxis) const
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
    if (!linesMayStretch() || !crossAxis.definiteSize)
        return;

    auto linesCrossSize = [&] {
        auto size = LayoutUnit { };
        for (size_t lineIndex = 0; lineIndex < flexLinesCrossSizeList.size(); ++lineIndex)
            size += flexLinesCrossSizeList[lineIndex];
        return size;
    }();
    if (*crossAxis.definiteSize <= linesCrossSize)
        return;

    auto extraSpace = (*crossAxis.definiteSize - linesCrossSize) / numberOfLines;
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

            auto spaceToDistrubute = availableMainSpace - lineContentOuterMainSize;
            if (!autoMarginCount || spaceToDistrubute <= 0)
                return;

            lineContentOuterMainSize = availableMainSpace;
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

        auto justifyContent = [&] {
            // 2. Align the items along the main-axis per justify-content.
            auto justifyContentValue = flexContainerStyle().justifyContent();
            auto initialOffset = [&] {
                switch (justifyContentValue.distribution()) {
                case ContentDistribution::Default:
                    // Fall back to justifyContentValue.position()
                    break;
                case ContentDistribution::SpaceBetween:
                    return LayoutUnit { };
                case ContentDistribution::SpaceAround: {
                    auto itemCount = availableMainSpace > lineContentOuterMainSize ? lineRange.distance() : 1;
                    return (availableMainSpace - lineContentOuterMainSize) / itemCount / 2;
                }
                case ContentDistribution::SpaceEvenly: {
                    auto gapCount = availableMainSpace > lineContentOuterMainSize ? lineRange.distance() + 1 : 2;
                    return (availableMainSpace - lineContentOuterMainSize) / gapCount;
                }
                default:
                    ASSERT_NOT_IMPLEMENTED_YET();
                    break;
                }

                auto positionalAlignment = [&] {
                    auto positionalAlignmentValue = justifyContentValue.position();
                    if (!FlexFormattingGeometry::isMainAxisParallelWithInlineAxis(flexContainer()) && (positionalAlignmentValue == ContentPosition::Left || positionalAlignmentValue == ContentPosition::Right))
                        positionalAlignmentValue = ContentPosition::Start;
                    return positionalAlignmentValue;
                };

                switch (positionalAlignment()) {
                // logical alignments
                case ContentPosition::Normal:
                case ContentPosition::FlexStart:
                    return LayoutUnit { };
                case ContentPosition::FlexEnd:
                    return availableMainSpace - lineContentOuterMainSize;
                case ContentPosition::Center:
                    return availableMainSpace / 2 - lineContentOuterMainSize / 2;
                // non-logical alignments
                case ContentPosition::Left:
                case ContentPosition::Start:
                    if (FlexFormattingGeometry::isReversedToContentDirection(flexContainer()))
                        return availableMainSpace - lineContentOuterMainSize;
                    return LayoutUnit { };
                case ContentPosition::Right:
                case ContentPosition::End:
                    if (FlexFormattingGeometry::isReversedToContentDirection(flexContainer()))
                        return LayoutUnit { };
                    return availableMainSpace - lineContentOuterMainSize;
                default:
                    ASSERT_NOT_IMPLEMENTED_YET();
                    break;
                }
                ASSERT_NOT_REACHED();
                return LayoutUnit { };
            };

            auto gapBetweenItems = [&] {
                switch (justifyContentValue.distribution()) {
                case ContentDistribution::Default:
                    return LayoutUnit { };
                case ContentDistribution::SpaceBetween:
                    if (lineRange.distance() == 1)
                        return LayoutUnit { };
                    return std::max(0_lu, availableMainSpace - lineContentOuterMainSize) / (lineRange.distance() - 1);
                case ContentDistribution::SpaceAround:
                    return std::max(0_lu, availableMainSpace - lineContentOuterMainSize) / lineRange.distance();
                case ContentDistribution::SpaceEvenly:
                    return std::max(0_lu, availableMainSpace - lineContentOuterMainSize) / (lineRange.distance() + 1);
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
                        auto marginCrossSpace = flexLinesCrossSizeList[lineIndex] - flexItemsCrossSizeList[flexItemIndex];
                        auto setMargins = [&](auto startValue, auto endValue) {
                            marginStart = startValue;
                            marginEnd = endValue;
                        };
                        marginStart ? setMargins(marginCrossSpace, 0_lu) : setMargins(0_lu, marginCrossSpace);
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

                auto& flexItemAlignSelf = flexItem.style().alignSelf();
                auto alignValue = flexItemAlignSelf.position() != ItemPosition::Auto ? flexItemAlignSelf : flexContainerStyle().alignItems();
                switch (alignValue.position()) {
                case ItemPosition::Stretch:
                case ItemPosition::Normal:
                    // This is taken care of at 9.4.11 see computeCrossSizeForFlexItems.
                    flexItemOuterCrossPosition = { };
                    break;
                case ItemPosition::Center:
                    flexItemOuterCrossPosition = flexLinesCrossSizeList[lineIndex] / 2 - flexItemOuterCrossSize  / 2;
                    break;
                case ItemPosition::Start:
                case ItemPosition::FlexStart:
                    flexItemOuterCrossPosition = { };
                    break;
                case ItemPosition::End:
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

FlexLayout::LinesCrossPositionList FlexLayout::handleCrossAxisAlignmentForFlexLines(const LogicalConstraints::AxisGeometry& crossAxis, const LineRanges& lineRanges, LinesCrossSizeList& flexLinesCrossSizeList) const
{
    // If the cross size property is a definite size, use that, clamped by the used min and max cross sizes of the flex container.
    // Otherwise, use the sum of the flex lines' cross sizes, clamped by the used min and max cross sizes of the flex container.
    if (isSingleLineFlexContainer())
        return { { } };

    auto flexLinesCrossSize = [&] {
        auto linesCrossSize = LayoutUnit { };
        for (auto crossSize : flexLinesCrossSizeList)
            linesCrossSize += crossSize;
        return linesCrossSize;
    }();
    auto flexContainerUsedCrossSize = crossAxis.definiteSize.value_or(flexLinesCrossSize);
    // Align all flex lines per align-content.
    auto initialOffset = [&]() -> LayoutUnit {
        switch (flexContainerStyle().alignContent().position()) {
        case ContentPosition::Start:
        case ContentPosition::Normal:
            return { };
        case ContentPosition::Center:
            return flexContainerUsedCrossSize / 2 - flexLinesCrossSize / 2;
        case ContentPosition::End:
            return flexContainerUsedCrossSize - flexLinesCrossSize;
        default:
            switch (flexContainerStyle().alignContent().distribution()) {
            case ContentDistribution::SpaceBetween:
            case ContentDistribution::Stretch:
                return { };
            case ContentDistribution::SpaceAround: {
                auto extraCrossSpace = flexContainerUsedCrossSize - flexLinesCrossSize;
                if (extraCrossSpace <= 0)
                    return { };
                return extraCrossSpace / lineRanges.size() / 2;
            }
            default:
                ASSERT_NOT_REACHED();
                return { };
            }
        }
    };

    auto gap = [&]() -> LayoutUnit {
        auto extraCrossSpace = flexContainerUsedCrossSize - flexLinesCrossSize;
        if (extraCrossSpace <= 0)
            return { };
        switch (flexContainerStyle().alignContent().distribution()) {
        case ContentDistribution::SpaceBetween:
            return extraCrossSpace / (lineRanges.size() - 1);
        case ContentDistribution::SpaceAround:
            return extraCrossSpace / lineRanges.size();
        case ContentDistribution::Stretch:
        case ContentDistribution::Default: {
            // Lines stretch to take up the remaining space. If the leftover free-space is negative,
            // this value is identical to flex-start. Otherwise, the free-space is split equally between all of the lines,
            // increasing their cross size.
            auto extraCrossSpaceForEachLine = extraCrossSpace / flexLinesCrossSizeList.size();
            for (size_t lineIndex = 0; lineIndex < flexLinesCrossSizeList.size(); ++lineIndex)
                flexLinesCrossSizeList[lineIndex] += extraCrossSpaceForEachLine;
            return { };
        }
        default:
            return { };
        }
    }();

    LinesCrossPositionList linesCrossPositionList(lineRanges.size());
    linesCrossPositionList[0] = initialOffset();
    for (size_t lineIndex = 1; lineIndex < lineRanges.size(); ++lineIndex)
        linesCrossPositionList[lineIndex] = (linesCrossPositionList[lineIndex - 1] + flexLinesCrossSizeList[lineIndex - 1]) + gap;
    return linesCrossPositionList;
}

const ElementBox& FlexLayout::flexContainer() const
{
    return flexFormattingContext().root();
}

const FlexFormattingContext& FlexLayout::flexFormattingContext() const
{
    return m_flexFormattingContext;
}

}
}
