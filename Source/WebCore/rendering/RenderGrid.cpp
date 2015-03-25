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
#include "GridResolvedPosition.h"
#include "LayoutRepainter.h"
#include "RenderLayer.h"
#include "RenderView.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

static const int infinity = -1;

class GridTrack {
public:
    GridTrack() {}

    const LayoutUnit& baseSize() const
    {
        ASSERT(isGrowthLimitBiggerThanBaseSize());
        return m_baseSize;
    }

    const LayoutUnit& growthLimit() const
    {
        ASSERT(isGrowthLimitBiggerThanBaseSize());
        return m_growthLimit;
    }

    void setBaseSize(LayoutUnit baseSize)
    {
        m_baseSize = baseSize;
        ensureGrowthLimitIsBiggerThanBaseSize();
    }

    void setGrowthLimit(LayoutUnit growthLimit)
    {
        m_growthLimit = growthLimit;
        ensureGrowthLimitIsBiggerThanBaseSize();
    }

    void growBaseSize(LayoutUnit growth)
    {
        ASSERT(growth >= 0);
        m_baseSize += growth;
        ensureGrowthLimitIsBiggerThanBaseSize();
    }

    void growGrowthLimit(LayoutUnit growth)
    {
        ASSERT(growth >= 0);
        if (m_growthLimit == infinity)
            m_growthLimit = m_baseSize + growth;
        else
            m_growthLimit += growth;

        ASSERT(m_growthLimit >= m_baseSize);
    }

    bool growthLimitIsInfinite() const
    {
        return m_growthLimit == infinity;
    }

    const LayoutUnit& growthLimitIfNotInfinite() const
    {
        ASSERT(isGrowthLimitBiggerThanBaseSize());
        return (m_growthLimit == infinity) ? m_baseSize : m_growthLimit;
    }

private:
    bool isGrowthLimitBiggerThanBaseSize() const { return growthLimitIsInfinite() || m_growthLimit >= m_baseSize; }

    void ensureGrowthLimitIsBiggerThanBaseSize()
    {
        if (m_growthLimit != infinity && m_growthLimit < m_baseSize)
            m_growthLimit = m_baseSize;
    }

    LayoutUnit m_baseSize { 0 };
    LayoutUnit m_growthLimit { 0 };
};

struct GridTrackForNormalization {
    GridTrackForNormalization(const GridTrack& track, double flex)
        : m_track(&track)
        , m_flex(flex)
        , m_normalizedFlexValue(track.baseSize() / flex)
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
    GridIterator(const Vector<Vector<Vector<RenderBox*, 1>>>& grid, GridTrackSizingDirection direction, unsigned fixedTrackIndex, unsigned varyingTrackIndex = 0)
        : m_grid(grid)
        , m_direction(direction)
        , m_rowIndex((direction == ForColumns) ? varyingTrackIndex : fixedTrackIndex)
        , m_columnIndex((direction == ForColumns) ? fixedTrackIndex : varyingTrackIndex)
        , m_childIndex(0)
    {
        ASSERT(m_rowIndex < m_grid.size());
        ASSERT(m_columnIndex < m_grid[0].size());
    }

    RenderBox* nextGridItem()
    {
        if (!m_grid.size())
            return 0;

        unsigned& varyingTrackIndex = (m_direction == ForColumns) ? m_rowIndex : m_columnIndex;
        const unsigned endOfVaryingTrackIndex = (m_direction == ForColumns) ? m_grid.size() : m_grid[0].size();
        for (; varyingTrackIndex < endOfVaryingTrackIndex; ++varyingTrackIndex) {
            const Vector<RenderBox*>& children = m_grid[m_rowIndex][m_columnIndex];
            if (m_childIndex < children.size())
                return children[m_childIndex++];

            m_childIndex = 0;
        }
        return 0;
    }

    bool isEmptyAreaEnough(unsigned rowSpan, unsigned columnSpan) const
    {
        // Ignore cells outside current grid as we will grow it later if needed.
        unsigned maxRows = std::min<unsigned>(m_rowIndex + rowSpan, m_grid.size());
        unsigned maxColumns = std::min<unsigned>(m_columnIndex + columnSpan, m_grid[0].size());

        // This adds a O(N^2) behavior that shouldn't be a big deal as we expect spanning areas to be small.
        for (unsigned row = m_rowIndex; row < maxRows; ++row) {
            for (unsigned column = m_columnIndex; column < maxColumns; ++column) {
                const Vector<RenderBox*>& children = m_grid[row][column];
                if (!children.isEmpty())
                    return false;
            }
        }

        return true;
    }

    std::unique_ptr<GridCoordinate> nextEmptyGridArea(unsigned fixedTrackSpan, unsigned varyingTrackSpan)
    {
        ASSERT(fixedTrackSpan >= 1 && varyingTrackSpan >= 1);

        if (m_grid.isEmpty())
            return nullptr;

        unsigned rowSpan = (m_direction == ForColumns) ? varyingTrackSpan : fixedTrackSpan;
        unsigned columnSpan = (m_direction == ForColumns) ? fixedTrackSpan : varyingTrackSpan;

        unsigned& varyingTrackIndex = (m_direction == ForColumns) ? m_rowIndex : m_columnIndex;
        const unsigned endOfVaryingTrackIndex = (m_direction == ForColumns) ? m_grid.size() : m_grid[0].size();
        for (; varyingTrackIndex < endOfVaryingTrackIndex; ++varyingTrackIndex) {
            if (isEmptyAreaEnough(rowSpan, columnSpan)) {
                std::unique_ptr<GridCoordinate> result = std::make_unique<GridCoordinate>(GridSpan(m_rowIndex, m_rowIndex + rowSpan - 1), GridSpan(m_columnIndex, m_columnIndex + columnSpan - 1));
                // Advance the iterator to avoid an infinite loop where we would return the same grid area over and over.
                ++varyingTrackIndex;
                return result;
            }
        }
        return nullptr;
    }

private:
    const Vector<Vector<Vector<RenderBox*, 1>>>& m_grid;
    GridTrackSizingDirection m_direction;
    unsigned m_rowIndex;
    unsigned m_columnIndex;
    unsigned m_childIndex;
};

