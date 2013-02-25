/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#include "config.h"
#include "RenderGrid.h"

#include "LayoutRepainter.h"
#include "NotImplemented.h"
#include "RenderLayer.h"
#include "RenderView.h"

namespace WebCore {

static const int infinity = intMaxForLayoutUnit;

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

class RenderGrid::GridIterator {
    WTF_MAKE_NONCOPYABLE(GridIterator);
public:
    // |direction| is the direction that is fixed to |fixedTrackIndex| so e.g
    // GridIterator(m_grid, ForColumns, 1) will walk over the rows of the 2nd column.
    GridIterator(const Vector<Vector<Vector<RenderBox*, 1> > >& grid, TrackSizingDirection direction, size_t fixedTrackIndex)
        : m_grid(grid)
        , m_direction(direction)
        , m_rowIndex((direction == ForColumns) ? 0 : fixedTrackIndex)
        , m_columnIndex((direction == ForColumns) ? fixedTrackIndex : 0)
        , m_childIndex(0)
    {
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

    PassOwnPtr<GridCoordinate> nextEmptyGridArea()
    {
        if (m_grid.isEmpty())
            return nullptr;

        size_t& varyingTrackIndex = (m_direction == ForColumns) ? m_rowIndex : m_columnIndex;
        const size_t endOfVaryingTrackIndex = (m_direction == ForColumns) ? m_grid.size() : m_grid[0].size();
        for (; varyingTrackIndex < endOfVaryingTrackIndex; ++varyingTrackIndex) {
            const Vector<RenderBox*>& children = m_grid[m_rowIndex][m_columnIndex];
            if (children.isEmpty()) {
                PassOwnPtr<GridCoordinate> result =  adoptPtr(new GridCoordinate(m_rowIndex, m_columnIndex));
                // Advance the iterator to avoid an infinite loop where we would return the same grid area over and over.
                ++varyingTrackIndex;
                return result;
            }
        }
        return nullptr;
    }

private:
    const Vector<Vector<Vector<RenderBox*, 1> > >& m_grid;
    TrackSizingDirection m_direction;
    size_t m_rowIndex;
    size_t m_columnIndex;
    size_t m_childIndex;
};

RenderGrid::RenderGrid(Element* element)
    : RenderBlock(element)
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
    LayoutStateMaintainer statePusher(view(), this, locationOffset(), hasTransform() || hasReflection() || style()->isFlippedBlocksWritingMode());

    if (inRenderFlowThread()) {
        // Regions changing widths can force us to relayout our children.
        if (logicalWidthChangedInRegions())
            relayoutChildren = true;
    }
    updateRegionsAndExclusionsLogicalSize();

    LayoutSize previousSize = size();

    setLogicalHeight(0);
    updateLogicalWidth();

    layoutGridItems();

    LayoutUnit oldClientAfterEdge = clientLogicalBottom();
    updateLogicalHeight();

    if (size() != previousSize)
        relayoutChildren = true;

    layoutPositionedObjects(relayoutChildren || isRoot());

    computeRegionRangeForBlock();

    computeOverflow(oldClientAfterEdge);
    statePusher.pop();

    updateLayerTransform();

    // Update our scroll information if we're overflow:auto/scroll/hidden now that we know if
    // we overflow or not.
    if (hasOverflowClip())
        layer()->updateScrollInfoAfterLayout();

    repainter.repaintAfterLayout();

    setNeedsLayout(false);
}

void RenderGrid::computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    const_cast<RenderGrid*>(this)->placeItemsOnGrid();

