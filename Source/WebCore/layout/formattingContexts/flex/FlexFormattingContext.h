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

#pragma once

#include <WebCore/FlexFormattingUtils.h>
#include <WebCore/FlexIntegrationUtils.h>
#include <WebCore/RenderBlock.h>
#include <wtf/Range.h>

namespace WebCore {

namespace Style {
class ComputedStyle;
struct MinimumSize;
}

class FlexLayoutState;
class RenderFlexibleBox;

namespace LayoutIntegration { class FlexIntegrationUtils; }

class FlexLayoutItem {
public:
    FlexLayoutItem(RenderBox&, bool flexContainerIsHorizontalFlow, bool everHadLayout, bool shouldInvalidateChildContent);

    LayoutUnit NODELETE hypotheticalMainAxisMarginBoxSize(LayoutUnit hypotheticalMainContentSize) const;
    LayoutUnit NODELETE flexBaseMarginBoxSize(LayoutUnit flexBaseContentSize) const;
    LayoutUnit NODELETE flexedMarginBoxSize(LayoutUnit mainSize) const;
    const Style::ComputedStyle& NODELETE style() const LIFETIME_BOUND;

    // The item's current, laid-out geometry.
    LayoutUnit NODELETE logicalWidth() const;
    LayoutUnit NODELETE logicalHeight() const;
    LayoutUnit NODELETE borderAndPaddingLogicalHeight() const;
    LayoutSize NODELETE intrinsicSize() const;
#if ASSERT_ENABLED
    bool needsLayout() const;
#endif

    bool NODELETE isTable() const;
    bool NODELETE isReplaced() const;

    CheckedRef<RenderBox> renderer;
    const LayoutUnit mainAxisBorderAndPadding;
    LayoutUnit mainAxisMargin;
    const LayoutUnit crossAxisBorderAndPadding;
    // True when the flex container's main axis is this item's inline axis (i.e. the item is not orthogonal to the container).
    const bool mainAxisIsInlineAxis;
    bool everHadLayout { false };
    // Whether the container is relaying out all its items this pass (RelayoutChildren::Yes); layoutFlexItemWithMainSize
    // combines it with !hasFlexItemCompletedLayout to decide whether to invalidate this child's content.
    bool shouldInvalidateChildContent { false };
};

using FlexLayoutItems = Vector<FlexLayoutItem, 4>;

struct FlexLayoutConstraints {
    // The formatting-context root's computed style (the flex container's own style).
    CheckedRef<const Style::ComputedStyle> style;
    bool isHorizontalFlow { false };
    bool isColumnFlow { false };
    bool isMultiline { false };
    bool isWrapReverse { false };
    bool isColumnOrRowReverse { false };
    bool isLeftToRightFlow { false };
    FlowDirection crossAxisDirection { };
    // Flow-relative border/padding, each as an inline {start, end} pair and a block {before, after} pair.
    std::pair<LayoutUnit, LayoutUnit> flowAwareBorderInline;
    std::pair<LayoutUnit, LayoutUnit> flowAwareBorderBlock;
    std::pair<LayoutUnit, LayoutUnit> flowAwarePaddingInline;
    std::pair<LayoutUnit, LayoutUnit> flowAwarePaddingBlock;
    LayoutUnit mainAxisAvailableSpace;
    LayoutUnit mainAxisSizeForLengthResolution;
    LayoutUnit mainAxisBorderBoxExtent;
    LayoutUnit crossAxisSizeForLengthResolution;
    LayoutUnit mainAxisScrollbarExtent;
    LayoutUnit crossAxisScrollbarExtent;
};

struct FlexContainerUsedExtents {
    LayoutUnit crossContentBox;
    LayoutUnit crossBorderBox;
    LayoutUnit blockContentBox;
    LayoutUnit blockBorderBox;
};

class FlexFormattingContext {
public:
    FlexFormattingContext(RenderFlexibleBox&, const FlexLayoutConstraints&, FlexLayoutState&, FlexItemContentCache&);

    struct Result {
        std::optional<LayoutUnit> alignContentStartOverflow;
        LayoutUnit justifyContentStartOverflow;
        size_t numberOfFlexItemsOnFirstLine { 0 };
        size_t numberOfFlexItemsOnLastLine { 0 };
    };
    Result layout(FlexLayoutItems&);

private:
    struct FlexBaseAndHypotheticalMainSize {
        LayoutUnit flexBase;
        LayoutUnit hypotheticalMainSize;
        std::pair<LayoutUnit, LayoutUnit> minMaxMainSizes;
    };

