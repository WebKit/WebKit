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

#include "config.h"
#include "GridTrackSizingAlgorithm.h"

#include "Grid.h"
#include "GridArea.h"
#include "GridLayoutFunctions.h"
#include "RenderGrid.h"
#include "rendering/style/RenderStyleConstants.h"

namespace WebCore {

const LayoutUnit& GridTrack::baseSize() const
{
    ASSERT(isGrowthLimitBiggerThanBaseSize());
    return m_baseSize;
}

const LayoutUnit& GridTrack::growthLimit() const
{
    ASSERT(isGrowthLimitBiggerThanBaseSize());
    ASSERT(!m_growthLimitCap || m_growthLimitCap.value() >= m_growthLimit || m_baseSize >= m_growthLimitCap.value());
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

const LayoutUnit& GridTrack::growthLimitIfNotInfinite() const
{
    ASSERT(isGrowthLimitBiggerThanBaseSize());
    return m_growthLimit == infinity ? m_baseSize : m_growthLimit;
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

void GridTrack::setCachedTrackSize(const GridTrackSize& cachedTrackSize)
{
    m_cachedTrackSize = cachedTrackSize;
}

void GridTrack::ensureGrowthLimitIsBiggerThanBaseSize()
{
    if (m_growthLimit != infinity && m_growthLimit < m_baseSize)
        m_growthLimit = m_baseSize;
}

// Static helper methods.

static GridAxis gridAxisForDirection(GridTrackSizingDirection direction)
{
    return direction == ForColumns ? GridRowAxis : GridColumnAxis;
}

static GridTrackSizingDirection gridDirectionForAxis(GridAxis axis)
{
    return axis == GridRowAxis ? ForColumns : ForRows;
}

static bool hasRelativeMarginOrPaddingForChild(const RenderBox& child, GridTrackSizingDirection direction)
{
    if (direction == ForColumns)
        return child.style().marginStart().isPercentOrCalculated() || child.style().marginEnd().isPercentOrCalculated() || child.style().paddingStart().isPercentOrCalculated() || child.style().paddingEnd().isPercentOrCalculated();
    return child.style().marginBefore().isPercentOrCalculated() || child.style().marginAfter().isPercentOrCalculated() || child.style().paddingBefore().isPercentOrCalculated() || child.style().paddingAfter().isPercentOrCalculated();
}

static bool hasRelativeOrIntrinsicSizeForChild(const RenderBox& child, GridTrackSizingDirection direction)
{
    if (direction == ForColumns)
        return child.hasRelativeLogicalWidth() || child.style().logicalWidth().isIntrinsicOrAuto();
    return child.hasRelativeLogicalHeight() || child.style().logicalHeight().isIntrinsicOrAuto();
}

static bool shouldClearOverridingContainingBlockContentSizeForChild(const RenderBox& child, GridTrackSizingDirection direction)
{
    return hasRelativeOrIntrinsicSizeForChild(child, direction) || hasRelativeMarginOrPaddingForChild(child, direction);
}

static void setOverridingContainingBlockContentSizeForChild(const RenderGrid& grid, RenderBox& child, GridTrackSizingDirection direction, std::optional<LayoutUnit> size)
{
    // This function sets the dimension based on the writing mode of the containing block.
    // For subgrids, this might not be the outermost grid, but could be a subgrid. If the
    // writing mode of the CB and the grid for which we're doing sizing don't match, swap
    // the directions.
    direction = GridLayoutFunctions::flowAwareDirectionForChild(grid, *child.containingBlock(), direction);
    if (direction == ForColumns)
        child.setOverridingContainingBlockContentLogicalWidth(size);
    else
        child.setOverridingContainingBlockContentLogicalHeight(size);
}

// GridTrackSizingAlgorithm private.

void GridTrackSizingAlgorithm::setFreeSpace(GridTrackSizingDirection direction, std::optional<LayoutUnit> freeSpace)
{
    if (direction == ForColumns)
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
    if (direction == ForColumns)
        m_availableSpaceColumns = availableSpace;
    else
        m_availableSpaceRows = availableSpace;
}

const GridTrackSize& GridTrackSizingAlgorithm::rawGridTrackSize(GridTrackSizingDirection direction, unsigned translatedIndex) const
{
    bool isRowAxis = direction == ForColumns;
    auto& renderStyle = m_renderGrid->style();
    auto& trackStyles = isRowAxis ? renderStyle.gridColumns() : renderStyle.gridRows();
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
        // We need to traspose the index because the first negative implicit line will get the last defined auto track and so on.
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
    LayoutUnit size;
    auto& allTracks = tracks(m_direction);
    for (auto& track : allTracks)
        size += track.baseSize();

    size += m_renderGrid->guttersSize(m_grid, m_direction, 0, allTracks.size(), availableSpace());

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

void GridTrackSizingAlgorithm::sizeTrackToFitNonSpanningItem(const GridSpan& span, RenderBox& gridItem, GridTrack& track)
{
    unsigned trackPosition = span.startLine();
    const auto& trackSize = tracks(m_direction)[trackPosition].cachedTrackSize();

    if (trackSize.hasMinContentMinTrackBreadth()) {
        track.setBaseSize(std::max(track.baseSize(), m_strategy->minContentForChild(gridItem)));
    } else if (trackSize.hasMaxContentMinTrackBreadth()) {
        track.setBaseSize(std::max(track.baseSize(), m_strategy->maxContentForChild(gridItem)));
    } else if (trackSize.hasAutoMinTrackBreadth()) {
        track.setBaseSize(std::max(track.baseSize(), m_strategy->minSizeForChild(gridItem)));
    }

    if (trackSize.hasMinContentMaxTrackBreadth()) {
        track.setGrowthLimit(std::max(track.growthLimit(), m_strategy->minContentForChild(gridItem)));
    } else if (trackSize.hasMaxContentOrAutoMaxTrackBreadth()) {
        LayoutUnit growthLimit = m_strategy->maxContentForChild(gridItem);
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

enum TrackSizeRestriction {
    AllowInfinity,
    ForbidInfinity,
};

LayoutUnit GridTrackSizingAlgorithm::itemSizeForTrackSizeComputationPhase(TrackSizeComputationPhase phase, RenderBox& gridItem) const
{
    switch (phase) {
    case ResolveIntrinsicMinimums:
        return m_strategy->minSizeForChild(gridItem);
    case ResolveContentBasedMinimums:
    case ResolveIntrinsicMaximums:
        return m_strategy->minContentForChild(gridItem);
    case ResolveMaxContentMinimums:
    case ResolveMaxContentMaximums:
        return m_strategy->maxContentForChild(gridItem);
    case MaximizeTracks:
        ASSERT_NOT_REACHED();
        return 0;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

static bool shouldProcessTrackForTrackSizeComputationPhase(TrackSizeComputationPhase phase, const GridTrackSize& trackSize)
{
    switch (phase) {
    case ResolveIntrinsicMinimums:
        return trackSize.hasIntrinsicMinTrackBreadth();
    case ResolveContentBasedMinimums:
        return trackSize.hasMinOrMaxContentMinTrackBreadth();
    case ResolveMaxContentMinimums:
        return trackSize.hasMaxContentMinTrackBreadth();
    case ResolveIntrinsicMaximums:
        return trackSize.hasIntrinsicMaxTrackBreadth();
    case ResolveMaxContentMaximums:
        return trackSize.hasMaxContentOrAutoMaxTrackBreadth();
    case MaximizeTracks:
        ASSERT_NOT_REACHED();
        return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}

static LayoutUnit trackSizeForTrackSizeComputationPhase(TrackSizeComputationPhase phase, GridTrack& track, TrackSizeRestriction restriction)
{
    switch (phase) {
    case ResolveIntrinsicMinimums:
    case ResolveContentBasedMinimums:
    case ResolveMaxContentMinimums:
    case MaximizeTracks:
        return track.baseSize();
    case ResolveIntrinsicMaximums:
    case ResolveMaxContentMaximums:
        return restriction == AllowInfinity ? track.growthLimit() : track.growthLimitIfNotInfinite();
    }

    ASSERT_NOT_REACHED();
    return track.baseSize();
}

static void updateTrackSizeForTrackSizeComputationPhase(TrackSizeComputationPhase phase, GridTrack& track)
{
    switch (phase) {
    case ResolveIntrinsicMinimums:
    case ResolveContentBasedMinimums:
    case ResolveMaxContentMinimums:
        track.setBaseSize(track.plannedSize());
        return;
    case ResolveIntrinsicMaximums:
    case ResolveMaxContentMaximums:
        track.setGrowthLimit(track.plannedSize());
        return;
    case MaximizeTracks:
        ASSERT_NOT_REACHED();
        return;
    }

    ASSERT_NOT_REACHED();
}

static bool trackShouldGrowBeyondGrowthLimitsForTrackSizeComputationPhase(TrackSizeComputationPhase phase, const GridTrackSize& trackSize)
{
    switch (phase) {
    case ResolveIntrinsicMinimums:
    case ResolveContentBasedMinimums:
        return trackSize.hasAutoOrMinContentMinTrackBreadthAndIntrinsicMaxTrackBreadth();
    case ResolveMaxContentMinimums:
        return trackSize.hasMaxContentMinTrackBreadthAndMaxContentMaxTrackBreadth();
    case ResolveIntrinsicMaximums:
    case ResolveMaxContentMaximums:
        return true;
    case MaximizeTracks:
        ASSERT_NOT_REACHED();
        return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}

static void markAsInfinitelyGrowableForTrackSizeComputationPhase(TrackSizeComputationPhase phase, GridTrack& track)
{
    switch (phase) {
    case ResolveIntrinsicMinimums:
    case ResolveContentBasedMinimums:
    case ResolveMaxContentMinimums:
        return;
    case ResolveIntrinsicMaximums:
        if (trackSizeForTrackSizeComputationPhase(phase, track, AllowInfinity) == infinity  && track.plannedSize() != infinity)
            track.setInfinitelyGrowable(true);
        return;
    case ResolveMaxContentMaximums:
        if (track.infinitelyGrowable())
            track.setInfinitelyGrowable(false);
        return;
    case MaximizeTracks:
        ASSERT_NOT_REACHED();
        return;
    }

    ASSERT_NOT_REACHED();
}

template <TrackSizeComputationVariant variant, TrackSizeComputationPhase phase>
void GridTrackSizingAlgorithm::increaseSizesToAccommodateSpanningItems(const GridItemsSpanGroupRange& gridItemsWithSpan)
{
    Vector<GridTrack>& allTracks = tracks(m_direction);
    for (const auto& trackIndex : m_contentSizedTracksIndex) {
        GridTrack& track = allTracks[trackIndex];
        track.setPlannedSize(trackSizeForTrackSizeComputationPhase(phase, track, AllowInfinity));
    }

    Vector<GridTrack*> growBeyondGrowthLimitsTracks;
    Vector<GridTrack*> filteredTracks;
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
            spanningTracksSize += trackSizeForTrackSizeComputationPhase(phase, track, ForbidInfinity);
            if (variant == TrackSizeComputationVariant::CrossingFlexibleTracks && !trackSize.maxTrackBreadth().isFlex())
                continue;
            if (!shouldProcessTrackForTrackSizeComputationPhase(phase, trackSize))
                continue;

            filteredTracks.append(&track);

            if (trackShouldGrowBeyondGrowthLimitsForTrackSizeComputationPhase(phase, trackSize))
                growBeyondGrowthLimitsTracks.append(&track);
        }

        if (filteredTracks.isEmpty())
            continue;

        spanningTracksSize += m_renderGrid->guttersSize(m_grid, m_direction, itemSpan.startLine(), itemSpan.integerSpan(), availableSpace());

        LayoutUnit extraSpace = itemSizeForTrackSizeComputationPhase(phase, gridItemWithSpan.gridItem()) - spanningTracksSize;
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
void GridTrackSizingAlgorithm::increaseSizesToAccommodateSpanningItems(const GridItemsSpanGroupRange& gridItemsWithSpan)
{
    increaseSizesToAccommodateSpanningItems<variant, ResolveIntrinsicMinimums>(gridItemsWithSpan);
    increaseSizesToAccommodateSpanningItems<variant, ResolveContentBasedMinimums>(gridItemsWithSpan);
    increaseSizesToAccommodateSpanningItems<variant, ResolveMaxContentMinimums>(gridItemsWithSpan);
    increaseSizesToAccommodateSpanningItems<variant, ResolveIntrinsicMaximums>(gridItemsWithSpan);
    increaseSizesToAccommodateSpanningItems<variant, ResolveMaxContentMaximums>(gridItemsWithSpan);
}

template <TrackSizeComputationVariant variant>
static double getSizeDistributionWeight(const GridTrack& track)
{
    if (variant != TrackSizeComputationVariant::CrossingFlexibleTracks)
        return 0;
    ASSERT(track.cachedTrackSize().maxTrackBreadth().isFlex());
    return track.cachedTrackSize().maxTrackBreadth().flex();
}

static bool sortByGridTrackGrowthPotential(const GridTrack* track1, const GridTrack* track2)
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
    if (phase != ResolveMaxContentMaximums || !track.growthLimitCap())
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
    LayoutUnit growthShare = limit == SpaceDistributionLimit::BeyondGrowthLimit || track.infiniteGrowthPotential() ? freeSpaceShare : std::min(freeSpaceShare, track.growthLimit() - trackSizeForTrackSizeComputationPhase(phase, track, ForbidInfinity));
    clampGrowthShareIfNeeded(phase, track, growthShare);
    ASSERT_WITH_MESSAGE(growthShare >= 0, "We must never shrink any grid track or else we can't guarantee we abide by our min-sizing function.");
    track.growTempSize(growthShare);
    freeSpace -= growthShare;
}

template <TrackSizeComputationVariant variant, TrackSizeComputationPhase phase, SpaceDistributionLimit limit>
static void distributeItemIncurredIncreases(Vector<GridTrack*>& tracks, LayoutUnit& freeSpace)
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
void GridTrackSizingAlgorithm::distributeSpaceToTracks(Vector<GridTrack*>& tracks, Vector<GridTrack*>* growBeyondGrowthLimitsTracks, LayoutUnit& freeSpace) const
{
    ASSERT(freeSpace >= 0);

    for (auto* track : tracks)
        track->setTempSize(trackSizeForTrackSizeComputationPhase(phase, *track, ForbidInfinity));

    if (freeSpace > 0)
        distributeItemIncurredIncreases<variant, phase, SpaceDistributionLimit::UpToGrowthLimit>(tracks, freeSpace);

    if (freeSpace > 0 && growBeyondGrowthLimitsTracks)
        distributeItemIncurredIncreases<variant, phase, SpaceDistributionLimit::BeyondGrowthLimit>(*growBeyondGrowthLimitsTracks, freeSpace);
    
    for (auto* track : tracks)
        track->setPlannedSize(track->plannedSize() == infinity ? track->tempSize() : std::max(track->plannedSize(), track->tempSize()));
}

std::optional<LayoutUnit> GridTrackSizingAlgorithm::estimatedGridAreaBreadthForChild(const RenderBox& child, GridTrackSizingDirection direction) const
{
    const GridSpan& span = m_renderGrid->gridSpanForChild(child, direction);
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

    gridAreaSize += m_renderGrid->guttersSize(m_grid, direction, span.startLine(), span.integerSpan(), availableSize);

    GridTrackSizingDirection childInlineDirection = GridLayoutFunctions::flowAwareDirectionForChild(*m_renderGrid, child, ForColumns);
    if (gridAreaIsIndefinite)
        return direction == childInlineDirection ? std::make_optional(std::max(child.maxPreferredLogicalWidth(), gridAreaSize)) : std::nullopt;
    return gridAreaSize;
}

std::optional<LayoutUnit> GridTrackSizingAlgorithm::gridAreaBreadthForChild(const RenderBox& child, GridTrackSizingDirection direction) const
{
    bool addContentAlignmentOffset =
        direction == ForColumns && (m_sizingState == RowSizingFirstIteration || m_sizingState == RowSizingExtraIterationForSizeContainment);
    // To determine the column track's size based on an orthogonal grid item we need it's logical
    // height, which may depend on the row track's size. It's possible that the row tracks sizing
    // logic has not been performed yet, so we will need to do an estimation.
    if (direction == ForRows && (m_sizingState == ColumnSizingFirstIteration || m_sizingState == ColumnSizingExtraIterationForSizeContainment || m_sizingState == ColumnSizingSecondIteration)) {
        ASSERT(GridLayoutFunctions::isOrthogonalChild(*m_renderGrid, child));
        // FIXME (jfernandez) Content Alignment should account for this heuristic.
        // https://github.com/w3c/csswg-drafts/issues/2697
        if (m_sizingState == ColumnSizingFirstIteration || m_sizingState == ColumnSizingExtraIterationForSizeContainment)
            return estimatedGridAreaBreadthForChild(child, ForRows);
        addContentAlignmentOffset = true;
    }

    const Vector<GridTrack>& allTracks = tracks(direction);
    const GridSpan& span = m_renderGrid->gridSpanForChild(child, direction);
    LayoutUnit gridAreaBreadth;
    for (auto trackPosition : span)
        gridAreaBreadth += allTracks[trackPosition].baseSize();

    if (addContentAlignmentOffset)
        gridAreaBreadth += (span.integerSpan() - 1) * m_renderGrid->gridItemOffset(direction);

    gridAreaBreadth += m_renderGrid->guttersSize(m_grid, direction, span.startLine(), span.integerSpan(), availableSpace(direction));

    return gridAreaBreadth;
}

bool GridTrackSizingAlgorithm::isRelativeGridLengthAsAuto(const GridLength& length, GridTrackSizingDirection direction) const
{
    return length.isPercentage() && !availableSpace(direction);
}

bool GridTrackSizingAlgorithm::isIntrinsicSizedGridArea(const RenderBox& child, GridAxis axis) const
{
    ASSERT(wasSetup());
    GridTrackSizingDirection direction = gridDirectionForAxis(axis);
    const GridSpan& span = m_renderGrid->gridSpanForChild(child, direction);
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
    if (m_direction == ForColumns && m_strategy->isComputingSizeContainment()) {
        if (auto size = m_renderGrid->explicitIntrinsicInnerLogicalSize(m_direction)) {
            m_minContentSize = size.value();
            m_maxContentSize = size.value();
            return;
        }
    }

    m_minContentSize = m_maxContentSize = 0_lu;

    Vector<GridTrack>& allTracks = tracks(m_direction);
    for (auto& track : allTracks) {
        ASSERT(m_strategy->isComputingSizeContainment() || m_strategy->isComputingInlineSizeContainment() || !track.infiniteGrowthPotential());
        m_minContentSize += track.baseSize();
        m_maxContentSize += track.growthLimitIsInfinite() ? track.baseSize() : track.growthLimit();
        // The growth limit caps must be cleared now in order to properly sort
        // tracks by growth potential on an eventual "Maximize Tracks".
        track.setGrowthLimitCap(std::nullopt);
    }
}

// GridTrackSizingAlgorithmStrategy.
LayoutUnit GridTrackSizingAlgorithmStrategy::logicalHeightForChild(RenderBox& child) const
{
    GridTrackSizingDirection childBlockDirection = GridLayoutFunctions::flowAwareDirectionForChild(*renderGrid(), child, ForRows);
    // If |child| has a relative logical height, we shouldn't let it override its intrinsic height, which is
    // what we are interested in here. Thus we need to set the block-axis override size to nullopt (no possible resolution).
    if (shouldClearOverridingContainingBlockContentSizeForChild(child, ForRows)) {
        setOverridingContainingBlockContentSizeForChild(*renderGrid(), child, childBlockDirection, std::nullopt);
        child.setNeedsLayout(MarkOnlyThis);
    }

    // We need to clear the stretched height to properly compute logical height during layout.
    if (child.needsLayout())
        child.clearOverridingLogicalHeight();

    child.layoutIfNeeded();
    return child.logicalHeight() + GridLayoutFunctions::marginLogicalSizeForChild(*renderGrid(), childBlockDirection, child) + m_algorithm.baselineOffsetForChild(child, gridAxisForDirection(direction()));
}

LayoutUnit GridTrackSizingAlgorithmStrategy::minContentForChild(RenderBox& child) const
{
    GridTrackSizingDirection childInlineDirection = GridLayoutFunctions::flowAwareDirectionForChild(*renderGrid(), child, ForColumns);
    if (direction() == childInlineDirection) {
        if (isComputingInlineSizeContainment())
            return { };
        // FIXME: It's unclear if we should return the intrinsic width or the preferred width.
        // See http://lists.w3.org/Archives/Public/www-style/2013Jan/0245.html
        if (child.needsPreferredWidthsRecalculation())
            child.setPreferredLogicalWidthsDirty(true);
        return child.minPreferredLogicalWidth() + GridLayoutFunctions::marginLogicalSizeForChild(*renderGrid(), childInlineDirection, child) + m_algorithm.baselineOffsetForChild(child, gridAxisForDirection(direction()));
    }

    if (updateOverridingContainingBlockContentSizeForChild(child, childInlineDirection)) {
        child.setNeedsLayout(MarkOnlyThis);
        // For a child with relative width constraints to the grid area, such as percentaged paddings, we reset the overridingContainingBlockContentSizeForChild value for columns when we are executing a definite strategy
        // for columns. Since we have updated the overridingContainingBlockContentSizeForChild inline-axis/width value here, we might need to recompute the child's relative width. For some cases, we probably will not
        // be able to do it during the RenderGrid::layoutGridItems() function as the grid area does't change there any more. Also, as we are doing a layout inside GridTrackSizingAlgorithmStrategy::logicalHeightForChild()
        // function, let's take the advantage and set it here. 
        if (shouldClearOverridingContainingBlockContentSizeForChild(child, childInlineDirection))
            child.setPreferredLogicalWidthsDirty(true);
    }
    return logicalHeightForChild(child);
}

LayoutUnit GridTrackSizingAlgorithmStrategy::maxContentForChild(RenderBox& child) const
{
    GridTrackSizingDirection childInlineDirection = GridLayoutFunctions::flowAwareDirectionForChild(*renderGrid(), child, ForColumns);
    if (direction() == childInlineDirection) {
        if (isComputingInlineSizeContainment())
            return { };
        // FIXME: It's unclear if we should return the intrinsic width or the preferred width.
        // See http://lists.w3.org/Archives/Public/www-style/2013Jan/0245.html
        if (child.needsPreferredWidthsRecalculation())
            child.setPreferredLogicalWidthsDirty(true);
        return child.maxPreferredLogicalWidth() + GridLayoutFunctions::marginLogicalSizeForChild(*renderGrid(), childInlineDirection, child) + m_algorithm.baselineOffsetForChild(child, gridAxisForDirection(direction()));
    }

    if (updateOverridingContainingBlockContentSizeForChild(child, childInlineDirection))
        child.setNeedsLayout(MarkOnlyThis);
    return logicalHeightForChild(child);
}

LayoutUnit GridTrackSizingAlgorithmStrategy::minSizeForChild(RenderBox& child) const
{
    GridTrackSizingDirection childInlineDirection = GridLayoutFunctions::flowAwareDirectionForChild(*renderGrid(), child, ForColumns);
    bool isRowAxis = direction() == childInlineDirection;
    if (isRowAxis && isComputingInlineSizeContainment())
        return { };
    const Length& childSize = isRowAxis ? child.style().logicalWidth() : child.style().logicalHeight();
    if (!childSize.isAuto() && !childSize.isPercentOrCalculated())
        return minContentForChild(child);

    const Length& childMinSize = isRowAxis ? child.style().logicalMinWidth() : child.style().logicalMinHeight();
    bool overflowIsVisible = isRowAxis ? child.effectiveOverflowInlineDirection() == Overflow::Visible : child.effectiveOverflowBlockDirection() == Overflow::Visible;
    LayoutUnit baselineShim = m_algorithm.baselineOffsetForChild(child, gridAxisForDirection(direction()));

    if (childMinSize.isAuto() && overflowIsVisible) {
        auto minSize = minContentForChild(child);
        const GridSpan& span = m_algorithm.m_renderGrid->gridSpanForChild(child, direction());

        LayoutUnit maxBreadth;
        auto allTracks = m_algorithm.tracks(direction());
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
            auto marginAndBorderAndPadding = GridLayoutFunctions::marginLogicalSizeForChild(*renderGrid(), direction(), child);
            marginAndBorderAndPadding += isRowAxis ? child.borderAndPaddingLogicalWidth() : child.borderAndPaddingLogicalHeight();
            minSize = std::max(maxBreadth, marginAndBorderAndPadding + baselineShim);
        }
        return minSize;
    }

    std::optional<LayoutUnit> gridAreaSize = m_algorithm.gridAreaBreadthForChild(child, childInlineDirection);
    return minLogicalSizeForChild(child, childMinSize, gridAreaSize) + baselineShim;
}

bool GridTrackSizingAlgorithm::canParticipateInBaselineAlignment(const RenderBox& child, GridAxis baselineAxis) const
{
    ASSERT(baselineAxis == GridColumnAxis ? m_columnBaselineItemsMap.contains(&child) : m_rowBaselineItemsMap.contains(&child));

    // Baseline cyclic dependencies only happen with synthesized
    // baselines. These cases include orthogonal or empty grid items
    // and replaced elements.
    bool isParallelToBaselineAxis = baselineAxis == GridColumnAxis ? !GridLayoutFunctions::isOrthogonalChild(*m_renderGrid, child) : GridLayoutFunctions::isOrthogonalChild(*m_renderGrid, child);
    if (isParallelToBaselineAxis && child.firstLineBaseline())
        return true;

    // FIXME: We don't currently allow items within subgrids that need to
    // synthesize a baseline, since we need a layout to have been completed
    // and performGridItemsPreLayout on the outer grid doesn't layout subgrid
    // items.
    if (child.parent() != renderGrid())
        return false;

    // Baseline cyclic dependencies only happen in grid areas with
    // intrinsically-sized tracks. 
    if (!isIntrinsicSizedGridArea(child, baselineAxis))
        return true;

    return isParallelToBaselineAxis ? !child.hasRelativeLogicalHeight() : !child.hasRelativeLogicalWidth() && !child.style().logicalWidth().isAuto();
}

bool GridTrackSizingAlgorithm::participateInBaselineAlignment(const RenderBox& child, GridAxis baselineAxis) const
{
    return baselineAxis == GridColumnAxis ? m_columnBaselineItemsMap.get(&child) : m_rowBaselineItemsMap.get(&child);
}

void GridTrackSizingAlgorithm::updateBaselineAlignmentContext(const RenderBox& child, GridAxis baselineAxis)
{
    ASSERT(wasSetup());
    ASSERT(canParticipateInBaselineAlignment(child, baselineAxis));

    ItemPosition align = m_renderGrid->selfAlignmentForChild(baselineAxis, child).position();
    const auto& span = m_renderGrid->gridSpanForChild(child, gridDirectionForAxis(baselineAxis));
    auto spanForBaselineAlignment = align == ItemPosition::Baseline ? span.startLine() : span.endLine();
    m_baselineAlignment.updateBaselineAlignmentContext(align, spanForBaselineAlignment, child, baselineAxis);
}

LayoutUnit GridTrackSizingAlgorithm::baselineOffsetForChild(const RenderBox& child, GridAxis baselineAxis) const
{
    // If we haven't yet initialized this axis (which can be the case if we're doing
    // prelayout of a subgrid), then we can't know the baseline offset.
    if (tracks(gridDirectionForAxis(baselineAxis)).isEmpty())
        return LayoutUnit();

    if (!participateInBaselineAlignment(child, baselineAxis))
        return LayoutUnit();

    ItemPosition align = m_renderGrid->selfAlignmentForChild(baselineAxis, child).position();
    const auto& span = m_renderGrid->gridSpanForChild(child, gridDirectionForAxis(baselineAxis));
    auto spanForBaselineAlignment = align == ItemPosition::Baseline ? span.startLine() : span.endLine();
    return m_baselineAlignment.baselineOffsetForChild(align, spanForBaselineAlignment, child, baselineAxis);
}

void GridTrackSizingAlgorithm::clearBaselineItemsCache()
{
    m_columnBaselineItemsMap.clear();
    m_rowBaselineItemsMap.clear();
}

void GridTrackSizingAlgorithm::cacheBaselineAlignedItem(const RenderBox& item, GridAxis axis)
{
    ASSERT(downcast<RenderGrid>(item.parent())->isBaselineAlignmentForChild(item, axis));

    if (GridLayoutFunctions::isOrthogonalParent(*m_renderGrid, *item.parent()))
        axis = axis == GridColumnAxis ? GridRowAxis : GridColumnAxis;

    if (axis == GridColumnAxis)
        m_columnBaselineItemsMap.add(&item, true);
    else
        m_rowBaselineItemsMap.add(&item, true);
}

void GridTrackSizingAlgorithm::copyBaselineItemsCache(const GridTrackSizingAlgorithm& source, GridAxis axis)
{
    if (axis == GridColumnAxis)
        m_columnBaselineItemsMap = source.m_columnBaselineItemsMap;
    else
        m_rowBaselineItemsMap = source.m_rowBaselineItemsMap;
}

bool GridTrackSizingAlgorithmStrategy::updateOverridingContainingBlockContentSizeForChild(RenderBox& child, GridTrackSizingDirection direction, std::optional<LayoutUnit> overrideSize) const
{
    if (!overrideSize)
        overrideSize = m_algorithm.gridAreaBreadthForChild(child, direction);

    if (renderGrid() != child.parent()) {
        // If child is part of a subgrid, find the nearest ancestor this is directly part of this grid
        // (either by being a child of the grid, or via being subgridded in this dimension.
        RenderGrid* grid = downcast<RenderGrid>(child.parent());
        GridTrackSizingDirection subgridDirection = GridLayoutFunctions::flowAwareDirectionForChild(*renderGrid(), *grid, direction);
        while (grid->parent() != renderGrid() && !grid->isSubgridOf(subgridDirection, *renderGrid())) {
            grid = downcast<RenderGrid>(grid->parent());
            subgridDirection = GridLayoutFunctions::flowAwareDirectionForChild(*renderGrid(), *grid, direction);
        }

        if (grid == child.parent() && grid->isSubgrid(subgridDirection)) {
            // If the item is subgridded in this direction (and thus the tracks it covers are tracks
            // owned by this sizing algorithm), then we want to take the breadth of the tracks we occupy,
            // and subtract any space occupied by the subgrid itself (and any ancestor subgrids).
            *overrideSize -= GridLayoutFunctions::extraMarginForSubgridAncestors(subgridDirection, child);
        } else {
            // Otherwise the tracks that this child covers (in this non-subgridded axis) are owned
            // by one of the intermediate RenderGrids (which are subgrids in the other axis), which may
            // be |grid| or a descendent.
            // Set the override size for |grid| (which is part of the outer grid), and force a layout
            // so that it computes the track sizes for the non-subgridded dimension and makes the size
            // of |child| available.
            bool overrideSizeHasChanged =
                updateOverridingContainingBlockContentSizeForChild(*grid, direction);
            layoutGridItemForMinSizeComputation(*grid, overrideSizeHasChanged);
            return overrideSizeHasChanged;
        }
    }

    if (GridLayoutFunctions::hasOverridingContainingBlockContentSizeForChild(child, direction) && GridLayoutFunctions::overridingContainingBlockContentSizeForChild(child, direction) == overrideSize)
        return false;

    setOverridingContainingBlockContentSizeForChild(*renderGrid(), child, direction, overrideSize);
    return true;
}

LayoutUnit GridTrackSizingAlgorithmStrategy::minLogicalSizeForChild(RenderBox& child, const Length& childMinSize, std::optional<LayoutUnit> availableSize) const
{
    GridTrackSizingDirection childInlineDirection = GridLayoutFunctions::flowAwareDirectionForChild(*renderGrid(), child, ForColumns);
    bool isRowAxis = direction() == childInlineDirection;
    if (isRowAxis)
        return isComputingInlineSizeContainment() ? 0_lu : child.computeLogicalWidthInFragmentUsing(MinSize, childMinSize, availableSize.value_or(0), *renderGrid(), nullptr) + GridLayoutFunctions::marginLogicalSizeForChild(*renderGrid(), childInlineDirection, child);
    bool overrideSizeHasChanged = updateOverridingContainingBlockContentSizeForChild(child, childInlineDirection, availableSize);
    layoutGridItemForMinSizeComputation(child, overrideSizeHasChanged);
    GridTrackSizingDirection childBlockDirection = GridLayoutFunctions::flowAwareDirectionForChild(*renderGrid(), child, ForRows);
    return child.computeLogicalHeightUsing(MinSize, childMinSize, std::nullopt).value_or(0) + GridLayoutFunctions::marginLogicalSizeForChild(*renderGrid(), childBlockDirection, child);
}

class IndefiniteSizeStrategy final : public GridTrackSizingAlgorithmStrategy {
public:
    IndefiniteSizeStrategy(GridTrackSizingAlgorithm& algorithm)
        : GridTrackSizingAlgorithmStrategy(algorithm) { }

private:
    void layoutGridItemForMinSizeComputation(RenderBox&, bool overrideSizeHasChanged) const override;
    void maximizeTracks(Vector<GridTrack>&, std::optional<LayoutUnit>& freeSpace) override;
    double findUsedFlexFraction(Vector<unsigned>& flexibleSizedTracksIndex, GridTrackSizingDirection, std::optional<LayoutUnit> freeSpace) const override;
    bool recomputeUsedFlexFractionIfNeeded(double& flexFraction, LayoutUnit& totalGrowth) const override;
    LayoutUnit freeSpaceForStretchAutoTracksStep() const override;
    bool isComputingSizeContainment() const override { return renderGrid()->shouldApplySizeContainment(); }
    bool isComputingInlineSizeContainment() const override { return renderGrid()->shouldApplyInlineSizeContainment(); }
    void accumulateFlexFraction(double& flexFraction, GridIterator&, GridTrackSizingDirection outermostDirection, HashSet<RenderBox*>& itemsSet) const;
};

void IndefiniteSizeStrategy::layoutGridItemForMinSizeComputation(RenderBox& child, bool overrideSizeHasChanged) const
{
    if (overrideSizeHasChanged && direction() != ForColumns)
        child.setNeedsLayout(MarkOnlyThis);
    child.layoutIfNeeded();
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

void IndefiniteSizeStrategy::accumulateFlexFraction(double& flexFraction, GridIterator& iterator, GridTrackSizingDirection outermostDirection, HashSet<RenderBox*>& itemsSet) const
{
    while (auto* gridItem = iterator.nextGridItem()) {
        if (is<RenderGrid>(gridItem) && downcast<RenderGrid>(gridItem)->isSubgridInParentDirection(iterator.direction())) {
            RenderGrid* inner = downcast<RenderGrid>(gridItem);

            GridIterator childIterator = GridIterator::createForSubgrid(*inner, iterator);
            accumulateFlexFraction(flexFraction, childIterator, outermostDirection, itemsSet);
            continue;
        }
        // Do not include already processed items.
        if (!itemsSet.add(gridItem).isNewEntry)
            continue;

        GridSpan span = m_algorithm.renderGrid()->gridSpanForChild(*gridItem, outermostDirection);

        // Removing gutters from the max-content contribution of the item, so they are not taken into account in FindFrUnitSize().
        LayoutUnit leftOverSpace = maxContentForChild(*gridItem) - renderGrid()->guttersSize(m_algorithm.grid(), outermostDirection, span.startLine(), span.integerSpan(), availableSpace());
        flexFraction = std::max(flexFraction, findFrUnitSize(span, leftOverSpace));
    }
}

double IndefiniteSizeStrategy::findUsedFlexFraction(Vector<unsigned>& flexibleSizedTracksIndex, GridTrackSizingDirection direction, std::optional<LayoutUnit> freeSpace) const
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
    if (!grid.hasGridItems())
        return flexFraction;

    HashSet<RenderBox*> itemsSet;
    for (const auto& trackIndex : flexibleSizedTracksIndex) {
        GridIterator iterator(grid, direction, trackIndex);
        accumulateFlexFraction(flexFraction, iterator, direction, itemsSet);
    }

    return flexFraction;
}

bool IndefiniteSizeStrategy::recomputeUsedFlexFractionIfNeeded(double& flexFraction, LayoutUnit& totalGrowth) const
{
    if (direction() == ForColumns)
        return false;

    const RenderGrid* renderGrid = this->renderGrid();

    auto minSize = renderGrid->computeContentLogicalHeight(MinSize, renderGrid->style().logicalMinHeight(), std::nullopt);
    auto maxSize = renderGrid->computeContentLogicalHeight(MaxSize, renderGrid->style().logicalMaxHeight(), std::nullopt);

    // Redo the flex fraction computation using min|max-height as definite available space in case
    // the total height is smaller than min-height or larger than max-height.
    LayoutUnit rowsSize = totalGrowth + computeTrackBasedSize();
    bool checkMinSize = minSize && rowsSize < minSize.value();
    bool checkMaxSize = maxSize && rowsSize > maxSize.value();
    if (!checkMinSize && !checkMaxSize)
        return false;

    LayoutUnit freeSpace = checkMaxSize ? maxSize.value() : -1_lu;
    const Grid& grid = m_algorithm.grid();
    freeSpace = std::max(freeSpace, minSize.value_or(0_lu)) - renderGrid->guttersSize(grid, ForRows, 0, grid.numTracks(ForRows), availableSpace());

    size_t numberOfTracks = m_algorithm.tracks(ForRows).size();
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
    double findUsedFlexFraction(Vector<unsigned>& flexibleSizedTracksIndex, GridTrackSizingDirection, std::optional<LayoutUnit> freeSpace) const override;
    bool recomputeUsedFlexFractionIfNeeded(double& flexFraction, LayoutUnit& totalGrowth) const override;
    LayoutUnit freeSpaceForStretchAutoTracksStep() const override;
    LayoutUnit minContentForChild(RenderBox&) const override;
    LayoutUnit minLogicalSizeForChild(RenderBox&, const Length& childMinSize, std::optional<LayoutUnit> availableSize) const override;
    bool isComputingSizeContainment() const override { return false; }
    bool isComputingInlineSizeContainment() const override { return false; }
};

LayoutUnit IndefiniteSizeStrategy::freeSpaceForStretchAutoTracksStep() const
{
    ASSERT(!m_algorithm.freeSpace(direction()));
    if (direction() == ForColumns)
        return 0_lu;

    auto minSize = renderGrid()->computeContentLogicalHeight(MinSize, renderGrid()->style().logicalMinHeight(), std::nullopt);
    if (!minSize)
        return 0_lu;
    return minSize.value() - computeTrackBasedSize();
}

LayoutUnit DefiniteSizeStrategy::minLogicalSizeForChild(RenderBox& child, const Length& childMinSize, std::optional<LayoutUnit> availableSize) const
{
    GridTrackSizingDirection childInlineDirection = GridLayoutFunctions::flowAwareDirectionForChild(*renderGrid(), child, ForColumns);
    GridTrackSizingDirection flowAwareDirection = GridLayoutFunctions::flowAwareDirectionForChild(*renderGrid(), child, direction());
    if (hasRelativeMarginOrPaddingForChild(child, flowAwareDirection) || (direction() != childInlineDirection && hasRelativeOrIntrinsicSizeForChild(child, flowAwareDirection))) {
        auto indefiniteSize = direction() == childInlineDirection ? std::make_optional(0_lu) : std::nullopt;
        setOverridingContainingBlockContentSizeForChild(*renderGrid(), child, direction(), indefiniteSize);
    }
    return GridTrackSizingAlgorithmStrategy::minLogicalSizeForChild(child, childMinSize, availableSize);
}

void DefiniteSizeStrategy::maximizeTracks(Vector<GridTrack>& tracks, std::optional<LayoutUnit>& freeSpace)
{
    size_t tracksSize = tracks.size();
    Vector<GridTrack*> tracksForDistribution(tracksSize);
    for (size_t i = 0; i < tracksSize; ++i) {
        tracksForDistribution[i] = tracks.data() + i;
        tracksForDistribution[i]->setPlannedSize(tracksForDistribution[i]->baseSize());
    }

    ASSERT(freeSpace);
    distributeSpaceToTracks(tracksForDistribution, freeSpace.value());

    for (auto* track : tracksForDistribution)
        track->setBaseSize(track->plannedSize());
}


void DefiniteSizeStrategy::layoutGridItemForMinSizeComputation(RenderBox& child, bool overrideSizeHasChanged) const
{
    if (overrideSizeHasChanged)
        child.setNeedsLayout(MarkOnlyThis);
    child.layoutIfNeeded();
}

double DefiniteSizeStrategy::findUsedFlexFraction(Vector<unsigned>&, GridTrackSizingDirection direction, std::optional<LayoutUnit> freeSpace) const
{
    GridSpan allTracksSpan = GridSpan::translatedDefiniteGridSpan(0, m_algorithm.tracks(direction).size());
    ASSERT(freeSpace);
    return findFrUnitSize(allTracksSpan, freeSpace.value());
}

LayoutUnit DefiniteSizeStrategy::freeSpaceForStretchAutoTracksStep() const
{
    return m_algorithm.freeSpace(direction()).value();
}

LayoutUnit DefiniteSizeStrategy::minContentForChild(RenderBox& child) const
{
    GridTrackSizingDirection childInlineDirection = GridLayoutFunctions::flowAwareDirectionForChild(*renderGrid(), child, ForColumns);
    if (direction() == childInlineDirection && child.needsLayout() && shouldClearOverridingContainingBlockContentSizeForChild(child, ForColumns))
        setOverridingContainingBlockContentSizeForChild(*renderGrid(), child, childInlineDirection, LayoutUnit());
    return GridTrackSizingAlgorithmStrategy::minContentForChild(child);
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
    const bool indefiniteHeight = m_direction == ForRows && !m_renderGrid->hasDefiniteLogicalHeight();
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
    if (direction == ForColumns)
        return startEdge ? grid.marginAndBorderAndPaddingStart() : grid.marginAndBorderAndPaddingEnd();
    return startEdge ? grid.marginAndBorderAndPaddingBefore() : grid.marginAndBorderAndPaddingAfter();
}

// https://drafts.csswg.org/css-grid-2/#subgrid-edge-placeholders
// FIXME: This is a simplification of the specified behaviour, where we add the hypothetical
// items directly to the edge tracks as if they had a span of 1. This matches the current Gecko
// behavior.
static void addSubgridMarginBorderPadding(const RenderGrid* outermost, GridTrackSizingDirection outermostDirection, Vector<GridTrack>& allTracks, GridSpan& span, RenderGrid* subgrid)
{
    // Convert the direction into the coordinate space of subgrid (which may not be a direct child
    // of the outermost grid for which we're running the track sizing algorithm).
    GridTrackSizingDirection direction = GridLayoutFunctions::flowAwareDirectionForChild(*outermost, *subgrid, outermostDirection);
    bool reversed = GridLayoutFunctions::isSubgridReversedDirection(*outermost, outermostDirection, *subgrid);

    if (allTracks[span.startLine()].cachedTrackSize().hasIntrinsicMinTrackBreadth()) {
        // If the subgrid has a reversed flow direction relative to the outermost grid, then
        // we want the MBP from the end edge in its local coordinate space.
        LayoutUnit mbpStart = marginAndBorderAndPaddingForEdge(*subgrid, direction, !reversed);
        allTracks[span.startLine()].setBaseSize(std::max(allTracks[span.startLine()].baseSize(), mbpStart));
    }
    if (allTracks[span.endLine() - 1].cachedTrackSize().hasIntrinsicMinTrackBreadth()) {
        LayoutUnit mbpEnd = marginAndBorderAndPaddingForEdge(*subgrid, direction, reversed);
        allTracks[span.endLine() - 1].setBaseSize(std::max(allTracks[span.endLine() - 1].baseSize(), mbpEnd));
    }
}

void GridTrackSizingAlgorithm::accumulateIntrinsicSizesForTrack(GridTrack& track, GridIterator& iterator, Vector<GridItemWithSpan>& itemsSortedByIncreasingSpan, Vector<GridItemWithSpan>& itemsCrossingFlexibleTracks, HashSet<RenderBox*>& itemsSet)
{
    Vector<GridTrack>& allTracks = tracks(m_direction);

    while (auto* gridItem = iterator.nextGridItem()) {
        bool isNewEntry = itemsSet.add(gridItem).isNewEntry;
        if (is<RenderGrid>(gridItem) && downcast<RenderGrid>(gridItem)->isSubgridInParentDirection(iterator.direction())) {
            // Contribute the mbp of wrapper to the first and last tracks that we span.
            RenderGrid* inner = downcast<RenderGrid>(gridItem);
            if (isNewEntry) {
                GridSpan span = m_renderGrid->gridSpanForChild(*gridItem, m_direction);
                addSubgridMarginBorderPadding(m_renderGrid, m_direction, allTracks, span, inner);
            }

            GridIterator childIterator = GridIterator::createForSubgrid(*inner, iterator);
            accumulateIntrinsicSizesForTrack(track, childIterator, itemsSortedByIncreasingSpan, itemsCrossingFlexibleTracks, itemsSet);
            continue;
        }
        if (!isNewEntry)
            continue;
        GridSpan span = m_renderGrid->gridSpanForChild(*gridItem, m_direction);

        if (spanningItemCrossesFlexibleSizedTracks(span))
            itemsCrossingFlexibleTracks.append(GridItemWithSpan(*gridItem, span));
        else if (span.integerSpan() == 1)
            sizeTrackToFitNonSpanningItem(span, *gridItem, track);
        else
            itemsSortedByIncreasingSpan.append(GridItemWithSpan(*gridItem, span));
    }
}

void GridTrackSizingAlgorithm::resolveIntrinsicTrackSizes()
{
    Vector<GridTrack>& allTracks = tracks(m_direction);
    auto handleInfinityGrowthLimit = [&]() {
        for (auto trackIndex : m_contentSizedTracksIndex) {
            GridTrack& track = allTracks[trackIndex];
            if (track.growthLimit() == infinity)
                track.setGrowthLimit(track.baseSize());
        }
    };

    if (m_strategy->isComputingSizeContainment()) {
        handleInfinityGrowthLimit();
        return;
    }

    Vector<GridItemWithSpan> itemsSortedByIncreasingSpan;
    Vector<GridItemWithSpan> itemsCrossingFlexibleTracks;
    HashSet<RenderBox*> itemsSet;

    if (m_grid.hasGridItems()) {
        for (auto trackIndex : m_contentSizedTracksIndex) {
            GridIterator iterator(m_grid, m_direction, trackIndex);
            GridTrack& track = allTracks[trackIndex];

            accumulateIntrinsicSizesForTrack(track, iterator, itemsSortedByIncreasingSpan, itemsCrossingFlexibleTracks, itemsSet);
        }
        std::sort(itemsSortedByIncreasingSpan.begin(), itemsSortedByIncreasingSpan.end());
    }

    auto it = itemsSortedByIncreasingSpan.begin();
    auto end = itemsSortedByIncreasingSpan.end();
    while (it != end) {
        GridItemsSpanGroupRange spanGroupRange = { it, std::upper_bound(it, end, *it) };
        increaseSizesToAccommodateSpanningItems<TrackSizeComputationVariant::NotCrossingFlexibleTracks>(spanGroupRange);
        it = spanGroupRange.rangeEnd;
    }
    GridItemsSpanGroupRange tracksGroupRange = { itemsCrossingFlexibleTracks.begin(), itemsCrossingFlexibleTracks.end() };
    increaseSizesToAccommodateSpanningItems<TrackSizeComputationVariant::CrossingFlexibleTracks>(tracksGroupRange);
    handleInfinityGrowthLimit();
}

void GridTrackSizingAlgorithm::stretchFlexibleTracks(std::optional<LayoutUnit> freeSpace)
{
    if (m_flexibleSizedTracksIndex.isEmpty())
        return;

    double flexFraction = m_strategy->findUsedFlexFraction(m_flexibleSizedTracksIndex, m_direction, freeSpace);

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
    case ColumnSizingFirstIteration:
        m_sizingState = m_strategy->isComputingSizeContainment() || m_strategy->isComputingInlineSizeContainment() ? ColumnSizingExtraIterationForSizeContainment : RowSizingFirstIteration;
        return;
    case ColumnSizingExtraIterationForSizeContainment:
        m_sizingState = RowSizingFirstIteration;
        return;
    case RowSizingFirstIteration:
        m_sizingState = m_strategy->isComputingSizeContainment() ? RowSizingExtraIterationForSizeContainment : ColumnSizingSecondIteration;
        return;
    case RowSizingExtraIterationForSizeContainment:
        m_sizingState = ColumnSizingSecondIteration;
        return;
    case ColumnSizingSecondIteration:
        m_sizingState = RowSizingSecondIteration;
        return;
    case RowSizingSecondIteration:
        m_sizingState = ColumnSizingFirstIteration;
        return;
    }
    ASSERT_NOT_REACHED();
    m_sizingState = ColumnSizingFirstIteration;
}

bool GridTrackSizingAlgorithm::isValidTransition() const
{
    switch (m_sizingState) {
    case ColumnSizingFirstIteration:
    case ColumnSizingExtraIterationForSizeContainment:
    case ColumnSizingSecondIteration:
        return m_direction == ForColumns;
    case RowSizingFirstIteration:
    case RowSizingExtraIterationForSizeContainment:
    case RowSizingSecondIteration:
        return m_direction == ForRows;
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
    case IntrinsicSizeComputation:
        m_strategy = makeUnique<IndefiniteSizeStrategy>(*this);
        break;
    case TrackSizing:
        m_strategy = makeUnique<DefiniteSizeStrategy>(*this);
        break;
    }

    m_contentSizedTracksIndex.shrink(0);
    m_flexibleSizedTracksIndex.shrink(0);
    m_autoSizedTracksForStretchIndex.shrink(0);

    if (availableSpace) {
        LayoutUnit guttersSize = m_renderGrid->guttersSize(m_grid, direction, 0, m_grid.numTracks(direction), this->availableSpace(direction));
        setFreeSpace(direction, *availableSpace - guttersSize);
    } else
        setFreeSpace(direction, std::nullopt);
    tracks(direction).resize(numTracks);

    m_needsSetup = false;
    m_hasPercentSizedRowsIndefiniteHeight = false;
    m_hasFlexibleMaxTrackBreadth = false;

    computeBaselineAlignmentContext();
}

void GridTrackSizingAlgorithm::computeBaselineAlignmentContext()
{
    GridAxis axis = gridAxisForDirection(m_direction);
    m_baselineAlignment.clear(axis);
    m_baselineAlignment.setBlockFlow(m_renderGrid->style().writingMode());
    BaselineItemsCache& baselineItemsCache = axis == GridColumnAxis ? m_columnBaselineItemsMap : m_rowBaselineItemsMap;
    BaselineItemsCache tmpBaselineItemsCache = baselineItemsCache;
    for (auto* child : tmpBaselineItemsCache.keys()) {
        // FIXME (jfernandez): We may have to get rid of the baseline participation
        // flag (hence just using a HashSet) depending on the CSS WG resolution on
        // https://github.com/w3c/csswg-drafts/issues/3046
        if (canParticipateInBaselineAlignment(*child, axis)) {
            updateBaselineAlignmentContext(*child, axis);
            baselineItemsCache.set(child, true);
        } else
            baselineItemsCache.set(child, false);
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
    ASSERT(is<RenderGrid>(m_renderGrid->parent()));
    RenderGrid* outer = downcast<RenderGrid>(m_renderGrid->parent());
    GridTrackSizingAlgorithm& parentAlgo = outer->m_trackSizingAlgorithm;
    GridTrackSizingDirection direction = GridLayoutFunctions::flowAwareDirectionForParent(*m_renderGrid, *outer, m_direction);
    Vector<GridTrack>& parentTracks = parentAlgo.tracks(direction);

    if (!parentTracks.size())
        return false;

    GridSpan span = outer->gridSpanForChild(*m_renderGrid, direction);
    Vector<GridTrack>& allTracks = tracks(m_direction);
    int numTracks = allTracks.size();
    for (int i = 0; i < numTracks; i++)
        allTracks[i] = parentTracks[i + span.startLine()];

    if (GridLayoutFunctions::isSubgridReversedDirection(*outer, direction, *m_renderGrid))
        allTracks.reverse();

    LayoutUnit startMBP = (m_direction == ForColumns) ? m_renderGrid->marginAndBorderAndPaddingStart() : m_renderGrid->marginAndBorderAndPaddingBefore();
    removeSubgridMarginBorderPaddingFromTracks(allTracks, startMBP, true);
    LayoutUnit endMBP = (m_direction == ForColumns) ? m_renderGrid->marginAndBorderAndPaddingEnd() : m_renderGrid->marginAndBorderAndPaddingAfter();
    removeSubgridMarginBorderPaddingFromTracks(allTracks, endMBP, false);

    LayoutUnit gapDifference = (m_renderGrid->gridGap(m_direction, availableSpace(m_direction)) - outer->gridGap(direction)) / 2;
    for (int i = 0; i < numTracks; i++) {
        LayoutUnit size = allTracks[i].baseSize();
        if (i)
            size -= gapDifference;
        if (i != numTracks - 1)
            size -= gapDifference;
        allTracks[i].setBaseSize(std::max(size, 0_lu));
    }
    return true;
}

void GridTrackSizingAlgorithm::run()
{
    ASSERT(wasSetup());
    StateMachine stateMachine(*this);

    if (m_renderGrid->isSubgrid(m_direction) && copyUsedTrackSizesForSubgrid())
        return;

    // Step 1.
    const std::optional<LayoutUnit> initialFreeSpace = freeSpace(m_direction);
    initializeTrackSizes();

    // Step 2.
    if (!m_contentSizedTracksIndex.isEmpty())
        resolveIntrinsicTrackSizes();

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
    m_strategy->maximizeTracks(tracks(m_direction), m_direction == ForColumns ? m_freeSpaceColumns : m_freeSpaceRows);
    if (m_strategy->isComputingSizeContainment() && !m_renderGrid->explicitIntrinsicInnerLogicalSize(m_direction))
        return;

    // Step 4.
    stretchFlexibleTracks(initialFreeSpace);

    // Step 5.
    stretchAutoTracks();
}

void GridTrackSizingAlgorithm::reset()
{
    ASSERT(wasSetup());
    m_sizingState = ColumnSizingFirstIteration;
    m_columns.shrink(0);
    m_rows.shrink(0);
    m_contentSizedTracksIndex.shrink(0);
    m_flexibleSizedTracksIndex.shrink(0);
    m_autoSizedTracksForStretchIndex.shrink(0);
    setAvailableSpace(ForRows, std::nullopt);
    setAvailableSpace(ForColumns, std::nullopt);
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

} // namespace WebCore