    const Vector<GridTrackSize>& trackStyles = style()->gridColumns();
    for (size_t i = 0; i < trackStyles.size(); ++i) {
        LayoutUnit minTrackBreadth = computePreferredTrackWidth(trackStyles[i].minTrackBreadth(), i);
        LayoutUnit maxTrackBreadth = computePreferredTrackWidth(trackStyles[i].maxTrackBreadth(), i);
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

    // FIXME: We don't take our own logical width into account.
    computeIntrinsicLogicalWidths(m_minPreferredLogicalWidth, m_maxPreferredLogicalWidth);
    // FIXME: We should account for min / max logical width.

    LayoutUnit borderAndPaddingInInlineDirection = borderAndPaddingLogicalWidth();
    m_minPreferredLogicalWidth += borderAndPaddingInInlineDirection;
    m_maxPreferredLogicalWidth += borderAndPaddingInInlineDirection;

    setPreferredLogicalWidthsDirty(false);
}

LayoutUnit RenderGrid::computePreferredTrackWidth(const Length& length, size_t trackIndex) const
{
    if (length.isFixed()) {
        // Grid areas don't have borders, margins or paddings so we don't need to account for them.
        return length.intValue();
    }

    if (length.isMinContent()) {
        LayoutUnit minContentSize = 0;
        GridIterator iterator(m_grid, ForColumns, trackIndex);
        while (RenderBox* gridItem = iterator.nextGridItem()) {
            // FIXME: We should include the child's fixed margins like RenderFlexibleBox.
            minContentSize = std::max(minContentSize, gridItem->minPreferredLogicalWidth());
        }
        return minContentSize;
    }

    if (length.isMaxContent()) {
        LayoutUnit maxContentSize = 0;
        GridIterator iterator(m_grid, ForColumns, trackIndex);
        while (RenderBox* gridItem = iterator.nextGridItem()) {
            // FIXME: We should include the child's fixed margins like RenderFlexibleBox.
            maxContentSize = std::max(maxContentSize, gridItem->maxPreferredLogicalWidth());
        }
        return maxContentSize;
    }

    // FIXME: css3-sizing mentions that we should resolve "definite sizes"
    // (including <percentage> and calc()) but we don't do it elsewhere.
    return 0;
}

void RenderGrid::computedUsedBreadthOfGridTracks(TrackSizingDirection direction, Vector<GridTrack>& columnTracks, Vector<GridTrack>& rowTracks)
{
    const Vector<GridTrackSize>& trackStyles = (direction == ForColumns) ? style()->gridColumns() : style()->gridRows();
    LayoutUnit availableLogicalSpace = (direction == ForColumns) ? availableLogicalWidth() : availableLogicalHeight(IncludeMarginBorderPadding);
    Vector<GridTrack>& tracks = (direction == ForColumns) ? columnTracks : rowTracks;
    for (size_t i = 0; i < trackStyles.size(); ++i) {
        GridTrack& track = tracks[i];
        const Length& minTrackBreadth = trackStyles[i].minTrackBreadth();
        const Length& maxTrackBreadth = trackStyles[i].maxTrackBreadth();

        track.m_usedBreadth = computeUsedBreadthOfMinLength(direction, minTrackBreadth);
        track.m_maxBreadth = computeUsedBreadthOfMaxLength(direction, maxTrackBreadth);

        track.m_maxBreadth = std::max(track.m_maxBreadth, track.m_usedBreadth);

        availableLogicalSpace -= track.m_usedBreadth;
    }

    // FIXME: We shouldn't call resolveContentBasedTrackSizingFunctions if we have no min-content / max-content tracks.
    resolveContentBasedTrackSizingFunctions(direction, columnTracks, rowTracks, availableLogicalSpace);

    if (availableLogicalSpace <= 0)
        return;

    const size_t tracksSize = tracks.size();
    Vector<GridTrack*> tracksForDistribution(tracksSize);
    for (size_t i = 0; i < tracksSize; ++i)
        tracksForDistribution[i] = tracks.data() + i;

    distributeSpaceToTracks(tracksForDistribution, 0, &GridTrack::usedBreadth, &GridTrack::growUsedBreadth, availableLogicalSpace);
}

LayoutUnit RenderGrid::computeUsedBreadthOfMinLength(TrackSizingDirection direction, const Length& trackLength) const
{
    if (trackLength.isFixed() || trackLength.isPercent() || trackLength.isViewportPercentage())
        return computeUsedBreadthOfSpecifiedLength(direction, trackLength);

    ASSERT(trackLength.isMinContent() || trackLength.isMaxContent() || trackLength.isAuto());
    return 0;
}

LayoutUnit RenderGrid::computeUsedBreadthOfMaxLength(TrackSizingDirection direction, const Length& trackLength) const
{
    if (trackLength.isFixed() || trackLength.isPercent() || trackLength.isViewportPercentage()) {
        LayoutUnit computedBreadth = computeUsedBreadthOfSpecifiedLength(direction, trackLength);
        // FIXME: We should ASSERT that computedBreadth cannot return infinity but it's currently
        // possible. See https://bugs.webkit.org/show_bug.cgi?id=107053
        return computedBreadth;
    }

    ASSERT(trackLength.isMinContent() || trackLength.isMaxContent() || trackLength.isAuto());
    return infinity;
}

LayoutUnit RenderGrid::computeUsedBreadthOfSpecifiedLength(TrackSizingDirection direction, const Length& trackLength) const
{
    // FIXME: We still need to support calc() here (https://webkit.org/b/103761).
    ASSERT(trackLength.isFixed() || trackLength.isPercent() || trackLength.isViewportPercentage());
    return valueForLength(trackLength, direction == ForColumns ? logicalWidth() : computeContentLogicalHeight(MainOrPreferredSize, style()->logicalHeight()), view());
}

const GridTrackSize& RenderGrid::gridTrackSize(TrackSizingDirection direction, size_t i)
{
    const Vector<GridTrackSize>& trackStyles = (direction == ForColumns) ? style()->gridColumns() : style()->gridRows();
    if (i >= trackStyles.size()) {
        // FIXME: This should match the default grid sizing (https://webkit.org/b/103333)
        DEFINE_STATIC_LOCAL(GridTrackSize, defaultAutoSize, (Auto));
        return defaultAutoSize;
    }
    return trackStyles[i];
}

size_t RenderGrid::maximumIndexInDirection(TrackSizingDirection direction) const
{
    const Vector<GridTrackSize>& trackStyles = (direction == ForColumns) ? style()->gridColumns() : style()->gridRows();

    size_t maximumIndex = std::max<size_t>(1, trackStyles.size());

    for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        const GridPositions& positions = (direction == ForColumns) ? child->style()->gridItemColumn() : child->style()->gridItemRow();
        // 'auto' items will need to be resolved in seperate phases anyway. Note that because maximumIndex is at least 1,
        // the grid-auto-flow == none case is automatically handled.
        if (positions.firstPosition().isAuto())
            continue;

        // This function bypasses the cache (cachedGridCoordinate()) as it is used to build it.
        maximumIndex = std::max(maximumIndex, resolveGridPositionFromStyle(positions.firstPosition()) + 1);
    }

