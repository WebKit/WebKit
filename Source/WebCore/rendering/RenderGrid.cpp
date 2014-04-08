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

#include "config.h"
#include "RenderGrid.h"

#if ENABLE(CSS_GRID_LAYOUT)

#include "GridCoordinate.h"
#include "LayoutRepainter.h"
#include "NotImplemented.h"
#include "RenderLayer.h"
#include "RenderView.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

static const int infinity = -1;

class GridTrack {
public:
    GridTrack()
        : m_usedBreadth(0)
        , m_maxBreadth(0)
    {
    }

    void growUsedBreadth(LayoutUnit growth)
    {
        ASSERT(growth >= 0);
        m_usedBreadth += growth;
    }
    LayoutUnit usedBreadth() const { return m_usedBreadth; }

    void growMaxBreadth(LayoutUnit growth)
    {
        if (m_maxBreadth == infinity)
            m_maxBreadth = m_usedBreadth + growth;
        else
            m_maxBreadth += growth;
    }
    LayoutUnit maxBreadthIfNotInfinite() const
    {
        return (m_maxBreadth == infinity) ? m_usedBreadth : m_maxBreadth;
    }

    LayoutUnit m_usedBreadth;
    LayoutUnit m_maxBreadth;
};

struct GridTrackForNormalization {
    GridTrackForNormalization(const GridTrack& track, double flex)
        : m_track(&track)
        , m_flex(flex)
        , m_normalizedFlexValue(track.m_usedBreadth / flex)
    {
    }

    const GridTrack* m_track;
    double m_flex;
    LayoutUnit m_normalizedFlexValue;
};

class RenderGrid::GridIterator {
    WTF_MAKE_NONCOPYABLE(GridIterator);
public:
    // |direction| is the direction that is fixed to |fixedTrackIndex| so e.g
    // GridIterator(m_grid, ForColumns, 1) will walk over the rows of the 2nd column.
    GridIterator(const Vector<Vector<Vector<RenderBox*, 1>>>& grid, GridTrackSizingDirection direction, size_t fixedTrackIndex)
        : m_grid(grid)
        , m_direction(direction)
        , m_rowIndex((direction == ForColumns) ? 0 : fixedTrackIndex)
        , m_columnIndex((direction == ForColumns) ? fixedTrackIndex : 0)
        , m_childIndex(0)
    {
        ASSERT(m_rowIndex < m_grid.size());
        ASSERT(m_columnIndex < m_grid[0].size());
    }

    RenderBox* nextGridItem()
    {
        if (!m_grid.size())
            return 0;

        size_t& varyingTrackIndex = (m_direction == ForColumns) ? m_rowIndex : m_columnIndex;
        const size_t endOfVaryingTrackIndex = (m_direction == ForColumns) ? m_grid.size() : m_grid[0].size();
        for (; varyingTrackIndex < endOfVaryingTrackIndex; ++varyingTrackIndex) {
            const Vector<RenderBox*>& children = m_grid[m_rowIndex][m_columnIndex];
            if (m_childIndex < children.size())
                return children[m_childIndex++];

            m_childIndex = 0;
        }
        return 0;
    }

    std::unique_ptr<GridCoordinate> nextEmptyGridArea()
    {
        if (m_grid.isEmpty())
            return nullptr;

        size_t& varyingTrackIndex = (m_direction == ForColumns) ? m_rowIndex : m_columnIndex;
        const size_t endOfVaryingTrackIndex = (m_direction == ForColumns) ? m_grid.size() : m_grid[0].size();
        for (; varyingTrackIndex < endOfVaryingTrackIndex; ++varyingTrackIndex) {
            const Vector<RenderBox*>& children = m_grid[m_rowIndex][m_columnIndex];
            if (children.isEmpty()) {
                std::unique_ptr<GridCoordinate> result = std::make_unique<GridCoordinate>(GridSpan(m_rowIndex, m_rowIndex), GridSpan(m_columnIndex, m_columnIndex));
                // Advance the iterator to avoid an infinite loop where we would return the same grid area over and over.
                ++varyingTrackIndex;
                return std::move(result);
            }
        }
        return nullptr;
    }

private:
    const Vector<Vector<Vector<RenderBox*, 1>>>& m_grid;
    GridTrackSizingDirection m_direction;
    size_t m_rowIndex;
    size_t m_columnIndex;
    size_t m_childIndex;
};

class RenderGrid::GridSizingData {
    WTF_MAKE_NONCOPYABLE(GridSizingData);
public:
    GridSizingData(size_t gridColumnCount, size_t gridRowCount)
        : columnTracks(gridColumnCount)
        , rowTracks(gridRowCount)
    {
    }

    Vector<GridTrack> columnTracks;
    Vector<GridTrack> rowTracks;
    Vector<size_t> contentSizedTracksIndex;

    // Performance optimization: hold onto these Vectors until the end of Layout to avoid repeated malloc / free.
    Vector<LayoutUnit> distributeTrackVector;
    Vector<GridTrack*> filteredTracks;
};

RenderGrid::RenderGrid(Element& element, PassRef<RenderStyle> style)
    : RenderBlock(element, std::move(style), 0)
    , m_orderIterator(*this)
{
    // All of our children must be block level.
    setChildrenInline(false);
}

RenderGrid::~RenderGrid()
{
}

void RenderGrid::layoutBlock(bool relayoutChildren, LayoutUnit)
{
    ASSERT(needsLayout());

    if (!relayoutChildren && simplifiedLayout())
        return;

    // FIXME: Much of this method is boiler plate that matches RenderBox::layoutBlock and Render*FlexibleBox::layoutBlock.
    // It would be nice to refactor some of the duplicate code.
    LayoutRepainter repainter(*this, checkForRepaintDuringLayout());
    LayoutStateMaintainer statePusher(view(), *this, locationOffset(), hasTransform() || hasReflection() || style().isFlippedBlocksWritingMode());

    preparePaginationBeforeBlockLayout(relayoutChildren);

    LayoutSize previousSize = size();

    setLogicalHeight(0);
    updateLogicalWidth();

    layoutGridItems();

    LayoutUnit oldClientAfterEdge = clientLogicalBottom();
    updateLogicalHeight();

    if (size() != previousSize)
        relayoutChildren = true;

    layoutPositionedObjects(relayoutChildren || isRoot());

    computeOverflow(oldClientAfterEdge);
    statePusher.pop();

    updateLayerTransform();

    // Update our scroll information if we're overflow:auto/scroll/hidden now that we know if
    // we overflow or not.
    updateScrollInfoAfterLayout();

    repainter.repaintAfterLayout();

    clearNeedsLayout();
}

void RenderGrid::computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    const_cast<RenderGrid*>(this)->placeItemsOnGrid();

    GridSizingData sizingData(gridColumnCount(), gridRowCount());
    LayoutUnit availableLogicalSpace = 0;
    const_cast<RenderGrid*>(this)->computeUsedBreadthOfGridTracks(ForColumns, sizingData, availableLogicalSpace);

    for (size_t i = 0; i < sizingData.columnTracks.size(); ++i) {
        LayoutUnit minTrackBreadth = sizingData.columnTracks[i].m_usedBreadth;
        LayoutUnit maxTrackBreadth = sizingData.columnTracks[i].m_maxBreadth;
        maxTrackBreadth = std::max(maxTrackBreadth, minTrackBreadth);

        minLogicalWidth += minTrackBreadth;
        maxLogicalWidth += maxTrackBreadth;

        // FIXME: This should add in the scrollbarWidth (e.g. see RenderFlexibleBox).
    }

    const_cast<RenderGrid*>(this)->clearGrid();
}

