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
    m_growthLimit = growthLimit == infinity ? growthLimit : std::min(growthLimit, m_growthLimitCap.valueOr(growthLimit));
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

void GridTrack::setGrowthLimitCap(Optional<LayoutUnit> growthLimitCap)
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

static bool shouldClearOverrideContainingBlockContentSizeForChild(const RenderBox& child, GridTrackSizingDirection direction)
{
    if (direction == ForColumns)
        return child.hasRelativeLogicalWidth() || child.style().logicalWidth().isIntrinsicOrAuto();
    return child.hasRelativeLogicalHeight() || child.style().logicalHeight().isIntrinsicOrAuto();
}

static void setOverrideContainingBlockContentSizeForChild(RenderBox& child, GridTrackSizingDirection direction, Optional<LayoutUnit> size)
{
    if (direction == ForColumns)
        child.setOverrideContainingBlockContentLogicalWidth(size);
    else
        child.setOverrideContainingBlockContentLogicalHeight(size);
}

// FIXME: we borrowed this from RenderBlock. We cannot call it from here because it's protected for RenderObjects.
static LayoutUnit marginIntrinsicLogicalWidthForChild(const RenderGrid* renderGrid, RenderBox& child)
{
    // A margin has three types: fixed, percentage, and auto (variable).
    // Auto and percentage margins become 0 when computing min/max width.
    // Fixed margins can be added in as is.
    Length marginLeft = child.style().marginStartUsing(&renderGrid->style());
    Length marginRight = child.style().marginEndUsing(&renderGrid->style());
    LayoutUnit margin;
    if (marginLeft.isFixed())
        margin += marginLeft.value();
    if (marginRight.isFixed())
        margin += marginRight.value();
    return margin;
}

// GridTrackSizingAlgorithm private.

void GridTrackSizingAlgorithm::setFreeSpace(GridTrackSizingDirection direction, Optional<LayoutUnit> freeSpace)
{
    if (direction == ForColumns)
        m_freeSpaceColumns = freeSpace;
    else
        m_freeSpaceRows = freeSpace;
}

Optional<LayoutUnit> GridTrackSizingAlgorithm::availableSpace() const
{
    ASSERT(wasSetup());
    return availableSpace(m_direction);
}

void GridTrackSizingAlgorithm::setAvailableSpace(GridTrackSizingDirection direction, Optional<LayoutUnit> availableSpace)
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
        return valueForLength(trackLength, std::max<LayoutUnit>(availableSpace().valueOr(0), 0));

    ASSERT(trackLength.isMinContent() || trackLength.isAuto() || trackLength.isMaxContent());
    return 0;
}