    return maximumIndex;
}

LayoutUnit RenderGrid::logicalContentHeightForChild(RenderBox* child, Vector<GridTrack>& columnTracks)
{
    // FIXME: We shouldn't force a layout every time this function is called but
    // 1) Return computeLogicalHeight's value if it's available. Unfortunately computeLogicalHeight
    // doesn't return if the logical height is available so would need to be changed.
    // 2) Relayout if the column track's used breadth changed OR the logical height is unavailable.
    if (!child->needsLayout())
        child->setNeedsLayout(true, MarkOnlyThis);

    const GridCoordinate& coordinate = cachedGridCoordinate(child);
    child->setOverrideContainingBlockContentLogicalWidth(columnTracks[coordinate.columnIndex].m_usedBreadth);
    child->clearOverrideContainingBlockContentLogicalHeight();
    child->layout();
    return child->logicalHeight();
}

LayoutUnit RenderGrid::minContentForChild(RenderBox* child, TrackSizingDirection direction, Vector<GridTrack>& columnTracks)
{
    bool hasOrthogonalWritingMode = child->isHorizontalWritingMode() != isHorizontalWritingMode();
    // FIXME: Properly support orthogonal writing mode.
    if (hasOrthogonalWritingMode)
        return 0;

    if (direction == ForColumns) {
        // FIXME: It's unclear if we should return the intrinsic width or the preferred width.
        // See http://lists.w3.org/Archives/Public/www-style/2013Jan/0245.html
        return child->minPreferredLogicalWidth();
    }

    return logicalContentHeightForChild(child, columnTracks);
}

LayoutUnit RenderGrid::maxContentForChild(RenderBox* child, TrackSizingDirection direction, Vector<GridTrack>& columnTracks)
{
    bool hasOrthogonalWritingMode = child->isHorizontalWritingMode() != isHorizontalWritingMode();
    // FIXME: Properly support orthogonal writing mode.
    if (hasOrthogonalWritingMode)
        return LayoutUnit();

    if (direction == ForColumns) {
        // FIXME: It's unclear if we should return the intrinsic width or the preferred width.
        // See http://lists.w3.org/Archives/Public/www-style/2013Jan/0245.html
        return child->maxPreferredLogicalWidth();
    }

    return logicalContentHeightForChild(child, columnTracks);
}

