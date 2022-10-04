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
class ElementBox;

class TableGrid {
    WTF_MAKE_ISO_ALLOCATED(TableGrid);
public:
    TableGrid();

    void appendCell(const ElementBox&);
    void insertCell(const ElementBox&, const ElementBox& before);
    void removeCell(const ElementBox&);

    void setHorizontalSpacing(LayoutUnit horizontalSpacing) { m_horizontalSpacing = horizontalSpacing; }
    LayoutUnit horizontalSpacing() const { return m_horizontalSpacing; }

    void setVerticalSpacing(LayoutUnit verticalSpacing) { m_verticalSpacing = verticalSpacing; }
    LayoutUnit verticalSpacing() const { return m_verticalSpacing; }

    void setCollapsedBorder(const Edges& collapsedBorder) { m_collapsedBorder = collapsedBorder; }
    std::optional<Edges> collapsedBorder() const { return m_collapsedBorder; }

    void setWidthConstraints(IntrinsicWidthConstraints intrinsicWidthConstraints) { m_intrinsicWidthConstraints = intrinsicWidthConstraints; }
    std::optional<IntrinsicWidthConstraints> widthConstraints() const { return m_intrinsicWidthConstraints; }

    bool isEmpty() const { return m_slotMap.isEmpty(); }
    // Column represents a vertical set of slots in the grid. A column has horizontal position and width.
    class Column {
    public:
        Column(const ElementBox*);

        void setUsedLogicalLeft(LayoutUnit);
        LayoutUnit usedLogicalLeft() const;
        LayoutUnit usedLogicalRight() const { return usedLogicalLeft() + usedLogicalWidth(); }
        void setUsedLogicalWidth(LayoutUnit);
        LayoutUnit usedLogicalWidth() const;

        void setComputedLogicalWidth(Length&&);
        const Length& computedLogicalWidth() const { return m_computedLogicalWidth; }

        const ElementBox* box() const { return m_layoutBox.get(); }

    private:
        LayoutUnit m_usedLogicalWidth;
        LayoutUnit m_usedLogicalLeft;
        Length m_computedLogicalWidth;
        CheckedPtr<const ElementBox> m_layoutBox;

#if ASSERT_ENABLED
        bool m_hasUsedWidth { false };
        bool m_hasUsedLeft { false };
#endif
    };

    class Columns {
    public:
        using ColumnList = Vector<Column>;
        ColumnList& list() { return m_columnList; }
        const ColumnList& list() const { return m_columnList; }
        size_t size() const { return m_columnList.size(); }

        void addColumn(const ElementBox&);
        void addAnonymousColumn();

        LayoutUnit logicalWidth() const { return m_columnList.last().usedLogicalRight() - m_columnList.first().usedLogicalLeft(); }

    private:
        ColumnList m_columnList;
    };

    class Row {
    public:
        Row(const ElementBox&);

        void setLogicalTop(LayoutUnit logicalTop) { m_logicalTop = logicalTop; }
        LayoutUnit logicalTop() const { return m_logicalTop; }
        LayoutUnit logicalBottom() const { return logicalTop() + logicalHeight(); }

        void setLogicalHeight(LayoutUnit logicalHeight) { m_logicalHeight = logicalHeight; }
        LayoutUnit logicalHeight() const { return m_logicalHeight; }

        void setBaseline(InlineLayoutUnit baseline) { m_baseline = baseline; }
        InlineLayoutUnit baseline() const { return m_baseline; }

        const ElementBox& box() const { return m_layoutBox; }

    private:
        LayoutUnit m_logicalTop;
        LayoutUnit m_logicalHeight;
        InlineLayoutUnit m_baseline { 0 };
        CheckedRef<const ElementBox> m_layoutBox;
    };

    class Rows {
    public:
        using RowList = Vector<Row>;
        RowList& list() { return m_rowList; }
        const RowList& list() const { return m_rowList; }

        void addRow(const ElementBox&);

