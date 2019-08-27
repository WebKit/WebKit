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

#include "IntPointHash.h"
#include "LayoutBox.h"
#include <wtf/HashMap.h>
#include <wtf/IsoMalloc.h>
#include <wtf/ListHashSet.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
namespace Layout {

class TableGrid {
    WTF_MAKE_ISO_ALLOCATED(TableGrid);
public:
    TableGrid();

    void appendCell(const Box&);
    void insertCell(const Box&, const Box& before);
    void removeCell(const Box&);

    using SlotPosition = IntPoint;

    // Cell represents a <td> or <th>. It can span multiple slots in the grid.
    using CellSize = IntSize;
    struct CellInfo : public CanMakeWeakPtr<CellInfo> {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
        CellInfo(const Box& tableCellBox, SlotPosition, CellSize);

        const Box& tableCellBox;
        SlotPosition position;
        CellSize size;
    };
    using CellList = WTF::ListHashSet<std::unique_ptr<CellInfo>>;
    CellList& cells() { return m_cellList; }

    // Column represents a vertical set of slots in the grid. A column has min/max and final width.
    class ColumnsContext;
    class Column {
    public:
        void setWidthConstraints(FormattingContext::IntrinsicWidthConstraints);
        FormattingContext::IntrinsicWidthConstraints widthConstraints() const;

        void setLogicalWidth(LayoutUnit);
        LayoutUnit logicalWidth() const;

        void setLogicalLeft(LayoutUnit);
        LayoutUnit logicalLeft() const;

        LayoutUnit logicalRight() const { return logicalLeft() + logicalWidth(); }

    private:
        friend class ColumnsContext;
        Column() = default;

        FormattingContext::IntrinsicWidthConstraints m_widthConstraints;
        LayoutUnit m_computedLogicalWidth;
        LayoutUnit m_computedLogicalLeft;
#ifndef NDEBUG
        bool m_hasWidthConstraints { false };
        bool m_hasComputedWidth { false };
        bool m_hasComputedLeft { false };
#endif
    };

    class ColumnsContext {
    public:
        using ColumnList = Vector<Column>;
        ColumnList& columns() { return m_columns; }
        const ColumnList& columns() const { return m_columns; }

        enum class WidthConstraintsType { Minimum, Maximum };
        void useAsLogicalWidth(WidthConstraintsType);

    private:
        friend class TableGrid;
        void addColumn();

        ColumnList m_columns;
    };
    ColumnsContext& columnsContext() { return m_columnsContext; }

    struct Row {
    public:
        void setLogicalTop(LayoutUnit logicalTop) { m_logicalTop = logicalTop; }
        LayoutUnit logicalTop() const { return m_logicalTop; }

        void setLogicalHeight(LayoutUnit logicalHeight) { m_logicalHeight = logicalHeight; }
        LayoutUnit logicalHeight() const { return m_logicalHeight; }

        LayoutUnit logicalBottom() const { return logicalTop() + logicalHeight(); }

    private:
        LayoutUnit m_logicalTop;
        LayoutUnit m_logicalHeight;
    };
    using RowList = WTF::Vector<Row>;
    RowList& rows() { return m_rows; }

    struct SlotInfo {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
        SlotInfo() = default;
        SlotInfo(CellInfo&);

        WeakPtr<CellInfo> cell;
        FormattingContext::IntrinsicWidthConstraints widthConstraints;
    };
    SlotInfo* slot(SlotPosition);

    FormattingContext::IntrinsicWidthConstraints widthConstraints() const;

private:
    using SlotMap = WTF::HashMap<SlotPosition, std::unique_ptr<SlotInfo>>;

    SlotMap m_slotMap;
    CellList m_cellList;
    ColumnsContext m_columnsContext;
    RowList m_rows;
};

}
}
#endif
