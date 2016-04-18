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

#include "GridPositionsResolver.h"
#include "OrderIterator.h"
#include "RenderBlock.h"

namespace WebCore {

class GridArea;
class GridSpan;
class GridTrack;
class GridItemWithSpan;

struct ContentAlignmentData;

enum GridAxisPosition {GridAxisStart, GridAxisEnd, GridAxisCenter};

class RenderGrid final : public RenderBlock {
public:
    RenderGrid(Element&, Ref<RenderStyle>&&);
    virtual ~RenderGrid();

    Element& element() const { return downcast<Element>(nodeForNonAnonymous()); }

    void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;
    void layoutBlock(bool relayoutChildren, LayoutUnit pageLogicalHeight = 0) override;

    bool avoidsFloats() const override { return true; }
    bool canDropAnonymousBlockChild() const override { return false; }

    const Vector<LayoutUnit>& columnPositions() const { return m_columnPositions; }
    const Vector<LayoutUnit>& rowPositions() const { return m_rowPositions; }

    LayoutUnit guttersSize(GridTrackSizingDirection, size_t span) const;

private:
    const char* renderName() const override;
    bool isRenderGrid() const override { return true; }
    void computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const override;

    Optional<LayoutUnit> computeIntrinsicLogicalContentHeightUsing(Length logicalHeightLength, Optional<LayoutUnit> intrinsicContentHeight, LayoutUnit borderAndPadding) const override;

    class GridIterator;
    class GridSizingData;
    void computeUsedBreadthOfGridTracks(GridTrackSizingDirection, GridSizingData&, LayoutUnit& baseSizesWithoutMaximization, LayoutUnit& growthLimitsWithoutMaximization);
    LayoutUnit computeUsedBreadthOfMinLength(const GridLength&, LayoutUnit maxSize) const;
    LayoutUnit computeUsedBreadthOfMaxLength(const GridLength&, LayoutUnit usedBreadth, LayoutUnit maxSize) const;
    void resolveContentBasedTrackSizingFunctions(GridTrackSizingDirection, GridSizingData&);

    void ensureGridSize(unsigned maximumRowSize, unsigned maximumColumnSize);
    void insertItemIntoGrid(RenderBox&, const GridArea&);
    void placeItemsOnGrid();
    void populateExplicitGridAndOrderIterator();
    std::unique_ptr<GridArea> createEmptyGridAreaAtSpecifiedPositionsOutsideGrid(const RenderBox&, GridTrackSizingDirection, const GridSpan&) const;
    void placeSpecifiedMajorAxisItemsOnGrid(const Vector<RenderBox*>&);
    void placeAutoMajorAxisItemsOnGrid(const Vector<RenderBox*>&);
    typedef std::pair<unsigned, unsigned> AutoPlacementCursor;
    void placeAutoMajorAxisItemOnGrid(RenderBox&, AutoPlacementCursor&);
    GridTrackSizingDirection autoPlacementMajorAxisDirection() const;
    GridTrackSizingDirection autoPlacementMinorAxisDirection() const;

    void prepareChildForPositionedLayout(RenderBox&);
    void layoutPositionedObject(RenderBox&, bool relayoutChildren, bool fixedPositionObjectsOnly) override;
    void offsetAndBreadthForPositionedChild(const RenderBox&, GridTrackSizingDirection, LayoutUnit& offset, LayoutUnit& breadth);

    void computeIntrinsicLogicalHeight(GridSizingData&);
    LayoutUnit computeTrackBasedLogicalHeight(const GridSizingData&) const;
    void computeTrackSizesForDirection(GridTrackSizingDirection, GridSizingData&, LayoutUnit freeSpace);

    void layoutGridItems(GridSizingData&);
    void populateGridPositions(GridSizingData&);
    void clearGrid();

    enum TrackSizeRestriction {
        AllowInfinity,
        ForbidInfinity,
    };
    enum TrackSizeComputationPhase {
        ResolveIntrinsicMinimums,
        ResolveContentBasedMinimums,
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
    LayoutUnit currentItemSizeForTrackSizeComputationPhase(TrackSizeComputationPhase, RenderBox&, GridTrackSizingDirection, GridSizingData&);

    typedef struct GridItemsSpanGroupRange GridItemsSpanGroupRange;
    void resolveContentBasedTrackSizingFunctionsForNonSpanningItems(GridTrackSizingDirection, const GridSpan&, RenderBox& gridItem, GridTrack&, GridSizingData&);
    template <TrackSizeComputationPhase> void resolveContentBasedTrackSizingFunctionsForItems(GridTrackSizingDirection, GridSizingData&, const GridItemsSpanGroupRange&);
    template <TrackSizeComputationPhase> void distributeSpaceToTracks(Vector<GridTrack*>&, const Vector<GridTrack*>* growBeyondGrowthLimitsTracks, LayoutUnit& availableLogicalSpace);