void RenderGrid::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());

    m_minPreferredLogicalWidth = 0;
    m_maxPreferredLogicalWidth = 0;

    // FIXME: We don't take our own logical width into account. Once we do, we need to make sure
    // we apply (and test the interaction with) min-width / max-width.

    computeIntrinsicLogicalWidths(m_minPreferredLogicalWidth, m_maxPreferredLogicalWidth);

    LayoutUnit borderAndPaddingInInlineDirection = borderAndPaddingLogicalWidth();
    m_minPreferredLogicalWidth += borderAndPaddingInInlineDirection;
    m_maxPreferredLogicalWidth += borderAndPaddingInInlineDirection;

    setPreferredLogicalWidthsDirty(false);
}

void RenderGrid::computeUsedBreadthOfGridTracks(GridTrackSizingDirection direction, GridSizingData& sizingData)
{
    LayoutUnit availableLogicalSpace = (direction == ForColumns) ? availableLogicalWidth() : availableLogicalHeight(IncludeMarginBorderPadding);
    computeUsedBreadthOfGridTracks(direction, sizingData, availableLogicalSpace);
}

bool RenderGrid::gridElementIsShrinkToFit()
{
    return isFloatingOrOutOfFlowPositioned();
}

void RenderGrid::computeUsedBreadthOfGridTracks(GridTrackSizingDirection direction, GridSizingData& sizingData, LayoutUnit& availableLogicalSpace)
{
    Vector<GridTrack>& tracks = (direction == ForColumns) ? sizingData.columnTracks : sizingData.rowTracks;
    Vector<size_t> flexibleSizedTracksIndex;
    sizingData.contentSizedTracksIndex.shrink(0);

    // 1. Initialize per Grid track variables.
    for (size_t i = 0; i < tracks.size(); ++i) {
        GridTrack& track = tracks[i];
        const GridTrackSize& trackSize = gridTrackSize(direction, i);
        const GridLength& minTrackBreadth = trackSize.minTrackBreadth();
        const GridLength& maxTrackBreadth = trackSize.maxTrackBreadth();

        track.m_usedBreadth = computeUsedBreadthOfMinLength(direction, minTrackBreadth);
        track.m_maxBreadth = computeUsedBreadthOfMaxLength(direction, maxTrackBreadth, track.m_usedBreadth);

        track.m_maxBreadth = std::max(track.m_maxBreadth, track.m_usedBreadth);

        if (trackSize.isContentSized())
            sizingData.contentSizedTracksIndex.append(i);
        if (trackSize.maxTrackBreadth().isFlex())
            flexibleSizedTracksIndex.append(i);
    }

    // 2. Resolve content-based TrackSizingFunctions.
    if (!sizingData.contentSizedTracksIndex.isEmpty())
        resolveContentBasedTrackSizingFunctions(direction, sizingData);

    for (size_t i = 0; i < tracks.size(); ++i) {
        ASSERT(tracks[i].m_maxBreadth != infinity);
        availableLogicalSpace -= tracks[i].m_usedBreadth;
    }

    const bool hasUndefinedRemainingSpace = (direction == ForRows) ? style().logicalHeight().isAuto() : gridElementIsShrinkToFit();

    if (!hasUndefinedRemainingSpace && availableLogicalSpace <= 0)
        return;

    // 3. Grow all Grid tracks in GridTracks from their UsedBreadth up to their MaxBreadth value until
    // availableLogicalSpace (RemainingSpace in the specs) is exhausted.
    const size_t tracksSize = tracks.size();
    if (!hasUndefinedRemainingSpace) {
        Vector<GridTrack*> tracksForDistribution(tracksSize);
        for (size_t i = 0; i < tracksSize; ++i)
            tracksForDistribution[i] = tracks.data() + i;

        distributeSpaceToTracks(tracksForDistribution, 0, &GridTrack::usedBreadth, &GridTrack::growUsedBreadth, sizingData, availableLogicalSpace);
    } else {
        for (size_t i = 0; i < tracksSize; ++i)
            tracks[i].m_usedBreadth = tracks[i].m_maxBreadth;
    }

    if (flexibleSizedTracksIndex.isEmpty())
        return;

    // 4. Grow all Grid tracks having a fraction as the MaxTrackSizingFunction.
    double normalizedFractionBreadth = 0;
    if (!hasUndefinedRemainingSpace)
        normalizedFractionBreadth = computeNormalizedFractionBreadth(tracks, GridSpan(0, tracks.size() - 1), direction, availableLogicalSpace);
    else {
        for (size_t i = 0; i < flexibleSizedTracksIndex.size(); ++i) {
            const size_t trackIndex = flexibleSizedTracksIndex[i];
            const GridTrackSize& trackSize = gridTrackSize(direction, trackIndex);
            normalizedFractionBreadth = std::max(normalizedFractionBreadth, tracks[trackIndex].m_usedBreadth / trackSize.maxTrackBreadth().flex());
        }

        for (size_t i = 0; i < flexibleSizedTracksIndex.size(); ++i) {
            GridIterator iterator(m_grid, direction, flexibleSizedTracksIndex[i]);
            while (RenderBox* gridItem = iterator.nextGridItem()) {
                const GridCoordinate coordinate = cachedGridCoordinate(gridItem);
                const GridSpan span = (direction == ForColumns) ? coordinate.columns : coordinate.rows;

                // Do not include already processed items.
                if (i > 0 && span.initialPositionIndex <= flexibleSizedTracksIndex[i - 1])
                    continue;

                double itemNormalizedFlexBreadth = computeNormalizedFractionBreadth(tracks, span, direction, maxContentForChild(gridItem, direction, sizingData.columnTracks));
                normalizedFractionBreadth = std::max(normalizedFractionBreadth, itemNormalizedFlexBreadth);
            }
        }
    }

    for (size_t i = 0; i < flexibleSizedTracksIndex.size(); ++i) {
        const size_t trackIndex = flexibleSizedTracksIndex[i];
        const GridTrackSize& trackSize = gridTrackSize(direction, trackIndex);

        tracks[trackIndex].m_usedBreadth = std::max<LayoutUnit>(tracks[trackIndex].m_usedBreadth, normalizedFractionBreadth * trackSize.maxTrackBreadth().flex());
    }
}

LayoutUnit RenderGrid::computeUsedBreadthOfMinLength(GridTrackSizingDirection direction, const GridLength& gridLength) const
{
    if (gridLength.isFlex())
        return 0;

    const Length& trackLength = gridLength.length();
    ASSERT(!trackLength.isAuto());
    if (trackLength.isSpecified())
        return computeUsedBreadthOfSpecifiedLength(direction, trackLength);

    ASSERT(trackLength.isMinContent() || trackLength.isMaxContent());
    return 0;
}

LayoutUnit RenderGrid::computeUsedBreadthOfMaxLength(GridTrackSizingDirection direction, const GridLength& gridLength, LayoutUnit usedBreadth) const
{
    if (gridLength.isFlex())
        return usedBreadth;

    const Length& trackLength = gridLength.length();
    ASSERT(!trackLength.isAuto());
    if (trackLength.isSpecified()) {
        LayoutUnit computedBreadth = computeUsedBreadthOfSpecifiedLength(direction, trackLength);
        ASSERT(computedBreadth != infinity);
        return computedBreadth;
    }

    ASSERT(trackLength.isMinContent() || trackLength.isMaxContent());
    return infinity;
}

