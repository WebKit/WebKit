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
{
}

void ImplicitGrid::insertUnplacedGridItem(const UnplacedGridItem& unplacedGridItem)
{
    // https://drafts.csswg.org/css-grid/#common-uses-numeric
    // Grid positions have already been normalized to non-negative matrix indices.
    auto [columnStart, columnEnd] = unplacedGridItem.normalizedColumnStartEnd();
    auto [rowStart, rowEnd] = unplacedGridItem.normalizedRowStartEnd();

    // FIXME: Support explicit items spanning multiple columns.
    // Multi-cell items (spanning multiple columns) are not yet supported.
    if (columnEnd - columnStart > 1) {
        ASSERT_NOT_IMPLEMENTED_YET();
        return;
    }

    // FIXME: Support explicit items spanning multiple rows.
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
    // FIXME: Support multi-row spans.
    ASSERT(normalizedRowEnd - normalizedRowStart == 1);

    auto columnPosition = findColumnPositionForDefiniteRowItem(normalizedRowStart, normalizedRowEnd, columnSpan, autoFlowOptions.strategy);

    if (!columnPosition) {
        growGridColumnsToFit(columnSpan, normalizedRowStart, normalizedRowEnd);

        // Retry finding position in the grown grid
        columnPosition = findColumnPositionForDefiniteRowItem(normalizedRowStart, normalizedRowEnd, columnSpan, autoFlowOptions.strategy);
#ifndef NDEBUG
        ASSERT(columnPosition);
        ASSERT(isCellRangeEmpty(*columnPosition, *columnPosition + columnSpan, normalizedRowStart, normalizedRowEnd));
#endif
    }

    insertItemInArea(unplacedGridItem, *columnPosition, *columnPosition + columnSpan, normalizedRowStart, normalizedRowEnd);

    if (autoFlowOptions.strategy != PackingStrategy::Dense) {
        for (size_t row = normalizedRowStart; row < normalizedRowEnd; ++row)
            m_rowCursors.set(row, *columnPosition + columnSpan);
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

std::optional<size_t> ImplicitGrid::findColumnPositionForDefiniteRowItem(size_t rowStart, size_t rowEnd, size_t columnSpan, PackingStrategy strategy) const
{
    size_t startSearchColumn = 0;

    if (strategy == PackingStrategy::Sparse) {
        // Use per-row cursors to maintain placement order
        for (size_t row = rowStart; row < rowEnd; ++row)
            startSearchColumn = std::max(startSearchColumn, m_rowCursors.get(row));
    }

    return findFirstAvailableColumnPosition(rowStart, rowEnd, columnSpan, startSearchColumn);
}

void ImplicitGrid::insertItemInArea(const UnplacedGridItem& unplacedGridItem, size_t columnStart, size_t columnEnd, size_t rowStart, size_t rowEnd)
{
    for (size_t row = rowStart; row < rowEnd; ++row) {
        for (size_t column = columnStart; column < columnEnd; ++column)
            m_gridMatrix[row][column].append(unplacedGridItem);
    }
}

std::optional<GridPosition> ImplicitGrid::findFirstAvailablePosition(size_t rowSpan, size_t columnSpan, size_t startRow, size_t startColumn) const
{
    // Search for the first available position starting from (startRow, startColumn)
    // following row-major order (left-to-right, top-to-bottom).
    // This implements the auto-placement cursor search for grid-auto-flow: row.

    auto currentColumnsCount = columnsCount();
    auto currentRowsCount = rowsCount();

    // Search through existing grid rows
    for (size_t row = startRow; row < currentRowsCount; ++row) {
        // For the starting row, begin search from startColumn
        // For subsequent rows, start from column 0
        size_t searchStartColumn = (row == startRow) ? startColumn : 0;

        // Try each column position in this row
        for (size_t column = searchStartColumn; column < currentColumnsCount; ++column) {
            // Check if item fits within current grid bounds
            if (column + columnSpan <= currentColumnsCount && row + rowSpan <= currentRowsCount) {
                if (isCellRangeEmpty(column, column + columnSpan, row, row + rowSpan))
                    return GridPosition { row, column };
            }
        }
    }

    // No position found in existing grid
    return std::nullopt;
}

void ImplicitGrid::growGridToSize(size_t newColumnsCount, size_t newRowsCount)
{
    auto targetColumnsCount = std::max(columnsCount(), newColumnsCount);

    // Grow columns in existing rows if needed
    if (targetColumnsCount > columnsCount()) {
        for (auto& row : m_gridMatrix)
            row.resize(targetColumnsCount);
    }

    // Grow rows if needed (with correct column count)
    while (m_gridMatrix.size() < newRowsCount)
        m_gridMatrix.append(Vector<GridCell>(targetColumnsCount));
}

GridPosition ImplicitGrid::placeAutoPositionedItem(const UnplacedGridItem& item)
{
    auto rowSpan = item.rowSpanSize();
    auto columnSpan = item.columnSpanSize();

    // FIXME: Support multi-span items
    // Multi-span items should be blocked by coverage check in LayoutIntegrationGridCoverage.cpp
    if (rowSpan != 1 || columnSpan != 1) {
        ASSERT_NOT_IMPLEMENTED_YET();
        RELEASE_ASSERT_NOT_REACHED();
    }

    // Try to find an available position starting from the cursor
    auto position = findFirstAvailablePosition(rowSpan, columnSpan, m_autoPlacementCursor.row, m_autoPlacementCursor.column);

    if (!position) {
        // No position found in existing grid - need to grow the grid
        // Add a new implicit row and place item at the start of that row
        auto newRowIndex = rowsCount();
        auto newColumnsCount = std::max(columnsCount(), columnSpan);
        growGridToSize(newColumnsCount, newRowIndex + rowSpan);

        // Per CSS Grid spec, when no position is found in existing rows,
        // add a new implicit row at the end and place the item there.
        // https://drafts.csswg.org/css-grid-1/#auto-placement-algo step 4.1.2.3
        position = GridPosition { newRowIndex, 0 };

        // Verify the position is empty after grid growth
        ASSERT(isCellRangeEmpty(0, columnSpan, newRowIndex, newRowIndex + rowSpan));
    }

    insertItemInArea(item, position->column, position->column + columnSpan, position->row, position->row + rowSpan);

    return *position;
}

void ImplicitGrid::insertAutoPositionedItems(const Vector<UnplacedGridItem>& autoPositionedItems, GridAutoFlowOptions autoFlowOptions)
{
    // CSS Grid Spec Section 8.5 Step 3: Position the remaining grid items
    // https://drafts.csswg.org/css-grid-1/#auto-placement-algo
    // FIXME: Step 4.1.2.1/4.1.2.2 (items with definite column position, auto row)
    // are not handled; we only support fully auto-positioned items here.

    if (autoFlowOptions.direction != GridAutoFlowDirection::Row) {
        // FIXME: Support grid-auto-flow: column.
        ASSERT_NOT_IMPLEMENTED_YET();
        return;
    }

    m_autoPlacementCursor.reset();

    for (const auto& item : autoPositionedItems) {
        if (item.rowSpanSize() != 1 || item.columnSpanSize() != 1) {
            // FIXME: Support auto-placement for spanning items.
            ASSERT_NOT_IMPLEMENTED_YET();
            continue;
        }

        auto position = placeAutoPositionedItem(item);

        // Update cursor based on packing strategy
        if (autoFlowOptions.strategy == PackingStrategy::Dense)
            m_autoPlacementCursor.reset();
        else
            m_autoPlacementCursor.advancePast(position.row, position.column + item.columnSpanSize(), columnsCount());
    }
}

} // namespace Layout
} // namespace WebCore
