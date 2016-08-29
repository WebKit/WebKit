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
    RenderGrid(Element&, RenderStyle&&);
    virtual ~RenderGrid();

    Element& element() const { return downcast<Element>(nodeForNonAnonymous()); }

    void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;
    void layoutBlock(bool relayoutChildren, LayoutUnit pageLogicalHeight = 0) override;

    bool avoidsFloats() const override { return true; }
    bool canDropAnonymousBlockChild() const override { return false; }

    Vector<LayoutUnit> trackSizesForComputedStyle(GridTrackSizingDirection) const;

    const Vector<LayoutUnit>& columnPositions() const { return m_columnPositions; }
    const Vector<LayoutUnit>& rowPositions() const { return m_rowPositions; }

    size_t autoRepeatCountForDirection(GridTrackSizingDirection) const;

private:
    const char* renderName() const override;
    bool isRenderGrid() const override { return true; }
    void computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const override;

    Optional<LayoutUnit> computeIntrinsicLogicalContentHeightUsing(Length logicalHeightLength, Optional<LayoutUnit> intrinsicContentHeight, LayoutUnit borderAndPadding) const override;

    class GridIterator;
    class GridSizingData;
    enum SizingOperation { TrackSizing, IntrinsicSizeComputation };
    void computeUsedBreadthOfGridTracks(GridTrackSizingDirection, GridSizingData&, LayoutUnit& baseSizesWithoutMaximization, LayoutUnit& growthLimitsWithoutMaximization) const;
    LayoutUnit computeUsedBreadthOfMinLength(const GridLength&, LayoutUnit maxSize) const;
    LayoutUnit computeUsedBreadthOfMaxLength(const GridLength&, LayoutUnit usedBreadth, LayoutUnit maxSize) const;
    void resolveContentBasedTrackSizingFunctions(GridTrackSizingDirection, GridSizingData&) const;

    void ensureGridSize(unsigned maximumRowSize, unsigned maximumColumnSize);
    void insertItemIntoGrid(RenderBox&, const GridArea&);

    unsigned computeAutoRepeatTracksCount(GridTrackSizingDirection, SizingOperation) const;

    typedef ListHashSet<size_t> OrderedTrackIndexSet;
    std::unique_ptr<OrderedTrackIndexSet> computeEmptyTracksForAutoRepeat(GridTrackSizingDirection) const;

    bool hasAutoRepeatEmptyTracks(GridTrackSizingDirection) const;
    bool isEmptyAutoRepeatTrack(GridTrackSizingDirection, unsigned track) const;

    void placeItemsOnGrid(SizingOperation);
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

    void repeatTracksSizingIfNeeded(GridSizingData&, LayoutUnit availableSpaceForColumns, LayoutUnit availableSpaceForRows);

    void layoutGridItems(GridSizingData&);
    void populateGridPositionsForDirection(GridSizingData&, GridTrackSizingDirection);
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
    LayoutUnit currentItemSizeForTrackSizeComputationPhase(TrackSizeComputationPhase, RenderBox&, GridTrackSizingDirection, GridSizingData&) const;

    typedef struct GridItemsSpanGroupRange GridItemsSpanGroupRange;
    void resolveContentBasedTrackSizingFunctionsForNonSpanningItems(GridTrackSizingDirection, const GridSpan&, RenderBox& gridItem, GridTrack&, GridSizingData&) const;
    template <TrackSizeComputationPhase> void resolveContentBasedTrackSizingFunctionsForItems(GridTrackSizingDirection, GridSizingData&, const GridItemsSpanGroupRange&) const;
    template <TrackSizeComputationPhase> void distributeSpaceToTracks(Vector<GridTrack*>&, const Vector<GridTrack*>* growBeyondGrowthLimitsTracks, LayoutUnit& availableLogicalSpace) const;

    typedef HashSet<unsigned, DefaultHash<unsigned>::Hash, WTF::UnsignedWithZeroKeyHashTraits<unsigned>> TrackIndexSet;
    double computeFlexFactorUnitSize(const Vector<GridTrack>&, GridTrackSizingDirection, SizingOperation, double flexFactorSum, LayoutUnit leftOverSpace, const Vector<unsigned, 8>& flexibleTracksIndexes, std::unique_ptr<TrackIndexSet> tracksToTreatAsInflexible = nullptr) const;
    double findFlexFactorUnitSize(const Vector<GridTrack>&, const GridSpan&, GridTrackSizingDirection, SizingOperation, LayoutUnit spaceToFill) const;

    const GridTrackSize& rawGridTrackSize(GridTrackSizingDirection, unsigned) const;
    GridTrackSize gridTrackSize(GridTrackSizingDirection, unsigned, SizingOperation) const;

    bool updateOverrideContainingBlockContentSizeForChild(RenderBox&, GridTrackSizingDirection, GridSizingData&) const;
    LayoutUnit logicalHeightForChild(RenderBox&) const;
    LayoutUnit minSizeForChild(RenderBox&, GridTrackSizingDirection, GridSizingData&) const;
    LayoutUnit minContentForChild(RenderBox&, GridTrackSizingDirection, GridSizingData&) const;
    LayoutUnit maxContentForChild(RenderBox&, GridTrackSizingDirection, GridSizingData&) const;
    GridAxisPosition columnAxisPositionForChild(const RenderBox&) const;
    GridAxisPosition rowAxisPositionForChild(const RenderBox&) const;
    LayoutUnit columnAxisOffsetForChild(const RenderBox&) const;
    LayoutUnit rowAxisOffsetForChild(const RenderBox&) const;
    ContentAlignmentData computeContentPositionAndDistributionOffset(GridTrackSizingDirection, const LayoutUnit& availableFreeSpace, unsigned numberOfGridTracks) const;
    LayoutPoint findChildLogicalPosition(const RenderBox&) const;
    GridArea cachedGridArea(const RenderBox&) const;
    GridSpan cachedGridSpan(const RenderBox&, GridTrackSizingDirection) const;


    LayoutUnit gridAreaBreadthForChild(const RenderBox& child, GridTrackSizingDirection, const GridSizingData&) const;
    LayoutUnit gridAreaBreadthForChildIncludingAlignmentOffsets(const RenderBox&, GridTrackSizingDirection, const GridSizingData&) const;
    LayoutUnit assumedRowsSizeForOrthogonalChild(const RenderBox&, SizingOperation) const;

    void applyStretchAlignmentToTracksIfNeeded(GridTrackSizingDirection, GridSizingData&);

    void paintChildren(PaintInfo& forSelf, const LayoutPoint& paintOffset, PaintInfo& forChild, bool usePrintRect) override;
    bool needToStretchChildLogicalHeight(const RenderBox&) const;
    LayoutUnit marginLogicalHeightForChild(const RenderBox&) const;
    LayoutUnit computeMarginLogicalSizeForChild(GridTrackSizingDirection, const RenderBox&) const;
    LayoutUnit availableAlignmentSpaceForChildBeforeStretching(LayoutUnit gridAreaBreadthForChild, const RenderBox&) const;
    StyleSelfAlignmentData justifySelfForChild(const RenderBox&) const;
    StyleSelfAlignmentData alignSelfForChild(const RenderBox&) const;
    void applyStretchAlignmentToChildIfNeeded(RenderBox&);
    bool hasAutoSizeInColumnAxis(const RenderBox& child) const { return isHorizontalWritingMode() ? child.style().height().isAuto() : child.style().width().isAuto(); }
    bool hasAutoSizeInRowAxis(const RenderBox& child) const { return isHorizontalWritingMode() ? child.style().width().isAuto() : child.style().height().isAuto(); }
    bool allowedToStretchChildAlongColumnAxis(const RenderBox& child) const { return alignSelfForChild(child).position() == ItemPositionStretch && hasAutoSizeInColumnAxis(child) && !hasAutoMarginsInColumnAxis(child); }
    bool allowedToStretchChildAlongRowAxis(const RenderBox& child) const { return justifySelfForChild(child).position() == ItemPositionStretch && hasAutoSizeInRowAxis(child) && !hasAutoMarginsInRowAxis(child); }
    bool hasAutoMarginsInColumnAxis(const RenderBox&) const;
    bool hasAutoMarginsInRowAxis(const RenderBox&) const;
    void resetAutoMarginsAndLogicalTopInColumnAxis(RenderBox& child);
    void updateAutoMarginsInColumnAxisIfNeeded(RenderBox&);
    void updateAutoMarginsInRowAxisIfNeeded(RenderBox&);