        size_t size() const { return m_rowList.size(); }

    private:
        RowList m_rowList;
    };

    // Cell represents a <td> or <th>. It can span multiple slots in the grid.
    class Cell : public CanMakeWeakPtr<Cell> {
        WTF_MAKE_ISO_ALLOCATED_INLINE(Cell);
    public:
        Cell(const ElementBox&, SlotPosition, CellSpan);

        size_t startColumn() const { return m_position.column; }
        size_t endColumn() const { return m_position.column + m_span.column; }

        size_t startRow() const { return m_position.row; }
        size_t endRow() const { return m_position.row + m_span.row; }

        size_t columnSpan() const { return m_span.column; }
        size_t rowSpan() const { return m_span.row; }

        SlotPosition position() const { return m_position; }
        CellSpan span() const { return m_span; }

        void setBaseline(InlineLayoutUnit baseline) { m_baseline = baseline; }
        InlineLayoutUnit baseline() const { return m_baseline; }

        const ElementBox& box() const { return *m_layoutBox.get(); }

    private:
        CheckedPtr<const ElementBox> m_layoutBox;
        SlotPosition m_position;
        CellSpan m_span;
        InlineLayoutUnit m_baseline { 0 };
    };

    class Slot {
    public:
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
        Slot() = default;
        Slot(Cell&, bool isColumnSpanned, bool isRowSpanned);

        const Cell& cell() const { return *m_cell; }
        Cell& cell() { return *m_cell; }

        const IntrinsicWidthConstraints& widthConstraints() const { return m_widthConstraints; }
        void setWidthConstraints(const IntrinsicWidthConstraints& widthConstraints) { m_widthConstraints = widthConstraints; }

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
        IntrinsicWidthConstraints m_widthConstraints;
    };

    const Columns& columns() const { return m_columns; }
    Columns& columns() { return m_columns; }

    const Rows& rows() const { return m_rows; }
    Rows& rows() { return m_rows; }

    using Cells = ListHashSet<std::unique_ptr<Cell>>;
    Cells& cells() { return m_cells; }

    Slot* slot(SlotPosition);
    const Slot* slot(SlotPosition position) const { return m_slotMap.get(position); }
    bool isSpanned(SlotPosition);

private:
    using SlotMap = HashMap<SlotPosition, std::unique_ptr<Slot>>;

    Columns m_columns;
    Rows m_rows;
    Cells m_cells;
    SlotMap m_slotMap;

    LayoutUnit m_horizontalSpacing;
    LayoutUnit m_verticalSpacing;
    std::optional<IntrinsicWidthConstraints> m_intrinsicWidthConstraints;
    std::optional<Edges> m_collapsedBorder;
};

inline void TableGrid::Column::setComputedLogicalWidth(Length&& computedLogicalWidth)
{
    ASSERT(computedLogicalWidth.type() == LengthType::Fixed || computedLogicalWidth.type() == LengthType::Percent || computedLogicalWidth.type() == LengthType::Relative);
    m_computedLogicalWidth = WTFMove(computedLogicalWidth);
}

inline void TableGrid::Column::setUsedLogicalWidth(LayoutUnit usedLogicalWidth)
{
#if ASSERT_ENABLED
    m_hasUsedWidth = true;
#endif
    m_usedLogicalWidth = usedLogicalWidth;
}

inline LayoutUnit TableGrid::Column::usedLogicalWidth() const
{
    ASSERT(m_hasUsedWidth);
    return m_usedLogicalWidth;
}

inline void TableGrid::Column::setUsedLogicalLeft(LayoutUnit usedLogicalLeft)
{
#if ASSERT_ENABLED
    m_hasUsedLeft = true;
#endif
    m_usedLogicalLeft = usedLogicalLeft;
}

inline LayoutUnit TableGrid::Column::usedLogicalLeft() const
{
    ASSERT(m_hasUsedLeft);
    return m_usedLogicalLeft;
}

}
}

