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

#include "LayoutBox.h"
#include "LayoutBoxGeometry.h"
#include "RenderStyleInlines.h"
#include "TableFormattingGeometry.h"

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
    bool isEmpty() const { return !preferredSize; }

    enum class Type {
        Percent,
        Fixed,
        Relative,
        Auto
    };
    Type type { Type::Auto };
    // Preferred width/height for column/row we start the distribution with (usually the minimum content width).
    float preferredSize { 0 };
    // The base space value for the distribution computation e.g [ 9 ] [ 1 ] values for 2 columns in the table means that if
    // we've go 100px flexible space to distribute, 90px and 10px go to the columns respectively.
    float flexBase { 0 };
    float minimumSize { 0 };
};

inline static GridSpace& operator-(GridSpace& a, const GridSpace& b)
{
    a.preferredSize = std::max(0.0f, a.preferredSize - b.preferredSize);
    a.flexBase = std::max(0.0f, a.flexBase - b.flexBase);
    return a;
}

inline static GridSpace& operator+=(GridSpace& a, const GridSpace& b)
{
    a.preferredSize += b.preferredSize;
    a.flexBase += b.flexBase;
    return a;
}

inline static GridSpace& operator-=(GridSpace& a, const GridSpace& b)
{
    return a - b;
}

inline static GridSpace& operator/(GridSpace& a, unsigned value)
{
    a.preferredSize /= value;
    a.flexBase /= value;
    return a;
}