LayoutUnit RenderGrid::computeUsedBreadthOfSpecifiedLength(GridTrackSizingDirection direction, const Length& trackLength) const
{
    ASSERT(trackLength.isSpecified());
    return valueForLength(trackLength, direction == ForColumns ? logicalWidth() : computeContentLogicalHeight(style().logicalHeight()));
}

double RenderGrid::computeNormalizedFractionBreadth(Vector<GridTrack>& tracks, const GridSpan& tracksSpan, GridTrackSizingDirection direction, LayoutUnit availableLogicalSpace) const
{
    // |availableLogicalSpace| already accounts for the used breadths so no need to remove it here.

    Vector<GridTrackForNormalization> tracksForNormalization;
    for (size_t i = tracksSpan.initialPositionIndex; i <= tracksSpan.finalPositionIndex; ++i) {
        const GridTrackSize& trackSize = gridTrackSize(direction, i);
        if (!trackSize.maxTrackBreadth().isFlex())
            continue;

        tracksForNormalization.append(GridTrackForNormalization(tracks[i], trackSize.maxTrackBreadth().flex()));
    }

    // The function is not called if we don't have <flex> grid tracks
    ASSERT(!tracksForNormalization.isEmpty());

    std::sort(tracksForNormalization.begin(), tracksForNormalization.end(),
              [](const GridTrackForNormalization& track1, const GridTrackForNormalization& track2) {
                  return track1.m_normalizedFlexValue < track2.m_normalizedFlexValue;
              });

    // These values work together: as we walk over our grid tracks, we increase fractionValueBasedOnGridItemsRatio
    // to match a grid track's usedBreadth to <flex> ratio until the total fractions sized grid tracks wouldn't
    // fit into availableLogicalSpaceIgnoringFractionTracks.
    double accumulatedFractions = 0;
    LayoutUnit fractionValueBasedOnGridItemsRatio = 0;
    LayoutUnit availableLogicalSpaceIgnoringFractionTracks = availableLogicalSpace;

    for (size_t i = 0; i < tracksForNormalization.size(); ++i) {
        const GridTrackForNormalization& track = tracksForNormalization[i];
        if (track.m_normalizedFlexValue > fractionValueBasedOnGridItemsRatio) {
            // If the normalized flex value (we ordered |tracksForNormalization| by increasing normalized flex value)
            // will make us overflow our container, then stop. We have the previous step's ratio is the best fit.
            if (track.m_normalizedFlexValue * accumulatedFractions > availableLogicalSpaceIgnoringFractionTracks)
                break;

            fractionValueBasedOnGridItemsRatio = track.m_normalizedFlexValue;
        }

        accumulatedFractions += track.m_flex;
        // This item was processed so we re-add its used breadth to the available space to accurately count the remaining space.
        availableLogicalSpaceIgnoringFractionTracks += track.m_track->m_usedBreadth;
    }

    return availableLogicalSpaceIgnoringFractionTracks / accumulatedFractions;
}

const GridTrackSize& RenderGrid::gridTrackSize(GridTrackSizingDirection direction, size_t i) const
{
    const Vector<GridTrackSize>& trackStyles = (direction == ForColumns) ? style().gridColumns() : style().gridRows();
    if (i >= trackStyles.size())
        return (direction == ForColumns) ? style().gridAutoColumns() : style().gridAutoRows();

    const GridTrackSize& trackSize = trackStyles[i];
    // If the logical width/height of the grid container is indefinite, percentage values are treated as <auto>.
    if (trackSize.isPercentage()) {
        Length logicalSize = direction == ForColumns ? style().logicalWidth() : style().logicalHeight();
        if (logicalSize.isIntrinsicOrAuto()) {
            static NeverDestroyed<GridTrackSize> autoTrackSize(Auto);
            return autoTrackSize.get();
        }
    }

    return trackSize;
}

size_t RenderGrid::explicitGridColumnCount() const
{
    return style().gridColumns().size();
}

size_t RenderGrid::explicitGridRowCount() const
{
    return style().gridRows().size();
}

static inline bool isColumnSide(GridPositionSide side)
{
    return side == ColumnStartSide || side == ColumnEndSide;
}

size_t RenderGrid::explicitGridSizeForSide(GridPositionSide side) const
{
    return isColumnSide(side) ? explicitGridColumnCount() : explicitGridRowCount();
}

LayoutUnit RenderGrid::logicalContentHeightForChild(RenderBox* child, Vector<GridTrack>& columnTracks)
{
    LayoutUnit oldOverrideContainingBlockContentLogicalWidth = child->hasOverrideContainingBlockLogicalWidth() ? child->overrideContainingBlockContentLogicalWidth() : LayoutUnit();
    LayoutUnit overrideContainingBlockContentLogicalWidth = gridAreaBreadthForChild(child, ForColumns, columnTracks);
    if (child->style().logicalHeight().isPercent() || oldOverrideContainingBlockContentLogicalWidth != overrideContainingBlockContentLogicalWidth)
        child->setNeedsLayout(MarkOnlyThis);

    child->setOverrideContainingBlockContentLogicalWidth(overrideContainingBlockContentLogicalWidth);
    // If |child| has a percentage logical height, we shouldn't let it override its intrinsic height, which is
    // what we are interested in here. Thus we need to set the override logical height to -1 (no possible resolution).
    child->setOverrideContainingBlockContentLogicalHeight(-1);
    child->layoutIfNeeded();
    return child->logicalHeight() + child->marginLogicalHeight();
}

LayoutUnit RenderGrid::minContentForChild(RenderBox* child, GridTrackSizingDirection direction, Vector<GridTrack>& columnTracks)
{
    bool hasOrthogonalWritingMode = child->isHorizontalWritingMode() != isHorizontalWritingMode();
    // FIXME: Properly support orthogonal writing mode.
    if (hasOrthogonalWritingMode)
        return 0;

    if (direction == ForColumns) {
        // FIXME: It's unclear if we should return the intrinsic width or the preferred width.
        // See http://lists.w3.org/Archives/Public/www-style/2013Jan/0245.html
        return child->minPreferredLogicalWidth() + marginIntrinsicLogicalWidthForChild(*child);
    }

    return logicalContentHeightForChild(child, columnTracks);
}

LayoutUnit RenderGrid::maxContentForChild(RenderBox* child, GridTrackSizingDirection direction, Vector<GridTrack>& columnTracks)
{
    bool hasOrthogonalWritingMode = child->isHorizontalWritingMode() != isHorizontalWritingMode();
    // FIXME: Properly support orthogonal writing mode.
    if (hasOrthogonalWritingMode)
        return LayoutUnit();

    if (direction == ForColumns) {
        // FIXME: It's unclear if we should return the intrinsic width or the preferred width.
        // See http://lists.w3.org/Archives/Public/www-style/2013Jan/0245.html
        return child->maxPreferredLogicalWidth() + marginIntrinsicLogicalWidthForChild(*child);
    }

    return logicalContentHeightForChild(child, columnTracks);
}

