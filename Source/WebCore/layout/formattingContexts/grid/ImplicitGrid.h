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
#include <wtf/HashMap.h>
#include <wtf/Vector.h>

namespace WebCore {

namespace Layout {

struct GridAutoFlowOptions;
struct LeadingImplicitTracks;
struct UnplacedGridItems;

// https://drafts.csswg.org/css-grid-1/#implicit-grids
class ImplicitGrid {
public:
    // Builds the implicit grid that the grid item placement algorithm starts from: the explicit grid,
    // grown to cover every definite item placement that falls outside of it and wide enough for the
    // largest column span among the items without a definite column position.
    static ImplicitGrid createInitialGrid(const UnplacedGridItems&, LeadingImplicitTracks, size_t explicitColumnsCount, size_t explicitRowsCount);

    ImplicitGrid(size_t totalColumnsCount, size_t totalRowsCount);

    size_t rowsCount() const { return m_gridMatrix.size(); }
    size_t columnsCount() const { return m_columnsCount; }

    GridAreaLines insertUnplacedGridItem(const UnplacedGridItem&);
    GridAreaLines insertDefiniteRowItem(const UnplacedGridItem&, GridAutoFlowOptions);
    void determineImplicitGridColumns(const Vector<UnplacedGridItem>&);
    GridAreaLines insertAutoPositionedItem(const UnplacedGridItem&, GridAutoFlowOptions);

private:
    using RowCursors = HashMap<size_t, size_t, WTF::DefaultHash<size_t>, WTF::UnsignedWithZeroKeyHashTraits<size_t>>;
    std::optional<size_t> NODELETE findFirstAvailableColumnPosition(WTF::Range<size_t> rowRange, size_t columnSpan, size_t startSearchColumn) const;
    std::optional<size_t> findColumnPositionForDefiniteRowItem(WTF::Range<size_t> rowRange, size_t columnSpan, GridAutoFlowOptions) const;
    void growColumnsForDefiniteRowItem(size_t columnSpan, WTF::Range<size_t> rowRange);
    bool NODELETE isCellRangeEmpty(WTF::Range<size_t> columnRange, WTF::Range<size_t> rowRange) const;
    GridAreaLines markAreaAsOccupied(WTF::Range<size_t> columnRange, WTF::Range<size_t> rowRange);

    // Helper functions for auto-positioned items
    void growColumnsToFit(size_t requiredCount);
    void growRowsToFit(size_t requiredRowIndex);
    GridAreaLines placeAutoPositionedItemWithDefiniteColumn(const UnplacedGridItem&, GridAutoFlowOptions);
    GridAreaLines placeAutoPositionedItemWithAutoColumnAndRow(const UnplacedGridItem&, GridAutoFlowOptions);

    GridMatrix m_gridMatrix;

    // The width of every row. BitVector::size() reports the inline bit capacity rather than the
    // number of bits asked for, so the grid has to remember how wide it actually is.
    size_t m_columnsCount { 0 };

    // Per-row cursors for sparse packing in Step 2 (definite row items only).
    RowCursors m_rowCursors;

    // Global cursor for Step 4 (auto-positioned items with both axes automatic).
    // Tracks the current insertion point as (row, column) to ensure monotonic placement.
    size_t m_autoPlacementCursorRow { 0 };
    size_t m_autoPlacementCursorColumn { 0 };
};

} // namespace Layout

} // namespace WebCore