void RenderGrid::resolveContentBasedTrackSizingFunctions(TrackSizingDirection direction, Vector<GridTrack>& columnTracks, Vector<GridTrack>& rowTracks, LayoutUnit& availableLogicalSpace)
{
    // FIXME: Split the grid tracks once we support spanning or fractions (step 1 and 2 of the algorithm).

    Vector<GridTrack>& tracks = (direction == ForColumns) ? columnTracks : rowTracks;

    for (size_t i = 0; i < tracks.size(); ++i) {
        const GridTrackSize& trackSize = gridTrackSize(direction, i);
        GridTrack& track = tracks[i];
        const Length& minTrackBreadth = trackSize.minTrackBreadth();
        if (minTrackBreadth.isMinContent() || minTrackBreadth.isMaxContent()) {
            LayoutUnit oldUsedBreadth = track.m_usedBreadth;
            resolveContentBasedTrackSizingFunctionsForItems(direction, columnTracks, rowTracks, i, &RenderGrid::minContentForChild, &GridTrack::usedBreadth, &GridTrack::growUsedBreadth);
            availableLogicalSpace -= (track.m_usedBreadth - oldUsedBreadth);
        }

        if (minTrackBreadth.isMaxContent()) {
            LayoutUnit oldUsedBreadth = track.m_usedBreadth;
            resolveContentBasedTrackSizingFunctionsForItems(direction, columnTracks, rowTracks, i, &RenderGrid::maxContentForChild, &GridTrack::usedBreadth, &GridTrack::growUsedBreadth);
            availableLogicalSpace -= (track.m_usedBreadth - oldUsedBreadth);
        }

        const Length& maxTrackBreadth = trackSize.maxTrackBreadth();
        if (maxTrackBreadth.isMinContent() || maxTrackBreadth.isMaxContent())
            resolveContentBasedTrackSizingFunctionsForItems(direction, columnTracks, rowTracks, i, &RenderGrid::minContentForChild, &GridTrack::maxBreadthIfNotInfinite, &GridTrack::growMaxBreadth);

        if (maxTrackBreadth.isMaxContent())
            resolveContentBasedTrackSizingFunctionsForItems(direction, columnTracks, rowTracks, i, &RenderGrid::maxContentForChild, &GridTrack::maxBreadthIfNotInfinite, &GridTrack::growMaxBreadth);

        if (track.m_maxBreadth == infinity)
            track.m_maxBreadth = track.m_usedBreadth;
    }
}

void RenderGrid::resolveContentBasedTrackSizingFunctionsForItems(TrackSizingDirection direction, Vector<GridTrack>& columnTracks, Vector<GridTrack>& rowTracks, size_t trackIndex, SizingFunction sizingFunction, AccumulatorGetter trackGetter, AccumulatorGrowFunction trackGrowthFunction)
{
    GridTrack& track = (direction == ForColumns) ? columnTracks[trackIndex] : rowTracks[trackIndex];
    GridIterator iterator(m_grid, direction, trackIndex);
    while (RenderBox* gridItem = iterator.nextGridItem()) {
        LayoutUnit contentSize = (this->*sizingFunction)(gridItem, direction, columnTracks);
        LayoutUnit additionalBreadthSpace = contentSize - (track.*trackGetter)();
        Vector<GridTrack*> tracks;
        tracks.append(&track);
        // FIXME: We should pass different values for |tracksForGrowthAboveMaxBreadth|.
        distributeSpaceToTracks(tracks, &tracks, trackGetter, trackGrowthFunction, additionalBreadthSpace);
    }
}

static bool sortByGridTrackGrowthPotential(const GridTrack* track1, const GridTrack* track2)
{
    return (track1->m_maxBreadth - track1->m_usedBreadth) < (track2->m_maxBreadth - track2->m_usedBreadth);
}