void RenderGrid::resolveContentBasedTrackSizingFunctions(GridTrackSizingDirection direction, GridSizingData& sizingData)
{
    // FIXME: Split the grid tracks into groups that doesn't overlap a <flex> grid track.

    for (size_t i = 0; i < sizingData.contentSizedTracksIndex.size(); ++i) {
        GridIterator iterator(m_grid, direction, sizingData.contentSizedTracksIndex[i]);
        while (RenderBox* gridItem = iterator.nextGridItem()) {
            resolveContentBasedTrackSizingFunctionsForItems(direction, sizingData, gridItem, &GridTrackSize::hasMinOrMaxContentMinTrackBreadth, &RenderGrid::minContentForChild, &GridTrack::usedBreadth, &GridTrack::growUsedBreadth);
            resolveContentBasedTrackSizingFunctionsForItems(direction, sizingData, gridItem, &GridTrackSize::hasMaxContentMinTrackBreadth, &RenderGrid::maxContentForChild, &GridTrack::usedBreadth, &GridTrack::growUsedBreadth);
            resolveContentBasedTrackSizingFunctionsForItems(direction, sizingData, gridItem, &GridTrackSize::hasMinOrMaxContentMaxTrackBreadth, &RenderGrid::minContentForChild, &GridTrack::maxBreadthIfNotInfinite, &GridTrack::growMaxBreadth);
            resolveContentBasedTrackSizingFunctionsForItems(direction, sizingData, gridItem, &GridTrackSize::hasMaxContentMaxTrackBreadth, &RenderGrid::maxContentForChild, &GridTrack::maxBreadthIfNotInfinite, &GridTrack::growMaxBreadth);
        }

        GridTrack& track = (direction == ForColumns) ? sizingData.columnTracks[i] : sizingData.rowTracks[i];
        if (track.m_maxBreadth == infinity)
            track.m_maxBreadth = track.m_usedBreadth;
    }
}

void RenderGrid::resolveContentBasedTrackSizingFunctionsForItems(GridTrackSizingDirection direction, GridSizingData& sizingData, RenderBox* gridItem, FilterFunction filterFunction, SizingFunction sizingFunction, AccumulatorGetter trackGetter, AccumulatorGrowFunction trackGrowthFunction)
{
    const GridCoordinate coordinate = cachedGridCoordinate(gridItem);
    const size_t initialTrackIndex = (direction == ForColumns) ? coordinate.columns.initialPositionIndex : coordinate.rows.initialPositionIndex;
    const size_t finalTrackIndex = (direction == ForColumns) ? coordinate.columns.finalPositionIndex : coordinate.rows.finalPositionIndex;

    sizingData.filteredTracks.shrink(0);
    for (size_t trackIndex = initialTrackIndex; trackIndex <= finalTrackIndex; ++trackIndex) {
        const GridTrackSize& trackSize = gridTrackSize(direction, trackIndex);
        if (!(trackSize.*filterFunction)())
            continue;

        GridTrack& track = (direction == ForColumns) ? sizingData.columnTracks[trackIndex] : sizingData.rowTracks[trackIndex];
        sizingData.filteredTracks.append(&track);
    }

    if (sizingData.filteredTracks.isEmpty())
        return;

    LayoutUnit additionalBreadthSpace = (this->*sizingFunction)(gridItem, direction, sizingData.columnTracks);
    for (size_t trackIndexForSpace = initialTrackIndex; trackIndexForSpace <= finalTrackIndex; ++trackIndexForSpace) {
        GridTrack& track = (direction == ForColumns) ? sizingData.columnTracks[trackIndexForSpace] : sizingData.rowTracks[trackIndexForSpace];
        additionalBreadthSpace -= (track.*trackGetter)();
    }

    // FIXME: We should pass different values for |tracksForGrowthAboveMaxBreadth|.
    distributeSpaceToTracks(sizingData.filteredTracks, &sizingData.filteredTracks, trackGetter, trackGrowthFunction, sizingData, additionalBreadthSpace);
}

static bool sortByGridTrackGrowthPotential(const GridTrack* track1, const GridTrack* track2)
{
    return (track1->m_maxBreadth - track1->m_usedBreadth) < (track2->m_maxBreadth - track2->m_usedBreadth);
}

void RenderGrid::distributeSpaceToTracks(Vector<GridTrack*>& tracks, Vector<GridTrack*>* tracksForGrowthAboveMaxBreadth, AccumulatorGetter trackGetter, AccumulatorGrowFunction trackGrowthFunction, GridSizingData& sizingData, LayoutUnit& availableLogicalSpace)
{
    std::sort(tracks.begin(), tracks.end(), sortByGridTrackGrowthPotential);

    size_t tracksSize = tracks.size();
    sizingData.distributeTrackVector.resize(tracksSize);

    for (size_t i = 0; i < tracksSize; ++i) {
        GridTrack& track = *tracks[i];
        LayoutUnit availableLogicalSpaceShare = availableLogicalSpace / (tracksSize - i);
        LayoutUnit trackBreadth = (tracks[i]->*trackGetter)();
        LayoutUnit growthShare = std::max(LayoutUnit(), std::min(availableLogicalSpaceShare, track.m_maxBreadth - trackBreadth));
        // We should never shrink any grid track or else we can't guarantee we abide by our min-sizing function.
        sizingData.distributeTrackVector[i] = trackBreadth + growthShare;
        availableLogicalSpace -= growthShare;
    }

    if (availableLogicalSpace > 0 && tracksForGrowthAboveMaxBreadth) {
        tracksSize = tracksForGrowthAboveMaxBreadth->size();
        for (size_t i = 0; i < tracksSize; ++i) {
            LayoutUnit growthShare = availableLogicalSpace / (tracksSize - i);
            sizingData.distributeTrackVector[i] += growthShare;
            availableLogicalSpace -= growthShare;
        }
    }

    for (size_t i = 0; i < tracksSize; ++i) {
        LayoutUnit growth = sizingData.distributeTrackVector[i] - (tracks[i]->*trackGetter)();
        if (growth >= 0)
            (tracks[i]->*trackGrowthFunction)(growth);
    }
}

#ifndef NDEBUG
bool RenderGrid::tracksAreWiderThanMinTrackBreadth(GridTrackSizingDirection direction, const Vector<GridTrack>& tracks)
{
    for (size_t i = 0; i < tracks.size(); ++i) {
        const GridTrackSize& trackSize = gridTrackSize(direction, i);
        const GridLength& minTrackBreadth = trackSize.minTrackBreadth();
        if (computeUsedBreadthOfMinLength(direction, minTrackBreadth) > tracks[i].m_usedBreadth)
            return false;
    }
    return true;
}
#endif

void RenderGrid::growGrid(GridTrackSizingDirection direction)
{
    if (direction == ForColumns) {
        const size_t oldColumnSize = m_grid[0].size();
        for (size_t row = 0; row < m_grid.size(); ++row)
            m_grid[row].grow(oldColumnSize + 1);
    } else {
        const size_t oldRowSize = m_grid.size();
        m_grid.grow(oldRowSize + 1);
        m_grid[oldRowSize].grow(m_grid[0].size());
    }
}

void RenderGrid::insertItemIntoGrid(RenderBox* child, const GridCoordinate& coordinate)
{
    for (size_t row = coordinate.rows.initialPositionIndex; row <= coordinate.rows.finalPositionIndex; ++row) {
        for (size_t column = coordinate.columns.initialPositionIndex; column <= coordinate.columns.finalPositionIndex; ++column)
            m_grid[row][column].append(child);
    }
    m_gridItemCoordinate.set(child, coordinate);
}

