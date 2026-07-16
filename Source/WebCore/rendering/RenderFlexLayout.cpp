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
#include "RenderFlexLayout.h"

#include "FlexLayoutUtils.h"
#include "InspectorInstrumentation.h"
#include "RenderBoxInlines.h"
#include "RenderFlexibleBox.h"
#include "RenderLayer.h"
#include "RenderObjectInlines.h"
#include "RenderTable.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "WritingMode.h"

namespace WebCore {

FlexLayout::FlexLayout(RenderFlexibleBox& flexBox, const FlexLayoutConstraints& constraints)
    : m_flexBox(flexBox)
    , m_constraints(constraints)
{
}

const FlexLayoutUtils& FlexLayout::flexLayoutUtils() const
{
    return m_flexBox.flexLayoutUtils();
}

FlexLayout::Result FlexLayout::performFlexLayout(FlexLayoutItems& flexItems, RelayoutChildren relayoutChildren)
{
    // RenderFlexibleBox collects the items and handles the empty case, so there is always at least one item here.
    ASSERT(!flexItems.isEmpty());
    // 9.2. Determine the flex base size and hypothetical main size of each item RenderFlexibleBox collected.
    FlexBaseAndHypotheticalMainSizeList flexBaseAndHypotheticalMainSizeList;
    for (auto& flexItem : flexItems) {
        flexBaseAndHypotheticalMainSizeList.append(flexBaseAndHypotheticalMainSize(flexItem));
        // Snapshot the main-axis margin now that flexBaseAndHypotheticalMainSize has laid the item out: an orthogonal
        // flex item only resolves its physical margins during that layout, so reading them at FlexLayoutItem
        // construction time (before layout) would capture stale values and throw off margin-trim.
        flexItem.mainAxisMargin = flexLayoutUtils().isHorizontalFlow() ? flexItem.renderer->horizontalMarginExtent() : flexItem.renderer->verticalMarginExtent();
        // flexBaseAndHypotheticalMainSize may set the override containing block height, so any cached definiteness could be stale.
        m_flexBox.resetHasDefiniteHeight();
    }

    LayoutUnit gapBetweenItems = flexLayoutUtils().computeGap(FlexLayoutUtils::GapType::BetweenItems);
    LayoutUnit gapBetweenLines = flexLayoutUtils().computeGap(FlexLayoutUtils::GapType::BetweenLines);

    FlexLines flexLines;
    Vector<LayoutUnit> mainSizeList;
    Vector<LayoutUnit> lineCrossSizeList;
    Vector<LayoutUnit> lineCrossOffsetList;
    LayoutUnit crossAxisStartEdge;
    LayoutUnit flexContainerLogicalHeight;

    auto performContentSizing = [&] {
        InspectorInstrumentation::flexibleBoxRendererBeganLayout(m_flexBox);
        // 9.3. (#5) Collect the flex items into flex lines.
        flexLines = computeFlexLines(flexItems, flexBaseAndHypotheticalMainSizeList.span());
        // 9.3. (#6) Resolve the flexible lengths to find the used main size of each item.
        mainSizeList = computeMainSizeForFlexItems(flexItems, flexLines, flexBaseAndHypotheticalMainSizeList.span(), gapBetweenItems);
        trimCrossAxisMarginsForFlexItems(flexItems, flexLines);
        layoutFlexItems(flexItems.mutableSpan(), mainSizeList.span(), relayoutChildren);
        // 9.4. (#7) Determine the hypothetical cross size of each item.
        auto hypotheticalCrossSizeList = hypotheticalCrossSizeForFlexItems(flexItems);
        // 9.4. (#8) Calculate the cross size of each flex line.
        lineCrossSizeList = crossSizeForFlexLines(flexLines, flexItems, hypotheticalCrossSizeList);

        // Record each line's cross-axis offset, growing the container's cross size to fit the lines (row flow).
        // Column flow's logical height is its main size, set later while placing the items, so accumulate here for row.
        flexContainerLogicalHeight = m_flexBox.logicalHeight();
        lineCrossOffsetList = Vector<LayoutUnit>(flexLines.ranges.size());
        LayoutUnit crossAxisOffset = m_constraints.flowAwareBorderBlock.first + m_constraints.flowAwarePaddingBlock.first;
        for (size_t lineIndex = 0; lineIndex < flexLines.ranges.size(); ++lineIndex) {
            InspectorInstrumentation::flexibleBoxRendererWrappedToNextLine(m_flexBox, flexLines.ranges[lineIndex].end());
            if (!m_constraints.isColumnFlow)
                flexContainerLogicalHeight = std::max(flexContainerLogicalHeight, crossAxisOffset + m_constraints.flowAwareBorderBlock.second + m_constraints.flowAwarePaddingBlock.second + lineCrossSizeList[lineIndex] + flexLayoutUtils().crossAxisScrollbarExtent());
            lineCrossOffsetList[lineIndex] = crossAxisOffset;
            crossAxisOffset += lineCrossSizeList[lineIndex];
        }
    };
    performContentSizing();

    Vector<LayoutPoint> positionList;
    Vector<LayoutUnit> crossSizeList;
    auto performContentAlignment = [&] {
        // 9.5. (#12) Main-Axis Alignment.
        positionList = handleMainAxisAlignment(flexLines, flexItems, mainSizeList, lineCrossOffsetList, gapBetweenItems);
        setFlexItemCountsForFirstAndLastLine(flexLines);

        // 9.6. (#15) Determine the flex container's used cross size: hand the accumulated content extent to the
        // container so it resolves its own logical height (specified vs content, min/max, box-sizing).
        auto interLineGapTotal = !m_constraints.isColumnFlow && flexLines.ranges.size() > 1 ? gapBetweenLines * (flexLines.ranges.size() - 1) : 0_lu;
        m_flexBox.updateLogicalHeightForFlexContent(flexContainerLogicalHeight, m_constraints.minimumHeightForLineIfEmpty, interLineGapTotal);

        // Multi-line column flex only knows its main size now, so re-resolve the flexible lengths of any lines that were left short.
        distributeMainAxisFreeSpaceForMultilineColumnIfNeeded(flexLines, flexItems, flexBaseAndHypotheticalMainSizeList.span(), mainSizeList, positionList, lineCrossOffsetList, gapBetweenItems);

        // 9.6. (#13 - #16) Cross-Axis Alignment.
        crossAxisStartEdge = lineCrossOffsetList.isEmpty() ? 0_lu : lineCrossOffsetList[0];
        // If we have a single line flexbox, the line height is all the available space. For flex-direction: row,
        // this means we need to use the height, so we do this after calling updateLogicalHeight.
        if (!m_constraints.isMultiline && !lineCrossSizeList.isEmpty())
            lineCrossSizeList[0] = flexLayoutUtils().crossAxisContentExtent();

        // 9.4. (#9) Handle 'align-content: stretch' and 9.6. (#16) align all flex lines per align-content.
        handleCrossAxisAlignmentForFlexLines(flexLines, positionList, lineCrossOffsetList, lineCrossSizeList, gapBetweenLines);

        // 9.4. (#11) Determine the used cross size of each flex item.
        crossSizeList = computeCrossSizeForFlexItems(flexLines, flexItems, lineCrossSizeList);

        // 9.6. (#13 - #14) Resolve cross-axis auto margins and align each item per align-self.
        handleCrossAxisAlignmentForFlexItems(flexLines, flexItems, crossSizeList, lineCrossSizeList, positionList);
    };
    performContentAlignment();

    // 9.6. Place each flex item at its final flow-aware location, applying the wrap-reverse and rtl-column
    // cross-axis flips, and write it to the renderer (cf. FlexLayout::computeFlexItemRects).
    auto computeFlexItemRects = [&] {
        auto crossContentExtent = flexLayoutUtils().crossAxisContentExtent();
        auto crossExtent = flexLayoutUtils().crossAxisExtent();
        bool isRightToLeftColumn = !m_constraints.style.writingMode().isLogicalLeftInlineStart() && m_constraints.isColumnFlow;
        for (size_t lineIndex = 0; lineIndex < flexLines.ranges.size(); ++lineIndex) {
            auto lineRange = flexLines.ranges[lineIndex];
            for (auto flexItemIndex = lineRange.begin(); flexItemIndex < lineRange.end(); ++flexItemIndex) {
                auto location = positionList[flexItemIndex];
                if (m_constraints.isWrapReverse) {
                    auto originalOffset = lineCrossOffsetList[lineIndex] - crossAxisStartEdge;
                    location.move(0_lu, (crossContentExtent - originalOffset - lineCrossSizeList[lineIndex]) - originalOffset);
                }
                if (isRightToLeftColumn) {
                    // For vertical flows, setFlowAwareLocationForFlexItem will transpose x and
                    // y, so using the y axis for a column cross axis extent is correct.
                    location.setY(crossExtent - crossSizeList[flexItemIndex] - location.y());
                    if (!m_constraints.style.writingMode().isHorizontal())
                        location.move(LayoutSize(0, -m_flexBox.horizontalScrollbarHeight()));
                }
                setFlexItemGeometry(flexItems[flexItemIndex], location);
            }
        }
    };
    computeFlexItemRects();
    return m_result;
}


static bool flexContainerIsHorizontalFlow(const RenderBox& flexItem)
{
    return downcast<RenderFlexibleBox>(*flexItem.parent()).flexLayoutUtils().isHorizontalFlow();
}

FlexLayoutItem::FlexLayoutItem(RenderBox& flexItem, bool everHadLayout)
    : renderer(flexItem)
    , mainAxisBorderAndPadding(flexContainerIsHorizontalFlow(flexItem) ? flexItem.horizontalBorderAndPaddingExtent() : flexItem.verticalBorderAndPaddingExtent())
    , crossAxisBorderAndPadding(flexContainerIsHorizontalFlow(flexItem) ? flexItem.verticalBorderAndPaddingExtent() : flexItem.horizontalBorderAndPaddingExtent())
    , mainAxisIsInlineAxis(flexContainerIsHorizontalFlow(flexItem) == flexItem.isHorizontalWritingMode())
    , everHadLayout(everHadLayout)
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

static LayoutUnit constrainSizeByMinMax(LayoutUnit size, std::pair<LayoutUnit, LayoutUnit> minMaxSizes)
{
    return std::max(minMaxSizes.first, std::min(size, minMaxSizes.second));
}

FlexLayout::FlexLines FlexLayout::computeFlexLines(FlexLayoutItems& flexItems, std::span<const FlexBaseAndHypotheticalMainSize> sizingList)
{
    // 9.3. (#5) Collect flex items into flex lines: a single-line container collects all items into one line; a
    // multi-line container collects consecutive items until the next item's outer hypothetical main size would not
    // fit in the inner main size. The first uncollected item is always collected, even if it does not fit.
    auto mainAxisAvailableSpace = m_constraints.mainAxisAvailableSpace;
    auto gapBetweenItems = flexLayoutUtils().computeGap(FlexLayoutUtils::GapType::BetweenItems);
    FlexLines flexLines;
    size_t nextIndex = 0;
    while (nextIndex < flexItems.size()) {
        auto lineStartIndex = nextIndex;
        LayoutUnit sumFlexBaseSize;
        LayoutUnit sumHypotheticalMainSize;
        // Trim the main-axis margin of the item at the start of the flex line.
        if (flexLayoutUtils().shouldTrimMainAxisMarginStart())
            trimMainAxisMarginStart(flexItems[nextIndex]);
        for (; nextIndex < flexItems.size(); ++nextIndex) {
            const auto& flexLayoutItem = flexItems[nextIndex];
            ASSERT(!flexLayoutItem.renderer->isOutOfFlowPositioned());
            if (m_constraints.isMultiline && (sumHypotheticalMainSize + flexLayoutItem.hypotheticalMainAxisMarginBoxSize(sizingList[nextIndex].hypotheticalMainContentSize) > mainAxisAvailableSpace && !canFitItemWithTrimmedMarginEnd(flexLayoutItem, sizingList[nextIndex].hypotheticalMainContentSize, sumHypotheticalMainSize, mainAxisAvailableSpace)) && nextIndex > lineStartIndex)
                break;
            sumFlexBaseSize += flexLayoutItem.flexBaseMarginBoxSize(sizingList[nextIndex].flexBaseContentSize) + gapBetweenItems;
            sumHypotheticalMainSize += flexLayoutItem.hypotheticalMainAxisMarginBoxSize(sizingList[nextIndex].hypotheticalMainContentSize) + gapBetweenItems;
        }

        // We added a gap after every item but there shouldn't be one after the last item, so subtract it here. Note
        // that sums might be negative here due to negative margins in flex items.
        sumHypotheticalMainSize -= gapBetweenItems;
        sumFlexBaseSize -= gapBetweenItems;

        // Trim the main-axis margin of the item at the end of the flex line.
        if (flexLayoutUtils().shouldTrimMainAxisMarginEnd()) {
            auto& lastItem = flexItems[nextIndex - 1];
            removeMarginEndFromFlexSizes(lastItem, sumFlexBaseSize, sumHypotheticalMainSize);
            trimMainAxisMarginEnd(lastItem);
        }

        flexLines.ranges.append({ lineStartIndex, nextIndex });
        flexLines.hypotheticalMainSizes.append(sumHypotheticalMainSize);
    }
    return flexLines;
}

Vector<LayoutUnit> FlexLayout::computeMainSizeForFlexItems(FlexLayoutItems& flexItems, const FlexLines& flexLines, std::span<const FlexBaseAndHypotheticalMainSize> sizingList, LayoutUnit gapBetweenItems)
{
    Vector<LayoutUnit> mainSizeList(flexItems.size());
    for (size_t lineIndex = 0; lineIndex < flexLines.ranges.size(); ++lineIndex) {
        auto lineRange = flexLines.ranges[lineIndex];
        auto lineItems = flexItems.mutableSpan().subspan(lineRange.begin(), lineRange.distance());
        auto lineSizing = sizingList.subspan(lineRange.begin(), lineRange.distance());
        auto lineMainSizes = mainSizeList.mutableSpan().subspan(lineRange.begin(), lineRange.distance());
        auto containerMainInnerSize = m_constraints.isColumnFlow ? flexLayoutUtils().columnInnerMainSize(flexLines.hypotheticalMainSizes[lineIndex]) : m_flexBox.contentBoxLogicalWidth();
        resolveFlexibleLengthsForLineItems(lineItems, lineSizing, lineMainSizes, containerMainInnerSize, gapBetweenItems);
    }
    return mainSizeList;
}

LayoutUnit FlexLayout::resolveFlexibleLengthsForLineItems(std::span<FlexLayoutItem> lineItems, std::span<const FlexBaseAndHypotheticalMainSize> lineSizing, std::span<LayoutUnit> mainSizes, LayoutUnit containerMainInnerSize, LayoutUnit gapBetweenItems)
{
    Vector<bool> frozen(FillWith { }, lineItems.size(), false);
    double totalFlexGrow = 0;
    double totalFlexShrink = 0;
    double totalWeightedFlexShrink = 0;
    LayoutUnit sumFlexBaseSize;
    LayoutUnit sumHypotheticalMainSize;
    for (size_t index = 0; index < lineItems.size(); ++index) {
        auto& flexLayoutItem = lineItems[index];
        mainSizes[index] = lineSizing[index].flexBaseContentSize;
        totalFlexGrow += flexLayoutItem.style().flexGrow().value;
        totalFlexShrink += flexLayoutItem.style().flexShrink().value;
        totalWeightedFlexShrink += flexLayoutItem.style().flexShrink().value * lineSizing[index].flexBaseContentSize;
        sumFlexBaseSize += flexLayoutItem.flexBaseMarginBoxSize(lineSizing[index].flexBaseContentSize);
        sumHypotheticalMainSize += flexLayoutItem.hypotheticalMainAxisMarginBoxSize(lineSizing[index].hypotheticalMainContentSize);
    }
    if (lineItems.size() > 1) {
        auto totalGap = (lineItems.size() - 1) * gapBetweenItems;
        sumFlexBaseSize += totalGap;
        sumHypotheticalMainSize += totalGap;
    }

    auto remainingFreeSpace = containerMainInnerSize - sumFlexBaseSize;
    // Step 1 (determine the used flex factor): if the summed hypothetical main sizes are less than the container's
    // inner main size, use the flex grow factor for the rest of the algorithm, otherwise the flex shrink factor.
    auto flexSign = (sumHypotheticalMainSize < containerMainInnerSize) ? FlexSign::PositiveFlexibility : FlexSign::NegativeFlexibility;

    auto freezeViolations = [&](Vector<size_t, 4>& violations) {
        for (auto index : violations) {
            auto& flexLayoutItem = lineItems[index];
            ASSERT(!frozen[index]);
            auto& flexItemStyle = flexLayoutItem.style();
            LayoutUnit flexItemSize = mainSizes[index];
            remainingFreeSpace -= flexItemSize - lineSizing[index].flexBaseContentSize;
            totalFlexGrow -= flexItemStyle.flexGrow().value;
            totalFlexShrink -= flexItemStyle.flexShrink().value;
            totalWeightedFlexShrink -= flexItemStyle.flexShrink().value * lineSizing[index].flexBaseContentSize;
            // totalWeightedFlexShrink can be negative when we exceed the precision of
            // a double when we initially calcuate totalWeightedFlexShrink. We then
            // subtract each child's weighted flex shrink with full precision, now
            // leading to a negative result. See
            // css3/flexbox/large-flex-shrink-assert.html
            totalWeightedFlexShrink = std::max(totalWeightedFlexShrink, 0.0);
            frozen[index] = true;
        }
    };

    // Step 2 (size inflexible items), https://drafts.csswg.org/css-flexbox/#resolve-flexible-lengths: freeze, at
    // its hypothetical main size, every item with a flex factor of zero, and -- when growing -- any item whose flex
    // base size is greater than its hypothetical main size, or -- when shrinking -- smaller than it.
    Vector<size_t, 4> newInflexibleItems;
    for (size_t index = 0; index < lineItems.size(); ++index) {
        auto& flexLayoutItem = lineItems[index];
        ASSERT(!flexLayoutItem.renderer->isOutOfFlowPositioned());
        ASSERT(!frozen[index]);
        float flexFactor = (flexSign == FlexSign::PositiveFlexibility) ? flexLayoutItem.style().flexGrow().value : flexLayoutItem.style().flexShrink().value;
        if (!flexFactor || (flexSign == FlexSign::PositiveFlexibility && lineSizing[index].flexBaseContentSize > lineSizing[index].hypotheticalMainContentSize) || (flexSign == FlexSign::NegativeFlexibility && lineSizing[index].flexBaseContentSize < lineSizing[index].hypotheticalMainContentSize)) {
            mainSizes[index] = lineSizing[index].hypotheticalMainContentSize;
            newInflexibleItems.append(index);
        }
    }
    freezeViolations(newInflexibleItems);

    // Step 3: record the initial free space (the remaining free space after freezing the inflexible items).
    auto initialFreeSpace = remainingFreeSpace;

    // Step 4 (loop): while unfrozen items remain, distribute the remaining free space over them by their flex
    // factors, clamp each to min/max, and freeze the items whose clamp introduced a violation.
    while (true) {
        LayoutUnit totalViolation;
        LayoutUnit usedFreeSpace;
        Vector<size_t, 4> minViolations;
        Vector<size_t, 4> maxViolations;

        // If the unfrozen items' flex factors sum to less than one, multiply the initial free space by that sum
        // and use it as the remaining free space when its magnitude is smaller.
        double sumFlexFactors = (flexSign == FlexSign::PositiveFlexibility) ? totalFlexGrow : totalFlexShrink;
        if (sumFlexFactors > 0 && sumFlexFactors < 1) {
            LayoutUnit fractional(initialFreeSpace * sumFlexFactors);
            if (fractional.abs() < remainingFreeSpace.abs())
                remainingFreeSpace = fractional;
        }

        for (size_t index = 0; index < lineItems.size(); ++index) {
            auto& flexLayoutItem = lineItems[index];
            // This check also covers out-of-flow children.
            if (frozen[index])
                continue;

            auto& flexItemStyle = flexLayoutItem.style();
            LayoutUnit flexItemSize = lineSizing[index].flexBaseContentSize;
            double extraSpace = 0;
            if (remainingFreeSpace > 0 && totalFlexGrow > 0 && flexSign == FlexSign::PositiveFlexibility && std::isfinite(totalFlexGrow))
                extraSpace = remainingFreeSpace * (flexItemStyle.flexGrow().value / totalFlexGrow);
            else if (remainingFreeSpace < 0 && totalWeightedFlexShrink > 0 && flexSign == FlexSign::NegativeFlexibility && std::isfinite(totalWeightedFlexShrink) && !flexItemStyle.flexShrink().isZero())
                extraSpace = remainingFreeSpace * flexItemStyle.flexShrink().value * lineSizing[index].flexBaseContentSize / totalWeightedFlexShrink;
            if (std::isfinite(extraSpace))
                flexItemSize += LayoutUnit::fromFloatRound(extraSpace);

            LayoutUnit adjustedFlexItemSize = constrainSizeByMinMax(flexItemSize, lineSizing[index].minMaxMainSizes);
            ASSERT(adjustedFlexItemSize >= 0);
            mainSizes[index] = adjustedFlexItemSize;
            usedFreeSpace += adjustedFlexItemSize - lineSizing[index].flexBaseContentSize;

            LayoutUnit violation = adjustedFlexItemSize - flexItemSize;
            if (violation > 0)
                minViolations.append(index);
            else if (violation < 0)
                maxViolations.append(index);
            totalViolation += violation;
        }

        if (!totalViolation) {
            remainingFreeSpace -= usedFreeSpace;
            break;
        }
        freezeViolations(totalViolation < 0 ? maxViolations : minViolations);
    }

    return remainingFreeSpace;
}

void FlexLayout::distributeMainAxisFreeSpaceForMultilineColumnIfNeeded(const FlexLines& flexLines, FlexLayoutItems& flexItems, std::span<const FlexBaseAndHypotheticalMainSize> sizingList, Vector<LayoutUnit>& mainSizeList, Vector<LayoutPoint>& positionList, const Vector<LayoutUnit>& lineCrossOffsetList, LayoutUnit gapBetweenItems)
{
    // In multi-line column flex, the container's main size (height) is only known
    // after all lines are laid out. Lines whose items had flex-grow may not have
    // received enough space because the container height wasn't final during the
    // per-line pass. Re-resolve and relayout those lines now.
    if (!m_constraints.isMultiline || !m_constraints.isColumnFlow)
        return;

    auto containerMainInnerSize = m_flexBox.contentBoxLogicalHeight();
    for (size_t lineIndex = 0; lineIndex < flexLines.ranges.size(); ++lineIndex) {
        auto lineRange = flexLines.ranges[lineIndex];
        auto lineItems = flexItems.mutableSpan().subspan(lineRange.begin(), lineRange.distance());
        auto lineSizing = sizingList.subspan(lineRange.begin(), lineRange.distance());
        auto lineMainSizes = mainSizeList.mutableSpan().subspan(lineRange.begin(), lineRange.distance());
        auto linePositions = positionList.mutableSpan().subspan(lineRange.begin(), lineRange.distance());

        auto lineMainSize = LayoutUnit { };
        for (size_t index = 0; index < lineItems.size(); ++index)
            lineMainSize += lineItems[index].flexedMarginBoxSize(lineMainSizes[index]);
        lineMainSize += (lineItems.size() - 1) * gapBetweenItems;
        if (lineMainSize >= containerMainInnerSize)
            continue;

        resolveFlexibleLengthsForLineItems(lineItems, lineSizing, lineMainSizes, containerMainInnerSize, gapBetweenItems);

        auto remainingFreeSpace = containerMainInnerSize;
        for (size_t index = 0; index < lineItems.size(); ++index)
            remainingFreeSpace -= lineItems[index].flexedMarginBoxSize(lineMainSizes[index]);
        remainingFreeSpace -= (lineItems.size() - 1) * gapBetweenItems;

        layoutFlexItems(lineItems, lineMainSizes, RelayoutChildren::No);
        placeFlexItems(lineCrossOffsetList[lineIndex], lineItems, linePositions, remainingFreeSpace, gapBetweenItems);
    }
}

void FlexLayout::trimCrossAxisMarginsForFlexItems(FlexLayoutItems& flexItems, const FlexLines& flexLines)
{
    // Cross axis margins are only trimmed on the first and last flex line.
    auto shouldTrimStart = flexLayoutUtils().shouldTrimCrossAxisMarginStart();
    auto shouldTrimEnd = flexLayoutUtils().shouldTrimCrossAxisMarginEnd();
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

void FlexLayout::layoutFlexItems(std::span<FlexLayoutItem> flexLayoutItems, std::span<const LayoutUnit> mainSizes, RelayoutChildren relayoutChildren)
{
    for (size_t index = 0; index < flexLayoutItems.size(); ++index)
        m_flexBox.layoutFlexItemAfterMainSizing(flexLayoutItems[index], mainSizes[index], relayoutChildren);
}


Vector<LayoutUnit> FlexLayout::hypotheticalCrossSizeForFlexItems(const FlexLayoutItems& flexItems)
{
    // 9.4. (#7) The hypothetical cross size of each item is the cross size it would have at its used main size.
    Vector<LayoutUnit> hypotheticalCrossSizeList(flexItems.size());
    for (size_t flexItemIndex = 0; flexItemIndex < flexItems.size(); ++flexItemIndex)
        hypotheticalCrossSizeList[flexItemIndex] = crossAxisIntrinsicExtentForFlexItem(flexItems[flexItemIndex]);
    return hypotheticalCrossSizeList;
}

Vector<LayoutUnit> FlexLayout::crossSizeForFlexLines(const FlexLines& flexLines, const FlexLayoutItems& flexItems, const Vector<LayoutUnit>& hypotheticalCrossSizeList)
{
    // 9.4. (#8) The used cross size of each flex line is the largest of: the summed baseline ascent and descent of
    // its baseline-aligned items, the largest outer hypothetical cross size of the remaining items, and zero. (The
    // single-line container with a definite cross size uses the inner cross size; the caller applies that.)
    Vector<LayoutUnit> flexLinesCrossSizeList(flexLines.ranges.size());
    for (size_t lineIndex = 0; lineIndex < flexLines.ranges.size(); ++lineIndex) {
        auto lineRange = flexLines.ranges[lineIndex];

        LayoutUnit maxFlexItemCrossAxisExtent;
        LayoutUnit maxAscent;
        LayoutUnit maxDescent = LayoutUnit::min();
        LayoutUnit lastBaselineMaxAscent;
        for (auto flexItemIndex = lineRange.begin(); flexItemIndex < lineRange.end(); ++flexItemIndex) {
            auto& flexItem = flexItems[flexItemIndex].renderer.get();
            ASSERT(!flexItem.isOutOfFlowPositioned());

            LayoutUnit flexItemCrossAxisMarginBoxExtent;
            auto alignment = flexLayoutUtils().alignmentForFlexItem(flexItem);
            if ((alignment == ItemPosition::Baseline || alignment == ItemPosition::LastBaseline) && !flexLayoutUtils().hasAutoMarginsInCrossAxis(flexItem)) {
                LayoutUnit ascent = flexLayoutUtils().marginBoxAscentForFlexItem(flexItem, flexLayoutUtils().crossAxisExtentForFlexItem(flexItem));
                LayoutUnit descent = (flexLayoutUtils().crossAxisMarginExtentForFlexItem(flexItem) + flexLayoutUtils().crossAxisExtentForFlexItem(flexItem)) - ascent;
                maxDescent = std::max(maxDescent, descent);
                if (alignment == ItemPosition::Baseline) {
                    maxAscent = std::max(maxAscent, ascent);
                    flexItemCrossAxisMarginBoxExtent = maxAscent + maxDescent;
                } else {
                    lastBaselineMaxAscent = std::max(lastBaselineMaxAscent, ascent);
                    flexItemCrossAxisMarginBoxExtent = lastBaselineMaxAscent + maxDescent;
                }
            } else
                flexItemCrossAxisMarginBoxExtent = hypotheticalCrossSizeList[flexItemIndex] + flexLayoutUtils().crossAxisMarginExtentForFlexItem(flexItem);

            maxFlexItemCrossAxisExtent = std::max(maxFlexItemCrossAxisExtent, flexItemCrossAxisMarginBoxExtent);
        }
        flexLinesCrossSizeList[lineIndex] = maxFlexItemCrossAxisExtent;
    }
    return flexLinesCrossSizeList;
}

Vector<LayoutPoint> FlexLayout::handleMainAxisAlignment(const FlexLines& flexLines, FlexLayoutItems& flexItems, const Vector<LayoutUnit>& mainSizeList, const Vector<LayoutUnit>& lineCrossOffsetList, LayoutUnit gapBetweenItems)
{
    Vector<LayoutPoint> positionList(flexItems.size());
    for (size_t lineIndex = 0; lineIndex < flexLines.ranges.size(); ++lineIndex) {
        auto lineRange = flexLines.ranges[lineIndex];
        auto containerMainInnerSize = m_constraints.isColumnFlow ? flexLayoutUtils().columnInnerMainSize(flexLines.hypotheticalMainSizes[lineIndex]) : m_flexBox.contentBoxLogicalWidth();

        // The remaining free space is the line's inner main size minus the used outer main sizes of its items.
        // (The 0..1 flex-factor adjustment means we recompute it here rather than trust the resolve step's leftover.)
        auto remainingFreeSpace = containerMainInnerSize;
        for (auto flexItemIndex = lineRange.begin(); flexItemIndex < lineRange.end(); ++flexItemIndex) {
            ASSERT(!flexItems[flexItemIndex].renderer->isOutOfFlowPositioned());
            remainingFreeSpace -= mainSizeList[flexItemIndex] + flexItems[flexItemIndex].mainAxisBorderAndPadding + flexItems[flexItemIndex].mainAxisMargin;
        }
        remainingFreeSpace -= (lineRange.distance() - 1) * gapBetweenItems;

        placeFlexItems(lineCrossOffsetList[lineIndex], flexItems.mutableSpan().subspan(lineRange.begin(), lineRange.distance()), positionList.mutableSpan().subspan(lineRange.begin(), lineRange.distance()), remainingFreeSpace, gapBetweenItems);
    }
    return positionList;
}

Vector<LayoutUnit> FlexLayout::computeCrossSizeForFlexItems(const FlexLines& flexLines, FlexLayoutItems& flexItems, const Vector<LayoutUnit>& lineCrossSizeList)
{
    Vector<LayoutUnit> crossSizeList(flexItems.size());
    for (size_t lineIndex = 0; lineIndex < flexLines.ranges.size(); ++lineIndex) {
        auto lineRange = flexLines.ranges[lineIndex];
        for (auto flexItemIndex = lineRange.begin(); flexItemIndex < lineRange.end(); ++flexItemIndex) {
            auto& flexItem = flexItems[flexItemIndex].renderer.get();
            ASSERT(!flexItem.isOutOfFlowPositioned());
            // If a flex item has align-self: stretch, its computed cross size property is auto, and neither of its cross-axis margins are auto, the used outer cross size is the used cross size
            // of its flex line, clamped according to the item's used min and max cross sizes. Otherwise, the used cross size is the item's hypothetical cross size.
            if (flexLayoutUtils().alignmentForFlexItem(flexItem) == ItemPosition::Stretch && !flexLayoutUtils().hasAutoMarginsInCrossAxis(flexItem))
                crossSizeList[flexItemIndex] = applyStretchAlignmentToFlexItem(flexItems[flexItemIndex], lineCrossSizeList[lineIndex]);
            else
                crossSizeList[flexItemIndex] = flexLayoutUtils().crossAxisExtentForFlexItem(flexItem);
        }
    }
    return crossSizeList;
}

void FlexLayout::handleCrossAxisAlignmentForFlexLines(const FlexLines& flexLines, Vector<LayoutPoint>& positionList, Vector<LayoutUnit>& lineCrossOffsetList, Vector<LayoutUnit>& lineCrossSizeList, LayoutUnit gapBetweenLines)
{
    // 9.6. (#16) Align the flex lines within the flex container per align-content, and (#9) grow the lines to fill
    // the container for align-content: stretch. A single-line container has nothing to align.
    if (flexLines.ranges.isEmpty() || !m_constraints.isMultiline)
        return;

    auto alignedContent = m_constraints.style.alignContent().resolve(FlexLayoutUtils::contentAlignmentNormalBehavior());
    auto position = alignedContent.position();
    auto distribution = alignedContent.distribution();
    auto safety = alignedContent.overflow();

    bool isWrapReverse = m_constraints.isWrapReverse;

    if (position == ContentPosition::FlexStart && !gapBetweenLines && safety != OverflowAlignment::Safe && !isWrapReverse)
        return;

    size_t numLines = flexLines.ranges.size();
    LayoutUnit availableCrossAxisSpace = flexLayoutUtils().crossAxisContentExtent() - (numLines - 1) * gapBetweenLines;
    for (size_t i = 0; i < numLines; ++i)
        availableCrossAxisSpace -= lineCrossSizeList[i];

    m_result.alignContentStartOverflow = FlexLayoutUtils::contentAlignmentStartOverflow(availableCrossAxisSpace, position, distribution, safety, isWrapReverse);
    LayoutUnit lineOffset = FlexLayoutUtils::initialAlignContentOffset(availableCrossAxisSpace, position, distribution, safety, numLines, isWrapReverse);
    for (unsigned lineNumber = 0; lineNumber < numLines; ++lineNumber) {
        lineCrossOffsetList[lineNumber] += lineOffset;
        // Fold this line's align-content offset into each of its items' cross-axis position.
        auto lineRange = flexLines.ranges[lineNumber];
        for (auto flexItemIndex = lineRange.begin(); flexItemIndex < lineRange.end(); ++flexItemIndex)
            positionList[flexItemIndex].move(0_lu, lineOffset);

        if (distribution == ContentDistribution::Stretch && availableCrossAxisSpace > 0)
            lineCrossSizeList[lineNumber] += availableCrossAxisSpace / static_cast<unsigned>(numLines);

        lineOffset += FlexLayoutUtils::alignContentSpaceBetweenFlexItems(availableCrossAxisSpace, distribution, numLines) + gapBetweenLines;
    }
}

void FlexLayout::handleCrossAxisAlignmentForFlexItems(const FlexLines& flexLines, FlexLayoutItems& flexItems, const Vector<LayoutUnit>& crossSizeList, const Vector<LayoutUnit>& lineCrossSizeList, Vector<LayoutPoint>& positionList)
{
    // 9.6. (#13, #14) For each item resolve its cross-axis auto margins, then -- when neither cross-axis margin is
    // auto -- align it within its line per align-self (baseline-aligned items go through performBaselineAlignment).
    Vector<LayoutUnit> crossItemOffsetList(flexItems.size());
    for (size_t lineIndex = 0; lineIndex < flexLines.ranges.size(); ++lineIndex) {
        auto lineRange = flexLines.ranges[lineIndex];
        LayoutUnit lineCrossAxisExtent = lineCrossSizeList[lineIndex];

        performBaselineAlignment(lineRange, flexItems, crossItemOffsetList, crossSizeList, lineCrossAxisExtent);

        for (auto flexItemIndex = lineRange.begin(); flexItemIndex < lineRange.end(); ++flexItemIndex) {
            auto& flexLayoutItem = flexItems[flexItemIndex];
            ASSERT(!flexLayoutItem.renderer->isOutOfFlowPositioned());

            auto safety = flexLayoutUtils().overflowAlignmentForFlexItem(flexLayoutItem.renderer);
            auto position = flexLayoutUtils().alignmentForFlexItem(flexLayoutItem.renderer);
            if (updateAutoMarginsInCrossAxis(flexLayoutItem, crossItemOffsetList[flexItemIndex], std::max(0_lu, flexLayoutUtils().availableAlignmentSpaceForFlexItem(lineCrossAxisExtent, flexLayoutItem.renderer, crossSizeList[flexItemIndex]))) || position == ItemPosition::Baseline || position == ItemPosition::LastBaseline)
                continue;

            LayoutUnit availableSpace = flexLayoutUtils().availableAlignmentSpaceForFlexItem(lineCrossAxisExtent, flexLayoutItem.renderer, crossSizeList[flexItemIndex]);
            if (availableSpace < 0 && safety == OverflowAlignment::Safe)
                position = ItemPosition::FlexStart; // See Start == FlexStart assumption in flexLayoutUtils().alignmentForFlexItem().
            LayoutUnit offset = FlexLayoutUtils::alignmentOffset(availableSpace, position, { }, { }, m_constraints.isWrapReverse);
            crossItemOffsetList[flexItemIndex] += offset;
        }
    }
    // Fold each item's cross-axis alignment offset (align-self / auto-margin / baseline) into its position.
    for (size_t flexItemIndex = 0; flexItemIndex < flexItems.size(); ++flexItemIndex)
        positionList[flexItemIndex].move(0_lu, crossItemOffsetList[flexItemIndex]);
}

void FlexLayout::performBaselineAlignment(WTF::Range<size_t> lineRange, FlexLayoutItems& flexItems, Vector<LayoutUnit>& crossItemOffsetList, const Vector<LayoutUnit>& crossSizeList, LayoutUnit lineCrossAxisExtent)
{
    // 9.6. (#14) Align each baseline-aligned item (align-self: baseline / last baseline) so its baseline sits on
    // its baseline-sharing group's shared baseline within the flex line.
    bool containerHasWrapReverse = m_constraints.isWrapReverse;

    auto flexItemWritingModeForBaselineAlignment = [&](const RenderBox& flexItem) {
        if (flexLayoutUtils().mainAxisIsFlexItemInlineAxis(flexItem))
            return flexItem.style().writingMode();

        auto alignmentContextAxis = m_constraints.style.isRowFlexDirection() ? LogicalBoxAxis::Inline : LogicalBoxAxis::Block;
        return BaselineAlignment::usedWritingModeForBaselineAlignment(alignmentContextAxis, m_constraints.style.writingMode(), flexItem.writingMode());
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
        auto& flexItem = flexItems[itemIndex].renderer.get();
        auto alignment = flexLayoutUtils().alignmentForFlexItem(flexItem);
        if ((alignment != ItemPosition::Baseline && alignment != ItemPosition::LastBaseline) || flexLayoutUtils().hasAutoMarginsInCrossAxis(flexItem))
            continue;
        if (!baselineAlignmentState) {
            auto alignmentContextAxis = m_constraints.style.isRowFlexDirection() ? LogicalBoxAxis::Inline : LogicalBoxAxis::Block;
            baselineAlignmentState = BaselineAlignmentState { alignmentContextAxis, m_constraints.style.writingMode() };
        }
        auto baselineSharingGroupIndex = baselineAlignmentState->sharedGroupIndex(flexItem.writingMode(), alignment);
        if (baselineSharingGroupIndex == baselineSharingGroups.size())
            baselineSharingGroups.append({ });
        auto& group = baselineSharingGroups[baselineSharingGroupIndex];
        group.maxAscent = std::max(group.maxAscent, flexLayoutUtils().marginBoxAscentForFlexItem(flexItem, crossSizeList[itemIndex]));
        group.items.append(itemIndex);
    }

    for (auto& baselineSharingGroup : baselineSharingGroups) {
        LayoutUnit minMarginAfterBaseline = LayoutUnit::max();
        for (auto itemIndex : baselineSharingGroup.items) {
            auto& flexItem = flexItems[itemIndex].renderer.get();
            auto position = flexLayoutUtils().alignmentForFlexItem(flexItem);
            ASSERT(position == ItemPosition::Baseline || position == ItemPosition::LastBaseline);
            auto offset = FlexLayoutUtils::alignmentOffset(flexLayoutUtils().availableAlignmentSpaceForFlexItem(lineCrossAxisExtent, flexItem, crossSizeList[itemIndex]), position, flexLayoutUtils().marginBoxAscentForFlexItem(flexItem, crossSizeList[itemIndex]), baselineSharingGroup.maxAscent, containerHasWrapReverse);
            crossItemOffsetList[itemIndex] += offset;

            if (shouldAdjustItemTowardsCrossAxisEnd(flexItemWritingModeForBaselineAlignment(flexItem).blockDirection(), position))
                minMarginAfterBaseline = std::min(minMarginAfterBaseline, flexLayoutUtils().availableAlignmentSpaceForFlexItem(lineCrossAxisExtent, flexItem, crossSizeList[itemIndex]) - offset);
        }
        // css-align-3 9.3 part 3:
        // Position the aligned baseline-sharing group within the alignment container according to its
        // fallback alignment. The fallback alignment of a baseline-sharing group is the fallback alignment
        // of its items as resolved to physical directions.
        if (minMarginAfterBaseline) {
            for (auto itemIndex : baselineSharingGroup.items) {
                auto& flexItem = flexItems[itemIndex].renderer.get();
                if (shouldAdjustItemTowardsCrossAxisEnd(flexItemWritingModeForBaselineAlignment(flexItem).blockDirection(), flexLayoutUtils().alignmentForFlexItem(flexItem)) && !flexLayoutUtils().hasAutoMarginsInCrossAxis(flexItem))
                    crossItemOffsetList[itemIndex] += minMarginAfterBaseline;
            }
        }
    }
}

void FlexLayout::placeFlexItems(LayoutUnit crossAxisOffset, std::span<FlexLayoutItem> flexLayoutItems, std::span<LayoutPoint> positions, LayoutUnit availableFreeSpace, LayoutUnit gapBetweenItems)
{
    // 9.5. (#12) Position the items along the main axis: first give any positive free space to main-axis auto
    // margins, then place each item per justify-content (the initial offset plus the spacing between items).
    LayoutUnit autoMarginOffset = autoMarginOffsetInMainAxis(flexLayoutItems, availableFreeSpace);
    LayoutUnit mainAxisOffset = m_constraints.flowAwareBorderInline.first + m_constraints.flowAwarePaddingInline.first;
    mainAxisOffset += FlexLayoutUtils::initialJustifyContentOffset(m_constraints.style, availableFreeSpace, flexLayoutItems.size(), m_constraints.isColumnOrRowReverse);
    if (m_constraints.style.flexDirection() == FlexDirection::RowReverse)
        mainAxisOffset += m_constraints.isHorizontalFlow ? m_flexBox.verticalScrollbarWidth() : m_flexBox.horizontalScrollbarHeight();

    if (availableFreeSpace < 0) {
        auto resolvedJustifyContent = m_constraints.style.justifyContent().resolve(FlexLayoutUtils::contentAlignmentNormalBehavior());
        auto distribution = resolvedJustifyContent.distribution();
        auto safety = resolvedJustifyContent.overflow();
        auto position = FlexLayoutUtils::resolveLeftRightAlignment(resolvedJustifyContent.position(), resolvedJustifyContent, m_constraints.style, m_constraints.isColumnOrRowReverse);
        LayoutUnit overflow = FlexLayoutUtils::contentAlignmentStartOverflow(availableFreeSpace, position, distribution, safety, m_constraints.isColumnOrRowReverse);
        m_result.justifyContentStartOverflow = std::max(m_result.justifyContentStartOverflow, overflow);
    }

    LayoutUnit totalMainExtent = flexLayoutUtils().mainAxisExtent();

    auto resolvedJustifyContent = m_constraints.style.justifyContent().resolve(FlexLayoutUtils::contentAlignmentNormalBehavior());
    auto distribution = resolvedJustifyContent.distribution();
    bool shouldFlipMainAxis = !m_constraints.isColumnFlow && !m_constraints.isLeftToRightFlow;
    for (size_t i = 0; i < flexLayoutItems.size(); ++i) {
        auto& flexItem = flexLayoutItems[i].renderer.get();

        ASSERT(!flexItem.isOutOfFlowPositioned());

        updateAutoMarginsInMainAxis(flexItem, autoMarginOffset);

        mainAxisOffset += flexLayoutUtils().flowAwareMarginStartForFlexItem(flexItem);

        LayoutUnit flexItemMainExtent = flexLayoutUtils().mainAxisExtentForFlexItem(flexItem);
        // In an RTL column situation, this will apply the margin-right/margin-end
        // on the left. This will be fixed later by the rtl-column flip in computeFlexItemRects.
        auto leadingScrollbarSize = m_constraints.style.writingMode().isInlineFlipped() && m_constraints.style.writingMode().isVertical() ? flexLayoutUtils().mainAxisScrollbarExtent() : LayoutUnit();
        LayoutPoint location(shouldFlipMainAxis ? totalMainExtent - mainAxisOffset - flexItemMainExtent - leadingScrollbarSize : mainAxisOffset, crossAxisOffset + flexLayoutUtils().flowAwareMarginBeforeForFlexItem(flexItem));
        positions[i] = location;
        setFlexItemGeometry(flexLayoutItems[i], positions[i]);
        mainAxisOffset += flexItemMainExtent + flexLayoutUtils().flowAwareMarginEndForFlexItem(flexItem);

        if (i != flexLayoutItems.size() - 1) {
            // The last item does not get extra space added.
            mainAxisOffset += FlexLayoutUtils::justifyContentSpaceBetweenFlexItems(availableFreeSpace, distribution, flexLayoutItems.size()) + gapBetweenItems;
        }

        // FIXME: Deal with pagination.
    }

    if (m_constraints.isColumnFlow)
        m_flexBox.setLogicalHeight(std::max(m_flexBox.logicalHeight(), mainAxisOffset + m_constraints.flowAwareBorderInline.second + m_constraints.flowAwarePaddingInline.second + m_flexBox.scrollbarLogicalHeight()));

    if (m_constraints.style.flexDirection() == FlexDirection::ColumnReverse) {
        // We have to do an extra pass for column-reverse to reposition the flex
        // items since the start depends on the height of the flexbox, which we
        // only know after we've positioned all the flex items.
        m_flexBox.updateLogicalHeight();
        layoutColumnReverse(flexLayoutItems, positions, crossAxisOffset, availableFreeSpace, gapBetweenItems);
    }
}

void FlexLayout::layoutColumnReverse(std::span<FlexLayoutItem> flexLayoutItems, std::span<LayoutPoint> positions, LayoutUnit crossAxisOffset, LayoutUnit availableFreeSpace, LayoutUnit gapBetweenItems)
{
    // This is similar to the logic in placeFlexItems, except we place
    // the children starting from the end of the flexbox. We also don't need to
    // layout anything since we're just moving the children to a new position.
    LayoutUnit mainAxisOffset = m_flexBox.logicalHeight() - m_constraints.flowAwareBorderInline.second - m_constraints.flowAwarePaddingInline.second;
    mainAxisOffset -= FlexLayoutUtils::initialJustifyContentOffset(m_constraints.style, availableFreeSpace, flexLayoutItems.size(), m_constraints.isColumnOrRowReverse);
    mainAxisOffset -= m_constraints.isHorizontalFlow ? m_flexBox.verticalScrollbarWidth() : m_flexBox.horizontalScrollbarHeight();

    auto distribution = m_constraints.style.justifyContent().resolve(FlexLayoutUtils::contentAlignmentNormalBehavior()).distribution();

    for (size_t i = 0; i < flexLayoutItems.size(); ++i) {
        auto& flexItem = flexLayoutItems[i].renderer;
        ASSERT(!flexItem->isOutOfFlowPositioned());
        mainAxisOffset -= flexLayoutUtils().mainAxisExtentForFlexItem(flexItem) + flexLayoutUtils().flowAwareMarginEndForFlexItem(flexItem);
        positions[i] = LayoutPoint(mainAxisOffset, crossAxisOffset + flexLayoutUtils().flowAwareMarginBeforeForFlexItem(flexItem));
        setFlexItemGeometry(flexLayoutItems[i], positions[i]);
        mainAxisOffset -= flexLayoutUtils().flowAwareMarginStartForFlexItem(flexItem);

        if (i != flexLayoutItems.size() - 1) {
            // The last item does not get extra space added.
            mainAxisOffset -= FlexLayoutUtils::justifyContentSpaceBetweenFlexItems(availableFreeSpace, distribution, flexLayoutItems.size()) + gapBetweenItems;
        }
    }
}

void FlexLayout::setFlexItemCountsForFirstAndLastLine(const FlexLines& flexLines)
{
    if (flexLines.ranges.isEmpty())
        return;

    auto isWrapReverse = m_constraints.isWrapReverse;
    auto firstLineItemsCountInOriginalOrder = flexLines.ranges.first().distance();
    auto lastLineItemsCountInOriginalOrder = flexLines.ranges.last().distance();

    m_result.numberOfFlexItemsOnFirstLine = !isWrapReverse ? firstLineItemsCountInOriginalOrder : lastLineItemsCountInOriginalOrder;
    m_result.numberOfFlexItemsOnLastLine = !isWrapReverse ? lastLineItemsCountInOriginalOrder : firstLineItemsCountInOriginalOrder;
}

FlexLayout::FlexBaseAndHypotheticalMainSize FlexLayout::flexBaseAndHypotheticalMainSize(const FlexLayoutItem& flexLayoutItem)
{
    auto flexBaseContentSize = flexBaseSizeForFlexItem(flexLayoutItem);
    auto minMaxMainSizes = computeFlexItemMinMaxMainSizes(flexLayoutItem);
    // The hypothetical main size is the item's flex base size clamped according to its used min and max main sizes.
    return { flexBaseContentSize, std::max(minMaxMainSizes.first, std::min(flexBaseContentSize, minMaxMainSizes.second)), minMaxMainSizes };
}

// This is a RAII class that is used to temporarily set the flex basis as the child size in the main axis.
class ScopedFlexBasisAsFlexItemMainSize {
public:
    ScopedFlexBasisAsFlexItemMainSize(RenderBox& flexItem, Style::PreferredSize&& flexBasis, bool mainAxisIsInlineAxis)
        : m_flexItem(flexItem)
        , m_mainAxisIsInlineAxis(mainAxisIsInlineAxis)
    {
        if (flexBasis.isAuto())
            return;

        if (m_mainAxisIsInlineAxis)
            m_flexItem.setOverridingBorderBoxLogicalWidthForFlexBasisComputation(WTF::move(flexBasis));
        else
            m_flexItem.setOverridingBorderBoxLogicalHeightForFlexBasisComputation(WTF::move(flexBasis));
        m_didOverride = true;
    }

    ~ScopedFlexBasisAsFlexItemMainSize()
    {
        if (!m_didOverride)
            return;

        if (m_mainAxisIsInlineAxis)
            m_flexItem.clearOverridingLogicalWidthForFlexBasisComputation();
        else
            m_flexItem.clearOverridingLogicalHeightForFlexBasisComputation();
    }

private:
    RenderBox& m_flexItem;
    bool m_mainAxisIsInlineAxis { true };
    bool m_didOverride { false };
};

// https://drafts.csswg.org/css-flexbox/#algo-main-item
LayoutUnit FlexLayout::flexBaseSizeForFlexItem(const FlexLayoutItem& flexLayoutItem)
{
    auto& flexItem = flexLayoutItem.renderer.get();
    auto flexBasis = flexLayoutUtils().flexBasisForFlexItem(flexItem);
    ScopedFlexBasisAsFlexItemMainSize scoped(flexItem, flexBasis.tryPreferredSize().value_or(Style::PreferredSize { CSS::Keyword::MaxContent { } }), flexLayoutItem.mainAxisIsInlineAxis);
    // FIXME: While we are supposed to ignore min/max here, the cached
    // The cached block-axis size entry may hold a min/max-constrained size.
    auto computingBaseSizesScope = m_flexBox.scopedComputingFlexBaseSizes();
    auto blockAxisContentSize = ensureBlockAxisContentSizeForFlexItemIfNeeded(flexLayoutItem);

    // A. If the item has a definite used flex basis, that's the flex base size.
    if (m_flexBox.flexItemMainSizeIsDefinite(flexItem, flexBasis))
        return std::max(0_lu, computeMainAxisExtentForFlexItem(flexLayoutItem, flexBasis).value());

    // B. If the flex item has a preferred aspect ratio, a used flex basis of content, and a definite cross size,
    // the flex base size is calculated from its used cross size and the flex item's aspect ratio.
    if (flexItemHasComputableAspectRatioAndCrossSizeIsConsideredDefinite(flexLayoutItem)) {
        auto& crossSizeLength = flexLayoutUtils().preferredCrossSizeLengthForFlexItem(flexItem);
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
        ASSERT(!flexItem.needsLayout());
        ASSERT(blockAxisContentSize);
        return blockAxisContentSize.value_or(0_lu);
    }

    // We don't need to add scrollbarLogicalWidth here because the preferred
    // width includes the scrollbar, even for overflow: auto.
    RenderFlexibleBox::ScopedCrossAxisOverrideForFlexItem crossSizeScope(m_flexBox, flexItem, RenderFlexibleBox::ScopedCrossAxisOverrideForFlexItem::InvalidateContentWidths::Yes);
    auto mainAxisExtent = flexItem.maxContentLogicalWidthContribution();
    auto mainAxisBorderAndPadding = m_constraints.isHorizontalFlow ? flexItem.horizontalBorderAndPaddingExtent() : flexItem.verticalBorderAndPaddingExtent();
    return mainAxisExtent - mainAxisBorderAndPadding;
}

bool FlexLayout::flexBaseSizeNeedsBlockAxisContentSize(const FlexLayoutItem& flexLayoutItem)
{
    auto& flexItem = flexLayoutItem.renderer.get();
    if (flexLayoutItem.mainAxisIsInlineAxis)
        return false;

    auto flexBasis = flexLayoutUtils().flexBasisForFlexItem(flexItem);
    auto minSize = flexLayoutUtils().minMainSizeLengthForFlexItem(flexItem);
    auto maxSize = flexLayoutUtils().maxMainSizeLengthForFlexItem(flexItem);
    // FIXME: we must run m_flexBox.flexItemMainSizeIsDefinite() because it might end up calling computePercentageLogicalHeight()
    // which has some side effects like calling addPercentHeightDescendant() for example so it is not possible to skip
    // the call for example by moving it to the end of the conditional expression. This is error-prone and we should
    // refactor computePercentageLogicalHeight() at some point so that it only computes stuff without those side effects.
    if (!m_flexBox.flexItemMainSizeIsDefinite(flexItem, flexBasis) || minSize.isIntrinsic() || maxSize.isIntrinsic())
        return true;

    if (flexLayoutUtils().useContentBasedMinimumSize(flexItem))
        return true;

    return false;
}

std::optional<LayoutUnit> FlexLayout::ensureBlockAxisContentSizeForFlexItemIfNeeded(const FlexLayoutItem& flexLayoutItem)
{
    if (!flexBaseSizeNeedsBlockAxisContentSize(flexLayoutItem))
        return { };
    // Laying the item out, reusing the previously cached size, and caching the new one are all render-tree work; ask RenderFlexibleBox.
    return m_flexBox.computeBlockAxisContentSizeForFlexItem(flexLayoutItem.renderer.get());
}

std::pair<LayoutUnit, LayoutUnit> FlexLayout::computeFlexItemMinMaxMainSizes(const FlexLayoutItem& flexLayoutItem)
{
    auto& flexItem = flexLayoutItem.renderer.get();
    auto maxExtent = computeUsedMaxMainSize(flexLayoutItem);
    auto resolvedMax = maxExtent.value_or(LayoutUnit::max());

    // useContentBasedMinimumSize covers both auto-equivalent cases: min:auto with
    // non-scrollable overflow (§ 4.5) and block-axis intrinsic keywords (CSS Sizing
    // 3 § 5.2 makes those behave like auto, regardless of overflow).
    if (flexLayoutUtils().useContentBasedMinimumSize(flexItem))
        return { computeContentBasedMinMainSize(flexLayoutItem, maxExtent), resolvedMax };

    auto min = flexLayoutUtils().minMainSizeLengthForFlexItem(flexItem);
    if (!min.isAuto())
        return { computeUsedNonAutoMinMainSize(flexLayoutItem, min), resolvedMax };

    // min:auto on a scroll container — spec says the automatic minimum size is zero.
    return { 0_lu, resolvedMax };
}

std::optional<LayoutUnit> FlexLayout::computeUsedMaxMainSize(const FlexLayoutItem& flexLayoutItem)
{
    auto& flexItem = flexLayoutItem.renderer.get();
    auto max = flexLayoutUtils().maxMainSizeLengthForFlexItem(flexItem);
    if (max.isSpecified())
        return computeMainAxisExtentForFlexItem(flexLayoutItem, max);
    if (max.isIntrinsicOrStretch()) {
        RenderFlexibleBox::ScopedCrossAxisOverrideForFlexItem scopedCrossAxisOverride(m_flexBox, flexItem, RenderFlexibleBox::ScopedCrossAxisOverrideForFlexItem::InvalidateContentWidths::No);
        return computeMainAxisExtentForFlexItem(flexLayoutItem, max);
    }
    return { };
}

LayoutUnit FlexLayout::computeUsedNonAutoMinMainSize(const FlexLayoutItem& flexLayoutItem, const Style::MinimumSize& min)
{
    auto& flexItem = flexLayoutItem.renderer.get();
    // https://drafts.csswg.org/css-flexbox/#main-size-property
    // Resolves the used min main size for every case except min:auto. Three values
    // route here: a specified length/percentage, the stretch keyword, and intrinsic
    // keywords (min-content/max-content/fit-content) on the inline axis. Per CSS
    // Sizing 3 § 5.2, intrinsic keywords on the block axis behave like auto, so
    // those go through computeContentBasedMinMainSize instead.
    auto minExtent = [&] {
        if (min.isIntrinsicOrStretch()) {
            RenderFlexibleBox::ScopedCrossAxisOverrideForFlexItem scopedCrossAxisOverride(m_flexBox, flexItem, RenderFlexibleBox::ScopedCrossAxisOverrideForFlexItem::InvalidateContentWidths::No);
            return computeMainAxisExtentForFlexItem(flexLayoutItem, min).value_or(0_lu);
        }
        return computeMainAxisExtentForFlexItem(flexLayoutItem, min).value_or(0_lu);
    }();

    // We must never return a min size smaller than the min preferred size for tables.
    if (flexItem.isRenderTable() && flexLayoutItem.mainAxisIsInlineAxis) {
        RenderFlexibleBox::ScopedCrossAxisOverrideForFlexItem scopedCrossAxisOverride(m_flexBox, flexItem, RenderFlexibleBox::ScopedCrossAxisOverrideForFlexItem::InvalidateContentWidths::Yes);
        minExtent = std::max(minExtent, flexItem.minContentLogicalWidthContribution());
    }
    return minExtent;
}

LayoutUnit FlexLayout::computeContentBasedMinMainSize(const FlexLayoutItem& flexLayoutItem, std::optional<LayoutUnit> maxExtent)
{
    auto& flexItem = flexLayoutItem.renderer.get();
    // The content-based minimum size (https://drafts.csswg.org/css-flexbox/#min-size-auto): for a non-replaced item
    // the larger, and for a replaced item the smaller, of the content size suggestion and the transferred size
    // suggestion, capped by the specified size suggestion, and clamped by the definite maximum main size.
    // FIXME: If the min value is expected to be valid here, we need to come up with a non optional version of computeMainAxisExtentForFlexItem and
    // ensure it's valid through the virtual calls of computeSizingKeywordLogicalContentHeightUsing.
    LayoutUnit contentSize;
    auto& flexItemCrossSizeLength = flexLayoutUtils().preferredCrossSizeLengthForFlexItem(flexItem);

    // Content size suggestion: the min-content size in the main axis, clamped (when the item has a preferred
    // aspect ratio) through the aspect ratio.
    bool canComputeSizeThroughAspectRatio = flexLayoutUtils().flexItemHasComputableAspectRatio(flexItem) && flexItemCrossSizeIsDefinite(flexLayoutItem, flexItemCrossSizeLength);
    if (canComputeSizeThroughAspectRatio)
        contentSize = computeMainSizeFromAspectRatioUsing(flexLayoutItem, flexItemCrossSizeLength);

    if (!canComputeSizeThroughAspectRatio || !flexItem.isRenderReplaced()) {
        RenderFlexibleBox::ScopedCrossAxisOverrideForFlexItem scopedCrossAxisOverride(m_flexBox, flexItem, RenderFlexibleBox::ScopedCrossAxisOverrideForFlexItem::InvalidateContentWidths::No);
        auto minContentSize = computeMainAxisExtentForFlexItem(flexLayoutItem, Style::MinimumSize { CSS::Keyword::MinContent { } }).value_or(0_lu);
        contentSize = std::max(contentSize, minContentSize);
    }

    if (flexLayoutUtils().flexItemHasAspectRatio(flexItem))
        contentSize = adjustFlexItemSizeForAspectRatioCrossAxisMinAndMax(flexLayoutItem, contentSize);

    contentSize = std::max(0_lu, contentSize);
    ASSERT(contentSize >= 0);
    contentSize = std::min(contentSize, maxExtent.value_or(contentSize));

    // Specified size suggestion: if the item's preferred main size is definite, cap the result by that size.
    auto mainSize = flexLayoutUtils().preferredMainSizeLengthForFlexItem(flexItem);
    if (m_flexBox.flexItemMainSizeIsDefinite(flexItem, mainSize)) {
        auto resolvedMainSize = computeMainAxisExtentForFlexItem(flexLayoutItem, mainSize).value_or(0);
        ASSERT(resolvedMainSize >= 0);
        auto specifiedSize = std::min(resolvedMainSize, maxExtent.value_or(resolvedMainSize));
        return std::min(specifiedSize, contentSize);
    }

    // Transferred size suggestion: a replaced item's definite cross size converted through its aspect ratio.
    if (flexItem.isRenderReplaced() && flexItemHasComputableAspectRatioAndCrossSizeIsConsideredDefinite(flexLayoutItem)) {
        auto transferredSize = computeMainSizeFromAspectRatioUsing(flexLayoutItem, flexItemCrossSizeLength);
        transferredSize = adjustFlexItemSizeForAspectRatioCrossAxisMinAndMax(flexLayoutItem, transferredSize);
        return std::min(transferredSize, contentSize);
    }

    return contentSize;
}

template<typename SizeType> std::optional<LayoutUnit> FlexLayout::computeMainAxisExtentForFlexItem(const FlexLayoutItem& flexLayoutItem, const SizeType& size)
{
    auto& flexItem = flexLayoutItem.renderer.get();
    // If we have a horizontal flow, that means the main size is the width.
    // That's the logical width for horizontal writing modes, and the logical
    // height in vertical writing modes. For a vertical flow, main size is the
    // height, so it's the inverse. So we need the logical width if we have a
    // horizontal flow and horizontal writing mode, or vertical flow and vertical
    // writing mode. Otherwise we need the logical height.
    if (!flexLayoutItem.mainAxisIsInlineAxis) {
        // No "auto" check needed: computeContentLogicalHeight returns nullopt for
        // auto and we propagate that below.
        auto height = flexItem.computeContentLogicalHeight(size, m_flexBox.flexItemContentLogicalHeight(flexItem));
        if (!height)
            return height;

        // Tables interpret overriding sizes as the size of captions + rows. However the specified height of a table
        // only includes the size of the rows. That's why we need to add the size of the captions here so that the table
        // layout algorithm behaves appropriately.
        LayoutUnit captionsHeight;
        if (CheckedPtr table = dynamicDowncast<RenderTable>(flexItem); table && m_flexBox.flexItemMainSizeIsDefinite(flexItem, size))
            captionsHeight = table->sumCaptionsLogicalHeight();

        // scrollbarLogicalHeight depends on layout having run. flexBaseSizeForFlexItem
        // calls ensureBlockAxisContentSizeForFlexItemIfNeeded before reaching here,
        // which forces layout when flexBaseSizeNeedsBlockAxisContentSize is true. On
        // the false path (definite flex-basis + non-auto min-size + non-visible/clip
        // overflow) layout has not run and this returns 0; that coincides with the
        // spec, which does not attribute a scrollbar contribution to the flex base
        // size on that path.
        return *height + flexItem.scrollbarLogicalHeight() + captionsHeight;
    }

    // computeLogicalWidth always re-computes the intrinsic widths. However, when
    // our logical width is auto, we can just use our cached value. So let's do
    // that here. (Compare code in RenderBlock::computeIntrinsicLogicalWidthContributions)
    if (flexItem.style().logicalWidth().isAuto() && !flexLayoutUtils().flexItemHasAspectRatio(flexItem)) {
        if (size.isMinContent()) {
            if (flexItem.shouldInvalidateContentWidths())
                flexItem.invalidateContentLogicalWidths(MarkingBehavior::MarkOnlyThis);
            return flexItem.minContentLogicalWidthContribution() - flexItem.borderAndPaddingLogicalWidth();
        }
        if (size.isMaxContent()) {
            if (flexItem.shouldInvalidateContentWidths())
                flexItem.invalidateContentLogicalWidths(MarkingBehavior::MarkOnlyThis);
            return flexItem.maxContentLogicalWidthContribution() - flexItem.borderAndPaddingLogicalWidth();
        }
    }

    auto mainAxisWidth = m_constraints.isColumnFlow ? m_flexBox.availableLogicalHeight(AvailableLogicalHeightType::ExcludeMarginBorderPadding) : m_flexBox.contentBoxLogicalWidth();
    return flexItem.computeLogicalWidthUsing(size, mainAxisWidth, m_flexBox) - flexItem.borderAndPaddingLogicalWidth();
}

// FIXME: computeMainSizeFromAspectRatioUsing may need to return an std::optional<LayoutUnit> in the future
// rather than returning indefinite sizes as 0/-1.
template<typename SizeType> LayoutUnit FlexLayout::computeMainSizeFromAspectRatioUsing(const FlexLayoutItem& flexLayoutItem, const SizeType& crossSizeLength) const
{
    auto& flexItem = flexLayoutItem.renderer.get();
    ASSERT(flexLayoutUtils().flexItemHasAspectRatio(flexItem));
    auto flexItemCrossAxisBorderAndPadding = flexLayoutItem.crossAxisBorderAndPadding;

    // All paths return border-box cross size.
    auto crossSizeOptional = WTF::switchOn(crossSizeLength,
        [&](const SizeType::Fixed& fixedCrossSizeLength) -> std::optional<LayoutUnit> {
            auto value = LayoutUnit { fixedCrossSizeLength.resolveZoom(flexItem.style().usedZoomForLength()) };
            if (flexItem.style().boxSizing() == BoxSizing::ContentBox)
                value += flexItemCrossAxisBorderAndPadding;
            return value;
        },
        [&](const SizeType::Percentage& percentageCrossSizeLength) -> std::optional<LayoutUnit> {
            return flexLayoutItem.mainAxisIsInlineAxis
                ? flexItem.computePercentageLogicalHeight(SizeType { percentageCrossSizeLength })
                : m_flexBox.adjustBorderBoxLogicalWidthForBoxSizing(Style::evaluate<LayoutUnit>(percentageCrossSizeLength, m_flexBox.contentBoxWidth()));
        },
        [&](const SizeType::Calc& calcCrossSizeLength) -> std::optional<LayoutUnit> {
            return flexLayoutItem.mainAxisIsInlineAxis
                ? flexItem.computePercentageLogicalHeight(calcCrossSizeLength)
                : m_flexBox.adjustBorderBoxLogicalWidthForBoxSizing(Style::evaluate<LayoutUnit>(calcCrossSizeLength, m_flexBox.contentBoxWidth(), flexItem.style().usedZoomForLength()));
        },
        [&](const CSS::Keyword::Auto&) -> std::optional<LayoutUnit> {
            ASSERT(flexLayoutUtils().hasDefiniteCrossSizeForFlexItem(flexItem));
            return flexLayoutUtils().innerCrossSizeForFlexItem(flexItem);
        },
        [&](const CSS::Keyword::Stretch&) -> std::optional<LayoutUnit> {
            // Resolve stretch against the flex container's cross-axis definite size.
            return flexLayoutUtils().innerCrossSizeForFlexItem(flexItem);
        },
        [&](const CSS::Keyword::WebkitFillAvailable&) -> std::optional<LayoutUnit> {
            return flexLayoutUtils().innerCrossSizeForFlexItem(flexItem);
        },
        [&](const auto&) -> std::optional<LayoutUnit> {
            ASSERT_NOT_REACHED();
            return { };
        }
    );
    if (!crossSizeOptional)
        return 0_lu;

    auto crossSize = *crossSizeOptional;
    auto preferredAspectRatio = flexLayoutUtils().preferredAspectRatioForFlexItem(flexItem);

    auto useCSSAspectRatio = flexItem.style().aspectRatio().isRatio() || (flexItem.style().aspectRatio().isAutoAndRatio() && flexItem.intrinsicSize().isEmpty());
    if (!useCSSAspectRatio) {
        // Intrinsic aspect ratio (e.g. from <img>). The sizing calculations that floor
        // the content box size at zero when applying box-sizing are also ignored.
        // https://drafts.csswg.org/css-flexbox/#algo-main-item.
        crossSize -= flexItemCrossAxisBorderAndPadding;
        return std::max(0_lu, LayoutUnit { crossSize * preferredAspectRatio });
    }

    auto boxSizingForAspectRatio = flexItem.style().boxSizingForAspectRatio();
    if (boxSizingForAspectRatio == BoxSizing::ContentBox) {
        // Ratio applies to content dimensions. Convert border-box cross size to content-box.
        crossSize -= flexItemCrossAxisBorderAndPadding;
        return std::max(0_lu, LayoutUnit { crossSize * preferredAspectRatio });
    }

    // Ratio applies to border-box dimensions. Compute border-box main size,
    // then subtract main-axis border+padding to return content-box.
    ASSERT(flexItem.style().boxSizing() == BoxSizing::BorderBox);
    auto flexItemMainAxisBorderAndPadding = m_constraints.isHorizontalFlow ? flexItem.horizontalBorderAndPaddingExtent() : flexItem.verticalBorderAndPaddingExtent();
    return std::max(0_lu, LayoutUnit { crossSize * preferredAspectRatio } - flexItemMainAxisBorderAndPadding);
}

LayoutUnit FlexLayout::adjustFlexItemSizeForAspectRatioCrossAxisMinAndMax(const FlexLayoutItem& flexLayoutItem, LayoutUnit flexItemSize)
{
    auto& flexItem = flexLayoutItem.renderer.get();
    auto& crossMin = flexLayoutUtils().minCrossSizeLengthForFlexItem(flexItem);
    auto& crossMax = flexLayoutUtils().maxCrossSizeLengthForFlexItem(flexItem);

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

LayoutUnit FlexLayout::crossAxisIntrinsicExtentForFlexItem(const FlexLayoutItem& flexLayoutItem)
{
    return flexLayoutItem.mainAxisIsInlineAxis ? flexItemIntrinsicLogicalHeight(flexLayoutItem) : flexItemIntrinsicLogicalWidth(flexLayoutItem);
}

LayoutUnit FlexLayout::flexItemIntrinsicLogicalHeight(const FlexLayoutItem& flexLayoutItem) const
{
    auto& flexItem = flexLayoutItem.renderer.get();
    // This should only be called if the logical height is the cross size
    ASSERT(flexLayoutItem.mainAxisIsInlineAxis);
    if (flexLayoutUtils().needToStretchFlexItemLogicalHeight(flexItem)) {
        LayoutUnit flexItemContentHeight = m_flexBox.flexItemContentLogicalHeight(flexItem);
        LayoutUnit flexItemLogicalHeight = flexItemContentHeight + flexItem.scrollbarLogicalHeight() + flexItem.borderAndPaddingLogicalHeight();
        return flexItem.constrainLogicalHeightByMinMax(flexItemLogicalHeight, flexItemContentHeight);
    }
    return flexItem.logicalHeight();
}

LayoutUnit FlexLayout::flexItemIntrinsicLogicalWidth(const FlexLayoutItem& flexLayoutItem)
{
    auto& flexItem = flexLayoutItem.renderer.get();
    // This should only be called if the logical width is the cross size
    ASSERT(!flexLayoutItem.mainAxisIsInlineAxis);
    if (flexItemCrossSizeIsDefinite(flexLayoutItem, flexItem.style().logicalWidth()))
        return flexItem.logicalWidth();

    RenderBox::LogicalExtentComputedValues values;
    {
        RenderFlexibleBox::OverridingSizesScope cleanOverridingWidthScope(flexItem, RenderFlexibleBox::OverridingSizesScope::Axis::Inline);
        flexItem.computeLogicalWidth(values);
    }
    return values.extent;
}

template<typename SizeType> bool FlexLayout::flexItemCrossSizeIsDefinite(const FlexLayoutItem& flexLayoutItem, const SizeType& size)
{
    auto& flexItem = flexLayoutItem.renderer.get();
    if constexpr (!std::same_as<SizeType, Style::MaximumSize>) {
        if (size.isAuto())
            return false;
    }

    // Stretch is definite in the same cases as percentages, i.e. when the
    // container's cross size is definite. We use a dummy percentage for stretch
    // since computePercentageLogicalHeight evaluates the value as a percentage.
    auto crossSizeIsDefinite = [&](const auto& sizeForPercentageComputation) {
        if (!flexLayoutItem.mainAxisIsInlineAxis || m_flexBox.hasDefiniteHeight() == RenderFlexibleBox::SizeDefiniteness::Definite)
            return true;
        if (m_flexBox.hasDefiniteHeight() == RenderFlexibleBox::SizeDefiniteness::Indefinite)
            return false;
        bool definite = bool(flexItem.computePercentageLogicalHeight(sizeForPercentageComputation));
        m_flexBox.setHasDefiniteHeight(definite ? RenderFlexibleBox::SizeDefiniteness::Definite : RenderFlexibleBox::SizeDefiniteness::Indefinite);
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

bool FlexLayout::flexItemHasComputableAspectRatioAndCrossSizeIsConsideredDefinite(const FlexLayoutItem& flexLayoutItem)
{
    auto& flexItem = flexLayoutItem.renderer.get();
    return flexLayoutUtils().flexItemHasComputableAspectRatio(flexItem)
        && (flexItemCrossSizeIsDefinite(flexLayoutItem, flexLayoutUtils().preferredCrossSizeLengthForFlexItem(flexItem)) || flexLayoutUtils().hasDefiniteCrossSizeForFlexItem(flexItem));
}

void FlexLayout::trimMainAxisMarginStart(FlexLayoutItem& flexLayoutItem)
{
    auto horizontalFlow = m_constraints.isHorizontalFlow;
    flexLayoutItem.mainAxisMargin -= horizontalFlow
        ? flexLayoutItem.renderer->marginStart(m_constraints.style.writingMode())
        : flexLayoutItem.renderer->marginBefore(m_constraints.style.writingMode());
    if (horizontalFlow)
        m_flexBox.setTrimmedMarginForChild(flexLayoutItem.renderer, Style::MarginTrimSide::InlineStart);
    else
        m_flexBox.setTrimmedMarginForChild(flexLayoutItem.renderer, Style::MarginTrimSide::BlockStart);
    m_flexBox.addItemAtFlexLineStart(flexLayoutItem.renderer);
}

void FlexLayout::trimMainAxisMarginEnd(FlexLayoutItem& flexLayoutItem)
{
    auto horizontalFlow = m_constraints.isHorizontalFlow;
    flexLayoutItem.mainAxisMargin -= horizontalFlow
        ? flexLayoutItem.renderer->marginEnd(m_constraints.style.writingMode())
        : flexLayoutItem.renderer->marginAfter(m_constraints.style.writingMode());
    if (horizontalFlow)
        m_flexBox.setTrimmedMarginForChild(flexLayoutItem.renderer, Style::MarginTrimSide::InlineEnd);
    else
        m_flexBox.setTrimmedMarginForChild(flexLayoutItem.renderer, Style::MarginTrimSide::BlockEnd);
    m_flexBox.addItemAtFlexLineEnd(flexLayoutItem.renderer);
}

void FlexLayout::trimCrossAxisMarginStart(const FlexLayoutItem& flexLayoutItem)
{
    if (m_constraints.isHorizontalFlow)
        m_flexBox.setTrimmedMarginForChild(flexLayoutItem.renderer, Style::MarginTrimSide::BlockStart);
    else
        m_flexBox.setTrimmedMarginForChild(flexLayoutItem.renderer, Style::MarginTrimSide::InlineStart);
    m_flexBox.addItemOnFirstFlexLine(flexLayoutItem.renderer);
}

void FlexLayout::trimCrossAxisMarginEnd(const FlexLayoutItem& flexLayoutItem)
{
    if (m_constraints.isHorizontalFlow)
        m_flexBox.setTrimmedMarginForChild(flexLayoutItem.renderer, Style::MarginTrimSide::BlockEnd);
    else
        m_flexBox.setTrimmedMarginForChild(flexLayoutItem.renderer, Style::MarginTrimSide::InlineEnd);
    m_flexBox.addItemOnLastFlexLine(flexLayoutItem.renderer);
}

bool FlexLayout::canFitItemWithTrimmedMarginEnd(const FlexLayoutItem& flexLayoutItem, LayoutUnit hypotheticalMainContentSize, LayoutUnit sumHypotheticalMainSize, LayoutUnit mainAxisAvailableSpace) const
{
    auto marginTrim = m_constraints.style.marginTrim();
    if ((m_constraints.isHorizontalFlow && marginTrim.contains(Style::MarginTrimSide::InlineEnd)) || (m_constraints.isColumnFlow && marginTrim.contains(Style::MarginTrimSide::BlockEnd)))
        return sumHypotheticalMainSize + flexLayoutItem.hypotheticalMainAxisMarginBoxSize(hypotheticalMainContentSize) - flexLayoutUtils().flowAwareMarginEndForFlexItem(flexLayoutItem.renderer) <= mainAxisAvailableSpace;
    return false;
}

void FlexLayout::removeMarginEndFromFlexSizes(FlexLayoutItem& flexLayoutItem, LayoutUnit& sumFlexBaseSize, LayoutUnit& sumHypotheticalMainSize) const
{
    LayoutUnit margin;
    if (m_constraints.isHorizontalFlow)
        margin = flexLayoutItem.renderer->marginEnd(m_constraints.style.writingMode());
    else
        margin = flexLayoutItem.renderer->marginAfter(m_constraints.style.writingMode());
    sumFlexBaseSize -= margin;
    sumHypotheticalMainSize -= margin;
}

LayoutUnit FlexLayout::autoMarginOffsetInMainAxis(std::span<const FlexLayoutItem> flexLayoutItems, LayoutUnit& availableFreeSpace)
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

void FlexLayout::updateAutoMarginsInMainAxis(RenderBox& flexItem, LayoutUnit autoMarginOffset)
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

bool FlexLayout::updateAutoMarginsInCrossAxis(FlexLayoutItem& flexLayoutItem, LayoutUnit& crossOffset, LayoutUnit availableAlignmentSpace)
{
    // 9.6. (#13) Resolve cross-axis auto margins: if both cross-axis margins are auto, split the free space
    // between them; if only one is auto, give it all the free space so the item's outer cross size fills the line.
    auto& flexItem = flexLayoutItem.renderer.get();
    ASSERT(!flexItem.isOutOfFlowPositioned());
    ASSERT(availableAlignmentSpace >= 0_lu);

    bool isHorizontal = m_constraints.isHorizontalFlow;
    auto& topOrLeft = isHorizontal ? flexItem.style().marginTop() : flexItem.style().marginLeft();
    auto& bottomOrRight = isHorizontal ? flexItem.style().marginBottom() : flexItem.style().marginRight();
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

LayoutUnit FlexLayout::applyStretchAlignmentToFlexItem(const FlexLayoutItem& flexLayoutItem, LayoutUnit lineCrossAxisExtent)
{
    auto& flexItem = flexLayoutItem.renderer.get();
    // 9.4. (#11) A stretched item's used cross size is its flex line's cross size minus the item's cross-axis
    // margins, clamped by the item's used min and max cross sizes. (An item with a definite cross size is not
    // stretched, but still gets the stretch min/max clamp via applyStretchMinMaxCrossSize.)
    auto flexLayoutScope = m_flexBox.scopedAfterCrossAxisItemSizing();
    if (flexLayoutItem.mainAxisIsInlineAxis) {
        // Cross axis is block axis (height).
        if (!flexItem.style().logicalHeight().isAuto() && !flexItem.style().logicalHeight().isStretch())
            return applyStretchMinMaxCrossSize(flexLayoutItem, lineCrossAxisExtent, LogicalBoxAxis::Block);

        auto stretchedLogicalHeight = std::max(flexItem.borderAndPaddingLogicalHeight(),
            lineCrossAxisExtent - flexLayoutUtils().crossAxisMarginExtentForFlexItem(flexItem));
        ASSERT(!flexItem.needsLayout());
        LayoutUnit desiredLogicalHeight = flexItem.constrainLogicalHeightByMinMax(stretchedLogicalHeight, m_flexBox.flexItemContentLogicalHeight(flexItem));

        // FIXME: Can avoid laying out here in some cases. See https://webkit.org/b/87905.
        bool flexItemNeedsRelayout = desiredLogicalHeight != flexItem.logicalHeight();
        if (!flexItemNeedsRelayout && m_flexBox.hasFlexItemCompletedLayout(flexItem) && m_flexBox.flexItemHasPercentHeightDescendants(flexItem)) {
            // Have to force another relayout even though the child is sized
            // correctly, because its descendants are not sized correctly yet. Our
            // previous layout of the child was done without an override height set.
            // So, redo it here.
            flexItemNeedsRelayout = true;
        }
        m_flexBox.stretchFlexItemLogicalHeight(flexItem, desiredLogicalHeight, flexItemNeedsRelayout);
        return desiredLogicalHeight;
    }

    // Cross axis is inline axis (width).
    if (!flexItem.style().logicalWidth().isAuto() && !flexItem.style().logicalWidth().isStretch())
        return applyStretchMinMaxCrossSize(flexLayoutItem, lineCrossAxisExtent, LogicalBoxAxis::Inline);

    auto flexItemWidth = std::max(0_lu, lineCrossAxisExtent - flexLayoutUtils().crossAxisMarginExtentForFlexItem(flexItem));
    flexItemWidth = flexItem.constrainLogicalWidthByMinMax(flexItemWidth, flexLayoutUtils().crossAxisContentExtent(), m_flexBox);

    if (flexItemWidth != flexItem.logicalWidth())
        m_flexBox.relayoutFlexItemForStretchedCrossSize(flexItem, flexItemWidth, LogicalBoxAxis::Inline);
    return flexItemWidth;
}

LayoutUnit FlexLayout::applyStretchMinMaxCrossSize(const FlexLayoutItem& flexLayoutItem, LayoutUnit lineCrossAxisExtent, LogicalBoxAxis crossAxis)
{
    auto& flexItem = flexLayoutItem.renderer.get();
    // Clamp an item that has a definite cross size by its used min and max cross sizes, resolving a 'stretch'
    // keyword on either against the flex line's cross size (part of 9.4 #11).
    bool isBlockAxis = crossAxis == LogicalBoxAxis::Block;
    auto& style = flexItem.style();
    auto& min = isBlockAxis ? style.logicalMinHeight() : style.logicalMinWidth();
    auto& max = isBlockAxis ? style.logicalMaxHeight() : style.logicalMaxWidth();
    bool minIsStretch = min.isStretch();
    bool maxIsStretch = !max.isNone() && max.isStretch();
    if (!minIsStretch && !maxIsStretch)
        return flexLayoutUtils().crossAxisExtentForFlexItem(flexItem);

    // The block-axis floor ensures the stretched size never goes below border+padding,
    // matching the behavior in applyStretchAlignmentToFlexItem.
    auto stretchValue = std::max(isBlockAxis ? flexItem.borderAndPaddingLogicalHeight() : 0_lu,
        lineCrossAxisExtent - flexLayoutUtils().crossAxisMarginExtentForFlexItem(flexItem));

    auto computeBlockSize = [&](const auto& size, LayoutUnit fallback) {
        return flexItem.computeLogicalHeightUsing(size, std::nullopt).value_or(fallback);
    };
    auto computeInlineSize = [&](const auto& size) {
        return flexItem.computeLogicalWidthUsing(size, flexLayoutUtils().crossAxisContentExtent(), m_flexBox);
    };

    // Compute the specified cross-size, unclamped by stretch min/max.
    // We cannot use the current laid-out size because the initial layout
    // resolves stretch against the container, not the flex line.
    auto specifiedSize = isBlockAxis
        ? computeBlockSize(style.logicalHeight(), flexItem.logicalHeight())
        : computeInlineSize(style.logicalWidth());

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

    auto currentSize = isBlockAxis ? flexItem.logicalHeight() : flexItem.logicalWidth();
    if (newSize != currentSize)
        m_flexBox.relayoutFlexItemForStretchedCrossSize(flexItem, newSize, crossAxis);
    return newSize;
}

void FlexLayout::setFlowAwareLocationForFlexItem(RenderBox& flexItem, const LayoutPoint& location)
{
    if (m_constraints.isHorizontalFlow)
        flexItem.setLocation(location);
    else
        flexItem.setLocation(location.transposedPoint());
}

void FlexLayout::setFlexItemGeometry(FlexLayoutItem& flexLayoutItem, const LayoutPoint& location)
{
    setFlowAwareLocationForFlexItem(flexLayoutItem.renderer, location);
}


} // namespace WebCore