    typedef HashSet<unsigned, DefaultHash<unsigned>::Hash, WTF::UnsignedWithZeroKeyHashTraits<unsigned>> TrackIndexSet;
    double computeFlexFactorUnitSize(const Vector<GridTrack>&, GridTrackSizingDirection, double flexFactorSum, LayoutUnit leftOverSpace, const Vector<unsigned, 8>& flexibleTracksIndexes, std::unique_ptr<TrackIndexSet> tracksToTreatAsInflexible = nullptr) const;
    double findFlexFactorUnitSize(const Vector<GridTrack>&, const GridSpan&, GridTrackSizingDirection, LayoutUnit spaceToFill) const;

    GridTrackSize gridTrackSize(GridTrackSizingDirection, unsigned) const;

    LayoutUnit logicalContentHeightForChild(RenderBox&, GridSizingData&);
    LayoutUnit minSizeForChild(RenderBox&, GridTrackSizingDirection, GridSizingData&);
    LayoutUnit minContentForChild(RenderBox&, GridTrackSizingDirection, GridSizingData&);
    LayoutUnit maxContentForChild(RenderBox&, GridTrackSizingDirection, GridSizingData&);
    GridAxisPosition columnAxisPositionForChild(const RenderBox&) const;
    GridAxisPosition rowAxisPositionForChild(const RenderBox&) const;
    LayoutUnit columnAxisOffsetForChild(const RenderBox&) const;
    LayoutUnit rowAxisOffsetForChild(const RenderBox&) const;
    ContentAlignmentData computeContentPositionAndDistributionOffset(GridTrackSizingDirection, const LayoutUnit& availableFreeSpace, unsigned numberOfGridTracks) const;
    LayoutPoint findChildLogicalPosition(const RenderBox&) const;
    GridArea cachedGridArea(const RenderBox&) const;
    GridSpan cachedGridSpan(const RenderBox&, GridTrackSizingDirection) const;


    LayoutUnit gridAreaBreadthForChild(const RenderBox& child, GridTrackSizingDirection, const Vector<GridTrack>&) const;
    LayoutUnit gridAreaBreadthForChildIncludingAlignmentOffsets(const RenderBox&, GridTrackSizingDirection, const GridSizingData&) const;

    void applyStretchAlignmentToTracksIfNeeded(GridTrackSizingDirection, GridSizingData&);

    void paintChildren(PaintInfo& forSelf, const LayoutPoint& paintOffset, PaintInfo& forChild, bool usePrintRect) override;
    bool needToStretchChildLogicalHeight(const RenderBox&) const;
    LayoutUnit marginLogicalHeightForChild(const RenderBox&) const;
    LayoutUnit computeMarginLogicalHeightForChild(const RenderBox&) const;
    LayoutUnit availableAlignmentSpaceForChildBeforeStretching(LayoutUnit gridAreaBreadthForChild, const RenderBox&) const;
    void applyStretchAlignmentToChildIfNeeded(RenderBox&);
    bool hasAutoMarginsInColumnAxis(const RenderBox&) const;
    bool hasAutoMarginsInRowAxis(const RenderBox&) const;
    void resetAutoMarginsAndLogicalTopInColumnAxis(RenderBox& child);
    void updateAutoMarginsInColumnAxisIfNeeded(RenderBox&);
    void updateAutoMarginsInRowAxisIfNeeded(RenderBox&);

#ifndef NDEBUG
    bool tracksAreWiderThanMinTrackBreadth(GridTrackSizingDirection, GridSizingData&);
#endif

    bool gridWasPopulated() const { return !m_grid.isEmpty() && !m_grid[0].isEmpty(); }

    bool spanningItemCrossesFlexibleSizedTracks(const GridSpan&, GridTrackSizingDirection) const;

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
    LayoutUnit translateRTLCoordinate(LayoutUnit) const;

    Vector<Vector<Vector<RenderBox*, 1>>> m_grid;
    Vector<LayoutUnit> m_columnPositions;
    Vector<LayoutUnit> m_rowPositions;
    LayoutUnit m_offsetBetweenColumns;
    LayoutUnit m_offsetBetweenRows;
    HashMap<const RenderBox*, GridArea> m_gridItemArea;
    OrderIterator m_orderIterator;

    Optional<LayoutUnit> m_minContentHeight;
    Optional<LayoutUnit> m_maxContentHeight;

    int m_smallestColumnStart;
    int m_smallestRowStart;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderGrid, isRenderGrid())

#endif /* ENABLE(CSS_GRID_LAYOUT) */

#endif // RenderGrid_h
