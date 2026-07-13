/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/BaselineAlignment.h>
#include <WebCore/FlexLayoutUtils.h>
#include <WebCore/OrderIterator.h>
#include <WebCore/RenderBlock.h>
#include <wtf/Range.h>
#include <wtf/SetForScope.h>
#include <wtf/WeakHashSet.h>

namespace WebCore {

namespace LayoutIntegration {
class FlexLayout;
}

class RenderFlexibleBox : public RenderBlock {
    WTF_MAKE_TZONE_ALLOCATED(RenderFlexibleBox);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RenderFlexibleBox);
public:
    const FlexLayoutUtils& flexLayoutUtils() const LIFETIME_BOUND { return m_flexLayoutUtils; }

    RenderFlexibleBox(Type, Element&, Style::ComputedStyle&&);
    RenderFlexibleBox(Type, Document&, Style::ComputedStyle&&);
    virtual ~RenderFlexibleBox();

    using Direction = FlowDirection;

    ASCIILiteral renderName() const override;

    bool canDropAnonymousBlockChild() const final { return false; }
    void layoutBlock(RelayoutChildren, LayoutUnit pageLogicalHeight = 0_lu) final;

    std::optional<LayoutUnit> firstLineBaseline() const override;
    std::optional<LayoutUnit> lastLineBaseline() const override;

    void styleDidChange(Style::Difference, const Style::ComputedStyle*) override;
    bool hitTestChildren(const HitTestRequest&, HitTestResult&, const HitTestLocation&, const LayoutPoint& adjustedLocation, HitTestAction) override;
    void paintChildren(PaintInfo& forSelf, const LayoutPoint&, PaintInfo& forChild, bool usePrintRect) override;

    bool willStretchItem(const RenderBox& item, LogicalBoxAxis containingAxis, StretchingMode = StretchingMode::Normal) const override;

    const OrderIterator& orderIterator() const LIFETIME_BOUND { return m_orderIterator; }

    LayoutOptionalOutsets allowedLayoutOverflow() const override;

    virtual bool isFlexibleBoxImpl() const { return false; };

    std::optional<LayoutUnit> usedFlexItemOverridingLogicalHeightForPercentageResolution(const RenderBox&);
    bool canUseFlexItemForPercentageResolution(const RenderBox&);

    void invalidateBlockAxisSizeForFlexItem(const RenderBox& flexItem);
    void flexItemWillBeRemoved(const RenderBox& flexItem);

    LayoutUnit flexItemContentLogicalHeight(const RenderBox& flexItem) const;
    void setFlexItemContentLogicalHeightIfNeeded(const RenderBox& flexItem, LayoutUnit height);

    LayoutUnit staticMainAxisPositionForPositionedFlexItem(const RenderBox&);
    LayoutUnit staticCrossAxisPositionForPositionedFlexItem(const RenderBox&);

    LayoutUnit staticInlinePositionForPositionedFlexItem(const RenderBox&);
    LayoutUnit staticBlockPositionForPositionedFlexItem(const RenderBox&);

    // Returns true if the position changed. In that case, the flexItem will have to
    // be laid out again.
    bool setStaticPositionForPositionedLayout(const RenderBox&);

    enum class GapType : uint8_t { BetweenLines, BetweenItems };
    LayoutUnit computeGap(GapType) const;

    bool isComputingFlexBaseSizes() const { return m_isComputingFlexBaseSizes; }

    bool hasModernLayout() const { return m_hasFlexFormattingContextLayout && *m_hasFlexFormattingContextLayout; }

    bool shouldResetFlexItemLogicalHeightBeforeLayout() const { return m_shouldResetFlexItemLogicalHeightBeforeLayout; }
    bool isInCrossAxisStretchLayout() const { return m_inLayout && m_afterCrossAxisItemSizing; }

    class OverridingSizesScope {
    public:
        enum class Axis { Inline, Block, Both };

        OverridingSizesScope(RenderBox&, Axis, std::optional<LayoutUnit> size = std::nullopt);
        ~OverridingSizesScope();

    private:
        RenderBox& m_box;
        Axis m_axis;
        std::optional<LayoutUnit> m_previousOverridingBorderBoxLogicalWidth;
        std::optional<LayoutUnit> m_previousOverridingBorderBoxLogicalHeight;
    };

    class ScopedCrossAxisOverrideForFlexItem {
    public:
        enum class InvalidateContentWidths : bool { No, Yes };
        ScopedCrossAxisOverrideForFlexItem(const RenderFlexibleBox&, RenderBox& flexItem, InvalidateContentWidths);
        ~ScopedCrossAxisOverrideForFlexItem();

