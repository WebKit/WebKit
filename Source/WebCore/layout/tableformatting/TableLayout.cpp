/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "TableFormattingContext.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "DisplayBox.h"
#include "LayoutBox.h"

namespace WebCore {
namespace Layout {

// https://www.w3.org/TR/css-tables-3/#table-layout-algorithm
TableFormattingContext::TableLayout::TableLayout(const TableFormattingContext& formattingContext, const TableGrid& grid)
    : m_formattingContext(formattingContext)
    , m_grid(grid)
{
}

struct ColumnSpan {
    static size_t hasSpan(const TableGrid::Slot& slot) { return slot.hasColumnSpan(); }
    static size_t isSpanned(const TableGrid::Slot& slot) { return slot.isColumnSpanned(); }

    static size_t spanCount(const TableGrid::Cell& cell) { return cell.columnSpan(); }
    static size_t startSpan(const TableGrid::Cell& cell) { return cell.startColumn(); }
    static size_t endSpan(const TableGrid::Cell& cell) { return cell.endColumn(); }

    static size_t index(size_t columnIndex, size_t /*rowIndex*/) { return columnIndex; }
    static size_t size(const TableGrid& grid) { return grid.columns().size(); }

    static LayoutUnit spacing(const TableGrid& grid) { return grid.horizontalSpacing(); }
};

struct RowSpan {
    static size_t hasSpan(const TableGrid::Slot& slot) { return slot.hasRowSpan(); }
    static size_t isSpanned(const TableGrid::Slot& slot) { return slot.isRowSpanned(); }

    static size_t spanCount(const TableGrid::Cell& cell) { return cell.rowSpan(); }
    static size_t startSpan(const TableGrid::Cell& cell) { return cell.startRow(); }
    static size_t endSpan(const TableGrid::Cell& cell) { return cell.endRow(); }

    static size_t index(size_t /*columnIndex*/, size_t rowIndex) { return rowIndex; }
    static size_t size(const TableGrid& grid) { return grid.rows().size(); }

    static LayoutUnit spacing(const TableGrid& grid) { return grid.verticalSpacing(); }
};

struct GridSpace {
    bool isEmpty() const { return !value; }

    // Initial width/height for column/row we start the distribution width (usually a minumum width).
    float value { 0 };
    // The base to compute the distribution ratio. It normally matches the [value] but in some cases we use the maximum value to distribute the extra space. 
    float distributionBase { 0 };
};

inline static GridSpace max(const GridSpace& a, const GridSpace& b)
{
    return { std::max(a.value, b.value), std::max(a.distributionBase, b.distributionBase) };
}

inline static GridSpace& operator-(GridSpace& a, const GridSpace& b)
{
    a.value = std::max(0.0f, a.value - b.value);
    a.distributionBase = std::max(0.0f, a.distributionBase - b.distributionBase);
    return a;
}

inline static GridSpace& operator+=(GridSpace& a, const GridSpace& b)
{
    a.value += b.value;
    a.distributionBase += b.distributionBase;
    return a;
}

inline static GridSpace& operator-=(GridSpace& a, const GridSpace& b)
{
    return a - b;
}

inline static GridSpace& operator/(GridSpace& a, unsigned value)
{
    a.value /= value;
    a.distributionBase /= value;
    return a;
}

template <typename SpanType>
static Vector<LayoutUnit> distributeAvailableSpace(const TableGrid& grid, LayoutUnit availableSpace, const WTF::Function<GridSpace(const TableGrid::Slot&, size_t)>& slotSpace)
{
    struct ResolvedItem {
        GridSpace slotSpace;
        bool isFixed { false };
    };

    auto& columns = grid.columns();
    auto& rows = grid.rows();
    // 1. Collect the non-spanning spaces first. They are used for the final distribution as well as for distributing the spanning space.
    Vector<Optional<ResolvedItem>> resolvedItems(SpanType::size(grid));
    for (size_t columnIndex = 0; columnIndex < columns.size(); ++columnIndex) {
        for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
            auto& slot = *grid.slot({ columnIndex, rowIndex });
            if (SpanType::hasSpan(slot) || SpanType::isSpanned(slot))
                continue;
            auto index = SpanType::index(columnIndex, rowIndex);
            if (!resolvedItems[index])
                resolvedItems[index] = ResolvedItem { };
            resolvedItems[index]->slotSpace = max(resolvedItems[index]->slotSpace, slotSpace(slot, index));
        }
    }

