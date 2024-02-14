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

#pragma once

#include "FlexFormattingConstraints.h"
#include "FlexRect.h"
#include "LogicalFlexItem.h"
#include "RenderStyleConstants.h"
#include <wtf/Range.h>

namespace WebCore {

class RenderStyle;

namespace Layout {

class FlexFormattingContext;
struct FlexBaseAndHypotheticalMainSize;
struct PositionAndMargins;

// This class implements the layout logic for flex formatting contexts.
// https://www.w3.org/TR/css-flexbox-1/
class FlexLayout {
public:
    FlexLayout(FlexFormattingContext&);

    using LogicalFlexItems = Vector<LogicalFlexItem>;
    struct LogicalConstraints {
        struct AxisGeometry {
            std::optional<LayoutUnit> definiteSize;
            std::optional<LayoutUnit> minimumSize;
            std::optional<LayoutUnit> maximumSize;

            std::optional<LayoutUnit> minimumContentSize;
            std::optional<LayoutUnit> maximumContentSize;
            LayoutUnit availableSize; // space available to the flex container minus margin, border, and padding.
        };
        AxisGeometry mainAxis;
        AxisGeometry crossAxis;
    };
    using LogicalFlexItemRects = FixedVector<FlexRect>;
    LogicalFlexItemRects layout(const LogicalConstraints&, const LogicalFlexItems&);

private:
    using FlexBaseAndHypotheticalMainSizeList = Vector<FlexBaseAndHypotheticalMainSize>;
    using LineRanges = Vector<WTF::Range<size_t>>;
    using SizeList = FixedVector<LayoutUnit>;
    using PositionAndMarginsList = FixedVector<PositionAndMargins>;
    using LinesCrossSizeList = Vector<LayoutUnit>;
    using LinesCrossPositionList = Vector<LayoutUnit>;

    void computeAvailableMainAndCrossSpace(const LogicalConstraints&);
    FlexBaseAndHypotheticalMainSizeList flexBaseAndHypotheticalMainSizeForFlexItems(const LogicalConstraints::AxisGeometry& mainAxis, const LogicalFlexItems&);
    LayoutUnit flexContainerMainSize(const LogicalConstraints::AxisGeometry& mainAxis) const;
    LineRanges computeFlexLines(const LogicalFlexItems&, LayoutUnit flexContainerMainSize, const FlexBaseAndHypotheticalMainSizeList&) const;
    SizeList computeMainSizeForFlexItems(const LogicalFlexItems&, const LineRanges&, LayoutUnit flexContainerMainSize, const FlexBaseAndHypotheticalMainSizeList&) const;
    SizeList hypotheticalCrossSizeForFlexItems(const LogicalFlexItems&, const SizeList& flexItemsMainSizeList);
    LinesCrossSizeList crossSizeForFlexLines(const LineRanges&, const LogicalConstraints::AxisGeometry& crossAxis, const LogicalFlexItems&, const SizeList& flexItemsHypotheticalCrossSizeList) const;
    void stretchFlexLines(LinesCrossSizeList& flexLinesCrossSizeList, size_t numberOfLines, const LogicalConstraints::AxisGeometry& crossAxis) const;
    bool collapseNonVisibleFlexItems();
    SizeList computeCrossSizeForFlexItems(const LogicalFlexItems&, const LineRanges&, const LinesCrossSizeList& flexLinesCrossSizeList, const SizeList& flexItemsHypotheticalCrossSizeList) const;
    PositionAndMarginsList handleMainAxisAlignment(LayoutUnit availableMainSpace, const LineRanges&, const LogicalFlexItems&, const SizeList& flexItemsMainSizeList) const;
    PositionAndMarginsList handleCrossAxisAlignmentForFlexItems(const LogicalFlexItems&, const LineRanges&, const SizeList& flexItemsCrossSizeList, const LinesCrossSizeList& flexLinesCrossSizeList) const;
    LinesCrossPositionList handleCrossAxisAlignmentForFlexLines(const LogicalConstraints::AxisGeometry& crossAxis, const LineRanges&, LinesCrossSizeList& flexLinesCrossSizeList) const;

    LayoutUnit maxContentForFlexItem(const LogicalFlexItem&);

    bool isSingleLineFlexContainer() const { return flexContainer().style().flexWrap() == FlexWrap::NoWrap; }
    const ElementBox& flexContainer() const;
    FlexFormattingContext& flexFormattingContext();
    const RenderStyle& flexContainerStyle() const { return flexContainer().style(); }

private:
    FlexFormattingContext& m_flexFormattingContext;

    LayoutUnit m_availableMainSpace;
    LayoutUnit m_availableCrossSpace;
};

}
}