template <typename SpanType>
static Vector<LayoutUnit> distributeAvailableSpace(const TableGrid& grid, LayoutUnit availableSpace, const Function<GridSpace(const TableGrid::Slot&, size_t)>& slotSpace)
{
    auto& columns = grid.columns();
    auto& rows = grid.rows();
    Vector<std::optional<GridSpace>> resolvedItems(SpanType::size(grid));
    auto collectNonSpanningCells = [&] {
        // 1. Collect the non-spanning spaces first. They are used for the final distribution as well as for distributing the spanning space.
        for (size_t columnIndex = 0; columnIndex < columns.size(); ++columnIndex) {
            for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
                auto& slot = *grid.slot({ columnIndex, rowIndex });
                if (SpanType::hasSpan(slot) || SpanType::isSpanned(slot))
                    continue;
                auto index = SpanType::index(columnIndex, rowIndex);
                auto currentSpace = slotSpace(slot, index);
                if (!resolvedItems[index]) {
                    resolvedItems[index] = currentSpace;
                    continue;
                }
                auto& resolvedItem = resolvedItems[index];
                // FIXME: Add support for mixed column/row types e.g. first row first column is fixed, while second row first column is percent.
                ASSERT(resolvedItem->type == currentSpace.type);
                resolvedItem->preferredSize = std::max(resolvedItem->preferredSize, currentSpace.preferredSize);
                resolvedItem->flexBase = std::max(resolvedItem->flexBase, currentSpace.flexBase);
            }
        }
    };
    collectNonSpanningCells();

    auto collectAndDistributeSpanningCells = [&] {
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
                    unresolvedSpanningSpace -= *resolvedItems[spanIndex];
                }
                ASSERT(unresolvedColumnCount);
                auto equalSpaceForSpannedColumns = unresolvedSpanningSpace / unresolvedColumnCount;
                for (auto spanIndex = SpanType::startSpan(cell); spanIndex < SpanType::endSpan(cell); ++spanIndex) {
                    if (resolvedItems[spanIndex])
                        continue;
                    resolvedItems[spanIndex] = equalSpaceForSpannedColumns;
                }
            } else {
                // 1. Collect the non-spanning resolved spaces.
                // 2. Distribute the extra space among the spanned columns/rows based on the resolved space values.
                // e.g. spanning width: [   9   ]. Resolved widths for the spanned columns: [ 1 ] [ 2 ]
                // New resolved widths: [ 3 ] [ 6 ].
                auto resolvedSpanningSpace = GridSpace { };
                for (auto spanIndex = SpanType::startSpan(cell); spanIndex < SpanType::endSpan(cell); ++spanIndex)
                    resolvedSpanningSpace += *resolvedItems[spanIndex];
                if (resolvedSpanningSpace.preferredSize >= unresolvedSpanningSpace.preferredSize) {
                    // The spanning cell fits the spanned columns/rows just fine. Nothing to distribute.
                    continue;
                }
                auto spacing = SpanType::spacing(grid) * (SpanType::spanCount(cell) - 1);
                auto spaceToDistribute = unresolvedSpanningSpace - GridSpace { { }, spacing, spacing } - resolvedSpanningSpace;
                if (!spaceToDistribute.isEmpty()) {
                    auto columnsFlexBase = spaceToDistribute.flexBase / resolvedSpanningSpace.flexBase;
                    for (auto spanIndex = SpanType::startSpan(cell); spanIndex < SpanType::endSpan(cell); ++spanIndex)
                        *resolvedItems[spanIndex] += GridSpace { resolvedItems[spanIndex]->type, resolvedItems[spanIndex]->preferredSize * columnsFlexBase, resolvedItems[spanIndex]->flexBase * columnsFlexBase };
                }
            }
        }
    };
    collectAndDistributeSpanningCells();

    Vector<LayoutUnit> distributedSpaces(resolvedItems.size());
    auto spaceToDistribute = 0.f;
    auto computeSpaceToDistribute = [&] {
        // 4. Distribute the extra space using the final resolved sizes.
        spaceToDistribute = availableSpace - (resolvedItems.size() + 1) * SpanType::spacing(grid);
        auto adjustabledSpace = GridSpace { };
        for (auto& resolvedItem : resolvedItems) {
            ASSERT(resolvedItem);
            adjustabledSpace += *resolvedItem;
        }
        spaceToDistribute -= adjustabledSpace.preferredSize;
    };
    computeSpaceToDistribute();

    // Let's start with the preferred size for each column.
    for (size_t columnIndex = 0; columnIndex < resolvedItems.size(); ++columnIndex)
        distributedSpaces[columnIndex] = LayoutUnit { resolvedItems[columnIndex]->preferredSize };

    auto distributeSpace = [&] {
        if (!spaceToDistribute)
            return;
        // Setup the priority lists. We use these when expanding/shrinking slots.
        Vector<size_t> autoColumnIndexes;
        Vector<size_t> relativeColumnIndexes;
        Vector<size_t> fixedColumnIndexes;
        Vector<size_t> percentColumnIndexes;

        for (size_t columnIndex = 0; columnIndex < resolvedItems.size(); ++columnIndex) {
            switch (resolvedItems[columnIndex]->type) {
            case GridSpace::Type::Percent:
                percentColumnIndexes.append(columnIndex);
                break;
            case GridSpace::Type::Fixed:
                fixedColumnIndexes.append(columnIndex);
                break;
            case GridSpace::Type::Relative:
                relativeColumnIndexes.append(columnIndex);
                break;
            case GridSpace::Type::Auto:
                autoColumnIndexes.append(columnIndex);
                break;
            default:
                ASSERT_NOT_REACHED();
                break;
            }
        }

        if (spaceToDistribute > 0) {
            // Each column can get some extra space.
            auto hasSpaceToDistribute = [&] {
                ASSERT(spaceToDistribute > -LayoutUnit::epsilon());
                return spaceToDistribute > LayoutUnit::epsilon();
            };

            auto expandSpace = [&](const auto& columnIndexes) {
                auto adjustabledSpace = GridSpace { };
                for (auto& columnIndex : columnIndexes)
                    adjustabledSpace += *resolvedItems[columnIndex];

                auto columnsFlexBase = adjustabledSpace.flexBase ? spaceToDistribute / adjustabledSpace.flexBase : 0.f;
                for (auto& columnIndex : columnIndexes) {
                    auto extraSpace = columnsFlexBase * resolvedItems[columnIndex]->flexBase;
                    distributedSpaces[columnIndex] += LayoutUnit { extraSpace };
                    spaceToDistribute -= extraSpace;
                    if (!hasSpaceToDistribute())
                        return;
                }
            };
            // We distribute the extra space among columns in the priority order as follows:
            expandSpace(fixedColumnIndexes);
            if (hasSpaceToDistribute())
                expandSpace(percentColumnIndexes);
            if (hasSpaceToDistribute())
                expandSpace(relativeColumnIndexes);
            if (hasSpaceToDistribute())
                expandSpace(autoColumnIndexes);
            ASSERT(!hasSpaceToDistribute());
            return;
        }
        // Can't accommodate the preferred width. Let's use the priority list to shrink columns.
        auto spaceNeeded = -spaceToDistribute;

        auto needsMoreSpace = [&] {
            ASSERT(spaceNeeded > -LayoutUnit::epsilon());
            return spaceNeeded > LayoutUnit::epsilon();
        };

        auto shrinkSpace = [&](const auto& columnIndexes) {
            auto adjustabledSpace = GridSpace { };
            for (auto& columnIndex : columnIndexes)
                adjustabledSpace += *resolvedItems[columnIndex];

            auto columnsFlexBase = adjustabledSpace.flexBase ? spaceNeeded / adjustabledSpace.flexBase : 0.f;
            for (auto& columnIndex : columnIndexes) {
                auto& resolvedItem = *resolvedItems[columnIndex];
                auto spaceToRemove = std::min(resolvedItem.preferredSize - resolvedItem.minimumSize, columnsFlexBase * resolvedItem.flexBase);
                spaceToRemove = std::min(spaceToRemove, spaceNeeded);
                distributedSpaces[columnIndex] -= spaceToRemove;
                spaceNeeded -= spaceToRemove;
                if (!needsMoreSpace())
                    return;
            }
        };
        shrinkSpace(autoColumnIndexes);
        if (needsMoreSpace())
            shrinkSpace(relativeColumnIndexes);
        if (needsMoreSpace())
            shrinkSpace(fixedColumnIndexes);
        if (needsMoreSpace())
            shrinkSpace(percentColumnIndexes);
        ASSERT(!needsMoreSpace());
    };
    distributeSpace();

    return distributedSpaces;
}