LayoutUnit GridTrackSizingAlgorithm::initialGrowthLimit(const GridTrackSize& trackSize, LayoutUnit baseSize) const
{
    const GridLength& gridLength = trackSize.maxTrackBreadth();
    if (gridLength.isFlex())
        return baseSize;

    const Length& trackLength = gridLength.length();
    if (trackLength.isSpecified())
        return valueForLength(trackLength, std::max<LayoutUnit>(availableSpace().valueOr(0), 0));

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
            growthLimit = std::min(growthLimit, valueForLength(trackSize.fitContentTrackBreadth().length(), availableSpace().valueOr(0)));
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

template <TrackSizeComputationPhase phase>
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
        ASSERT(gridItemWithSpan.span().integerSpan() > 1);
        const GridSpan& itemSpan = gridItemWithSpan.span();

        filteredTracks.shrink(0);
        growBeyondGrowthLimitsTracks.shrink(0);
        LayoutUnit spanningTracksSize;
        for (auto trackPosition : itemSpan) {
            GridTrack& track = allTracks[trackPosition];
            const auto& trackSize = track.cachedTrackSize();
            spanningTracksSize += trackSizeForTrackSizeComputationPhase(phase, track, ForbidInfinity);
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
        distributeSpaceToTracks<phase>(filteredTracks, &tracksToGrowBeyondGrowthLimits, extraSpace);
    }

    for (const auto& trackIndex : m_contentSizedTracksIndex) {
        GridTrack& track = allTracks[trackIndex];
        markAsInfinitelyGrowableForTrackSizeComputationPhase(phase, track);
        updateTrackSizeForTrackSizeComputationPhase(phase, track);
    }
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

    LayoutUnit track1Limit = track1->growthLimitCap().valueOr(track1->growthLimit());
    LayoutUnit track2Limit = track2->growthLimitCap().valueOr(track2->growthLimit());
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

template <TrackSizeComputationPhase phase>
void GridTrackSizingAlgorithm::distributeSpaceToTracks(Vector<GridTrack*>& tracks, Vector<GridTrack*>* growBeyondGrowthLimitsTracks, LayoutUnit& freeSpace) const
{
    ASSERT(freeSpace >= 0);

    for (auto* track : tracks)
        track->setTempSize(trackSizeForTrackSizeComputationPhase(phase, *track, ForbidInfinity));

    if (freeSpace > 0) {
        std::sort(tracks.begin(), tracks.end(), sortByGridTrackGrowthPotential);

        unsigned tracksSize = tracks.size();
        for (unsigned i = 0; i < tracksSize; ++i) {
            GridTrack& track = *tracks[i];
            const LayoutUnit& trackBreadth = trackSizeForTrackSizeComputationPhase(phase, track, ForbidInfinity);
            bool infiniteGrowthPotential = track.infiniteGrowthPotential();
            LayoutUnit trackGrowthPotential = infiniteGrowthPotential ? track.growthLimit() : track.growthLimit() - trackBreadth;
            // Let's avoid computing availableLogicalSpaceShare as much as possible as it's a hot spot in performance tests.
            if (trackGrowthPotential > 0 || infiniteGrowthPotential) {
                LayoutUnit availableLogicalSpaceShare = freeSpace / (tracksSize - i);
                LayoutUnit growthShare = infiniteGrowthPotential ? availableLogicalSpaceShare : std::min(availableLogicalSpaceShare, trackGrowthPotential);
                clampGrowthShareIfNeeded(phase, track, growthShare);
                ASSERT_WITH_MESSAGE(growthShare >= 0, "We should never shrink any grid track or else we can't guarantee we abide by our min-sizing function. We can still have 0 as growthShare if the amount of tracks greatly exceeds the freeSpace.");
                track.growTempSize(growthShare);
                freeSpace -= growthShare;
            }
        }
    }

    if (freeSpace > 0 && growBeyondGrowthLimitsTracks) {
        // We need to sort them because there might be tracks with growth limit caps (like the ones
        // with fit-content()) which cannot indefinitely grow over the limits.
        if (phase == ResolveMaxContentMaximums)
            std::sort(growBeyondGrowthLimitsTracks->begin(), growBeyondGrowthLimitsTracks->end(), sortByGridTrackGrowthPotential);

        unsigned tracksGrowingBeyondGrowthLimitsSize = growBeyondGrowthLimitsTracks->size();
        for (unsigned i = 0; i < tracksGrowingBeyondGrowthLimitsSize; ++i) {
            GridTrack* track = growBeyondGrowthLimitsTracks->at(i);
            LayoutUnit growthShare = freeSpace / (tracksGrowingBeyondGrowthLimitsSize - i);
            clampGrowthShareIfNeeded(phase, *track, growthShare);
            track->growTempSize(growthShare);
            freeSpace -= growthShare;
        }
    }

    for (auto* track : tracks)
        track->setPlannedSize(track->plannedSize() == infinity ? track->tempSize() : std::max(track->plannedSize(), track->tempSize()));
}

LayoutSize GridTrackSizingAlgorithm::estimatedGridAreaBreadthForChild(const RenderBox& child) const
{
    return {estimatedGridAreaBreadthForChild(child, ForColumns), estimatedGridAreaBreadthForChild(child, ForRows)};
}

LayoutUnit GridTrackSizingAlgorithm::estimatedGridAreaBreadthForChild(const RenderBox& child, GridTrackSizingDirection direction) const
{
    const GridSpan& span = m_grid.gridItemSpan(child, direction);
    LayoutUnit gridAreaSize;
    bool gridAreaIsIndefinite = false;
    Optional<LayoutUnit> availableSize = availableSpace(direction);
    for (auto trackPosition : span) {
        // We may need to estimate the grid area size before running the track sizing algorithm in order to perform the pre-layout of orthogonal items.
        // We cannot use tracks(direction)[trackPosition].cachedTrackSize() because tracks(direction) is empty, since we are either performing pre-layout
        // or are running the track sizing algorithm in the opposite direction and haven't run it in the desired direction yet.
        const auto& trackSize = wasSetup() ? calculateGridTrackSize(direction, trackPosition) : rawGridTrackSize(direction, trackPosition);
        GridLength maxTrackSize = trackSize.maxTrackBreadth();
        if (maxTrackSize.isContentSized() || maxTrackSize.isFlex() || isRelativeGridLengthAsAuto(maxTrackSize, direction))
            gridAreaIsIndefinite = true;
        else
            gridAreaSize += valueForLength(maxTrackSize.length(), availableSize.valueOr(0_lu));
    }

    gridAreaSize += m_renderGrid->guttersSize(m_grid, direction, span.startLine(), span.integerSpan(), availableSize);

    GridTrackSizingDirection childInlineDirection = GridLayoutFunctions::flowAwareDirectionForChild(*m_renderGrid, child, ForColumns);
    if (gridAreaIsIndefinite)
        return direction == childInlineDirection ? std::max(child.maxPreferredLogicalWidth(), gridAreaSize) : -1_lu;
    return gridAreaSize;
}

LayoutUnit GridTrackSizingAlgorithm::gridAreaBreadthForChild(const RenderBox& child, GridTrackSizingDirection direction) const
{
    bool addContentAlignmentOffset =
        direction == ForColumns && m_sizingState == RowSizingFirstIteration;
    // To determine the column track's size based on an orthogonal grid item we need it's logical
    // height, which may depend on the row track's size. It's possible that the row tracks sizing
    // logic has not been performed yet, so we will need to do an estimation.
    if (direction == ForRows && (m_sizingState == ColumnSizingFirstIteration || m_sizingState == ColumnSizingSecondIteration)) {
        ASSERT(GridLayoutFunctions::isOrthogonalChild(*m_renderGrid, child));
        // FIXME (jfernandez) Content Alignment should account for this heuristic.
        // https://github.com/w3c/csswg-drafts/issues/2697
        if (m_sizingState == ColumnSizingFirstIteration)
            return estimatedGridAreaBreadthForChild(child, ForRows);
        addContentAlignmentOffset = true;
    }

    const Vector<GridTrack>& allTracks = tracks(direction);
    const GridSpan& span = m_grid.gridItemSpan(child, direction);
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
    const GridSpan& span = m_grid.gridItemSpan(child, direction);
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
        return { Length(Fixed), LengthTrackSizing };

    auto& trackSize = rawGridTrackSize(direction, translatedIndex);
    if (trackSize.isFitContent())
        return isRelativeGridLengthAsAuto(trackSize.fitContentTrackBreadth(), direction) ? GridTrackSize(Length(Auto), Length(MaxContent)) : trackSize;

    GridLength minTrackBreadth = trackSize.minTrackBreadth();
    GridLength maxTrackBreadth = trackSize.maxTrackBreadth();
    // If the logical width/height of the grid container is indefinite, percentage
    // values are treated as <auto>.
    if (isRelativeGridLengthAsAuto(trackSize.minTrackBreadth(), direction))
        minTrackBreadth = Length(Auto);
    if (isRelativeGridLengthAsAuto(trackSize.maxTrackBreadth(), direction))
        maxTrackBreadth = Length(Auto);

    // Flex sizes are invalid as a min sizing function. However we still can have a flexible |minTrackBreadth|
    // if the track size is just a flex size (e.g. "1fr"), the spec says that in this case it implies an automatic minimum.
    if (minTrackBreadth.isFlex())
        minTrackBreadth = Length(Auto);

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
    for (size_t i = 0; i < numFlexTracks; ++i) {
        unsigned trackIndex = m_flexibleSizedTracksIndex[i];
        const auto& trackSize = allTracks[trackIndex].cachedTrackSize();
        ASSERT(trackSize.maxTrackBreadth().isFlex());
        LayoutUnit oldBaseSize = allTracks[trackIndex].baseSize();
        LayoutUnit newBaseSize = std::max(oldBaseSize, LayoutUnit(flexFraction * trackSize.maxTrackBreadth().flex()));
        increments[i] = newBaseSize - oldBaseSize;
        totalGrowth += increments[i];
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
    m_minContentSize = m_maxContentSize = 0_lu;

    Vector<GridTrack>& allTracks = tracks(m_direction);
    for (auto& track : allTracks) {
        ASSERT(!track.infiniteGrowthPotential());
        m_minContentSize += track.baseSize();
        m_maxContentSize += track.growthLimit();
        // The growth limit caps must be cleared now in order to properly sort
        // tracks by growth potential on an eventual "Maximize Tracks".
        track.setGrowthLimitCap(WTF::nullopt);
    }
}

// GridTrackSizingAlgorithmStrategy.
LayoutUnit GridTrackSizingAlgorithmStrategy::logicalHeightForChild(RenderBox& child) const
{
    GridTrackSizingDirection childBlockDirection = GridLayoutFunctions::flowAwareDirectionForChild(*renderGrid(), child, ForRows);
    // If |child| has a relative logical height, we shouldn't let it override its intrinsic height, which is
    // what we are interested in here. Thus we need to set the block-axis override size to -1 (no possible resolution).
    if (shouldClearOverrideContainingBlockContentSizeForChild(child, ForRows)) {
        setOverrideContainingBlockContentSizeForChild(child, childBlockDirection, WTF::nullopt);
        child.setNeedsLayout(MarkOnlyThis);
    }

    // We need to clear the stretched height to properly compute logical height during layout.
    if (child.needsLayout())
        child.clearOverrideContentLogicalHeight();

    child.layoutIfNeeded();
    return child.logicalHeight() + GridLayoutFunctions::marginLogicalSizeForChild(*renderGrid(), childBlockDirection, child) + m_algorithm.baselineOffsetForChild(child, gridAxisForDirection(direction()));
}

LayoutUnit GridTrackSizingAlgorithmStrategy::minContentForChild(RenderBox& child) const
{
    GridTrackSizingDirection childInlineDirection = GridLayoutFunctions::flowAwareDirectionForChild(*renderGrid(), child, ForColumns);
    if (direction() == childInlineDirection) {
        // FIXME: It's unclear if we should return the intrinsic width or the preferred width.
        // See http://lists.w3.org/Archives/Public/www-style/2013Jan/0245.html
        return child.minPreferredLogicalWidth() + GridLayoutFunctions::marginLogicalSizeForChild(*renderGrid(), childInlineDirection, child) + m_algorithm.baselineOffsetForChild(child, gridAxisForDirection(direction()));
    }

    if (updateOverrideContainingBlockContentSizeForChild(child, childInlineDirection))
        child.setNeedsLayout(MarkOnlyThis);
    return logicalHeightForChild(child);
}

LayoutUnit GridTrackSizingAlgorithmStrategy::maxContentForChild(RenderBox& child) const
{
    GridTrackSizingDirection childInlineDirection = GridLayoutFunctions::flowAwareDirectionForChild(*renderGrid(), child, ForColumns);
    if (direction() == childInlineDirection) {
        // FIXME: It's unclear if we should return the intrinsic width or the preferred width.
        // See http://lists.w3.org/Archives/Public/www-style/2013Jan/0245.html
        return child.maxPreferredLogicalWidth() + GridLayoutFunctions::marginLogicalSizeForChild(*renderGrid(), childInlineDirection, child) + m_algorithm.baselineOffsetForChild(child, gridAxisForDirection(direction()));
    }

    if (updateOverrideContainingBlockContentSizeForChild(child, childInlineDirection))
        child.setNeedsLayout(MarkOnlyThis);
    return logicalHeightForChild(child);
}

LayoutUnit GridTrackSizingAlgorithmStrategy::minSizeForChild(RenderBox& child) const
{
    GridTrackSizingDirection childInlineDirection = GridLayoutFunctions::flowAwareDirectionForChild(*renderGrid(), child, ForColumns);
    bool isRowAxis = direction() == childInlineDirection;
    const Length& childSize = isRowAxis ? child.style().logicalWidth() : child.style().logicalHeight();
    if (!childSize.isAuto() && !childSize.isPercentOrCalculated())
        return minContentForChild(child);

    const Length& childMinSize = isRowAxis ? child.style().logicalMinWidth() : child.style().logicalMinHeight();
    bool overflowIsVisible = isRowAxis ? child.style().overflowInlineDirection() == Overflow::Visible : child.style().overflowBlockDirection() == Overflow::Visible;
    LayoutUnit baselineShim = m_algorithm.baselineOffsetForChild(child, gridAxisForDirection(direction()));

    if (childMinSize.isAuto() && overflowIsVisible) {
        auto minSize = minContentForChild(child);
        LayoutUnit maxBreadth;
        auto allTracks = m_algorithm.tracks(direction());
        for (auto trackPosition : m_algorithm.grid().gridItemSpan(child, direction())) {
            const auto& trackSize = allTracks[trackPosition].cachedTrackSize();
            if (!trackSize.hasFixedMaxTrackBreadth())
                return minSize;
            maxBreadth += valueForLength(trackSize.maxTrackBreadth().length(), availableSpace().valueOr(0_lu));
        }
        if (minSize > maxBreadth) {
            auto marginAndBorderAndPadding = GridLayoutFunctions::marginLogicalSizeForChild(*renderGrid(), direction(), child);
            marginAndBorderAndPadding += isRowAxis ? child.borderAndPaddingLogicalWidth() : child.borderAndPaddingLogicalHeight();
            minSize = std::max(maxBreadth, marginAndBorderAndPadding + baselineShim);
        }
        return minSize;
    }

    LayoutUnit gridAreaSize = m_algorithm.gridAreaBreadthForChild(child, childInlineDirection);
    if (isRowAxis)
        return minLogicalWidthForChild(child, childMinSize, gridAreaSize) + baselineShim;

    bool overrideSizeHasChanged = updateOverrideContainingBlockContentSizeForChild(child, childInlineDirection, gridAreaSize);
    layoutGridItemForMinSizeComputation(child, overrideSizeHasChanged);

    return child.computeLogicalHeightUsing(MinSize, childMinSize, WTF::nullopt).valueOr(0) + child.marginLogicalHeight() + child.scrollbarLogicalHeight() + baselineShim;
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
    ASSERT(!child.needsLayout());

    ItemPosition align = m_renderGrid->selfAlignmentForChild(baselineAxis, child).position();
    const auto& span = m_grid.gridItemSpan(child, gridDirectionForAxis(baselineAxis));
    m_baselineAlignment.updateBaselineAlignmentContext(align, span.startLine(), child, baselineAxis);
}

LayoutUnit GridTrackSizingAlgorithm::baselineOffsetForChild(const RenderBox& child, GridAxis baselineAxis) const
{
    if (!participateInBaselineAlignment(child, baselineAxis))
        return LayoutUnit();

    ItemPosition align = m_renderGrid->selfAlignmentForChild(baselineAxis, child).position();
    const auto& span = m_grid.gridItemSpan(child, gridDirectionForAxis(baselineAxis));
    return m_baselineAlignment.baselineOffsetForChild(align, span.startLine(), child, baselineAxis);
}

void GridTrackSizingAlgorithm::clearBaselineItemsCache()
{
    m_columnBaselineItemsMap.clear();
    m_rowBaselineItemsMap.clear();
}

void GridTrackSizingAlgorithm::cacheBaselineAlignedItem(const RenderBox& item, GridAxis axis)
{
    ASSERT(m_renderGrid->isBaselineAlignmentForChild(item, axis));
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

bool GridTrackSizingAlgorithmStrategy::updateOverrideContainingBlockContentSizeForChild(RenderBox& child, GridTrackSizingDirection direction, Optional<LayoutUnit> overrideSize) const
{
    if (!overrideSize)
        overrideSize = m_algorithm.gridAreaBreadthForChild(child, direction);
    if (GridLayoutFunctions::hasOverrideContainingBlockContentSizeForChild(child, direction) && GridLayoutFunctions::overrideContainingBlockContentSizeForChild(child, direction) == overrideSize)
        return false;

    setOverrideContainingBlockContentSizeForChild(child, direction, overrideSize);
    return true;
}

class IndefiniteSizeStrategy final : public GridTrackSizingAlgorithmStrategy {
public:
    IndefiniteSizeStrategy(GridTrackSizingAlgorithm& algorithm)
        : GridTrackSizingAlgorithmStrategy(algorithm) { }

private:
    LayoutUnit minLogicalWidthForChild(RenderBox&, Length childMinSize, LayoutUnit availableSize) const override;
    void layoutGridItemForMinSizeComputation(RenderBox&, bool overrideSizeHasChanged) const override;
    void maximizeTracks(Vector<GridTrack>&, Optional<LayoutUnit>& freeSpace) override;
    double findUsedFlexFraction(Vector<unsigned>& flexibleSizedTracksIndex, GridTrackSizingDirection, Optional<LayoutUnit> freeSpace) const override;
    bool recomputeUsedFlexFractionIfNeeded(double& flexFraction, LayoutUnit& totalGrowth) const override;
    LayoutUnit freeSpaceForStretchAutoTracksStep() const override;
};

LayoutUnit IndefiniteSizeStrategy::minLogicalWidthForChild(RenderBox& child, Length childMinSize, LayoutUnit availableSize) const
{
    return child.computeLogicalWidthInFragmentUsing(MinSize, childMinSize, availableSize, *renderGrid(), nullptr) + marginIntrinsicLogicalWidthForChild(renderGrid(), child);
}

void IndefiniteSizeStrategy::layoutGridItemForMinSizeComputation(RenderBox& child, bool overrideSizeHasChanged) const
{
    if (overrideSizeHasChanged && direction() != ForColumns)
        child.setNeedsLayout(MarkOnlyThis);
    child.layoutIfNeeded();
}

void IndefiniteSizeStrategy::maximizeTracks(Vector<GridTrack>& tracks, Optional<LayoutUnit>& freeSpace)
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

double IndefiniteSizeStrategy::findUsedFlexFraction(Vector<unsigned>& flexibleSizedTracksIndex, GridTrackSizingDirection direction, Optional<LayoutUnit> freeSpace) const
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

    for (unsigned i = 0; i < flexibleSizedTracksIndex.size(); ++i) {
        GridIterator iterator(grid, direction, flexibleSizedTracksIndex[i]);
        while (auto* gridItem = iterator.nextGridItem()) {
            const GridSpan& span = grid.gridItemSpan(*gridItem, direction);

            // Do not include already processed items.
            if (i > 0 && span.startLine() <= flexibleSizedTracksIndex[i - 1])
                continue;

            // Removing gutters from the max-content contribution of the item, so they are not taken into account in FindFrUnitSize().
            LayoutUnit leftOverSpace = maxContentForChild(*gridItem) - renderGrid()->guttersSize(m_algorithm.grid(), direction, span.startLine(), span.integerSpan(), availableSpace());
            flexFraction = std::max(flexFraction, findFrUnitSize(span, leftOverSpace));
        }
    }

    return flexFraction;
}

bool IndefiniteSizeStrategy::recomputeUsedFlexFractionIfNeeded(double& flexFraction, LayoutUnit& totalGrowth) const
{
    if (direction() == ForColumns)
        return false;

    const RenderGrid* renderGrid = this->renderGrid();

    auto minSize = renderGrid->computeContentLogicalHeight(MinSize, renderGrid->style().logicalMinHeight(), WTF::nullopt);
    auto maxSize = renderGrid->computeContentLogicalHeight(MaxSize, renderGrid->style().logicalMaxHeight(), WTF::nullopt);

    // Redo the flex fraction computation using min|max-height as definite available space in case
    // the total height is smaller than min-height or larger than max-height.
    LayoutUnit rowsSize = totalGrowth + computeTrackBasedSize();
    bool checkMinSize = minSize && rowsSize < minSize.value();
    bool checkMaxSize = maxSize && rowsSize > maxSize.value();
    if (!checkMinSize && !checkMaxSize)
        return false;

    LayoutUnit freeSpace = checkMaxSize ? maxSize.value() : -1_lu;
    const Grid& grid = m_algorithm.grid();
    freeSpace = std::max(freeSpace, minSize.valueOr(0_lu)) - renderGrid->guttersSize(grid, ForRows, 0, grid.numTracks(ForRows), availableSpace());

    size_t numberOfTracks = m_algorithm.tracks(ForRows).size();
    flexFraction = findFrUnitSize(GridSpan::translatedDefiniteGridSpan(0, numberOfTracks), freeSpace);
    return true;
}

class DefiniteSizeStrategy final : public GridTrackSizingAlgorithmStrategy {
public:
    DefiniteSizeStrategy(GridTrackSizingAlgorithm& algorithm)
        : GridTrackSizingAlgorithmStrategy(algorithm) { }

private:
    LayoutUnit minLogicalWidthForChild(RenderBox&, Length childMinSize, LayoutUnit availableSize) const override;
    void layoutGridItemForMinSizeComputation(RenderBox&, bool overrideSizeHasChanged) const override;
    void maximizeTracks(Vector<GridTrack>&, Optional<LayoutUnit>& freeSpace) override;
    double findUsedFlexFraction(Vector<unsigned>& flexibleSizedTracksIndex, GridTrackSizingDirection, Optional<LayoutUnit> freeSpace) const override;
    bool recomputeUsedFlexFractionIfNeeded(double& flexFraction, LayoutUnit& totalGrowth) const override;
    LayoutUnit freeSpaceForStretchAutoTracksStep() const override;
};

LayoutUnit IndefiniteSizeStrategy::freeSpaceForStretchAutoTracksStep() const
{
    ASSERT(!m_algorithm.freeSpace(direction()));
    if (direction() == ForColumns)
        return 0_lu;

    auto minSize = renderGrid()->computeContentLogicalHeight(MinSize, renderGrid()->style().logicalMinHeight(), WTF::nullopt);
    if (!minSize)
        return 0_lu;
    return minSize.value() - computeTrackBasedSize();
}

LayoutUnit DefiniteSizeStrategy::minLogicalWidthForChild(RenderBox& child, Length childMinSize, LayoutUnit availableSize) const
{
    LayoutUnit marginLogicalWidth =
        GridLayoutFunctions::computeMarginLogicalSizeForChild(*renderGrid(), ForColumns, child);
    return child.computeLogicalWidthInFragmentUsing(MinSize, childMinSize, availableSize, *renderGrid(), nullptr) + marginLogicalWidth;
}

void DefiniteSizeStrategy::maximizeTracks(Vector<GridTrack>& tracks, Optional<LayoutUnit>& freeSpace)
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

double DefiniteSizeStrategy::findUsedFlexFraction(Vector<unsigned>&, GridTrackSizingDirection direction, Optional<LayoutUnit> freeSpace) const
{
    GridSpan allTracksSpan = GridSpan::translatedDefiniteGridSpan(0, m_algorithm.tracks(direction).size());
    ASSERT(freeSpace);
    return findFrUnitSize(allTracksSpan, freeSpace.value());
}

LayoutUnit DefiniteSizeStrategy::freeSpaceForStretchAutoTracksStep() const
{
    return m_algorithm.freeSpace(direction()).value();
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

    Vector<GridTrack>& allTracks = tracks(m_direction);
    const bool indefiniteHeight = m_direction == ForRows && !m_renderGrid->hasDefiniteLogicalHeight();
    LayoutUnit maxSize = std::max(0_lu, availableSpace().valueOr(0_lu));
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

        if (!m_hasPercentSizedRowsIndefiniteHeight && indefiniteHeight) {
            auto& rawTrackSize = rawGridTrackSize(m_direction, i);
            if (rawTrackSize.minTrackBreadth().isPercentage() || rawTrackSize.maxTrackBreadth().isPercentage())
                m_hasPercentSizedRowsIndefiniteHeight = true;
        }
    }
}

void GridTrackSizingAlgorithm::resolveIntrinsicTrackSizes()
{
    Vector<GridItemWithSpan> itemsSortedByIncreasingSpan;
    HashSet<RenderBox*> itemsSet;
    Vector<GridTrack>& allTracks = tracks(m_direction);
    if (m_grid.hasGridItems()) {
        for (auto trackIndex : m_contentSizedTracksIndex) {
            GridIterator iterator(m_grid, m_direction, trackIndex);
            GridTrack& track = allTracks[trackIndex];

            while (auto* gridItem = iterator.nextGridItem()) {
                if (itemsSet.add(gridItem).isNewEntry) {
                    const GridSpan& span = m_grid.gridItemSpan(*gridItem, m_direction);
                    if (span.integerSpan() == 1)
                        sizeTrackToFitNonSpanningItem(span, *gridItem, track);
                    else if (!spanningItemCrossesFlexibleSizedTracks(span))
                        itemsSortedByIncreasingSpan.append(GridItemWithSpan(*gridItem, span));
                }
            }
        }
        std::sort(itemsSortedByIncreasingSpan.begin(), itemsSortedByIncreasingSpan.end());
    }

    auto it = itemsSortedByIncreasingSpan.begin();
    auto end = itemsSortedByIncreasingSpan.end();
    while (it != end) {
        GridItemsSpanGroupRange spanGroupRange = { it, std::upper_bound(it, end, *it) };
        increaseSizesToAccommodateSpanningItems<ResolveIntrinsicMinimums>(spanGroupRange);
        increaseSizesToAccommodateSpanningItems<ResolveContentBasedMinimums>(spanGroupRange);
        increaseSizesToAccommodateSpanningItems<ResolveMaxContentMinimums>(spanGroupRange);
        increaseSizesToAccommodateSpanningItems<ResolveIntrinsicMaximums>(spanGroupRange);
        increaseSizesToAccommodateSpanningItems<ResolveMaxContentMaximums>(spanGroupRange);
        it = spanGroupRange.rangeEnd;
    }

    for (auto trackIndex : m_contentSizedTracksIndex) {
        GridTrack& track = allTracks[trackIndex];
        if (track.growthLimit() == infinity)
            track.setGrowthLimit(track.baseSize());
    }
}

void GridTrackSizingAlgorithm::stretchFlexibleTracks(Optional<LayoutUnit> freeSpace)
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
        m_sizingState = RowSizingFirstIteration;
        return;
    case RowSizingFirstIteration:
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
    case ColumnSizingSecondIteration:
        return m_direction == ForColumns;
    case RowSizingFirstIteration:
    case RowSizingSecondIteration:
        return m_direction == ForRows;
    }
    ASSERT_NOT_REACHED();
    return false;
}

// GridTrackSizingAlgorithm API.

void GridTrackSizingAlgorithm::setup(GridTrackSizingDirection direction, unsigned numTracks, SizingOperation sizingOperation, Optional<LayoutUnit> availableSpace, Optional<LayoutUnit> freeSpace)
{
    ASSERT(m_needsSetup);
    m_direction = direction;
    setAvailableSpace(direction, availableSpace);

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

    setFreeSpace(direction, freeSpace);
    tracks(direction).resize(numTracks);

    m_needsSetup = false;
    m_hasPercentSizedRowsIndefiniteHeight = false;

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

void GridTrackSizingAlgorithm::run()
{
    ASSERT(wasSetup());
    StateMachine stateMachine(*this);

    // Step 1.
    const Optional<LayoutUnit> initialFreeSpace = freeSpace(m_direction);
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
    setAvailableSpace(ForRows, WTF::nullopt);
    setAvailableSpace(ForColumns, WTF::nullopt);
    m_hasPercentSizedRowsIndefiniteHeight = false;
}

#if ASSERT_ENABLED
bool GridTrackSizingAlgorithm::tracksAreWiderThanMinTrackBreadth() const
{
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