    using FlexBaseAndHypotheticalMainSizeList = Vector<FlexBaseAndHypotheticalMainSize, 4>;
    struct FlexLines;

    using LineRanges = Vector<WTF::Range<size_t>>;
    using SizeList = Vector<LayoutUnit>;
    using PositionList = Vector<LayoutPoint>;
    using LinesCrossSizeList = Vector<LayoutUnit>;
    using LinesCrossPositionList = Vector<LayoutUnit>;
    struct FlexLines {
        LineRanges ranges;
        Vector<LayoutUnit> hypotheticalMainSizes;
    };

    struct BaselineSharingGroup {
        LayoutUnit maxAscent;
        Vector<size_t> items;
    };
    // A line almost always has a single baseline-sharing group (at most 3 can exist), so keep one inline.
    using BaselineSharingGroups = Vector<BaselineSharingGroup, 1>;

    FlexLines computeFlexLines(FlexLayoutItems& flexItems, std::span<const FlexBaseAndHypotheticalMainSize> flexBaseAndHypotheticalMainSizeList);
    SizeList computeMainSizeForFlexItems(FlexLayoutItems& flexItems, const FlexLines&, std::span<const FlexBaseAndHypotheticalMainSize> flexBaseAndHypotheticalMainSizeList);
    void resolveFlexibleLengthsForLineItems(std::span<FlexLayoutItem> lineItems, std::span<const FlexBaseAndHypotheticalMainSize> lineFlexBaseAndHypotheticalMainSizeList, std::span<LayoutUnit> flexItemsMainSizeList, LayoutUnit flexContainerInnerMainSize);
    void distributeMainAxisFreeSpaceForMultilineColumnIfNeeded(const FlexLines&, FlexLayoutItems&, std::span<const FlexBaseAndHypotheticalMainSize> flexBaseAndHypotheticalMainSizeList, SizeList& flexItemsMainSizeList, PositionList& flexItemsPositionList, const LinesCrossPositionList& flexLinesCrossPositionList, LayoutUnit containerMainBlockContentExtent);
    LayoutUnit mainAxisAvailableSpaceForItemAlignment(LayoutUnit mainAxisAvailableSpace, size_t numberOfFlexItems) const;
    LayoutUnit crossAxisAvailableSpaceForLineSizingAndAlignment(LayoutUnit crossAxisAvailableSpace, size_t numberOfFlexLines) const;
    void trimCrossAxisMarginsForFlexItems(FlexLayoutItems& flexItems, const FlexLines&);
    void layoutFlexItems(FlexLayoutItems&, std::span<const LayoutUnit> flexItemsMainSizeList);
    void layoutFlexItemsWithMainSizes(std::span<FlexLayoutItem>, std::span<const LayoutUnit> flexItemsMainSizeList);
    SizeList hypotheticalCrossSizeForFlexItems(const FlexLayoutItems&);
    LinesCrossSizeList crossSizeForFlexLines(const FlexLines&, const FlexLayoutItems&, const SizeList& flexItemsHypotheticalCrossSizeList);
    LinesCrossPositionList computeFlexLineCrossPositions(const FlexLines&, const LinesCrossSizeList&, LayoutUnit& flexContentBlockExtent);
    PositionList handleMainAxisAlignment(const FlexLines&, FlexLayoutItems&, const SizeList& flexItemsMainSizeList, const LinesCrossPositionList& flexLinesCrossPositionList, LayoutUnit& columnMainContentExtent);
    SizeList computeCrossSizeForFlexItems(const FlexLines&, FlexLayoutItems&, const LinesCrossSizeList& flexLinesCrossSizeList, LayoutUnit crossContentExtent);
    void handleCrossAxisAlignmentForFlexLines(const FlexLines&, PositionList& flexItemsPositionList, LinesCrossPositionList& flexLinesCrossPositionList, LinesCrossSizeList& flexLinesCrossSizeList, LayoutUnit crossContentExtent);
    void handleCrossAxisAlignmentForFlexItems(const FlexLines&, FlexLayoutItems&, const SizeList& flexItemsCrossSizeList, const LinesCrossSizeList& flexLinesCrossSizeList, PositionList& flexItemsPositionList);
    void performBaselineAlignment(WTF::Range<size_t> lineRange, FlexLayoutItems&, Vector<LayoutUnit>& flexItemsCrossOffsetList, const SizeList& flexItemsCrossSizeList, LayoutUnit lineCrossAxisExtent);
    void computeFlexItemRects(const FlexLines&, FlexLayoutItems&, const PositionList& flexItemsPositionList, const LinesCrossPositionList& flexLinesCrossPositionList, const LinesCrossSizeList& flexLinesCrossSizeList, const SizeList& flexItemsCrossSizeList, LayoutUnit crossAxisStartEdge, LayoutUnit crossContentExtent, LayoutUnit crossExtent);