void RenderGrid::insertItemIntoGrid(RenderBox* child, size_t rowTrack, size_t columnTrack)
{
    const GridSpan& rowSpan = resolveGridPositionsFromAutoPlacementPosition(child, ForRows, rowTrack);
    const GridSpan& columnSpan = resolveGridPositionsFromAutoPlacementPosition(child, ForColumns, columnTrack);
    insertItemIntoGrid(child, GridCoordinate(rowSpan, columnSpan));
}

void RenderGrid::placeItemsOnGrid()
{
    ASSERT(!gridWasPopulated());
    ASSERT(m_gridItemCoordinate.isEmpty());

    populateExplicitGridAndOrderIterator();

    Vector<RenderBox*> autoMajorAxisAutoGridItems;
    Vector<RenderBox*> specifiedMajorAxisAutoGridItems;
    GridAutoFlow autoFlow = style().gridAutoFlow();
    for (RenderBox* child = m_orderIterator.first(); child; child = m_orderIterator.next()) {
        // FIXME: We never re-resolve positions if the grid is grown during auto-placement which may lead auto / <integer>
        // positions to not match the author's intent. The specification is unclear on what should be done in this case.
        std::unique_ptr<GridSpan> rowPositions = resolveGridPositionsFromStyle(child, ForRows);
        std::unique_ptr<GridSpan> columnPositions = resolveGridPositionsFromStyle(child, ForColumns);
        if (!rowPositions || !columnPositions) {
            GridSpan* majorAxisPositions = (autoPlacementMajorAxisDirection() == ForColumns) ? columnPositions.get() : rowPositions.get();
            if (!majorAxisPositions)
                autoMajorAxisAutoGridItems.append(child);
            else
                specifiedMajorAxisAutoGridItems.append(child);
            continue;
        }
        insertItemIntoGrid(child, GridCoordinate(*rowPositions, *columnPositions));
    }

    ASSERT(gridRowCount() >= style().gridRows().size());
    ASSERT(gridColumnCount() >= style().gridColumns().size());

    if (autoFlow == AutoFlowNone) {
        // If we did collect some grid items, they won't be placed thus never laid out.
        ASSERT(!autoMajorAxisAutoGridItems.size());
        ASSERT(!specifiedMajorAxisAutoGridItems.size());
        return;
    }

    placeSpecifiedMajorAxisItemsOnGrid(specifiedMajorAxisAutoGridItems);
    placeAutoMajorAxisItemsOnGrid(autoMajorAxisAutoGridItems);
}

void RenderGrid::populateExplicitGridAndOrderIterator()
{
    // FIXME: We should find a way to share OrderValues's initialization code with RenderFlexibleBox.
    OrderIterator::OrderValues orderValues;
    size_t maximumRowIndex = std::max<size_t>(1, explicitGridRowCount());
    size_t maximumColumnIndex = std::max<size_t>(1, explicitGridColumnCount());

    for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        // Avoid growing the vector for the common-case default value of 0. This optimizes the most common case which is
        // one or a few values with the default order 0
        int order = child->style().order();
        if (orderValues.isEmpty() || orderValues.last() != order)
            orderValues.append(order);

        // This function bypasses the cache (cachedGridCoordinate()) as it is used to build it.
        std::unique_ptr<GridSpan> rowPositions = resolveGridPositionsFromStyle(child, ForRows);
        std::unique_ptr<GridSpan> columnPositions = resolveGridPositionsFromStyle(child, ForColumns);

        // |positions| is 0 if we need to run the auto-placement algorithm. Our estimation ignores
        // this case as the auto-placement algorithm will grow the grid as needed.
        if (rowPositions)
            maximumRowIndex = std::max(maximumRowIndex, rowPositions->finalPositionIndex + 1);
        if (columnPositions)
            maximumColumnIndex = std::max(maximumColumnIndex, columnPositions->finalPositionIndex + 1);
    }

    m_grid.grow(maximumRowIndex);
    for (size_t i = 0; i < m_grid.size(); ++i)
        m_grid[i].grow(maximumColumnIndex);

    m_orderIterator.setOrderValues(std::move(orderValues));
}

void RenderGrid::placeSpecifiedMajorAxisItemsOnGrid(Vector<RenderBox*> autoGridItems)
{
    for (size_t i = 0; i < autoGridItems.size(); ++i) {
        std::unique_ptr<GridSpan> majorAxisPositions = resolveGridPositionsFromStyle(autoGridItems[i], autoPlacementMajorAxisDirection());
        GridIterator iterator(m_grid, autoPlacementMajorAxisDirection(), majorAxisPositions->initialPositionIndex);
        if (std::unique_ptr<GridCoordinate> emptyGridArea = iterator.nextEmptyGridArea()) {
            insertItemIntoGrid(autoGridItems[i], emptyGridArea->rows.initialPositionIndex, emptyGridArea->columns.initialPositionIndex);
            continue;
        }

        growGrid(autoPlacementMinorAxisDirection());
        std::unique_ptr<GridCoordinate> emptyGridArea = iterator.nextEmptyGridArea();
        ASSERT(emptyGridArea);
        insertItemIntoGrid(autoGridItems[i], emptyGridArea->rows.initialPositionIndex, emptyGridArea->columns.initialPositionIndex);
    }
}

void RenderGrid::placeAutoMajorAxisItemsOnGrid(Vector<RenderBox*> autoGridItems)
{
    for (size_t i = 0; i < autoGridItems.size(); ++i)
        placeAutoMajorAxisItemOnGrid(autoGridItems[i]);
}

void RenderGrid::placeAutoMajorAxisItemOnGrid(RenderBox* gridItem)
{
    std::unique_ptr<GridSpan> minorAxisPositions = resolveGridPositionsFromStyle(gridItem, autoPlacementMinorAxisDirection());
    ASSERT(!resolveGridPositionsFromStyle(gridItem, autoPlacementMajorAxisDirection()));
    size_t minorAxisIndex = 0;
    if (minorAxisPositions) {
        minorAxisIndex = minorAxisPositions->initialPositionIndex;
        GridIterator iterator(m_grid, autoPlacementMinorAxisDirection(), minorAxisIndex);
        if (std::unique_ptr<GridCoordinate> emptyGridArea = iterator.nextEmptyGridArea()) {
            insertItemIntoGrid(gridItem, emptyGridArea->rows.initialPositionIndex, emptyGridArea->columns.initialPositionIndex);
            return;
        }
    } else {
        const size_t endOfMajorAxis = (autoPlacementMajorAxisDirection() == ForColumns) ? gridColumnCount() : gridRowCount();
        for (size_t majorAxisIndex = 0; majorAxisIndex < endOfMajorAxis; ++majorAxisIndex) {
            GridIterator iterator(m_grid, autoPlacementMajorAxisDirection(), majorAxisIndex);
            if (std::unique_ptr<GridCoordinate> emptyGridArea = iterator.nextEmptyGridArea()) {
                insertItemIntoGrid(gridItem, emptyGridArea->rows.initialPositionIndex, emptyGridArea->columns.initialPositionIndex);
                return;
            }
        }
    }

    // We didn't find an empty grid area so we need to create an extra major axis line and insert our gridItem in it.
    const size_t columnIndex = (autoPlacementMajorAxisDirection() == ForColumns) ? m_grid[0].size() : minorAxisIndex;
    const size_t rowIndex = (autoPlacementMajorAxisDirection() == ForColumns) ? minorAxisIndex : m_grid.size();
    growGrid(autoPlacementMajorAxisDirection());
    insertItemIntoGrid(gridItem, rowIndex, columnIndex);
}