class RenderGrid::GridSizingData {
    WTF_MAKE_NONCOPYABLE(GridSizingData);
public:
    GridSizingData(unsigned gridColumnCount, unsigned gridRowCount)
        : columnTracks(gridColumnCount)
        , rowTracks(gridRowCount)
    {
    }

    Vector<GridTrack> columnTracks;
    Vector<GridTrack> rowTracks;
    Vector<unsigned> contentSizedTracksIndex;

    // Performance optimization: hold onto these Vectors until the end of Layout to avoid repeated malloc / free.
    Vector<LayoutUnit> distributeTrackVector;
    Vector<GridTrack*> filteredTracks;
    Vector<unsigned> growAboveMaxBreadthTrackIndexes;
    Vector<GridItemWithSpan> itemsSortedByIncreasingSpan;
};

RenderGrid::RenderGrid(Element& element, Ref<RenderStyle>&& style)
    : RenderBlock(element, WTF::move(style), 0)
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
    bool wasPopulated = gridWasPopulated();
    if (!wasPopulated)
        const_cast<RenderGrid*>(this)->placeItemsOnGrid();

    GridSizingData sizingData(gridColumnCount(), gridRowCount());
    LayoutUnit availableLogicalSpace = 0;
    const_cast<RenderGrid*>(this)->computeUsedBreadthOfGridTracks(ForColumns, sizingData, availableLogicalSpace);

    for (auto& column : sizingData.columnTracks) {
        LayoutUnit minTrackBreadth = column.baseSize();
        LayoutUnit maxTrackBreadth = column.growthLimit();

        minLogicalWidth += minTrackBreadth;
        maxLogicalWidth += maxTrackBreadth;

        // FIXME: This should add in the scrollbarWidth (e.g. see RenderFlexibleBox).
    }

    if (!wasPopulated)
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
    const LayoutUnit initialAvailableLogicalSpace = availableLogicalSpace;
    Vector<GridTrack>& tracks = (direction == ForColumns) ? sizingData.columnTracks : sizingData.rowTracks;
    Vector<unsigned> flexibleSizedTracksIndex;
    sizingData.contentSizedTracksIndex.shrink(0);

    // 1. Initialize per Grid track variables.
    for (unsigned i = 0; i < tracks.size(); ++i) {
        GridTrack& track = tracks[i];
        const GridTrackSize& trackSize = gridTrackSize(direction, i);
        const GridLength& minTrackBreadth = trackSize.minTrackBreadth();
        const GridLength& maxTrackBreadth = trackSize.maxTrackBreadth();

        track.setBaseSize(computeUsedBreadthOfMinLength(direction, minTrackBreadth));
        track.setGrowthLimit(computeUsedBreadthOfMaxLength(direction, maxTrackBreadth, track.baseSize()));

        if (trackSize.isContentSized())
            sizingData.contentSizedTracksIndex.append(i);
        if (trackSize.maxTrackBreadth().isFlex())
            flexibleSizedTracksIndex.append(i);
    }

    // 2. Resolve content-based TrackSizingFunctions.
    if (!sizingData.contentSizedTracksIndex.isEmpty())
        resolveContentBasedTrackSizingFunctions(direction, sizingData);

    for (auto& track : tracks) {
        ASSERT(!track.growthLimitIsInfinite());
        availableLogicalSpace -= track.baseSize();
    }

    const bool hasUndefinedRemainingSpace = (direction == ForRows) ? style().logicalHeight().isAuto() : gridElementIsShrinkToFit();

    if (!hasUndefinedRemainingSpace && availableLogicalSpace <= 0)
        return;

    // 3. Grow all Grid tracks in GridTracks from their UsedBreadth up to their MaxBreadth value until availableLogicalSpace is exhausted.
    if (!hasUndefinedRemainingSpace) {
        const unsigned tracksSize = tracks.size();
        Vector<GridTrack*> tracksForDistribution(tracksSize);
        for (unsigned i = 0; i < tracksSize; ++i)
            tracksForDistribution[i] = tracks.data() + i;

        distributeSpaceToTracks(tracksForDistribution, nullptr, &GridTrack::baseSize, &GridTrack::growBaseSize, sizingData, availableLogicalSpace);
    } else {
        for (auto& track : tracks)
            track.setBaseSize(track.growthLimit());
    }

    if (flexibleSizedTracksIndex.isEmpty())
        return;

    // 4. Grow all Grid tracks having a fraction as the MaxTrackSizingFunction.
    double normalizedFractionBreadth = 0;
    if (!hasUndefinedRemainingSpace)
        normalizedFractionBreadth = computeNormalizedFractionBreadth(tracks, GridSpan(0, tracks.size() - 1), direction, initialAvailableLogicalSpace);
    else {
        for (auto trackIndex : flexibleSizedTracksIndex) {
            const GridTrackSize& trackSize = gridTrackSize(direction, trackIndex);
            normalizedFractionBreadth = std::max(normalizedFractionBreadth, tracks[trackIndex].baseSize() / trackSize.maxTrackBreadth().flex());
        }

        for (unsigned i = 0; i < flexibleSizedTracksIndex.size(); ++i) {
            GridIterator iterator(m_grid, direction, flexibleSizedTracksIndex[i]);
            while (RenderBox* gridItem = iterator.nextGridItem()) {
                const GridCoordinate coordinate = cachedGridCoordinate(*gridItem);
                const GridSpan span = (direction == ForColumns) ? coordinate.columns : coordinate.rows;

                // Do not include already processed items.
                if (i > 0 && span.resolvedInitialPosition.toInt() <= flexibleSizedTracksIndex[i - 1])
                    continue;

                double itemNormalizedFlexBreadth = computeNormalizedFractionBreadth(tracks, span, direction, maxContentForChild(*gridItem, direction, sizingData.columnTracks));
                normalizedFractionBreadth = std::max(normalizedFractionBreadth, itemNormalizedFlexBreadth);
            }
        }
    }

    for (auto trackIndex : flexibleSizedTracksIndex) {
        const GridTrackSize& trackSize = gridTrackSize(direction, trackIndex);
        GridTrack& track = tracks[trackIndex];
        LayoutUnit baseSize = std::max<LayoutUnit>(track.baseSize(), normalizedFractionBreadth * trackSize.maxTrackBreadth().flex());
        track.setBaseSize(baseSize);
        availableLogicalSpace -= baseSize;
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

double RenderGrid::computeNormalizedFractionBreadth(Vector<GridTrack>& tracks, const GridSpan& tracksSpan, GridTrackSizingDirection direction, LayoutUnit spaceToFill) const
{
    LayoutUnit allocatedSpace;
    Vector<GridTrackForNormalization> tracksForNormalization;
    for (auto& position : tracksSpan) {
        GridTrack& track = tracks[position.toInt()];
        allocatedSpace += track.baseSize();

        const GridTrackSize& trackSize = gridTrackSize(direction, position.toInt());
        if (!trackSize.maxTrackBreadth().isFlex())
            continue;

        tracksForNormalization.append(GridTrackForNormalization(track, trackSize.maxTrackBreadth().flex()));
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
    LayoutUnit availableLogicalSpaceIgnoringFractionTracks = spaceToFill - allocatedSpace;

    for (auto& track : tracksForNormalization) {
        if (track.m_normalizedFlexValue > fractionValueBasedOnGridItemsRatio) {
            // If the normalized flex value (we ordered |tracksForNormalization| by increasing normalized flex value)
            // will make us overflow our container, then stop. We have the previous step's ratio is the best fit.
            if (track.m_normalizedFlexValue * accumulatedFractions > availableLogicalSpaceIgnoringFractionTracks)
                break;

            fractionValueBasedOnGridItemsRatio = track.m_normalizedFlexValue;
        }

        accumulatedFractions += track.m_flex;
        // This item was processed so we re-add its used breadth to the available space to accurately count the remaining space.
        availableLogicalSpaceIgnoringFractionTracks += track.m_track->baseSize();
    }

    return availableLogicalSpaceIgnoringFractionTracks / accumulatedFractions;
}

GridTrackSize RenderGrid::gridTrackSize(GridTrackSizingDirection direction, unsigned i) const
{
    bool isForColumns = (direction == ForColumns);
    auto& trackStyles =  isForColumns ? style().gridColumns() : style().gridRows();
    auto& trackSize = (i >= trackStyles.size()) ? (isForColumns ? style().gridAutoColumns() : style().gridAutoRows()) : trackStyles[i];

    // If the logical width/height of the grid container is indefinite, percentage values are treated as <auto> (or in
    // the case of minmax() as min-content for the first position and max-content for the second).
    Length logicalSize = isForColumns ? style().logicalWidth() : style().logicalHeight();
    if (logicalSize.isIntrinsicOrAuto()) {
        const GridLength& oldMinTrackBreadth = trackSize.minTrackBreadth();
        const GridLength& oldMaxTrackBreadth = trackSize.maxTrackBreadth();
        return GridTrackSize(oldMinTrackBreadth.isPercentage() ? Length(MinContent) : oldMinTrackBreadth, oldMaxTrackBreadth.isPercentage() ? Length(MaxContent) : oldMaxTrackBreadth);
    }

    return trackSize;
}

LayoutUnit RenderGrid::logicalContentHeightForChild(RenderBox& child, Vector<GridTrack>& columnTracks)
{
    LayoutUnit oldOverrideContainingBlockContentLogicalWidth = child.hasOverrideContainingBlockLogicalWidth() ? child.overrideContainingBlockContentLogicalWidth() : LayoutUnit();
    LayoutUnit overrideContainingBlockContentLogicalWidth = gridAreaBreadthForChild(child, ForColumns, columnTracks);
    if (child.style().logicalHeight().isPercent() || oldOverrideContainingBlockContentLogicalWidth != overrideContainingBlockContentLogicalWidth)
        child.setNeedsLayout(MarkOnlyThis);

    child.setOverrideContainingBlockContentLogicalWidth(overrideContainingBlockContentLogicalWidth);
    // If |child| has a percentage logical height, we shouldn't let it override its intrinsic height, which is
    // what we are interested in here. Thus we need to set the override logical height to -1 (no possible resolution).
    child.setOverrideContainingBlockContentLogicalHeight(-1);
    child.layoutIfNeeded();
    return child.logicalHeight() + child.marginLogicalHeight();
}

LayoutUnit RenderGrid::minContentForChild(RenderBox& child, GridTrackSizingDirection direction, Vector<GridTrack>& columnTracks)
{
    bool hasOrthogonalWritingMode = child.isHorizontalWritingMode() != isHorizontalWritingMode();
    // FIXME: Properly support orthogonal writing mode.
    if (hasOrthogonalWritingMode)
        return 0;

    if (direction == ForColumns) {
        // FIXME: It's unclear if we should return the intrinsic width or the preferred width.
        // See http://lists.w3.org/Archives/Public/www-style/2013Jan/0245.html
        return child.minPreferredLogicalWidth() + marginIntrinsicLogicalWidthForChild(child);
    }

    return logicalContentHeightForChild(child, columnTracks);
}

LayoutUnit RenderGrid::maxContentForChild(RenderBox& child, GridTrackSizingDirection direction, Vector<GridTrack>& columnTracks)
{
    bool hasOrthogonalWritingMode = child.isHorizontalWritingMode() != isHorizontalWritingMode();
    // FIXME: Properly support orthogonal writing mode.
    if (hasOrthogonalWritingMode)
        return LayoutUnit();

    if (direction == ForColumns) {
        // FIXME: It's unclear if we should return the intrinsic width or the preferred width.
        // See http://lists.w3.org/Archives/Public/www-style/2013Jan/0245.html
        return child.maxPreferredLogicalWidth() + marginIntrinsicLogicalWidthForChild(child);
    }

    return logicalContentHeightForChild(child, columnTracks);
}

class GridItemWithSpan {
public:
    GridItemWithSpan(RenderBox& gridItem, GridCoordinate coordinate, GridTrackSizingDirection direction)
        : m_gridItem(gridItem)
        , m_coordinate(coordinate)
    {
        const GridSpan& span = (direction == ForRows) ? coordinate.rows : coordinate.columns;
        m_span = span.resolvedFinalPosition.toInt() - span.resolvedInitialPosition.toInt() + 1;
    }

    RenderBox& gridItem() const { return m_gridItem; }
    GridCoordinate coordinate() const { return m_coordinate; }
#if !ASSERT_DISABLED
    size_t span() const { return m_span; }
#endif

    bool operator<(const GridItemWithSpan other) const
    {
        return m_span < other.m_span;
    }

private:
    std::reference_wrapper<RenderBox> m_gridItem;
    GridCoordinate m_coordinate;
    unsigned m_span;
};

bool RenderGrid::spanningItemCrossesFlexibleSizedTracks(const GridCoordinate& coordinate, GridTrackSizingDirection direction) const
{
    const GridSpan itemSpan = (direction == ForColumns) ? coordinate.columns : coordinate.rows;
    for (auto trackPosition : itemSpan) {
        const GridTrackSize& trackSize = gridTrackSize(direction, trackPosition.toInt());
        if (trackSize.minTrackBreadth().isFlex() || trackSize.maxTrackBreadth().isFlex())
            return true;
    }

    return false;
}

static inline unsigned integerSpanForDirection(const GridCoordinate& coordinate, GridTrackSizingDirection direction)
{
    return (direction == ForRows) ? coordinate.rows.integerSpan() : coordinate.columns.integerSpan();
}

void RenderGrid::resolveContentBasedTrackSizingFunctions(GridTrackSizingDirection direction, GridSizingData& sizingData)
{
    sizingData.itemsSortedByIncreasingSpan.shrink(0);
    HashSet<RenderBox*> itemsSet;
    for (auto trackIndex : sizingData.contentSizedTracksIndex) {
        GridIterator iterator(m_grid, direction, trackIndex);
        GridTrack& track = (direction == ForColumns) ? sizingData.columnTracks[trackIndex] : sizingData.rowTracks[trackIndex];

        while (RenderBox* gridItem = iterator.nextGridItem()) {
            if (itemsSet.add(gridItem).isNewEntry) {
                const GridCoordinate& coordinate = cachedGridCoordinate(*gridItem);
                if (integerSpanForDirection(coordinate, direction) == 1)
                    resolveContentBasedTrackSizingFunctionsForNonSpanningItems(direction, coordinate, *gridItem, track, sizingData.columnTracks);
                else if (!spanningItemCrossesFlexibleSizedTracks(coordinate, direction))
                    sizingData.itemsSortedByIncreasingSpan.append(GridItemWithSpan(*gridItem, coordinate, direction));
            }
        }
    }
    std::sort(sizingData.itemsSortedByIncreasingSpan.begin(), sizingData.itemsSortedByIncreasingSpan.end());

    for (auto& itemWithSpan : sizingData.itemsSortedByIncreasingSpan) {
        resolveContentBasedTrackSizingFunctionsForItems(direction, sizingData, itemWithSpan, &GridTrackSize::hasMinOrMaxContentMinTrackBreadth, &RenderGrid::minContentForChild, &GridTrack::baseSize, &GridTrack::growBaseSize, &GridTrackSize::hasMinContentMinTrackBreadthAndMinOrMaxContentMaxTrackBreadth);
        resolveContentBasedTrackSizingFunctionsForItems(direction, sizingData, itemWithSpan, &GridTrackSize::hasMaxContentMinTrackBreadth, &RenderGrid::maxContentForChild, &GridTrack::baseSize, &GridTrack::growBaseSize, &GridTrackSize::hasMaxContentMinTrackBreadthAndMaxContentMaxTrackBreadth);
        resolveContentBasedTrackSizingFunctionsForItems(direction, sizingData, itemWithSpan, &GridTrackSize::hasMinOrMaxContentMaxTrackBreadth, &RenderGrid::minContentForChild, &GridTrack::growthLimitIfNotInfinite, &GridTrack::growGrowthLimit);
        resolveContentBasedTrackSizingFunctionsForItems(direction, sizingData, itemWithSpan, &GridTrackSize::hasMaxContentMaxTrackBreadth, &RenderGrid::maxContentForChild, &GridTrack::growthLimitIfNotInfinite, &GridTrack::growGrowthLimit);
    }

    for (auto trackIndex : sizingData.contentSizedTracksIndex) {
        GridTrack& track = (direction == ForColumns) ? sizingData.columnTracks[trackIndex] : sizingData.rowTracks[trackIndex];
        if (track.growthLimitIsInfinite())
            track.setGrowthLimit(track.baseSize());
    }
}

void RenderGrid::resolveContentBasedTrackSizingFunctionsForNonSpanningItems(GridTrackSizingDirection direction, const GridCoordinate& coordinate, RenderBox& gridItem, GridTrack& track, Vector<GridTrack>& columnTracks)
{
    const GridResolvedPosition trackPosition = (direction == ForColumns) ? coordinate.columns.resolvedInitialPosition : coordinate.rows.resolvedInitialPosition;
    GridTrackSize trackSize = gridTrackSize(direction, trackPosition.toInt());

    if (trackSize.hasMinContentMinTrackBreadth())
        track.setBaseSize(std::max(track.baseSize(), minContentForChild(gridItem, direction, columnTracks)));
    else if (trackSize.hasMaxContentMinTrackBreadth())
        track.setBaseSize(std::max(track.baseSize(), maxContentForChild(gridItem, direction, columnTracks)));

    if (trackSize.hasMinContentMaxTrackBreadth())
        track.setGrowthLimit(std::max(track.growthLimit(), minContentForChild(gridItem, direction, columnTracks)));
    else if (trackSize.hasMaxContentMaxTrackBreadth())
        track.setGrowthLimit(std::max(track.growthLimit(), maxContentForChild(gridItem, direction, columnTracks)));
}

void RenderGrid::resolveContentBasedTrackSizingFunctionsForItems(GridTrackSizingDirection direction, GridSizingData& sizingData, GridItemWithSpan& gridItemWithSpan, FilterFunction filterFunction, SizingFunction sizingFunction, AccumulatorGetter trackGetter, AccumulatorGrowFunction trackGrowthFunction, FilterFunction growAboveMaxBreadthFilterFunction)
{
    ASSERT(gridItemWithSpan.span() > 1);
    const GridCoordinate& coordinate = gridItemWithSpan.coordinate();
    const GridResolvedPosition initialTrackPosition = (direction == ForColumns) ? coordinate.columns.resolvedInitialPosition : coordinate.rows.resolvedInitialPosition;
    const GridResolvedPosition finalTrackPosition = (direction == ForColumns) ? coordinate.columns.resolvedFinalPosition : coordinate.rows.resolvedFinalPosition;

    sizingData.filteredTracks.shrink(0);
    sizingData.growAboveMaxBreadthTrackIndexes.shrink(0);
    for (GridResolvedPosition trackIndex = initialTrackPosition; trackIndex <= finalTrackPosition; ++trackIndex) {
        const GridTrackSize& trackSize = gridTrackSize(direction, trackIndex.toInt());
        if (!(trackSize.*filterFunction)())
            continue;

        GridTrack& track = (direction == ForColumns) ? sizingData.columnTracks[trackIndex.toInt()] : sizingData.rowTracks[trackIndex.toInt()];
        sizingData.filteredTracks.append(&track);

        if (growAboveMaxBreadthFilterFunction && (trackSize.*growAboveMaxBreadthFilterFunction)())
            sizingData.growAboveMaxBreadthTrackIndexes.append(sizingData.filteredTracks.size() - 1);
    }

    if (sizingData.filteredTracks.isEmpty())
        return;

    LayoutUnit additionalBreadthSpace = (this->*sizingFunction)(gridItemWithSpan.gridItem(), direction, sizingData.columnTracks);
    for (GridResolvedPosition trackPositionForSpace = initialTrackPosition; trackPositionForSpace <= finalTrackPosition; ++trackPositionForSpace) {
        GridTrack& track = (direction == ForColumns) ? sizingData.columnTracks[trackPositionForSpace.toInt()] : sizingData.rowTracks[trackPositionForSpace.toInt()];
        additionalBreadthSpace -= (track.*trackGetter)();
    }

    // Specs mandate to floor additionalBreadthSpace (extra-space in specs) to 0. Instead we directly avoid the function
    // call in those cases as it will be a noop in terms of track sizing.
    if (additionalBreadthSpace > 0)
        distributeSpaceToTracks(sizingData.filteredTracks, &sizingData.growAboveMaxBreadthTrackIndexes, trackGetter, trackGrowthFunction, sizingData, additionalBreadthSpace);
}

static bool sortByGridTrackGrowthPotential(const GridTrack* track1, const GridTrack* track2)
{
    // This check ensures that we respect the irreflexivity property of the strict weak ordering required by std::sort
    // (forall x: NOT x < x).
    if (track1->growthLimitIsInfinite() && track2->growthLimitIsInfinite())
        return false;

    if (track1->growthLimitIsInfinite() || track2->growthLimitIsInfinite())
        return track2->growthLimitIsInfinite();

    return (track1->growthLimit() - track1->baseSize()) < (track2->growthLimit() - track2->baseSize());
}

void RenderGrid::distributeSpaceToTracks(Vector<GridTrack*>& tracks, Vector<unsigned>* growAboveMaxBreadthTrackIndexes, AccumulatorGetter trackGetter, AccumulatorGrowFunction trackGrowthFunction, GridSizingData& sizingData, LayoutUnit& availableLogicalSpace)
{
    ASSERT(availableLogicalSpace > 0);
    std::sort(tracks.begin(), tracks.end(), sortByGridTrackGrowthPotential);

    unsigned tracksSize = tracks.size();
    sizingData.distributeTrackVector.resize(tracksSize);

    for (unsigned i = 0; i < tracksSize; ++i) {
        GridTrack& track = *tracks[i];
        const LayoutUnit& trackBreadth = (tracks[i]->*trackGetter)();
        bool infiniteGrowthPotential = track.growthLimitIsInfinite();
        LayoutUnit trackGrowthPotential = infiniteGrowthPotential ? track.growthLimit() : track.growthLimit() - trackBreadth;
        sizingData.distributeTrackVector[i] = trackBreadth;
        // Let's avoid computing availableLogicalSpaceShare as much as possible as it's a hot spot in performance tests.
        if (trackGrowthPotential > 0 || infiniteGrowthPotential) {
            LayoutUnit availableLogicalSpaceShare = availableLogicalSpace / (tracksSize - i);
            LayoutUnit growthShare = infiniteGrowthPotential ? availableLogicalSpaceShare : std::min(availableLogicalSpaceShare, trackGrowthPotential);
            ASSERT_WITH_MESSAGE(growthShare >= 0, "We should never shrink any grid track or else we can't guarantee we abide by our min-sizing function. We can still have 0 as growthShare if the amount of tracks greatly exceeds the availableLogicalSpace.");
            sizingData.distributeTrackVector[i] += growthShare;
            availableLogicalSpace -= growthShare;
        }
    }

    if (availableLogicalSpace > 0 && growAboveMaxBreadthTrackIndexes) {
        unsigned indexesSize = growAboveMaxBreadthTrackIndexes->size();
        unsigned tracksGrowingAboveMaxBreadthSize = indexesSize ? indexesSize : tracksSize;
        // If we have a non-null empty vector of track indexes to grow above max breadth means that we should grow all
        // affected tracks.
        for (unsigned i = 0; i < tracksGrowingAboveMaxBreadthSize; ++i) {
            LayoutUnit growthShare = availableLogicalSpace / (tracksGrowingAboveMaxBreadthSize - i);
            unsigned distributeTrackIndex = indexesSize ? growAboveMaxBreadthTrackIndexes->at(i) : i;
            sizingData.distributeTrackVector[distributeTrackIndex] += growthShare;
            availableLogicalSpace -= growthShare;
        }
    }

    for (unsigned i = 0; i < tracksSize; ++i) {
        LayoutUnit growth = sizingData.distributeTrackVector[i] - (tracks[i]->*trackGetter)();
        if (growth >= 0)
            (tracks[i]->*trackGrowthFunction)(growth);
    }
}

#ifndef NDEBUG
bool RenderGrid::tracksAreWiderThanMinTrackBreadth(GridTrackSizingDirection direction, const Vector<GridTrack>& tracks)
{
    for (unsigned i = 0; i < tracks.size(); ++i) {
        const GridTrackSize& trackSize = gridTrackSize(direction, i);
        const GridLength& minTrackBreadth = trackSize.minTrackBreadth();
        if (computeUsedBreadthOfMinLength(direction, minTrackBreadth) > tracks[i].baseSize())
            return false;
    }
    return true;
}
#endif

void RenderGrid::ensureGridSize(unsigned maximumRowIndex, unsigned maximumColumnIndex)
{
    const unsigned oldRowCount = gridRowCount();
    if (maximumRowIndex >= oldRowCount) {
        m_grid.grow(maximumRowIndex + 1);
        for (unsigned row = oldRowCount; row < gridRowCount(); ++row)
            m_grid[row].grow(gridColumnCount());
    }

    if (maximumColumnIndex >= gridColumnCount()) {
        for (unsigned row = 0; row < gridRowCount(); ++row)
            m_grid[row].grow(maximumColumnIndex + 1);
    }
}

void RenderGrid::insertItemIntoGrid(RenderBox& child, const GridCoordinate& coordinate)
{
    ensureGridSize(coordinate.rows.resolvedFinalPosition.toInt(), coordinate.columns.resolvedFinalPosition.toInt());

    for (auto& row : coordinate.rows) {
        for (auto& column : coordinate.columns)
            m_grid[row.toInt()][column.toInt()].append(&child);
    }
    m_gridItemCoordinate.set(&child, coordinate);
}

void RenderGrid::placeItemsOnGrid()
{
    ASSERT(!gridWasPopulated());
    ASSERT(m_gridItemCoordinate.isEmpty());

    populateExplicitGridAndOrderIterator();

    Vector<RenderBox*> autoMajorAxisAutoGridItems;
    Vector<RenderBox*> specifiedMajorAxisAutoGridItems;
    for (RenderBox* child = m_orderIterator.first(); child; child = m_orderIterator.next()) {
        // FIXME: We never re-resolve positions if the grid is grown during auto-placement which may lead auto / <integer>
        // positions to not match the author's intent. The specification is unclear on what should be done in this case.
        std::unique_ptr<GridSpan> rowPositions = GridResolvedPosition::resolveGridPositionsFromStyle(style(), *child, ForRows);
        std::unique_ptr<GridSpan> columnPositions = GridResolvedPosition::resolveGridPositionsFromStyle(style(), *child, ForColumns);
        if (!rowPositions || !columnPositions) {
            GridSpan* majorAxisPositions = (autoPlacementMajorAxisDirection() == ForColumns) ? columnPositions.get() : rowPositions.get();
            if (!majorAxisPositions)
                autoMajorAxisAutoGridItems.append(child);
            else
                specifiedMajorAxisAutoGridItems.append(child);
            continue;
        }
        insertItemIntoGrid(*child, GridCoordinate(*rowPositions, *columnPositions));
    }

    ASSERT(gridRowCount() >= GridResolvedPosition::explicitGridRowCount(style()));
    ASSERT(gridColumnCount() >= GridResolvedPosition::explicitGridColumnCount(style()));

    placeSpecifiedMajorAxisItemsOnGrid(specifiedMajorAxisAutoGridItems);
    placeAutoMajorAxisItemsOnGrid(autoMajorAxisAutoGridItems);
}

void RenderGrid::populateExplicitGridAndOrderIterator()
{
    OrderIteratorPopulator populator(m_orderIterator);
    unsigned maximumRowIndex = std::max<unsigned>(1, GridResolvedPosition::explicitGridRowCount(style()));
    unsigned maximumColumnIndex = std::max<unsigned>(1, GridResolvedPosition::explicitGridColumnCount(style()));

    for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        populator.collectChild(*child);

        // This function bypasses the cache (cachedGridCoordinate()) as it is used to build it.
        std::unique_ptr<GridSpan> rowPositions = GridResolvedPosition::resolveGridPositionsFromStyle(style(), *child, ForRows);
        std::unique_ptr<GridSpan> columnPositions = GridResolvedPosition::resolveGridPositionsFromStyle(style(), *child, ForColumns);

        // |positions| is 0 if we need to run the auto-placement algorithm.
        if (rowPositions)
            maximumRowIndex = std::max(maximumRowIndex, rowPositions->resolvedFinalPosition.next().toInt());
        else {
            // Grow the grid for items with a definite row span, getting the largest such span.
            GridSpan positions = GridResolvedPosition::resolveGridPositionsFromAutoPlacementPosition(style(), *child, ForRows, GridResolvedPosition(0));
            maximumRowIndex = std::max(maximumRowIndex, positions.resolvedFinalPosition.next().toInt());
        }

        if (columnPositions)
            maximumColumnIndex = std::max(maximumColumnIndex, columnPositions->resolvedFinalPosition.next().toInt());
        else {
            // Grow the grid for items with a definite column span, getting the largest such span.
            GridSpan positions = GridResolvedPosition::resolveGridPositionsFromAutoPlacementPosition(style(), *child, ForColumns, GridResolvedPosition(0));
            maximumColumnIndex = std::max(maximumColumnIndex, positions.resolvedFinalPosition.next().toInt());
        }
    }

    m_grid.grow(maximumRowIndex);
    for (auto& column : m_grid)
        column.grow(maximumColumnIndex);
}

std::unique_ptr<GridCoordinate> RenderGrid::createEmptyGridAreaAtSpecifiedPositionsOutsideGrid(const RenderBox& gridItem, GridTrackSizingDirection specifiedDirection, const GridSpan& specifiedPositions) const
{
    GridTrackSizingDirection crossDirection = specifiedDirection == ForColumns ? ForRows : ForColumns;
    const unsigned endOfCrossDirection = crossDirection == ForColumns ? gridColumnCount() : gridRowCount();
    GridSpan crossDirectionPositions = GridResolvedPosition::resolveGridPositionsFromAutoPlacementPosition(style(), gridItem, crossDirection, GridResolvedPosition(endOfCrossDirection));
    return std::make_unique<GridCoordinate>(specifiedDirection == ForColumns ? crossDirectionPositions : specifiedPositions, specifiedDirection == ForColumns ? specifiedPositions : crossDirectionPositions);
}

void RenderGrid::placeSpecifiedMajorAxisItemsOnGrid(const Vector<RenderBox*>& autoGridItems)
{
    for (auto& autoGridItem : autoGridItems) {
        std::unique_ptr<GridSpan> majorAxisPositions = GridResolvedPosition::resolveGridPositionsFromStyle(style(), *autoGridItem, autoPlacementMajorAxisDirection());
        GridSpan minorAxisPositions = GridResolvedPosition::resolveGridPositionsFromAutoPlacementPosition(style(), *autoGridItem, autoPlacementMinorAxisDirection(), GridResolvedPosition(0));

        GridIterator iterator(m_grid, autoPlacementMajorAxisDirection(), majorAxisPositions->resolvedInitialPosition.toInt());
        std::unique_ptr<GridCoordinate> emptyGridArea = iterator.nextEmptyGridArea(majorAxisPositions->integerSpan(), minorAxisPositions.integerSpan());
        if (!emptyGridArea)
            emptyGridArea = createEmptyGridAreaAtSpecifiedPositionsOutsideGrid(*autoGridItem, autoPlacementMajorAxisDirection(), *majorAxisPositions);
        insertItemIntoGrid(*autoGridItem, *emptyGridArea);
    }
}

void RenderGrid::placeAutoMajorAxisItemsOnGrid(const Vector<RenderBox*>& autoGridItems)
{
    AutoPlacementCursor autoPlacementCursor = {0, 0};
    bool isGridAutoFlowDense = style().isGridAutoFlowAlgorithmDense();

    for (auto& autoGridItem : autoGridItems) {
        placeAutoMajorAxisItemOnGrid(*autoGridItem, autoPlacementCursor);

        if (isGridAutoFlowDense) {
            autoPlacementCursor.first = 0;
            autoPlacementCursor.second = 0;
        }
    }
}

void RenderGrid::placeAutoMajorAxisItemOnGrid(RenderBox& gridItem, AutoPlacementCursor& autoPlacementCursor)
{
    std::unique_ptr<GridSpan> minorAxisPositions = GridResolvedPosition::resolveGridPositionsFromStyle(style(), gridItem, autoPlacementMinorAxisDirection());
    ASSERT(!GridResolvedPosition::resolveGridPositionsFromStyle(style(), gridItem, autoPlacementMajorAxisDirection()));
    GridSpan majorAxisPositions = GridResolvedPosition::resolveGridPositionsFromAutoPlacementPosition(style(), gridItem, autoPlacementMajorAxisDirection(), GridResolvedPosition(0));

    const unsigned endOfMajorAxis = (autoPlacementMajorAxisDirection() == ForColumns) ? gridColumnCount() : gridRowCount();
    unsigned majorAxisAutoPlacementCursor = autoPlacementMajorAxisDirection() == ForColumns ? autoPlacementCursor.second : autoPlacementCursor.first;
    unsigned minorAxisAutoPlacementCursor = autoPlacementMajorAxisDirection() == ForColumns ? autoPlacementCursor.first : autoPlacementCursor.second;

    std::unique_ptr<GridCoordinate> emptyGridArea;
    if (minorAxisPositions) {
        // Move to the next track in major axis if initial position in minor axis is before auto-placement cursor.
        if (minorAxisPositions->resolvedInitialPosition.toInt() < minorAxisAutoPlacementCursor)
            majorAxisAutoPlacementCursor++;

        if (majorAxisAutoPlacementCursor < endOfMajorAxis) {
            GridIterator iterator(m_grid, autoPlacementMinorAxisDirection(), minorAxisPositions->resolvedInitialPosition.toInt(), majorAxisAutoPlacementCursor);
            emptyGridArea = iterator.nextEmptyGridArea(minorAxisPositions->integerSpan(), majorAxisPositions.integerSpan());
        }

        if (!emptyGridArea)
            emptyGridArea = createEmptyGridAreaAtSpecifiedPositionsOutsideGrid(gridItem, autoPlacementMinorAxisDirection(), *minorAxisPositions);
    } else {
        GridSpan minorAxisPositions = GridResolvedPosition::resolveGridPositionsFromAutoPlacementPosition(style(), gridItem, autoPlacementMinorAxisDirection(), GridResolvedPosition(0));

        for (unsigned majorAxisIndex = majorAxisAutoPlacementCursor; majorAxisIndex < endOfMajorAxis; ++majorAxisIndex) {
            GridIterator iterator(m_grid, autoPlacementMajorAxisDirection(), majorAxisIndex, minorAxisAutoPlacementCursor);
            emptyGridArea = iterator.nextEmptyGridArea(majorAxisPositions.integerSpan(), minorAxisPositions.integerSpan());

            if (emptyGridArea) {
                // Check that it fits in the minor axis direction, as we shouldn't grow in that direction here (it was already managed in populateExplicitGridAndOrderIterator()).
                GridResolvedPosition minorAxisFinalPositionIndex = autoPlacementMinorAxisDirection() == ForColumns ? emptyGridArea->columns.resolvedFinalPosition : emptyGridArea->rows.resolvedFinalPosition;
                const unsigned endOfMinorAxis = autoPlacementMinorAxisDirection() == ForColumns ? gridColumnCount() : gridRowCount();
                if (minorAxisFinalPositionIndex.toInt() < endOfMinorAxis)
                    break;

                // Discard empty grid area as it does not fit in the minor axis direction.
                // We don't need to create a new empty grid area yet as we might find a valid one in the next iteration.
                emptyGridArea = nullptr;
            }

            // As we're moving to the next track in the major axis we should reset the auto-placement cursor in the minor axis.
            minorAxisAutoPlacementCursor = 0;
        }

        if (!emptyGridArea)
            emptyGridArea = createEmptyGridAreaAtSpecifiedPositionsOutsideGrid(gridItem, autoPlacementMinorAxisDirection(), minorAxisPositions);
    }

    insertItemIntoGrid(gridItem, *emptyGridArea);
    autoPlacementCursor.first = emptyGridArea->rows.resolvedInitialPosition.toInt();
    autoPlacementCursor.second = emptyGridArea->columns.resolvedInitialPosition.toInt();
}

GridTrackSizingDirection RenderGrid::autoPlacementMajorAxisDirection() const
{
    return style().isGridAutoFlowDirectionColumn() ? ForColumns : ForRows;
}

GridTrackSizingDirection RenderGrid::autoPlacementMinorAxisDirection() const
{
    return style().isGridAutoFlowDirectionColumn() ? ForRows : ForColumns;
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

        LayoutUnit overrideContainingBlockContentLogicalWidth = gridAreaBreadthForChild(*child, ForColumns, sizingData.columnTracks);
        LayoutUnit overrideContainingBlockContentLogicalHeight = gridAreaBreadthForChild(*child, ForRows, sizingData.rowTracks);
        if (oldOverrideContainingBlockContentLogicalWidth != overrideContainingBlockContentLogicalWidth || (oldOverrideContainingBlockContentLogicalHeight != overrideContainingBlockContentLogicalHeight && child->hasRelativeLogicalHeight()))
            child->setNeedsLayout(MarkOnlyThis);

        child->setOverrideContainingBlockContentLogicalWidth(overrideContainingBlockContentLogicalWidth);
        child->setOverrideContainingBlockContentLogicalHeight(overrideContainingBlockContentLogicalHeight);

        LayoutRect oldChildRect = child->frameRect();

        // FIXME: Grid items should stretch to fill their cells. Once we
        // implement grid-{column,row}-align, we can also shrink to fit. For
        // now, just size as if we were a regular child.
        child->layoutIfNeeded();

        child->setLogicalLocation(findChildLogicalPosition(*child, sizingData));

        // If the child moved, we have to repaint it as well as any floating/positioned
        // descendants. An exception is if we need a layout. In this case, we know we're going to
        // repaint ourselves (and the child) anyway.
        if (!selfNeedsLayout() && child->checkForRepaintDuringLayout())
            child->repaintDuringLayoutIfMoved(oldChildRect);
    }

    for (auto& row : sizingData.rowTracks)
        setLogicalHeight(logicalHeight() + row.baseSize());

    // min / max logical height is handled in updateLogicalHeight().
    setLogicalHeight(logicalHeight() + borderAndPaddingLogicalHeight());
    if (hasLineIfEmpty()) {
        LayoutUnit minHeight = borderAndPaddingLogicalHeight()
            + lineHeight(true, isHorizontalWritingMode() ? HorizontalLine : VerticalLine, PositionOfInteriorLineBoxes)
            + scrollbarLogicalHeight();
        if (height() < minHeight)
            setLogicalHeight(minHeight);
    }

    clearGrid();
}

