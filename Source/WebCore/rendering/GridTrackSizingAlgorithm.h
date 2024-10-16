/*
 * Copyright (C) 2017 Igalia S.L.
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "GridLayoutState.h"
#include "GridTrackSize.h"
#include "LayoutSize.h"
#include "RenderBoxInlines.h"
#include <wtf/StdMap.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {
class GridTrack;
}

namespace WTF {
template<typename T> struct IsDeprecatedWeakRefSmartPointerException;
template<> struct IsDeprecatedWeakRefSmartPointerException<WebCore::GridTrack> : std::true_type { };
}

namespace WebCore {

static const int infinity = -1;

enum class SizingOperation : uint8_t { TrackSizing, IntrinsicSizeComputation };

enum class TrackSizeComputationVariant : uint8_t {
    NotCrossingFlexibleTracks,
    CrossingFlexibleTracks,
};

enum class TrackSizeComputationPhase : uint8_t {
    ResolveIntrinsicMinimums,
    ResolveContentBasedMinimums,
    ResolveMaxContentMinimums,
    ResolveIntrinsicMaximums,
    ResolveMaxContentMaximums,
    MaximizeTracks,
};

enum class SpaceDistributionLimit : uint8_t  {
    UpToGrowthLimit,
    BeyondGrowthLimit,
};

class GridTrackSizingAlgorithmStrategy;
class GridItemWithSpan;

class GridTrack : public CanMakeWeakPtr<GridTrack> {
    WTF_MAKE_TZONE_ALLOCATED(GridTrack);
public:
    GridTrack() = default;

    LayoutUnit baseSize() const;
    LayoutUnit unclampedBaseSize() const;
    void setBaseSize(LayoutUnit);

    const LayoutUnit& growthLimit() const;
    bool growthLimitIsInfinite() const { return m_growthLimit == infinity; }
    void setGrowthLimit(LayoutUnit);

    bool infiniteGrowthPotential() const { return growthLimitIsInfinite() || m_infinitelyGrowable; }
    LayoutUnit growthLimitIfNotInfinite() const;

    const LayoutUnit& plannedSize() const { return m_plannedSize; }
    void setPlannedSize(LayoutUnit plannedSize) { m_plannedSize = plannedSize; }

    const LayoutUnit& tempSize() const { return m_tempSize; }
    void setTempSize(const LayoutUnit&);
    void growTempSize(const LayoutUnit&);

    bool infinitelyGrowable() const { return m_infinitelyGrowable; }
    void setInfinitelyGrowable(bool infinitelyGrowable) { m_infinitelyGrowable = infinitelyGrowable; }

    void setGrowthLimitCap(std::optional<LayoutUnit>);
    std::optional<LayoutUnit> growthLimitCap() const { return m_growthLimitCap; }

    const GridTrackSize& cachedTrackSize() const;
    void setCachedTrackSize(const GridTrackSize&);

private:
    bool isGrowthLimitBiggerThanBaseSize() const { return growthLimitIsInfinite() || m_growthLimit >= std::max(m_baseSize, 0_lu); }

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
    WTF_MAKE_TZONE_ALLOCATED(GridTrackSizingAlgorithm);
    friend class GridTrackSizingAlgorithmStrategy;
    friend class DefiniteSizeStrategy;

public:
    GridTrackSizingAlgorithm(const RenderGrid*, Grid&);
    ~GridTrackSizingAlgorithm();

    void run(GridTrackSizingDirection, unsigned numTracks, SizingOperation, std::optional<LayoutUnit> availableSpace, GridLayoutState&);
    void reset();

    // Required by RenderGrid. Try to minimize the exposed surface.
    const Grid& grid() const { return m_grid; }

    const RenderGrid* renderGrid() const { return m_renderGrid; };

    LayoutUnit minContentSize() const { return m_minContentSize; };
    LayoutUnit maxContentSize() const { return m_maxContentSize; };

    LayoutUnit baselineOffsetForGridItem(const RenderBox&, GridAxis) const;

    // The estimated grid area should be use pre-layout versus the grid area, which should be used once
    // layout is complete.
    std::optional<LayoutUnit> gridAreaBreadthForGridItem(const RenderBox&, GridTrackSizingDirection) const;
    std::optional<LayoutUnit> estimatedGridAreaBreadthForGridItem(const RenderBox&, GridTrackSizingDirection) const;

    void cacheBaselineAlignedItem(const RenderBox&, GridAxis, bool cachingRowSubgridsForRootGrid);
    void copyBaselineItemsCache(const GridTrackSizingAlgorithm&, GridAxis);
    void clearBaselineItemsCache();

    Vector<GridTrack>& tracks(GridTrackSizingDirection direction) { return direction == GridTrackSizingDirection::ForColumns ? m_columns : m_rows; }
    const Vector<GridTrack>& tracks(GridTrackSizingDirection direction) const { return direction == GridTrackSizingDirection::ForColumns ? m_columns : m_rows; }

    std::optional<LayoutUnit> freeSpace(GridTrackSizingDirection direction) const { return direction == GridTrackSizingDirection::ForColumns ? m_freeSpaceColumns : m_freeSpaceRows; }
    void setFreeSpace(GridTrackSizingDirection, std::optional<LayoutUnit>);

    std::optional<LayoutUnit> availableSpace(GridTrackSizingDirection direction) const { return direction == GridTrackSizingDirection::ForColumns ? m_availableSpaceColumns : m_availableSpaceRows; }
    void setAvailableSpace(GridTrackSizingDirection, std::optional<LayoutUnit>);

    LayoutUnit computeTrackBasedSize() const;

    bool hasAnyPercentSizedRowsIndefiniteHeight() const { return m_hasPercentSizedRowsIndefiniteHeight; }
    bool hasAnyFlexibleMaxTrackBreadth() const { return m_hasFlexibleMaxTrackBreadth; }
    bool hasAnyBaselineAlignmentItem() const { return !m_columnBaselineItemsMap.isEmpty() || !m_rowBaselineItemsMap.isEmpty(); }

#if ASSERT_ENABLED
    bool tracksAreWiderThanMinTrackBreadth() const;
#endif

private:
    using SpanLength = unsigned;

    void setup(GridTrackSizingDirection, unsigned numTracks, SizingOperation, std::optional<LayoutUnit> availableSpace);
    struct MasonryMinMaxTrackSize {
        LayoutUnit minContentSize;
        LayoutUnit maxContentSize;
        LayoutUnit minSize;
    };

    struct MasonryMinMaxTrackSizeWithGridSpan {
        MasonryMinMaxTrackSize trackSize;
        GridSpan gridSpan;
    };

    std::optional<LayoutUnit> availableSpace() const;
    bool isRelativeGridLengthAsAuto(const GridLength&, GridTrackSizingDirection) const;
    GridTrackSize calculateGridTrackSize(GridTrackSizingDirection, unsigned translatedIndex) const;
    const GridTrackSize& rawGridTrackSize(GridTrackSizingDirection, unsigned translatedIndex) const;

    // Helper methods for step 1. initializeTrackSizes().
    LayoutUnit initialBaseSize(const GridTrackSize&) const;
    LayoutUnit initialGrowthLimit(const GridTrackSize&, LayoutUnit baseSize) const;

    // Helper methods for step 2. resolveIntrinsicTrackSizes().
    void sizeTrackToFitNonSpanningItem(const GridSpan&, RenderBox& gridItem, GridTrack&, GridLayoutState&);
    void sizeTrackToFitSingleSpanMasonryGroup(const GridSpan&, MasonryMinMaxTrackSize&, GridTrack&);

    bool spanningItemCrossesFlexibleSizedTracks(const GridSpan&) const;

    typedef struct GridItemsSpanGroupRange GridItemsSpanGroupRange;
    template <TrackSizeComputationVariant variant, TrackSizeComputationPhase phase> void increaseSizesToAccommodateSpanningItems(const GridItemsSpanGroupRange& gridItemsWithSpan, GridLayoutState&);
    template <TrackSizeComputationVariant variant> void increaseSizesToAccommodateSpanningItems(const GridItemsSpanGroupRange& gridItemsWithSpan, GridLayoutState&);

    // 12.5 Resolve Intrinsic Track Sizing : Step 3
    // https://drafts.csswg.org/css-grid-2/#algo-spanning-items
    //
    // Take all grid items (definite and indefinite) that span 2 or more tracks, and distribute space to intrinsic tracks (non-flex).
    // The implementation diverges from increaseSizesToAccommodateSpanningItems(), because we are grouping items together that are the same span length.
    // This function is divided into two main sections:
    //
    // 1. Constructing the track items
    // This step takes the definite and indefinite items, and merges them into one large map to send over to the second step.
    // Since the indefinite items are grouped together from a prior computation, this step also need to create "fake" grid items that
    // will be considered in each track.
    //
    // 2. Distribute space to intrinsic tracks
    // This step behaves similar to increaseSizesToAccommodateSpanningItems() where we start at the lowest span length and distribute space to the tracks.
    // Then look at the next smallest span length, and repeat step 2 until we exhaust all grid items.
    template <TrackSizeComputationVariant variant> void increaseSizesToAccommodateSpanningItemsMasonry(StdMap<SpanLength, Vector<MasonryMinMaxTrackSizeWithGridSpan>>&);

    // 12.5 Resolve Intrinsic Track Sizing : Step 4
    // https://drafts.csswg.org/css-grid-2/#algo-spanning-items
    //
    // Take all grid items (definite and indefinite) that span 1 or tracks, and distribute space to only flex tracks.
    // The implementation diverges from increaseSizesToAccommodateSpanningItems(), because we are grouping items together that are the same span length.
    // This function is divided into two main sections:
    //
    // 1. Constructing the track items
    // This step takes the definite and indefinite items, and merges them into one large map to send over to the second step.
    // Since the indefinite items are grouped together from a prior computation, this step also need to create "fake" grid items that
    // will be considered in each track.
    //
    // 2. Distribute space to intrinsic tracks
    // This step behaves similar to increaseSizesToAccommodateSpanningItems() where we consider all track items at once instead of per span length.
    template <TrackSizeComputationVariant variant> void increaseSizesToAccommodateSpanningItemsMasonryWithFlex(Vector<MasonryMinMaxTrackSizeWithGridSpan>&);

    void convertIndefiniteItemsToDefiniteMasonry(const StdMap<SpanLength, MasonryMinMaxTrackSize>& gridTrackSpans, StdMap<SpanLength, Vector<MasonryMinMaxTrackSizeWithGridSpan>>&, Vector<MasonryMinMaxTrackSizeWithGridSpan>&);

    LayoutUnit itemSizeForTrackSizeComputationPhase(TrackSizeComputationPhase, RenderBox&, GridLayoutState&) const;
    LayoutUnit itemSizeForTrackSizeComputationPhaseMasonry(TrackSizeComputationPhase, const MasonryMinMaxTrackSize&) const;

    template <TrackSizeComputationVariant variant, TrackSizeComputationPhase phase> void distributeSpaceToTracks(Vector<WeakPtr<GridTrack>>& tracks, Vector<WeakPtr<GridTrack>>* growBeyondGrowthLimitsTracks, LayoutUnit& freeSpace) const;

    void computeBaselineAlignmentContext();
    void updateBaselineAlignmentContext(const RenderBox&, GridAxis);
    bool canParticipateInBaselineAlignment(const RenderBox&, GridAxis) const;
    bool participateInBaselineAlignment(const RenderBox&, GridAxis) const;

    bool isIntrinsicSizedGridArea(const RenderBox&, GridAxis) const;
    void computeGridContainerIntrinsicSizes();

    // Helper methods for step 4. Stretch flexible tracks.
    typedef HashSet<unsigned, DefaultHash<unsigned>, WTF::UnsignedWithZeroKeyHashTraits<unsigned>> TrackIndexSet;
    double computeFlexFactorUnitSize(const Vector<GridTrack>& tracks, double flexFactorSum, LayoutUnit& leftOverSpace, const Vector<unsigned, 8>& flexibleTracksIndexes, std::unique_ptr<TrackIndexSet> tracksToTreatAsInflexible = nullptr) const;
    void computeFlexSizedTracksGrowth(double flexFraction, Vector<LayoutUnit>& increments, LayoutUnit& totalGrowth) const;
    double findFrUnitSize(const GridSpan& tracksSpan, LayoutUnit leftOverSpace) const;


    void handleInfinityGrowthLimit();

    // Build up a map of min/max sizes for each span length for use during resolving intrinsic track sizes.
    // We also need to keep track of definite items separately, since they do not contribute to every track like indefinite items do.
    void computeDefiniteAndIndefiniteItemsForMasonry(StdMap<SpanLength, MasonryMinMaxTrackSize>&, StdMap<SpanLength, Vector<MasonryMinMaxTrackSizeWithGridSpan>>&, Vector<MasonryMinMaxTrackSizeWithGridSpan>&, GridLayoutState&);
    bool shouldExcludeGridItemForMasonryTrackSizing(const RenderBox& gridItem, unsigned trackIndex, GridSpan itemSpan) const;
    // Track sizing algorithm steps. Note that the "Maximize Tracks" step is done
    // entirely inside the strategies, that's why we don't need an additional
    // method at this level.
    void initializeTrackSizes();
    void resolveIntrinsicTrackSizes(GridLayoutState&);

    // Masonry Implementation of https://drafts.csswg.org/css-grid-2/#algo-content.
    // To implement Masonry performanently, we need to abandon the traditional Grid approach of treating
    // each item individually and start grouping items based on their span. A grid item has 3 major values we care about
    // the minContentSize, maxContentSize, and minSize. These values can be aggregated together and then the max will be chosen.
    // The main three scenarios we need to focus on are items that only span 1 track, items that span multiple tracks without crossing a flex track,
    // and items that span multiple tracks with crossing a flex track.
    //
    // Further details on the optimization can be found at https://fantasai.inkedblade.net/style/specs/masonry/performance.
    void resolveIntrinsicTrackSizesMasonry(GridLayoutState&);
    void stretchFlexibleTracks(std::optional<LayoutUnit> freeSpace, GridLayoutState&);
    void stretchAutoTracks();

    void accumulateIntrinsicSizesForTrack(GridTrack&, unsigned trackIndex, GridIterator&, Vector<GridItemWithSpan>& itemsSortedByIncreasingSpan, Vector<GridItemWithSpan>& itemsCrossingFlexibleTracks, SingleThreadWeakHashSet<RenderBox>& itemsSet, LayoutUnit currentAccumulatedMbp, GridLayoutState&);

    bool copyUsedTrackSizesForSubgrid();

    // State machine.
    void advanceNextState();
    bool isValidTransition() const;

    bool isDirectionInMasonryDirection() const;

    // Data.
    bool wasSetup() const { return !!m_strategy; }
    bool m_needsSetup { true };
    bool m_hasPercentSizedRowsIndefiniteHeight { false };
    bool m_hasFlexibleMaxTrackBreadth { false };
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

    enum class SizingState : uint8_t {
        ColumnSizingFirstIteration,
        RowSizingFirstIteration,
        RowSizingExtraIterationForSizeContainment,
        ColumnSizingSecondIteration,
        RowSizingSecondIteration
    };
    SizingState m_sizingState;

    GridBaselineAlignment m_baselineAlignment;
    using BaselineItemsCache = UncheckedKeyHashMap<SingleThreadWeakRef<const RenderBox>, bool>;
    BaselineItemsCache m_columnBaselineItemsMap;
    BaselineItemsCache m_rowBaselineItemsMap;

    SingleThreadWeakHashSet<RenderGrid> m_rowSubgridsWithBaselineAlignedItems;

    // This is a RAII class used to ensure that the track sizing algorithm is
    // executed as it is supposed to be, i.e., first resolve columns and then
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
    WTF_MAKE_TZONE_ALLOCATED(GridTrackSizingAlgorithmStrategy);
public:
    virtual LayoutUnit minContentContributionForGridItem(RenderBox&, GridLayoutState&) const;
    LayoutUnit maxContentContributionForGridItem(RenderBox&, GridLayoutState&) const;
    LayoutUnit minContributionForGridItem(RenderBox&, GridLayoutState&) const;

    virtual ~GridTrackSizingAlgorithmStrategy() = default;

    virtual void maximizeTracks(Vector<GridTrack>&, std::optional<LayoutUnit>& freeSpace) = 0;
    virtual double findUsedFlexFraction(Vector<unsigned>& flexibleSizedTracksIndex, GridTrackSizingDirection, std::optional<LayoutUnit> initialFreeSpace, GridLayoutState&) const = 0;
    virtual bool recomputeUsedFlexFractionIfNeeded(double& flexFraction, LayoutUnit& totalGrowth) const = 0;
    virtual LayoutUnit freeSpaceForStretchAutoTracksStep() const = 0;
    virtual bool isComputingSizeContainment() const = 0;
    virtual bool isComputingInlineSizeContainment() const = 0;
    virtual bool isComputingSizeOrInlineSizeContainment() const = 0;

protected:
    GridTrackSizingAlgorithmStrategy(GridTrackSizingAlgorithm& algorithm)
        : m_algorithm(algorithm) { }

    virtual LayoutUnit minLogicalSizeForGridItem(RenderBox&, const Length& gridItemMinSize, std::optional<LayoutUnit> availableSize) const;
    virtual void layoutGridItemForMinSizeComputation(RenderBox&, bool overrideSizeHasChanged) const = 0;

    LayoutUnit logicalHeightForGridItem(RenderBox&, GridLayoutState&) const;
    bool updateOverridingContainingBlockContentSizeForGridItem(RenderBox&, GridTrackSizingDirection, std::optional<LayoutUnit> = std::nullopt) const;

    // GridTrackSizingAlgorithm accessors for subclasses.
    LayoutUnit computeTrackBasedSize() const { return m_algorithm.computeTrackBasedSize(); }
    GridTrackSizingDirection direction() const { return m_algorithm.m_direction; }
    GridTrackSizingAlgorithm::SizingState sizingState() const { return m_algorithm.m_sizingState; }
    double findFrUnitSize(const GridSpan& tracksSpan, LayoutUnit leftOverSpace) const { return m_algorithm.findFrUnitSize(tracksSpan, leftOverSpace); }
    void distributeSpaceToTracks(Vector<WeakPtr<GridTrack>>& tracks, LayoutUnit& availableLogicalSpace) const { m_algorithm.distributeSpaceToTracks<TrackSizeComputationVariant::NotCrossingFlexibleTracks, TrackSizeComputationPhase::MaximizeTracks>(tracks, nullptr, availableLogicalSpace); }
    const RenderGrid* renderGrid() const { return m_algorithm.m_renderGrid; }
    std::optional<LayoutUnit> availableSpace() const { return m_algorithm.availableSpace(); }

    GridTrackSizingAlgorithm& m_algorithm;
};

} // namespace WebCore
