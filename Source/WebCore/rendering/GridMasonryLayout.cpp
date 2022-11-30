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
#include "WritingMode.h"

namespace WebCore {

void GridMasonryLayout::performMasonryPlacement(const HashMap<RenderBox*, GridArea>& firstTrackItems, const Vector<RenderBox*> itemsWithDefiniteGridAxisPosition, const Vector<RenderBox*> itemsWithIndefinitePosition, GridTrackSizingDirection masonryAxisDirection)
{
    m_masonryAxisDirection = masonryAxisDirection;
    m_gridAxisDirection = GridPositionsResolver::gridAxisDirection(m_masonryAxisDirection);
    m_masonryAxisGridGap = m_renderGrid.gridGap(m_masonryAxisDirection);
    m_gridAxisTracksCount = m_renderGrid.numTracks(m_gridAxisDirection);
    resizeAndResetRunningPositions(); 


    // 2.4 Masonry Layout Algorithm
    addItemsToFirstTrack(firstTrackItems);
    placeItemsWithDefiniteGridAxisPosition(itemsWithDefiniteGridAxisPosition);
    placeItemsWithIndefiniteGridAxisPosition(itemsWithIndefinitePosition);
}

void GridMasonryLayout::resizeAndResetRunningPositions()
{
    m_runningPositions.resize(m_gridAxisTracksCount);
    m_runningPositions.fill(LayoutUnit());
}

void GridMasonryLayout::addItemsToFirstTrack(const HashMap<RenderBox*, GridArea>& firstTrackItems)
{
    for (auto& [item, gridArea] : firstTrackItems)
        insertIntoGridAndLayoutItem(item, gridArea);
}

void GridMasonryLayout::placeItemsWithDefiniteGridAxisPosition(const Vector<RenderBox*>& itemsWithDefiniteGridAxisPosition)
{
    for (auto* item : itemsWithDefiniteGridAxisPosition) {
        auto itemSpan = m_renderGrid.currentGrid().gridItemSpan(*item, m_gridAxisDirection);

        ASSERT(!itemSpan.isIndefinite());

        itemSpan.translate(m_renderGrid.currentGrid().explicitGridStart(m_gridAxisDirection));

        GridArea area = m_masonryAxisDirection == ForRows ? GridArea(m_masonryAxisSpan, itemSpan) : GridArea(itemSpan, m_masonryAxisSpan);
        insertIntoGridAndLayoutItem(item, area);
    }
}

void GridMasonryLayout::placeItemsWithIndefiniteGridAxisPosition(const Vector<RenderBox*>& itemsWithIndefinitePosition)
{
    for (auto* item : itemsWithIndefinitePosition)
        insertIntoGridAndLayoutItem(item, nextMasonryPositionForItem(item));
}

void GridMasonryLayout::setItemGridAxisContainingBlockToGridArea(RenderBox* child)
{
    if (m_gridAxisDirection == ForColumns)
        child->setOverridingContainingBlockContentLogicalWidth(m_renderGrid.m_trackSizingAlgorithm.estimatedGridAreaBreadthForChild(*child, ForColumns));
    else
        child->setOverridingContainingBlockContentLogicalHeight(m_renderGrid.m_trackSizingAlgorithm.estimatedGridAreaBreadthForChild(*child, ForRows));
}

void GridMasonryLayout::insertIntoGridAndLayoutItem(RenderBox* child, const GridArea& area)
{
    m_renderGrid.currentGrid().insert(*child, area);
    setItemGridAxisContainingBlockToGridArea(child);
    child->layoutIfNeeded();
    updateRunningPositions(child, area);
}

LayoutUnit GridMasonryLayout::masonryAxisMarginBoxForItem(const RenderBox* child)
{
    LayoutUnit marginBoxSize;
    if (m_masonryAxisDirection == ForRows) {
        if (GridLayoutFunctions::isOrthogonalChild(m_renderGrid, *child))
            marginBoxSize = child->isHorizontalWritingMode() ? child->width() + child->horizontalMarginExtent() : child->height() + child->verticalMarginExtent();
        else
            marginBoxSize = child->logicalHeight() + child->marginLogicalHeight();

    } else {
        if (GridLayoutFunctions::isOrthogonalChild(m_renderGrid, *child))
            marginBoxSize = child->isHorizontalWritingMode() ? child->height() + child->verticalMarginExtent() : child->width() + child->horizontalMarginExtent();
        else
            marginBoxSize = child->logicalWidth() + child->marginLogicalWidth();
    }
    return marginBoxSize;
}

void GridMasonryLayout::updateRunningPositions(const RenderBox* child, const GridArea& area)
{
    auto gridSpan = m_masonryAxisDirection == ForRows ? area.columns : area.rows;
    ASSERT(gridSpan.startLine() < m_runningPositions.size() && gridSpan.endLine() <= m_runningPositions.size());
    gridSpan.clamp(m_runningPositions.size());

    LayoutUnit previousRunningPosition;
    for (auto line : gridSpan)
        previousRunningPosition = std::max(previousRunningPosition, m_runningPositions[line]);

    auto newRunningPosition = masonryAxisMarginBoxForItem(child) + previousRunningPosition + m_masonryAxisGridGap; 
    m_gridContentSize = std::max(m_gridContentSize, newRunningPosition - m_masonryAxisGridGap);

    for (auto span : gridSpan)
        m_runningPositions[span] = std::max(m_runningPositions[span], newRunningPosition);

    updateItemOffset(child, previousRunningPosition);
}

void GridMasonryLayout::updateItemOffset(const RenderBox* child, LayoutUnit offset)
{
    m_itemOffsets.add(child, offset);
}

GridArea GridMasonryLayout::nextMasonryPositionForItem(const RenderBox* item)
{
    auto itemSpanLength = GridPositionsResolver::spanSizeForAutoPlacedItem(*item, m_gridAxisDirection);
    auto gridAxisLines = m_gridAxisTracksCount + 1;

    LayoutUnit smallestMaxPos = LayoutUnit::max();
    unsigned smallestMaxPosLine = 0;
    for (unsigned startingLine = 0; startingLine < gridAxisLines - itemSpanLength; startingLine++) {
        LayoutUnit maxPosForCurrentStartingLine;
        for (unsigned lineOffset = 0; lineOffset < itemSpanLength; lineOffset++)
            maxPosForCurrentStartingLine = std::max(maxPosForCurrentStartingLine, m_runningPositions[startingLine + lineOffset]);
        if (maxPosForCurrentStartingLine < smallestMaxPos) {
            smallestMaxPos = maxPosForCurrentStartingLine;
            smallestMaxPosLine = startingLine;
        } 
    }

    GridSpan masonryAxisSpan = GridSpan::translatedDefiniteGridSpan(0, 1);
    GridSpan gridAxisSpan = GridSpan::translatedDefiniteGridSpan(smallestMaxPosLine, smallestMaxPosLine + itemSpanLength);
    return m_masonryAxisDirection == ForRows ? GridArea { masonryAxisSpan, gridAxisSpan } : GridArea { gridAxisSpan, masonryAxisSpan };
}

LayoutUnit GridMasonryLayout::offsetForChild(const RenderBox& child) const
{
    const auto& offsetIter = m_itemOffsets.find(&child);
    if (offsetIter == m_itemOffsets.end())
        return 0_lu;
    return offsetIter->value;
}

} // end namespace WebCore
