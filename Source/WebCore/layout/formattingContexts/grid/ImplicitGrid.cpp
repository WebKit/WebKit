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
#include <wtf/Range.h>

namespace WebCore {
namespace Layout {

// The implicit grid is created from the explicit grid + items that are placed outside
// of the explicit grid. Since we know the explicit tracks from style we start the
// implicit grid as exactly the explicit grid and allow placement to add implicit
// tracks and grow the grid.
ImplicitGrid::ImplicitGrid(size_t gridTemplateColumnsCount, size_t gridTemplateRowsCount)
    : m_gridMatrix(Vector(gridTemplateRowsCount, Vector<GridCell>(gridTemplateColumnsCount)))
{
}

void ImplicitGrid::insertUnplacedGridItem(const UnplacedGridItem& unplacedGridItem)
{
    // https://drafts.csswg.org/css-grid/#common-uses-numeric
    auto explicitColumnStart = unplacedGridItem.explicitColumnStart();
    auto explicitColumnEnd = unplacedGridItem.explicitColumnEnd();
    if (explicitColumnStart < 0 || explicitColumnEnd < 0) {
        ASSERT_NOT_IMPLEMENTED_YET();
        return;
    }

    if (explicitColumnEnd <= explicitColumnStart) {
        ASSERT_NOT_IMPLEMENTED_YET();
        return;
    }

    auto columnsCount = static_cast<int>(this->columnsCount());
    if (explicitColumnStart > columnsCount || explicitColumnEnd > columnsCount) {
        ASSERT_NOT_IMPLEMENTED_YET();
        return;
    }

    auto explicitRowStart = unplacedGridItem.explicitRowStart();
    auto explicitRowEnd = unplacedGridItem.explicitRowEnd();
    if (explicitRowStart < 0 || explicitRowEnd < 0) {
        ASSERT_NOT_IMPLEMENTED_YET();
        return;
    }

    if (explicitRowEnd <= explicitRowStart) {
        ASSERT_NOT_IMPLEMENTED_YET();
        return;
    }

    auto rowsCount = static_cast<int>(this->rowsCount());
    if (explicitRowStart > rowsCount || explicitRowEnd > rowsCount) {
        ASSERT_NOT_IMPLEMENTED_YET();
        return;
    }

    if (explicitColumnEnd - explicitColumnStart > 1) {
        ASSERT_NOT_IMPLEMENTED_YET();
        return;
    }

    if (explicitRowEnd - explicitRowStart > 1) {
        ASSERT_NOT_IMPLEMENTED_YET();
        return;
    }

    auto columnsRange = WTF::Range(explicitColumnStart, explicitColumnEnd);
    auto rowsRange = WTF::Range(explicitRowStart, explicitRowEnd);
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

void ImplicitGrid::insertDefiniteRowItem(const UnplacedGridItem& unplacedGridItem, GridAutoFlowOptions autoFlowOptions, HashMap<unsigned, unsigned, DefaultHash<unsigned>, WTF::UnsignedWithZeroKeyHashTraits<unsigned>>* rowCursors)
{
    // Step 2 of CSS Grid auto-placement algorithm:
    // Process items locked to a given row (definite row position, auto column position)
    // See: https://www.w3.org/TR/css-grid-1/#auto-placement-algo

    auto columnSpan = unplacedGridItem.columnSpanSize();
    // FIXME: Support multi-column spans
    ASSERT(columnSpan == 1);

    ASSERT(unplacedGridItem.hasDefiniteRowPosition() && !unplacedGridItem.hasDefiniteColumnPosition());
    auto [rowStart, rowEnd] = unplacedGridItem.definiteRowStartEnd();
    // FIXME: Support multi-row spans
    ASSERT(rowEnd - rowStart == 1);

    if (rowStart < 0 || rowEnd <= rowStart || rowEnd > static_cast<int>(rowsCount())) {
        ASSERT_NOT_IMPLEMENTED_YET();
        // FIXME: Handle implicit grid growth for items outside explicit grid
        // FIXME Handle inverted row ranges (rowEnd <= rowStart)
        return;
    }

    std::optional<size_t> columnPosition;
    if (autoFlowOptions.strategy == PackingStrategy::Dense) {
        // Dense packing: always start searching from column 0
        columnPosition = findFirstAvailableColumnPosition(rowStart, rowEnd, columnSpan, 0);
    } else {
        // Sparse packing: use per-row cursors to maintain placement order
        // For multi-row items, use the maximum cursor position across all spanned rows
        size_t startSearchColumn = 0;
        for (int row = rowStart; row < rowEnd; ++row)
            startSearchColumn = std::max(startSearchColumn, static_cast<size_t>(rowCursors->get(row)));
        columnPosition = findFirstAvailableColumnPosition(rowStart, rowEnd, columnSpan, startSearchColumn);
    }

    if (!columnPosition) {
        ASSERT_NOT_IMPLEMENTED_YET();
        // FIXME: Handle implicit grid growth when no space is found within current grid
        return;
    }

    insertItemInArea(unplacedGridItem, *columnPosition, *columnPosition + columnSpan, rowStart, rowEnd);

    if (autoFlowOptions.strategy != PackingStrategy::Dense) {
        for (int row = rowStart; row < rowEnd; ++row)
            rowCursors->set(row, static_cast<unsigned>(*columnPosition + columnSpan));
    }
}

std::optional<size_t> ImplicitGrid::findFirstAvailableColumnPosition(int rowStart, int rowEnd, size_t columnSpan, size_t startSearchColumn) const
{
    auto currentColumnsCount = columnsCount();

    // Validate we can fit the span within the grid
    if (columnSpan > currentColumnsCount)
        ASSERT_NOT_IMPLEMENTED_YET();

    // Search within existing grid bounds
    for (size_t columnStart = startSearchColumn; columnStart <= currentColumnsCount - columnSpan; ++columnStart) {
        if (isCellRangeEmpty(columnStart, columnStart + columnSpan, rowStart, rowEnd))
            return columnStart;
    }
    // FIXME: Support implicit grid growth
    return std::nullopt;
}

bool ImplicitGrid::isCellRangeEmpty(size_t columnStart, size_t columnEnd, int rowStart, int rowEnd) const
{
    for (int row = rowStart; row < rowEnd; ++row) {
        for (size_t column = columnStart; column < columnEnd; ++column) {
            if (!m_gridMatrix[row][column].isEmpty())
                return false;
        }
    }
    return true;
}

void ImplicitGrid::insertItemInArea(const UnplacedGridItem& unplacedGridItem, size_t columnStart, size_t columnEnd, int rowStart, int rowEnd)
{
    for (int row = rowStart; row < rowEnd; ++row) {
        for (size_t column = columnStart; column < columnEnd; ++column)
            m_gridMatrix[row][column].append(unplacedGridItem);
    }
}

} // namespace Layout
} // namespace WebCore
