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

#include "BaselineAlignmentInlines.h"
#include "Grid.h"
#include "GridMasonryLayout.h"
#include "GridTrackSizingAlgorithm.h"
#include "RenderBlock.h"
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class GridArea;
class GridLayoutState;
class GridSpan;

struct ContentAlignmentData {
    LayoutUnit positionOffset;
    LayoutUnit distributionOffset;
};

enum class GridAxisPosition : uint8_t { GridAxisStart, GridAxisEnd, GridAxisCenter };

class RenderGrid final : public RenderBlock {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(RenderGrid);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RenderGrid);
public:
    RenderGrid(Element&, RenderStyle&&);
    virtual ~RenderGrid();

    Element& element() const { return downcast<Element>(nodeForNonAnonymous()); }

    void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;
    void layoutBlock(bool relayoutChildren, LayoutUnit pageLogicalHeight = 0_lu) override;

    bool avoidsFloats() const override { return true; }
    bool canDropAnonymousBlockChild() const override { return false; }

    void dirtyGrid(bool subgridChanged = false);
    Vector<LayoutUnit> trackSizesForComputedStyle(GridTrackSizingDirection) const;

    const Vector<LayoutUnit>& columnPositions() const { return m_columnPositions; }
    const Vector<LayoutUnit>& rowPositions() const { return m_rowPositions; }

    unsigned autoRepeatCountForDirection(GridTrackSizingDirection direction) const { return currentGrid().autoRepeatTracks(direction); }
    unsigned explicitGridStartForDirection(GridTrackSizingDirection direction) const { return currentGrid().explicitGridStart(direction); }

    // Required by GridTrackSizingAlgorithm. Keep them under control.
    LayoutUnit guttersSize(GridTrackSizingDirection, unsigned startLine, unsigned span, std::optional<LayoutUnit> availableSize) const;
    LayoutUnit gridItemOffset(GridTrackSizingDirection) const;

    std::optional<LayoutUnit> explicitIntrinsicInnerLogicalSize(GridTrackSizingDirection) const;
    void updateGridAreaLogicalSize(RenderBox&, std::optional<LayoutUnit> width, std::optional<LayoutUnit> height) const;
    bool isBaselineAlignmentForGridItem(const RenderBox&) const;
    bool isBaselineAlignmentForGridItem(const RenderBox& gridItem, GridAxis, AllowedBaseLine = AllowedBaseLine::BothLines) const;

    StyleSelfAlignmentData selfAlignmentForGridItem(GridAxis, const RenderBox&, const RenderStyle* = nullptr) const;

    StyleContentAlignmentData contentAlignment(GridTrackSizingDirection) const;

    // These functions handle the actual implementation of layoutBlock based on if
    // the grid is a standard grid or a masonry one. While masonry is an extension of grid,
    // keeping the logic in the same function was leading to a messy amount of if statements being added to handle
    // specific masonry cases.
    void layoutGrid(bool);
    void layoutMasonry(bool);

    // Computes the span relative to this RenderGrid, even if the RenderBox is a grid item
    // of a descendant subgrid.
    GridSpan gridSpanForGridItem(const RenderBox&, GridTrackSizingDirection) const;

    bool isSubgrid(GridTrackSizingDirection) const;
    bool isSubgridRows() const
    {
        return isSubgrid(GridTrackSizingDirection::ForRows);
    }
    bool isSubgridColumns() const
    {
        return isSubgrid(GridTrackSizingDirection::ForColumns);
    }
    bool isSubgridInParentDirection(GridTrackSizingDirection parentDirection) const;

    // Returns true if this grid is inheriting subgridded tracks for
    // the given direction from the specified ancestor. This handles
    // nested subgrids, where ancestor may not be our direct parent.
    bool isSubgridOf(GridTrackSizingDirection, const RenderGrid& ancestor) const;

    bool isMasonry() const;
    bool isMasonry(GridTrackSizingDirection) const;
    bool areMasonryRows() const;
    bool areMasonryColumns() const;

    const Grid& currentGrid() const;
    Grid& currentGrid();

    unsigned numTracks(GridTrackSizingDirection) const;

    void placeItems();

    std::optional<LayoutUnit> availableSpaceForGutters(GridTrackSizingDirection) const;
    LayoutUnit gridGap(GridTrackSizingDirection) const;
    LayoutUnit gridGap(GridTrackSizingDirection, std::optional<LayoutUnit> availableSize) const;

    LayoutUnit masonryContentSize() const;
    Vector<LayoutRect> gridItemsLayoutRects();

    bool shouldCheckExplicitIntrinsicInnerLogicalSize(GridTrackSizingDirection) const;