void RenderGrid::distributeSpaceToTracks(Vector<GridTrack*>& tracks, Vector<GridTrack*>* tracksForGrowthAboveMaxBreadth, AccumulatorGetter trackGetter, AccumulatorGrowFunction trackGrowthFunction, LayoutUnit& availableLogicalSpace)
{
    std::sort(tracks.begin(), tracks.end(), sortByGridTrackGrowthPotential);

    size_t tracksSize = tracks.size();
    Vector<LayoutUnit> updatedTrackBreadths(tracksSize);

    for (size_t i = 0; i < tracksSize; ++i) {
        GridTrack& track = *tracks[i];
        LayoutUnit availableLogicalSpaceShare = availableLogicalSpace / (tracksSize - i);
        LayoutUnit trackBreadth = (tracks[i]->*trackGetter)();
        LayoutUnit growthShare = std::min(availableLogicalSpaceShare, track.m_maxBreadth - trackBreadth);
        updatedTrackBreadths[i] = trackBreadth + growthShare;
        availableLogicalSpace -= growthShare;
    }

    if (availableLogicalSpace > 0 && tracksForGrowthAboveMaxBreadth) {
        tracksSize = tracksForGrowthAboveMaxBreadth->size();
        for (size_t i = 0; i < tracksSize; ++i) {
            LayoutUnit growthShare = availableLogicalSpace / (tracksSize - i);
            updatedTrackBreadths[i] += growthShare;
            availableLogicalSpace -= growthShare;
        }
    }

    for (size_t i = 0; i < tracksSize; ++i) {
        LayoutUnit growth = updatedTrackBreadths[i] - (tracks[i]->*trackGetter)();
        if (growth >= 0)
            (tracks[i]->*trackGrowthFunction)(growth);
    }
}

#ifndef NDEBUG
bool RenderGrid::tracksAreWiderThanMinTrackBreadth(TrackSizingDirection direction, const Vector<GridTrack>& tracks)
{
    for (size_t i = 0; i < tracks.size(); ++i) {
        const GridTrackSize& trackSize = gridTrackSize(direction, i);
        const Length& minTrackBreadth = trackSize.minTrackBreadth();
        if (computeUsedBreadthOfMinLength(direction, minTrackBreadth) > tracks[i].m_usedBreadth)
            return false;
    }
    return true;
}
#endif

void RenderGrid::growGrid(TrackSizingDirection direction)
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

void RenderGrid::insertItemIntoGrid(RenderBox* child, size_t rowTrack, size_t columnTrack)
{
    m_grid[rowTrack][columnTrack].append(child);
    m_gridItemCoordinate.set(child, GridCoordinate(rowTrack, columnTrack));
}

void RenderGrid::placeItemsOnGrid()
{
    ASSERT(!gridWasPopulated());
    ASSERT(m_gridItemCoordinate.isEmpty());

    m_grid.grow(maximumIndexInDirection(ForRows));
    size_t maximumColumnIndex = maximumIndexInDirection(ForColumns);
    for (size_t i = 0; i < m_grid.size(); ++i)
        m_grid[i].grow(maximumColumnIndex);

    Vector<RenderBox*> autoMajorAxisAutoGridItems;
    Vector<RenderBox*> specifiedMajorAxisAutoGridItems;
    GridAutoFlow autoFlow = style()->gridAutoFlow();
    for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        const GridPositions& columnPositions = child->style()->gridItemColumn();
        const GridPositions& rowPositions = child->style()->gridItemRow();
        if (autoFlow != AutoFlowNone && (columnPositions.firstPosition().isAuto() || rowPositions.firstPosition().isAuto())) {
            const GridPositions& majorAxisPositions = autoPlacementMajorAxisPositionsForChild(child);
            if (majorAxisPositions.firstPosition().isAuto())
                autoMajorAxisAutoGridItems.append(child);
            else
                specifiedMajorAxisAutoGridItems.append(child);
            continue;
        }
        size_t columnTrack = resolveGridPositionFromStyle(columnPositions.firstPosition());
        size_t rowTrack = resolveGridPositionFromStyle(rowPositions.firstPosition());
        insertItemIntoGrid(child, rowTrack, columnTrack);
    }

    ASSERT(gridRowCount() >= style()->gridRows().size());
    ASSERT(gridColumnCount() >= style()->gridColumns().size());

    if (autoFlow == AutoFlowNone) {
        // If we did collect some grid items, they won't be placed thus never laid out.
        ASSERT(!autoMajorAxisAutoGridItems.size());
        ASSERT(!specifiedMajorAxisAutoGridItems.size());
        return;
    }

    placeSpecifiedMajorAxisItemsOnGrid(specifiedMajorAxisAutoGridItems);
    placeAutoMajorAxisItemsOnGrid(autoMajorAxisAutoGridItems);
}