TableFormattingContext::TableLayout::DistributedSpaces TableFormattingContext::TableLayout::distributedHorizontalSpace(LayoutUnit availableHorizontalSpace)
{
    auto hasEnoughAvailableSpaceForMaximumWidth = availableHorizontalSpace >= m_grid.widthConstraints()->maximum;
    return distributeAvailableSpace<ColumnSpan>(m_grid, availableHorizontalSpace, [&] (const TableGrid::Slot& slot, size_t columnIndex) {
        auto& column = m_grid.columns().list()[columnIndex];
        auto columnWidth = std::optional<float> { };
        auto type = GridSpace::Type::Auto;

        auto& computedLogicalWidth = column.computedLogicalWidth();
        switch (computedLogicalWidth.type()) {
        case LengthType::Fixed:
            columnWidth = computedLogicalWidth.value();
            type = GridSpace::Type::Fixed;
            break;
        case LengthType::Percent:
            columnWidth = computedLogicalWidth.value() * availableHorizontalSpace / 100.0f;
            type = GridSpace::Type::Percent;
            break;
        case LengthType::Relative:
            ASSERT_NOT_IMPLEMENTED_YET();
            break;
        default:
            break;
        }

        float minimumContentWidth = slot.widthConstraints().minimum;
        float maximumContentWidth = slot.widthConstraints().maximum;
        auto preferredWidth = std::max(minimumContentWidth, columnWidth.value_or(maximumContentWidth));
        if (!hasEnoughAvailableSpaceForMaximumWidth) {
            // maximum width > available horizontal space
            // preferred width: it's always at least as wide as the minimum content width and if column fixed width is set then the greater of the two.
            // base value for adjustable space distribution for a cell: preferred width - minimum content width
            // total adjustable horizontal space: available horizontal space - preferred width(s)
            // column extra width: total adjustable space / base value for distribution for all columns * base value for distribution for this column
            // column final width: preferred width + extra width
            // e.g
            // <table style="width: 120px">
            //   <td style="width: 80px;">some content</td>
            //   <td style="width: 20px;">some content></td>
            // </table>
            // maximum width (160px) > available horizontal space (120)
            // "some content" -> minimum content width is 50px 
            // preferred widths: 80px and 50px
            // base values for distribution: 30px and 0px
            // total adjustable space: 120px - 80px - 50px = -10px <- Note the negative available space with over-constrained values.
            // columns extra widths: -10px / (30px + 0px) * 30px = -10px and -10px / (30px + 0px) * 0px = 0px
            // columns final widths: 70px and 50px (first column is narrower than its preferred width and the second is as wide as the minimum content width)
            return GridSpace { type, preferredWidth, preferredWidth - minimumContentWidth, minimumContentWidth };
        }

        // maximum width <= available horizontal space
        // preferred width: it's usually the maximum content width but at least as wide as the minimum content width (or the column fixed width)
        // base value for adjustable space distribution for a cell: preferred width (again usually the maximum content width)
        // total adjustable horizontal space: available horizontal space - preferred width(s)
        // column extra width: total adjustable space / base value for distribution for all columns * base value for distribution for this column
        // column final width: preferred width + extra width
        // e.g
        // <table style="width: 2600px">
        //   <td style="width: 40px;"><div style="width: 200px;"></div></td>
        //   <td style="width: 300px;"><div style="width: 100px;"></div></td>
        // </table>
        // maximum width (500px) < available horizontal space (2600px)
        // preferred widths: 200px and 300px
        // base values for distribution: 200px and 300px
        // total adjustable space: 2600px - 500px = 2100px
        // columns extra widths: 2100px / 500px * 200px = 840px and 2100px / 500px * 300px = 1260px
        // columns final widths: 1040px and 1560px
        return GridSpace { type, preferredWidth, preferredWidth, minimumContentWidth };
    });
}

