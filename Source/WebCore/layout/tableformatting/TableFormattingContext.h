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
#include "TableFormattingState.h"
#include "TableGrid.h"
#include <wtf/IsoMalloc.h>
#include <wtf/UniqueRef.h>

namespace WebCore {
namespace Layout {

class InvalidationState;
// This class implements the layout logic for table formatting contexts.
// https://www.w3.org/TR/CSS22/tables.html
class TableFormattingContext final : public FormattingContext {
    WTF_MAKE_ISO_ALLOCATED(TableFormattingContext);
public:
    TableFormattingContext(const ContainerBox& formattingContextRoot, TableFormattingState&);
    void layoutInFlowContent(InvalidationState&, const ConstraintsForInFlowContent&) override;

    static UniqueRef<TableGrid> ensureTableGrid(const ContainerBox& tableBox);

private:
    class TableLayout {
    public:
        TableLayout(const TableFormattingContext&, const TableGrid&);

        using DistributedSpaces = Vector<LayoutUnit>;
        DistributedSpaces distributedHorizontalSpace(LayoutUnit availableHorizontalSpace);
        DistributedSpaces distributedVerticalSpace(Optional<LayoutUnit> availableVerticalSpace);

    private:
        const TableFormattingContext& formattingContext() const { return m_formattingContext; }

        const TableFormattingContext& m_formattingContext;
        const TableGrid& m_grid;
    };

    class Geometry : public FormattingContext::Geometry {
    public:
        LayoutUnit cellHeigh(const ContainerBox&) const;
        Edges computedCellBorder(const TableGrid::Cell&) const;
        Optional<LayoutUnit> computedColumnWidth(const ContainerBox& columnBox) const;
        FormattingContext::IntrinsicWidthConstraints intrinsicWidthConstraintsForCell(const TableGrid::Cell&);
        InlineLayoutUnit usedBaselineForCell(const ContainerBox& cellBox);

    private:
        friend class TableFormattingContext;
        Geometry(const TableFormattingContext&, const TableGrid&);

        const TableFormattingContext& formattingContext() const { return downcast<TableFormattingContext>(FormattingContext::Geometry::formattingContext()); }
        const TableGrid& m_grid;
    };
    TableFormattingContext::Geometry geometry() const { return Geometry(*this, formattingState().tableGrid()); }
    TableFormattingContext::TableLayout tableLayout() const { return TableLayout(*this, formattingState().tableGrid()); }

    IntrinsicWidthConstraints computedIntrinsicWidthConstraints() override;
    void layoutCell(const TableGrid::Cell&, LayoutUnit availableHorizontalSpace, Optional<LayoutUnit> usedCellHeight = WTF::nullopt);
    void setUsedGeometryForCells(LayoutUnit availableHorizontalSpace);
    void setUsedGeometryForRows(LayoutUnit availableHorizontalSpace);
    void setUsedGeometryForSections(const ConstraintsForInFlowContent&);

    IntrinsicWidthConstraints computedPreferredWidthForColumns();
    void computeAndDistributeExtraSpace(LayoutUnit availableHorizontalSpace, Optional<LayoutUnit> availableVerticalSpace);

    const TableFormattingState& formattingState() const { return downcast<TableFormattingState>(FormattingContext::formattingState()); }
    TableFormattingState& formattingState() { return downcast<TableFormattingState>(FormattingContext::formattingState()); }
};

inline TableFormattingContext::Geometry::Geometry(const TableFormattingContext& tableFormattingContext, const TableGrid& grid)
    : FormattingContext::Geometry(tableFormattingContext)
    , m_grid(grid)
{
}

}
}

SPECIALIZE_TYPE_TRAITS_LAYOUT_FORMATTING_CONTEXT(TableFormattingContext, isTableFormattingContext())

#endif
