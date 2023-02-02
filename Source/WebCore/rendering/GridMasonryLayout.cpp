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
#include "RenderGrid.h"
#include "WritingMode.h"

namespace WebCore {

void GridMasonryLayout::performMasonryPlacement(unsigned gridAxisTracks, GridTrackSizingDirection masonryAxisDirection)
{
    m_masonryAxisDirection = masonryAxisDirection;
    m_masonryAxisGridGap = m_renderGrid.gridGap(m_masonryAxisDirection);
    m_gridAxisTracksCount = gridAxisTracks;

    allocateCapacityForMasonryVectors();
    collectMasonryItems();
    m_renderGrid.currentGrid().setupGridForMasonryLayout();
    m_renderGrid.populateExplicitGridAndOrderIterator();

    resizeAndResetRunningPositions(); 

    // 2.4 Masonry Layout Algorithm
    addItemsToFirstTrack();
    
    // the insertIntoGridAndLayoutItem() will modify the m_autoFlowNextCursor, so m_autoFlowNextCursor needs to be reset.
    m_autoFlowNextCursor = 0;

    if (m_renderGrid.style().masonryAutoFlow().placementOrder == MasonryAutoFlowPlacementOrder::Ordered)
        placeItemsUsingOrderModifiedDocumentOrder();
    else {
        placeItemsWithDefiniteGridAxisPosition();
        placeItemsWithIndefiniteGridAxisPosition();
    }
}

void GridMasonryLayout::collectMasonryItems()
{
    ASSERT(m_gridAxisTracksCount);
    m_firstTrackItems.clear();
    m_itemsWithDefiniteGridAxisPosition.resize(0);
    m_itemsWithIndefiniteGridAxisPosition.resize(0);

    auto& grid = m_renderGrid.currentGrid();
    for (auto* child = grid.orderIterator().first(); child; child = grid.orderIterator().next()) {
        if (grid.orderIterator().shouldSkipChild(*child))
            continue;

        auto gridArea = grid.gridItemArea(*child);
        if (m_firstTrackItems.size() != m_gridAxisTracksCount && itemGridAreaStartsAtFirstLine(gridArea, m_masonryAxisDirection) && m_renderGrid.itemGridAreaIsWithinImplicitGrid(gridArea, m_gridAxisTracksCount + 1, gridAxisDirection()))
            m_firstTrackItems.add(child, gridArea);
        else if (m_renderGrid.style().masonryAutoFlow().placementOrder == MasonryAutoFlowPlacementOrder::Ordered)
            m_itemsWithDefiniteGridAxisPosition.append(child);
        else if (m_renderGrid.style().masonryAutoFlow().placementOrder == MasonryAutoFlowPlacementOrder::DefiniteFirst) {
            if (hasDefiniteGridAxisPosition(*child, gridAxisDirection()))
                m_itemsWithDefiniteGridAxisPosition.append(child);
            else
                m_itemsWithIndefiniteGridAxisPosition.append(child);
        }
    }
}

void GridMasonryLayout::allocateCapacityForMasonryVectors()
{
    auto gridCapacity = m_renderGrid.currentGrid().numTracks(ForRows) * m_renderGrid.currentGrid().numTracks(ForColumns);
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

void GridMasonryLayout::addItemsToFirstTrack()
{
    for (auto& [item, gridArea] : m_firstTrackItems) {
        ASSERT(item);
        if (!item)
            continue;
        insertIntoGridAndLayoutItem(*item, gridArea);
    }
}

void GridMasonryLayout::placeItemsUsingOrderModifiedDocumentOrder()
{
    for (auto* child : m_itemsWithDefiniteGridAxisPosition) {
        ASSERT(child);
        if (!child)
            continue;

        if (hasDefiniteGridAxisPosition(*child, gridAxisDirection()))
            insertIntoGridAndLayoutItem(*child, gridAreaForDefiniteGridAxisItem(*child));
        else
            insertIntoGridAndLayoutItem(*child, gridAreaForIndefiniteGridAxisItem(*child));
    }   
}

void GridMasonryLayout::placeItemsWithDefiniteGridAxisPosition()
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
        insertIntoGridAndLayoutItem(*item, gridArea);
    }
}

GridArea GridMasonryLayout::gridAreaForDefiniteGridAxisItem(const RenderBox& child) const
{
    auto itemSpan = m_renderGrid.currentGrid().gridItemSpan(child, gridAxisDirection());
    ASSERT(!itemSpan.isIndefinite());
    itemSpan.translate(m_renderGrid.currentGrid().explicitGridStart(gridAxisDirection()));
    return masonryGridAreaFromGridAxisSpan(itemSpan);
}

void GridMasonryLayout::placeItemsWithIndefiniteGridAxisPosition()
{
    for (auto* item : m_itemsWithIndefiniteGridAxisPosition) {
        ASSERT(item);
        if (!item)
            continue;
        insertIntoGridAndLayoutItem(*item, gridAreaForIndefiniteGridAxisItem(*item));
    }
}