void RenderGrid::placeSpecifiedMajorAxisItemsOnGrid(Vector<RenderBox*> autoGridItems)
{
    for (size_t i = 0; i < autoGridItems.size(); ++i) {
        const GridPositions& majorAxisPositions = autoPlacementMajorAxisPositionsForChild(autoGridItems[i]);
        ASSERT(!majorAxisPositions.firstPosition().isAuto());
        GridIterator iterator(m_grid, autoPlacementMajorAxisDirection(), resolveGridPositionFromStyle(majorAxisPositions.firstPosition()));
        if (OwnPtr<GridCoordinate> emptyGridArea = iterator.nextEmptyGridArea()) {
            insertItemIntoGrid(autoGridItems[i], emptyGridArea->rowIndex, emptyGridArea->columnIndex);
            continue;
        }

        growGrid(autoPlacementMinorAxisDirection());
        OwnPtr<GridCoordinate> emptyGridArea = iterator.nextEmptyGridArea();
        ASSERT(emptyGridArea);
        insertItemIntoGrid(autoGridItems[i], emptyGridArea->rowIndex, emptyGridArea->columnIndex);
    }
}

void RenderGrid::placeAutoMajorAxisItemsOnGrid(Vector<RenderBox*> autoGridItems)
{
    for (size_t i = 0; i < autoGridItems.size(); ++i) {
        ASSERT(autoPlacementMajorAxisPositionsForChild(autoGridItems[i]).firstPosition().isAuto());
        placeAutoMajorAxisItemOnGrid(autoGridItems[i]);
    }
}

