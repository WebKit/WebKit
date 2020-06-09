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

#pragma once

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "FormattingContext.h"
#include "LayoutUnits.h"
#include <wtf/HashMap.h>
#include <wtf/IsoMalloc.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/ListHashSet.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
namespace Layout {
class Box;
class ContainerBox;

class TableGrid {
    WTF_MAKE_ISO_ALLOCATED(TableGrid);
public:
    TableGrid();

    void appendCell(const ContainerBox&);
    void insertCell(const ContainerBox&, const ContainerBox& before);
    void removeCell(const ContainerBox&);

    void setHorizontalSpacing(LayoutUnit horizontalSpacing) { m_horizontalSpacing = horizontalSpacing; }
    LayoutUnit horizontalSpacing() const { return m_horizontalSpacing; }

    void setVerticalSpacing(LayoutUnit verticalSpacing) { m_verticalSpacing = verticalSpacing; }
    LayoutUnit verticalSpacing() const { return m_verticalSpacing; }

    void setCollapsedBorder(const Edges& collapsedBorder) { m_collapsedBorder = collapsedBorder; }
    Optional<Edges> collapsedBorder() const { return m_collapsedBorder; }

    void setWidthConstraints(FormattingContext::IntrinsicWidthConstraints intrinsicWidthConstraints) { m_intrinsicWidthConstraints = intrinsicWidthConstraints; }
    Optional<FormattingContext::IntrinsicWidthConstraints> widthConstraints() const { return m_intrinsicWidthConstraints; }

    bool isEmpty() const { return m_slotMap.isEmpty(); }
    // Column represents a vertical set of slots in the grid. A column has horizontal position and width.
    class Column {
    public:
        Column(const ContainerBox*);

        void setLogicalLeft(LayoutUnit);
        LayoutUnit logicalLeft() const;
        LayoutUnit logicalRight() const { return logicalLeft() + logicalWidth(); }
        void setLogicalWidth(LayoutUnit);
        LayoutUnit logicalWidth() const;

        bool isFixedWidth() const;

        void setHasFixedWidthCell() { m_hasFixedWidthCell = true; }
        const ContainerBox* box() const { return m_layoutBox.get(); }

    private:
        bool hasFixedWidthCell() const { return m_hasFixedWidthCell; }

        LayoutUnit m_computedLogicalWidth;
        LayoutUnit m_computedLogicalLeft;
        WeakPtr<const ContainerBox> m_layoutBox;
        bool m_hasFixedWidthCell { false };

#if ASSERT_ENABLED
        bool m_hasComputedWidth { false };
        bool m_hasComputedLeft { false };
#endif
    };

    class Columns {
    public:
        using ColumnList = Vector<Column>;
        ColumnList& list() { return m_columnList; }
        const ColumnList& list() const { return m_columnList; }
        size_t size() const { return m_columnList.size(); }

        void addColumn(const ContainerBox&);
        void addAnonymousColumn();

        LayoutUnit logicalWidth() const { return m_columnList.last().logicalRight() - m_columnList.first().logicalLeft(); }
        bool hasFixedColumnsOnly() const;

    private:
        ColumnList m_columnList;
    };

    class Row {
    public:
        Row(const ContainerBox&);

        void setLogicalTop(LayoutUnit logicalTop) { m_logicalTop = logicalTop; }
        LayoutUnit logicalTop() const { return m_logicalTop; }
        LayoutUnit logicalBottom() const { return logicalTop() + logicalHeight(); }

        void setLogicalHeight(LayoutUnit logicalHeight) { m_logicalHeight = logicalHeight; }
        LayoutUnit logicalHeight() const { return m_logicalHeight; }

        void setBaselineOffset(InlineLayoutUnit baselineOffset) { m_baselineOffset = baselineOffset; }
        InlineLayoutUnit baselineOffset() const { return m_baselineOffset; }

        const ContainerBox& box() const { return *m_layoutBox.get(); }