RenderGrid::GridTrackSizingDirection RenderGrid::autoPlacementMajorAxisDirection() const
{
    GridAutoFlow flow = style().gridAutoFlow();
    ASSERT(flow != AutoFlowNone);
    return (flow == AutoFlowColumn) ? ForColumns : ForRows;
}

RenderGrid::GridTrackSizingDirection RenderGrid::autoPlacementMinorAxisDirection() const
{
    GridAutoFlow flow = style().gridAutoFlow();
    ASSERT(flow != AutoFlowNone);
    return (flow == AutoFlowColumn) ? ForRows : ForColumns;
}

void RenderGrid::clearGrid()
{
    m_grid.clear();
    m_gridItemCoordinate.clear();
}

void RenderGrid::layoutGridItems()
{
    placeItemsOnGrid();

    GridSizingData sizingData(gridColumnCount(), gridRowCount());
    computeUsedBreadthOfGridTracks(ForColumns, sizingData);
    ASSERT(tracksAreWiderThanMinTrackBreadth(ForColumns, sizingData.columnTracks));
    computeUsedBreadthOfGridTracks(ForRows, sizingData);
    ASSERT(tracksAreWiderThanMinTrackBreadth(ForRows, sizingData.rowTracks));

    populateGridPositions(sizingData);

    for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        // Because the grid area cannot be styled, we don't need to adjust
        // the grid breadth to account for 'box-sizing'.
        LayoutUnit oldOverrideContainingBlockContentLogicalWidth = child->hasOverrideContainingBlockLogicalWidth() ? child->overrideContainingBlockContentLogicalWidth() : LayoutUnit();
        LayoutUnit oldOverrideContainingBlockContentLogicalHeight = child->hasOverrideContainingBlockLogicalHeight() ? child->overrideContainingBlockContentLogicalHeight() : LayoutUnit();

        LayoutUnit overrideContainingBlockContentLogicalWidth = gridAreaBreadthForChild(child, ForColumns, sizingData.columnTracks);
        LayoutUnit overrideContainingBlockContentLogicalHeight = gridAreaBreadthForChild(child, ForRows, sizingData.rowTracks);
        if (oldOverrideContainingBlockContentLogicalWidth != overrideContainingBlockContentLogicalWidth || (oldOverrideContainingBlockContentLogicalHeight != overrideContainingBlockContentLogicalHeight && (child->hasRelativeLogicalHeight() || child->hasViewportPercentageLogicalHeight())))
            child->setNeedsLayout(MarkOnlyThis);

        child->setOverrideContainingBlockContentLogicalWidth(overrideContainingBlockContentLogicalWidth);
        child->setOverrideContainingBlockContentLogicalHeight(overrideContainingBlockContentLogicalHeight);

        LayoutRect oldChildRect = child->frameRect();

        // FIXME: Grid items should stretch to fill their cells. Once we
        // implement grid-{column,row}-align, we can also shrink to fit. For
        // now, just size as if we were a regular child.
        child->layoutIfNeeded();

        child->setLogicalLocation(findChildLogicalPosition(child, sizingData));

        // If the child moved, we have to repaint it as well as any floating/positioned
        // descendants. An exception is if we need a layout. In this case, we know we're going to
        // repaint ourselves (and the child) anyway.
        if (!selfNeedsLayout() && child->checkForRepaintDuringLayout())
            child->repaintDuringLayoutIfMoved(oldChildRect);
    }

    for (size_t i = 0; i < sizingData.rowTracks.size(); ++i)
        setLogicalHeight(logicalHeight() + sizingData.rowTracks[i].m_usedBreadth);

    // min / max logical height is handled in updateLogicalHeight().
    setLogicalHeight(logicalHeight() + borderAndPaddingLogicalHeight());
    clearGrid();
}

GridCoordinate RenderGrid::cachedGridCoordinate(const RenderBox* gridItem) const
{
    ASSERT(m_gridItemCoordinate.contains(gridItem));
    return m_gridItemCoordinate.get(gridItem);
}

GridSpan RenderGrid::resolveGridPositionsFromAutoPlacementPosition(const RenderBox*, GridTrackSizingDirection, size_t initialPosition) const
{
    // FIXME: We don't support spanning with auto positions yet. Once we do, this is wrong. Also we should make
    // sure the grid can accomodate the new item as we only grow 1 position in a given direction.
    return GridSpan(initialPosition, initialPosition);
}

static inline bool gridLineDefinedBeforeGridArea(size_t namedGridLineFirstDefinition, const GridCoordinate& gridAreaCoordinates, GridPositionSide side)
{
    switch (side) {
    case ColumnStartSide:
        return namedGridLineFirstDefinition < gridAreaCoordinates.columns.initialPositionIndex;
    case ColumnEndSide:
        return namedGridLineFirstDefinition < gridAreaCoordinates.columns.finalPositionIndex;
    case RowStartSide:
        return namedGridLineFirstDefinition < gridAreaCoordinates.rows.initialPositionIndex;
    case RowEndSide:
        return namedGridLineFirstDefinition < gridAreaCoordinates.rows.finalPositionIndex;
    }
    ASSERT_NOT_REACHED();
    return false;
}

static void setNamedLinePositionIfDefinedBeforeArea(GridPosition& position, const NamedGridLinesMap& namedLinesMap, const String& lineName, const GridCoordinate& gridAreaCoordinate, GridPositionSide side)
{
    auto linesIterator = namedLinesMap.find(lineName);
    if (linesIterator == namedLinesMap.end())
        return;

    size_t namedGridLineFirstDefinition = GridPosition::adjustGridPositionForSide(linesIterator->value[0], side);
    if (gridLineDefinedBeforeGridArea(namedGridLineFirstDefinition, gridAreaCoordinate, side))
        position.setExplicitPosition(1, lineName);
}

void RenderGrid::adjustNamedGridItemPosition(GridPosition& position, GridPositionSide side) const
{
    // The StyleBuilder always treats <custom-ident> as a named grid area. We must decide here if they are going to be
    // resolved to either a grid area or a grid line.
    ASSERT(position.isNamedGridArea());
    const NamedGridAreaMap& gridAreaMap = style().namedGridArea();
    const NamedGridLinesMap& namedLinesMap = isColumnSide(side) ? style().namedGridColumnLines() : style().namedGridRowLines();

    String namedGridAreaOrGridLine = position.namedGridLine();
    bool isStartSide = side == ColumnStartSide || side == RowStartSide;

    auto areaIterator = gridAreaMap.find(namedGridAreaOrGridLine);
    if (areaIterator != gridAreaMap.end()) {
        String gridLineName = namedGridAreaOrGridLine + (isStartSide ? "-start" : "-end");
        setNamedLinePositionIfDefinedBeforeArea(position, namedLinesMap, gridLineName, areaIterator->value, side);
        return;
    }

    bool hasStartSuffix = namedGridAreaOrGridLine.endsWith("-start");
    bool hasEndSuffix = namedGridAreaOrGridLine.endsWith("-end");
    if ((hasStartSuffix && isStartSide) || (hasEndSuffix && !isStartSide)) {
        size_t suffixLength = hasStartSuffix ? strlen("-start") : strlen("-end");
        String gridAreaName = namedGridAreaOrGridLine.substring(0, namedGridAreaOrGridLine.length() - suffixLength);

        auto areaIterator = gridAreaMap.find(gridAreaName);
        if (areaIterator != gridAreaMap.end()) {
            position.setNamedGridArea(gridAreaName);
            setNamedLinePositionIfDefinedBeforeArea(position, namedLinesMap, namedGridAreaOrGridLine, areaIterator->value, side);
            return;
        }
    }

    if (namedLinesMap.contains(namedGridAreaOrGridLine))
        position.setExplicitPosition(1, namedGridAreaOrGridLine);
    else
        position.setAutoPosition();
}

