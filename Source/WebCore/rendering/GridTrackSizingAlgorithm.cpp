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

#include "config.h"
#include "GridTrackSizingAlgorithm.h"

#include "AncestorSubgridIterator.h"
#include "Grid.h"
#include "GridArea.h"
#include "GridLayoutFunctions.h"
#include "GridPositionsResolver.h"
#include "RenderElementInlines.h"
#include "RenderGrid.h"
#include "RenderStyleConstants.h"
#include "StyleSelfAlignmentData.h"
#include <wtf/StdMap.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(GridTrack);
WTF_MAKE_TZONE_ALLOCATED_IMPL(GridTrackSizingAlgorithm);
WTF_MAKE_TZONE_ALLOCATED_IMPL(GridTrackSizingAlgorithmStrategy);

GridTrackSizingAlgorithm::GridTrackSizingAlgorithm(const RenderGrid* renderGrid, Grid& grid)
    : m_grid(grid)
    , m_renderGrid(renderGrid)
    , m_sizingState(SizingState::ColumnSizingFirstIteration)
{
}

GridTrackSizingAlgorithm::~GridTrackSizingAlgorithm() = default;

LayoutUnit GridTrack::baseSize() const
{
    ASSERT(isGrowthLimitBiggerThanBaseSize());
    return std::max(m_baseSize, 0_lu);
}

LayoutUnit GridTrack::unclampedBaseSize() const
{
    ASSERT(isGrowthLimitBiggerThanBaseSize());
    return m_baseSize;
}

const LayoutUnit& GridTrack::growthLimit() const
{
    ASSERT(isGrowthLimitBiggerThanBaseSize());
    ASSERT(!m_growthLimitCap || m_growthLimitCap.value() >= m_growthLimit || baseSize() >= m_growthLimitCap.value());
    return m_growthLimit;
}

void GridTrack::setBaseSize(LayoutUnit baseSize)
{
    m_baseSize = baseSize;
    ensureGrowthLimitIsBiggerThanBaseSize();
}

void GridTrack::setGrowthLimit(LayoutUnit growthLimit)
{
    m_growthLimit = growthLimit == infinity ? growthLimit : std::min(growthLimit, m_growthLimitCap.value_or(growthLimit));
    ensureGrowthLimitIsBiggerThanBaseSize();
}

LayoutUnit GridTrack::growthLimitIfNotInfinite() const
{
    ASSERT(isGrowthLimitBiggerThanBaseSize());
    return m_growthLimit == infinity ? baseSize() : m_growthLimit;
}

void GridTrack::setTempSize(const LayoutUnit& tempSize)
{
    ASSERT(tempSize >= 0);
    ASSERT(growthLimitIsInfinite() || growthLimit() >= tempSize);
    m_tempSize = tempSize;
}

void GridTrack::growTempSize(const LayoutUnit& tempSize)
{
    ASSERT(tempSize >= 0);
    m_tempSize += tempSize;
}

void GridTrack::setGrowthLimitCap(std::optional<LayoutUnit> growthLimitCap)
{
    ASSERT(!growthLimitCap || growthLimitCap.value() >= 0);
    m_growthLimitCap = growthLimitCap;
}

const GridTrackSize& GridTrack::cachedTrackSize() const
{
    RELEASE_ASSERT(m_cachedTrackSize);
    return *m_cachedTrackSize;
}

void GridTrack::setCachedTrackSize(const GridTrackSize& cachedTrackSize)
{
    m_cachedTrackSize = cachedTrackSize;
}

void GridTrack::ensureGrowthLimitIsBiggerThanBaseSize()
{
    if (m_growthLimit != infinity && m_growthLimit < std::max(m_baseSize, 0_lu))
        m_growthLimit = std::max(m_baseSize, 0_lu);
}

// Static helper methods.

static GridAxis gridAxisForDirection(GridTrackSizingDirection direction)
{
    return direction == GridTrackSizingDirection::ForColumns ? GridAxis::GridRowAxis : GridAxis::GridColumnAxis;
}

static GridTrackSizingDirection gridDirectionForAxis(GridAxis axis)
{
    return axis == GridAxis::GridRowAxis ? GridTrackSizingDirection::ForColumns : GridTrackSizingDirection::ForRows;
}

static bool hasRelativeMarginOrPaddingForGridItem(const RenderBox& gridItem, GridTrackSizingDirection direction)
{
    if (direction == GridTrackSizingDirection::ForColumns)
        return gridItem.style().marginStart().isPercentOrCalculated() || gridItem.style().marginEnd().isPercentOrCalculated() || gridItem.style().paddingStart().isPercentOrCalculated() || gridItem.style().paddingEnd().isPercentOrCalculated();
    return gridItem.style().marginBefore().isPercentOrCalculated() || gridItem.style().marginAfter().isPercentOrCalculated() || gridItem.style().paddingBefore().isPercentOrCalculated() || gridItem.style().paddingAfter().isPercentOrCalculated();
}

static bool hasRelativeOrIntrinsicSizeForGridItem(const RenderBox& gridItem, GridTrackSizingDirection direction)
{
    if (direction == GridTrackSizingDirection::ForColumns)
        return gridItem.hasRelativeLogicalWidth() || gridItem.style().logicalWidth().isIntrinsicOrAuto();
    return gridItem.hasRelativeLogicalHeight() || gridItem.style().logicalHeight().isIntrinsicOrAuto();
}

static bool shouldClearOverridingContainingBlockContentSizeForGridItem(const RenderBox& gridItem, GridTrackSizingDirection direction)
{
    return hasRelativeOrIntrinsicSizeForGridItem(gridItem, direction) || hasRelativeMarginOrPaddingForGridItem(gridItem, direction);
}

static void setOverridingContainingBlockContentSizeForGridItem(const RenderGrid& grid, RenderBox& gridItem, GridTrackSizingDirection direction, std::optional<LayoutUnit> size)
{
    // This function sets the dimension based on the writing mode of the containing block.
    // For subgrids, this might not be the outermost grid, but could be a subgrid. If the
    // writing mode of the CB and the grid for which we're doing sizing don't match, swap
    // the directions.
    direction = GridLayoutFunctions::flowAwareDirectionForGridItem(grid, *gridItem.containingBlock(), direction);
    if (direction == GridTrackSizingDirection::ForColumns)
        gridItem.setOverridingContainingBlockContentLogicalWidth(size);
    else
        gridItem.setOverridingContainingBlockContentLogicalHeight(size);
}

// GridTrackSizingAlgorithm private.

void GridTrackSizingAlgorithm::setFreeSpace(GridTrackSizingDirection direction, std::optional<LayoutUnit> freeSpace)
{
    if (direction == GridTrackSizingDirection::ForColumns)
        m_freeSpaceColumns = freeSpace;
    else
        m_freeSpaceRows = freeSpace;
}

std::optional<LayoutUnit> GridTrackSizingAlgorithm::availableSpace() const
{
    ASSERT(wasSetup());
    return availableSpace(m_direction);
}

void GridTrackSizingAlgorithm::setAvailableSpace(GridTrackSizingDirection direction, std::optional<LayoutUnit> availableSpace)
{
    if (direction == GridTrackSizingDirection::ForColumns)
        m_availableSpaceColumns = availableSpace;
    else
        m_availableSpaceRows = availableSpace;
}

const GridTrackSize& GridTrackSizingAlgorithm::rawGridTrackSize(GridTrackSizingDirection direction, unsigned translatedIndex) const
{
    bool isRowAxis = direction == GridTrackSizingDirection::ForColumns;
    auto& renderStyle = m_renderGrid->style();
    auto& trackStyles = isRowAxis ? renderStyle.gridColumnTrackSizes() : renderStyle.gridRowTrackSizes();
    auto& autoRepeatTrackStyles = isRowAxis ? renderStyle.gridAutoRepeatColumns() : renderStyle.gridAutoRepeatRows();
    auto& autoTrackStyles = isRowAxis ? renderStyle.gridAutoColumns() : renderStyle.gridAutoRows();
    unsigned insertionPoint = isRowAxis ? renderStyle.gridAutoRepeatColumnsInsertionPoint() : renderStyle.gridAutoRepeatRowsInsertionPoint();
    unsigned autoRepeatTracksCount = m_grid.autoRepeatTracks(direction);

    // We should not use GridPositionsResolver::explicitGridXXXCount() for this because the
    // explicit grid might be larger than the number of tracks in grid-template-rows|columns (if
    // grid-template-areas is specified for example).
    unsigned explicitTracksCount = trackStyles.size() + autoRepeatTracksCount;

    int untranslatedIndexAsInt = translatedIndex - m_grid.explicitGridStart(direction);
    unsigned autoTrackStylesSize = autoTrackStyles.size();
    if (untranslatedIndexAsInt < 0) {
        int index = untranslatedIndexAsInt % static_cast<int>(autoTrackStylesSize);
        // We need to transpose the index because the first negative implicit line will get the last defined auto track and so on.
        index += index ? autoTrackStylesSize : 0;
        ASSERT(index >= 0);
        return autoTrackStyles[index];
    }

    unsigned untranslatedIndex = static_cast<unsigned>(untranslatedIndexAsInt);
    if (untranslatedIndex >= explicitTracksCount)
        return autoTrackStyles[(untranslatedIndex - explicitTracksCount) % autoTrackStylesSize];

    if (!autoRepeatTracksCount || untranslatedIndex < insertionPoint)
        return trackStyles[untranslatedIndex];

    if (untranslatedIndex < (insertionPoint + autoRepeatTracksCount)) {
        unsigned autoRepeatLocalIndex = untranslatedIndexAsInt - insertionPoint;
        return autoRepeatTrackStyles[autoRepeatLocalIndex % autoRepeatTrackStyles.size()];
    }

    return trackStyles[untranslatedIndex - autoRepeatTracksCount];
}

LayoutUnit GridTrackSizingAlgorithm::computeTrackBasedSize() const
{
    if (isDirectionInMasonryDirection())
        return m_renderGrid->masonryContentSize();

    LayoutUnit size;
    auto& allTracks = tracks(m_direction);
    for (auto& track : allTracks)
        size += track.baseSize();

    size += m_renderGrid->guttersSize(m_direction, 0, allTracks.size(), availableSpace());

    return size;
}

LayoutUnit GridTrackSizingAlgorithm::initialBaseSize(const GridTrackSize& trackSize) const
{
    const GridLength& gridLength = trackSize.minTrackBreadth();
    if (gridLength.isFlex())
        return 0;

    const Length& trackLength = gridLength.length();
    if (trackLength.isSpecified())
        return valueForLength(trackLength, std::max<LayoutUnit>(availableSpace().value_or(0), 0));

    ASSERT(trackLength.isMinContent() || trackLength.isAuto() || trackLength.isMaxContent());
    return 0;
}

LayoutUnit GridTrackSizingAlgorithm::initialGrowthLimit(const GridTrackSize& trackSize, LayoutUnit baseSize) const
{
    const GridLength& gridLength = trackSize.maxTrackBreadth();
    if (gridLength.isFlex())
        return trackSize.minTrackBreadth().isContentSized() ? LayoutUnit(infinity) : baseSize;

    const Length& trackLength = gridLength.length();
    if (trackLength.isSpecified())
        return valueForLength(trackLength, std::max<LayoutUnit>(availableSpace().value_or(0), 0));

    ASSERT(trackLength.isMinContent() || trackLength.isAuto() || trackLength.isMaxContent());
    return infinity;
}

void GridTrackSizingAlgorithm::sizeTrackToFitSingleSpanMasonryGroup(const GridSpan& span, MasonryMinMaxTrackSize& masonryIndefiniteItems, GridTrack& track)
{
    auto trackPosition = span.startLine();
    const auto& trackSize = tracks(m_direction)[trackPosition].cachedTrackSize();

    if (trackSize.hasMinContentMinTrackBreadth())
        track.setBaseSize(std::max(track.baseSize(), masonryIndefiniteItems.minContentSize));
    else if (trackSize.hasMaxContentMinTrackBreadth())
        track.setBaseSize(std::max(track.baseSize(), masonryIndefiniteItems.maxContentSize));
    else if (trackSize.hasAutoMinTrackBreadth())
        track.setBaseSize(std::max(track.baseSize(), masonryIndefiniteItems.minSize));

    if (trackSize.hasMinContentMaxTrackBreadth())
        track.setGrowthLimit(std::max(track.growthLimit(), masonryIndefiniteItems.minContentSize));
    else if (trackSize.hasMaxContentOrAutoMaxTrackBreadth()) {
        auto growthLimit = masonryIndefiniteItems.maxContentSize;
        if (trackSize.isFitContent())
            growthLimit = std::min(growthLimit, valueForLength(trackSize.fitContentTrackBreadth().length(), availableSpace().value_or(0)));
        track.setGrowthLimit(std::max(track.growthLimit(), growthLimit));
    }
}

void GridTrackSizingAlgorithm::sizeTrackToFitNonSpanningItem(const GridSpan& span, RenderBox& gridItem, GridTrack& track, GridLayoutState& gridLayoutState)
{
    unsigned trackPosition = span.startLine();
    const auto& trackSize = tracks(m_direction)[trackPosition].cachedTrackSize();

    if (trackSize.hasMinContentMinTrackBreadth()) {
        track.setBaseSize(std::max(track.baseSize(), m_strategy->minContentContributionForGridItem(gridItem, gridLayoutState)));
    } else if (trackSize.hasMaxContentMinTrackBreadth()) {
        track.setBaseSize(std::max(track.baseSize(), m_strategy->maxContentContributionForGridItem(gridItem, gridLayoutState)));
    } else if (trackSize.hasAutoMinTrackBreadth()) {
        track.setBaseSize(std::max(track.baseSize(), m_strategy->minContributionForGridItem(gridItem, gridLayoutState)));
    }

    if (trackSize.hasMinContentMaxTrackBreadth()) {
        track.setGrowthLimit(std::max(track.growthLimit(), m_strategy->minContentContributionForGridItem(gridItem, gridLayoutState)));
    } else if (trackSize.hasMaxContentOrAutoMaxTrackBreadth()) {
        LayoutUnit growthLimit = m_strategy->maxContentContributionForGridItem(gridItem, gridLayoutState);
        if (trackSize.isFitContent())
            growthLimit = std::min(growthLimit, valueForLength(trackSize.fitContentTrackBreadth().length(), availableSpace().value_or(0)));
        track.setGrowthLimit(std::max(track.growthLimit(), growthLimit));
    }
}

bool GridTrackSizingAlgorithm::spanningItemCrossesFlexibleSizedTracks(const GridSpan& itemSpan) const
{
    const Vector<GridTrack>& trackList = tracks(m_direction);
    for (auto trackPosition : itemSpan) {
        const auto& trackSize = trackList[trackPosition].cachedTrackSize();
        if (trackSize.minTrackBreadth().isFlex() || trackSize.maxTrackBreadth().isFlex())
            return true;
    }

    return false;
}