    private:
        SetForScope<bool> m_intrinsicWidthComputation;
        std::optional<OverridingSizesScope> m_overridingScope;
#if ASSERT_ENABLED
        RenderBox& m_flexItem;
        bool m_didInvalidateContentLogicalWidths { false };
#endif
    };

protected:
    std::pair<LayoutUnit, LayoutUnit> computeIntrinsicLogicalWidths() const override;

private:
    friend class FlexLayoutUtils;

    struct FlexBaseAndHypotheticalMainSize {
        LayoutUnit flexBaseContentSize;
        LayoutUnit hypotheticalMainContentSize;
        std::pair<LayoutUnit, LayoutUnit> minMaxMainSizes;
    };

    class FlexLayoutItem {
    public:
        FlexLayoutItem(RenderBox&, bool everHadLayout);

        LayoutUnit NODELETE hypotheticalMainAxisMarginBoxSize(LayoutUnit hypotheticalMainContentSize, LayoutUnit mainAxisMargin) const;
        LayoutUnit NODELETE flexBaseMarginBoxSize(LayoutUnit flexBaseContentSize, LayoutUnit mainAxisMargin) const;
        LayoutUnit NODELETE flexedMarginBoxSize(LayoutUnit mainSize, LayoutUnit mainAxisMargin) const;
        const Style::ComputedStyle& NODELETE style() const LIFETIME_BOUND;

        CheckedRef<RenderBox> renderer;
        const LayoutUnit mainAxisBorderAndPadding;
        bool everHadLayout { false };
    };

    enum class FlexSign : uint8_t {
        PositiveFlexibility,
        NegativeFlexibility,
    };

    enum class SizeDefiniteness : uint8_t { Definite, Indefinite, Unknown };

    static constexpr unsigned s_flexLayoutItemsInitialCapacity = 4;
    using FlexItemBorderBoxRects = Vector<LayoutRect, s_flexLayoutItemsInitialCapacity>;
    using FlexLayoutItems = Vector<FlexLayoutItem, s_flexLayoutItemsInitialCapacity>;
    using FlexBaseAndHypotheticalMainSizeList = Vector<FlexBaseAndHypotheticalMainSize, s_flexLayoutItemsInitialCapacity>;
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

    void performFlexLayout(RelayoutChildren);
    FlexLayoutItems collectFlexItems(RelayoutChildren, FlexBaseAndHypotheticalMainSizeList& sizingList);
    FlexLines computeFlexLines(FlexLayoutItems& flexItems, std::span<const FlexBaseAndHypotheticalMainSize> sizingList, std::span<LayoutUnit> mainAxisMargins);
    // Resolves each flex item's flexed main size (spec 9.7) for every line, and returns the used main size of each item.
    Vector<LayoutUnit> computeMainSizeForFlexItems(FlexLayoutItems& flexItems, const FlexLines&, std::span<const FlexBaseAndHypotheticalMainSize> sizingList, std::span<const LayoutUnit> mainAxisMargins, LayoutUnit gapBetweenItems);
    LayoutUnit resolveFlexibleLengthsForLineItems(std::span<FlexLayoutItem>, std::span<const FlexBaseAndHypotheticalMainSize> lineSizing, std::span<LayoutUnit> mainSizes, std::span<const LayoutUnit> margins, LayoutUnit containerMainInnerSize, LayoutUnit gapBetweenItems);
    void distributeMainAxisFreeSpaceForMultilineColumnIfNeeded(const FlexLines&, FlexLayoutItems&, std::span<const FlexBaseAndHypotheticalMainSize> sizingList, Vector<LayoutUnit>& mainSizeList, const Vector<LayoutUnit>& marginsList, Vector<LayoutPoint>& positionList, const Vector<LayoutUnit>& lineCrossOffsetList, LayoutUnit gapBetweenItems);
    // Trims the cross-axis margins of the items on the first and last flex line (must run before laying the items out).
    void trimCrossAxisMarginsForFlexItems(FlexLayoutItems& flexItems, const FlexLines&);
    // Lays out each flex item at its resolved main size.
    void layoutFlexItems(std::span<FlexLayoutItem>, std::span<const LayoutUnit> mainSizes, RelayoutChildren);
    void layoutFlexItemAfterMainSizing(FlexLayoutItem&, LayoutUnit mainSize, RelayoutChildren);
    Vector<LayoutUnit> hypotheticalCrossSizeForFlexItems(const FlexLayoutItems&);
    Vector<LayoutUnit> crossSizeForFlexLines(const FlexLines&, const FlexLayoutItems&, const Vector<LayoutUnit>& hypotheticalCrossSizeList);
    Vector<LayoutPoint> handleMainAxisAlignment(const FlexLines&, FlexLayoutItems&, const Vector<LayoutUnit>& mainSizeList, const Vector<LayoutUnit>& marginsList, const Vector<LayoutUnit>& lineCrossOffsetList, LayoutUnit gapBetweenItems);
    Vector<LayoutUnit> computeCrossSizeForFlexItems(const FlexLines&, FlexLayoutItems&, const Vector<LayoutUnit>& lineCrossSizeList);
    void handleCrossAxisAlignmentForFlexLines(const FlexLines&, Vector<LayoutPoint>& positionList, Vector<LayoutUnit>& lineCrossOffsetList, Vector<LayoutUnit>& lineCrossSizeList, LayoutUnit gapBetweenLines);
    void handleCrossAxisAlignmentForFlexItems(const FlexLines&, FlexLayoutItems&, const Vector<LayoutUnit>& crossSizeList, const Vector<LayoutUnit>& lineCrossSizeList, Vector<LayoutPoint>& positionList);
    void performBaselineAlignment(WTF::Range<size_t> lineRange, FlexLayoutItems&, Vector<LayoutUnit>& crossItemOffsetList, const Vector<LayoutUnit>& crossSizeList, LayoutUnit lineCrossAxisExtent);

