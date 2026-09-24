/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ImplicitGrid.h"

#include "GridAreaLines.h"
#include "GridFormattingContext.h"
#include "PlacedGridItem.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "UnplacedGridItem.h"
#include <wtf/Assertions.h>

namespace WebCore {
namespace Layout {

// The implicit grid is created from the explicit grid + items that are placed outside
// of the explicit grid. Since we know the explicit tracks from style we start the
// implicit grid as exactly the explicit grid and allow placement to add implicit
// tracks and grow the grid.

ImplicitGrid::ImplicitGrid(size_t totalColumnsCount, size_t totalRowsCount)
    : m_gridMatrix(GridMatrix(FillWith { }, totalRowsCount, GridRow(totalColumnsCount)))
    , m_columnsCount(totalColumnsCount)
{
}

struct GridDimensions {
    size_t totalColumnsCount { 0 };
    size_t totalRowsCount { 0 };
};

static GridDimensions calculateInitialImplicitGridDimensions(const UnplacedGridItems& unplacedGridItems, LeadingImplicitTracks leadingImplicitTracks, size_t explicitColumnsCount, size_t explicitRowsCount)
{
    // The explicit grid is preceded by any leading implicit tracks generated for items placed with
    // a negative line that resolves before the grid start. Every item's line has already been
    // shifted forward by this amount, so include the leading tracks in the initial bounds.
    size_t maximumColumnIndex = explicitColumnsCount + leadingImplicitTracks.columnsCount;
    size_t maximumRowIndex = explicitRowsCount + leadingImplicitTracks.rowsCount;

    auto updateGridBounds = [&](const UnplacedGridItem& item) {
        if (item.hasDefiniteRowPosition()) {
            auto rowRange = item.definiteRowRange();
            maximumRowIndex = std::max({ maximumRowIndex, rowRange.begin(), rowRange.end() });
        }

        if (item.hasDefiniteColumnPosition()) {
            auto columnRange = item.definiteColumnRange();
            maximumColumnIndex = std::max({ maximumColumnIndex, columnRange.begin(), columnRange.end() });
        }
    };

    for (const auto& item : unplacedGridItems.nonAutoPositionedItems)
        updateGridBounds(item);
    for (const auto& item : unplacedGridItems.definiteRowPositionedItems)
        updateGridBounds(item);

    // The implicit grid always starts with at least one row. Grid coverage guarantees at least one
    // in-flow grid item, and every item occupies at least one row, so placement would end up
    // creating this row regardless.
    maximumRowIndex = std::max<size_t>(maximumRowIndex, 1);

    return {
        maximumColumnIndex,
        maximumRowIndex
    };
}

ImplicitGrid ImplicitGrid::createInitialGrid(const UnplacedGridItems& unplacedGridItems, LeadingImplicitTracks leadingImplicitTracks, size_t explicitColumnsCount, size_t explicitRowsCount)
{
    auto initialDimensions = calculateInitialImplicitGridDimensions(
        unplacedGridItems, leadingImplicitTracks, explicitColumnsCount, explicitRowsCount);

    ImplicitGrid implicitGrid(initialDimensions.totalColumnsCount, initialDimensions.totalRowsCount);
    // 3. Determine the columns in the implicit grid.
    implicitGrid.determineImplicitGridColumns(unplacedGridItems.autoPositionedItems);

    return implicitGrid;
}

GridAreaLines ImplicitGrid::insertUnplacedGridItem(const UnplacedGridItem& unplacedGridItem)
{
    // https://drafts.csswg.org/css-grid/#common-uses-numeric
    // The initial grid bounds already cover every definitely placed item, spans included.
    return markAreaAsOccupied(unplacedGridItem.definiteColumnRange(), unplacedGridItem.definiteRowRange());
}

GridAreaLines ImplicitGrid::insertDefiniteRowItem(const UnplacedGridItem& unplacedGridItem, GridAutoFlowOptions autoFlowOptions)
{
    // Step 2 of CSS Grid auto-placement algorithm:
    // Process items locked to a given row (definite row position, auto column position)
    // See: https://www.w3.org/TR/css-grid-1/#auto-placement-algo

    ASSERT(unplacedGridItem.hasDefiniteRowPosition() && !unplacedGridItem.hasDefiniteColumnPosition());
    auto rowRange = unplacedGridItem.definiteRowRange();

    auto columnSpan = unplacedGridItem.columnSpanSize();
    std::optional<size_t> columnPosition = findColumnPositionForDefiniteRowItem(rowRange, columnSpan, autoFlowOptions);

    if (!columnPosition) {
        growColumnsForDefiniteRowItem(columnSpan, rowRange);

        // Retry finding position in the grown grid
        columnPosition = findColumnPositionForDefiniteRowItem(rowRange, columnSpan, autoFlowOptions);
        ASSERT(columnPosition);
    }

    WTF::Range<size_t> columnRange { *columnPosition, *columnPosition + columnSpan };
    ASSERT(isCellRangeEmpty(columnRange, rowRange));
    auto gridAreaLines = markAreaAsOccupied(columnRange, rowRange);

    if (autoFlowOptions.strategy != PackingStrategy::Dense) {
        for (auto row : std::views::iota(rowRange.begin(), rowRange.end()))
            m_rowCursors.set(row, columnRange.end());
    }

    return gridAreaLines;
}

// https://drafts.csswg.org/css-grid-1/#auto-placement-algo
// Step 3: Determine the columns in the implicit grid.
void ImplicitGrid::determineImplicitGridColumns(const Vector<UnplacedGridItem>& autoPositionedItems)
{
    size_t requiredColumns = columnsCount();

    // Part 1: "Among all the items with a definite column position, add columns to the end
    // of the implicit grid as necessary to accommodate those items."
    for (auto& item : autoPositionedItems) {
        if (item.hasDefiniteColumnPosition())
            requiredColumns = std::max(requiredColumns, item.definiteColumnRange().end());
    }

    // Part 2: "If the largest column span among all the items without a definite column position
    // is larger than the width of the implicit grid, add columns to accommodate that column span."
    size_t maxColumnSpan = 0;
    for (auto& item : autoPositionedItems) {
        if (!item.hasDefiniteColumnPosition())
            maxColumnSpan = std::max(maxColumnSpan, item.columnSpanSize());
    }
    requiredColumns = std::max(requiredColumns, maxColumnSpan);

    // Grow grid once to accommodate both requirements
    if (requiredColumns > columnsCount())
        growColumnsToFit(requiredColumns);
}

// https://drafts.csswg.org/css-grid-1/#auto-placement-algo
// Step 4 of CSS Grid auto-placement algorithm: Position the remaining grid items
GridAreaLines ImplicitGrid::insertAutoPositionedItem(const UnplacedGridItem& item, GridAutoFlowOptions autoFlowOptions)
{
    if (autoFlowOptions.direction != GridAutoFlowDirection::Row) {
        ASSERT_NOT_IMPLEMENTED_YET();
        // Not a real placement: column flow is rejected by grid coverage before placement runs.
        // Return a single cell rather than an empty area, since a zero span underflows the interior
        // gutter count in GridLayoutUtils::gridAreaDimensionSize().
        return { 0, 1, 0, 1 };
    }

    if (item.hasDefiniteColumnPosition())
        return placeAutoPositionedItemWithDefiniteColumn(item, autoFlowOptions);
    return placeAutoPositionedItemWithAutoColumnAndRow(item, autoFlowOptions);
}

std::optional<size_t> ImplicitGrid::findFirstAvailableColumnPosition(WTF::Range<size_t> rowRange, size_t columnSpan, size_t startSearchColumn) const
{
    auto currentColumnsCount = columnsCount();

    // If we can't fit the span starting from the search position, signal that we need to grow the grid
    if (startSearchColumn + columnSpan > currentColumnsCount)
        return std::nullopt;

    // Search within existing grid bounds
    for (size_t columnStart = startSearchColumn; columnStart <= currentColumnsCount - columnSpan; ++columnStart) {
        if (isCellRangeEmpty({ columnStart, columnStart + columnSpan }, rowRange))
            return columnStart;
    }
    // If we are unable to find a valid position, signal that we need to grow the grid.
    return std::nullopt;
}

std::optional<size_t> ImplicitGrid::findColumnPositionForDefiniteRowItem(WTF::Range<size_t> rowRange, size_t columnSpan, GridAutoFlowOptions autoFlowOptions) const
{
    if (autoFlowOptions.strategy == PackingStrategy::Dense) {
        // Dense packing: always start searching from column 0
        return findFirstAvailableColumnPosition(rowRange, columnSpan, 0);
    }
    // Sparse packing: use per-row cursors to maintain placement order
    // For multi-row items, use the maximum cursor position across all spanned rows
    ASSERT(autoFlowOptions.strategy == PackingStrategy::Sparse);
    size_t startSearchColumn = 0;
    for (auto row : std::views::iota(rowRange.begin(), rowRange.end()))
        startSearchColumn = std::max(startSearchColumn, m_rowCursors.get(row));
    return findFirstAvailableColumnPosition(rowRange, columnSpan, startSearchColumn);
}

void ImplicitGrid::growColumnsForDefiniteRowItem(size_t columnSpan, WTF::Range<size_t> rowRange)
{
    // Only reached when the item does not fit in any of the existing columns, so it has to go
    // past everything already in the rows it spans. An earlier gap is no use: the item needs the
    // same columns free in every spanned row, and the caller already searched for such a run.
    // The column is optional so that rows which are still empty stay distinguishable from rows
    // whose very first column is occupied.
    std::optional<size_t> lastOccupiedColumn;
    for (auto row : std::views::iota(rowRange.begin(), rowRange.end())) {
        for (size_t column = columnsCount(); column > 0; --column) {
            if (m_gridMatrix[row].quickGet(column - 1)) {
                lastOccupiedColumn = std::max(lastOccupiedColumn.value_or(0), column - 1);
                break;
            }
        }
    }

    // The item starts right after the last occupied cell, or at the very first column when none
    // of the spanned rows hold anything yet.
    auto columnStart = lastOccupiedColumn ? *lastOccupiedColumn + 1 : 0;
    growColumnsToFit(columnStart + columnSpan);
}

bool ImplicitGrid::isCellRangeEmpty(WTF::Range<size_t> columnRange, WTF::Range<size_t> rowRange) const
{
    for (auto row : std::views::iota(rowRange.begin(), rowRange.end())) {
        for (auto column : std::views::iota(columnRange.begin(), columnRange.end())) {
            if (m_gridMatrix[row].quickGet(column))
                return false;
        }
    }
    return true;
}

GridAreaLines ImplicitGrid::markAreaAsOccupied(WTF::Range<size_t> columnRange, WTF::Range<size_t> rowRange)
{
    // Every caller is responsible for growing the implicit grid to cover the area first.
    ASSERT(columnRange.end() <= columnsCount() && rowRange.end() <= rowsCount());

    for (auto row : std::views::iota(rowRange.begin(), rowRange.end())) {
        for (auto column : std::views::iota(columnRange.begin(), columnRange.end()))
            m_gridMatrix[row].quickSet(column);
    }

    return { columnRange.begin(), columnRange.end(), rowRange.begin(), rowRange.end() };
}

void ImplicitGrid::growColumnsToFit(size_t requiredCount)
{
    if (requiredCount > m_columnsCount) {
        // ensureSize() zeroes the bits it adds, and bits past the current width are never set, so
        // the widened part of each row always reads as unoccupied.
        for (auto& row : m_gridMatrix)
            row.ensureSize(requiredCount);
        m_columnsCount = requiredCount;
    }
}

void ImplicitGrid::growRowsToFit(size_t requiredRowIndex)
{
    while (requiredRowIndex >= rowsCount())
        m_gridMatrix.append(GridRow(m_columnsCount));
}

// FIXME: optimize cursor setting by setting to an empty slot instead of to the start for dense placement.
GridAreaLines ImplicitGrid::placeAutoPositionedItemWithDefiniteColumn(const UnplacedGridItem& item, GridAutoFlowOptions autoFlowOptions)
{
    ASSERT(item.hasDefiniteColumnPosition());
    ASSERT(!item.hasDefiniteRowPosition());

    // Items with definite column position and auto row position
    // Search vertically down the specified column.
    auto columnRange = item.definiteColumnRange();
    auto rowSpan = item.rowSpanSize();

    // Step 3 grew the implicit grid to cover the column-end line of every item in this step that
    // has a definite column position, so the cells this searches are always in bounds.
    ASSERT(columnRange.end() <= columnsCount());

    if (autoFlowOptions.strategy == PackingStrategy::Dense) {
        // Set the row position of the cursor to the start-most row line in the implicit grid.
        m_autoPlacementCursorRow = 0;
    } else {
        // Sparse packing: Check if we would be going backwards (to earlier column)
        // If so, advance the row count to avoid backtracking.
        if (columnRange.begin() < m_autoPlacementCursorColumn)
            ++m_autoPlacementCursorRow;
    }

    // "Set the column position of the cursor to the grid item's column-start line."
    m_autoPlacementCursorColumn = columnRange.begin();

    // Increment the cursor's row position until a value is found where the grid item
    // does not overlap any occupied grid cells (creating new rows in the implicit grid as necessary).
    while (true) {
        growRowsToFit(m_autoPlacementCursorRow + rowSpan - 1);

        WTF::Range<size_t> rowRange { m_autoPlacementCursorRow, m_autoPlacementCursorRow + rowSpan };
        if (isCellRangeEmpty(columnRange, rowRange)) {
            // Set the item's row-start line to the cursor's row position. The cursor stays where the
            // item landed (row at the placed row, column was already set).
            return markAreaAsOccupied(columnRange, rowRange);
        }

        // Try next row down this column.
        ++m_autoPlacementCursorRow;
    }
}

// FIXME: optimize cursor setting by setting to an empty slot instead of to the start for dense placement.
GridAreaLines ImplicitGrid::placeAutoPositionedItemWithAutoColumnAndRow(const UnplacedGridItem& item, GridAutoFlowOptions autoFlowOptions)
{
    ASSERT(!item.hasDefiniteColumnPosition() && !item.hasDefiniteRowPosition());

    auto rowSpan = item.rowSpanSize();
    auto columnSpan = item.columnSpanSize();

    // Step 3 widened the implicit grid to the largest column span among the items in this step
    // without a definite column position, so a row always has room for this item and the search
    // below terminates.
    ASSERT(columnSpan <= columnsCount());

    // Position items with automatic grid position in both axes.
    // Search left-to-right, top-to-bottom.
    if (autoFlowOptions.strategy == PackingStrategy::Dense) {
        // Set the row position of the cursor to the start-most position in the implicit grid.
        m_autoPlacementCursorRow = 0;
        m_autoPlacementCursorColumn = 0;
    }

    // Increment the column position of the auto-placement cursor until either this item's grid area
    // does not overlap any occupied grid cells, or the cursor's column position, plus the item's column span,
    // overflow the number of columns in the implicit grid, then move the cursor to the start of the next row.
    while (true) {
        // Check if we need to move to a new row.
        if (m_autoPlacementCursorColumn + columnSpan > columnsCount()) {
            // Advance to next row, reset column to 0.
            ++m_autoPlacementCursorRow;
            m_autoPlacementCursorColumn = 0;
        }

        // Ensure the grid has enough rows before checking if the range is empty.
        growRowsToFit(m_autoPlacementCursorRow + rowSpan - 1);

        // Try to place at current cursor position.
        WTF::Range<size_t> columnRange { m_autoPlacementCursorColumn, m_autoPlacementCursorColumn + columnSpan };
        WTF::Range<size_t> rowRange { m_autoPlacementCursorRow, m_autoPlacementCursorRow + rowSpan };
        if (isCellRangeEmpty(columnRange, rowRange)) {
            auto gridAreaLines = markAreaAsOccupied(columnRange, rowRange);
            // Sparse packing: Advance cursor past this item to maintain document order.
            // Spec: "Set the auto-placement cursor to the end of the item's grid area."
            // Dense packing: Cursor will be reset to (0, 0) before the next fully-auto item.
            if (autoFlowOptions.strategy == PackingStrategy::Sparse)
                m_autoPlacementCursorColumn += columnSpan;
            return gridAreaLines;
        }
        // Spec: "Increment the column position of the auto-placement cursor."
        ++m_autoPlacementCursorColumn;
    }
}

} // namespace Layout
} // namespace WebCore
