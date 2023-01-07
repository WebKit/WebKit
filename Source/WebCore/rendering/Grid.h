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

#pragma once

#include "GridPositionsResolver.h"
#include "OrderIterator.h"
#include <wtf/HashMap.h>
#include <wtf/ListHashSet.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

typedef Vector<WeakPtr<RenderBox>, 1> GridCell;
typedef Vector<Vector<GridCell>> GridAsMatrix;
typedef ListHashSet<size_t> OrderedTrackIndexSet;

class GridArea;
class GridPositionsResolver;
class RenderGrid;

class Grid final {
public:
    explicit Grid(RenderGrid&);

    unsigned numTracks(GridTrackSizingDirection) const;

    void ensureGridSize(unsigned maximumRowSize, unsigned maximumColumnSize);
    GridArea insert(RenderBox&, const GridArea&);

    // Note that each in flow child of a grid container becomes a grid item. This means that
    // this method will return false for a grid container with only out of flow children.
    bool hasGridItems() const { return !m_gridItemArea.isEmpty(); }

    GridArea gridItemArea(const RenderBox& item) const;
    void setGridItemArea(const RenderBox& item, GridArea);

    GridSpan gridItemSpan(const RenderBox&, GridTrackSizingDirection) const;
    GridSpan gridItemSpanIgnoringCollapsedTracks(const RenderBox&, GridTrackSizingDirection) const;

    const GridCell& cell(unsigned row, unsigned column) const;

    unsigned explicitGridStart(GridTrackSizingDirection) const;
    void setExplicitGridStart(unsigned rowStart, unsigned columnStart);

    unsigned autoRepeatTracks(GridTrackSizingDirection) const;
    void setAutoRepeatTracks(unsigned autoRepeatRows, unsigned autoRepeatColumns);

    void setClampingForSubgrid(unsigned maxRows, unsigned maxColumns);

    void clampAreaToSubgridIfNeeded(GridArea&);

    void setAutoRepeatEmptyColumns(std::unique_ptr<OrderedTrackIndexSet>);
    void setAutoRepeatEmptyRows(std::unique_ptr<OrderedTrackIndexSet>);

    unsigned autoRepeatEmptyTracksCount(GridTrackSizingDirection) const;
    bool hasAutoRepeatEmptyTracks(GridTrackSizingDirection) const;
    bool isEmptyAutoRepeatTrack(GridTrackSizingDirection, unsigned) const;

    OrderedTrackIndexSet* autoRepeatEmptyTracks(GridTrackSizingDirection) const;

    OrderIterator& orderIterator() { return m_orderIterator; }

    void setNeedsItemsPlacement(bool);
    bool needsItemsPlacement() const { return m_needsItemsPlacement; };

    void setupGridForMasonryLayout();

private:
    void ensureStorageForRow(unsigned row);

    OrderIterator m_orderIterator;

    unsigned m_explicitColumnStart { 0 };
    unsigned m_explicitRowStart { 0 };

    unsigned m_autoRepeatColumns { 0 };
    unsigned m_autoRepeatRows { 0 };

    unsigned m_maxColumns { 0 };
    unsigned m_maxRows { 0 };

    bool m_needsItemsPlacement { true };

    GridAsMatrix m_grid;

    HashMap<const RenderBox*, GridArea> m_gridItemArea;

    std::unique_ptr<OrderedTrackIndexSet> m_autoRepeatEmptyColumns;
    std::unique_ptr<OrderedTrackIndexSet> m_autoRepeatEmptyRows;
};

class GridIterator {
    WTF_MAKE_NONCOPYABLE(GridIterator);
public:
    // |direction| is the direction that is fixed to |fixedTrackIndex| so e.g
    // GridIterator(m_grid, ForColumns, 1) will walk over the rows of the 2nd column.
    GridIterator(const Grid&, GridTrackSizingDirection, unsigned fixedTrackIndex, unsigned varyingTrackIndex = 0);

    static GridIterator createForSubgrid(const RenderGrid& subgrid, const GridIterator& outer);

    RenderBox* nextGridItem();
    bool isEmptyAreaEnough(unsigned rowSpan, unsigned columnSpan) const;
    std::unique_ptr<GridArea> nextEmptyGridArea(unsigned fixedTrackSpan, unsigned varyingTrackSpan);

    GridTrackSizingDirection direction() const
    {
        return m_direction;
    }

private:
    const Grid& m_grid;
    GridTrackSizingDirection m_direction;
    unsigned m_rowIndex;
    unsigned m_columnIndex;
    unsigned m_childIndex;
};

} // namespace WebCore