    LayoutUnit placeFlexItems(LayoutUnit crossAxisOffset, std::span<FlexLayoutItem>, std::span<LayoutPoint> positions, LayoutUnit availableFreeSpace);
    void reverseColumnLinesFromContainerMainEndIfNeeded(const FlexLines&, FlexLayoutItems&, const SizeList& flexItemsMainSizeList, PositionList& flexItemsPositionList, const LinesCrossPositionList& flexLinesCrossPositionList, LayoutUnit containerMainBlockContentExtent, LayoutUnit containerMainBorderBoxExtent);
    void layoutColumnReverse(std::span<FlexLayoutItem>, std::span<LayoutPoint> positions, LayoutUnit crossAxisOffset, LayoutUnit availableFreeSpace, LayoutUnit columnMainBorderBoxExtent);
    void setFlexItemCountsForFirstAndLastLine(const FlexLines&);

    FlexBaseAndHypotheticalMainSizeList computeFlexBaseAndHypotheticalMainSizes(FlexLayoutItems&);
    LayoutUnit flexBaseSizeForFlexItem(const FlexLayoutItem&);
    std::optional<LayoutUnit> ensureBlockAxisContentSizeForFlexItemIfNeeded(const FlexLayoutItem&);
    std::pair<LayoutUnit, LayoutUnit> minMaxMainSizesForFlexItem(const FlexLayoutItem&);
    std::optional<LayoutUnit> computeUsedMaxMainSize(const FlexLayoutItem&);
    LayoutUnit computeUsedNonAutoMinMainSize(const FlexLayoutItem&, const Style::MinimumSize&);
    LayoutUnit computeContentBasedMinMainSize(const FlexLayoutItem&, std::optional<LayoutUnit> maxExtent);
    template<typename SizeType> LayoutUnit computeMainSizeFromAspectRatioUsing(const FlexLayoutItem&, const SizeType& crossSizeLength) const;
    LayoutUnit adjustFlexItemSizeForAspectRatioCrossAxisMinAndMax(const FlexLayoutItem&, LayoutUnit flexItemSize);
    template<typename SizeType> bool flexItemCrossSizeIsDefinite(const FlexLayoutItem&, const SizeType&);

    bool flexItemHasComputableAspectRatioAndCrossSizeIsConsideredDefinite(const FlexLayoutItem&);

    bool canFitItemWithTrimmedMarginEnd(const FlexLayoutItem&, LayoutUnit hypotheticalMainContentSize, LayoutUnit sumHypotheticalMainSize, LayoutUnit mainAxisAvailableSpace) const;
    void removeMarginEndFromFlexSizes(FlexLayoutItem&, LayoutUnit& sumFlexBaseSize, LayoutUnit& sumHypotheticalMainSize) const;

    LayoutUnit NODELETE autoMarginOffsetInMainAxis(std::span<const FlexLayoutItem>, LayoutUnit& availableFreeSpace);

    LayoutUnit applyStretchAlignmentToFlexItem(const FlexLayoutItem&, LayoutUnit lineCrossAxisExtent, LayoutUnit crossContentExtent);
    LayoutUnit applyStretchMinMaxCrossSize(const FlexLayoutItem&, LayoutUnit lineCrossAxisExtent, LogicalBoxAxis, LayoutUnit crossContentExtent);

    const FlexFormattingUtils& flexFormattingUtils() const;
    FlexLayoutState& layoutState() const;
    const LayoutIntegration::FlexIntegrationUtils& integrationUtils() const LIFETIME_BOUND { return m_integrationUtils; }
    LayoutIntegration::FlexIntegrationUtils& integrationUtils() LIFETIME_BOUND { return m_integrationUtils; }

    const CheckedRef<const RenderFlexibleBox> m_flexBox;
    LayoutIntegration::FlexIntegrationUtils m_integrationUtils;
    FlexFormattingUtils m_flexFormattingUtils;
    const FlexLayoutConstraints m_constraints;
    Result m_result;
};

} // namespace WebCore
