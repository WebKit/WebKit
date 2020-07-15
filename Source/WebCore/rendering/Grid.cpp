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
#include "Grid.h"

#include "GridArea.h"
#include "RenderGrid.h"

namespace WebCore {

Grid::Grid(RenderGrid& grid)
    : m_orderIterator(grid)
{
}

unsigned Grid::numTracks(GridTrackSizingDirection direction) const
{
    if (direction == ForRows)
        return m_grid.size();
    return m_grid.size() ? m_grid[0].size() : 0;
}

void Grid::ensureGridSize(unsigned maximumRowSize, unsigned maximumColumnSize)
{
    ASSERT(static_cast<int>(maximumRowSize) < GridPosition::max() * 2);
    ASSERT(static_cast<int>(maximumColumnSize) < GridPosition::max() * 2);
    const size_t oldColumnSize = numTracks(ForColumns);
    const size_t oldRowSize = numTracks(ForRows);
    if (maximumRowSize > oldRowSize) {
        m_grid.grow(maximumRowSize);
        for (size_t row = oldRowSize; row < maximumRowSize; ++row)
            m_grid[row].grow(oldColumnSize);
    }

    if (maximumColumnSize > oldColumnSize) {
        for (size_t row = 0; row < numTracks(ForRows); ++row)
            m_grid[row].grow(maximumColumnSize);
    }
}

void Grid::insert(RenderBox& child, const GridArea& area)
{
    ASSERT(area.rows.isTranslatedDefinite() && area.columns.isTranslatedDefinite());
    ensureGridSize(area.rows.endLine(), area.columns.endLine());

    for (auto row : area.rows) {
        for (auto column : area.columns)
            m_grid[row][column].append(makeWeakPtr(child));
    }

    setGridItemArea(child, area);
}

void Grid::setExplicitGridStart(unsigned rowStart, unsigned columnStart)
{
    m_explicitRowStart = rowStart;
    m_explicitColumnStart = columnStart;
}

unsigned Grid::explicitGridStart(GridTrackSizingDirection direction) const
{
    return direction == ForRows ? m_explicitRowStart : m_explicitColumnStart;
}

GridArea Grid::gridItemArea(const RenderBox& item) const
{
    ASSERT(m_gridItemArea.contains(&item));
    return m_gridItemArea.get(&item);
}

void Grid::setGridItemArea(const RenderBox& item, GridArea area)
{
    m_gridItemArea.set(&item, area);
}

void Grid::setAutoRepeatTracks(unsigned autoRepeatRows, unsigned autoRepeatColumns)
{
    ASSERT(static_cast<unsigned>(GridPosition::max()) >= numTracks(ForRows) + autoRepeatRows);
    ASSERT(static_cast<unsigned>(GridPosition::max()) >= numTracks(ForColumns) + autoRepeatColumns);
    m_autoRepeatRows = autoRepeatRows;
    m_autoRepeatColumns =  autoRepeatColumns;
}

unsigned Grid::autoRepeatTracks(GridTrackSizingDirection direction) const
{
    return direction == ForRows ? m_autoRepeatRows : m_autoRepeatColumns;
}

void Grid::setAutoRepeatEmptyColumns(std::unique_ptr<OrderedTrackIndexSet> autoRepeatEmptyColumns)
{
    ASSERT(!autoRepeatEmptyColumns || (autoRepeatEmptyColumns->size() <= m_autoRepeatColumns));
    m_autoRepeatEmptyColumns = WTFMove(autoRepeatEmptyColumns);
}

void Grid::setAutoRepeatEmptyRows(std::unique_ptr<OrderedTrackIndexSet> autoRepeatEmptyRows)
{
    ASSERT(!autoRepeatEmptyRows || (autoRepeatEmptyRows->size() <= m_autoRepeatRows));
    m_autoRepeatEmptyRows = WTFMove(autoRepeatEmptyRows);
}

bool Grid::hasAutoRepeatEmptyTracks(GridTrackSizingDirection direction) const
{
    return direction == ForColumns ? !!m_autoRepeatEmptyColumns : !!m_autoRepeatEmptyRows;
}

bool Grid::isEmptyAutoRepeatTrack(GridTrackSizingDirection direction, unsigned line) const
{
    ASSERT(hasAutoRepeatEmptyTracks(direction));
    return autoRepeatEmptyTracks(direction)->contains(line);
}

OrderedTrackIndexSet* Grid::autoRepeatEmptyTracks(GridTrackSizingDirection direction) const
{
    ASSERT(hasAutoRepeatEmptyTracks(direction));
    return direction == ForColumns ? m_autoRepeatEmptyColumns.get() : m_autoRepeatEmptyRows.get();
}

GridSpan Grid::gridItemSpan(const RenderBox& gridItem, GridTrackSizingDirection direction) const
{
    GridArea area = gridItemArea(gridItem);
    return direction == ForColumns ? area.columns : area.rows;
}

void Grid::setNeedsItemsPlacement(bool needsItemsPlacement)
{
    m_needsItemsPlacement = needsItemsPlacement;

    if (!needsItemsPlacement) {
        m_grid.shrinkToFit();
        return;
    }

    m_grid.shrink(0);
    m_gridItemArea.clear();
    m_explicitRowStart = 0;
    m_explicitColumnStart = 0;
    m_autoRepeatEmptyColumns = nullptr;
    m_autoRepeatEmptyRows = nullptr;
    m_autoRepeatColumns = 0;
    m_autoRepeatRows = 0;
}

GridIterator::GridIterator(const Grid& grid, GridTrackSizingDirection direction, unsigned fixedTrackIndex, unsigned varyingTrackIndex)
    : m_grid(grid.m_grid)
    , m_direction(direction)
    , m_rowIndex((direction == ForColumns) ? varyingTrackIndex : fixedTrackIndex)
    , m_columnIndex((direction == ForColumns) ? fixedTrackIndex : varyingTrackIndex)
    , m_childIndex(0)
{
    ASSERT(!m_grid.isEmpty());
    ASSERT(!m_grid[0].isEmpty());
    ASSERT(m_rowIndex < m_grid.size());
    ASSERT(m_columnIndex < m_grid[0].size());
}

RenderBox* GridIterator::nextGridItem()
{
    ASSERT(!m_grid.isEmpty());
    ASSERT(!m_grid[0].isEmpty());

    unsigned& varyingTrackIndex = (m_direction == ForColumns) ? m_rowIndex : m_columnIndex;
    const unsigned endOfVaryingTrackIndex = (m_direction == ForColumns) ? m_grid.size() : m_grid[0].size();
    for (; varyingTrackIndex < endOfVaryingTrackIndex; ++varyingTrackIndex) {
        const auto& children = m_grid[m_rowIndex][m_columnIndex];
        if (m_childIndex < children.size())
            return children[m_childIndex++].get();

        m_childIndex = 0;
    }
    return 0;
}

bool GridIterator::isEmptyAreaEnough(unsigned rowSpan, unsigned columnSpan) const
{
    ASSERT(!m_grid.isEmpty());
    ASSERT(!m_grid[0].isEmpty());

    // Ignore cells outside current grid as we will grow it later if needed.
    unsigned maxRows = std::min<unsigned>(m_rowIndex + rowSpan, m_grid.size());
    unsigned maxColumns = std::min<unsigned>(m_columnIndex + columnSpan, m_grid[0].size());

    // This adds a O(N^2) behavior that shouldn't be a big deal as we expect spanning areas to be small.
    for (unsigned row = m_rowIndex; row < maxRows; ++row) {
        for (unsigned column = m_columnIndex; column < maxColumns; ++column) {
            auto& children = m_grid[row][column];
            if (!children.isEmpty())
                return false;
        }
    }

    return true;
}

std::unique_ptr<GridArea> GridIterator::nextEmptyGridArea(unsigned fixedTrackSpan, unsigned varyingTrackSpan)
{
    ASSERT(!m_grid.isEmpty());
    ASSERT(!m_grid[0].isEmpty());
    ASSERT(fixedTrackSpan >= 1);
    ASSERT(varyingTrackSpan >= 1);

    if (m_grid.isEmpty())
        return nullptr;

    unsigned rowSpan = (m_direction == ForColumns) ? varyingTrackSpan : fixedTrackSpan;
    unsigned columnSpan = (m_direction == ForColumns) ? fixedTrackSpan : varyingTrackSpan;

    unsigned& varyingTrackIndex = (m_direction == ForColumns) ? m_rowIndex : m_columnIndex;
    const unsigned endOfVaryingTrackIndex = (m_direction == ForColumns) ? m_grid.size() : m_grid[0].size();
    for (; varyingTrackIndex < endOfVaryingTrackIndex; ++varyingTrackIndex) {
        if (isEmptyAreaEnough(rowSpan, columnSpan)) {
            std::unique_ptr<GridArea> result = makeUnique<GridArea>(GridSpan::translatedDefiniteGridSpan(m_rowIndex, m_rowIndex + rowSpan), GridSpan::translatedDefiniteGridSpan(m_columnIndex, m_columnIndex + columnSpan));
            // Advance the iterator to avoid an infinite loop where we would return the same grid area over and over.
            ++varyingTrackIndex;
            return result;
        }
    }
    return nullptr;
}

} // namespace WebCore