    void placeFlexItems(LayoutUnit crossAxisOffset, std::span<FlexLayoutItem>, std::span<LayoutPoint> positions, LayoutUnit availableFreeSpace, LayoutUnit gapBetweenItems);
    void layoutColumnReverse(std::span<FlexLayoutItem>, std::span<LayoutPoint> positions, LayoutUnit crossAxisOffset, LayoutUnit availableFreeSpace, LayoutUnit gapBetweenItems);
    void setFlexItemCountsForFirstAndLastLine(const FlexLines&);
    void adjustLogicalHeightForLineIfEmpty();

    void appendFlexItemBorderBoxRects(FlexItemBorderBoxRects&);
    void repaintFlexItemsDuringLayoutIfMoved(const FlexItemBorderBoxRects&);
    FlexBaseAndHypotheticalMainSize flexBaseAndHypotheticalMainSize(const FlexLayoutItem&);
    LayoutUnit flexBaseSizeForFlexItem(const FlexLayoutItem&);
    bool flexBaseSizeNeedsBlockAxisContentSize(const FlexLayoutItem&);
    void ensureBlockAxisContentSizeForFlexItemIfNeeded(const FlexLayoutItem&);
    std::pair<LayoutUnit, LayoutUnit> computeFlexItemMinMaxMainSizes(const FlexLayoutItem&);
    std::optional<LayoutUnit> computeUsedMaxMainSize(const FlexLayoutItem&);
    LayoutUnit computeUsedNonAutoMinMainSize(const FlexLayoutItem&, const Style::MinimumSize&);
    LayoutUnit computeContentBasedMinMainSize(const FlexLayoutItem&, std::optional<LayoutUnit> maxExtent);
    template<typename SizeType> std::optional<LayoutUnit> computeMainAxisExtentForFlexItem(RenderBox& flexItem, const SizeType&);
    template<typename SizeType> LayoutUnit computeMainSizeFromAspectRatioUsing(const RenderBox& flexItem, const SizeType& crossSizeLength) const;
    LayoutUnit adjustFlexItemSizeForAspectRatioCrossAxisMinAndMax(const RenderBox& flexItem, LayoutUnit flexItemSize);
    LayoutUnit mainAxisAvailableSpace();

    LayoutUnit crossAxisIntrinsicExtentForFlexItem(const FlexLayoutItem&);
    LayoutUnit flexItemIntrinsicLogicalHeight(RenderBox& flexItem) const;
    LayoutUnit flexItemIntrinsicLogicalWidth(RenderBox& flexItem);
    template<typename SizeType> bool canComputePercentageFlexBasis(const RenderBox& flexItem, const SizeType&, UpdatePercentageHeightDescendants);
    template<typename SizeType> bool flexItemMainSizeIsDefinite(const RenderBox&, const SizeType&);
    template<typename SizeType> bool flexItemCrossSizeIsDefinite(const RenderBox&, const SizeType&);

    bool flexItemHasComputableAspectRatioAndCrossSizeIsConsideredDefinite(const RenderBox&);

    void initializeMarginTrimState();
    void trimMainAxisMarginStart(const FlexLayoutItem&, LayoutUnit& mainAxisMargin);
    void trimMainAxisMarginEnd(const FlexLayoutItem&, LayoutUnit& mainAxisMargin);
    void trimCrossAxisMarginStart(const FlexLayoutItem&);
    void trimCrossAxisMarginEnd(const FlexLayoutItem&);
    bool isChildEligibleForMarginTrim(Style::MarginTrimSide, const RenderBox&) const final;
    bool canFitItemWithTrimmedMarginEnd(const FlexLayoutItem&, LayoutUnit hypotheticalMainContentSize, LayoutUnit mainAxisMargin, LayoutUnit sumHypotheticalMainSize, LayoutUnit mainAxisAvailableSpace) const;
    void removeMarginEndFromFlexSizes(FlexLayoutItem&, LayoutUnit& sumFlexBaseSize, LayoutUnit& sumHypotheticalMainSize) const;

