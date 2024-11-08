/*
 * Copyright (C) 2022 Apple Inc.
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
#include "GridMasonryLayout.h"

#include "GridLayoutFunctions.h"
#include "GridPositionsResolver.h"
#include "RenderBoxInlines.h"
#include "RenderGrid.h"
#include "RenderStyleInlines.h"
#include "StyleGridData.h"
#include "WritingMode.h"

namespace WebCore {

void GridMasonryLayout::initializeMasonry(unsigned gridAxisTracks, GridTrackSizingDirection masonryAxisDirection)
{
    // Reset global variables as they may contain state from previous runs of Masonry.
    m_masonryAxisDirection = masonryAxisDirection;
    m_masonryAxisGridGap = m_renderGrid.gridGap(m_masonryAxisDirection);
    m_gridAxisTracksCount = gridAxisTracks;
    m_gridContentSize = 0;

    allocateCapacityForMasonryVectors();
    collectMasonryItems();
    m_renderGrid.currentGrid().setupGridForMasonryLayout();
    m_renderGrid.populateExplicitGridAndOrderIterator();

    resizeAndResetRunningPositions();
}

void GridMasonryLayout::performMasonryPlacement(const GridTrackSizingAlgorithm& algorithm, unsigned gridAxisTracks, GridTrackSizingDirection masonryAxisDirection, GridMasonryLayout::MasonryLayoutPhase layoutPhase)
{
    initializeMasonry(gridAxisTracks, masonryAxisDirection);

    m_renderGrid.populateGridPositionsForDirection(algorithm, GridTrackSizingDirection::ForColumns);
    m_renderGrid.populateGridPositionsForDirection(algorithm, GridTrackSizingDirection::ForRows);

    // 2.3 Masonry Layout Algorithm
    // https://drafts.csswg.org/css-grid-3/#masonry-layout-algorithm
    
    // the insertIntoGridAndLayoutItem() will modify the m_autoFlowNextCursor, so m_autoFlowNextCursor needs to be reset.
    m_autoFlowNextCursor = 0;

    if (m_renderGrid.style().masonryAutoFlow().placementOrder == MasonryAutoFlowPlacementOrder::Ordered)
        placeItemsUsingOrderModifiedDocumentOrder(algorithm, layoutPhase);
    else {
        placeItemsWithDefiniteGridAxisPosition(algorithm, layoutPhase);
        placeItemsWithIndefiniteGridAxisPosition(algorithm, layoutPhase);
    }
}

void GridMasonryLayout::collectMasonryItems()
{
    ASSERT(m_gridAxisTracksCount);

    m_itemsWithDefiniteGridAxisPosition.shrink(0);
    m_itemsWithIndefiniteGridAxisPosition.shrink(0);

    auto& grid = m_renderGrid.currentGrid();
    for (auto* gridItem = grid.orderIterator().first(); gridItem; gridItem = grid.orderIterator().next()) {
        if (grid.orderIterator().shouldSkipChild(*gridItem))
            continue;

        if (m_renderGrid.style().masonryAutoFlow().placementOrder == MasonryAutoFlowPlacementOrder::Ordered)
            m_itemsWithDefiniteGridAxisPosition.append(gridItem);
        else if (m_renderGrid.style().masonryAutoFlow().placementOrder == MasonryAutoFlowPlacementOrder::DefiniteFirst) {
            if (hasDefiniteGridAxisPosition(*gridItem, gridAxisDirection()))
                m_itemsWithDefiniteGridAxisPosition.append(gridItem);
            else
                m_itemsWithIndefiniteGridAxisPosition.append(gridItem);
        }
    }
}

void GridMasonryLayout::allocateCapacityForMasonryVectors()
{
    auto gridCapacity = m_renderGrid.currentGrid().numTracks(GridTrackSizingDirection::ForRows) * m_renderGrid.currentGrid().numTracks(GridTrackSizingDirection::ForColumns);
    if (m_renderGrid.style().masonryAutoFlow().placementOrder == MasonryAutoFlowPlacementOrder::DefiniteFirst) {
        m_itemsWithDefiniteGridAxisPosition.reserveCapacity(gridCapacity / m_masonryDefiniteItemsQuarterCapacity);
        m_itemsWithIndefiniteGridAxisPosition.reserveCapacity(gridCapacity / m_masonryIndefiniteItemsHalfCapacity);
    } else
        // We use m_itemsWithDefiniteGridAxisPosition to hold all non first track items
        // when masonry-auto-flow is set to ordered so that we can just easily iterate
        // over the vector when placing items
        m_itemsWithDefiniteGridAxisPosition.reserveCapacity(gridCapacity);
}

void GridMasonryLayout::resizeAndResetRunningPositions()
{
    m_runningPositions.resize(m_gridAxisTracksCount);
    m_runningPositions.fill(LayoutUnit());
}

void GridMasonryLayout::placeItemsUsingOrderModifiedDocumentOrder(const GridTrackSizingAlgorithm& algorithm, GridMasonryLayout::MasonryLayoutPhase layoutPhase)
{
    for (auto* gridItem : m_itemsWithDefiniteGridAxisPosition) {
        ASSERT(gridItem);
        if (!gridItem)
            continue;

        if (hasDefiniteGridAxisPosition(*gridItem, gridAxisDirection()))
            insertIntoGridAndLayoutItem(algorithm, *gridItem, gridAreaForDefiniteGridAxisItem(*gridItem), layoutPhase);
        else
            insertIntoGridAndLayoutItem(algorithm, *gridItem, gridAreaForIndefiniteGridAxisItem(*gridItem), layoutPhase);
    }
}

void GridMasonryLayout::placeItemsWithDefiniteGridAxisPosition(const GridTrackSizingAlgorithm& algorithm, GridMasonryLayout::MasonryLayoutPhase layoutPhase)
{
    for (auto* item : m_itemsWithDefiniteGridAxisPosition) {
        ASSERT(item);
        if (!item)
            continue;

        auto itemSpan = m_renderGrid.currentGrid().gridItemSpan(*item, gridAxisDirection());

        ASSERT(!itemSpan.isIndefinite());

        itemSpan.translate(m_renderGrid.currentGrid().explicitGridStart(gridAxisDirection()));
        auto gridArea = gridAreaForDefiniteGridAxisItem(*item);
        m_renderGrid.currentGrid().clampAreaToSubgridIfNeeded(gridArea);
        insertIntoGridAndLayoutItem(algorithm, *item, gridArea, layoutPhase);
    }
}

GridArea GridMasonryLayout::gridAreaForDefiniteGridAxisItem(const RenderBox& gridItem) const
{
    auto itemSpan = m_renderGrid.currentGrid().gridItemSpan(gridItem, gridAxisDirection());
    ASSERT(!itemSpan.isIndefinite());
    itemSpan.translate(m_renderGrid.currentGrid().explicitGridStart(gridAxisDirection()));
    return masonryGridAreaFromGridAxisSpan(itemSpan);
}

void GridMasonryLayout::placeItemsWithIndefiniteGridAxisPosition(const GridTrackSizingAlgorithm& algorithm, GridMasonryLayout::MasonryLayoutPhase layoutPhase)
{
    for (auto* item : m_itemsWithIndefiniteGridAxisPosition) {
        ASSERT(item);
        if (!item)
            continue;
        insertIntoGridAndLayoutItem(algorithm, *item, gridAreaForIndefiniteGridAxisItem(*item), layoutPhase);
    }
}

LayoutUnit GridMasonryLayout::calculateMasonryIntrinsicLogicalWidth(RenderBox& gridItem, GridMasonryLayout::MasonryLayoutPhase layoutPhase)
{
    switch (layoutPhase) {
    case MasonryLayoutPhase::MinContentPhase:
        return gridItem.computeIntrinsicLogicalWidthUsing(Length(LengthType::MinContent), { }, gridItem.borderAndPaddingLogicalWidth());
    case MasonryLayoutPhase::MaxContentPhase:
        return gridItem.computeIntrinsicLogicalWidthUsing(Length(LengthType::MaxContent), { }, gridItem.borderAndPaddingLogicalWidth());
    case MasonryLayoutPhase::LayoutPhase:
        ASSERT_NOT_REACHED();
        return { };
    }

    return { };
}

void GridMasonryLayout::setItemGridAxisContainingBlockToGridArea(const GridTrackSizingAlgorithm& algorithm, RenderBox& gridItem)
{
    if (gridAxisDirection() == GridTrackSizingDirection::ForColumns)
        gridItem.setOverridingContainingBlockContentLogicalWidth(algorithm.gridAreaBreadthForGridItem(gridItem, GridTrackSizingDirection::ForColumns));
    else
        gridItem.setOverridingContainingBlockContentLogicalHeight(algorithm.gridAreaBreadthForGridItem(gridItem, GridTrackSizingDirection::ForRows));
    
    // FIXME(249230): Try to cache masonry layout sizes
    gridItem.setChildNeedsLayout(MarkOnlyThis);
}

void GridMasonryLayout::insertIntoGridAndLayoutItem(const GridTrackSizingAlgorithm& algorithm, RenderBox& gridItem, const GridArea& area, GridMasonryLayout::MasonryLayoutPhase layoutPhase)
{
    auto shouldOverrideLogicalWidth = [&](RenderBox& gridItem, GridMasonryLayout::MasonryLayoutPhase layoutPhase) {
        if (layoutPhase == MasonryLayoutPhase::LayoutPhase)
            return false;

        if (!(gridItem.style().logicalWidth().isAuto() || gridItem.style().logicalWidth().isPercent()))
            return false;

        ASSERT(m_renderGrid.isMasonry(GridTrackSizingDirection::ForColumns));

        if (gridItem.style().writingMode().isOrthogonal(m_renderGrid.style().writingMode()))
            return false;

        if (auto* renderGrid = dynamicDowncast<RenderGrid>(gridItem); renderGrid && renderGrid->isSubgridRows())
            return false;

        return true;
    };

    if (shouldOverrideLogicalWidth(gridItem, layoutPhase))
        gridItem.setOverridingLogicalWidth(calculateMasonryIntrinsicLogicalWidth(gridItem, layoutPhase));

    m_renderGrid.currentGrid().insert(gridItem, area);
    setItemGridAxisContainingBlockToGridArea(algorithm, gridItem);
    gridItem.layoutIfNeeded();
    updateRunningPositions(gridItem, area);
    m_autoFlowNextCursor = gridAxisSpanFromArea(area).endLine() % m_gridAxisTracksCount;
}

LayoutUnit GridMasonryLayout::masonryAxisMarginBoxForItem(const RenderBox& gridItem)
{
    LayoutUnit marginBoxSize;
    if (m_masonryAxisDirection == GridTrackSizingDirection::ForRows) {
        if (GridLayoutFunctions::isOrthogonalGridItem(m_renderGrid, gridItem))
            marginBoxSize = gridItem.isHorizontalWritingMode() ? gridItem.width() + gridItem.horizontalMarginExtent() : gridItem.height() + gridItem.verticalMarginExtent();
        else
            marginBoxSize = gridItem.logicalHeight() + gridItem.marginLogicalHeight();

    } else {
        if (GridLayoutFunctions::isOrthogonalGridItem(m_renderGrid, gridItem))
            marginBoxSize = gridItem.isHorizontalWritingMode() ? gridItem.height() + gridItem.verticalMarginExtent() : gridItem.width() + gridItem.horizontalMarginExtent();
        else
            marginBoxSize = gridItem.logicalWidth() + gridItem.marginLogicalWidth();
    }
    return marginBoxSize;
}

void GridMasonryLayout::updateRunningPositions(const RenderBox& gridItem, const GridArea& area)
{
    auto gridAxisSpan = gridAxisSpanFromArea(area);
    ASSERT(gridAxisSpan.startLine() < m_runningPositions.size() && gridAxisSpan.endLine() <= m_runningPositions.size());
    gridAxisSpan.clamp(m_runningPositions.size());

    LayoutUnit previousRunningPosition;
    for (auto line : gridAxisSpan)
        previousRunningPosition = std::max(previousRunningPosition, m_runningPositions[line]);

    auto newRunningPosition = masonryAxisMarginBoxForItem(gridItem) + previousRunningPosition + m_masonryAxisGridGap;
    m_gridContentSize = std::max(m_gridContentSize, newRunningPosition - m_masonryAxisGridGap);

    for (auto span : gridAxisSpan)
        m_runningPositions[span] = std::max(m_runningPositions[span], newRunningPosition);

    updateItemOffset(gridItem, previousRunningPosition);
}

void GridMasonryLayout::updateItemOffset(const RenderBox& gridItem, LayoutUnit offset)
{
    // We set() and not add() to update the value if the |gridItem| is already inserted
    m_itemOffsets.set(gridItem, offset);
}

GridSpan GridMasonryLayout::gridAxisPositionUsingPackAutoFlow(const RenderBox& item) const
{
    auto itemSpanLength = GridPositionsResolver::spanSizeForAutoPlacedItem(item, gridAxisDirection());
    LayoutUnit smallestMaxPos = LayoutUnit::max();
    unsigned smallestMaxPosLine = 0;
    auto gridAxisLines = m_gridAxisTracksCount + 1;
    for (unsigned startingLine = 0; startingLine < gridAxisLines - itemSpanLength; startingLine++) {
        LayoutUnit maxPosForCurrentStartingLine;
        for (unsigned lineOffset = 0; lineOffset < itemSpanLength; lineOffset++)
            maxPosForCurrentStartingLine = std::max(maxPosForCurrentStartingLine, m_runningPositions[startingLine + lineOffset]);
        if (maxPosForCurrentStartingLine < smallestMaxPos) {
            smallestMaxPos = maxPosForCurrentStartingLine;
            smallestMaxPosLine = startingLine;
        } 
    }
    return GridSpan::translatedDefiniteGridSpan(smallestMaxPosLine, smallestMaxPosLine + itemSpanLength);
}

GridSpan GridMasonryLayout::gridAxisPositionUsingNextAutoFlow(const RenderBox& item)
{
    auto itemSpanLength = GridPositionsResolver::spanSizeForAutoPlacedItem(item, gridAxisDirection());
    if (!hasEnoughSpaceAtPosition(m_autoFlowNextCursor, itemSpanLength))
        m_autoFlowNextCursor = 0;
    return GridSpan::translatedDefiniteGridSpan(m_autoFlowNextCursor, m_autoFlowNextCursor + itemSpanLength);
}

GridArea GridMasonryLayout::gridAreaForIndefiniteGridAxisItem(const RenderBox& item)
{
    // Determine the logic to use for positioning based on the value of masonry-auto-flow
    GridSpan gridAxisPosition = m_renderGrid.style().masonryAutoFlow().placementAlgorithm == MasonryAutoFlowPlacementAlgorithm::Pack ? gridAxisPositionUsingPackAutoFlow(item) : gridAxisPositionUsingNextAutoFlow(item);
    return masonryGridAreaFromGridAxisSpan(gridAxisPosition);
}

LayoutUnit GridMasonryLayout::offsetForGridItem(const RenderBox& gridItem) const
{
    const auto& offsetIter = m_itemOffsets.find(gridItem);
    if (offsetIter == m_itemOffsets.end())
        return 0_lu;
    return offsetIter->value;
}

inline GridTrackSizingDirection GridMasonryLayout::gridAxisDirection() const
{
    // The masonry axis and grid axis can never be the same. 
    // They are always perpendicular to each other.
    return m_masonryAxisDirection == GridTrackSizingDirection::ForRows ? GridTrackSizingDirection::ForColumns : GridTrackSizingDirection::ForRows;
}

bool GridMasonryLayout::hasDefiniteGridAxisPosition(const RenderBox& gridItem, GridTrackSizingDirection gridAxisDirection) const
{
    auto itemSpan = GridPositionsResolver::resolveGridPositionsFromStyle(m_renderGrid, gridItem, gridAxisDirection);
    return !itemSpan.isIndefinite();
}

GridSpan GridMasonryLayout::gridAxisSpanFromArea(const GridArea& gridArea) const
{
    return gridAxisDirection() == GridTrackSizingDirection::ForRows ? gridArea.rows : gridArea.columns;
}

GridArea GridMasonryLayout::masonryGridAreaFromGridAxisSpan(const GridSpan& gridAxisSpan) const
{
    return m_masonryAxisDirection == GridTrackSizingDirection::ForRows ? GridArea { m_masonryAxisSpan, gridAxisSpan } : GridArea { gridAxisSpan, m_masonryAxisSpan };
}

bool GridMasonryLayout::hasEnoughSpaceAtPosition(unsigned startingPosition, unsigned spanLength) const
{
    ASSERT(startingPosition < m_gridAxisTracksCount);
    return (startingPosition + spanLength) <= m_gridAxisTracksCount;
}
} // end namespace WebCore