void RenderGrid::adjustGridPositionsFromStyle(GridPosition& initialPosition, GridPosition& finalPosition, GridPositionSide initialPositionSide, GridPositionSide finalPositionSide) const
{
    ASSERT(isColumnSide(initialPositionSide) == isColumnSide(finalPositionSide));

    // We must handle the placement error handling code here instead of in the StyleAdjuster because we don't want to
    // overwrite the specified values.
    if (initialPosition.isSpan() && finalPosition.isSpan())
        finalPosition.setAutoPosition();

    if (initialPosition.isNamedGridArea())
        adjustNamedGridItemPosition(initialPosition, initialPositionSide);

    if (finalPosition.isNamedGridArea())
        adjustNamedGridItemPosition(finalPosition, finalPositionSide);
}

std::unique_ptr<GridSpan> RenderGrid::resolveGridPositionsFromStyle(const RenderBox* gridItem, GridTrackSizingDirection direction) const
{
    GridPosition initialPosition = (direction == ForColumns) ? gridItem->style().gridItemColumnStart() : gridItem->style().gridItemRowStart();
    const GridPositionSide initialPositionSide = (direction == ForColumns) ? ColumnStartSide : RowStartSide;
    GridPosition finalPosition = (direction == ForColumns) ? gridItem->style().gridItemColumnEnd() : gridItem->style().gridItemRowEnd();
    const GridPositionSide finalPositionSide = (direction == ForColumns) ? ColumnEndSide : RowEndSide;

    adjustGridPositionsFromStyle(initialPosition, finalPosition, initialPositionSide, finalPositionSide);

    if (initialPosition.shouldBeResolvedAgainstOppositePosition() && finalPosition.shouldBeResolvedAgainstOppositePosition()) {
        if (style().gridAutoFlow() == AutoFlowNone)
            return std::make_unique<GridSpan>(0, 0);

        // We can't get our grid positions without running the auto placement algorithm.
        return nullptr;
    }

    if (initialPosition.shouldBeResolvedAgainstOppositePosition()) {
        // Infer the position from the final position ('auto / 1' or 'span 2 / 3' case).
        const size_t finalResolvedPosition = resolveGridPositionFromStyle(finalPosition, finalPositionSide);
        return resolveGridPositionAgainstOppositePosition(finalResolvedPosition, initialPosition, initialPositionSide);
    }

    if (finalPosition.shouldBeResolvedAgainstOppositePosition()) {
        // Infer our position from the initial position ('1 / auto' or '3 / span 2' case).
        const size_t initialResolvedPosition = resolveGridPositionFromStyle(initialPosition, initialPositionSide);
        return resolveGridPositionAgainstOppositePosition(initialResolvedPosition, finalPosition, finalPositionSide);
    }

    size_t resolvedInitialPosition = resolveGridPositionFromStyle(initialPosition, initialPositionSide);
    size_t resolvedFinalPosition = resolveGridPositionFromStyle(finalPosition, finalPositionSide);

    // If 'grid-row-end' specifies a line at or before that specified by 'grid-row-start', it computes to 'span 1'.
    if (resolvedFinalPosition < resolvedInitialPosition)
        resolvedFinalPosition = resolvedInitialPosition;

    return std::make_unique<GridSpan>(resolvedInitialPosition, resolvedFinalPosition);
}

size_t RenderGrid::resolveNamedGridLinePositionFromStyle(const GridPosition& position, GridPositionSide side) const
{
    ASSERT(!position.namedGridLine().isNull());

    const NamedGridLinesMap& gridLinesNames = isColumnSide(side) ? style().namedGridColumnLines() : style().namedGridRowLines();
    NamedGridLinesMap::const_iterator it = gridLinesNames.find(position.namedGridLine());
    if (it == gridLinesNames.end()) {
        if (position.isPositive())
            return 0;
        const size_t lastLine = explicitGridSizeForSide(side);
        return GridPosition::adjustGridPositionForSide(lastLine, side);
    }

    size_t namedGridLineIndex;
    if (position.isPositive())
        namedGridLineIndex = std::min<size_t>(position.integerPosition(), it->value.size()) - 1;
    else
        namedGridLineIndex = std::max<int>(it->value.size() - abs(position.integerPosition()), 0);
    return GridPosition::adjustGridPositionForSide(it->value[namedGridLineIndex], side);
}