    LayoutUnit NODELETE autoMarginOffsetInMainAxis(std::span<const FlexLayoutItem>, LayoutUnit& availableFreeSpace);
    void NODELETE updateAutoMarginsInMainAxis(RenderBox& flexItem, LayoutUnit autoMarginOffset);

    bool NODELETE updateAutoMarginsInCrossAxis(FlexLayoutItem&, LayoutUnit& crossOffset, LayoutUnit availableAlignmentSpace);
    LayoutUnit applyStretchAlignmentToFlexItem(const FlexLayoutItem&, LayoutUnit lineCrossAxisExtent);
    LayoutUnit applyStretchMinMaxCrossSize(const FlexLayoutItem&, LayoutUnit lineCrossAxisExtent, LogicalBoxAxis);
    void setOverridingMainSizeForFlexItem(RenderBox&, LayoutUnit);

    void clearFlexItemOverridingSizes();

    void resetAutoMarginsAndLogicalTopInCrossAxis(RenderBox& flexItem);
    void NODELETE setFlowAwareLocationForFlexItem(RenderBox& flexItem, const LayoutPoint&);
    void setFlexItemGeometry(FlexLayoutItem&, const LayoutPoint& location);
    const RenderBox* flexItemForFirstBaseline() const;
    const RenderBox* flexItemForLastBaseline() const;
    const RenderBox* firstBaselineCandidateOnLine(OrderIterator, size_t numberOfItemsOnLine) const;
    const RenderBox* lastBaselineCandidateOnLine(OrderIterator, size_t numberOfItemsOnLine) const;

    bool flexItemHasPercentHeightDescendants(const RenderBox&) const;
    void dirtyPercentHeightDescendantsWithinFlexItem(RenderBox&);
    void prepareFlexItemForPositionedLayout(RenderBox& flexItem);

    void prepareOrderIteratorAndMargins();

    void resetHasDefiniteHeight() { m_hasDefiniteHeight = SizeDefiniteness::Unknown; }

    bool layoutUsingFlexFormattingContext();

    // Inner main size for flex items where main axis is the item's block axis (column flex or orthogonal).
    HashMap<SingleThreadWeakRef<const RenderBox>, LayoutUnit> m_blockAxisSize;

    // This is used to cache the intrinsic size on the cross axis to avoid
    // relayouts when stretching.
    HashMap<SingleThreadWeakRef<const RenderBox>, LayoutUnit> m_contentLogicalHeights;

    // This set is used to keep track of which children we laid out in this
    // current layout iteration. We need it because the ones in this set may
    // need an additional layout pass for correct stretch alignment handling, as
    // the first layout likely did not use the correct value for percentage
    // sizing of children.
    SingleThreadWeakHashSet<const RenderBox> m_flexItemsWithCompletedLayout;

    mutable OrderIterator m_orderIterator { *this };
    const FlexLayoutUtils m_flexLayoutUtils { *this };
    size_t m_numberOfFlexItemsOnFirstLine { 0 };
    size_t m_numberOfFlexItemsOnLastLine { 0 };

    struct MarginTrimItems {
        SingleThreadWeakHashSet<const RenderBox> m_itemsAtFlexLineStart;
        SingleThreadWeakHashSet<const RenderBox> m_itemsAtFlexLineEnd;
        SingleThreadWeakHashSet<const RenderBox> m_itemsOnFirstFlexLine;
        SingleThreadWeakHashSet<const RenderBox> m_itemsOnLastFlexLine;
    } m_marginTrimItems;

    LayoutUnit m_alignContentStartOverflow { 0 };
    LayoutUnit m_justifyContentStartOverflow { 0 };

    // This is SizeIsUnknown outside of layoutBlock()
    SizeDefiniteness m_hasDefiniteHeight { SizeDefiniteness::Unknown };
    bool m_inLayout { false };
    bool m_afterMainAxisItemSizing { false };
    bool m_afterCrossAxisItemSizing { false };
    bool m_inSimplifiedLayout { false };
    bool m_inPostFlexUpdateScrollbarLayout { false };
    mutable bool m_inFlexItemIntrinsicWidthComputation { false };
    bool m_shouldResetFlexItemLogicalHeightBeforeLayout { false };
    bool m_isComputingFlexBaseSizes { false };
    std::optional<bool> m_hasFlexFormattingContextLayout;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderFlexibleBox, isRenderFlexibleBox())
