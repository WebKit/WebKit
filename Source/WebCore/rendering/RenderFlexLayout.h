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

#include <WebCore/RenderBlock.h>
#include <wtf/Range.h>
#include <wtf/WeakHashSet.h>

namespace WebCore {

namespace Style {
class ComputedStyle;
struct MinimumSize;
}

class FlexLayoutUtils;
class RenderFlexibleBox;

// A flex item during layout: the live renderer plus geometry snapshotted from it up front. RenderFlexibleBox
// builds the list of these (its formatting-context "children") and hands it to FlexLayout, mirroring how the
// LFC integration builds LogicalFlexItems for Layout::FlexLayout.
class FlexLayoutItem {
public:
    FlexLayoutItem(RenderBox&, bool everHadLayout, std::optional<LayoutUnit> cachedBlockAxisContentSize);

    LayoutUnit NODELETE hypotheticalMainAxisMarginBoxSize(LayoutUnit hypotheticalMainContentSize) const;
    LayoutUnit NODELETE flexBaseMarginBoxSize(LayoutUnit flexBaseContentSize) const;
    LayoutUnit NODELETE flexedMarginBoxSize(LayoutUnit mainSize) const;
    const Style::ComputedStyle& NODELETE style() const LIFETIME_BOUND;

    CheckedRef<RenderBox> renderer;
    const LayoutUnit mainAxisBorderAndPadding;
    const LayoutUnit crossAxisBorderAndPadding;
    // True when the flex container's main axis is this item's inline axis (i.e. the item is not orthogonal to the container).
    const bool mainAxisIsInlineAxis;
    // The item's main-axis margin extent. Snapshotted in FlexLayout::performFlexLayout after the item is laid out for
    // its flex base size (an orthogonal item only resolves its physical margins then), not at construction time.
    // margin-trim reduces it during line collection.
    LayoutUnit mainAxisMargin;
    bool everHadLayout { false };
    // The item's block-axis content size that RenderFlexibleBox cached in a previous layout (nullopt if none or
    // invalidated); flex-base sizing reuses it to skip re-laying-out the item while it stays clean.
    const std::optional<LayoutUnit> cachedBlockAxisContentSize;
};

using FlexLayoutItems = Vector<FlexLayoutItem, 4>;

// The flex container's flow properties and fixed border/padding, snapshotted from the formatting-context root
// (RenderFlexibleBox) and handed to FlexLayout so the pipeline reads them here rather than reaching back through
// the container. The available cross/main sizes are deliberately not included: the container's cross size grows
// during layout, so those stay live reads for now.
struct FlexLayoutConstraints {
    // The formatting-context root's computed style (the flex container's own style).
    const Style::ComputedStyle& style;
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
    // The minimum logical height the container needs when it has a line even while empty (nullopt when it does not).
    std::optional<LayoutUnit> minimumHeightForLineIfEmpty;
};

// The layout logic for a legacy flex container, factored out of RenderFlexibleBox (mirrors the
// LFC Layout::FlexLayout that drives the flex formatting context). It runs the CSS Flexbox
// https://www.w3.org/TR/css-flexbox-1/ pipeline, reaching the flex container and its items
// through the container reference; RenderFlexibleBox owns one per layout and befriends it so the
// pipeline can reach the container's layout-phase state and geometry.
class FlexLayout {
public:
    FlexLayout(RenderFlexibleBox&, const FlexLayoutConstraints&);

    // What the layout produces for RenderFlexibleBox to store: the align/justify content start overflow
    // that allowedLayoutOverflow reports, and the first/last line item counts the baseline uses.
    // alignContentStartOverflow is nullopt when this layout did not run align-content, so the container
    // keeps its previous value (matching the legacy behavior where only justify was reset each layout).
    struct Result {
        std::optional<LayoutUnit> alignContentStartOverflow;
        LayoutUnit justifyContentStartOverflow;
        size_t numberOfFlexItemsOnFirstLine { 0 };
        size_t numberOfFlexItemsOnLastLine { 0 };
        // The block-axis content size computed for each flex item (re)laid out this layout, for RenderFlexibleBox
        // to cache on itself so a subsequent layout can skip re-laying-out that item while it stays clean.
        Vector<std::pair<CheckedRef<const RenderBox>, LayoutUnit>> blockAxisContentSizes;
    };
    Result performFlexLayout(FlexLayoutItems&, RelayoutChildren);

private:
    struct FlexBaseAndHypotheticalMainSize {
        LayoutUnit flexBaseContentSize;
        LayoutUnit hypotheticalMainContentSize;
        std::pair<LayoutUnit, LayoutUnit> minMaxMainSizes;
    };