    private:
        LayoutUnit m_logicalTop;
        LayoutUnit m_logicalHeight;
        InlineLayoutUnit m_baselineOffset;
        WeakPtr<const ContainerBox> m_layoutBox;
    };

    class Rows {
    public:
        using RowList = Vector<Row>;
        RowList& list() { return m_rowList; }
        const RowList& list() const { return m_rowList; }

        void addRow(const ContainerBox&);

        size_t size() const { return m_rowList.size(); }

    private:
        RowList m_rowList;
    };

    // Cell represents a <td> or <th>. It can span multiple slots in the grid.
    class Cell : public CanMakeWeakPtr<Cell> {
        WTF_MAKE_ISO_ALLOCATED_INLINE(Cell);
    public:
        Cell(const ContainerBox&, SlotPosition, CellSpan);

        size_t startColumn() const { return m_position.column; }
        size_t endColumn() const { return m_position.column + m_span.column; }

        size_t startRow() const { return m_position.row; }
        size_t endRow() const { return m_position.row + m_span.row; }

        size_t columnSpan() const { return m_span.column; }
        size_t rowSpan() const { return m_span.row; }

        SlotPosition position() const { return m_position; }
        CellSpan span() const { return m_span; }

        void setBaselineOffset(InlineLayoutUnit baselineOffset) { m_baselineOffset = baselineOffset; }
        InlineLayoutUnit baselineOffset() const { return m_baselineOffset; }

        bool isFixedWidth() const;

        const ContainerBox& box() const { return *m_layoutBox.get(); }

    private:
        WeakPtr<const ContainerBox> m_layoutBox;
        SlotPosition m_position;
        CellSpan m_span;
        InlineLayoutUnit m_baselineOffset { 0 };
    };

    class Slot {
    public:
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
        Slot() = default;
        Slot(Cell&, bool isColumnSpanned, bool isRowSpanned);

        const Cell& cell() const { return *m_cell; }
        Cell& cell() { return *m_cell; }

        const FormattingContext::IntrinsicWidthConstraints& widthConstraints() const { return m_widthConstraints; }
        void setWidthConstraints(const FormattingContext::IntrinsicWidthConstraints& widthConstraints) { m_widthConstraints = widthConstraints; }

        // Initial slot position for a spanning cell.
        // <td></td><td colspan=2></td> [1, 0] slot has column span of 2.
        bool hasColumnSpan() const { return m_cell->columnSpan() > 1 && !isColumnSpanned(); }
        bool hasRowSpan() const { return m_cell->rowSpan() > 1 && !isRowSpanned(); }

        // Non-initial spanned slot.
        // <td></td><td colspan=2></td> [2, 0] slot is column spanned by [1, 0].
        // <td></td><td></td><td></td>
        bool isColumnSpanned() const { return m_isColumnSpanned; }
        bool isRowSpanned() const { return m_isRowSpanned; }

    private:
        WeakPtr<Cell> m_cell;
        bool m_isColumnSpanned { false };
        bool m_isRowSpanned { false };
        FormattingContext::IntrinsicWidthConstraints m_widthConstraints;
    };

    const Columns& columns() const { return m_columns; }
    Columns& columns() { return m_columns; }

    const Rows& rows() const { return m_rows; }
    Rows& rows() { return m_rows; }

    using Cells = WTF::ListHashSet<std::unique_ptr<Cell>>;
    Cells& cells() { return m_cells; }

    Slot* slot(SlotPosition);
    const Slot* slot(SlotPosition position) const { return m_slotMap.get(position); }
    bool isSpanned(SlotPosition);

private:
    using SlotMap = WTF::HashMap<SlotPosition, std::unique_ptr<Slot>>;

    Columns m_columns;
    Rows m_rows;
    Cells m_cells;
    SlotMap m_slotMap;

    LayoutUnit m_horizontalSpacing;
    LayoutUnit m_verticalSpacing;
    Optional<FormattingContext::IntrinsicWidthConstraints> m_intrinsicWidthConstraints;
    Optional<Edges> m_collapsedBorder;
};

}
}

#endif