size_t RenderGrid::resolveGridPositionFromStyle(const GridPosition& position, GridPositionSide side) const
{
    switch (position.type()) {
    case ExplicitPosition: {
        ASSERT(position.integerPosition());

        if (!position.namedGridLine().isNull())
            return resolveNamedGridLinePositionFromStyle(position, side);

        // Handle <integer> explicit position.
        if (position.isPositive())
            return GridPosition::adjustGridPositionForSide(position.integerPosition() - 1, side);

        size_t resolvedPosition = abs(position.integerPosition()) - 1;
        const size_t endOfTrack = explicitGridSizeForSide(side);

        // Per http://lists.w3.org/Archives/Public/www-style/2013Mar/0589.html, we clamp negative value to the first line.
        if (endOfTrack < resolvedPosition)
            return 0;

        return GridPosition::adjustGridPositionForSide(endOfTrack - resolvedPosition, side);
    }
    case NamedGridAreaPosition:
    {
        NamedGridAreaMap::const_iterator it = style().namedGridArea().find(position.namedGridLine());
        // Unknown grid area should have been computed to 'auto' by now.
        ASSERT(it != style().namedGridArea().end());
        const GridCoordinate& gridAreaCoordinate = it->value;
        switch (side) {
        case ColumnStartSide:
            return gridAreaCoordinate.columns.initialPositionIndex;
        case ColumnEndSide:
            return gridAreaCoordinate.columns.finalPositionIndex;
        case RowStartSide:
            return gridAreaCoordinate.rows.initialPositionIndex;
        case RowEndSide:
            return gridAreaCoordinate.rows.finalPositionIndex;
        }
        ASSERT_NOT_REACHED();
        return 0;
    }
    case AutoPosition:
    case SpanPosition:
        // 'auto' and span depend on the opposite position for resolution (e.g. grid-row: auto / 1 or grid-column: span 3 / "myHeader").
        ASSERT_NOT_REACHED();
        return 0;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

std::unique_ptr<GridSpan> RenderGrid::resolveGridPositionAgainstOppositePosition(size_t resolvedOppositePosition, const GridPosition& position, GridPositionSide side) const
{
    if (position.isAuto())
        return std::make_unique<GridSpan>(resolvedOppositePosition, resolvedOppositePosition);

    ASSERT(position.isSpan());
    ASSERT(position.spanPosition() > 0);

    if (!position.namedGridLine().isNull()) {
        // span 2 'c' -> we need to find the appropriate grid line before / after our opposite position.
        return resolveNamedGridLinePositionAgainstOppositePosition(resolvedOppositePosition, position, side);
    }

    // 'span 1' is contained inside a single grid track regardless of the direction.
    // That's why the CSS span value is one more than the offset we apply.
    size_t positionOffset = position.spanPosition() - 1;
    if (side == ColumnStartSide || side == RowStartSide) {
        size_t initialResolvedPosition = std::max<int>(0, resolvedOppositePosition - positionOffset);
        return std::make_unique<GridSpan>(initialResolvedPosition, resolvedOppositePosition);
    }

    return std::make_unique<GridSpan>(resolvedOppositePosition, resolvedOppositePosition + positionOffset);
}

std::unique_ptr<GridSpan> RenderGrid::resolveNamedGridLinePositionAgainstOppositePosition(size_t resolvedOppositePosition, const GridPosition& position, GridPositionSide side) const
{
    ASSERT(position.isSpan());
    ASSERT(!position.namedGridLine().isNull());
    // Negative positions are not allowed per the specification and should have been handled during parsing.
    ASSERT(position.spanPosition() > 0);

    const NamedGridLinesMap& gridLinesNames = isColumnSide(side) ? style().namedGridColumnLines() : style().namedGridRowLines();
    NamedGridLinesMap::const_iterator it = gridLinesNames.find(position.namedGridLine());

    // If there is no named grid line of that name, we resolve the position to 'auto' (which is equivalent to 'span 1' in this case).
    // See http://lists.w3.org/Archives/Public/www-style/2013Jun/0394.html.
    if (it == gridLinesNames.end())
        return std::make_unique<GridSpan>(resolvedOppositePosition, resolvedOppositePosition);

    if (side == RowStartSide || side == ColumnStartSide)
        return resolveRowStartColumnStartNamedGridLinePositionAgainstOppositePosition(resolvedOppositePosition, position, it->value);

    return resolveRowEndColumnEndNamedGridLinePositionAgainstOppositePosition(resolvedOppositePosition, position, it->value);
}

static inline size_t firstNamedGridLineBeforePosition(size_t position, const Vector<size_t>& gridLines)
{
    // The grid line inequality needs to be strict (which doesn't match the after / end case) because |position| is
    // already converted to an index in our grid representation (ie one was removed from the grid line to account for
    // the side).
    size_t firstLineBeforePositionIndex = 0;
    const size_t* firstLineBeforePosition = std::lower_bound(gridLines.begin(), gridLines.end(), position);
    if (firstLineBeforePosition != gridLines.end()) {
        if (*firstLineBeforePosition > position && firstLineBeforePosition != gridLines.begin())
            --firstLineBeforePosition;

        firstLineBeforePositionIndex = firstLineBeforePosition - gridLines.begin();
    }
    return firstLineBeforePositionIndex;
}

std::unique_ptr<GridSpan> RenderGrid::resolveRowStartColumnStartNamedGridLinePositionAgainstOppositePosition(size_t resolvedOppositePosition, const GridPosition& position, const Vector<size_t>& gridLines) const
{
    size_t gridLineIndex = std::max<int>(0, firstNamedGridLineBeforePosition(resolvedOppositePosition, gridLines) - position.spanPosition() + 1);
    size_t resolvedGridLinePosition = gridLines[gridLineIndex];
    if (resolvedGridLinePosition > resolvedOppositePosition)
        resolvedGridLinePosition = resolvedOppositePosition;
    return std::make_unique<GridSpan>(resolvedGridLinePosition, resolvedOppositePosition);
}

std::unique_ptr<GridSpan> RenderGrid::resolveRowEndColumnEndNamedGridLinePositionAgainstOppositePosition(size_t resolvedOppositePosition, const GridPosition& position, const Vector<size_t>& gridLines) const
{
    size_t firstLineAfterOppositePositionIndex = gridLines.size() - 1;
    const size_t* firstLineAfterOppositePosition = std::upper_bound(gridLines.begin(), gridLines.end(), resolvedOppositePosition);
    if (firstLineAfterOppositePosition != gridLines.end())
        firstLineAfterOppositePositionIndex = firstLineAfterOppositePosition - gridLines.begin();

    size_t gridLineIndex = std::min(gridLines.size() - 1, firstLineAfterOppositePositionIndex + position.spanPosition() - 1);
    size_t resolvedGridLinePosition = GridPosition::adjustGridPositionForRowEndColumnEndSide(gridLines[gridLineIndex]);
    if (resolvedGridLinePosition < resolvedOppositePosition)
        resolvedGridLinePosition = resolvedOppositePosition;
    return std::make_unique<GridSpan>(resolvedOppositePosition, resolvedGridLinePosition);
}

LayoutUnit RenderGrid::gridAreaBreadthForChild(const RenderBox* child, GridTrackSizingDirection direction, const Vector<GridTrack>& tracks) const
{
    const GridCoordinate& coordinate = cachedGridCoordinate(child);
    const GridSpan& span = (direction == ForColumns) ? coordinate.columns : coordinate.rows;
    LayoutUnit gridAreaBreadth = 0;
    for (size_t trackIndex = span.initialPositionIndex; trackIndex <= span.finalPositionIndex; ++trackIndex)
        gridAreaBreadth += tracks[trackIndex].m_usedBreadth;
    return gridAreaBreadth;
}

void RenderGrid::populateGridPositions(const GridSizingData& sizingData)
{
    m_columnPositions.resizeToFit(sizingData.columnTracks.size() + 1);
    m_columnPositions[0] = borderAndPaddingStart();
    for (size_t i = 0; i < m_columnPositions.size() - 1; ++i)
        m_columnPositions[i + 1] = m_columnPositions[i] + sizingData.columnTracks[i].m_usedBreadth;

    m_rowPositions.resizeToFit(sizingData.rowTracks.size() + 1);
    m_rowPositions[0] = borderAndPaddingBefore();
    for (size_t i = 0; i < m_rowPositions.size() - 1; ++i)
        m_rowPositions[i + 1] = m_rowPositions[i] + sizingData.rowTracks[i].m_usedBreadth;
}

LayoutPoint RenderGrid::findChildLogicalPosition(RenderBox* child, const GridSizingData& sizingData)
{
    const GridCoordinate& coordinate = cachedGridCoordinate(child);
    ASSERT_UNUSED(sizingData, coordinate.columns.initialPositionIndex < sizingData.columnTracks.size());
    ASSERT_UNUSED(sizingData, coordinate.rows.initialPositionIndex < sizingData.rowTracks.size());

    // The grid items should be inside the grid container's border box, that's why they need to be shifted.
    return LayoutPoint(m_columnPositions[coordinate.columns.initialPositionIndex] + marginStartForChild(*child), m_rowPositions[coordinate.rows.initialPositionIndex] + marginBeforeForChild(*child));
}

void RenderGrid::paintChildren(PaintInfo& paintInfo, const LayoutPoint& paintOffset, PaintInfo& forChild, bool usePrintRect)
{
    for (RenderBox* child = m_orderIterator.first(); child; child = m_orderIterator.next())
        paintChild(*child, paintInfo, paintOffset, forChild, usePrintRect);
}

const char* RenderGrid::renderName() const
{
    if (isFloating())
        return "RenderGrid (floating)";
    if (isOutOfFlowPositioned())
        return "RenderGrid (positioned)";
    if (isAnonymous())
        return "RenderGrid (generated)";
    if (isRelPositioned())
        return "RenderGrid (relative positioned)";
    return "RenderGrid";
}

} // namespace WebCore

#endif /* ENABLE(CSS_GRID_LAYOUT) */