void RenderGrid::placeAutoMajorAxisItemOnGrid(RenderBox* gridItem)
{
    ASSERT(autoPlacementMajorAxisPositionsForChild(gridItem).firstPosition().isAuto());
    const GridPositions& minorAxisPositions = autoPlacementMinorAxisPositionsForChild(gridItem);
    size_t minorAxisIndex = 0;
    if (!minorAxisPositions.firstPosition().isAuto()) {
        minorAxisIndex = resolveGridPositionFromStyle(minorAxisPositions.firstPosition());
        GridIterator iterator(m_grid, autoPlacementMinorAxisDirection(), minorAxisIndex);
        if (OwnPtr<GridCoordinate> emptyGridArea = iterator.nextEmptyGridArea()) {
            insertItemIntoGrid(gridItem, emptyGridArea->rowIndex, emptyGridArea->columnIndex);
            return;
        }
    } else {
        const size_t endOfMajorAxis = (autoPlacementMajorAxisDirection() == ForColumns) ? gridColumnCount() : gridRowCount();
        for (size_t majorAxisIndex = 0; majorAxisIndex < endOfMajorAxis; ++majorAxisIndex) {
            GridIterator iterator(m_grid, autoPlacementMajorAxisDirection(), majorAxisIndex);
            if (OwnPtr<GridCoordinate> emptyGridArea = iterator.nextEmptyGridArea()) {
                insertItemIntoGrid(gridItem, emptyGridArea->rowIndex, emptyGridArea->columnIndex);
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

const GridPositions& RenderGrid::autoPlacementMajorAxisPositionsForChild(const RenderBox* gridItem) const
{
    GridAutoFlow flow = style()->gridAutoFlow();
    ASSERT(flow != AutoFlowNone);
    return (flow == AutoFlowColumn) ? gridItem->style()->gridItemColumn() : gridItem->style()->gridItemRow();
}

const GridPositions& RenderGrid::autoPlacementMinorAxisPositionsForChild(const RenderBox* gridItem) const
{
    GridAutoFlow flow = style()->gridAutoFlow();
    ASSERT(flow != AutoFlowNone);
    return (flow == AutoFlowColumn) ? gridItem->style()->gridItemRow() : gridItem->style()->gridItemColumn();
}

RenderGrid::TrackSizingDirection RenderGrid::autoPlacementMajorAxisDirection() const
{
    GridAutoFlow flow = style()->gridAutoFlow();
    ASSERT(flow != AutoFlowNone);
    return (flow == AutoFlowColumn) ? ForColumns : ForRows;
}

RenderGrid::TrackSizingDirection RenderGrid::autoPlacementMinorAxisDirection() const
{
    GridAutoFlow flow = style()->gridAutoFlow();
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

    Vector<GridTrack> columnTracks(gridColumnCount());
    Vector<GridTrack> rowTracks(gridRowCount());
    computedUsedBreadthOfGridTracks(ForColumns, columnTracks, rowTracks);
    ASSERT(tracksAreWiderThanMinTrackBreadth(ForColumns, columnTracks));
    computedUsedBreadthOfGridTracks(ForRows, columnTracks, rowTracks);
    ASSERT(tracksAreWiderThanMinTrackBreadth(ForRows, rowTracks));

    for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        LayoutPoint childPosition = findChildLogicalPosition(child, columnTracks, rowTracks);

        const GridCoordinate& childCoordinate = cachedGridCoordinate(child);

        // Because the grid area cannot be styled, we don't need to adjust
        // the grid breadth to account for 'box-sizing'.
        LayoutUnit oldOverrideContainingBlockContentLogicalWidth = child->hasOverrideContainingBlockLogicalWidth() ? child->overrideContainingBlockContentLogicalWidth() : LayoutUnit();
        LayoutUnit oldOverrideContainingBlockContentLogicalHeight = child->hasOverrideContainingBlockLogicalHeight() ? child->overrideContainingBlockContentLogicalHeight() : LayoutUnit();

        // FIXME: For children in a content sized track, we clear the overrideContainingBlockContentLogicalHeight
        // in minContentForChild / maxContentForChild which means that we will always relayout the child.
        if (oldOverrideContainingBlockContentLogicalWidth != columnTracks[childCoordinate.columnIndex].m_usedBreadth || oldOverrideContainingBlockContentLogicalHeight != rowTracks[childCoordinate.rowIndex].m_usedBreadth)
            child->setNeedsLayout(true, MarkOnlyThis);

        child->setOverrideContainingBlockContentLogicalWidth(columnTracks[childCoordinate.columnIndex].m_usedBreadth);
        child->setOverrideContainingBlockContentLogicalHeight(rowTracks[childCoordinate.rowIndex].m_usedBreadth);

        // FIXME: Grid items should stretch to fill their cells. Once we
        // implement grid-{column,row}-align, we can also shrink to fit. For
        // now, just size as if we were a regular child.
        child->layoutIfNeeded();

        // FIXME: Handle border & padding on the grid element.
        child->setLogicalLocation(childPosition);
    }

    for (size_t i = 0; i < rowTracks.size(); ++i)
        setLogicalHeight(logicalHeight() + rowTracks[i].m_usedBreadth);

    // FIXME: We should handle min / max logical height.

    setLogicalHeight(logicalHeight() + borderAndPaddingLogicalHeight());
    clearGrid();
}

RenderGrid::GridCoordinate RenderGrid::cachedGridCoordinate(const RenderBox* gridItem) const
{
    ASSERT(m_gridItemCoordinate.contains(gridItem));
    return m_gridItemCoordinate.get(gridItem);
}

size_t RenderGrid::resolveGridPositionFromStyle(const GridPosition& position) const
{
    // FIXME: Handle other values for grid-{row,column} like ranges or line names.
    switch (position.type()) {
    case IntegerPosition:
        // FIXME: What does a non-positive integer mean for a column/row?
        if (!position.isPositive())
            return 0;

        return position.integerPosition() - 1;
    case AutoPosition:
        // We cannot resolve grid positions if grid-auto-flow != none as it requires several iterations.
        ASSERT(style()->gridAutoFlow() == AutoFlowNone);
        return 0;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

LayoutPoint RenderGrid::findChildLogicalPosition(RenderBox* child, const Vector<GridTrack>& columnTracks, const Vector<GridTrack>& rowTracks)
{
    const GridCoordinate& coordinate = cachedGridCoordinate(child);

    LayoutPoint offset;
    // FIXME: |columnTrack| and |rowTrack| should be smaller than our column / row count.
    for (size_t i = 0; i < coordinate.columnIndex && i < columnTracks.size(); ++i)
        offset.setX(offset.x() + columnTracks[i].m_usedBreadth);
    for (size_t i = 0; i < coordinate.rowIndex && i < rowTracks.size(); ++i)
        offset.setY(offset.y() + rowTracks[i].m_usedBreadth);

    // FIXME: Handle margins on the grid item.
    return offset;
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