    enum class FlexSign : uint8_t {
        PositiveFlexibility,
        NegativeFlexibility,
    };

    using FlexBaseAndHypotheticalMainSizeList = Vector<FlexBaseAndHypotheticalMainSize, 4>;
    struct FlexLines;

    // A flex line is a contiguous range of flexItems. computeFlexLines collects every line's range upfront
    // plus the per-line hypothetical main size that columnInnerMainSize needs.
    using LineRanges = Vector<WTF::Range<size_t>>;
    struct FlexLines {
        LineRanges ranges;
        Vector<LayoutUnit> hypotheticalMainSizes;
    };

    // A baseline-sharing group's members (indices into the owning line's items) and the ascent they align
    // to. Flex owns this because it positions each member within the group; the shared BaselineAlignmentState
    // only decides grouping.
    struct BaselineSharingGroup {
        LayoutUnit maxAscent;
        Vector<size_t> items;
    };
    // A line almost always has a single baseline-sharing group (at most 3 can exist), so keep one inline.
    using BaselineSharingGroups = Vector<BaselineSharingGroup, 1>;

    FlexLines computeFlexLines(FlexLayoutItems& flexItems, std::span<const FlexBaseAndHypotheticalMainSize> sizingList);
    // Resolves each flex item's flexed main size (spec 9.7) for every line, and returns the used main size of each item.
    Vector<LayoutUnit> computeMainSizeForFlexItems(FlexLayoutItems& flexItems, const FlexLines&, std::span<const FlexBaseAndHypotheticalMainSize> sizingList, LayoutUnit gapBetweenItems);
    LayoutUnit resolveFlexibleLengthsForLineItems(std::span<FlexLayoutItem>, std::span<const FlexBaseAndHypotheticalMainSize> lineSizing, std::span<LayoutUnit> mainSizes, LayoutUnit containerMainInnerSize, LayoutUnit gapBetweenItems);
    void distributeMainAxisFreeSpaceForMultilineColumnIfNeeded(const FlexLines&, FlexLayoutItems&, std::span<const FlexBaseAndHypotheticalMainSize> sizingList, Vector<LayoutUnit>& mainSizeList, Vector<LayoutPoint>& positionList, const Vector<LayoutUnit>& lineCrossOffsetList, LayoutUnit gapBetweenItems);
    // Trims the cross-axis margins of the items on the first and last flex line (must run before laying the items out).
    void trimCrossAxisMarginsForFlexItems(FlexLayoutItems& flexItems, const FlexLines&);
    // Lays out each flex item at its resolved main size.
    void layoutFlexItems(std::span<FlexLayoutItem>, std::span<const LayoutUnit> mainSizes, RelayoutChildren);
    void layoutFlexItemAfterMainSizing(FlexLayoutItem&, LayoutUnit mainSize, RelayoutChildren);
    Vector<LayoutUnit> hypotheticalCrossSizeForFlexItems(const FlexLayoutItems&);
    Vector<LayoutUnit> crossSizeForFlexLines(const FlexLines&, const FlexLayoutItems&, const Vector<LayoutUnit>& hypotheticalCrossSizeList);
    Vector<LayoutPoint> handleMainAxisAlignment(const FlexLines&, FlexLayoutItems&, const Vector<LayoutUnit>& mainSizeList, const Vector<LayoutUnit>& lineCrossOffsetList, LayoutUnit gapBetweenItems);
    Vector<LayoutUnit> computeCrossSizeForFlexItems(const FlexLines&, FlexLayoutItems&, const Vector<LayoutUnit>& lineCrossSizeList);
    void handleCrossAxisAlignmentForFlexLines(const FlexLines&, Vector<LayoutPoint>& positionList, Vector<LayoutUnit>& lineCrossOffsetList, Vector<LayoutUnit>& lineCrossSizeList, LayoutUnit gapBetweenLines);
    void handleCrossAxisAlignmentForFlexItems(const FlexLines&, FlexLayoutItems&, const Vector<LayoutUnit>& crossSizeList, const Vector<LayoutUnit>& lineCrossSizeList, Vector<LayoutPoint>& positionList);
    void performBaselineAlignment(WTF::Range<size_t> lineRange, FlexLayoutItems&, Vector<LayoutUnit>& crossItemOffsetList, const Vector<LayoutUnit>& crossSizeList, LayoutUnit lineCrossAxisExtent);

    void placeFlexItems(LayoutUnit crossAxisOffset, std::span<FlexLayoutItem>, std::span<LayoutPoint> positions, LayoutUnit availableFreeSpace, LayoutUnit gapBetweenItems);
    void layoutColumnReverse(std::span<FlexLayoutItem>, std::span<LayoutPoint> positions, LayoutUnit crossAxisOffset, LayoutUnit availableFreeSpace, LayoutUnit gapBetweenItems);
    void setFlexItemCountsForFirstAndLastLine(const FlexLines&);

