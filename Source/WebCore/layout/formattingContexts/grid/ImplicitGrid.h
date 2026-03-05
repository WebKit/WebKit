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

// https://drafts.csswg.org/css-grid-1/#implicit-grids
class ImplicitGrid {
public:
    ImplicitGrid(size_t totalColumnsCount, size_t totalRowsCount);

    size_t rowsCount() const { return m_gridMatrix.size(); }
    size_t columnsCount() const { return rowsCount() ? m_gridMatrix[0].size() : m_initialColumnsCount; }

    void insertUnplacedGridItem(const UnplacedGridItem&);
    void insertDefiniteRowItem(const UnplacedGridItem&, GridAutoFlowOptions);
    void determineImplicitGridColumns(const Vector<UnplacedGridItem>&);
    void insertAutoPositionedItems(const Vector<UnplacedGridItem>&, GridAutoFlowOptions);

    GridAreas gridAreas() const;

private:
    using RowCursors = HashMap<size_t, size_t, WTF::DefaultHash<size_t>, WTF::UnsignedWithZeroKeyHashTraits<size_t>>;
    std::optional<size_t> NODELETE findFirstAvailableColumnPosition(size_t rowStart, size_t rowEnd, size_t columnSpan, size_t startSearchColumn) const;
    std::optional<size_t> findColumnPositionForDefiniteRowItem(size_t normalizedRowStart, size_t normalizedRowEnd, size_t columnSpan, GridAutoFlowOptions) const;
    void growGridColumnsToFit(size_t columnSpan, size_t normalizedRowStart, size_t normalizedRowEnd);
    bool NODELETE isCellRangeEmpty(size_t columnStart, size_t columnEnd, size_t rowStart, size_t rowEnd) const;
    void insertItemInArea(const UnplacedGridItem&, size_t columnStart, size_t columnEnd, size_t rowStart, size_t rowEnd);

    // Helper functions for auto-positioned items
    void growColumnsToFit(size_t requiredCount);
    void growRowsToFit(size_t requiredRowIndex);
    void placeAutoPositionedItemWithDefiniteColumn(const UnplacedGridItem&, GridAutoFlowOptions);
    void placeAutoPositionedItemWithAutoColumnAndRow(const UnplacedGridItem&, GridAutoFlowOptions);

    GridMatrix m_gridMatrix;

    // Track column count. This is needed when the initial grid has 0 rows and the column
    // count would otherwise be lost.
    size_t m_initialColumnsCount { 0 };

    // Per-row cursors for sparse packing in Step 2 (definite row items only).
    RowCursors m_rowCursors;

    // Global cursor for Step 4 (auto-positioned items with both axes automatic).
    // Tracks the current insertion point as (row, column) to ensure monotonic placement.
    size_t m_autoPlacementCursorRow { 0 };
    size_t m_autoPlacementCursorColumn { 0 };
};

} // namespace Layout

} // namespace WebCore
