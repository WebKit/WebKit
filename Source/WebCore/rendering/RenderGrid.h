/*
 * Copyright (C) 2011, 2022 Apple Inc. All rights reserved.
 * Copyright (C) 2013-2017 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "Grid.h"
#include "GridMasonryLayout.h"
#include "GridTrackSizingAlgorithm.h"
#include "RenderBlock.h"
#include "StyleGridTrackSizingDirection.h"
#include <wtf/TZoneMalloc.h>

namespace WTF {
template<typename T>
class Range;
}

namespace WebCore {

namespace LayoutIntegration {
class GridLayout;
}

class GridArea;
class GridLayoutState;
class GridSpan;
class LayoutRange;

struct ContentAlignmentData {
    LayoutUnit positionOffset;
    LayoutUnit distributionOffset;
};

enum class GridAxisPosition : uint8_t { GridAxisStart, GridAxisEnd, GridAxisCenter };

enum class SubgridDidChange : bool { No, Yes };

class GridItemSizeCache {
public:
    void setSizeForGridItem(const RenderBox& gridItem, LayoutUnit size);
    std::optional<LayoutUnit> sizeForItem(const RenderBox& gridItem) const;
    void invalidateSizeForItem(const RenderBox& gridItem);

private:
    SingleThreadWeakHashMap<const RenderBox, std::optional<LayoutUnit>> m_sizes;
};

class RenderGrid final : public RenderBlock {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(RenderGrid);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RenderGrid);
public:
    RenderGrid(Element&, RenderStyle&&);
    virtual ~RenderGrid();

    Element& element() const { return downcast<Element>(nodeForNonAnonymous()); }

    void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;
    void layoutBlock(RelayoutChildren, LayoutUnit pageLogicalHeight = 0_lu) override;

    bool canDropAnonymousBlockChild() const override { return false; }

    bool isComputingTrackSizes() const { return m_isComputingTrackSizes; }

    bool hasDefiniteLogicalHeight() const;
    const std::optional<LayoutUnit> availableLogicalHeightForContentBox() const;

    void setNeedsItemPlacement(SubgridDidChange descendantSubgridsNeedItemPlacement = SubgridDidChange::No);
    Vector<LayoutUnit> trackSizesForComputedStyle(Style::GridTrackSizingDirection) const;

    const Vector<LayoutUnit>& columnPositions() const { return m_columnPositions; }
    const Vector<LayoutUnit>& rowPositions() const { return m_rowPositions; }
    const Vector<LayoutUnit>& positions(Style::GridTrackSizingDirection direction) const { return direction == Style::GridTrackSizingDirection::Columns ? columnPositions() : rowPositions(); }

    unsigned autoRepeatCountForDirection(Style::GridTrackSizingDirection direction) const { return currentGrid().autoRepeatTracks(direction); }
    unsigned explicitGridStartForDirection(Style::GridTrackSizingDirection direction) const { return currentGrid().explicitGridStart(direction); }

    // Required by GridTrackSizingAlgorithm. Keep them under control.
    LayoutUnit guttersSize(Style::GridTrackSizingDirection, unsigned startLine, unsigned span, std::optional<LayoutUnit> availableSize) const;
    LayoutUnit gridItemOffset(Style::GridTrackSizingDirection) const;

    std::optional<LayoutUnit> explicitIntrinsicInnerLogicalSize(Style::GridTrackSizingDirection) const;
    void updateGridAreaLogicalSize(RenderBox&, std::optional<LayoutUnit> width, std::optional<LayoutUnit> height) const;
    bool isBaselineAlignmentForGridItem(const RenderBox&) const;
    bool isBaselineAlignmentForGridItem(const RenderBox& gridItem, Style::GridTrackSizingDirection alignmentContext) const;

    StyleSelfAlignmentData selfAlignmentForGridItem(Style::GridTrackSizingDirection alignmentContext, const RenderBox&, const RenderStyle* = nullptr) const;

    StyleContentAlignmentData contentAlignment(Style::GridTrackSizingDirection) const;

    // These functions handle the actual implementation of layoutBlock based on if
    // the grid is a standard grid or a masonry one. While masonry is an extension of grid,
    // keeping the logic in the same function was leading to a messy amount of if statements being added to handle
    // specific masonry cases.
    void layoutGrid(RelayoutChildren);
    void layoutMasonry(RelayoutChildren);

    // Computes the span relative to this RenderGrid, even if the RenderBox is a grid item
    // of a descendant subgrid.
    GridSpan gridSpanForGridItem(const RenderBox&, Style::GridTrackSizingDirection) const;

    bool isSubgrid() const;
    bool isSubgrid(Style::GridTrackSizingDirection) const;
    bool isSubgridRows() const { return isSubgrid(Style::GridTrackSizingDirection::Rows); }
    bool isSubgridColumns() const { return isSubgrid(Style::GridTrackSizingDirection::Columns); }
    bool isSubgridInParentDirection(Style::GridTrackSizingDirection parentDirection) const;

    // Returns true if this grid is inheriting subgridded tracks for
    // the given direction from the specified ancestor. This handles
    // nested subgrids, where ancestor may not be our direct parent.
    bool isSubgridOf(Style::GridTrackSizingDirection, const RenderGrid& ancestor) const;

    bool isMasonry() const;
    bool isMasonry(Style::GridTrackSizingDirection) const;
    bool areMasonryRows() const { return isMasonry(Style::GridTrackSizingDirection::Rows); }
    bool areMasonryColumns() const { return isMasonry(Style::GridTrackSizingDirection::Columns); }

    const Grid& currentGrid() const;
    Grid& currentGrid();

    unsigned numTracks(Style::GridTrackSizingDirection) const;

    void placeItems();

    std::optional<LayoutUnit> availableSpaceForGutters(Style::GridTrackSizingDirection) const;
    LayoutUnit gridGap(Style::GridTrackSizingDirection) const;
    LayoutUnit gridGap(Style::GridTrackSizingDirection, std::optional<LayoutUnit> availableSize) const;

    LayoutUnit masonryContentSize() const;

    void updateIntrinsicLogicalHeightsForRowSizingFirstPassCacheAvailability();
    std::optional<GridItemSizeCache>& intrinsicLogicalHeightsForRowSizingFirstPass() const;

    bool shouldCheckExplicitIntrinsicInnerLogicalSize(Style::GridTrackSizingDirection) const;

    // Checks both the grid container and the grid since the grid container is sized
    // according to the rules of the formatting context it lives in while the size of the
    // grid is determined by the lines/grid areas which come from track sizing.
    bool isExtrinsicallySized() const;

private:
    friend class GridTrackSizingAlgorithm;
    friend class GridTrackSizingAlgorithmStrategy;
    friend class GridMasonryLayout;
    friend class PositionedLayoutConstraints;

    void computeLayoutRequirementsForItemsBeforeLayout(GridLayoutState&) const;
    bool canSetColumnAxisStretchRequirementForItem(const RenderBox&) const;

    ItemPosition selfAlignmentNormalBehavior(const RenderBox* gridItem = nullptr) const override
    {
        ASSERT(gridItem);
        return gridItem->isRenderReplaced() ? ItemPosition::Start : ItemPosition::Stretch;
    }

    ASCIILiteral renderName() const override;
    void computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const override;

    bool selfAlignmentChangedToStretch(Style::GridTrackSizingDirection alignmentContext, const RenderStyle& oldStyle, const RenderStyle& newStyle, const RenderBox&) const;
    bool selfAlignmentChangedFromStretch(Style::GridTrackSizingDirection alignmentContext, const RenderStyle& oldStyle, const RenderStyle& newStyle, const RenderBox&) const;

    SubgridDidChange subgridDidChange(const RenderStyle& oldStyle) const;
    bool explicitGridDidResize(const RenderStyle&) const;
    bool namedGridLinesDefinitionDidChange(const RenderStyle&) const;
    bool implicitGridLinesDefinitionDidChange(const RenderStyle&) const;

    unsigned computeAutoRepeatTracksCount(Style::GridTrackSizingDirection, std::optional<LayoutUnit> availableSize) const;

    unsigned clampAutoRepeatTracks(Style::GridTrackSizingDirection, unsigned autoRepeatTracks) const;

    WTF::Range<size_t> autoRepeatTracksRange(Style::GridTrackSizingDirection) const;
    std::unique_ptr<OrderedTrackIndexSet> computeEmptyTracksForAutoRepeat(Style::GridTrackSizingDirection) const;

    enum class ShouldUpdateGridAreaLogicalSize : bool { No, Yes };
    void performPreLayoutForGridItems(const GridTrackSizingAlgorithm&, const ShouldUpdateGridAreaLogicalSize) const;

    void placeItemsOnGrid(std::optional<LayoutUnit> availableLogicalWidth);
    void populateExplicitGridAndOrderIterator();
    GridArea createEmptyGridAreaAtSpecifiedPositionsOutsideGrid(const RenderBox&, Style::GridTrackSizingDirection, const GridSpan&) const;
    bool isPlacedWithinExtrinsicallySizedExplicitTracks(const RenderBox&) const;
    void placeSpecifiedMajorAxisItemsOnGrid(const Vector<RenderBox*>&);
    void placeAutoMajorAxisItemsOnGrid(const Vector<RenderBox*>&);
    typedef std::pair<unsigned, unsigned> AutoPlacementCursor;
    void placeAutoMajorAxisItemOnGrid(RenderBox&, AutoPlacementCursor&);
    Style::GridTrackSizingDirection autoPlacementMajorAxisDirection() const;
    Style::GridTrackSizingDirection autoPlacementMinorAxisDirection() const;

    static bool itemGridAreaIsWithinImplicitGrid(const GridArea& area, unsigned gridAxisLinesCount, Style::GridTrackSizingDirection gridAxisDirection)
    {
        auto itemSpan = area.span(gridAxisDirection);
        return itemSpan.startLine() < gridAxisLinesCount && itemSpan.endLine() < gridAxisLinesCount;
    }

    bool canPerformSimplifiedLayout() const final;
    void prepareGridItemForPositionedLayout(RenderBox&);
    bool hasStaticPositionForGridItem(const RenderBox&, Style::GridTrackSizingDirection) const;
    void layoutOutOfFlowBox(RenderBox&, RelayoutChildren, bool fixedPositionObjectsOnly) override;

    void computeTrackSizesForDefiniteSize(Style::GridTrackSizingDirection, LayoutUnit availableSpace, GridLayoutState&);
    void computeTrackSizesForIndefiniteSize(GridTrackSizingAlgorithm&, Style::GridTrackSizingDirection, GridLayoutState&, LayoutUnit* minIntrinsicSize = nullptr, LayoutUnit* maxIntrinsicSize = nullptr) const;
    LayoutUnit computeTrackBasedLogicalHeight() const;

    void repeatTracksSizingIfNeeded(LayoutUnit availableSpaceForColumns, LayoutUnit availableSpaceForRows, GridLayoutState&);

    void updateGridAreaForAspectRatioItems(const Vector<RenderBox*>&, GridLayoutState&);

    void layoutGridItems(GridLayoutState&);
    void layoutMasonryItems(GridLayoutState&);

    void populateGridPositionsForDirection(const GridTrackSizingAlgorithm&, Style::GridTrackSizingDirection);

    LayoutRange gridAreaRangeForOutOfFlow(const RenderBox&, Style::GridTrackSizingDirection) const;
    std::pair<LayoutUnit, LayoutUnit> gridAreaPositionForInFlowGridItem(const RenderBox&, Style::GridTrackSizingDirection) const;

    GridAxisPosition columnAxisPositionForGridItem(const RenderBox&) const;
    GridAxisPosition rowAxisPositionForGridItem(const RenderBox&) const;
    LayoutUnit columnAxisOffsetForGridItem(const RenderBox&) const;
    LayoutUnit rowAxisOffsetForGridItem(const RenderBox&) const;
    ContentAlignmentData computeContentPositionAndDistributionOffset(Style::GridTrackSizingDirection, const LayoutUnit& availableFreeSpace, unsigned numberOfGridTracks) const;
    void setLogicalPositionForGridItem(RenderBox&) const;
    LayoutUnit logicalOffsetForGridItem(const RenderBox&, Style::GridTrackSizingDirection) const;

    LayoutUnit gridAreaBreadthForGridItemIncludingAlignmentOffsets(const RenderBox&, Style::GridTrackSizingDirection) const;

    void paintChildren(PaintInfo& forSelf, const LayoutPoint& paintOffset, PaintInfo& forChild, bool usePrintRect) override;
    bool hitTestChildren(const HitTestRequest&, HitTestResult&, const HitTestLocation&, const LayoutPoint& adjustedLocation, HitTestAction) override;
    LayoutOptionalOutsets allowedLayoutOverflow() const override;
    void computeOverflow(LayoutUnit oldClientAfterEdge, OptionSet<ComputeOverflowOptions> = { }) override;

    StyleSelfAlignmentData justifySelfForGridItem(const RenderBox&, StretchingMode = StretchingMode::Any, const RenderStyle* = nullptr) const;
    StyleSelfAlignmentData alignSelfForGridItem(const RenderBox&, StretchingMode = StretchingMode::Any, const RenderStyle* = nullptr) const;
    void applyStretchAlignmentToGridItemIfNeeded(RenderBox&, GridLayoutState&);
    void applySubgridStretchAlignmentToGridItemIfNeeded(RenderBox&);
    bool isChildEligibleForMarginTrim(Style::MarginTrimSide, const RenderBox&) const final;

    std::optional<LayoutUnit> firstLineBaseline() const final;
    std::optional<LayoutUnit> lastLineBaseline() const final;
    SingleThreadWeakPtr<RenderBox> getBaselineGridItem(ItemPosition alignment) const;

    LayoutUnit columnAxisBaselineOffsetForGridItem(const RenderBox&) const;
    LayoutUnit rowAxisBaselineOffsetForGridItem(const RenderBox&) const;

    unsigned nonCollapsedTracks(Style::GridTrackSizingDirection) const;

    LayoutUnit translateRTLCoordinate(LayoutUnit) const;

    bool shouldResetLogicalHeightBeforeLayout() const override { return true; }

    bool aspectRatioPrefersInline(const RenderBox& gridItem, bool blockFlowIsColumnAxis);

    Vector<RenderBox*> computeAspectRatioDependentAndBaselineItems(GridLayoutState&);

    GridSpan gridSpanForOutOfFlowGridItem(const RenderBox&, Style::GridTrackSizingDirection) const;

    bool computeGridPositionsForOutOfFlowGridItem(const RenderBox&, Style::GridTrackSizingDirection, int&, bool&, int&, bool&) const;

    AutoRepeatType autoRepeatType(Style::GridTrackSizingDirection) const;

    Vector<LayoutUnit>& positions(Style::GridTrackSizingDirection direction) { return direction == Style::GridTrackSizingDirection::Columns ? m_columnPositions : m_rowPositions; }

    ContentAlignmentData& offsetBetweenTracks(Style::GridTrackSizingDirection direction) { return direction == Style::GridTrackSizingDirection::Columns ? m_offsetBetweenColumns : m_offsetBetweenRows; }
    const ContentAlignmentData& offsetBetweenTracks(Style::GridTrackSizingDirection direction) const { return direction == Style::GridTrackSizingDirection::Columns ? m_offsetBetweenColumns : m_offsetBetweenRows; }

    bool canCreateIntrinsicLogicalHeightsForRowSizingFirstPassCache() const;

    class GridWrapper {
        Grid m_layoutGrid;
    public:
        GridWrapper(RenderGrid&);
        void resetCurrentGrid() const;
        mutable std::reference_wrapper<Grid> m_currentGrid { std::ref(m_layoutGrid) };
    } m_grid;

    // FIXME: Refactor m_trackSizingAlgorithm to be inside of layoutGrid and layoutMasonry.
    // https://bugs.webkit.org/show_bug.cgi?id=277496
    GridTrackSizingAlgorithm m_trackSizingAlgorithm;

    Vector<LayoutUnit> m_columnPositions;
    Vector<LayoutUnit> m_rowPositions;
    ContentAlignmentData m_offsetBetweenColumns;
    ContentAlignmentData m_offsetBetweenRows;

    mutable GridMasonryLayout m_masonryLayout;

    bool m_baselineItemsCached {false};

    mutable std::optional<GridItemSizeCache> m_intrinsicLogicalHeightsForRowSizingFirstPass;

    bool layoutUsingGridFormattingContext();

    std::optional<bool> m_hasGridFormattingContextLayout;

    mutable bool m_isComputingTrackSizes { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderGrid, isRenderGrid())