class GridItemWithSpan {
public:
    GridItemWithSpan(RenderBox& gridItem, GridSpan span)
        : m_gridItem(gridItem)
        , m_span(span)
    {
    }

    RenderBox& gridItem() const { return m_gridItem; }
    GridSpan span() const { return m_span; }

    bool operator<(const GridItemWithSpan other) const { return m_span.integerSpan() < other.m_span.integerSpan(); }

private:
    std::reference_wrapper<RenderBox> m_gridItem;
    GridSpan m_span;
};

struct GridItemsSpanGroupRange {
    Vector<GridItemWithSpan>::iterator rangeStart;
    Vector<GridItemWithSpan>::iterator rangeEnd;
};

enum class TrackSizeRestriction : uint8_t {
    AllowInfinity,
    ForbidInfinity,
};

LayoutUnit GridTrackSizingAlgorithm::itemSizeForTrackSizeComputationPhase(TrackSizeComputationPhase phase, RenderBox& gridItem, GridLayoutState& gridLayoutState) const
{
    switch (phase) {
    case TrackSizeComputationPhase::ResolveIntrinsicMinimums:
        return m_strategy->minContributionForGridItem(gridItem, gridLayoutState);
    case TrackSizeComputationPhase::ResolveContentBasedMinimums:
    case TrackSizeComputationPhase::ResolveIntrinsicMaximums:
        return m_strategy->minContentContributionForGridItem(gridItem, gridLayoutState);
    case TrackSizeComputationPhase::ResolveMaxContentMinimums:
    case TrackSizeComputationPhase::ResolveMaxContentMaximums:
        return m_strategy->maxContentContributionForGridItem(gridItem, gridLayoutState);
    case TrackSizeComputationPhase::MaximizeTracks:
        ASSERT_NOT_REACHED();
        return 0;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

LayoutUnit GridTrackSizingAlgorithm::itemSizeForTrackSizeComputationPhaseMasonry(TrackSizeComputationPhase phase, const MasonryMinMaxTrackSize& trackSize) const
{
    switch (phase) {
    case TrackSizeComputationPhase::ResolveIntrinsicMinimums:
        return trackSize.minSize;
    case TrackSizeComputationPhase::ResolveContentBasedMinimums:
    case TrackSizeComputationPhase::ResolveIntrinsicMaximums:
        return trackSize.minContentSize;
    case TrackSizeComputationPhase::ResolveMaxContentMinimums:
    case TrackSizeComputationPhase::ResolveMaxContentMaximums:
        return trackSize.maxContentSize;
    case TrackSizeComputationPhase::MaximizeTracks:
        ASSERT_NOT_REACHED();
        return 0;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

static bool shouldProcessTrackForTrackSizeComputationPhase(TrackSizeComputationPhase phase, const GridTrackSize& trackSize)
{
    switch (phase) {
    case TrackSizeComputationPhase::ResolveIntrinsicMinimums:
        return trackSize.hasIntrinsicMinTrackBreadth();
    case TrackSizeComputationPhase::ResolveContentBasedMinimums:
        return trackSize.hasMinOrMaxContentMinTrackBreadth();
    case TrackSizeComputationPhase::ResolveMaxContentMinimums:
        return trackSize.hasMaxContentMinTrackBreadth();
    case TrackSizeComputationPhase::ResolveIntrinsicMaximums:
        return trackSize.hasIntrinsicMaxTrackBreadth();
    case TrackSizeComputationPhase::ResolveMaxContentMaximums:
        return trackSize.hasMaxContentOrAutoMaxTrackBreadth();
    case TrackSizeComputationPhase::MaximizeTracks:
        ASSERT_NOT_REACHED();
        return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}

static LayoutUnit trackSizeForTrackSizeComputationPhase(TrackSizeComputationPhase phase, GridTrack& track, TrackSizeRestriction restriction)
{
    switch (phase) {
    case TrackSizeComputationPhase::ResolveIntrinsicMinimums:
    case TrackSizeComputationPhase::ResolveContentBasedMinimums:
    case TrackSizeComputationPhase::ResolveMaxContentMinimums:
    case TrackSizeComputationPhase::MaximizeTracks:
        return track.baseSize();
    case TrackSizeComputationPhase::ResolveIntrinsicMaximums:
    case TrackSizeComputationPhase::ResolveMaxContentMaximums:
        return restriction == TrackSizeRestriction::AllowInfinity ? track.growthLimit() : track.growthLimitIfNotInfinite();
    }

    ASSERT_NOT_REACHED();
    return track.baseSize();
}

static void updateTrackSizeForTrackSizeComputationPhase(TrackSizeComputationPhase phase, GridTrack& track)
{
    switch (phase) {
    case TrackSizeComputationPhase::ResolveIntrinsicMinimums:
    case TrackSizeComputationPhase::ResolveContentBasedMinimums:
    case TrackSizeComputationPhase::ResolveMaxContentMinimums:
        track.setBaseSize(track.plannedSize());
        return;
    case TrackSizeComputationPhase::ResolveIntrinsicMaximums:
    case TrackSizeComputationPhase::ResolveMaxContentMaximums:
        track.setGrowthLimit(track.plannedSize());
        return;
    case TrackSizeComputationPhase::MaximizeTracks:
        ASSERT_NOT_REACHED();
        return;
    }

    ASSERT_NOT_REACHED();
}

static bool trackShouldGrowBeyondGrowthLimitsForTrackSizeComputationPhase(TrackSizeComputationPhase phase, const GridTrackSize& trackSize)
{
    switch (phase) {
    case TrackSizeComputationPhase::ResolveIntrinsicMinimums:
    case TrackSizeComputationPhase::ResolveContentBasedMinimums:
        return trackSize.hasAutoOrMinContentMinTrackBreadthAndIntrinsicMaxTrackBreadth();
    case TrackSizeComputationPhase::ResolveMaxContentMinimums:
        return trackSize.hasMaxContentMinTrackBreadthAndMaxContentMaxTrackBreadth();
    case TrackSizeComputationPhase::ResolveIntrinsicMaximums:
    case TrackSizeComputationPhase::ResolveMaxContentMaximums:
        return true;
    case TrackSizeComputationPhase::MaximizeTracks:
        ASSERT_NOT_REACHED();
        return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}

static void markAsInfinitelyGrowableForTrackSizeComputationPhase(TrackSizeComputationPhase phase, GridTrack& track)
{
    switch (phase) {
    case TrackSizeComputationPhase::ResolveIntrinsicMinimums:
    case TrackSizeComputationPhase::ResolveContentBasedMinimums:
    case TrackSizeComputationPhase::ResolveMaxContentMinimums:
        return;
    case TrackSizeComputationPhase::ResolveIntrinsicMaximums:
        if (trackSizeForTrackSizeComputationPhase(phase, track, TrackSizeRestriction::AllowInfinity) == infinity  && track.plannedSize() != infinity)
            track.setInfinitelyGrowable(true);
        return;
    case TrackSizeComputationPhase::ResolveMaxContentMaximums:
        if (track.infinitelyGrowable())
            track.setInfinitelyGrowable(false);
        return;
    case TrackSizeComputationPhase::MaximizeTracks:
        ASSERT_NOT_REACHED();
        return;
    }

    ASSERT_NOT_REACHED();
}

template <TrackSizeComputationVariant variant, TrackSizeComputationPhase phase>
void GridTrackSizingAlgorithm::increaseSizesToAccommodateSpanningItems(const GridItemsSpanGroupRange& gridItemsWithSpan, GridLayoutState& gridLayoutState)
{
    Vector<GridTrack>& allTracks = tracks(m_direction);
    for (const auto& trackIndex : m_contentSizedTracksIndex) {
        GridTrack& track = allTracks[trackIndex];
        track.setPlannedSize(trackSizeForTrackSizeComputationPhase(phase, track, TrackSizeRestriction::AllowInfinity));
    }

    Vector<WeakPtr<GridTrack>> growBeyondGrowthLimitsTracks;
    Vector<WeakPtr<GridTrack>> filteredTracks;
    for (auto it = gridItemsWithSpan.rangeStart; it != gridItemsWithSpan.rangeEnd; ++it) {
        GridItemWithSpan& gridItemWithSpan = *it;
        const GridSpan& itemSpan = gridItemWithSpan.span();
        ASSERT(variant == TrackSizeComputationVariant::CrossingFlexibleTracks || itemSpan.integerSpan() > 1u);

        filteredTracks.shrink(0);
        growBeyondGrowthLimitsTracks.shrink(0);
        LayoutUnit spanningTracksSize;
        for (auto trackPosition : itemSpan) {
            GridTrack& track = allTracks[trackPosition];
            const auto& trackSize = track.cachedTrackSize();
            spanningTracksSize += trackSizeForTrackSizeComputationPhase(phase, track, TrackSizeRestriction::ForbidInfinity);
            if (variant == TrackSizeComputationVariant::CrossingFlexibleTracks && !trackSize.maxTrackBreadth().isFlex())
                continue;
            if (!shouldProcessTrackForTrackSizeComputationPhase(phase, trackSize))
                continue;

            filteredTracks.append(track);

            if (trackShouldGrowBeyondGrowthLimitsForTrackSizeComputationPhase(phase, trackSize))
                growBeyondGrowthLimitsTracks.append(track);
        }

        if (filteredTracks.isEmpty())
            continue;

        spanningTracksSize += m_renderGrid->guttersSize(m_direction, itemSpan.startLine(), itemSpan.integerSpan(), availableSpace());

        LayoutUnit extraSpace = itemSizeForTrackSizeComputationPhase(phase, gridItemWithSpan.gridItem(), gridLayoutState) - spanningTracksSize;
        extraSpace = std::max<LayoutUnit>(extraSpace, 0);
        auto& tracksToGrowBeyondGrowthLimits = growBeyondGrowthLimitsTracks.isEmpty() ? filteredTracks : growBeyondGrowthLimitsTracks;
        distributeSpaceToTracks<variant, phase>(filteredTracks, &tracksToGrowBeyondGrowthLimits, extraSpace);
    }

    for (const auto& trackIndex : m_contentSizedTracksIndex) {
        GridTrack& track = allTracks[trackIndex];
        markAsInfinitelyGrowableForTrackSizeComputationPhase(phase, track);
        updateTrackSizeForTrackSizeComputationPhase(phase, track);
    }
}

template <TrackSizeComputationVariant variant>
void GridTrackSizingAlgorithm::increaseSizesToAccommodateSpanningItems(const GridItemsSpanGroupRange& gridItemsWithSpan, GridLayoutState& gridLayoutState)
{
    increaseSizesToAccommodateSpanningItems<variant, TrackSizeComputationPhase::ResolveIntrinsicMinimums>(gridItemsWithSpan, gridLayoutState);
    increaseSizesToAccommodateSpanningItems<variant, TrackSizeComputationPhase::ResolveContentBasedMinimums>(gridItemsWithSpan, gridLayoutState);
    increaseSizesToAccommodateSpanningItems<variant, TrackSizeComputationPhase::ResolveMaxContentMinimums>(gridItemsWithSpan, gridLayoutState);
    increaseSizesToAccommodateSpanningItems<variant, TrackSizeComputationPhase::ResolveIntrinsicMaximums>(gridItemsWithSpan, gridLayoutState);
    increaseSizesToAccommodateSpanningItems<variant, TrackSizeComputationPhase::ResolveMaxContentMaximums>(gridItemsWithSpan, gridLayoutState);
}


template <TrackSizeComputationVariant variant>
void GridTrackSizingAlgorithm::increaseSizesToAccommodateSpanningItemsMasonry(StdMap<SpanLength, Vector<MasonryMinMaxTrackSizeWithGridSpan>>& definiteItemSizes)
{
    auto increaseSizes = [&]<TrackSizeComputationPhase phase>(Vector<MasonryMinMaxTrackSizeWithGridSpan>& definiteItemSizes)
    {
        Vector<GridTrack>& allTracks = tracks(m_direction);
        for (const auto& trackIndex : m_contentSizedTracksIndex) {
            auto& track = allTracks[trackIndex];
            track.setPlannedSize(trackSizeForTrackSizeComputationPhase(phase, track, TrackSizeRestriction::AllowInfinity));
        }

        Vector<WeakPtr<GridTrack>> growBeyondGrowthLimitsTracks;
        Vector<WeakPtr<GridTrack>> filteredTracks;

        for (auto definiteItem : definiteItemSizes) {
            const auto& itemSpan = definiteItem.gridSpan;
            ASSERT(variant == TrackSizeComputationVariant::CrossingFlexibleTracks || itemSpan.integerSpan() > 1u);

            filteredTracks.shrink(0);
            growBeyondGrowthLimitsTracks.shrink(0);
            LayoutUnit spanningTracksSize;
            for (auto trackPosition : itemSpan) {
                auto& track = allTracks[trackPosition];
                const auto& trackSize = track.cachedTrackSize();
                spanningTracksSize += trackSizeForTrackSizeComputationPhase(phase, track, TrackSizeRestriction::ForbidInfinity);

                if (!shouldProcessTrackForTrackSizeComputationPhase(phase, trackSize))
                    continue;

                filteredTracks.append(track);

                if (trackShouldGrowBeyondGrowthLimitsForTrackSizeComputationPhase(phase, trackSize))
                    growBeyondGrowthLimitsTracks.append(track);
            }

            if (filteredTracks.isEmpty())
                continue;

            spanningTracksSize += m_renderGrid->guttersSize(m_direction, itemSpan.startLine(), itemSpan.integerSpan(), availableSpace());

            auto extraSpace = itemSizeForTrackSizeComputationPhaseMasonry(phase, definiteItem.trackSize) - spanningTracksSize;
            extraSpace = std::max<LayoutUnit>(extraSpace, 0);
            auto& tracksToGrowBeyondGrowthLimits = growBeyondGrowthLimitsTracks.isEmpty() ? filteredTracks : growBeyondGrowthLimitsTracks;
            distributeSpaceToTracks<variant, phase>(filteredTracks, &tracksToGrowBeyondGrowthLimits, extraSpace);
        }

        for (const auto& trackIndex : m_contentSizedTracksIndex) {
            auto& track = allTracks[trackIndex];
            markAsInfinitelyGrowableForTrackSizeComputationPhase(phase, track);
            updateTrackSizeForTrackSizeComputationPhase(phase, track);
        }
    };

    for (auto definiteItemSpanGroup : definiteItemSizes) {
        increaseSizes.template operator()<TrackSizeComputationPhase::ResolveIntrinsicMinimums>(definiteItemSpanGroup.second);
        increaseSizes.template operator()<TrackSizeComputationPhase::ResolveContentBasedMinimums>(definiteItemSpanGroup.second);
        increaseSizes.template operator()<TrackSizeComputationPhase::ResolveMaxContentMinimums>(definiteItemSpanGroup.second);
        increaseSizes.template operator()<TrackSizeComputationPhase::ResolveIntrinsicMaximums>(definiteItemSpanGroup.second);
        increaseSizes.template operator()<TrackSizeComputationPhase::ResolveMaxContentMaximums>(definiteItemSpanGroup.second);
    }
}

template <TrackSizeComputationVariant variant>
void GridTrackSizingAlgorithm::increaseSizesToAccommodateSpanningItemsMasonryWithFlex(Vector<MasonryMinMaxTrackSizeWithGridSpan>& definiteItemSizesSpanFlexTracks)
{
    auto increaseSizes = [&]<TrackSizeComputationPhase phase>(Vector<MasonryMinMaxTrackSizeWithGridSpan>& definiteItemSizesSpanFlexTracks)
    {
        auto& allTracks = tracks(m_direction);
        for (const auto& trackIndex : m_contentSizedTracksIndex) {
            auto& track = allTracks[trackIndex];
            track.setPlannedSize(trackSizeForTrackSizeComputationPhase(phase, track, TrackSizeRestriction::AllowInfinity));
        }

        Vector<WeakPtr<GridTrack>> growBeyondGrowthLimitsTracks;
        Vector<WeakPtr<GridTrack>> filteredTracks;

        for (auto& item : definiteItemSizesSpanFlexTracks) {
            const auto& itemSpan = item.gridSpan;
            ASSERT(variant == TrackSizeComputationVariant::CrossingFlexibleTracks || itemSpan.integerSpan() > 1u);

            filteredTracks.shrink(0);
            growBeyondGrowthLimitsTracks.shrink(0);
            LayoutUnit spanningTracksSize;
            for (auto trackPosition : itemSpan) {
                auto& track = allTracks[trackPosition];
                const auto& trackSize = track.cachedTrackSize();
                spanningTracksSize += trackSizeForTrackSizeComputationPhase(phase, track, TrackSizeRestriction::ForbidInfinity);
                if (!trackSize.maxTrackBreadth().isFlex())
                    continue;
                if (!shouldProcessTrackForTrackSizeComputationPhase(phase, trackSize))
                    continue;

                filteredTracks.append(track);

                if (trackShouldGrowBeyondGrowthLimitsForTrackSizeComputationPhase(phase, trackSize))
                    growBeyondGrowthLimitsTracks.append(track);
            }

            if (filteredTracks.isEmpty())
                continue;

            spanningTracksSize += m_renderGrid->guttersSize(m_direction, itemSpan.startLine(), itemSpan.integerSpan(), availableSpace());

            auto extraSpace = itemSizeForTrackSizeComputationPhaseMasonry(phase, item.trackSize) - spanningTracksSize;
            extraSpace = std::max<LayoutUnit>(extraSpace, 0);
            auto& tracksToGrowBeyondGrowthLimits = growBeyondGrowthLimitsTracks.isEmpty() ? filteredTracks : growBeyondGrowthLimitsTracks;
            distributeSpaceToTracks<variant, phase>(filteredTracks, &tracksToGrowBeyondGrowthLimits, extraSpace);
        }

        for (const auto& trackIndex : m_contentSizedTracksIndex) {
            auto& track = allTracks[trackIndex];
            markAsInfinitelyGrowableForTrackSizeComputationPhase(phase, track);
            updateTrackSizeForTrackSizeComputationPhase(phase, track);
        }
    };

    increaseSizes.template operator()<TrackSizeComputationPhase::ResolveIntrinsicMinimums>(definiteItemSizesSpanFlexTracks);
    increaseSizes.template operator()<TrackSizeComputationPhase::ResolveContentBasedMinimums>(definiteItemSizesSpanFlexTracks);
    increaseSizes.template operator()<TrackSizeComputationPhase::ResolveMaxContentMinimums>(definiteItemSizesSpanFlexTracks);
    increaseSizes.template operator()<TrackSizeComputationPhase::ResolveIntrinsicMaximums>(definiteItemSizesSpanFlexTracks);
    increaseSizes.template operator()<TrackSizeComputationPhase::ResolveMaxContentMaximums>(definiteItemSizesSpanFlexTracks);
}

void GridTrackSizingAlgorithm::convertIndefiniteItemsToDefiniteMasonry(const StdMap<SpanLength, MasonryMinMaxTrackSize>& indefiniteSpanSizes, StdMap<SpanLength, Vector<MasonryMinMaxTrackSizeWithGridSpan>>& definiteItemSizes, Vector<MasonryMinMaxTrackSizeWithGridSpan>& definiteItemSizesSpanFlexTracks)
{
    auto& allTracks = tracks(m_direction);

    for (auto& indefiniteItem : indefiniteSpanSizes) {
        for (auto trackIndex = 0u; trackIndex < allTracks.size(); trackIndex++) {
            auto endLine = trackIndex + indefiniteItem.first;
            auto itemSpan = GridSpan::translatedDefiniteGridSpan(trackIndex, endLine);

            if (endLine > allTracks.size())
                continue;

            // The spec requires items with a span of 1 to be handled earlier.
            if (itemSpan.integerSpan() != 1 && !spanningItemCrossesFlexibleSizedTracks(itemSpan))
                definiteItemSizes[itemSpan.integerSpan()].append(MasonryMinMaxTrackSizeWithGridSpan { indefiniteItem.second, itemSpan });

            if (spanningItemCrossesFlexibleSizedTracks(itemSpan))
                definiteItemSizesSpanFlexTracks.append(MasonryMinMaxTrackSizeWithGridSpan { indefiniteItem.second, itemSpan });
        }
    }
}

template <TrackSizeComputationVariant variant>
static double getSizeDistributionWeight(const GridTrack& track)
{
    if (variant != TrackSizeComputationVariant::CrossingFlexibleTracks)
        return 0;
    ASSERT(track.cachedTrackSize().maxTrackBreadth().isFlex());
    return track.cachedTrackSize().maxTrackBreadth().flex();
}

static bool sortByGridTrackGrowthPotential(const WeakPtr<GridTrack>& track1, const WeakPtr<GridTrack>& track2)
{
    // This check ensures that we respect the irreflexivity property of the strict weak ordering required by std::sort
    // (forall x: NOT x < x).
    bool track1HasInfiniteGrowthPotentialWithoutCap = track1->infiniteGrowthPotential() && !track1->growthLimitCap();
    bool track2HasInfiniteGrowthPotentialWithoutCap = track2->infiniteGrowthPotential() && !track2->growthLimitCap();

    if (track1HasInfiniteGrowthPotentialWithoutCap && track2HasInfiniteGrowthPotentialWithoutCap)
        return false;

    if (track1HasInfiniteGrowthPotentialWithoutCap || track2HasInfiniteGrowthPotentialWithoutCap)
        return track2HasInfiniteGrowthPotentialWithoutCap;

    LayoutUnit track1Limit = track1->growthLimitCap().value_or(track1->growthLimit());
    LayoutUnit track2Limit = track2->growthLimitCap().value_or(track2->growthLimit());
    return (track1Limit - track1->baseSize()) < (track2Limit - track2->baseSize());
}

static void clampGrowthShareIfNeeded(TrackSizeComputationPhase phase, const GridTrack& track, LayoutUnit& growthShare)
{
    if (phase != TrackSizeComputationPhase::ResolveMaxContentMaximums || !track.growthLimitCap())
        return;

    LayoutUnit distanceToCap = track.growthLimitCap().value() - track.tempSize();
    if (distanceToCap <= 0)
        return;

    growthShare = std::min(growthShare, distanceToCap);
}

template <TrackSizeComputationPhase phase, SpaceDistributionLimit limit>
static void distributeItemIncurredIncreaseToTrack(GridTrack& track, LayoutUnit& freeSpace, double shareFraction)
{
    LayoutUnit freeSpaceShare(freeSpace / shareFraction);
    LayoutUnit growthShare = limit == SpaceDistributionLimit::BeyondGrowthLimit || track.infiniteGrowthPotential() ? freeSpaceShare : std::min(freeSpaceShare, track.growthLimit() - trackSizeForTrackSizeComputationPhase(phase, track, TrackSizeRestriction::ForbidInfinity));
    clampGrowthShareIfNeeded(phase, track, growthShare);
    ASSERT_WITH_MESSAGE(growthShare >= 0, "We must never shrink any grid track or else we can't guarantee we abide by our min-sizing function.");
    track.growTempSize(growthShare);
    freeSpace -= growthShare;
}

template <TrackSizeComputationVariant variant, TrackSizeComputationPhase phase, SpaceDistributionLimit limit>
static void distributeItemIncurredIncreases(Vector<WeakPtr<GridTrack>>& tracks, LayoutUnit& freeSpace)
{
    uint32_t tracksSize = tracks.size();
    if (!tracksSize)
        return;
    if (variant == TrackSizeComputationVariant::NotCrossingFlexibleTracks) {
        // We have to sort tracks according to their growth potential. This is necessary even when distributing beyond growth limits,
        // because there might be tracks with growth limit caps (like the ones with fit-content()) which cannot indefinitely grow over the limits.
        std::sort(tracks.begin(), tracks.end(), sortByGridTrackGrowthPotential);
        for (uint32_t i = 0; i < tracksSize; ++i) {
            ASSERT(!getSizeDistributionWeight<variant>(*tracks[i]));
            distributeItemIncurredIncreaseToTrack<phase, limit>(*tracks[i], freeSpace, tracksSize - i);
        }
        return;
    }
    // We never grow flex tracks beyond growth limits, since they are infinite.
    ASSERT(limit != SpaceDistributionLimit::BeyondGrowthLimit);
    // For TrackSizeComputationVariant::CrossingFlexibleTracks we don't distribute equally, we need to take the weights into account.
    Vector<double> fractionsOfRemainingSpace(tracksSize);
    double weightSum = 0;
    for (int32_t i = tracksSize - 1; i >= 0; --i) {
        double weight = getSizeDistributionWeight<variant>(*tracks[i]);
        weightSum += weight;
        fractionsOfRemainingSpace[i] = weightSum > 0 ? weightSum / weight : tracksSize - i;
    }
    for (uint32_t i = 0; i < tracksSize; ++i) {
        // Sorting is not needed for TrackSizeComputationVariant::CrossingFlexibleTracks, since all tracks have an infinite growth potential.
        ASSERT(tracks[i]->growthLimitIsInfinite());
        distributeItemIncurredIncreaseToTrack<phase, limit>(*tracks[i], freeSpace, fractionsOfRemainingSpace[i]);
    }
}

template <TrackSizeComputationVariant variant, TrackSizeComputationPhase phase>
void GridTrackSizingAlgorithm::distributeSpaceToTracks(Vector<WeakPtr<GridTrack>>& tracks, Vector<WeakPtr<GridTrack>>* growBeyondGrowthLimitsTracks, LayoutUnit& freeSpace) const
{
    ASSERT(freeSpace >= 0);

    for (auto& track : tracks)
        track->setTempSize(trackSizeForTrackSizeComputationPhase(phase, *track, TrackSizeRestriction::ForbidInfinity));

    if (freeSpace > 0)
        distributeItemIncurredIncreases<variant, phase, SpaceDistributionLimit::UpToGrowthLimit>(tracks, freeSpace);

    if (freeSpace > 0 && growBeyondGrowthLimitsTracks)
        distributeItemIncurredIncreases<variant, phase, SpaceDistributionLimit::BeyondGrowthLimit>(*growBeyondGrowthLimitsTracks, freeSpace);

    for (auto& track : tracks)
        track->setPlannedSize(track->plannedSize() == infinity ? track->tempSize() : std::max(track->plannedSize(), track->tempSize()));
}

std::optional<LayoutUnit> GridTrackSizingAlgorithm::estimatedGridAreaBreadthForGridItem(const RenderBox& gridItem, GridTrackSizingDirection direction) const
{
    const GridSpan& span = m_renderGrid->gridSpanForGridItem(gridItem, direction);
    LayoutUnit gridAreaSize;
    bool gridAreaIsIndefinite = false;
    std::optional<LayoutUnit> availableSize = availableSpace(direction);
    for (auto trackPosition : span) {
        // We may need to estimate the grid area size before running the track sizing algorithm in order to perform the pre-layout of orthogonal items.
        // We cannot use tracks(direction)[trackPosition].cachedTrackSize() because tracks(direction) is empty, since we are either performing pre-layout
        // or are running the track sizing algorithm in the opposite direction and haven't run it in the desired direction yet.
        const auto& trackSize = wasSetup() ? calculateGridTrackSize(direction, trackPosition) : rawGridTrackSize(direction, trackPosition);
        GridLength maxTrackSize = trackSize.maxTrackBreadth();
        if (maxTrackSize.isContentSized() || maxTrackSize.isFlex() || isRelativeGridLengthAsAuto(maxTrackSize, direction))
            gridAreaIsIndefinite = true;
        else
            gridAreaSize += valueForLength(maxTrackSize.length(), availableSize.value_or(0_lu));
    }

    gridAreaSize += m_renderGrid->guttersSize(direction, span.startLine(), span.integerSpan(), availableSize);

    GridTrackSizingDirection gridItemInlineDirection = GridLayoutFunctions::flowAwareDirectionForGridItem(*m_renderGrid, gridItem, GridTrackSizingDirection::ForColumns);
    if (gridAreaIsIndefinite)
        return direction == gridItemInlineDirection ? std::make_optional(std::max(gridItem.maxPreferredLogicalWidth(), gridAreaSize)) : std::nullopt;
    return gridAreaSize;
}

static LayoutUnit computeGridSpanSize(const Vector<GridTrack>& tracks, const GridSpan& gridSpan, const std::optional<LayoutUnit> gridItemOffset, const LayoutUnit totalGuttersSize)
{
    LayoutUnit totalTracksSize;
    for (auto trackPosition : gridSpan)
        totalTracksSize += tracks[trackPosition].baseSize();
    return totalTracksSize + totalGuttersSize + ((gridSpan.integerSpan() - 1) * gridItemOffset.value_or(0_lu));
}

std::optional<LayoutUnit> GridTrackSizingAlgorithm::gridAreaBreadthForGridItem(const RenderBox& gridItem, GridTrackSizingDirection direction) const
{
    // FIXME: These checks only works if we have precomputed logical width/height of the grid, which is not guaranteed.
    if (m_renderGrid->areMasonryColumns() && direction == GridTrackSizingDirection::ForColumns)
        return m_renderGrid->contentLogicalWidth();

    if (m_renderGrid->areMasonryRows() && direction == GridTrackSizingDirection::ForRows && !GridLayoutFunctions::isOrthogonalGridItem(*m_renderGrid, gridItem))
        return m_renderGrid->contentLogicalHeight();

    bool addContentAlignmentOffset =
        direction == GridTrackSizingDirection::ForColumns && (m_sizingState == SizingState::RowSizingFirstIteration || m_sizingState == SizingState::RowSizingExtraIterationForSizeContainment);
    // To determine the column track's size based on an orthogonal grid item we need it's logical
    // height, which may depend on the row track's size. It's possible that the row tracks sizing
    // logic has not been performed yet, so we will need to do an estimation.
    if (direction == GridTrackSizingDirection::ForRows && (m_sizingState == SizingState::ColumnSizingFirstIteration || m_sizingState == SizingState::ColumnSizingSecondIteration) && !m_renderGrid->areMasonryColumns()) {
        ASSERT(GridLayoutFunctions::isOrthogonalGridItem(*m_renderGrid, gridItem));
        // FIXME (jfernandez) Content Alignment should account for this heuristic.
        // https://github.com/w3c/csswg-drafts/issues/2697
        if (m_sizingState == SizingState::ColumnSizingFirstIteration)
            return estimatedGridAreaBreadthForGridItem(gridItem, GridTrackSizingDirection::ForRows);
        addContentAlignmentOffset = true;
    }

    const GridSpan& span = m_renderGrid->gridSpanForGridItem(gridItem, direction);
    return computeGridSpanSize(tracks(direction), span, addContentAlignmentOffset ? std::make_optional(m_renderGrid->gridItemOffset(direction)) : std::nullopt, m_renderGrid->guttersSize(direction, span.startLine(), span.integerSpan(), availableSpace(direction)));
}

bool GridTrackSizingAlgorithm::isRelativeGridLengthAsAuto(const GridLength& length, GridTrackSizingDirection direction) const
{
    return length.isPercentage() && !availableSpace(direction);
}

bool GridTrackSizingAlgorithm::isIntrinsicSizedGridArea(const RenderBox& gridItem, GridAxis axis) const
{
    ASSERT(wasSetup());
    GridTrackSizingDirection direction = gridDirectionForAxis(axis);
    const GridSpan& span = m_renderGrid->gridSpanForGridItem(gridItem, direction);
    for (auto trackPosition : span) {
        const auto& trackSize = rawGridTrackSize(direction, trackPosition);
        // We consider fr units as 'auto' for the min sizing function.
        // FIXME(jfernandez): https://github.com/w3c/csswg-drafts/issues/2611
        //
        // The use of AvailableSize function may imply different results
        // for the same item when assuming indefinite or definite size
        // constraints depending on the phase we evaluate the item's
        // baseline participation.
        // FIXME(jfernandez): https://github.com/w3c/csswg-drafts/issues/3046
        if (trackSize.isContentSized() || trackSize.isFitContent() || trackSize.minTrackBreadth().isFlex() || (trackSize.maxTrackBreadth().isFlex() && !availableSpace(direction)))
            return true;
    }
    return false;
}

GridTrackSize GridTrackSizingAlgorithm::calculateGridTrackSize(GridTrackSizingDirection direction, unsigned translatedIndex) const
{
    ASSERT(wasSetup());
    // Collapse empty auto repeat tracks if auto-fit.
    if (m_grid.hasAutoRepeatEmptyTracks(direction) && m_grid.isEmptyAutoRepeatTrack(direction, translatedIndex))
        return { Length(LengthType::Fixed), LengthTrackSizing };

    auto& trackSize = rawGridTrackSize(direction, translatedIndex);
    if (trackSize.isFitContent())
        return isRelativeGridLengthAsAuto(trackSize.fitContentTrackBreadth(), direction) ? GridTrackSize(Length(LengthType::Auto), Length(LengthType::MaxContent)) : trackSize;

    GridLength minTrackBreadth = trackSize.minTrackBreadth();
    GridLength maxTrackBreadth = trackSize.maxTrackBreadth();
    // If the logical width/height of the grid container is indefinite, percentage
    // values are treated as <auto>.
    if (isRelativeGridLengthAsAuto(trackSize.minTrackBreadth(), direction))
        minTrackBreadth = Length(LengthType::Auto);
    if (isRelativeGridLengthAsAuto(trackSize.maxTrackBreadth(), direction))
        maxTrackBreadth = Length(LengthType::Auto);

    // Flex sizes are invalid as a min sizing function. However we still can have a flexible |minTrackBreadth|
    // if the track size is just a flex size (e.g. "1fr"), the spec says that in this case it implies an automatic minimum.
    if (minTrackBreadth.isFlex())
        minTrackBreadth = Length(LengthType::Auto);

    return GridTrackSize(minTrackBreadth, maxTrackBreadth);
}

double GridTrackSizingAlgorithm::computeFlexFactorUnitSize(const Vector<GridTrack>& tracks, double flexFactorSum, LayoutUnit& leftOverSpace, const Vector<unsigned, 8>& flexibleTracksIndexes, std::unique_ptr<TrackIndexSet> tracksToTreatAsInflexible) const
{
    // We want to avoid the effect of flex factors sum below 1 making the factor unit size to grow exponentially.
    double hypotheticalFactorUnitSize = leftOverSpace / std::max<double>(1, flexFactorSum);

    // product of the hypothetical "flex factor unit" and any flexible track's "flex factor" must be grater than such track's "base size".
    bool validFlexFactorUnit = true;
    for (auto index : flexibleTracksIndexes) {
        if (tracksToTreatAsInflexible && tracksToTreatAsInflexible->contains(index))
            continue;
        LayoutUnit baseSize = tracks[index].baseSize();
        double flexFactor = tracks[index].cachedTrackSize().maxTrackBreadth().flex();
        // treating all such tracks as inflexible.
        if (baseSize > hypotheticalFactorUnitSize * flexFactor) {
            leftOverSpace -= baseSize;
            flexFactorSum -= flexFactor;
            if (!tracksToTreatAsInflexible)
                tracksToTreatAsInflexible = makeUnique<TrackIndexSet>();
            tracksToTreatAsInflexible->add(index);
            validFlexFactorUnit = false;
        }
    }
    if (!validFlexFactorUnit)
        return computeFlexFactorUnitSize(tracks, flexFactorSum, leftOverSpace, flexibleTracksIndexes, WTFMove(tracksToTreatAsInflexible));
    return hypotheticalFactorUnitSize;
}

void GridTrackSizingAlgorithm::computeFlexSizedTracksGrowth(double flexFraction, Vector<LayoutUnit>& increments, LayoutUnit& totalGrowth) const
{
    size_t numFlexTracks = m_flexibleSizedTracksIndex.size();
    ASSERT(increments.size() == numFlexTracks);
    const Vector<GridTrack>& allTracks = tracks(m_direction);
    // The flexFraction multiplied by the flex factor can result in a non-integer size. Since we floor the stretched size to fit in a LayoutUnit,
    // we may lose the fractional part of the computation which can cause the entire free space not being distributed evenly. The leftover
    // fractional part from every flexible track are accumulated here to avoid this issue.
    double leftOverSize = 0;
    for (size_t i = 0; i < numFlexTracks; ++i) {
        unsigned trackIndex = m_flexibleSizedTracksIndex[i];
        const auto& trackSize = allTracks[trackIndex].cachedTrackSize();
        ASSERT(trackSize.maxTrackBreadth().isFlex());
        LayoutUnit oldBaseSize = allTracks[trackIndex].baseSize();
        double frShare = flexFraction * trackSize.maxTrackBreadth().flex() + leftOverSize;
        auto stretchedSize = LayoutUnit(frShare);
        LayoutUnit newBaseSize = std::max(oldBaseSize, stretchedSize);
        increments[i] = newBaseSize - oldBaseSize;
        totalGrowth += increments[i];
        // In the case that stretchedSize is greater than frShare, we floor it to 0 to avoid a negative leftover.
        leftOverSize = std::max(frShare - stretchedSize.toDouble(), 0.0);
    }
}

double GridTrackSizingAlgorithm::findFrUnitSize(const GridSpan& tracksSpan, LayoutUnit leftOverSpace) const
{
    if (leftOverSpace <= 0)
        return 0;

    const Vector<GridTrack>& allTracks = tracks(m_direction);
    double flexFactorSum = 0;
    Vector<unsigned, 8> flexibleTracksIndexes;
    for (auto trackIndex : tracksSpan) {
        const auto& trackSize = allTracks[trackIndex].cachedTrackSize();
        if (!trackSize.maxTrackBreadth().isFlex())
            leftOverSpace -= allTracks[trackIndex].baseSize();
        else {
            double flexFactor = trackSize.maxTrackBreadth().flex();
            flexibleTracksIndexes.append(trackIndex);
            flexFactorSum += flexFactor;
        }
    }
    // We don't remove the gutters from left_over_space here, because that was already done before.

    // The function is not called if we don't have <flex> grid tracks.
    ASSERT(!flexibleTracksIndexes.isEmpty());

    return computeFlexFactorUnitSize(allTracks, flexFactorSum, leftOverSpace, flexibleTracksIndexes);
}

void GridTrackSizingAlgorithm::computeGridContainerIntrinsicSizes()
{
    if (m_direction == GridTrackSizingDirection::ForColumns && m_strategy->isComputingSizeOrInlineSizeContainment()) {
        if (auto size = m_renderGrid->explicitIntrinsicInnerLogicalSize(m_direction)) {
            m_minContentSize = size.value();
            m_maxContentSize = size.value();
            return;
        }
    }

    m_minContentSize = m_maxContentSize = 0_lu;

    Vector<GridTrack>& allTracks = tracks(m_direction);
    for (auto& track : allTracks) {
        ASSERT(m_strategy->isComputingSizeOrInlineSizeContainment() || !track.infiniteGrowthPotential());
        m_minContentSize += track.baseSize();
        m_maxContentSize += track.growthLimitIsInfinite() ? track.baseSize() : track.growthLimit();
        // The growth limit caps must be cleared now in order to properly sort
        // tracks by growth potential on an eventual "Maximize Tracks".
        track.setGrowthLimitCap(std::nullopt);
    }
}

// GridTrackSizingAlgorithmStrategy.
LayoutUnit GridTrackSizingAlgorithmStrategy::logicalHeightForGridItem(RenderBox& gridItem, GridLayoutState& gridLayoutState) const
{
    GridTrackSizingDirection gridItemBlockDirection = GridLayoutFunctions::flowAwareDirectionForGridItem(*renderGrid(), gridItem, GridTrackSizingDirection::ForRows);
    // If |gridItem| has a relative logical height, we shouldn't let it override its intrinsic height, which is
    // what we are interested in here. Thus we need to set the block-axis override size to nullopt (no possible resolution).
    auto hasOverridingContainingBlockContentSizeForGridItem = [&] {
        if (auto overridingContainingBlockContentSizeForGridItem = GridLayoutFunctions::overridingContainingBlockContentSizeForGridItem(gridItem, GridTrackSizingDirection::ForRows); overridingContainingBlockContentSizeForGridItem && *overridingContainingBlockContentSizeForGridItem)
            return true;
        return false;
    };
    if (hasOverridingContainingBlockContentSizeForGridItem() && shouldClearOverridingContainingBlockContentSizeForGridItem(gridItem, GridTrackSizingDirection::ForRows)) {
        setOverridingContainingBlockContentSizeForGridItem(*renderGrid(), gridItem, gridItemBlockDirection, std::nullopt);
        gridItem.setNeedsLayout(MarkOnlyThis);

        if (renderGrid()->canSetColumnAxisStretchRequirementForItem(gridItem))
            gridLayoutState.setLayoutRequirementForGridItem(gridItem, ItemLayoutRequirement::NeedsColumnAxisStretchAlignment);
    }

    // We need to clear the stretched content size to properly compute logical height during layout.
    if (gridItem.needsLayout())
        gridItem.clearOverridingContentSize();

    gridItem.layoutIfNeeded();
    return gridItem.logicalHeight() + GridLayoutFunctions::marginLogicalSizeForGridItem(*renderGrid(), gridItemBlockDirection, gridItem) + m_algorithm.baselineOffsetForGridItem(gridItem, gridAxisForDirection(direction()));
}

LayoutUnit GridTrackSizingAlgorithmStrategy::minContentContributionForGridItem(RenderBox& gridItem, GridLayoutState& gridLayoutState) const
{
    GridTrackSizingDirection gridItemInlineDirection = GridLayoutFunctions::flowAwareDirectionForGridItem(*renderGrid(), gridItem, GridTrackSizingDirection::ForColumns);
    if (direction() == gridItemInlineDirection) {
        if (isComputingInlineSizeContainment())
            return { };

        bool needsGridItemMinContentContributionForSecondColumnPass = sizingState() == GridTrackSizingAlgorithm::SizingState::ColumnSizingSecondIteration
            && gridLayoutState.containsLayoutRequirementForGridItem(gridItem, ItemLayoutRequirement::MinContentContributionForSecondColumnPass);

        // FIXME: It's unclear if we should return the intrinsic width or the preferred width.
        // See http://lists.w3.org/Archives/Public/www-style/2013Jan/0245.html
        if (gridItem.needsPreferredWidthsRecalculation() ||  needsGridItemMinContentContributionForSecondColumnPass)
            gridItem.setPreferredLogicalWidthsDirty(true);

        if (needsGridItemMinContentContributionForSecondColumnPass) {
            auto rowSize = renderGrid()->gridAreaBreadthForGridItemIncludingAlignmentOffsets(gridItem, GridTrackSizingDirection::ForRows);
            auto stretchedSize = !GridLayoutFunctions::isOrthogonalGridItem(*renderGrid(), gridItem) ? gridItem.constrainLogicalHeightByMinMax(rowSize, { }) : gridItem.constrainLogicalWidthInFragmentByMinMax(rowSize, renderGrid()->contentWidth(), *renderGrid(), nullptr);
            GridLayoutFunctions::setOverridingContentSizeForGridItem(*renderGrid(), gridItem, stretchedSize, GridTrackSizingDirection::ForRows);
        }

        auto minContentLogicalWidth = gridItem.minPreferredLogicalWidth();

        if (needsGridItemMinContentContributionForSecondColumnPass)
            GridLayoutFunctions::clearOverridingContentSizeForGridItem(*renderGrid(), gridItem, GridTrackSizingDirection::ForRows);

        auto minLogicalWidth = [&] {
            auto gridItemLogicalMinWidth = gridItem.style().logicalMinWidth();

            if (gridItemLogicalMinWidth.isFixed())
                return LayoutUnit { gridItemLogicalMinWidth.value() };
            if (gridItemLogicalMinWidth.isMaxContent())
                return gridItem.maxPreferredLogicalWidth();

            // FIXME: We should be able to handle other values for the logical min width.
            return 0_lu;
        }();

        return std::max(minContentLogicalWidth, minLogicalWidth) + GridLayoutFunctions::marginLogicalSizeForGridItem(*renderGrid(), gridItemInlineDirection, gridItem) + m_algorithm.baselineOffsetForGridItem(gridItem, gridAxisForDirection(direction()));
    }

    if (updateOverridingContainingBlockContentSizeForGridItem(gridItem, gridItemInlineDirection)) {
        gridItem.setNeedsLayout(MarkOnlyThis);
        // For a grid item with relative width constraints to the grid area, such as percentaged paddings, we reset the overridingContainingBlockContentSizeForGridItem value for columns when we are executing a definite strategy
        // for columns. Since we have updated the overridingContainingBlockContentSizeForGridItem inline-axis/width value here, we might need to recompute the grid item's relative width. For some cases, we probably will not
        // be able to do it during the RenderGrid::layoutGridItems() function as the grid area does't change there any more. Also, as we are doing a layout inside GridTrackSizingAlgorithmStrategy::logicalHeightForGridItem()
        // function, let's take the advantage and set it here.
        if (shouldClearOverridingContainingBlockContentSizeForGridItem(gridItem, gridItemInlineDirection))
            gridItem.setPreferredLogicalWidthsDirty(true);
    }
    return logicalHeightForGridItem(gridItem, gridLayoutState);
}

LayoutUnit GridTrackSizingAlgorithmStrategy::maxContentContributionForGridItem(RenderBox& gridItem, GridLayoutState& gridLayoutState) const
{
    GridTrackSizingDirection gridItemInlineDirection = GridLayoutFunctions::flowAwareDirectionForGridItem(*renderGrid(), gridItem, GridTrackSizingDirection::ForColumns);
    if (direction() == gridItemInlineDirection) {
        if (isComputingInlineSizeContainment())
            return { };
        // FIXME: It's unclear if we should return the intrinsic width or the preferred width.
        // See http://lists.w3.org/Archives/Public/www-style/2013Jan/0245.html
        if (gridItem.needsPreferredWidthsRecalculation())
            gridItem.setPreferredLogicalWidthsDirty(true);
        return gridItem.maxPreferredLogicalWidth() + GridLayoutFunctions::marginLogicalSizeForGridItem(*renderGrid(), gridItemInlineDirection, gridItem) + m_algorithm.baselineOffsetForGridItem(gridItem, gridAxisForDirection(direction()));
    }

    if (updateOverridingContainingBlockContentSizeForGridItem(gridItem, gridItemInlineDirection))
        gridItem.setNeedsLayout(MarkOnlyThis);
    return logicalHeightForGridItem(gridItem, gridLayoutState);
}

LayoutUnit GridTrackSizingAlgorithmStrategy::minContributionForGridItem(RenderBox& gridItem, GridLayoutState& gridLayoutState) const
{
    GridTrackSizingDirection gridItemInlineDirection = GridLayoutFunctions::flowAwareDirectionForGridItem(*renderGrid(), gridItem, GridTrackSizingDirection::ForColumns);
    bool isRowAxis = direction() == gridItemInlineDirection;
    if (isRowAxis && isComputingInlineSizeContainment())
        return { };
    const Length& gridItemSize = isRowAxis ? gridItem.style().logicalWidth() : gridItem.style().logicalHeight();
    if (!gridItemSize.isAuto() && !gridItemSize.isPercentOrCalculated())
        return minContentContributionForGridItem(gridItem, gridLayoutState);

    const Length& gridItemMinSize = isRowAxis ? gridItem.style().logicalMinWidth() : gridItem.style().logicalMinHeight();
    bool overflowIsVisible = isRowAxis ? gridItem.effectiveOverflowInlineDirection() == Overflow::Visible : gridItem.effectiveOverflowBlockDirection() == Overflow::Visible;
    LayoutUnit baselineShim = m_algorithm.baselineOffsetForGridItem(gridItem, gridAxisForDirection(direction()));

    if (gridItemMinSize.isAuto() && overflowIsVisible) {
        auto minSize = minContentContributionForGridItem(gridItem, gridLayoutState);
        const GridSpan& span = m_algorithm.m_renderGrid->gridSpanForGridItem(gridItem, direction());

        LayoutUnit maxBreadth;
        const auto& allTracks = m_algorithm.tracks(direction());
        bool allFixed = true;
        for (auto trackPosition : span) {
            const auto& trackSize = allTracks[trackPosition].cachedTrackSize();
            if (trackSize.maxTrackBreadth().isFlex() && span.integerSpan() > 1)
                return { };
            if (!trackSize.hasFixedMaxTrackBreadth())
                allFixed = false;
            else if (allFixed)
                maxBreadth += valueForLength(trackSize.maxTrackBreadth().length(), availableSpace().value_or(0_lu));
        }
        if (!allFixed)
            return minSize;
        if (minSize > maxBreadth) {
            auto marginAndBorderAndPadding = GridLayoutFunctions::marginLogicalSizeForGridItem(*renderGrid(), direction(), gridItem);
            marginAndBorderAndPadding += isRowAxis ? gridItem.borderAndPaddingLogicalWidth() : gridItem.borderAndPaddingLogicalHeight();
            minSize = std::max(maxBreadth, marginAndBorderAndPadding + baselineShim);
        }
        return minSize;
    }

    std::optional<LayoutUnit> gridAreaSize = m_algorithm.gridAreaBreadthForGridItem(gridItem, gridItemInlineDirection);
    return minLogicalSizeForGridItem(gridItem, gridItemMinSize, gridAreaSize) + baselineShim;
}

bool GridTrackSizingAlgorithm::canParticipateInBaselineAlignment(const RenderBox& gridItem, GridAxis baselineAxis) const
{
    ASSERT(baselineAxis == GridAxis::GridColumnAxis ? m_columnBaselineItemsMap.contains(&gridItem) : m_rowBaselineItemsMap.contains(&gridItem));

    // Baseline cyclic dependencies only happen with synthesized
    // baselines. These cases include orthogonal or empty grid items
    // and replaced elements.
    bool isParallelToBaselineAxis = baselineAxis == GridAxis::GridColumnAxis ? !GridLayoutFunctions::isOrthogonalGridItem(*m_renderGrid, gridItem) : GridLayoutFunctions::isOrthogonalGridItem(*m_renderGrid, gridItem);
    if (isParallelToBaselineAxis && gridItem.firstLineBaseline())
        return true;

    // FIXME: We don't currently allow items within subgrids that need to
    // synthesize a baseline, since we need a layout to have been completed
    // and performPreLayoutForGridItems on the outer grid doesn't layout subgrid
    // items.
    if (gridItem.parent() != renderGrid())
        return false;

    // Baseline cyclic dependencies only happen in grid areas with
    // intrinsically-sized tracks.
    if (!isIntrinsicSizedGridArea(gridItem, baselineAxis))
        return true;

    return isParallelToBaselineAxis ? !gridItem.hasRelativeLogicalHeight() : !gridItem.hasRelativeLogicalWidth() && !gridItem.style().logicalWidth().isAuto();
}

bool GridTrackSizingAlgorithm::participateInBaselineAlignment(const RenderBox& gridItem, GridAxis baselineAxis) const
{
    return baselineAxis == GridAxis::GridColumnAxis ? m_columnBaselineItemsMap.get(&gridItem) : m_rowBaselineItemsMap.get(&gridItem);
}

void GridTrackSizingAlgorithm::updateBaselineAlignmentContext(const RenderBox& gridItem, GridAxis baselineAxis)
{
    ASSERT(wasSetup());
    ASSERT(canParticipateInBaselineAlignment(gridItem, baselineAxis));

    ItemPosition align = m_renderGrid->selfAlignmentForGridItem(baselineAxis, gridItem).position();
    const auto& span = m_renderGrid->gridSpanForGridItem(gridItem, gridDirectionForAxis(baselineAxis));
    auto alignmentContext = GridLayoutFunctions::alignmentContextForBaselineAlignment(span, align);
    m_baselineAlignment.updateBaselineAlignmentContext(align, alignmentContext, gridItem, baselineAxis);
}

LayoutUnit GridTrackSizingAlgorithm::baselineOffsetForGridItem(const RenderBox& gridItem, GridAxis baselineAxis) const
{
    // If we haven't yet initialized this axis (which can be the case if we're doing
    // prelayout of a subgrid), then we can't know the baseline offset.
    if (tracks(gridDirectionForAxis(baselineAxis)).isEmpty())
        return LayoutUnit();

    if (!participateInBaselineAlignment(gridItem, baselineAxis))
        return LayoutUnit();

    ASSERT_IMPLIES(baselineAxis == GridAxis::GridColumnAxis, !m_renderGrid->isSubgridRows());
    ItemPosition align = m_renderGrid->selfAlignmentForGridItem(baselineAxis, gridItem).position();
    const auto& span = m_renderGrid->gridSpanForGridItem(gridItem, gridDirectionForAxis(baselineAxis));
    auto alignmentContext = GridLayoutFunctions::alignmentContextForBaselineAlignment(span, align);
    return m_baselineAlignment.baselineOffsetForGridItem(align, alignmentContext, gridItem, baselineAxis);
}

void GridTrackSizingAlgorithm::clearBaselineItemsCache()
{
    m_columnBaselineItemsMap.clear();
    m_rowBaselineItemsMap.clear();
}

void GridTrackSizingAlgorithm::cacheBaselineAlignedItem(const RenderBox& item, GridAxis axis, bool cachingRowSubgridsForRootGrid)
{
    ASSERT(downcast<RenderGrid>(item.parent())->isBaselineAlignmentForGridItem(item, axis));

    if (GridLayoutFunctions::isOrthogonalParent(*m_renderGrid, *item.parent()))
        axis = axis == GridAxis::GridColumnAxis ? GridAxis::GridRowAxis : GridAxis::GridColumnAxis;

    if (axis == GridAxis::GridColumnAxis)
        m_columnBaselineItemsMap.add(item, true);
    else
        m_rowBaselineItemsMap.add(item, true);

    const auto* gridItemParent = dynamicDowncast<RenderGrid>(item.parent());
    if (gridItemParent) {
        auto gridItemParentIsSubgridRowsOfRootGrid = GridLayoutFunctions::isOrthogonalGridItem(*m_renderGrid, *gridItemParent) ? gridItemParent->isSubgridColumns() : gridItemParent->isSubgridRows();
        if (cachingRowSubgridsForRootGrid && gridItemParentIsSubgridRowsOfRootGrid)
            m_rowSubgridsWithBaselineAlignedItems.add(*gridItemParent);
    }
}

void GridTrackSizingAlgorithm::copyBaselineItemsCache(const GridTrackSizingAlgorithm& source, GridAxis axis)
{
    if (axis == GridAxis::GridColumnAxis)
        m_columnBaselineItemsMap = source.m_columnBaselineItemsMap;
    else
        m_rowBaselineItemsMap = source.m_rowBaselineItemsMap;
}

bool GridTrackSizingAlgorithmStrategy::updateOverridingContainingBlockContentSizeForGridItem(RenderBox& gridItem, GridTrackSizingDirection direction, std::optional<LayoutUnit> overrideSize) const
{
    if (!overrideSize)
        overrideSize = m_algorithm.gridAreaBreadthForGridItem(gridItem, direction);

    if (renderGrid() != gridItem.parent()) {
        // If |gridItem| is part of a subgrid, find the nearest ancestor this is directly part of this grid
        // (either by being a child of the grid, or via being subgridded in this dimension.
        RenderGrid* grid = downcast<RenderGrid>(gridItem.parent());
        GridTrackSizingDirection subgridDirection = GridLayoutFunctions::flowAwareDirectionForGridItem(*renderGrid(), *grid, direction);
        while (grid->parent() != renderGrid() && !grid->isSubgridOf(subgridDirection, *renderGrid())) {
            grid = downcast<RenderGrid>(grid->parent());
            subgridDirection = GridLayoutFunctions::flowAwareDirectionForGridItem(*renderGrid(), *grid, direction);
        }

        if (grid == gridItem.parent() && grid->isSubgrid(subgridDirection)) {
            // If the item is subgridded in this direction (and thus the tracks it covers are tracks
            // owned by this sizing algorithm), then we want to take the breadth of the tracks we occupy,
            // and subtract any space occupied by the subgrid itself (and any ancestor subgrids).
            *overrideSize -= GridLayoutFunctions::extraMarginForSubgridAncestors(subgridDirection, gridItem).extraTotalMargin();
        } else {
            // Otherwise the tracks that this grid item covers (in this non-subgridded axis) are owned
            // by one of the intermediate RenderGrids (which are subgrids in the other axis), which may
            // be |grid| or a descendent.
            // Set the override size for |grid| (which is part of the outer grid), and force a layout
            // so that it computes the track sizes for the non-subgridded dimension and makes the size
            // of |gridItem| available.
            bool overrideSizeHasChanged =
                updateOverridingContainingBlockContentSizeForGridItem(*grid, direction);
            layoutGridItemForMinSizeComputation(*grid, overrideSizeHasChanged);
            return overrideSizeHasChanged;
        }
    }

    if (auto overridingContainingBlockContentSizeForGridItem = GridLayoutFunctions::overridingContainingBlockContentSizeForGridItem(gridItem, direction); overridingContainingBlockContentSizeForGridItem && *overridingContainingBlockContentSizeForGridItem == overrideSize)
        return false;

    setOverridingContainingBlockContentSizeForGridItem(*renderGrid(), gridItem, direction, overrideSize);
    return true;
}

LayoutUnit GridTrackSizingAlgorithmStrategy::minLogicalSizeForGridItem(RenderBox& gridItem, const Length& gridItemMinSize, std::optional<LayoutUnit> availableSize) const
{
    GridTrackSizingDirection gridItemInlineDirection = GridLayoutFunctions::flowAwareDirectionForGridItem(*renderGrid(), gridItem, GridTrackSizingDirection::ForColumns);
    bool isRowAxis = direction() == gridItemInlineDirection;
    if (isRowAxis)
        return isComputingInlineSizeContainment() ? 0_lu : gridItem.computeLogicalWidthInFragmentUsing(RenderBox::SizeType::MinSize, gridItemMinSize, availableSize.value_or(0), *renderGrid(), nullptr) + GridLayoutFunctions::marginLogicalSizeForGridItem(*renderGrid(), gridItemInlineDirection, gridItem);
    bool overrideSizeHasChanged = updateOverridingContainingBlockContentSizeForGridItem(gridItem, gridItemInlineDirection, availableSize);
    layoutGridItemForMinSizeComputation(gridItem, overrideSizeHasChanged);
    GridTrackSizingDirection gridItemBlockDirection = GridLayoutFunctions::flowAwareDirectionForGridItem(*renderGrid(), gridItem, GridTrackSizingDirection::ForRows);
    return gridItem.computeLogicalHeightUsing(RenderBox::SizeType::MinSize, gridItemMinSize, std::nullopt).value_or(0) + GridLayoutFunctions::marginLogicalSizeForGridItem(*renderGrid(), gridItemBlockDirection, gridItem);
}

class IndefiniteSizeStrategy final : public GridTrackSizingAlgorithmStrategy {
public:
    IndefiniteSizeStrategy(GridTrackSizingAlgorithm& algorithm)
        : GridTrackSizingAlgorithmStrategy(algorithm) { }

private:
    void layoutGridItemForMinSizeComputation(RenderBox&, bool overrideSizeHasChanged) const override;
    void maximizeTracks(Vector<GridTrack>&, std::optional<LayoutUnit>& freeSpace) override;
    double findUsedFlexFraction(Vector<unsigned>& flexibleSizedTracksIndex, GridTrackSizingDirection, std::optional<LayoutUnit> freeSpace, GridLayoutState&) const override;
    bool recomputeUsedFlexFractionIfNeeded(double& flexFraction, LayoutUnit& totalGrowth) const override;
    LayoutUnit freeSpaceForStretchAutoTracksStep() const override;
    bool isComputingSizeContainment() const override { return renderGrid()->shouldApplySizeContainment(); }
    bool isComputingInlineSizeContainment() const override { return renderGrid()->shouldApplyInlineSizeContainment(); }
    bool isComputingSizeOrInlineSizeContainment() const override { return renderGrid()->shouldApplySizeOrInlineSizeContainment(); }
    void accumulateFlexFraction(double& flexFraction, GridIterator&, GridTrackSizingDirection outermostDirection, SingleThreadWeakHashSet<RenderBox>& itemsSet, GridLayoutState&) const;
};

void IndefiniteSizeStrategy::layoutGridItemForMinSizeComputation(RenderBox& gridItem, bool overrideSizeHasChanged) const
{
    if (overrideSizeHasChanged && direction() != GridTrackSizingDirection::ForColumns)
        gridItem.setNeedsLayout(MarkOnlyThis);
    gridItem.layoutIfNeeded();
}

void IndefiniteSizeStrategy::maximizeTracks(Vector<GridTrack>& tracks, std::optional<LayoutUnit>& freeSpace)
{
    UNUSED_PARAM(freeSpace);
    for (auto& track : tracks)
        track.setBaseSize(track.growthLimit());
}


static inline double normalizedFlexFraction(const GridTrack& track)
{
    double flexFactor = track.cachedTrackSize().maxTrackBreadth().flex();
    return track.baseSize() / std::max<double>(1, flexFactor);
}

void IndefiniteSizeStrategy::accumulateFlexFraction(double& flexFraction, GridIterator& iterator, GridTrackSizingDirection outermostDirection, SingleThreadWeakHashSet<RenderBox>& itemsSet, GridLayoutState& gridLayoutState) const
{
    while (auto* gridItem = iterator.nextGridItem()) {
        if (CheckedPtr inner = dynamicDowncast<RenderGrid>(gridItem); inner && inner->isSubgridInParentDirection(iterator.direction())) {
            const RenderGrid& subgrid = *inner;
            GridSpan span = downcast<RenderGrid>(subgrid.parent())->gridSpanForGridItem(subgrid, iterator.direction());
            GridIterator subgridIterator = GridIterator::createForSubgrid(*inner, iterator, span);
            accumulateFlexFraction(flexFraction, subgridIterator, outermostDirection, itemsSet, gridLayoutState);
            continue;
        }
        // Do not include already processed items.
        if (!itemsSet.add(*gridItem).isNewEntry)
            continue;

        GridSpan span = m_algorithm.renderGrid()->gridSpanForGridItem(*gridItem, outermostDirection);

        // Removing gutters from the max-content contribution of the item, so they are not taken into account in FindFrUnitSize().
        LayoutUnit leftOverSpace = maxContentContributionForGridItem(*gridItem, gridLayoutState) - renderGrid()->guttersSize(outermostDirection, span.startLine(), span.integerSpan(), availableSpace());
        flexFraction = std::max(flexFraction, findFrUnitSize(span, leftOverSpace));
    }
}

double IndefiniteSizeStrategy::findUsedFlexFraction(Vector<unsigned>& flexibleSizedTracksIndex, GridTrackSizingDirection direction, std::optional<LayoutUnit> freeSpace, GridLayoutState& gridLayoutState) const
{
    UNUSED_PARAM(freeSpace);
    auto allTracks = m_algorithm.tracks(direction);

    double flexFraction = 0;
    for (const auto& trackIndex : flexibleSizedTracksIndex) {
        // FIXME: we pass TrackSizing to gridTrackSize() because it does not really matter
        // as we know the track is a flex sized track. It'd be nice not to have to do that.
        flexFraction = std::max(flexFraction, normalizedFlexFraction(allTracks[trackIndex]));
    }

    const Grid& grid = m_algorithm.grid();
    // If we are computing the flex fraction of the grid while under the "Sizing as if empty," phase,
    // then we should use the flex fraction computed up to this point since we do not want to avoid
    // taking into consideration the grid items.
    if (!grid.hasGridItems() || (renderGrid()->shouldCheckExplicitIntrinsicInnerLogicalSize(direction) && !m_algorithm.renderGrid()->explicitIntrinsicInnerLogicalSize(direction)))
        return flexFraction;

    SingleThreadWeakHashSet<RenderBox> itemsSet;
    for (const auto& trackIndex : flexibleSizedTracksIndex) {
        GridIterator iterator(grid, direction, trackIndex);
        accumulateFlexFraction(flexFraction, iterator, direction, itemsSet, gridLayoutState);
    }

    return flexFraction;
}

bool IndefiniteSizeStrategy::recomputeUsedFlexFractionIfNeeded(double& flexFraction, LayoutUnit& totalGrowth) const
{
    if (direction() == GridTrackSizingDirection::ForColumns)
        return false;

    const RenderGrid* renderGrid = this->renderGrid();

    auto minSize = renderGrid->computeContentLogicalHeight(RenderBox::SizeType::MinSize, renderGrid->style().logicalMinHeight(), std::nullopt);
    auto maxSize = renderGrid->computeContentLogicalHeight(RenderBox::SizeType::MaxSize, renderGrid->style().logicalMaxHeight(), std::nullopt);

    // Redo the flex fraction computation using min|max-height as definite available space in case
    // the total height is smaller than min-height or larger than max-height.
    LayoutUnit rowsSize = totalGrowth + computeTrackBasedSize();
    bool checkMinSize = minSize && rowsSize < minSize.value();
    bool checkMaxSize = maxSize && rowsSize > maxSize.value();
    if (!checkMinSize && !checkMaxSize)
        return false;

    LayoutUnit freeSpace = checkMaxSize ? maxSize.value() : -1_lu;
    const Grid& grid = m_algorithm.grid();
    freeSpace = std::max(freeSpace, minSize.value_or(0_lu)) - renderGrid->guttersSize(GridTrackSizingDirection::ForRows, 0, grid.numTracks(GridTrackSizingDirection::ForRows), availableSpace());

    size_t numberOfTracks = m_algorithm.tracks(GridTrackSizingDirection::ForRows).size();
    flexFraction = findFrUnitSize(GridSpan::translatedDefiniteGridSpan(0, numberOfTracks), freeSpace);
    return true;
}

class DefiniteSizeStrategy final : public GridTrackSizingAlgorithmStrategy {
public:
    DefiniteSizeStrategy(GridTrackSizingAlgorithm& algorithm)
        : GridTrackSizingAlgorithmStrategy(algorithm) { }

private:
    void layoutGridItemForMinSizeComputation(RenderBox&, bool overrideSizeHasChanged) const override;
    void maximizeTracks(Vector<GridTrack>&, std::optional<LayoutUnit>& freeSpace) override;
    double findUsedFlexFraction(Vector<unsigned>& flexibleSizedTracksIndex, GridTrackSizingDirection, std::optional<LayoutUnit> freeSpace, GridLayoutState&) const override;
    bool recomputeUsedFlexFractionIfNeeded(double& flexFraction, LayoutUnit& totalGrowth) const override;
    LayoutUnit freeSpaceForStretchAutoTracksStep() const override;
    LayoutUnit minContentContributionForGridItem(RenderBox&, GridLayoutState&) const override;
    LayoutUnit minLogicalSizeForGridItem(RenderBox&, const Length& gridItemMinSize, std::optional<LayoutUnit> availableSize) const override;
    bool isComputingSizeContainment() const override { return false; }
    bool isComputingInlineSizeContainment() const override { return false; }
    bool isComputingSizeOrInlineSizeContainment() const override { return false; }
};

LayoutUnit IndefiniteSizeStrategy::freeSpaceForStretchAutoTracksStep() const
{
    ASSERT(!m_algorithm.freeSpace(direction()));
    if (direction() == GridTrackSizingDirection::ForColumns)
        return 0_lu;

    auto minSize = renderGrid()->computeContentLogicalHeight(RenderBox::SizeType::MinSize, renderGrid()->style().logicalMinHeight(), std::nullopt);
    if (!minSize)
        return 0_lu;
    return minSize.value() - computeTrackBasedSize();
}

LayoutUnit DefiniteSizeStrategy::minLogicalSizeForGridItem(RenderBox& gridItem, const Length& gridItemMinSize, std::optional<LayoutUnit> availableSize) const
{
    GridTrackSizingDirection gridItemInlineDirection = GridLayoutFunctions::flowAwareDirectionForGridItem(*renderGrid(), gridItem, GridTrackSizingDirection::ForColumns);
    GridTrackSizingDirection flowAwareDirection = GridLayoutFunctions::flowAwareDirectionForGridItem(*renderGrid(), gridItem, direction());
    if (hasRelativeMarginOrPaddingForGridItem(gridItem, flowAwareDirection) || (direction() != gridItemInlineDirection && hasRelativeOrIntrinsicSizeForGridItem(gridItem, flowAwareDirection))) {
        auto indefiniteSize = direction() == gridItemInlineDirection ? std::make_optional(0_lu) : std::nullopt;
        setOverridingContainingBlockContentSizeForGridItem(*renderGrid(), gridItem, direction(), indefiniteSize);
    }
    return GridTrackSizingAlgorithmStrategy::minLogicalSizeForGridItem(gridItem, gridItemMinSize, availableSize);
}

void DefiniteSizeStrategy::maximizeTracks(Vector<GridTrack>& tracks, std::optional<LayoutUnit>& freeSpace)
{
    size_t tracksSize = tracks.size();
    Vector<WeakPtr<GridTrack>> tracksForDistribution(tracksSize);
    for (size_t i = 0; i < tracksSize; ++i) {
        tracksForDistribution[i] = tracks.data() + i;
        tracksForDistribution[i]->setPlannedSize(tracksForDistribution[i]->baseSize());
    }

    ASSERT(freeSpace);
    distributeSpaceToTracks(tracksForDistribution, freeSpace.value());

    for (auto& track : tracksForDistribution)
        track->setBaseSize(track->plannedSize());
}


void DefiniteSizeStrategy::layoutGridItemForMinSizeComputation(RenderBox& gridItem, bool overrideSizeHasChanged) const
{
    if (overrideSizeHasChanged)
        gridItem.setNeedsLayout(MarkOnlyThis);
    gridItem.layoutIfNeeded();
}

double DefiniteSizeStrategy::findUsedFlexFraction(Vector<unsigned>&, GridTrackSizingDirection direction, std::optional<LayoutUnit> freeSpace, GridLayoutState&) const
{
    GridSpan allTracksSpan = GridSpan::translatedDefiniteGridSpan(0, m_algorithm.tracks(direction).size());
    ASSERT(freeSpace);
    return findFrUnitSize(allTracksSpan, freeSpace.value());
}

LayoutUnit DefiniteSizeStrategy::freeSpaceForStretchAutoTracksStep() const
{
    return m_algorithm.freeSpace(direction()).value();
}

LayoutUnit DefiniteSizeStrategy::minContentContributionForGridItem(RenderBox& gridItem, GridLayoutState& gridLayoutState) const
{
    GridTrackSizingDirection gridItemInlineDirection = GridLayoutFunctions::flowAwareDirectionForGridItem(*renderGrid(), gridItem, GridTrackSizingDirection::ForColumns);

    auto shouldClearOverridingContainingBlockContentSize = [&] {
        auto direction = this->direction();

        if (!GridLayoutFunctions::isOrthogonalGridItem(*renderGrid(), gridItem) && direction == GridTrackSizingDirection::ForColumns && m_algorithm.isIntrinsicSizedGridArea(gridItem, GridAxis::GridRowAxis) && (gridItem.style().logicalWidth().isPercentOrCalculated() || hasRelativeMarginOrPaddingForGridItem(gridItem, direction)))
            return true;
        return direction == gridItemInlineDirection && gridItem.needsLayout() && shouldClearOverridingContainingBlockContentSizeForGridItem(gridItem, GridTrackSizingDirection::ForColumns);
    }();

    if (shouldClearOverridingContainingBlockContentSize)
        setOverridingContainingBlockContentSizeForGridItem(*renderGrid(), gridItem, gridItemInlineDirection, LayoutUnit());
    return GridTrackSizingAlgorithmStrategy::minContentContributionForGridItem(gridItem, gridLayoutState);
}

bool DefiniteSizeStrategy::recomputeUsedFlexFractionIfNeeded(double& flexFraction, LayoutUnit& totalGrowth) const
{
    UNUSED_PARAM(flexFraction);
    UNUSED_PARAM(totalGrowth);
    return false;
}

// GridTrackSizingAlgorithm steps.

void GridTrackSizingAlgorithm::initializeTrackSizes()
{
    ASSERT(m_contentSizedTracksIndex.isEmpty());
    ASSERT(m_flexibleSizedTracksIndex.isEmpty());
    ASSERT(m_autoSizedTracksForStretchIndex.isEmpty());
    ASSERT(!m_hasPercentSizedRowsIndefiniteHeight);
    ASSERT(!m_hasFlexibleMaxTrackBreadth);

    Vector<GridTrack>& allTracks = tracks(m_direction);
    const bool indefiniteHeight = m_direction == GridTrackSizingDirection::ForRows && !m_renderGrid->hasDefiniteLogicalHeight();
    LayoutUnit maxSize = std::max(0_lu, availableSpace().value_or(0_lu));
    // 1. Initialize per Grid track variables.
    for (unsigned i = 0; i < allTracks.size(); ++i) {
        GridTrack& track = allTracks[i];
        const auto& trackSize = calculateGridTrackSize(m_direction, i);
        track.setCachedTrackSize(trackSize);
        track.setBaseSize(initialBaseSize(trackSize));
        track.setGrowthLimit(initialGrowthLimit(trackSize, track.baseSize()));
        track.setInfinitelyGrowable(false);

        if (trackSize.isFitContent())
            track.setGrowthLimitCap(valueForLength(trackSize.fitContentTrackBreadth().length(), maxSize));
        if (trackSize.isContentSized())
            m_contentSizedTracksIndex.append(i);
        if (trackSize.maxTrackBreadth().isFlex())
            m_flexibleSizedTracksIndex.append(i);
        if (trackSize.hasAutoMaxTrackBreadth() && !trackSize.isFitContent())
            m_autoSizedTracksForStretchIndex.append(i);

        if (indefiniteHeight) {
            auto& rawTrackSize = rawGridTrackSize(m_direction, i);
            // Set the flag for repeating the track sizing algorithm. For flexible tracks, as per spec https://drafts.csswg.org/css-grid/#algo-flex-tracks,
            // in clause "if the free space is an indefinite length:", it states that "If using this flex fraction would cause the grid to be smaller than
            // the grid container’s min-width/height (or larger than the grid container’s max-width/height), then redo this step".
            if (!m_hasFlexibleMaxTrackBreadth && rawTrackSize.maxTrackBreadth().isFlex())
                m_hasFlexibleMaxTrackBreadth = true;
            if (!m_hasPercentSizedRowsIndefiniteHeight && (rawTrackSize.minTrackBreadth().isPercentage() || rawTrackSize.maxTrackBreadth().isPercentage()))
                m_hasPercentSizedRowsIndefiniteHeight = true;
        }
    }
}

static LayoutUnit marginAndBorderAndPaddingForEdge(const RenderGrid& grid, GridTrackSizingDirection direction, bool startEdge)
{
    if (direction == GridTrackSizingDirection::ForColumns)
        return startEdge ? grid.marginAndBorderAndPaddingStart() : grid.marginAndBorderAndPaddingEnd();
    return startEdge ? grid.marginAndBorderAndPaddingBefore() : grid.marginAndBorderAndPaddingAfter();
}

// https://drafts.csswg.org/css-grid-2/#subgrid-edge-placeholders
// FIXME: This is a simplification of the specified behaviour, where we add the hypothetical
// items directly to the edge tracks as if they had a span of 1. This matches the current Gecko
// behavior.
static LayoutUnit computeSubgridMarginBorderPadding(const RenderGrid* outermost, GridTrackSizingDirection outermostDirection, GridTrack& track, unsigned trackIndex, GridSpan& span, RenderGrid* subgrid)
{
    // Convert the direction into the coordinate space of subgrid (which may not be a direct child
    // of the outermost grid for which we're running the track sizing algorithm).
    GridTrackSizingDirection direction = GridLayoutFunctions::flowAwareDirectionForGridItem(*outermost, *subgrid, outermostDirection);
    bool reversed = GridLayoutFunctions::isSubgridReversedDirection(*outermost, outermostDirection, *subgrid);

    LayoutUnit subgridMbp;
    if (trackIndex == span.startLine() && track.cachedTrackSize().hasIntrinsicMinTrackBreadth()) {
        // If the subgrid has a reversed flow direction relative to the outermost grid, then
        // we want the MBP from the end edge in its local coordinate space.
        subgridMbp = marginAndBorderAndPaddingForEdge(*subgrid, direction, !reversed);
    }
    if (trackIndex == span.endLine() - 1 && track.cachedTrackSize().hasIntrinsicMinTrackBreadth())
        subgridMbp += marginAndBorderAndPaddingForEdge(*subgrid, direction, reversed);
    return subgridMbp;
}

static std::optional<LayoutUnit> extraMarginFromSubgridAncestorGutters(const RenderBox& gridItem, GridSpan itemSpan, unsigned trackIndex, GridTrackSizingDirection direction)
{
    if (itemSpan.startLine() != trackIndex && itemSpan.endLine() - 1 != trackIndex)
            return std::nullopt;

    auto gutterTotal = 0_lu;

    for (auto& currentAncestorSubgrid : ancestorSubgridsOfGridItem(gridItem, direction)) {
        std::optional<LayoutUnit> availableSpace;
        if (!GridLayoutFunctions::hasRelativeOrIntrinsicSizeForGridItem(currentAncestorSubgrid, direction))
            availableSpace = currentAncestorSubgrid.availableSpaceForGutters(direction);

        auto gridItemSpanInAncestor = currentAncestorSubgrid.gridSpanForGridItem(gridItem, direction);
        auto numTracksForCurrentAncestor = currentAncestorSubgrid.numTracks(direction);

        const auto* currentAncestorSubgridParent = dynamicDowncast<RenderGrid>(currentAncestorSubgrid.parent());
        ASSERT(currentAncestorSubgridParent);
        if (!currentAncestorSubgridParent)
            return std::nullopt;

        if (gridItemSpanInAncestor.startLine())
            gutterTotal += (currentAncestorSubgrid.gridGap(direction) - currentAncestorSubgridParent->gridGap(direction)) / 2;
        if (itemSpan.endLine() != numTracksForCurrentAncestor)
            gutterTotal += (currentAncestorSubgrid.gridGap(direction) - currentAncestorSubgridParent->gridGap(direction)) / 2;
        direction = GridLayoutFunctions::flowAwareDirectionForParent(currentAncestorSubgrid, *currentAncestorSubgridParent, direction);
    }
    return gutterTotal;
}

bool GridTrackSizingAlgorithm::shouldExcludeGridItemForMasonryTrackSizing(const RenderBox& gridItem, unsigned trackIndex, GridSpan itemSpan) const
{
    bool shouldExcludeGridItemForMasonryTrackSizing = true;

    // Items specifically placed in this track.
    if (m_renderGrid->gridSpanForGridItem(gridItem, m_direction).startLine() == trackIndex)
        shouldExcludeGridItemForMasonryTrackSizing = false;

    // Items that have an indefinite placement in the grid axis.
    if (GridPositionsResolver::resolveGridPositionsFromStyle(*m_renderGrid, gridItem, m_direction).isIndefinite())
        shouldExcludeGridItemForMasonryTrackSizing = false;

    // If the item is going past the end of track do not consider it for inclusion.
    if (itemSpan.integerSpan() + itemSpan.startLine() > tracks(m_direction).size())
        shouldExcludeGridItemForMasonryTrackSizing = true;

    return shouldExcludeGridItemForMasonryTrackSizing;
}

void GridTrackSizingAlgorithm::accumulateIntrinsicSizesForTrack(GridTrack& track, unsigned trackIndex, GridIterator& iterator, Vector<GridItemWithSpan>& itemsSortedByIncreasingSpan, Vector<GridItemWithSpan>& itemsCrossingFlexibleTracks, SingleThreadWeakHashSet<RenderBox>& itemsSet, LayoutUnit currentAccumulatedMbp, GridLayoutState& gridLayoutState)
{
    auto accumulateIntrinsicSizes = [&](RenderBox* gridItem) {
        bool isNewEntry = itemsSet.add(*gridItem).isNewEntry;
        GridSpan span = m_renderGrid->gridSpanForGridItem(*gridItem, m_direction);

        if (CheckedPtr inner = dynamicDowncast<RenderGrid>(gridItem); inner && inner->isSubgridInParentDirection(iterator.direction())) {
            // Contribute the mbp of wrapper to the first and last tracks that we span.
            GridSpan subgridSpan = downcast<RenderGrid>(inner->parent())->gridSpanForGridItem(*inner, iterator.direction());
            auto accumulatedMbpWithSubgrid = currentAccumulatedMbp + computeSubgridMarginBorderPadding(m_renderGrid, m_direction, track, trackIndex, span, inner.get());
            track.setBaseSize(std::max(track.baseSize(), accumulatedMbpWithSubgrid + extraMarginFromSubgridAncestorGutters(*gridItem, span, trackIndex, iterator.direction()).value_or(0_lu)));

            GridIterator subgridIterator = GridIterator::createForSubgrid(*inner, iterator, subgridSpan);

            accumulateIntrinsicSizesForTrack(track, trackIndex, subgridIterator, itemsSortedByIncreasingSpan, itemsCrossingFlexibleTracks, itemsSet, accumulatedMbpWithSubgrid, gridLayoutState);
            return;
        }

        if (!isNewEntry)
            return;

        if (spanningItemCrossesFlexibleSizedTracks(span))
            itemsCrossingFlexibleTracks.append(GridItemWithSpan(*gridItem, span));
        else if (span.integerSpan() == 1)
            sizeTrackToFitNonSpanningItem(span, *gridItem, track, gridLayoutState);
        else
            itemsSortedByIncreasingSpan.append(GridItemWithSpan(*gridItem, span));
    };

    while (CheckedPtr gridItem = iterator.nextGridItem())
        accumulateIntrinsicSizes(gridItem.get());
}

void GridTrackSizingAlgorithm::computeDefiniteAndIndefiniteItemsForMasonry(StdMap<SpanLength, MasonryMinMaxTrackSize>& indefiniteSpanSizes, StdMap<SpanLength, Vector<MasonryMinMaxTrackSizeWithGridSpan>>& definiteItemSizes, Vector<MasonryMinMaxTrackSizeWithGridSpan>& definiteItemSizesSpanFlexTrack, GridLayoutState& gridLayoutState)
{
    auto populateDefiniteItems = [&](unsigned trackIndex, GridSpan& gridSpan, unsigned spanLength, RenderBox* gridItem, Vector<GridTrack> & allTracks) {
        if (gridSpan.startLine() != trackIndex)
            return;

        auto minContentContributionForGridItem = m_strategy->minContentContributionForGridItem(*gridItem, gridLayoutState);
        auto maxContentContributionForGridItem = m_strategy->maxContentContributionForGridItem(*gridItem, gridLayoutState);
        auto minContributionForGridItem = m_strategy->minContributionForGridItem(*gridItem, gridLayoutState);

        bool spansFlexTracks = spanningItemCrossesFlexibleSizedTracks(gridSpan);

        if (spanLength == 1 && !spansFlexTracks)
            sizeTrackToFitNonSpanningItem(gridSpan, *gridItem, allTracks[trackIndex], gridLayoutState);
        else {
            auto minMaxTrackSizeWithGridSpan = MasonryMinMaxTrackSizeWithGridSpan { MasonryMinMaxTrackSize { minContentContributionForGridItem, maxContentContributionForGridItem, minContributionForGridItem }, gridSpan };

            if (spansFlexTracks)
                definiteItemSizesSpanFlexTrack.append(minMaxTrackSizeWithGridSpan);
            else
                definiteItemSizes[spanLength].append(minMaxTrackSizeWithGridSpan);
        }
    };

    auto populateIndefiniteItems = [&](RenderBox* gridItem, unsigned spanLength) {

        auto minContentContributionForGridItem = m_strategy->minContentContributionForGridItem(*gridItem, gridLayoutState);
        auto maxContentContributionForGridItem = m_strategy->maxContentContributionForGridItem(*gridItem, gridLayoutState);
        auto minContributionForGridItem = m_strategy->minContributionForGridItem(*gridItem, gridLayoutState);

        if (!indefiniteSpanSizes.contains(spanLength))
            indefiniteSpanSizes.insert({ spanLength, { } });

        auto& trackSize = indefiniteSpanSizes.find(spanLength)->second;

        trackSize.minContentSize = std::max(trackSize.minContentSize, minContentContributionForGridItem);
        trackSize.maxContentSize = std::max(trackSize.maxContentSize, maxContentContributionForGridItem);
        trackSize.minSize = std::max(trackSize.minSize, minContributionForGridItem);
    };

    auto& allTracks = tracks(m_direction);
    auto trackLength = allTracks.size();
    for (auto trackIndex = 0u; trackIndex < trackLength; trackIndex++) {
        GridIterator iterator(m_grid, m_direction, trackIndex);

        while (CheckedPtr gridItem = iterator.nextGridItem()) {
            auto gridSpan = m_renderGrid->gridSpanForGridItem(*gridItem, m_direction);
            auto spanLength = gridSpan.integerSpan();

            if (!GridPositionsResolver::resolveGridPositionsFromStyle(*m_renderGrid, *gridItem, m_direction).isIndefinite()) {
                populateDefiniteItems(trackIndex, gridSpan, spanLength, gridItem.get(), allTracks);
                continue;
            }

            auto endLine = trackIndex + spanLength;
            if (endLine > trackLength)
                continue;

            populateIndefiniteItems(gridItem.get(), spanLength);
        }
    }

}

void GridTrackSizingAlgorithm::handleInfinityGrowthLimit()
{
    Vector<GridTrack>& allTracks = tracks(m_direction);
    for (auto trackIndex : m_contentSizedTracksIndex) {
        GridTrack& track = allTracks[trackIndex];
        if (track.growthLimit() == infinity)
            track.setGrowthLimit(track.baseSize());
    }
}

void GridTrackSizingAlgorithm::resolveIntrinsicTrackSizes(GridLayoutState& gridLayoutState)
{
    if (m_strategy->isComputingSizeContainment()) {
        handleInfinityGrowthLimit();
        return;
    }

    Vector<GridTrack>& allTracks = tracks(m_direction);
    Vector<GridItemWithSpan> itemsSortedByIncreasingSpan;
    Vector<GridItemWithSpan> itemsCrossingFlexibleTracks;
    SingleThreadWeakHashSet<RenderBox> itemsSet;

    if (m_grid.hasGridItems()) {
        for (auto trackIndex : m_contentSizedTracksIndex) {
            GridIterator iterator(m_grid, m_direction, trackIndex);
            GridTrack& track = allTracks[trackIndex];

            accumulateIntrinsicSizesForTrack(track, trackIndex, iterator, itemsSortedByIncreasingSpan, itemsCrossingFlexibleTracks, itemsSet, 0_lu, gridLayoutState);
        }
        std::sort(itemsSortedByIncreasingSpan.begin(), itemsSortedByIncreasingSpan.end());
    }

    auto it = itemsSortedByIncreasingSpan.begin();
    auto end = itemsSortedByIncreasingSpan.end();
    while (it != end) {
        GridItemsSpanGroupRange spanGroupRange = { it, std::upper_bound(it, end, *it) };
        increaseSizesToAccommodateSpanningItems<TrackSizeComputationVariant::NotCrossingFlexibleTracks>(spanGroupRange, gridLayoutState);
        it = spanGroupRange.rangeEnd;
    }
    GridItemsSpanGroupRange tracksGroupRange = { itemsCrossingFlexibleTracks.begin(), itemsCrossingFlexibleTracks.end() };
    increaseSizesToAccommodateSpanningItems<TrackSizeComputationVariant::CrossingFlexibleTracks>(tracksGroupRange, gridLayoutState);
    handleInfinityGrowthLimit();
}

void GridTrackSizingAlgorithm::resolveIntrinsicTrackSizesMasonry(GridLayoutState& gridLayoutState)
{
    if (m_strategy->isComputingSizeContainment() || !m_grid.hasGridItems()) {
        handleInfinityGrowthLimit();
        return;
    }
    StdMap<SpanLength, MasonryMinMaxTrackSize> indefiniteSpanSizes;
    StdMap<SpanLength, Vector<MasonryMinMaxTrackSizeWithGridSpan>> definiteItemSizes;
    Vector<MasonryMinMaxTrackSizeWithGridSpan> definiteItemSizesSpanFlexTrack;

    computeDefiniteAndIndefiniteItemsForMasonry(indefiniteSpanSizes, definiteItemSizes, definiteItemSizesSpanFlexTrack, gridLayoutState);

    // Update intrinsic tracks with single span items that do not cross flex tracks.
    auto& allTracks = tracks(m_direction);

    if (auto item = indefiniteSpanSizes.find(1); item != indefiniteSpanSizes.end()) {
        auto& singleTrackSpanSize = item->second;
        for (auto trackIndex : m_contentSizedTracksIndex) {
            auto& track = allTracks[trackIndex];

            auto itemSpan = GridSpan::translatedDefiniteGridSpan(trackIndex, trackIndex + 1);
            if (spanningItemCrossesFlexibleSizedTracks(itemSpan))
                continue;

            sizeTrackToFitSingleSpanMasonryGroup(itemSpan, singleTrackSpanSize, track);
        }
    }

    convertIndefiniteItemsToDefiniteMasonry(indefiniteSpanSizes, definiteItemSizes, definiteItemSizesSpanFlexTrack);

    increaseSizesToAccommodateSpanningItemsMasonry<TrackSizeComputationVariant::NotCrossingFlexibleTracks>(definiteItemSizes);

    increaseSizesToAccommodateSpanningItemsMasonryWithFlex<TrackSizeComputationVariant::CrossingFlexibleTracks>(definiteItemSizesSpanFlexTrack);

    handleInfinityGrowthLimit();
}

void GridTrackSizingAlgorithm::stretchFlexibleTracks(std::optional<LayoutUnit> freeSpace, GridLayoutState& gridLayoutState)
{
    if (m_flexibleSizedTracksIndex.isEmpty())
        return;

    double flexFraction = m_strategy->findUsedFlexFraction(m_flexibleSizedTracksIndex, m_direction, freeSpace, gridLayoutState);

    LayoutUnit totalGrowth;
    Vector<LayoutUnit> increments;
    increments.grow(m_flexibleSizedTracksIndex.size());
    computeFlexSizedTracksGrowth(flexFraction, increments, totalGrowth);

    if (m_strategy->recomputeUsedFlexFractionIfNeeded(flexFraction, totalGrowth)) {
        totalGrowth = 0_lu;
        computeFlexSizedTracksGrowth(flexFraction, increments, totalGrowth);
    }

    size_t i = 0;
    Vector<GridTrack>& allTracks = tracks(m_direction);
    for (auto trackIndex : m_flexibleSizedTracksIndex) {
        auto& track = allTracks[trackIndex];
        if (LayoutUnit increment = increments[i++])
            track.setBaseSize(track.baseSize() + increment);
    }
    if (this->freeSpace(m_direction))
        setFreeSpace(m_direction, this->freeSpace(m_direction).value() - totalGrowth);
    m_maxContentSize += totalGrowth;
}

void GridTrackSizingAlgorithm::stretchAutoTracks()
{
    auto currentFreeSpace = m_strategy->freeSpaceForStretchAutoTracksStep();
    if (m_autoSizedTracksForStretchIndex.isEmpty() || currentFreeSpace <= 0
        || (m_renderGrid->contentAlignment(m_direction).distribution() != ContentDistribution::Stretch))
        return;

    Vector<GridTrack>& allTracks = tracks(m_direction);
    unsigned numberOfAutoSizedTracks = m_autoSizedTracksForStretchIndex.size();
    LayoutUnit sizeToIncrease = currentFreeSpace / numberOfAutoSizedTracks;
    for (const auto& trackIndex : m_autoSizedTracksForStretchIndex) {
        auto& track = allTracks[trackIndex];
        track.setBaseSize(track.baseSize() + sizeToIncrease);
    }
    setFreeSpace(m_direction, 0_lu);
}

void GridTrackSizingAlgorithm::advanceNextState()
{
    switch (m_sizingState) {
    case SizingState::ColumnSizingFirstIteration:
        m_sizingState = SizingState::RowSizingFirstIteration;
        return;
    case SizingState::RowSizingFirstIteration:
        m_sizingState = m_strategy->isComputingSizeContainment() ? SizingState::RowSizingExtraIterationForSizeContainment : SizingState::ColumnSizingSecondIteration;
        return;
    case SizingState::RowSizingExtraIterationForSizeContainment:
        m_sizingState = SizingState::ColumnSizingSecondIteration;
        return;
    case SizingState::ColumnSizingSecondIteration:
        m_sizingState = SizingState::RowSizingSecondIteration;
        return;
    case SizingState::RowSizingSecondIteration:
        m_sizingState = SizingState::ColumnSizingFirstIteration;
        return;
    }
    ASSERT_NOT_REACHED();
    m_sizingState = SizingState::ColumnSizingFirstIteration;
}

bool GridTrackSizingAlgorithm::isValidTransition() const
{
    switch (m_sizingState) {
    case SizingState::ColumnSizingFirstIteration:
    case SizingState::ColumnSizingSecondIteration:
        return m_direction == GridTrackSizingDirection::ForColumns;
    case SizingState::RowSizingFirstIteration:
    case SizingState::RowSizingExtraIterationForSizeContainment:
    case SizingState::RowSizingSecondIteration:
        return m_direction == GridTrackSizingDirection::ForRows;
    }
    ASSERT_NOT_REACHED();
    return false;
}

// GridTrackSizingAlgorithm API.

void GridTrackSizingAlgorithm::setup(GridTrackSizingDirection direction, unsigned numTracks, SizingOperation sizingOperation, std::optional<LayoutUnit> availableSpace)
{
    ASSERT(m_needsSetup);
    m_direction = direction;
    setAvailableSpace(direction, availableSpace ? std::max(0_lu, *availableSpace) : availableSpace);

    m_sizingOperation = sizingOperation;
    switch (m_sizingOperation) {
    case SizingOperation::IntrinsicSizeComputation:
        m_strategy = makeUnique<IndefiniteSizeStrategy>(*this);
        break;
    case SizingOperation::TrackSizing:
        m_strategy = makeUnique<DefiniteSizeStrategy>(*this);
        break;
    }

    m_contentSizedTracksIndex.shrink(0);
    m_flexibleSizedTracksIndex.shrink(0);
    m_autoSizedTracksForStretchIndex.shrink(0);

    if (availableSpace) {
        LayoutUnit guttersSize = m_renderGrid->guttersSize(direction, 0, m_grid.numTracks(direction), this->availableSpace(direction));
        setFreeSpace(direction, *availableSpace - guttersSize);
    } else
        setFreeSpace(direction, std::nullopt);
    tracks(direction).resize(numTracks);

    m_needsSetup = false;
    m_hasPercentSizedRowsIndefiniteHeight = false;
    m_hasFlexibleMaxTrackBreadth = false;

    auto resolveAndSetNonAutoRowStartMarginsOnRowSubgrids = [&] {
        for (auto& subgrid : m_rowSubgridsWithBaselineAlignedItems) {
            const auto subgridSpan = m_renderGrid->gridSpanForGridItem(subgrid, GridTrackSizingDirection::ForColumns);
            const auto subgridRowStartMargin = subgrid.style().marginBeforeUsing(&m_renderGrid->style());
            if (!subgridRowStartMargin.isAuto())
                m_renderGrid->setMarginBeforeForChild(subgrid, minimumValueForLength(subgridRowStartMargin, computeGridSpanSize(tracks(GridTrackSizingDirection::ForColumns), subgridSpan, std::make_optional(m_renderGrid->gridItemOffset(direction)), m_renderGrid->guttersSize(GridTrackSizingDirection::ForColumns, subgridSpan.startLine(), subgridSpan.integerSpan(), this->availableSpace(GridTrackSizingDirection::ForColumns)))));
        }
    };
    if (m_direction == GridTrackSizingDirection::ForRows && (m_sizingState == SizingState::RowSizingFirstIteration || m_sizingState == SizingState::RowSizingSecondIteration))
        resolveAndSetNonAutoRowStartMarginsOnRowSubgrids();

    computeBaselineAlignmentContext();
}

void GridTrackSizingAlgorithm::computeBaselineAlignmentContext()
{
    GridAxis axis = gridAxisForDirection(m_direction);
    m_baselineAlignment.clear(axis);
    m_baselineAlignment.setWritingMode(m_renderGrid->style().writingMode());
    BaselineItemsCache& baselineItemsCache = axis == GridAxis::GridColumnAxis ? m_columnBaselineItemsMap : m_rowBaselineItemsMap;
    BaselineItemsCache tmpBaselineItemsCache = baselineItemsCache;
    for (auto& gridItem : tmpBaselineItemsCache.keys()) {
        // FIXME (jfernandez): We may have to get rid of the baseline participation
        // flag (hence just using a HashSet) depending on the CSS WG resolution on
        // https://github.com/w3c/csswg-drafts/issues/3046
        if (canParticipateInBaselineAlignment(gridItem, axis)) {
            updateBaselineAlignmentContext(gridItem, axis);
            baselineItemsCache.set(gridItem, true);
        } else
            baselineItemsCache.set(gridItem, false);
    }
}

static void removeSubgridMarginBorderPaddingFromTracks(Vector<GridTrack>& tracks, LayoutUnit mbp, bool forwards)
{
    int numTracks = tracks.size();
    int i = forwards ? 0 : numTracks - 1;
    while (mbp > 0 && (forwards ? i < numTracks : i >= 0)) {
        LayoutUnit size = tracks[i].baseSize();
        if (size > mbp) {
            size -= mbp;
            mbp = 0;
        } else {
            mbp -= size;
            size = 0;
        }
        tracks[i].setBaseSize(size);

        forwards ? i++ : i--;
    }
}

bool GridTrackSizingAlgorithm::copyUsedTrackSizesForSubgrid()
{
    RenderGrid* outer = downcast<RenderGrid>(m_renderGrid->parent());
    GridTrackSizingAlgorithm& parentAlgo = outer->m_trackSizingAlgorithm;
    GridTrackSizingDirection direction = GridLayoutFunctions::flowAwareDirectionForParent(*m_renderGrid, *outer, m_direction);
    Vector<GridTrack>& parentTracks = parentAlgo.tracks(direction);

    if (!parentTracks.size())
        return false;

    GridSpan span = outer->gridSpanForGridItem(*m_renderGrid, direction);
    Vector<GridTrack>& allTracks = tracks(m_direction);
    int numTracks = allTracks.size();
    RELEASE_ASSERT((parentTracks.size()  - 1) >= (numTracks - 1 + span.startLine()));
    for (int i = 0; i < numTracks; i++)
        allTracks[i] = parentTracks[i + span.startLine()];

    if (GridLayoutFunctions::isSubgridReversedDirection(*outer, direction, *m_renderGrid))
        allTracks.reverse();

    LayoutUnit startMBP = (m_direction == GridTrackSizingDirection::ForColumns) ? m_renderGrid->marginAndBorderAndPaddingStart() : m_renderGrid->marginAndBorderAndPaddingBefore();
    removeSubgridMarginBorderPaddingFromTracks(allTracks, startMBP, true);
    LayoutUnit endMBP = (m_direction == GridTrackSizingDirection::ForColumns) ? m_renderGrid->marginAndBorderAndPaddingEnd() : m_renderGrid->marginAndBorderAndPaddingAfter();
    removeSubgridMarginBorderPaddingFromTracks(allTracks, endMBP, false);

    LayoutUnit gapDifference = (m_renderGrid->gridGap(m_direction, availableSpace(m_direction)) - outer->gridGap(direction)) / 2;
    for (int i = 0; i < numTracks; i++) {
        LayoutUnit size = allTracks[i].baseSize();
        if (i)
            size -= gapDifference;
        if (i != numTracks - 1)
            size -= gapDifference;
        allTracks[i].setBaseSize(size);
    }
    return true;
}

void GridTrackSizingAlgorithm::run(GridTrackSizingDirection direction, unsigned numTracks, SizingOperation sizingOperation, std::optional<LayoutUnit> availableSpace, GridLayoutState& gridLayoutState)
{
    setup(direction, numTracks, sizingOperation, availableSpace);

    StateMachine stateMachine(*this);

    if (m_renderGrid->isMasonry(m_direction))
        return;

    if (m_renderGrid->isSubgrid(m_direction) && copyUsedTrackSizesForSubgrid())
        return;

    // Step 1.
    const std::optional<LayoutUnit> initialFreeSpace = freeSpace(m_direction);
    initializeTrackSizes();

    // Step 2.
    if (!m_contentSizedTracksIndex.isEmpty())
        (!m_renderGrid->isMasonry()) ? resolveIntrinsicTrackSizes(gridLayoutState) : resolveIntrinsicTrackSizesMasonry(gridLayoutState);

    // This is not exactly a step of the track sizing algorithm, but we use the track sizes computed
    // up to this moment (before maximization) to calculate the grid container intrinsic sizes.
    computeGridContainerIntrinsicSizes();

    if (freeSpace(m_direction)) {
        LayoutUnit updatedFreeSpace = freeSpace(m_direction).value() - m_minContentSize;
        setFreeSpace(m_direction, updatedFreeSpace);
        if (updatedFreeSpace <= 0)
            return;
    }

    // Step 3.
    m_strategy->maximizeTracks(tracks(m_direction), m_direction == GridTrackSizingDirection::ForColumns ? m_freeSpaceColumns : m_freeSpaceRows);

    // Step 4.
    stretchFlexibleTracks(initialFreeSpace, gridLayoutState);

    // Step 5.
    stretchAutoTracks();
}

void GridTrackSizingAlgorithm::reset()
{
    ASSERT(wasSetup());
    m_sizingState = SizingState::ColumnSizingFirstIteration;
    m_columns.shrink(0);
    m_rows.shrink(0);
    m_contentSizedTracksIndex.shrink(0);
    m_flexibleSizedTracksIndex.shrink(0);
    m_autoSizedTracksForStretchIndex.shrink(0);
    setAvailableSpace(GridTrackSizingDirection::ForRows, std::nullopt);
    setAvailableSpace(GridTrackSizingDirection::ForColumns, std::nullopt);
    m_hasPercentSizedRowsIndefiniteHeight = false;
    m_hasFlexibleMaxTrackBreadth = false;
}

#if ASSERT_ENABLED
bool GridTrackSizingAlgorithm::tracksAreWiderThanMinTrackBreadth() const
{
    // Subgrids inherit their sizing directly from the parent, so may be unrelated
    // to their initial base size.
    if (m_renderGrid->isSubgrid(m_direction))
        return true;

    if (m_renderGrid->isMasonry(m_direction))
        return true;

    const Vector<GridTrack>& allTracks = tracks(m_direction);
    for (size_t i = 0; i < allTracks.size(); ++i) {
        const auto& trackSize = allTracks[i].cachedTrackSize();
        if (initialBaseSize(trackSize) > allTracks[i].baseSize())
            return false;
    }
    return true;
}
#endif // ASSERT_ENABLED

GridTrackSizingAlgorithm::StateMachine::StateMachine(GridTrackSizingAlgorithm& algorithm)
    : m_algorithm(algorithm)
{
    ASSERT(m_algorithm.isValidTransition());
    ASSERT(!m_algorithm.m_needsSetup);
}

GridTrackSizingAlgorithm::StateMachine::~StateMachine()
{
    m_algorithm.advanceNextState();
    m_algorithm.m_needsSetup = true;
}

bool GridTrackSizingAlgorithm::isDirectionInMasonryDirection() const
{
    return m_renderGrid->isMasonry(m_direction);
}

} // namespace WebCore