    FlexBaseAndHypotheticalMainSize flexBaseAndHypotheticalMainSize(const FlexLayoutItem&);
    LayoutUnit flexBaseSizeForFlexItem(const FlexLayoutItem&);
    bool flexBaseSizeNeedsBlockAxisContentSize(const FlexLayoutItem&);
    std::optional<LayoutUnit> ensureBlockAxisContentSizeForFlexItemIfNeeded(const FlexLayoutItem&);
    std::pair<LayoutUnit, LayoutUnit> computeFlexItemMinMaxMainSizes(const FlexLayoutItem&);
    std::optional<LayoutUnit> computeUsedMaxMainSize(const FlexLayoutItem&);
    LayoutUnit computeUsedNonAutoMinMainSize(const FlexLayoutItem&, const Style::MinimumSize&);
    LayoutUnit computeContentBasedMinMainSize(const FlexLayoutItem&, std::optional<LayoutUnit> maxExtent);
    template<typename SizeType> std::optional<LayoutUnit> computeMainAxisExtentForFlexItem(const FlexLayoutItem&, const SizeType&);
    template<typename SizeType> LayoutUnit computeMainSizeFromAspectRatioUsing(const FlexLayoutItem&, const SizeType& crossSizeLength) const;
    LayoutUnit adjustFlexItemSizeForAspectRatioCrossAxisMinAndMax(const FlexLayoutItem&, LayoutUnit flexItemSize);
    LayoutUnit mainAxisAvailableSpace();

    LayoutUnit crossAxisIntrinsicExtentForFlexItem(const FlexLayoutItem&);
    LayoutUnit flexItemIntrinsicLogicalHeight(const FlexLayoutItem&) const;
    LayoutUnit flexItemIntrinsicLogicalWidth(const FlexLayoutItem&);
    template<typename SizeType> bool flexItemCrossSizeIsDefinite(const FlexLayoutItem&, const SizeType&);

    bool flexItemHasComputableAspectRatioAndCrossSizeIsConsideredDefinite(const FlexLayoutItem&);

    void trimMainAxisMarginStart(FlexLayoutItem&);
    void trimMainAxisMarginEnd(FlexLayoutItem&);
    void trimCrossAxisMarginStart(const FlexLayoutItem&);
    void trimCrossAxisMarginEnd(const FlexLayoutItem&);
    bool canFitItemWithTrimmedMarginEnd(const FlexLayoutItem&, LayoutUnit hypotheticalMainContentSize, LayoutUnit sumHypotheticalMainSize, LayoutUnit mainAxisAvailableSpace) const;
    void removeMarginEndFromFlexSizes(FlexLayoutItem&, LayoutUnit& sumFlexBaseSize, LayoutUnit& sumHypotheticalMainSize) const;

    LayoutUnit NODELETE autoMarginOffsetInMainAxis(std::span<const FlexLayoutItem>, LayoutUnit& availableFreeSpace);
    void NODELETE updateAutoMarginsInMainAxis(RenderBox& flexItem, LayoutUnit autoMarginOffset);

    bool NODELETE updateAutoMarginsInCrossAxis(FlexLayoutItem&, LayoutUnit& crossOffset, LayoutUnit availableAlignmentSpace);
    LayoutUnit applyStretchAlignmentToFlexItem(const FlexLayoutItem&, LayoutUnit lineCrossAxisExtent);
    LayoutUnit applyStretchMinMaxCrossSize(const FlexLayoutItem&, LayoutUnit lineCrossAxisExtent, LogicalBoxAxis);
    void setOverridingMainSizeForFlexItem(RenderBox&, LayoutUnit);

    void resetAutoMarginsAndLogicalTopInCrossAxis(RenderBox& flexItem);
    void NODELETE setFlowAwareLocationForFlexItem(RenderBox& flexItem, const LayoutPoint&);
    void setFlexItemGeometry(FlexLayoutItem&, const LayoutPoint& location);

    bool flexItemHasPercentHeightDescendants(const RenderBox&) const;
    void dirtyPercentHeightDescendantsWithinFlexItem(RenderBox&);

    const FlexLayoutUtils& flexLayoutUtils() const;

    RenderFlexibleBox& m_flexBox;
    const FlexLayoutConstraints m_constraints;
    Result m_result;
    // The flex items laid out during this layout iteration; some may need a second layout pass for stretch
    // alignment (the first pass likely did not use the right value for percentage sizing of their children).
    SingleThreadWeakHashSet<const RenderBox> m_flexItemsWithCompletedLayout;
};

} // namespace WebCore
