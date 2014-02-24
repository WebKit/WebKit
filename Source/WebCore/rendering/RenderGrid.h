/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Igalia S.L. All rights reserved.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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

#include "OrderIterator.h"
#include "RenderBlock.h"

namespace WebCore {

class GridCoordinate;
class GridSpan;
class GridTrack;

enum GridTrackSizingDirection {
    ForColumns,
    ForRows
};

class RenderGrid final : public RenderBlock {
public:
    RenderGrid(Element&, PassRef<RenderStyle>);
    virtual ~RenderGrid();

    Element& element() const { return toElement(nodeForNonAnonymous()); }

    virtual void layoutBlock(bool relayoutChildren, LayoutUnit pageLogicalHeight = 0) override;

    virtual bool avoidsFloats() const override { return true; }
    virtual bool canCollapseAnonymousBlockChild() const override { return false; }

    enum GridPositionSide {
        ColumnStartSide,
        ColumnEndSide,
        RowStartSide,
        RowEndSide
    };

    const Vector<LayoutUnit>& columnPositions() const { return m_columnPositions; }
    const Vector<LayoutUnit>& rowPositions() const { return m_rowPositions; }

private:
    virtual const char* renderName() const override;
    virtual bool isRenderGrid() const override { return true; }
    virtual void computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const override;
    virtual void computePreferredLogicalWidths() override;

    class GridIterator;
    class GridSizingData;
    enum GridTrackSizingDirection { ForColumns, ForRows };
    void computeUsedBreadthOfGridTracks(GridTrackSizingDirection, GridSizingData&);
    void computeUsedBreadthOfGridTracks(GridTrackSizingDirection, GridSizingData&, LayoutUnit& availableLogicalSpace);
    bool gridElementIsShrinkToFit();
    LayoutUnit computeUsedBreadthOfMinLength(GridTrackSizingDirection, const GridLength&) const;
    LayoutUnit computeUsedBreadthOfMaxLength(GridTrackSizingDirection, const GridLength&, LayoutUnit usedBreadth) const;
    LayoutUnit computeUsedBreadthOfSpecifiedLength(GridTrackSizingDirection, const Length&) const;
    void resolveContentBasedTrackSizingFunctions(GridTrackSizingDirection, GridSizingData&);

    void growGrid(GridTrackSizingDirection);
    void insertItemIntoGrid(RenderBox*, size_t rowTrack, size_t columnTrack);
    void insertItemIntoGrid(RenderBox*, const GridCoordinate&);
    void placeItemsOnGrid();
    void populateExplicitGridAndOrderIterator();
    void placeSpecifiedMajorAxisItemsOnGrid(Vector<RenderBox*>);
    void placeAutoMajorAxisItemsOnGrid(Vector<RenderBox*>);
    void placeAutoMajorAxisItemOnGrid(RenderBox*);
    GridTrackSizingDirection autoPlacementMajorAxisDirection() const;
    GridTrackSizingDirection autoPlacementMinorAxisDirection() const;

    void layoutGridItems();
    void populateGridPositions(const GridSizingData&);
    void clearGrid();

    typedef LayoutUnit (RenderGrid::* SizingFunction)(RenderBox*, GridTrackSizingDirection, Vector<GridTrack>&);
    typedef LayoutUnit (GridTrack::* AccumulatorGetter)() const;
    typedef void (GridTrack::* AccumulatorGrowFunction)(LayoutUnit);
    typedef bool (GridTrackSize::* FilterFunction)() const;
    void resolveContentBasedTrackSizingFunctionsForItems(GridTrackSizingDirection, GridSizingData&, RenderBox*, FilterFunction, SizingFunction, AccumulatorGetter, AccumulatorGrowFunction);
    void distributeSpaceToTracks(Vector<GridTrack*>&, Vector<GridTrack*>* tracksForGrowthAboveMaxBreadth, AccumulatorGetter, AccumulatorGrowFunction, GridSizingData&, LayoutUnit& availableLogicalSpace);

    double computeNormalizedFractionBreadth(Vector<GridTrack>&, const GridSpan& tracksSpan, GridTrackSizingDirection, LayoutUnit availableLogicalSpace) const;

    const GridTrackSize& gridTrackSize(GridTrackSizingDirection, size_t) const;
    size_t explicitGridColumnCount() const;
    size_t explicitGridRowCount() const;
    size_t explicitGridSizeForSide(GridPositionSide) const;

    LayoutUnit logicalContentHeightForChild(RenderBox*, Vector<GridTrack>&);
    LayoutUnit minContentForChild(RenderBox*, GridTrackSizingDirection, Vector<GridTrack>& columnTracks);
    LayoutUnit maxContentForChild(RenderBox*, GridTrackSizingDirection, Vector<GridTrack>& columnTracks);
    LayoutPoint findChildLogicalPosition(RenderBox*, const GridSizingData&);
    GridCoordinate cachedGridCoordinate(const RenderBox*) const;

    GridSpan resolveGridPositionsFromAutoPlacementPosition(const RenderBox*, GridTrackSizingDirection, size_t) const;
    PassOwnPtr<GridSpan> resolveGridPositionsFromStyle(const RenderBox*, GridTrackSizingDirection) const;
    size_t resolveNamedGridLinePositionFromStyle(const GridPosition&, GridPositionSide) const;
    size_t resolveGridPositionFromStyle(const GridPosition&, GridPositionSide) const;
    PassOwnPtr<GridSpan> resolveGridPositionAgainstOppositePosition(size_t resolvedOppositePosition, const GridPosition&, GridPositionSide) const;
    PassOwnPtr<GridSpan> resolveNamedGridLinePositionAgainstOppositePosition(size_t resolvedOppositePosition, const GridPosition&, GridPositionSide) const;
    PassOwnPtr<GridSpan> resolveRowStartColumnStartNamedGridLinePositionAgainstOppositePosition(size_t resolvedOppositePosition, const GridPosition&, const Vector<size_t>&) const;
    PassOwnPtr<GridSpan> resolveRowEndColumnEndNamedGridLinePositionAgainstOppositePosition(size_t resolvedOppositePosition, const GridPosition&, const Vector<size_t>&) const;

    LayoutUnit gridAreaBreadthForChild(const RenderBox* child, GridTrackSizingDirection, const Vector<GridTrack>&) const;

    virtual void paintChildren(PaintInfo& forSelf, const LayoutPoint& paintOffset, PaintInfo& forChild, bool usePrintRect) override final;

#ifndef NDEBUG
    bool tracksAreWiderThanMinTrackBreadth(GridTrackSizingDirection, const Vector<GridTrack>&);
    bool gridWasPopulated() const { return !m_grid.isEmpty() && !m_grid[0].isEmpty(); }
#endif

    size_t gridColumnCount() const
    {
        ASSERT(gridWasPopulated());
        return m_grid[0].size();
    }
    size_t gridRowCount() const
    {
        ASSERT(gridWasPopulated());
        return m_grid.size();
    }

    Vector<Vector<Vector<RenderBox*, 1>>> m_grid;
    Vector<LayoutUnit> m_columnPositions;
    Vector<LayoutUnit> m_rowPositions;
    HashMap<const RenderBox*, GridCoordinate> m_gridItemCoordinate;
    OrderIterator m_orderIterator;
};

RENDER_OBJECT_TYPE_CASTS(RenderGrid, isRenderGrid())

} // namespace WebCore

#endif // RenderGrid_h
