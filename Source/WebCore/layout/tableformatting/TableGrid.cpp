/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "TableGrid.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(TableGrid);

TableGrid::Column::Column(const ContainerBox* columnBox)
    : m_layoutBox(makeWeakPtr(columnBox))
{
}

void TableGrid::Column::setLogicalWidth(LayoutUnit computedLogicalWidth)
{
#if ASSERT_ENABLED
    m_hasComputedWidth = true;
#endif
    m_computedLogicalWidth = computedLogicalWidth;
}

LayoutUnit TableGrid::Column::logicalWidth() const
{
    ASSERT(m_hasComputedWidth);
    return m_computedLogicalWidth;
}

void TableGrid::Column::setLogicalLeft(LayoutUnit computedLogicalLeft)
{
#if ASSERT_ENABLED
    m_hasComputedLeft = true;
#endif
    m_computedLogicalLeft = computedLogicalLeft;
}

LayoutUnit TableGrid::Column::logicalLeft() const
{
    ASSERT(m_hasComputedLeft);
    return m_computedLogicalLeft;
}

bool TableGrid::Column::isFixedWidth() const
{
    return hasFixedWidthCell() || (box() && box()->columnWidth());
}

void TableGrid::Columns::addColumn(const ContainerBox& columnBox)
{
    m_columnList.append({ &columnBox });
}

void TableGrid::Columns::addAnonymousColumn()
{
    m_columnList.append({ nullptr });
}

bool TableGrid::Columns::hasFixedColumnsOnly() const
{
    for (auto& column : m_columnList) {
        if (!column.isFixedWidth())
            return false;
    }
    return true;
}

void TableGrid::Rows::addRow(const ContainerBox& rowBox)
{
    m_rowList.append({ rowBox });
}

TableGrid::Row::Row(const ContainerBox& rowBox)
    : m_layoutBox(makeWeakPtr(rowBox))
{
}

TableGrid::Cell::Cell(const ContainerBox& cellBox, SlotPosition position, CellSpan span)
    : m_layoutBox(makeWeakPtr(cellBox))
    , m_position(position)
    , m_span(span)
{
}

bool TableGrid::Cell::isFixedWidth() const
{
    return box().style().logicalWidth().isFixed();
}

TableGrid::Slot::Slot(Cell& cell, bool isColumnSpanned, bool isRowSpanned)
    : m_cell(makeWeakPtr(cell))
    , m_isColumnSpanned(isColumnSpanned)
    , m_isRowSpanned(isRowSpanned)
{
}

TableGrid::TableGrid()
{
}

TableGrid::Slot* TableGrid::slot(SlotPosition position)
{
    return m_slotMap.get(position);
}

void TableGrid::appendCell(const ContainerBox& cellBox)
{
    auto rowSpan = cellBox.rowSpan();
    auto columnSpan = cellBox.columnSpan();
    auto isInNewRow = !cellBox.previousSibling();
    auto initialSlotPosition = SlotPosition { };

    if (!m_cells.isEmpty()) {
        auto& lastCell = m_cells.last();
        auto lastSlotPosition = lastCell->position();
        // First table cell in this row?
        if (isInNewRow)
            initialSlotPosition = SlotPosition { 0, lastSlotPosition.row + 1 };
        else
            initialSlotPosition = SlotPosition { lastSlotPosition.column + 1, lastSlotPosition.row };

        // Pick the next available slot by avoiding row and column spanners.
        while (true) {
            if (!m_slotMap.contains(initialSlotPosition))
                break;
            ++initialSlotPosition.column;
        }
    }
    auto cell = makeUnique<Cell>(cellBox, initialSlotPosition, CellSpan { columnSpan, rowSpan });
    // Row and column spanners create additional slots.
    for (size_t row = 0; row < rowSpan; ++row) {
        for (auto column = cell->startColumn(); column < cell->endColumn(); ++column) {
            auto position = SlotPosition { column, cell->startRow() + row };
            ASSERT(!m_slotMap.contains(position));
            // This slot is spanned by a cell at the initial slow position.
            auto isColumnSpanned = column != cell->startColumn();
            auto isRowSpanned = !!row;
            m_slotMap.add(position, makeUnique<Slot>(*cell, isColumnSpanned, isRowSpanned));
        }
    }
    // Initialize columns/rows if needed.
    auto missingNumberOfColumns = std::max<int>(0, initialSlotPosition.column + columnSpan - m_columns.size());
    for (auto column = 0; column < missingNumberOfColumns; ++column)
        m_columns.addAnonymousColumn();

    if (cell->isFixedWidth()) {
        for (auto column = cell->startColumn(); column < cell->endColumn(); ++column)
            m_columns.list()[column].setHasFixedWidthCell();
    }

    if (isInNewRow)
        m_rows.addRow(cellBox.parent());

    m_cells.add(WTFMove(cell));
}

void TableGrid::insertCell(const ContainerBox& cellBox, const ContainerBox& before)
{
    UNUSED_PARAM(cellBox);
    UNUSED_PARAM(before);
}

void TableGrid::removeCell(const ContainerBox& cellBox)
{
    UNUSED_PARAM(cellBox);
}

}
}
#endif