void GridMasonryLayout::setItemGridAxisContainingBlockToGridArea(RenderBox& child)
{
    if (gridAxisDirection() == ForColumns)
        child.setOverridingContainingBlockContentLogicalWidth(m_renderGrid.m_trackSizingAlgorithm.gridAreaBreadthForChild(child, ForColumns));
    else
        child.setOverridingContainingBlockContentLogicalHeight(m_renderGrid.m_trackSizingAlgorithm.gridAreaBreadthForChild(child, ForRows));
    
    // FIXME(249230): Try to cache masonry layout sizes
    child.setChildNeedsLayout(MarkOnlyThis);
}

void GridMasonryLayout::insertIntoGridAndLayoutItem(RenderBox& child, const GridArea& area)
{
    m_renderGrid.currentGrid().insert(child, area);
    setItemGridAxisContainingBlockToGridArea(child);
    child.layoutIfNeeded();
    updateRunningPositions(child, area);
    m_autoFlowNextCursor = gridAxisSpanFromArea(area).endLine() % m_gridAxisTracksCount;
}

LayoutUnit GridMasonryLayout::masonryAxisMarginBoxForItem(const RenderBox& child)
{
    LayoutUnit marginBoxSize;
    if (m_masonryAxisDirection == ForRows) {
        if (GridLayoutFunctions::isOrthogonalChild(m_renderGrid, child))
            marginBoxSize = child.isHorizontalWritingMode() ? child.width() + child.horizontalMarginExtent() : child.height() + child.verticalMarginExtent();
        else
            marginBoxSize = child.logicalHeight() + child.marginLogicalHeight();

    } else {
        if (GridLayoutFunctions::isOrthogonalChild(m_renderGrid, child))
            marginBoxSize = child.isHorizontalWritingMode() ? child.height() + child.verticalMarginExtent() : child.width() + child.horizontalMarginExtent();
        else
            marginBoxSize = child.logicalWidth() + child.marginLogicalWidth();
    }
    return marginBoxSize;
}

void GridMasonryLayout::updateRunningPositions(const RenderBox& child, const GridArea& area)
{
    auto gridAxisSpan = gridAxisSpanFromArea(area);
    ASSERT(gridAxisSpan.startLine() < m_runningPositions.size() && gridAxisSpan.endLine() <= m_runningPositions.size());
    gridAxisSpan.clamp(m_runningPositions.size());

    LayoutUnit previousRunningPosition;
    for (auto line : gridAxisSpan)
        previousRunningPosition = std::max(previousRunningPosition, m_runningPositions[line]);

    auto newRunningPosition = masonryAxisMarginBoxForItem(child) + previousRunningPosition + m_masonryAxisGridGap; 
    m_gridContentSize = std::max(m_gridContentSize, newRunningPosition - m_masonryAxisGridGap);

    for (auto span : gridAxisSpan)
        m_runningPositions[span] = std::max(m_runningPositions[span], newRunningPosition);

    updateItemOffset(child, previousRunningPosition);
}

void GridMasonryLayout::updateItemOffset(const RenderBox& child, LayoutUnit offset)
{
    m_itemOffsets.add(&child, offset);
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

LayoutUnit GridMasonryLayout::offsetForChild(const RenderBox& child) const
{
    const auto& offsetIter = m_itemOffsets.find(&child);
    if (offsetIter == m_itemOffsets.end())
        return 0_lu;
    return offsetIter->value;
}

inline GridTrackSizingDirection GridMasonryLayout::gridAxisDirection() const
{
    // The masonry axis and grid axis can never be the same. 
    // They are always perpendicular to each other.
    return m_masonryAxisDirection == ForRows ? ForColumns : ForRows;
}

bool GridMasonryLayout::hasDefiniteGridAxisPosition(const RenderBox& child, GridTrackSizingDirection gridAxisDirection) const 
{
    auto itemSpan = GridPositionsResolver::resolveGridPositionsFromStyle(m_renderGrid, child, gridAxisDirection);
    return !itemSpan.isIndefinite();
}

GridSpan GridMasonryLayout::gridAxisSpanFromArea(const GridArea& gridArea) const
{
    return gridAxisDirection() == ForRows ? gridArea.rows : gridArea.columns;
}

GridArea GridMasonryLayout::masonryGridAreaFromGridAxisSpan(const GridSpan& gridAxisSpan) const
{
    return m_masonryAxisDirection == ForRows ? GridArea { m_masonryAxisSpan, gridAxisSpan } : GridArea { gridAxisSpan, m_masonryAxisSpan };
}

bool GridMasonryLayout::hasEnoughSpaceAtPosition(unsigned startingPosition, unsigned spanLength) const
{
    ASSERT(startingPosition < m_gridAxisTracksCount);
    return (startingPosition + spanLength) <= m_gridAxisTracksCount;
}
} // end namespace WebCore
