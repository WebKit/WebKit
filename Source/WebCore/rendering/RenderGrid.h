/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2013, 2014 Igalia S.L.
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

#ifndef RenderGrid_h
#define RenderGrid_h

#if ENABLE(CSS_GRID_LAYOUT)

#include "GridResolvedPosition.h"
#include "OrderIterator.h"
#include "RenderBlock.h"

namespace WebCore {

class GridCoordinate;
class GridSpan;
class GridTrack;
class GridItemWithSpan;

enum GridAxisPosition {GridAxisStart, GridAxisEnd, GridAxisCenter};

class RenderGrid final : public RenderBlock {
public:
    RenderGrid(Element&, Ref<RenderStyle>&&);
    virtual ~RenderGrid();

    Element& element() const { return downcast<Element>(nodeForNonAnonymous()); }

    virtual void layoutBlock(bool relayoutChildren, LayoutUnit pageLogicalHeight = 0) override;

    virtual bool avoidsFloats() const override { return true; }
    virtual bool canCollapseAnonymousBlockChild() const override { return false; }

    const Vector<LayoutUnit>& columnPositions() const { return m_columnPositions; }
    const Vector<LayoutUnit>& rowPositions() const { return m_rowPositions; }

private:
    virtual const char* renderName() const override;
    virtual bool isRenderGrid() const override { return true; }
    virtual void computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const override;
    virtual void computePreferredLogicalWidths() override;

    class GridIterator;
    class GridSizingData;
    void computeUsedBreadthOfGridTracks(GridTrackSizingDirection, GridSizingData&);
    void computeUsedBreadthOfGridTracks(GridTrackSizingDirection, GridSizingData&, LayoutUnit& availableLogicalSpace);
    bool gridElementIsShrinkToFit();
    LayoutUnit computeUsedBreadthOfMinLength(GridTrackSizingDirection, const GridLength&) const;
    LayoutUnit computeUsedBreadthOfMaxLength(GridTrackSizingDirection, const GridLength&, LayoutUnit usedBreadth) const;
    LayoutUnit computeUsedBreadthOfSpecifiedLength(GridTrackSizingDirection, const Length&) const;
    void resolveContentBasedTrackSizingFunctions(GridTrackSizingDirection, GridSizingData&);

    void ensureGridSize(unsigned maximumRowIndex, unsigned maximumColumnIndex);
    void insertItemIntoGrid(RenderBox&, const GridCoordinate&);
    void placeItemsOnGrid();
    void populateExplicitGridAndOrderIterator();
    std::unique_ptr<GridCoordinate> createEmptyGridAreaAtSpecifiedPositionsOutsideGrid(const RenderBox&, GridTrackSizingDirection, const GridSpan&) const;
    void placeSpecifiedMajorAxisItemsOnGrid(const Vector<RenderBox*>&);
    void placeAutoMajorAxisItemsOnGrid(const Vector<RenderBox*>&);
    typedef std::pair<unsigned, unsigned> AutoPlacementCursor;
    void placeAutoMajorAxisItemOnGrid(RenderBox&, AutoPlacementCursor&);
    GridTrackSizingDirection autoPlacementMajorAxisDirection() const;
    GridTrackSizingDirection autoPlacementMinorAxisDirection() const;

    void layoutGridItems();
    void populateGridPositions(const GridSizingData&);
    void clearGrid();

    enum TrackSizeRestriction {
        AllowInfinity,
        ForbidInfinity,
    };
    enum TrackSizeComputationPhase {
        ResolveIntrinsicMinimums,
        ResolveMaxContentMinimums,
        ResolveIntrinsicMaximums,
        ResolveMaxContentMaximums,
        MaximizeTracks,
    };
    static const LayoutUnit& trackSizeForTrackSizeComputationPhase(TrackSizeComputationPhase, GridTrack&, TrackSizeRestriction);
    static bool shouldProcessTrackForTrackSizeComputationPhase(TrackSizeComputationPhase, const GridTrackSize&);
    static bool trackShouldGrowBeyondGrowthLimitsForTrackSizeComputationPhase(TrackSizeComputationPhase, const GridTrackSize&);
    static void markAsInfinitelyGrowableForTrackSizeComputationPhase(TrackSizeComputationPhase, GridTrack&);
    static void updateTrackSizeForTrackSizeComputationPhase(TrackSizeComputationPhase, GridTrack&);
    LayoutUnit currentItemSizeForTrackSizeComputationPhase(TrackSizeComputationPhase, RenderBox&, GridTrackSizingDirection, Vector<GridTrack>& columnTracks);

