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

TableGrid::Column::Column(const Box* columnBox)
    : m_columnBox(makeWeakPtr(columnBox))
{
}

void TableGrid::Column::setWidthConstraints(FormattingContext::IntrinsicWidthConstraints widthConstraints)
{
#if ASSERT_ENABLED
    m_hasWidthConstraints = true;
#endif
    m_widthConstraints = widthConstraints;
}

FormattingContext::IntrinsicWidthConstraints TableGrid::Column::widthConstraints() const
{
    ASSERT(m_hasWidthConstraints);
    return m_widthConstraints;
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

bool TableGrid::Column::hasFixedWidth() const
{
    // FIXME: This only covers the <col> attribute case.
    return columnBox() && columnBox()->columnWidth();
}

void TableGrid::ColumnsContext::addColumn(const Box* columnBox)
{
    m_columns.append({ columnBox });
}

TableGrid::Row::Row(const Box& rowBox)
    : m_layoutBox(rowBox)
{
}

TableGrid::CellInfo::CellInfo(const Box& tableCellBox, SlotPosition position, CellSize size)
    : tableCellBox(tableCellBox)
    , position(position)
    , size(size)
{
}

TableGrid::SlotInfo::SlotInfo(CellInfo& cell)
    : cell(makeWeakPtr(cell))
{
}

TableGrid::TableGrid()
{
}

TableGrid::SlotInfo* TableGrid::slot(SlotPosition position)
{
    return m_slotMap.get(position);
}

void TableGrid::appendCell(const Box& tableCellBox)
{
    int rowSpan = tableCellBox.rowSpan();
    int columnSpan = tableCellBox.columnSpan();
    auto isInNewRow = !tableCellBox.previousSibling();
    auto initialSlotPosition = SlotPosition { };

    if (!m_cellList.isEmpty()) {
        auto& lastCell = m_cellList.last();
        auto lastSlotPosition = lastCell->position;
        // First table cell in this row?
        if (isInNewRow)
            initialSlotPosition = SlotPosition { 0, lastSlotPosition.y() + 1 };
        else
            initialSlotPosition = SlotPosition { lastSlotPosition.x() + 1, lastSlotPosition.y() };

        // Pick the next available slot by avoiding row and column spanners.
        while (true) {
            if (!m_slotMap.contains(initialSlotPosition))
                break;
            initialSlotPosition.move(1, 0);
        }
    }
    auto cellInfo = makeUnique<CellInfo>(tableCellBox, initialSlotPosition, CellSize { rowSpan, columnSpan });
    // Row and column spanners create additional slots.
    for (int row = 1; row <= rowSpan; ++row) {
        for (int column = 1; column <= columnSpan; ++column) {
            auto position = SlotPosition { initialSlotPosition.x() + row - 1, initialSlotPosition.y() + column - 1 };
            ASSERT(!m_slotMap.contains(position));
            m_slotMap.add(position, makeUnique<SlotInfo>(*cellInfo));
        }
    }
    // Initialize columns/rows if needed.
    auto missingNumberOfColumns = std::max<int>(0, initialSlotPosition.x() + columnSpan - m_columnsContext.columns().size()); 
    for (auto column = 0; column < missingNumberOfColumns; ++column)
        m_columnsContext.addColumn();

    if (isInNewRow)
        m_rows.append({ *tableCellBox.parent() });

    m_cellList.add(WTFMove(cellInfo));
}

void TableGrid::insertCell(const Box& tableCellBox, const Box& before)
{
    UNUSED_PARAM(tableCellBox);
    UNUSED_PARAM(before);
}

void TableGrid::removeCell(const Box& tableCellBox)
{
    UNUSED_PARAM(tableCellBox);
}

FormattingContext::IntrinsicWidthConstraints TableGrid::widthConstraints()
{
    // FIXME: Add constraint invalidation for incremental layouts.
    if (m_intrinsicWidthConstraints)
        return *m_intrinsicWidthConstraints;

    m_intrinsicWidthConstraints = FormattingContext::IntrinsicWidthConstraints { };
    for (auto& column : m_columnsContext.columns())
        *m_intrinsicWidthConstraints += column.widthConstraints();
    m_intrinsicWidthConstraints->expand(totalHorizontalSpacing());
    return *m_intrinsicWidthConstraints;
}

}
}
#endif
