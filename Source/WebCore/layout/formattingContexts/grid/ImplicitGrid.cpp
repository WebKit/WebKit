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
#include "GridLayout.h"
#include "PlacedGridItem.h"
#include "UnplacedGridItem.h"
#include <wtf/Assertions.h>
#include <wtf/Range.h>

namespace WebCore {
namespace Layout {

// The implicit grid is created from the explicit grid + items that are placed outside
// of the explicit grid. Since we know the explicit tracks from style we start the
// implicit grid as exactly the explicit grid and allow placement to add implicit
// tracks and grow the grid.

ImplicitGrid::ImplicitGrid(size_t totalColumnsCount, size_t totalRowsCount)
    : m_gridMatrix(Vector(totalRowsCount, Vector<GridCell>(totalColumnsCount)))
    , m_initialColumnsCount(totalColumnsCount)
{
}

void ImplicitGrid::insertUnplacedGridItem(const UnplacedGridItem& unplacedGridItem)
{
    // https://drafts.csswg.org/css-grid/#common-uses-numeric
    // Grid positions have already been normalized to non-negative matrix indices.
    auto [columnStart, columnEnd] = unplacedGridItem.normalizedColumnStartEnd();
    auto [rowStart, rowEnd] = unplacedGridItem.normalizedRowStartEnd();

    // Multi-cell items (spanning multiple columns) are not yet supported.
    if (columnEnd - columnStart > 1) {
        ASSERT_NOT_IMPLEMENTED_YET();
        return;
    }

    // Multi-cell items (spanning multiple rows) are not yet supported.
    if (rowEnd - rowStart > 1) {
        ASSERT_NOT_IMPLEMENTED_YET();
        return;
    }

    auto columnsRange = WTF::Range(columnStart, columnEnd);
    auto rowsRange = WTF::Range(rowStart, rowEnd);
    for (auto rowIndex = rowsRange.begin(); rowIndex < rowsRange.end(); ++rowIndex) {
        for (auto columnIndex = columnsRange.begin(); columnIndex < columnsRange.end(); ++columnIndex)
            m_gridMatrix[rowIndex][columnIndex].append(unplacedGridItem);
    }

}

GridAreas ImplicitGrid::gridAreas() const
{
    GridAreas gridAreas;
    gridAreas.reserveInitialCapacity(rowsCount() * columnsCount());

    for (size_t rowIndex = 0; rowIndex < m_gridMatrix.size(); ++rowIndex) {
        for (size_t columnIndex = 0; columnIndex < m_gridMatrix[rowIndex].size(); ++columnIndex) {

            const auto& gridCell = m_gridMatrix[rowIndex][columnIndex];
            for (const auto& unplacedGridItem : gridCell) {
                gridAreas.ensure(unplacedGridItem, [&]() {
                    return GridAreaLines { columnIndex, columnIndex + 1, rowIndex, rowIndex + 1 };
                });
            }
        }
    }
    return gridAreas;
}

void ImplicitGrid::insertDefiniteRowItem(const UnplacedGridItem& unplacedGridItem, GridAutoFlowOptions autoFlowOptions)
{
    // Step 2 of CSS Grid auto-placement algorithm:
    // Process items locked to a given row (definite row position, auto column position)
    // See: https://www.w3.org/TR/css-grid-1/#auto-placement-algo

    auto columnSpan = unplacedGridItem.columnSpanSize();
    // FIXME: Support multi-column spans
    ASSERT(columnSpan == 1);

    ASSERT(unplacedGridItem.hasDefiniteRowPosition() && !unplacedGridItem.hasDefiniteColumnPosition());
    auto [normalizedRowStart, normalizedRowEnd] = unplacedGridItem.normalizedRowStartEnd();
    // FIXME: Support multi-row spans
    ASSERT(normalizedRowEnd - normalizedRowStart == 1);

    std::optional<size_t> columnPosition = findColumnPositionForDefiniteRowItem(normalizedRowStart, normalizedRowEnd, columnSpan, autoFlowOptions);

    if (!columnPosition) {
        growGridColumnsToFit(columnSpan, normalizedRowStart, normalizedRowEnd);

        // Retry finding position in the grown grid
        columnPosition = findColumnPositionForDefiniteRowItem(normalizedRowStart, normalizedRowEnd, columnSpan, autoFlowOptions);
        ASSERT(columnPosition);
        ASSERT(isCellRangeEmpty(*columnPosition, *columnPosition + columnSpan, normalizedRowStart, normalizedRowEnd));
    }

    insertItemInArea(unplacedGridItem, *columnPosition, *columnPosition + columnSpan, normalizedRowStart, normalizedRowEnd);

    if (autoFlowOptions.strategy != PackingStrategy::Dense) {
        for (size_t row = normalizedRowStart; row < normalizedRowEnd; ++row)
            m_rowCursors.set(row, *columnPosition + columnSpan);
    }
}

// https://drafts.csswg.org/css-grid-1/#auto-placement-algo
// Step 3: Determine the columns in the implicit grid.
void ImplicitGrid::determineImplicitGridColumns(const Vector<UnplacedGridItem>& autoPositionedItems)
{
    size_t requiredColumns = columnsCount();

    // Part 1: "Among all the items with a definite column position, add columns to the end
    // of the implicit grid as necessary to accommodate those items."
    for (const auto& item : autoPositionedItems) {
        if (item.hasDefiniteColumnPosition()) {
            auto [columnStart, columnEnd] = item.normalizedColumnStartEnd();
            requiredColumns = std::max(requiredColumns, columnEnd);
        }
    }

    // Part 2: "If the largest column span among all the items without a definite column position
    // is larger than the width of the implicit grid, add columns to accommodate that column span."
    size_t maxColumnSpan = 0;
    for (const auto& item : autoPositionedItems) {
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
void ImplicitGrid::insertAutoPositionedItems(const Vector<UnplacedGridItem>& autoPositionedItems, GridAutoFlowOptions autoFlowOptions)
{
    if (autoFlowOptions.direction != GridAutoFlowDirection::Row) {
        ASSERT_NOT_IMPLEMENTED_YET();
        return;
    }

    // Process each auto-positioned item with the appropriate search strategy
    for (const auto& item : autoPositionedItems) {
        // Multi-span items should be blocked by coverage check in LayoutIntegrationGridCoverage.cpp
        ASSERT(item.rowSpanSize() == 1 && item.columnSpanSize() == 1);

        if (item.hasDefiniteColumnPosition())
            placeAutoPositionedItemWithDefiniteColumn(item, autoFlowOptions);
        else
            placeAutoPositionedItemWithAutoColumnAndRow(item, autoFlowOptions);
    }
}

std::optional<size_t> ImplicitGrid::findFirstAvailableColumnPosition(size_t rowStart, size_t rowEnd, size_t columnSpan, size_t startSearchColumn) const
{
    auto currentColumnsCount = columnsCount();

    // If we can't fit the span starting from the search position, signal that we need to grow the grid
    if (startSearchColumn + columnSpan > currentColumnsCount)
        return std::nullopt;

    // Search within existing grid bounds
    for (size_t columnStart = startSearchColumn; columnStart <= currentColumnsCount - columnSpan; ++columnStart) {
        if (isCellRangeEmpty(columnStart, columnStart + columnSpan, rowStart, rowEnd))
            return columnStart;
    }
    // If we are unable to find a valid position, signal that we need to grow the grid.
    return std::nullopt;
}
std::optional<size_t> ImplicitGrid::findColumnPositionForDefiniteRowItem(size_t normalizedRowStart, size_t normalizedRowEnd, size_t columnSpan, GridAutoFlowOptions autoFlowOptions) const
{
    if (autoFlowOptions.strategy == PackingStrategy::Dense) {
        // Dense packing: always start searching from column 0
        return findFirstAvailableColumnPosition(normalizedRowStart, normalizedRowEnd, columnSpan, 0);
    }
    // Sparse packing: use per-row cursors to maintain placement order
    // For multi-row items, use the maximum cursor position across all spanned rows
    ASSERT(autoFlowOptions.strategy == PackingStrategy::Sparse);
    size_t startSearchColumn = 0;
    for (size_t row = normalizedRowStart; row < normalizedRowEnd; ++row)
        startSearchColumn = std::max(startSearchColumn, m_rowCursors.get(row));
    return findFirstAvailableColumnPosition(normalizedRowStart, normalizedRowEnd, columnSpan, startSearchColumn);
}

void ImplicitGrid::growGridColumnsToFit(size_t columnSpan, size_t normalizedRowStart, size_t normalizedRowEnd)
{
    auto currentColumnsCount = columnsCount();

    // Find the last occupied column in the spanned rows
    size_t lastOccupiedColumn = 0;
    for (size_t row = normalizedRowStart; row < normalizedRowEnd; ++row) {
        for (size_t column = currentColumnsCount; column > 0; --column) {
            if (!m_gridMatrix[row][column - 1].isEmpty()) {
                lastOccupiedColumn = std::max(lastOccupiedColumn, column - 1);
                break;
            }
        }
    }

    size_t minimumColumnsNeeded = lastOccupiedColumn + 1 + columnSpan;
    for (auto& row : m_gridMatrix)
        row.resize(minimumColumnsNeeded);
}

bool ImplicitGrid::isCellRangeEmpty(size_t columnStart, size_t columnEnd, size_t rowStart, size_t rowEnd) const
{
    for (size_t row = rowStart; row < rowEnd; ++row) {
        for (size_t column = columnStart; column < columnEnd; ++column) {
            if (!m_gridMatrix[row][column].isEmpty())
                return false;
        }
    }
    return true;
}

void ImplicitGrid::insertItemInArea(const UnplacedGridItem& unplacedGridItem, size_t columnStart, size_t columnEnd, size_t rowStart, size_t rowEnd)
{
    for (size_t row = rowStart; row < rowEnd; ++row) {
        for (size_t column = columnStart; column < columnEnd; ++column)
            m_gridMatrix[row][column].append(unplacedGridItem);
    }
}

void ImplicitGrid::growColumnsToFit(size_t requiredCount)
{
    if (requiredCount > columnsCount()) {
        for (auto& row : m_gridMatrix)
            row.resize(requiredCount);
    }
}

void ImplicitGrid::growRowsToFit(size_t requiredRowIndex)
{
    while (requiredRowIndex >= rowsCount())
        m_gridMatrix.append(Vector<GridCell>(columnsCount()));
}

// FIXME: optimize cursor setting by setting to an empty slot instead of to the start for dense placement.
void ImplicitGrid::placeAutoPositionedItemWithDefiniteColumn(const UnplacedGridItem& item, GridAutoFlowOptions autoFlowOptions)
{
    ASSERT(item.hasDefiniteColumnPosition());
    ASSERT(!item.hasDefiniteRowPosition());

    // Items with definite column position and auto row position
    // Search vertically down the specified column.
    auto [normalizedColumnStart, normalizedColumnEnd] = item.normalizedColumnStartEnd();
    auto rowSpan = item.rowSpanSize();

    if (autoFlowOptions.strategy == PackingStrategy::Dense) {
        // Set the row position of the cursor to the start-most row line in the implicit grid.
        m_autoPlacementCursorRow = 0;
    } else {
        // Sparse packing: Check if we would be going backwards (to earlier column)
        // If so, advance the row count to avoid backtracking.
        if (normalizedColumnStart < m_autoPlacementCursorColumn)
            ++m_autoPlacementCursorRow;
    }

    // "Set the column position of the cursor to the grid item's column-start line."
    m_autoPlacementCursorColumn = normalizedColumnStart;

    // Increment the cursor's row position until a value is found where the grid item
    // does not overlap any occupied grid cells (creating new rows in the implicit grid as necessary).
    while (true) {
        growRowsToFit(m_autoPlacementCursorRow + rowSpan - 1);

        if (isCellRangeEmpty(normalizedColumnStart, normalizedColumnEnd, m_autoPlacementCursorRow, m_autoPlacementCursorRow + rowSpan)) {
            // Set the item's row-start line to the cursor's row position.
            insertItemInArea(item, normalizedColumnStart, normalizedColumnEnd,
                m_autoPlacementCursorRow, m_autoPlacementCursorRow + rowSpan);
            // Cursor remains at the placed position (row at placed row, column was already set).
            break;
        }

        // Try next row down this column.
        ++m_autoPlacementCursorRow;
    }
}

// FIXME: optimize cursor setting by setting to an empty slot instead of to the start for dense placement.
void ImplicitGrid::placeAutoPositionedItemWithAutoColumnAndRow(const UnplacedGridItem& item, GridAutoFlowOptions autoFlowOptions)
{
    ASSERT(!item.hasDefiniteColumnPosition() && !item.hasDefiniteRowPosition());

    auto rowSpan = item.rowSpanSize();
    auto columnSpan = item.columnSpanSize();

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
        if (isCellRangeEmpty(m_autoPlacementCursorColumn, m_autoPlacementCursorColumn + columnSpan, m_autoPlacementCursorRow, m_autoPlacementCursorRow + rowSpan)) {
            insertItemInArea(item, m_autoPlacementCursorColumn, m_autoPlacementCursorColumn + columnSpan,
                m_autoPlacementCursorRow, m_autoPlacementCursorRow + rowSpan);
            // Sparse packing: Advance cursor past this item to maintain document order.
            // Spec: "Set the auto-placement cursor to the end of the item's grid area."
            // Dense packing: Cursor will be reset to (0, 0) before the next fully-auto item.
            if (autoFlowOptions.strategy == PackingStrategy::Sparse)
                m_autoPlacementCursorColumn += columnSpan;
            return;
        }
        // Spec: "Increment the column position of the auto-placement cursor."
        ++m_autoPlacementCursorColumn;
    }
}

} // namespace Layout
} // namespace WebCore
