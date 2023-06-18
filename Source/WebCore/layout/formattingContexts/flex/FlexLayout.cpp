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
    UNUSED_PARAM(mainAxis);
    UNUSED_PARAM(flexItems);
    return { };
}

LayoutUnit FlexLayout::flexContainerMainSize(const LogicalConstraints::AxisGeometry& mainAxis) const
{
    UNUSED_PARAM(mainAxis);
    return { };
}

FlexLayout::LineRanges FlexLayout::computeFlexLines(const LogicalFlexItems& flexItems, LayoutUnit flexContainerMainSize, const FlexBaseAndHypotheticalMainSizeList& flexBaseAndHypotheticalMainSizeList) const
{
    UNUSED_PARAM(flexItems);
    UNUSED_PARAM(flexContainerMainSize);
    UNUSED_PARAM(flexBaseAndHypotheticalMainSizeList);
    return { };
}

FlexLayout::SizeList FlexLayout::computeMainSizeForFlexItems(const LogicalFlexItems& flexItems, const LineRanges& lineRanges, LayoutUnit flexContainerMainSize, const FlexBaseAndHypotheticalMainSizeList& flexBaseAndHypotheticalMainSizeList) const
{
    UNUSED_PARAM(flexItems);
    UNUSED_PARAM(lineRanges);
    UNUSED_PARAM(flexContainerMainSize);
    UNUSED_PARAM(flexBaseAndHypotheticalMainSizeList);
    return { };
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