    // 2. Collect the spanning cells.
    struct SpanningCell {
        SlotPosition position;
        GridSpace unresolvedSpace;
    };
    Vector<SpanningCell> spanningCells;
    for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
        for (size_t columnIndex = 0; columnIndex < columns.size(); ++columnIndex) {
            auto& slot = *grid.slot({ columnIndex, rowIndex });
            if (SpanType::hasSpan(slot))
                spanningCells.append({ { columnIndex, rowIndex }, slotSpace(slot, SpanType::index(columnIndex, rowIndex)) });
        }
    }
    // We need these spanning cells in the order of the number of columns/rows they span so that
    // we can resolve overlapping spans starting with the shorter ones e.g.
    // <td colspan=4>#a</td><td>#b</td>
    // <td colspan=2>#c</td><td colspan=3>#d</td>
    std::sort(spanningCells.begin(), spanningCells.end(), [&] (auto& a, auto& b) {
        return SpanType::spanCount(grid.slot(a.position)->cell()) < SpanType::spanCount(grid.slot(b.position)->cell());
    });

    // 3. Distribute the spanning cells' mimimum space across the columns/rows using the non-spanning spaces.
    // e.g. [ 1 ][ 5 ][ 1 ]
    //      [    9   ][ 1 ]
    // The initial widths are: [ 2 ][ 7 ][ 1 ]
    for (auto spanningCell : spanningCells) {
        auto& cell = grid.slot(spanningCell.position)->cell();
        auto unresolvedSpanningSpace = spanningCell.unresolvedSpace;
        if (!resolvedItems[SpanType::startSpan(cell)] || !resolvedItems[SpanType::endSpan(cell) - 1]) {
            // <td colspan=4>#a</td><td>#b</td>
            // <td colspan=2>#c</td><td colspan=3>#d</td>
            // Unresolved columns are: 1 2 3 4
            // 1. Take colspan=2 (shortest span) and resolve column 1 and 2
            // 2. Take colspan=3 and resolve column 3 and 4 (5 is resolved because it already has a non-spanning cell).
            // 3. colspan=4 needs no resolving because all the spanned columns (1 2 3 4) have already been resolved.
            auto unresolvedColumnCount = cell.columnSpan();
            for (auto spanIndex = SpanType::startSpan(cell); spanIndex < SpanType::endSpan(cell); ++spanIndex) {
                if (!resolvedItems[spanIndex])
                    continue;
                ASSERT(unresolvedColumnCount);
                --unresolvedColumnCount;
                unresolvedSpanningSpace -= resolvedItems[spanIndex]->slotSpace;
            }
            ASSERT(unresolvedColumnCount);
            auto equalSpaceForSpannedColumns = unresolvedSpanningSpace / unresolvedColumnCount;
            for (auto spanIndex = SpanType::startSpan(cell); spanIndex < SpanType::endSpan(cell); ++spanIndex) {
                if (resolvedItems[spanIndex])
                    continue;
                resolvedItems[spanIndex] = ResolvedItem { equalSpaceForSpannedColumns, false };
            }
        } else {
            // 1. Collect the non-spaning resolved spaces.
            // 2. Distribute the extra space among the spanned columns/rows based on the resolved space values.
            // e.g. spanning width: [   9   ]. Resolved widths for the spanned columns: [ 1 ] [ 2 ]
            // New resolved widths: [ 3 ] [ 6 ].
            auto resolvedSpanningSpace = GridSpace { };
            for (auto spanIndex = SpanType::startSpan(cell); spanIndex < SpanType::endSpan(cell); ++spanIndex)
                resolvedSpanningSpace += resolvedItems[spanIndex]->slotSpace;
            if (resolvedSpanningSpace.value >= unresolvedSpanningSpace.value) {
                // The spanning cell fits the spanned columns/rows just fine. Nothing to distribute.
                continue;
            }
            auto spacing = SpanType::spacing(grid) * (SpanType::spanCount(cell) - 1);
            auto spaceToDistribute = unresolvedSpanningSpace - GridSpace { spacing, spacing } - resolvedSpanningSpace; 
            if (!spaceToDistribute.isEmpty()) {
                auto distributionRatio = spaceToDistribute.distributionBase / resolvedSpanningSpace.distributionBase;
                for (auto spanIndex = SpanType::startSpan(cell); spanIndex < SpanType::endSpan(cell); ++spanIndex)
                    resolvedItems[spanIndex]->slotSpace += GridSpace { resolvedItems[spanIndex]->slotSpace.value * distributionRatio, resolvedItems[spanIndex]->slotSpace.distributionBase * distributionRatio};
            }
        }
    }
    // 4. Distribute the extra space using the final resolved widths.
#if ASSERT_ENABLED
    // We have to have all the spaces resolved at this point.
    for (auto& resolvedItem : resolvedItems)
        ASSERT(resolvedItem);
#endif
    // Fixed size cells don't participate in available space distribution.
    auto adjustabledSpace = GridSpace { };
    for (auto& resolvedItem : resolvedItems) {
        if (resolvedItem->isFixed)
            continue;
        adjustabledSpace += resolvedItem->slotSpace;
    }