#ifndef NDEBUG
    bool tracksAreWiderThanMinTrackBreadth(GridTrackSizingDirection, GridSizingData&);
#endif

    LayoutUnit gridGapForDirection(GridTrackSizingDirection) const;
    LayoutUnit guttersSize(GridTrackSizingDirection, unsigned startLine, unsigned span) const;

    bool spanningItemCrossesFlexibleSizedTracks(const GridSpan&, GridTrackSizingDirection, SizingOperation) const;

    unsigned gridColumnCount() const;
    unsigned gridRowCount() const;

    LayoutUnit translateRTLCoordinate(LayoutUnit) const;

    bool isOrthogonalChild(const RenderBox&) const;
    GridTrackSizingDirection flowAwareDirectionForChild(const RenderBox&, GridTrackSizingDirection) const;

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

    unsigned m_autoRepeatColumns { 0 };
    unsigned m_autoRepeatRows { 0 };

    bool m_hasAnyOrthogonalChild;

    bool m_gridIsDirty { true };

    std::unique_ptr<OrderedTrackIndexSet> m_autoRepeatEmptyColumns { nullptr };
    std::unique_ptr<OrderedTrackIndexSet> m_autoRepeatEmptyRows { nullptr };
};

size_t inline RenderGrid::autoRepeatCountForDirection(GridTrackSizingDirection direction) const
{
    return direction == ForColumns ? m_autoRepeatColumns : m_autoRepeatRows;
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderGrid, isRenderGrid())

#endif /* ENABLE(CSS_GRID_LAYOUT) */

#endif // RenderGrid_h
