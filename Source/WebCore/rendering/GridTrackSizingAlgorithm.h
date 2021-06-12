/*
 * Copyright (C) 2017 Igalia S.L.
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
#include "GridBaselineAlignment.h"
#include "GridTrackSize.h"
#include "LayoutSize.h"

namespace WebCore {

static const int infinity = -1;

enum SizingOperation { TrackSizing, IntrinsicSizeComputation };

enum TrackSizeComputationPhase {
    ResolveIntrinsicMinimums,
    ResolveContentBasedMinimums,
    ResolveMaxContentMinimums,
    ResolveIntrinsicMaximums,
    ResolveMaxContentMaximums,
    MaximizeTracks,
};

class GridTrackSizingAlgorithmStrategy;

class GridTrack {
public:
    GridTrack() = default;

    const LayoutUnit& baseSize() const;
    void setBaseSize(LayoutUnit);

    const LayoutUnit& growthLimit() const;
    bool growthLimitIsInfinite() const { return m_growthLimit == infinity; }
    void setGrowthLimit(LayoutUnit);

    bool infiniteGrowthPotential() const { return growthLimitIsInfinite() || m_infinitelyGrowable; }
    const LayoutUnit& growthLimitIfNotInfinite() const;

    const LayoutUnit& plannedSize() const { return m_plannedSize; }
    void setPlannedSize(LayoutUnit plannedSize) { m_plannedSize = plannedSize; }

    const LayoutUnit& tempSize() const { return m_tempSize; }
    void setTempSize(const LayoutUnit&);
    void growTempSize(const LayoutUnit&);

    bool infinitelyGrowable() const { return m_infinitelyGrowable; }
    void setInfinitelyGrowable(bool infinitelyGrowable) { m_infinitelyGrowable = infinitelyGrowable; }

    void setGrowthLimitCap(std::optional<LayoutUnit>);
    std::optional<LayoutUnit> growthLimitCap() const { return m_growthLimitCap; }

    const GridTrackSize& cachedTrackSize() const { return *m_cachedTrackSize; }
    void setCachedTrackSize(const GridTrackSize&);

private:
    bool isGrowthLimitBiggerThanBaseSize() const { return growthLimitIsInfinite() || m_growthLimit >= m_baseSize; }

    void ensureGrowthLimitIsBiggerThanBaseSize();

    LayoutUnit m_baseSize { 0 };
    LayoutUnit m_growthLimit { 0 };
    LayoutUnit m_plannedSize { 0 };
    LayoutUnit m_tempSize { 0 };
    std::optional<LayoutUnit> m_growthLimitCap;
    bool m_infinitelyGrowable { false };
    std::optional<GridTrackSize> m_cachedTrackSize;
};

class GridTrackSizingAlgorithm final {
    friend class GridTrackSizingAlgorithmStrategy;

public:
    GridTrackSizingAlgorithm(const RenderGrid* renderGrid, Grid& grid)
        : m_grid(grid)
        , m_renderGrid(renderGrid)
        , m_sizingState(ColumnSizingFirstIteration)
    {
    }

    void setup(GridTrackSizingDirection, unsigned numTracks, SizingOperation, std::optional<LayoutUnit> availableSpace);
    void run();
    void reset();

    // Required by RenderGrid. Try to minimize the exposed surface.
    const Grid& grid() const { return m_grid; }
    // FIXME (jfernandez): We should remove any public getter for this attribute
    // and encapsulate any access in the algorithm class.
    Grid& mutableGrid() const { return m_grid; }

    LayoutUnit minContentSize() const { return m_minContentSize; };
    LayoutUnit maxContentSize() const { return m_maxContentSize; };

    LayoutUnit baselineOffsetForChild(const RenderBox&, GridAxis) const;
    std::optional<LayoutUnit> estimatedGridAreaBreadthForChild(const RenderBox&, GridTrackSizingDirection) const;

    void cacheBaselineAlignedItem(const RenderBox&, GridAxis);
    void copyBaselineItemsCache(const GridTrackSizingAlgorithm&, GridAxis);
    void clearBaselineItemsCache();

    Vector<GridTrack>& tracks(GridTrackSizingDirection direction) { return direction == ForColumns ? m_columns : m_rows; }
    const Vector<GridTrack>& tracks(GridTrackSizingDirection direction) const { return direction == ForColumns ? m_columns : m_rows; }

    std::optional<LayoutUnit> freeSpace(GridTrackSizingDirection direction) const { return direction == ForColumns ? m_freeSpaceColumns : m_freeSpaceRows; }
    void setFreeSpace(GridTrackSizingDirection, std::optional<LayoutUnit>);

    std::optional<LayoutUnit> availableSpace(GridTrackSizingDirection direction) const { return direction == ForColumns ? m_availableSpaceColumns : m_availableSpaceRows; }
    void setAvailableSpace(GridTrackSizingDirection, std::optional<LayoutUnit>);

    LayoutUnit computeTrackBasedSize() const;

    bool hasAnyPercentSizedRowsIndefiniteHeight() const { return m_hasPercentSizedRowsIndefiniteHeight; }

#if ASSERT_ENABLED
    bool tracksAreWiderThanMinTrackBreadth() const;
#endif

private:
    std::optional<LayoutUnit> availableSpace() const;
    bool isRelativeGridLengthAsAuto(const GridLength&, GridTrackSizingDirection) const;
    GridTrackSize calculateGridTrackSize(GridTrackSizingDirection, unsigned translatedIndex) const;
    const GridTrackSize& rawGridTrackSize(GridTrackSizingDirection, unsigned translatedIndex) const;

    // Helper methods for step 1. initializeTrackSizes().
    LayoutUnit initialBaseSize(const GridTrackSize&) const;
    LayoutUnit initialGrowthLimit(const GridTrackSize&, LayoutUnit baseSize) const;

    // Helper methods for step 2. resolveIntrinsicTrackSizes().
    void sizeTrackToFitNonSpanningItem(const GridSpan&, RenderBox& gridItem, GridTrack&);
    bool spanningItemCrossesFlexibleSizedTracks(const GridSpan&) const;
    typedef struct GridItemsSpanGroupRange GridItemsSpanGroupRange;
    template <TrackSizeComputationPhase phase> void increaseSizesToAccommodateSpanningItems(const GridItemsSpanGroupRange& gridItemsWithSpan);
    LayoutUnit itemSizeForTrackSizeComputationPhase(TrackSizeComputationPhase, RenderBox&) const;
    template <TrackSizeComputationPhase phase> void distributeSpaceToTracks(Vector<GridTrack*>& tracks, Vector<GridTrack*>* growBeyondGrowthLimitsTracks, LayoutUnit& availableLogicalSpace) const;
    std::optional<LayoutUnit> gridAreaBreadthForChild(const RenderBox&, GridTrackSizingDirection) const;

    void computeBaselineAlignmentContext();
    void updateBaselineAlignmentContext(const RenderBox&, GridAxis);
    bool canParticipateInBaselineAlignment(const RenderBox&, GridAxis) const;
    bool participateInBaselineAlignment(const RenderBox&, GridAxis) const;

    bool isIntrinsicSizedGridArea(const RenderBox&, GridAxis) const;
    void computeGridContainerIntrinsicSizes();

    // Helper methods for step 4. Strech flexible tracks.
    typedef HashSet<unsigned, DefaultHash<unsigned>, WTF::UnsignedWithZeroKeyHashTraits<unsigned>> TrackIndexSet;
    double computeFlexFactorUnitSize(const Vector<GridTrack>& tracks, double flexFactorSum, LayoutUnit& leftOverSpace, const Vector<unsigned, 8>& flexibleTracksIndexes, std::unique_ptr<TrackIndexSet> tracksToTreatAsInflexible = nullptr) const;
    void computeFlexSizedTracksGrowth(double flexFraction, Vector<LayoutUnit>& increments, LayoutUnit& totalGrowth) const;
    double findFrUnitSize(const GridSpan& tracksSpan, LayoutUnit leftOverSpace) const;

    // Track sizing algorithm steps. Note that the "Maximize Tracks" step is done
    // entirely inside the strategies, that's why we don't need an additional
    // method at thise level.
    void initializeTrackSizes();
    void resolveIntrinsicTrackSizes();
    void stretchFlexibleTracks(std::optional<LayoutUnit> freeSpace);
    void stretchAutoTracks();

    // State machine.
    void advanceNextState();
    bool isValidTransition() const;

    // Data.
    bool wasSetup() const { return !!m_strategy; }
    bool m_needsSetup { true };
    bool m_hasPercentSizedRowsIndefiniteHeight { false };
    std::optional<LayoutUnit> m_availableSpaceRows;
    std::optional<LayoutUnit> m_availableSpaceColumns;

    std::optional<LayoutUnit> m_freeSpaceColumns;
    std::optional<LayoutUnit> m_freeSpaceRows;

    // We need to keep both alive in order to properly size grids with orthogonal
    // writing modes.
    Vector<GridTrack> m_columns;
    Vector<GridTrack> m_rows;
    Vector<unsigned> m_contentSizedTracksIndex;
    Vector<unsigned> m_flexibleSizedTracksIndex;
    Vector<unsigned> m_autoSizedTracksForStretchIndex;

    GridTrackSizingDirection m_direction;
    SizingOperation m_sizingOperation;

    Grid& m_grid;

    const RenderGrid* m_renderGrid;
    std::unique_ptr<GridTrackSizingAlgorithmStrategy> m_strategy;

    // The track sizing algorithm is used for both layout and intrinsic size
    // computation. We're normally just interested in intrinsic inline sizes
    // (a.k.a widths in most of the cases) for the computeIntrinsicLogicalWidths()
    // computations. That's why we don't need to keep around different values for
    // rows/columns.
    LayoutUnit m_minContentSize;
    LayoutUnit m_maxContentSize;

    enum SizingState {
        ColumnSizingFirstIteration,
        RowSizingFirstIteration,
        RowSizingExtraIterationForSizeContainment,
        ColumnSizingSecondIteration,
        RowSizingSecondIteration
    };
    SizingState m_sizingState;

    GridBaselineAlignment m_baselineAlignment;
    typedef HashMap<const RenderBox*, bool> BaselineItemsCache;
    BaselineItemsCache m_columnBaselineItemsMap;
    BaselineItemsCache m_rowBaselineItemsMap;

    // This is a RAII class used to ensure that the track sizing algorithm is
    // executed as it is suppossed to be, i.e., first resolve columns and then
    // rows. Only if required a second iteration is run following the same order,
    // first columns and then rows.
    class StateMachine {
    public:
        StateMachine(GridTrackSizingAlgorithm&);
        ~StateMachine();

    private:
        GridTrackSizingAlgorithm& m_algorithm;
    };
};

class GridTrackSizingAlgorithmStrategy {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual LayoutUnit minContentForChild(RenderBox&) const;
    LayoutUnit maxContentForChild(RenderBox&) const;
    LayoutUnit minSizeForChild(RenderBox&) const;

    virtual ~GridTrackSizingAlgorithmStrategy() = default;

    virtual void maximizeTracks(Vector<GridTrack>&, std::optional<LayoutUnit>& freeSpace) = 0;
    virtual double findUsedFlexFraction(Vector<unsigned>& flexibleSizedTracksIndex, GridTrackSizingDirection, std::optional<LayoutUnit> initialFreeSpace) const = 0;
    virtual bool recomputeUsedFlexFractionIfNeeded(double& flexFraction, LayoutUnit& totalGrowth) const = 0;
    virtual LayoutUnit freeSpaceForStretchAutoTracksStep() const = 0;
    virtual bool isComputingSizeContainment() const = 0;

protected:
    GridTrackSizingAlgorithmStrategy(GridTrackSizingAlgorithm& algorithm)
        : m_algorithm(algorithm) { }

    virtual LayoutUnit minLogicalSizeForChild(RenderBox&, const Length& childMinSize, std::optional<LayoutUnit> availableSize) const;
    virtual void layoutGridItemForMinSizeComputation(RenderBox&, bool overrideSizeHasChanged) const = 0;

    LayoutUnit logicalHeightForChild(RenderBox&) const;
    bool updateOverridingContainingBlockContentSizeForChild(RenderBox&, GridTrackSizingDirection, std::optional<LayoutUnit> = std::nullopt) const;

    // GridTrackSizingAlgorithm accessors for subclasses.
    LayoutUnit computeTrackBasedSize() const { return m_algorithm.computeTrackBasedSize(); }
    GridTrackSizingDirection direction() const { return m_algorithm.m_direction; }
    double findFrUnitSize(const GridSpan& tracksSpan, LayoutUnit leftOverSpace) const { return m_algorithm.findFrUnitSize(tracksSpan, leftOverSpace); }
    void distributeSpaceToTracks(Vector<GridTrack*>& tracks, LayoutUnit& availableLogicalSpace) const { m_algorithm.distributeSpaceToTracks<MaximizeTracks>(tracks, nullptr, availableLogicalSpace); }
    const RenderGrid* renderGrid() const { return m_algorithm.m_renderGrid; }
    std::optional<LayoutUnit> availableSpace() const { return m_algorithm.availableSpace(); }

    GridTrackSizingAlgorithm& m_algorithm;
};

} // namespace WebCore