TableFormattingContext::TableLayout::DistributedSpaces TableFormattingContext::TableLayout::distributedVerticalSpace(std::optional<LayoutUnit> availableVerticalSpace)
{
    auto& rows = m_grid.rows();
    auto& columns = m_grid.columns();

    Vector<LayoutUnit> rowHeight(rows.size());
    auto tableUsedHeight = LayoutUnit { };
    // 2. Collect initial, baseline aligned row heights.
    for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
        auto maximumColumnAscent = InlineLayoutUnit { };
        auto maximumColumnDescent = InlineLayoutUnit { };
        // Initial minimum height is the computed height if available <tr style="height: 100px"><td></td></tr>
        rowHeight[rowIndex] = formattingContext().formattingGeometry().computedHeight(rows.list()[rowIndex].box(), availableVerticalSpace).value_or(0_lu);
        for (size_t columnIndex = 0; columnIndex < columns.size(); ++columnIndex) {
            auto& slot = *m_grid.slot({ columnIndex, rowIndex });
            if (slot.isRowSpanned())
                continue;
            if (slot.hasRowSpan())
                continue;
            // The minimum height of a row (without spanning-related height distribution) is defined as the height of an hypothetical
            // linebox containing the cells originating in the row.
            auto& cell = slot.cell();
            auto& cellBox = cell.box();
            auto height = formattingContext().geometryForBox(cellBox).borderBoxHeight();
            if (cellBox.style().verticalAlign() == VerticalAlign::Baseline) {
                maximumColumnAscent = std::max(maximumColumnAscent, cell.baseline());
                maximumColumnDescent = std::max(maximumColumnDescent, height - cell.baseline());
                rowHeight[rowIndex] = std::max(rowHeight[rowIndex], LayoutUnit { maximumColumnAscent + maximumColumnDescent });
            } else
                rowHeight[rowIndex] = std::max(rowHeight[rowIndex], height);
        }
        tableUsedHeight += rowHeight[rowIndex];
    }
    // FIXME: Collect spanning row maximum heights.

    tableUsedHeight += (rows.size() + 1) * m_grid.verticalSpacing();
    auto availableSpace = std::max(availableVerticalSpace.value_or(0_lu), tableUsedHeight);
    // Distribute extra space if the table is supposed to be taller than the sum of the row heights.
    return distributeAvailableSpace<RowSpan>(m_grid, availableSpace, [&] (const TableGrid::Slot& slot, size_t rowIndex) {
        if (slot.hasRowSpan()) {
            auto borderBoxHeight = formattingContext().geometryForBox(slot.cell().box()).borderBoxHeight();
            return GridSpace { { }, borderBoxHeight, borderBoxHeight, borderBoxHeight };
        }
        auto& rows = m_grid.rows();
        auto computedRowHeight = formattingContext().formattingGeometry().computedHeight(rows.list()[rowIndex].box(), { });
        auto height = std::max<float>(rowHeight[rowIndex], computedRowHeight.value_or(0_lu));
        return GridSpace { { }, height, height, height };
    });
}

}
}