    typedef struct GridItemsSpanGroupRange GridItemsSpanGroupRange;
    void resolveContentBasedTrackSizingFunctionsForNonSpanningItems(GridTrackSizingDirection, const GridCoordinate&, RenderBox& gridItem, GridTrack&, Vector<GridTrack>& columnTracks);
    template <TrackSizeComputationPhase> void resolveContentBasedTrackSizingFunctionsForItems(GridTrackSizingDirection, GridSizingData&, const GridItemsSpanGroupRange&);
    template <TrackSizeComputationPhase> void distributeSpaceToTracks(Vector<GridTrack*>&, const Vector<GridTrack*>* growBeyondGrowthLimitsTracks, LayoutUnit& availableLogicalSpace);

    double computeNormalizedFractionBreadth(Vector<GridTrack>&, const GridSpan& tracksSpan, GridTrackSizingDirection, LayoutUnit availableLogicalSpace) const;

    GridTrackSize gridTrackSize(GridTrackSizingDirection, unsigned) const;

    LayoutUnit logicalContentHeightForChild(RenderBox&, Vector<GridTrack>&);
    LayoutUnit minContentForChild(RenderBox&, GridTrackSizingDirection, Vector<GridTrack>& columnTracks);
    LayoutUnit maxContentForChild(RenderBox&, GridTrackSizingDirection, Vector<GridTrack>& columnTracks);
    GridAxisPosition columnAxisPositionForChild(const RenderBox&) const;
    GridAxisPosition rowAxisPositionForChild(const RenderBox&) const;
    LayoutUnit rowPositionForChild(const RenderBox&) const;
    LayoutUnit columnPositionForChild(const RenderBox&) const;
    LayoutPoint findChildLogicalPosition(const RenderBox&) const;
    GridCoordinate cachedGridCoordinate(const RenderBox&) const;


    LayoutUnit gridAreaBreadthForChild(const RenderBox& child, GridTrackSizingDirection, const Vector<GridTrack>&) const;

    virtual void paintChildren(PaintInfo& forSelf, const LayoutPoint& paintOffset, PaintInfo& forChild, bool usePrintRect) override;
    bool allowedToStretchLogicalHeightForChild(const RenderBox&) const;
    bool needToStretchChildLogicalHeight(const RenderBox&) const;
    LayoutUnit marginLogicalHeightForChild(const RenderBox&) const;
    LayoutUnit computeMarginLogicalHeightForChild(const RenderBox&) const;
    LayoutUnit availableAlignmentSpaceForChildBeforeStretching(LayoutUnit gridAreaBreadthForChild, const RenderBox&) const;
    void applyStretchAlignmentToChildIfNeeded(RenderBox&, LayoutUnit gridAreaBreadthForChild);

#ifndef NDEBUG
    bool tracksAreWiderThanMinTrackBreadth(GridTrackSizingDirection, const Vector<GridTrack>&);
#endif

    bool gridWasPopulated() const { return !m_grid.isEmpty() && !m_grid[0].isEmpty(); }

    bool spanningItemCrossesFlexibleSizedTracks(const GridCoordinate&, GridTrackSizingDirection) const;

    unsigned gridColumnCount() const
    {
        ASSERT(gridWasPopulated());
        return m_grid[0].size();
    }
    unsigned gridRowCount() const
    {
        ASSERT(gridWasPopulated());
        return m_grid.size();
    }

    bool hasDefiniteLogicalSize(GridTrackSizingDirection) const;

    Vector<Vector<Vector<RenderBox*, 1>>> m_grid;
    Vector<LayoutUnit> m_columnPositions;
    Vector<LayoutUnit> m_rowPositions;
    HashMap<const RenderBox*, GridCoordinate> m_gridItemCoordinate;
    OrderIterator m_orderIterator;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderGrid, isRenderGrid())

#endif /* ENABLE(CSS_GRID_LAYOUT) */

#endif // RenderGrid_h