GridCoordinate RenderGrid::cachedGridCoordinate(const RenderBox& gridItem) const
{
    ASSERT(m_gridItemCoordinate.contains(&gridItem));
    return m_gridItemCoordinate.get(&gridItem);
}

LayoutUnit RenderGrid::gridAreaBreadthForChild(const RenderBox& child, GridTrackSizingDirection direction, const Vector<GridTrack>& tracks) const
{
    const GridCoordinate& coordinate = cachedGridCoordinate(child);
    const GridSpan& span = (direction == ForColumns) ? coordinate.columns : coordinate.rows;
    LayoutUnit gridAreaBreadth = 0;
    for (auto& trackPosition : span)
        gridAreaBreadth += tracks[trackPosition.toInt()].baseSize();
    return gridAreaBreadth;
}

void RenderGrid::populateGridPositions(const GridSizingData& sizingData)
{
    m_columnPositions.resizeToFit(sizingData.columnTracks.size() + 1);
    m_columnPositions[0] = borderAndPaddingStart();
    for (unsigned i = 0; i < m_columnPositions.size() - 1; ++i)
        m_columnPositions[i + 1] = m_columnPositions[i] + sizingData.columnTracks[i].baseSize();

    m_rowPositions.resizeToFit(sizingData.rowTracks.size() + 1);
    m_rowPositions[0] = borderAndPaddingBefore();
    for (unsigned i = 0; i < m_rowPositions.size() - 1; ++i)
        m_rowPositions[i + 1] = m_rowPositions[i] + sizingData.rowTracks[i].baseSize();
}

LayoutPoint RenderGrid::findChildLogicalPosition(RenderBox& child, const GridSizingData& sizingData)
{
    const GridCoordinate& coordinate = cachedGridCoordinate(child);
    ASSERT_UNUSED(sizingData, coordinate.columns.resolvedInitialPosition.toInt() < sizingData.columnTracks.size());
    ASSERT_UNUSED(sizingData, coordinate.rows.resolvedInitialPosition.toInt() < sizingData.rowTracks.size());

    // The grid items should be inside the grid container's border box, that's why they need to be shifted.
    return LayoutPoint(m_columnPositions[coordinate.columns.resolvedInitialPosition.toInt()] + marginStartForChild(child), m_rowPositions[coordinate.rows.resolvedInitialPosition.toInt()] + marginBeforeForChild(child));
}

void RenderGrid::paintChildren(PaintInfo& paintInfo, const LayoutPoint& paintOffset, PaintInfo& forChild, bool usePrintRect)
{
    for (RenderBox* child = m_orderIterator.first(); child; child = m_orderIterator.next())
        paintChild(*child, paintInfo, paintOffset, forChild, usePrintRect, PaintAsInlineBlock);
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
