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
#include "TableFormattingContext.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "LayoutBox.h"
#include "TableFormattingState.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(TableFormattingContext);

TableFormattingContext::TableFormattingContext(const Box& formattingContextRoot, TableFormattingState& formattingState)
    : FormattingContext(formattingContextRoot, formattingState)
{
}

void TableFormattingContext::layout() const
{
    // https://www.w3.org/TR/css-tables-3/#table-layout-algorithm
    // To layout a table, user agents must apply the following actions:

    // 1. Ensure each cell slot is occupied by at least one cell.
    ensureTableGrid();
    // 2. Compute the minimum width of each column.
    computePreferredWidthForColumns();
    // 3. Compute the width of the table.
    computeTableWidth();
    // 4. Distribute the width of the table among columns.
    distributeAvailabeWidth();
    // 5. Compute the height of the table.
    computeTableHeight();
    // 6. Distribute the height of the table among rows.
    distributeAvailableHeight();
}

FormattingContext::IntrinsicWidthConstraints TableFormattingContext::computedIntrinsicWidthConstraints() const
{
    return { };
}

void TableFormattingContext::ensureTableGrid() const
{
    auto& tableWrapperBox = downcast<Container>(root());
    auto& tableGrid = formattingState().tableGrid();

    for (auto* section = tableWrapperBox.firstChild(); section; section = section->nextSibling()) {
        ASSERT(section->isTableHeader() || section->isTableBody() || section->isTableFooter());
        for (auto* row = downcast<Container>(*section).firstChild(); row; row = row->nextSibling()) {
            ASSERT(row->isTableRow());
            for (auto* cell = downcast<Container>(*row).firstChild(); cell; cell = cell->nextSibling()) {
                ASSERT(cell->isTableCell());
                tableGrid.appendCell(*cell);
            }
        }
    }
}

void TableFormattingContext::computePreferredWidthForColumns() const
{
}

void TableFormattingContext::computeTableWidth() const
{
}

void TableFormattingContext::distributeAvailabeWidth() const
{
}

void TableFormattingContext::computeTableHeight() const
{
}

void TableFormattingContext::distributeAvailableHeight() const
{
}

}
}

#endif
