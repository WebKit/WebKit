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
#include "TableGrid.h"
#include <wtf/IsoMalloc.h>

namespace WebCore {
namespace Layout {

class InvalidationState;
class TableFormattingState;
// This class implements the layout logic for table formatting contexts.
// https://www.w3.org/TR/CSS22/tables.html
class TableFormattingContext final : public FormattingContext {
    WTF_MAKE_ISO_ALLOCATED(TableFormattingContext);
public:
    TableFormattingContext(const Container& formattingContextRoot, TableFormattingState&);
    void layoutInFlowContent(InvalidationState&, const HorizontalConstraints&, const VerticalConstraints&) override;

private:
    class Geometry : public FormattingContext::Geometry {
    public:
        ContentHeightAndMargin tableCellHeightAndMargin(const Box&) const;
        Optional<LayoutUnit> computedColumnWidth(const Box& columnBox) const;

    private:
        friend class TableFormattingContext;
        Geometry(const TableFormattingContext&);

        const TableFormattingContext& formattingContext() const { return downcast<TableFormattingContext>(FormattingContext::Geometry::formattingContext()); }
    };
    TableFormattingContext::Geometry geometry() const { return Geometry(*this); }

    IntrinsicWidthConstraints computedIntrinsicWidthConstraints() override;
    void layoutTableCellBox(const Box& cellLayoutBox, const TableGrid::Column&, InvalidationState&, const HorizontalConstraints&);
    void positionTableCells();
    void setComputedGeometryForRows();
    void setComputedGeometryForSections();

    void ensureTableGrid();
    void computePreferredWidthForColumns();
    void computeAndDistributeExtraHorizontalSpace();
    enum class WidthConstraintsType { Minimum, Maximum };
    void useAsContentLogicalWidth(WidthConstraintsType);

    void initializeDisplayBoxToBlank(Display::Box&) const;

    const TableFormattingState& formattingState() const { return downcast<TableFormattingState>(FormattingContext::formattingState()); }
    TableFormattingState& formattingState() { return downcast<TableFormattingState>(FormattingContext::formattingState()); }
};

inline TableFormattingContext::Geometry::Geometry(const TableFormattingContext& tableFormattingContext)
    : FormattingContext::Geometry(tableFormattingContext)
{
}

}
}

SPECIALIZE_TYPE_TRAITS_LAYOUT_FORMATTING_CONTEXT(TableFormattingContext, isTableFormattingContext())

#endif