    Vector<LayoutUnit> distributedSpaces(resolvedItems.size());
    float spaceToDistribute = availableSpace - adjustabledSpace.value - ((resolvedItems.size() + 1) * SpanType::spacing(grid));
    // Essentially the remaining space to distribute should never be negative. LayoutUnit::epsilon() is required to compensate for LayoutUnit's low precision.
    ASSERT(spaceToDistribute >= -LayoutUnit::epsilon() * resolvedItems.size());
    // Distribute the extra space based on the resolved spaces.
    auto distributionRatio = spaceToDistribute / adjustabledSpace.distributionBase;
    for (size_t index = 0; index < resolvedItems.size(); ++index) {
        auto slotSpace = resolvedItems[index]->slotSpace.value;
        auto needsSpaceDistribution = spaceToDistribute && !resolvedItems[index]->isFixed;
        distributedSpaces[index] = LayoutUnit { slotSpace };
        if (!needsSpaceDistribution)
            continue;
        distributedSpaces[index] += LayoutUnit { resolvedItems[index]->slotSpace.distributionBase * distributionRatio };
    }
    return distributedSpaces;
}

TableFormattingContext::TableLayout::DistributedSpaces TableFormattingContext::TableLayout::distributedHorizontalSpace(LayoutUnit availableHorizontalSpace)
{
    enum class ColumnWidthBalancingBase { MinimumWidth, MaximumWidth };
    auto columnWidthBalancingBase = availableHorizontalSpace >= m_grid.widthConstraints()->maximum ? ColumnWidthBalancingBase::MaximumWidth : ColumnWidthBalancingBase::MinimumWidth;
    return distributeAvailableSpace<ColumnSpan>(m_grid, availableHorizontalSpace, [&] (const TableGrid::Slot& slot, size_t columnIndex) {
        auto& column = m_grid.columns().list()[columnIndex];
        auto columnBoxFixedWidth = column.box() ? column.box()->columnWidth().valueOr(0_lu) : 0_lu;
        auto minimumWidth = std::max<float>(slot.widthConstraints().minimum, columnBoxFixedWidth);
        auto maximumWidth = std::max<float>(slot.widthConstraints().maximum, columnBoxFixedWidth);

        if (columnWidthBalancingBase == ColumnWidthBalancingBase::MinimumWidth) {
            ASSERT(maximumWidth >= minimumWidth);
            return GridSpace { minimumWidth, maximumWidth - minimumWidth };
        }
        // When the column has a fixed width cell, the maximum width balancing is based on the minimum width.
        if (column.isFixedWidth())
            return GridSpace { minimumWidth, maximumWidth };
        return GridSpace { maximumWidth, maximumWidth };
    });
}

TableFormattingContext::TableLayout::DistributedSpaces TableFormattingContext::TableLayout::distributedVerticalSpace(Optional<LayoutUnit> availableVerticalSpace)
{
    auto& rows = m_grid.rows();
    auto& columns = m_grid.columns();

    Vector<LayoutUnit> rowHeight(rows.size());
    auto tableUsedHeight = LayoutUnit { };
    // 2. Collect initial, baseline aligned row heights.
    for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
        auto maximumColumnAscent = InlineLayoutUnit { };
        auto maximumColumnDescent = InlineLayoutUnit { };
        for (size_t columnIndex = 0; columnIndex < columns.size(); ++columnIndex) {
            auto& slot = *m_grid.slot({ columnIndex, rowIndex });
            if (slot.isRowSpanned())
                continue;
            if (slot.hasRowSpan())
                continue;
            // The minimum height of a row (without spanning-related height distribution) is defined as the height of an hypothetical
            // linebox containing the cells originating in the row.
            auto& cell = slot.cell();
            maximumColumnAscent = std::max(maximumColumnAscent, cell.baselineOffset());
            maximumColumnDescent = std::max(maximumColumnDescent, formattingContext().geometryForBox(cell.box()).height() - cell.baselineOffset());
        }
        // <tr style="height: 10px"> is considered as min height.
        rowHeight[rowIndex] = maximumColumnAscent + maximumColumnDescent;
        tableUsedHeight += rowHeight[rowIndex];
    }
    // FIXME: Collect spanning row maximum heights.

    tableUsedHeight += (rows.size() + 1) * m_grid.verticalSpacing();
    auto availableSpace = std::max(availableVerticalSpace.valueOr(0_lu), tableUsedHeight);
    // Distribute extra space if the table is supposed to be taller than the sum of the row heights.
    return distributeAvailableSpace<RowSpan>(m_grid, availableSpace, [&] (const TableGrid::Slot& slot, size_t rowIndex) {
        if (slot.hasRowSpan())
            return GridSpace { formattingContext().geometryForBox(slot.cell().box()).height(), formattingContext().geometryForBox(slot.cell().box()).height() };
        auto& rows = m_grid.rows();
        auto computedRowHeight = formattingContext().geometry().computedHeight(rows.list()[rowIndex].box(), { });
        auto height = std::max<float>(rowHeight[rowIndex], computedRowHeight.valueOr(0_lu));
        return GridSpace { height, height };
    });
}

}
}

#endif