private:
    friend class GridTrackSizingAlgorithm;
    friend class GridTrackSizingAlgorithmStrategy;
    friend class GridMasonryLayout;

    void computeLayoutRequirementsForItemsBeforeLayout(GridLayoutState&) const;
    bool canSetColumnAxisStretchRequirementForItem(const RenderBox&) const;

    ItemPosition selfAlignmentNormalBehavior(const RenderBox* gridItem = nullptr) const override
    {
        ASSERT(gridItem);
        return gridItem->isRenderReplaced() ? ItemPosition::Start : ItemPosition::Stretch;
    }

    ASCIILiteral renderName() const override;
    void computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const override;

    bool selfAlignmentChangedToStretch(GridAxis, const RenderStyle& oldStyle, const RenderStyle& newStyle, const RenderBox&) const;
    bool selfAlignmentChangedFromStretch(GridAxis, const RenderStyle& oldStyle, const RenderStyle& newStyle, const RenderBox&) const;

    bool subgridDidChange(const RenderStyle& oldStyle) const;
    bool explicitGridDidResize(const RenderStyle&) const;
    bool namedGridLinesDefinitionDidChange(const RenderStyle&) const;
    bool implicitGridLinesDefinitionDidChange(const RenderStyle&) const;

    unsigned computeAutoRepeatTracksCount(GridTrackSizingDirection, std::optional<LayoutUnit> availableSize) const;

    unsigned clampAutoRepeatTracks(GridTrackSizingDirection, unsigned autoRepeatTracks) const;

    std::unique_ptr<OrderedTrackIndexSet> computeEmptyTracksForAutoRepeat(GridTrackSizingDirection) const;

    enum class ShouldUpdateGridAreaLogicalSize : bool { No, Yes };
    void performPreLayoutForGridItems(const GridTrackSizingAlgorithm&, const ShouldUpdateGridAreaLogicalSize) const;

    void placeItemsOnGrid(std::optional<LayoutUnit> availableLogicalWidth);
    void populateExplicitGridAndOrderIterator();
    GridArea createEmptyGridAreaAtSpecifiedPositionsOutsideGrid(const RenderBox&, GridTrackSizingDirection, const GridSpan&) const;
    void placeSpecifiedMajorAxisItemsOnGrid(const Vector<RenderBox*>&);
    void placeAutoMajorAxisItemsOnGrid(const Vector<RenderBox*>&);
    void placeItemUsingMasonryPositioning(Grid&, RenderBox*) const;
    typedef std::pair<unsigned, unsigned> AutoPlacementCursor;
    void placeAutoMajorAxisItemOnGrid(RenderBox&, AutoPlacementCursor&);
    GridTrackSizingDirection autoPlacementMajorAxisDirection() const;
    GridTrackSizingDirection autoPlacementMinorAxisDirection() const;

    static bool itemGridAreaIsWithinImplicitGrid(const GridArea& area, unsigned gridAxisLinesCount, GridTrackSizingDirection gridAxisDirection)
    {
        auto itemSpan = gridAxisDirection == GridTrackSizingDirection::ForColumns ? area.columns : area.rows;
        return itemSpan.startLine() <  gridAxisLinesCount && itemSpan.endLine() < gridAxisLinesCount;
    }

    bool canPerformSimplifiedLayout() const final;
    void prepareGridItemForPositionedLayout(RenderBox&);
    bool hasStaticPositionForGridItem(const RenderBox&, GridTrackSizingDirection) const;
    void layoutPositionedObject(RenderBox&, bool relayoutChildren, bool fixedPositionObjectsOnly) override;

    void computeTrackSizesForDefiniteSize(GridTrackSizingDirection, LayoutUnit availableSpace, GridLayoutState&);
    void computeTrackSizesForIndefiniteSize(GridTrackSizingAlgorithm&, GridTrackSizingDirection, GridLayoutState&, LayoutUnit* minIntrinsicSize = nullptr, LayoutUnit* maxIntrinsicSize = nullptr) const;
    LayoutUnit computeTrackBasedLogicalHeight() const;

    void repeatTracksSizingIfNeeded(LayoutUnit availableSpaceForColumns, LayoutUnit availableSpaceForRows, GridLayoutState&);

    void updateGridAreaForAspectRatioItems(const Vector<RenderBox*>&, GridLayoutState&);

    void layoutGridItems(GridLayoutState&);
    void layoutMasonryItems(GridLayoutState&);

    void populateGridPositionsForDirection(const GridTrackSizingAlgorithm&, GridTrackSizingDirection);

    LayoutUnit gridAreaBreadthForOutOfFlowGridItem(const RenderBox&, GridTrackSizingDirection);
    LayoutUnit logicalOffsetForOutOfFlowGridItem(const RenderBox&, GridTrackSizingDirection, LayoutUnit) const;
    std::pair<LayoutUnit, LayoutUnit> gridAreaPositionForOutOfFlowGridItem(const RenderBox&, GridTrackSizingDirection) const;
    std::pair<LayoutUnit, LayoutUnit> gridAreaPositionForInFlowGridItem(const RenderBox&, GridTrackSizingDirection) const;
    std::pair<LayoutUnit, LayoutUnit> gridAreaPositionForGridItem(const RenderBox&, GridTrackSizingDirection) const;

    GridAxisPosition columnAxisPositionForGridItem(const RenderBox&) const;
    GridAxisPosition rowAxisPositionForGridItem(const RenderBox&) const;
    LayoutUnit columnAxisOffsetForGridItem(const RenderBox&) const;
    LayoutUnit rowAxisOffsetForGridItem(const RenderBox&) const;
    ContentAlignmentData computeContentPositionAndDistributionOffset(GridTrackSizingDirection, const LayoutUnit& availableFreeSpace, unsigned numberOfGridTracks) const;
    void setLogicalPositionForGridItem(RenderBox&) const;
    void setLogicalOffsetForGridItem(RenderBox&, GridTrackSizingDirection) const;
    LayoutUnit logicalOffsetForGridItem(const RenderBox&, GridTrackSizingDirection) const;

    LayoutUnit gridAreaBreadthForGridItemIncludingAlignmentOffsets(const RenderBox&, GridTrackSizingDirection) const;

    void paintChildren(PaintInfo& forSelf, const LayoutPoint& paintOffset, PaintInfo& forChild, bool usePrintRect) override;
    LayoutOptionalOutsets allowedLayoutOverflow() const override;
    void computeOverflow(LayoutUnit oldClientAfterEdge, bool recomputeFloats = false) final;

    LayoutUnit availableAlignmentSpaceForGridItemBeforeStretching(LayoutUnit gridAreaBreadthForGridItem, const RenderBox&, GridTrackSizingDirection) const;
    StyleSelfAlignmentData justifySelfForGridItem(const RenderBox&, StretchingMode = StretchingMode::Any, const RenderStyle* = nullptr) const;
    StyleSelfAlignmentData alignSelfForGridItem(const RenderBox&, StretchingMode = StretchingMode::Any, const RenderStyle* = nullptr) const;
    void applyStretchAlignmentToGridItemIfNeeded(RenderBox&, GridLayoutState&);
    void applySubgridStretchAlignmentToGridItemIfNeeded(RenderBox&);
    bool hasAutoSizeInColumnAxis(const RenderBox& gridItem) const;
    bool hasAutoSizeInRowAxis(const RenderBox& gridItem) const;
    inline bool allowedToStretchGridItemAlongColumnAxis(const RenderBox& gridItem) const;
    inline bool allowedToStretchGridItemAlongRowAxis(const RenderBox& gridItem) const;
    bool hasAutoMarginsInColumnAxis(const RenderBox&) const;
    bool hasAutoMarginsInRowAxis(const RenderBox&) const;
    void resetAutoMarginsAndLogicalTopInColumnAxis(RenderBox& child);
    void updateAutoMarginsInColumnAxisIfNeeded(RenderBox&);
    void updateAutoMarginsInRowAxisIfNeeded(RenderBox&);
    bool isChildEligibleForMarginTrim(MarginTrimType, const RenderBox&) const final;

    LayoutUnit baselinePosition(FontBaseline, bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const final;
    std::optional<LayoutUnit> firstLineBaseline() const final;
    std::optional<LayoutUnit> lastLineBaseline() const final;
    SingleThreadWeakPtr<RenderBox> getBaselineGridItem(ItemPosition alignment) const;
    std::optional<LayoutUnit> inlineBlockBaseline(LineDirectionMode) const final;
    bool isInlineBaselineAlignedChild(const RenderBox&) const;

    LayoutUnit columnAxisBaselineOffsetForGridItem(const RenderBox&) const;
    LayoutUnit rowAxisBaselineOffsetForGridItem(const RenderBox&) const;

    unsigned nonCollapsedTracks(GridTrackSizingDirection) const;

    LayoutUnit translateRTLCoordinate(LayoutUnit) const;

    bool shouldResetLogicalHeightBeforeLayout() const override { return true; }
    bool establishesIndependentFormattingContext() const override;

    bool aspectRatioPrefersInline(const RenderBox& gridItem, bool blockFlowIsColumnAxis);

    Vector<RenderBox*> computeAspectRatioDependentAndBaselineItems();

    GridSpan gridSpanForOutOfFlowGridItem(const RenderBox&, GridTrackSizingDirection) const;

    bool computeGridPositionsForOutOfFlowGridItem(const RenderBox&, GridTrackSizingDirection, int&, bool&, int&, bool&) const;

    AutoRepeatType autoRepeatColumnsType() const;
    AutoRepeatType autoRepeatRowsType() const;

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

    using OutOfFlowPositionsMap = UncheckedKeyHashMap<SingleThreadWeakRef<const RenderBox>, std::optional<size_t>>;
    OutOfFlowPositionsMap m_outOfFlowItemColumn;
    OutOfFlowPositionsMap m_outOfFlowItemRow;

    bool m_hasAspectRatioBlockSizeDependentItem { false };
    bool m_baselineItemsCached {false};
    bool m_hasAnyBaselineAlignmentItem { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderGrid, isRenderGrid())
