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

#pragma once

#include "GridTypeAliases.h"
#include <wtf/Forward.h>
#include <wtf/Vector.h>

namespace WebCore {

namespace Layout {

enum class GridLayoutAlgorithm : uint8_t;
struct GridAutoFlowOptions;

struct GridPosition {
    size_t row { 0 };
    size_t column { 0 };
};

struct AutoPlacementCursor {
    size_t row { 0 };
    size_t column { 0 };

    void reset() { row = 0; column = 0; }
    void advancePast(size_t placedRow, size_t placedColumnEnd, size_t gridColumnsCount)
    {
        row = placedRow;
        column = placedColumnEnd;
        if (column >= gridColumnsCount) {
            row++;
            column = 0;
        }
    }
};

// https://drafts.csswg.org/css-grid-1/#implicit-grids
class ImplicitGrid {
public:
    ImplicitGrid(size_t totalColumnsCount, size_t totalRowsCount);

    size_t rowsCount() const { return m_gridMatrix.size(); }
    size_t columnsCount() const { return rowsCount() ? m_gridMatrix[0].size() : 0; }

    void insertUnplacedGridItem(const UnplacedGridItem&);
    void insertDefiniteRowItem(const UnplacedGridItem&, GridAutoFlowOptions);
    void insertAutoPositionedItems(const Vector<UnplacedGridItem>&, GridAutoFlowOptions);

    GridAreas gridAreas() const;

private:
    using RowCursors = HashMap<size_t, size_t, DefaultHash<size_t>, WTF::UnsignedWithZeroKeyHashTraits<size_t>>;

    std::optional<size_t> findFirstAvailableColumnPosition(size_t rowStart, size_t rowEnd, size_t columnSpan, size_t startSearchColumn) const;
    std::optional<GridPosition> findFirstAvailablePosition(size_t rowSpan, size_t columnSpan, size_t startRow, size_t startColumn) const;
    std::optional<size_t> findColumnPositionForDefiniteRowItem(size_t rowStart, size_t rowEnd, size_t columnSpan, PackingStrategy) const;
    bool isCellRangeEmpty(size_t columnStart, size_t columnEnd, size_t rowStart, size_t rowEnd) const;
    void insertItemInArea(const UnplacedGridItem&, size_t columnStart, size_t columnEnd, size_t rowStart, size_t rowEnd);
    void growGridToSize(size_t newColumnsCount, size_t newRowsCount);
    void growGridColumnsToFit(size_t columnSpan, size_t normalizedRowStart, size_t normalizedRowEnd);
    GridPosition placeAutoPositionedItem(const UnplacedGridItem&);

    GridMatrix m_gridMatrix;

    // Per-row cursors for Step 2 sparse packing (definite row items)
    RowCursors m_rowCursors;

    // Auto-placement cursor for Step 3 of the grid placement algorithm
    // https://drafts.csswg.org/css-grid-1/#auto-placement-cursor
    AutoPlacementCursor m_autoPlacementCursor;
};

} // namespace Layout

} // namespace WebCore
