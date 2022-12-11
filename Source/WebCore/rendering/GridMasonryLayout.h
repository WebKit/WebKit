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
#pragma once

#include "GridArea.h"
#include "GridPositionsResolver.h"
#include "LayoutUnit.h"
#include "RenderBox.h"
#include "RenderGrid.h"
namespace WebCore {

class GridMasonryLayout {
public:
    GridMasonryLayout(RenderGrid& renderGrid)
        : m_renderGrid(renderGrid)
    {
    }

    void performMasonryPlacement(unsigned gridAxisTracks, GridTrackSizingDirection masonryAxisDirection);
    LayoutUnit offsetForChild(const RenderBox&) const;
    LayoutUnit gridContentSize() const { return m_gridContentSize; };
private:
    GridSpan gridAxisPositionUsingPackAutoFlow(const RenderBox& item) const;
    GridSpan gridAxisPositionUsingNextAutoFlow(const RenderBox& item);
    GridArea gridAreaForIndefiniteGridAxisItem(const RenderBox& item);
    GridArea gridAreaForDefiniteGridAxisItem(const RenderBox&) const;

    void collectMasonryItems();
    void addItemsToFirstTrack(const HashMap<RenderBox*, GridArea>& firstTrackItems); 
    void placeItemsUsingOrderModifiedDocumentOrder(); 
    void placeItemsWithDefiniteGridAxisPosition(const Vector<RenderBox*>& itemsWithDefinitePosition);
    void placeItemsWithIndefiniteGridAxisPosition(const Vector<RenderBox*>& itemsWithIndefinitePosition);
    void setItemGridAxisContainingBlockToGridArea(RenderBox&);
    void insertIntoGridAndLayoutItem(RenderBox&, const GridArea&);

    void resizeAndResetRunningPositions();
    void allocateCapacityForMasonryVectors();
    LayoutUnit masonryAxisMarginBoxForItem(const RenderBox& child);
    void updateRunningPositions(const RenderBox& child, const GridArea&);
    void updateItemOffset(const RenderBox& child, LayoutUnit offset);
    inline GridTrackSizingDirection gridAxisDirection() const;

    bool hasDefiniteGridAxisPosition(const RenderBox& child, GridTrackSizingDirection masonryDirection) const;
    static bool itemGridAreaStartsAtFirstLine(const GridArea& area, GridTrackSizingDirection masonryDirection)
    {
        return !(masonryDirection == ForRows ? area.rows.startLine() : area.columns.startLine());
    }
    GridArea masonryGridAreaFromGridAxisSpan(const GridSpan&) const;
    GridSpan gridAxisSpanFromArea(const GridArea&) const;
    bool hasEnoughSpaceAtPosition(unsigned startingPosition, unsigned spanLength) const;

    unsigned m_gridAxisTracksCount;

    HashMap<RenderBox*, GridArea> m_firstTrackItems;
    Vector<RenderBox*> m_itemsWithDefiniteGridAxisPosition;
    Vector<RenderBox*> m_itemsWithIndefiniteGridAxisPosition;

    Vector<LayoutUnit> m_runningPositions;
    HashMap<const RenderBox*, LayoutUnit> m_itemOffsets;
    RenderGrid& m_renderGrid;
    LayoutUnit m_masonryAxisGridGap;
    LayoutUnit m_gridContentSize;

    GridTrackSizingDirection m_masonryAxisDirection;
    const GridSpan m_masonryAxisSpan = GridSpan::masonryAxisTranslatedDefiniteGridSpan();
    const unsigned m_masonryDefiniteItemsQuarterCapacity = 4;
    const unsigned m_masonryIndefiniteItemsHalfCapacity = 2;
    unsigned m_autoFlowNextCursor;
};

} // end namespace WebCore
