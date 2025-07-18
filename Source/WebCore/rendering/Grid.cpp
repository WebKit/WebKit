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
#include "StyleGridTrackSizingDirection.h"

namespace WebCore {

Grid::Grid(RenderGrid& grid)
    : m_orderIterator(grid)
{
}

unsigned Grid::numTracks(Style::GridTrackSizingDirection direction) const
{
    if (direction == Style::GridTrackSizingDirection::Rows)
        return m_grid.size();
    return m_grid.size() ? m_grid[0].size() : 0;
}

void Grid::ensureGridSize(unsigned maximumRowSize, unsigned maximumColumnSize)
{
    ASSERT(static_cast<int>(maximumRowSize) < Style::GridPosition::max() * 2);
    ASSERT(static_cast<int>(maximumColumnSize) < Style::GridPosition::max() * 2);
    size_t oldColumnSize = numTracks(Style::GridTrackSizingDirection::Columns);
    size_t oldRowSize = numTracks(Style::GridTrackSizingDirection::Rows);
    if (maximumRowSize > oldRowSize) {
        m_grid.grow(maximumRowSize);
    }

    // Just grow the first row for now so that we know the requested size,
    // and we'll lazily allocate the others when they get used.
    if (maximumColumnSize > oldColumnSize && maximumRowSize) {
        m_grid[0].grow(maximumColumnSize);
    }
}

void Grid::ensureStorageForRow(unsigned row)
{
    m_grid[row].grow(m_grid[0].size());
}

GridArea Grid::insert(RenderBox& gridItem, const GridArea& area)
{
    GridArea clampedArea = area;
    if (m_maxRows)
        clampedArea.rows.clamp(m_maxRows);
    if (m_maxColumns)
        clampedArea.columns.clamp(m_maxColumns);

    ASSERT(clampedArea.rows.isTranslatedDefinite() && clampedArea.columns.isTranslatedDefinite());
    ensureGridSize(clampedArea.rows.endLine(), clampedArea.columns.endLine());

    for (auto row : clampedArea.rows) {
        ensureStorageForRow(row);
        for (auto column : clampedArea.columns)
            m_grid[row][column].append(gridItem);
    }

    setGridItemArea(gridItem, clampedArea);
    return clampedArea;
}

const GridCell& Grid::cell(unsigned row, unsigned column) const
{
    // Storage for rows other than the first is lazily allocated. We can
    // just return a fake entry if the request is outside the allocated area,
    // since it must be empty.
    static const NeverDestroyed<GridCell> emptyCell;
    if (row && m_grid[row].size() <= column)
        return emptyCell;

    return m_grid[row][column];
}

void Grid::setExplicitGridStart(unsigned rowStart, unsigned columnStart)
{
    m_explicitRowStart = rowStart;
    m_explicitColumnStart = columnStart;
}

unsigned Grid::explicitGridStart(Style::GridTrackSizingDirection direction) const
{
    return direction == Style::GridTrackSizingDirection::Rows ? m_explicitRowStart : m_explicitColumnStart;
}

void Grid::setClampingForSubgrid(unsigned maxRows, unsigned maxColumns)
{
    m_maxRows = maxRows;
    m_maxColumns = maxColumns;
}

void Grid::clampAreaToSubgridIfNeeded(GridArea& area)
{
    if (!area.rows.isIndefinite()) {
        if (m_maxRows)
            area.rows.clamp(m_maxRows);
    }
    if (!area.columns.isIndefinite()) {
        if (m_maxColumns)
            area.columns.clamp(m_maxColumns);
    }
}

GridArea Grid::gridItemArea(const RenderBox& item) const
{
    ASSERT(m_gridItemArea.contains(item));
    return m_gridItemArea.get(item);
}

void Grid::setGridItemArea(const RenderBox& item, GridArea area)
{
    m_gridItemArea.set(item, area);
}

void Grid::setAutoRepeatTracks(unsigned autoRepeatRows, unsigned autoRepeatColumns)
{
    ASSERT(static_cast<unsigned>(Style::GridPosition::max()) >= numTracks(Style::GridTrackSizingDirection::Rows) + autoRepeatRows);
    ASSERT(static_cast<unsigned>(Style::GridPosition::max()) >= numTracks(Style::GridTrackSizingDirection::Columns) + autoRepeatColumns);
    m_autoRepeatRows = autoRepeatRows;
    m_autoRepeatColumns =  autoRepeatColumns;
}

unsigned Grid::autoRepeatTracks(Style::GridTrackSizingDirection direction) const
{
    return direction == Style::GridTrackSizingDirection::Rows ? m_autoRepeatRows : m_autoRepeatColumns;
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

bool Grid::hasAutoRepeatEmptyTracks(Style::GridTrackSizingDirection direction) const
{
    return direction == Style::GridTrackSizingDirection::Columns ? !!m_autoRepeatEmptyColumns : !!m_autoRepeatEmptyRows;
}

bool Grid::isEmptyAutoRepeatTrack(Style::GridTrackSizingDirection direction, unsigned line) const
{
    ASSERT(hasAutoRepeatEmptyTracks(direction));
    return autoRepeatEmptyTracks(direction)->contains(line);
}

OrderedTrackIndexSet* Grid::autoRepeatEmptyTracks(Style::GridTrackSizingDirection direction) const
{
    ASSERT(hasAutoRepeatEmptyTracks(direction));
    return direction == Style::GridTrackSizingDirection::Columns ? m_autoRepeatEmptyColumns.get() : m_autoRepeatEmptyRows.get();
}

GridSpan Grid::gridItemSpan(const RenderBox& gridItem, Style::GridTrackSizingDirection direction) const
{
    return gridItemArea(gridItem).span(direction);
}

GridSpan Grid::gridItemSpanIgnoringCollapsedTracks(const RenderBox& gridItem, Style::GridTrackSizingDirection direction) const
{
    auto span = gridItemSpan(gridItem, direction);
    if (!span.startLine() || !hasAutoRepeatEmptyTracks(direction))
        return span;
    unsigned currentLine = span.startLine() - 1;

    while (currentLine && isEmptyAutoRepeatTrack(direction, currentLine))
        currentLine--;
    if (currentLine)
        return GridSpan::translatedDefiniteGridSpan(currentLine + 1, span.integerSpan());

    // Still need to check if the first track is empty
    return isEmptyAutoRepeatTrack(direction, currentLine) ? GridSpan::translatedDefiniteGridSpan(currentLine, span.integerSpan()) : GridSpan::translatedDefiniteGridSpan(currentLine + 1, span.integerSpan());
}

void Grid::setupGridForMasonryLayout()
{
    // FIXME(248472): See if we can resize grid instead of clearing it here: https://bugs.webkit.org/show_bug.cgi?id=248472
    m_grid.clear();
    m_gridItemArea.clear();
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
    m_maxColumns = 0;
    m_maxRows = 0;
}

GridIterator::GridIterator(const Grid& grid, Style::GridTrackSizingDirection direction, unsigned fixedTrackIndex, unsigned varyingTrackIndex)
    : m_grid(grid)
    , m_direction(direction)
    , m_rowIndex((direction == Style::GridTrackSizingDirection::Columns) ? varyingTrackIndex : fixedTrackIndex)
    , m_columnIndex((direction == Style::GridTrackSizingDirection::Columns) ? fixedTrackIndex : varyingTrackIndex)
    , m_gridItemIndex(0)
{
    if (m_grid.maxRows()) {
        ASSERT(m_grid.numTracks(Style::GridTrackSizingDirection::Rows));
        ASSERT(m_rowIndex < m_grid.numTracks(Style::GridTrackSizingDirection::Rows));
    }
    if (m_grid.maxColumns()) {
        ASSERT(m_grid.numTracks(Style::GridTrackSizingDirection::Columns));
        ASSERT(m_columnIndex < m_grid.numTracks(Style::GridTrackSizingDirection::Columns));
    }
}

RenderBox* GridIterator::nextGridItem()
{
    if (m_grid.maxRows())
        ASSERT(m_grid.numTracks(Style::GridTrackSizingDirection::Rows));
    if (m_grid.maxColumns())
        ASSERT(m_grid.numTracks(Style::GridTrackSizingDirection::Columns));
    unsigned& varyingTrackIndex = (m_direction == Style::GridTrackSizingDirection::Columns) ? m_rowIndex : m_columnIndex;
    const unsigned endOfVaryingTrackIndex = (m_direction == Style::GridTrackSizingDirection::Columns) ? m_grid.numTracks(Style::GridTrackSizingDirection::Rows) : m_grid.numTracks(Style::GridTrackSizingDirection::Columns);
    for (; varyingTrackIndex < endOfVaryingTrackIndex; ++varyingTrackIndex) {
        const auto& gridItems = m_grid.cell(m_rowIndex, m_columnIndex);
        if (m_gridItemIndex < gridItems.size())
            return gridItems[m_gridItemIndex++].get();

        m_gridItemIndex = 0;
    }
    return 0;
}

bool GridIterator::isEmptyAreaEnough(unsigned rowSpan, unsigned columnSpan) const
{
    if (m_grid.maxRows())
        ASSERT(m_grid.numTracks(Style::GridTrackSizingDirection::Rows));
    if (m_grid.maxColumns())
        ASSERT(m_grid.numTracks(Style::GridTrackSizingDirection::Columns));
    // Ignore cells outside current grid as we will grow it later if needed.
    unsigned maxRows = std::min<unsigned>(m_rowIndex + rowSpan, m_grid.numTracks(Style::GridTrackSizingDirection::Rows));
    unsigned maxColumns = std::min<unsigned>(m_columnIndex + columnSpan, m_grid.numTracks(Style::GridTrackSizingDirection::Columns));

    // This adds a O(N^2) behavior that shouldn't be a big deal as we expect spanning areas to be small.
    for (unsigned row = m_rowIndex; row < maxRows; ++row) {
        for (unsigned column = m_columnIndex; column < maxColumns; ++column) {
            auto& gridItems = m_grid.cell(row, column);
            if (!gridItems.isEmpty())
                return false;
        }
    }

    return true;
}

std::optional<GridArea> GridIterator::nextEmptyGridArea(unsigned fixedTrackSpan, unsigned varyingTrackSpan)
{
    if (m_grid.maxRows())
        ASSERT(m_grid.numTracks(Style::GridTrackSizingDirection::Rows));
    if (m_grid.maxColumns())
        ASSERT(m_grid.numTracks(Style::GridTrackSizingDirection::Columns));
    ASSERT(fixedTrackSpan >= 1);
    ASSERT(varyingTrackSpan >= 1);

    if (!m_grid.hasGridItems())
        return { };

    unsigned rowSpan = (m_direction == Style::GridTrackSizingDirection::Columns) ? varyingTrackSpan : fixedTrackSpan;
    unsigned columnSpan = (m_direction == Style::GridTrackSizingDirection::Columns) ? fixedTrackSpan : varyingTrackSpan;

    unsigned& varyingTrackIndex = (m_direction == Style::GridTrackSizingDirection::Columns) ? m_rowIndex : m_columnIndex;
    const unsigned endOfVaryingTrackIndex = (m_direction == Style::GridTrackSizingDirection::Columns) ? m_grid.numTracks(Style::GridTrackSizingDirection::Rows) : m_grid.numTracks(Style::GridTrackSizingDirection::Columns);
    for (; varyingTrackIndex < endOfVaryingTrackIndex; ++varyingTrackIndex) {
        if (isEmptyAreaEnough(rowSpan, columnSpan)) {
            auto result = GridArea { GridSpan::translatedDefiniteGridSpan(m_rowIndex, m_rowIndex + rowSpan), GridSpan::translatedDefiniteGridSpan(m_columnIndex, m_columnIndex + columnSpan) };
            // Advance the iterator to avoid an infinite loop where we would return the same grid area over and over.
            ++varyingTrackIndex;
            return result;
        }
    }
    return { };
}

GridIterator GridIterator::createForSubgrid(const RenderGrid& subgrid, const GridIterator& outer, GridSpan subgridSpanInOuter)
{
    ASSERT(subgrid.isSubgridInParentDirection(outer.direction()));
    CheckedPtr parent = downcast<RenderGrid>(subgrid.parent());

    // Translate the current row/column indices into the coordinate
    // space of the subgrid.
    unsigned fixedIndex = (outer.direction() == Style::GridTrackSizingDirection::Columns) ? outer.m_columnIndex : outer.m_rowIndex;
    fixedIndex -= subgridSpanInOuter.startLine();

    auto innerDirection = GridLayoutFunctions::flowAwareDirectionForGridItem(*parent, subgrid, outer.direction());
    ASSERT(subgrid.isSubgrid(innerDirection));

    if (GridLayoutFunctions::isSubgridReversedDirection(*parent, outer.direction(), subgrid)) {
        unsigned fixedMax = subgrid.currentGrid().numTracks(innerDirection);
        fixedIndex = fixedMax - fixedIndex - 1;
    }

    return GridIterator(subgrid.currentGrid(), innerDirection, fixedIndex);
}

} // namespace WebCore
