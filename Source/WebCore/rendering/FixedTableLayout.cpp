/*
 * Copyright (C) 2002 Lars Knoll (knoll@kde.org)
 *           (C) 2002 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "FixedTableLayout.h"

#include "RenderTable.h"
#include "RenderTableCell.h"
#include "RenderTableCol.h"
#include "RenderTableSection.h"

/*
  The text below is from the CSS 2.1 specs.

  Fixed table layout

  With this (fast) algorithm, the horizontal layout of the table does
  not depend on the contents of the cells; it only depends on the
  table's width, the width of the columns, and borders or cell
  spacing.

  The table's width may be specified explicitly with the 'width'
  property. A value of 'auto' (for both 'display: table' and 'display:
  inline-table') means use the automatic table layout algorithm.

  In the fixed table layout algorithm, the width of each column is
  determined as follows:

    1. A column element with a value other than 'auto' for the 'width'
    property sets the width for that column.

    2. Otherwise, a cell in the first row with a value other than
    'auto' for the 'width' property sets the width for that column. If
    the cell spans more than one column, the width is divided over the
    columns.

    3. Any remaining columns equally divide the remaining horizontal
    table space (minus borders or cell spacing).

  The width of the table is then the greater of the value of the
  'width' property for the table element and the sum of the column
  widths (plus cell spacing or borders). If the table is wider than
  the columns, the extra space should be distributed over the columns.


  In this manner, the user agent can begin to lay out the table once
  the entire first row has been received. Cells in subsequent rows do
  not affect column widths. Any cell that has content that overflows
  uses the 'overflow' property to determine whether to clip the
  overflow content.
*/

using namespace std;

namespace WebCore {

FixedTableLayout::FixedTableLayout(RenderTable* table)
    : TableLayout(table)
{
}

static RenderObject* nextCol(RenderObject* child)
{
    // If child is a colgroup, the next col is the colgroup's first child col.
    if (RenderObject* next = child->firstChild())
        return next;
    // Otherwise it's the next col along.
    if (RenderObject* next = child->nextSibling())
        return next;
    // Failing that, the child is the last col in a colgroup, so the next col is the next col/colgroup after its colgroup.
    if (child->parent()->isTableCol())
        return child->parent()->nextSibling();
    return 0;
}

int FixedTableLayout::calcWidthArray(int)
{
    int usedWidth = 0;

    // iterate over all <col> elements
    unsigned nEffCols = m_table->numEffCols();
    m_width.resize(nEffCols);
    m_width.fill(Length(Auto));

    unsigned currentEffectiveColumn = 0;
    for (RenderObject* child = m_table->firstChild();child && child->isTableCol(); child = nextCol(child)) {

        // Width specified by column-groups does not affect column width in fixed layout tables
        RenderTableCol* col = toRenderTableCol(child);
        col->computePreferredLogicalWidths();

        if (col->isTableColGroup())
            continue;

        Length colStyleLogicalWidth = col->style()->logicalWidth();
        int effectiveColWidth = 0;
        if (colStyleLogicalWidth.isFixed() && colStyleLogicalWidth.value() > 0)
            effectiveColWidth = colStyleLogicalWidth.value();

        unsigned span = col->span();
        while (span) {
            unsigned spanInCurrentEffectiveColumn;
            if (currentEffectiveColumn >= nEffCols) {
                m_table->appendColumn(span);
                nEffCols++;
                m_width.append(Length());
                spanInCurrentEffectiveColumn = span;
            } else {
                if (span < m_table->spanOfEffCol(currentEffectiveColumn)) {
                    m_table->splitColumn(currentEffectiveColumn, span);
                    nEffCols++;
                    m_width.append(Length());
                }
                spanInCurrentEffectiveColumn = m_table->spanOfEffCol(currentEffectiveColumn);
            }
            if ((colStyleLogicalWidth.isFixed() || colStyleLogicalWidth.isPercent()) && colStyleLogicalWidth.isPositive()) {
                m_width[currentEffectiveColumn] = colStyleLogicalWidth;
                m_width[currentEffectiveColumn] *= spanInCurrentEffectiveColumn;
                usedWidth += effectiveColWidth * spanInCurrentEffectiveColumn;
            }
            span -= spanInCurrentEffectiveColumn;
            currentEffectiveColumn++;
        }
    }

    // Iterate over the first row in case some are unspecified.
    RenderTableSection* section = m_table->topNonEmptySection();
    if (section) {
        unsigned cCol = 0;
        RenderObject* firstRow = section->firstChild();
        RenderObject* child = firstRow->firstChild();
        while (child) {
            if (child->isTableCell()) {
                RenderTableCell* cell = toRenderTableCell(child);
                if (cell->preferredLogicalWidthsDirty())
                    cell->computePreferredLogicalWidths();

                Length w = cell->styleOrColLogicalWidth();
                unsigned span = cell->colSpan();
                int effectiveColWidth = 0;
                if (w.isFixed() && w.isPositive()) {
                    w.setValue(w.value() + cell->borderAndPaddingLogicalWidth());
                    effectiveColWidth = w.value();
                }
                
                unsigned usedSpan = 0;
                unsigned i = 0;
                while (usedSpan < span && cCol + i < nEffCols) {
                    float eSpan = m_table->spanOfEffCol(cCol + i);
                    // Only set if no col element has already set it.
                    if (m_width[cCol + i].isAuto() && w.type() != Auto) {
                        m_width[cCol + i] = w;
                        m_width[cCol + i] *= eSpan / span;
                        usedWidth += effectiveColWidth * eSpan / span;
                    }
                    usedSpan += eSpan;
                    i++;
                }
                cCol += i;
            }
            child = child->nextSibling();
        }
    }

    return usedWidth;
}

// Use a very large value (in effect infinite). But not too large!
// numeric_limits<int>::max() will too easily overflow widths.
// Keep this in synch with BLOCK_MAX_WIDTH in RenderBlock.cpp
#define TABLE_MAX_WIDTH 15000

void FixedTableLayout::computePreferredLogicalWidths(LayoutUnit& minWidth, LayoutUnit& maxWidth)
{
    // FIXME: This entire calculation is incorrect for both minwidth and maxwidth.
    
    // we might want to wait until we have all of the first row before
    // layouting for the first time.

    // only need to calculate the minimum width as the sum of the
    // cols/cells with a fixed width.
    //
    // The maximum width is max(minWidth, tableWidth).
    int bordersPaddingAndSpacing = m_table->bordersPaddingAndSpacingInRowDirection();

    int tableLogicalWidth = m_table->style()->logicalWidth().isFixed() ? m_table->style()->logicalWidth().value() - bordersPaddingAndSpacing : 0;
    int mw = calcWidthArray(tableLogicalWidth) + bordersPaddingAndSpacing;

    minWidth = max(mw, tableLogicalWidth);
    maxWidth = minWidth;

    // This quirk is very similar to one that exists in RenderBlock::calcBlockPrefWidths().
    // Here's the example for this one:
    /*
        <table style="width:100%; background-color:red"><tr><td>
            <table style="background-color:blue"><tr><td>
                <table style="width:100%; background-color:green; table-layout:fixed"><tr><td>
                    Content
                </td></tr></table>
            </td></tr></table>
        </td></tr></table>
    */ 
    // In this example, the two inner tables should be as large as the outer table. 
    // We can achieve this effect by making the maxwidth of fixed tables with percentage
    // widths be infinite.
    if (m_table->document()->inQuirksMode() && m_table->style()->logicalWidth().isPercent() && maxWidth < TABLE_MAX_WIDTH)
        maxWidth = TABLE_MAX_WIDTH;
}

void FixedTableLayout::layout()
{
    int tableLogicalWidth = m_table->logicalWidth() - m_table->bordersPaddingAndSpacingInRowDirection();
    unsigned nEffCols = m_table->numEffCols();

    // FIXME: It is possible to be called without having properly updated our internal representation.
    // This means that our preferred logical widths were not recomputed as expected.
    if (nEffCols != m_width.size()) {
        calcWidthArray(tableLogicalWidth);
        // FIXME: Table layout shouldn't modify our table structure (but does due to columns and column-groups).
        nEffCols = m_table->numEffCols();
    }

    Vector<int> calcWidth(nEffCols, 0);

    unsigned numAuto = 0;
    unsigned autoSpan = 0;
    int totalFixedWidth = 0;
    int totalPercentWidth = 0;
    float totalPercent = 0;

    // Compute requirements and try to satisfy fixed and percent widths.
    // Percentages are of the table's width, so for example
    // for a table width of 100px with columns (40px, 10%), the 10% compute
    // to 10px here, and will scale up to 20px in the final (80px, 20px).
    for (unsigned i = 0; i < nEffCols; i++) {
        if (m_width[i].isFixed()) {
            calcWidth[i] = m_width[i].value();
            totalFixedWidth += calcWidth[i];
        } else if (m_width[i].isPercent()) {
            calcWidth[i] = valueForLength(m_width[i], tableLogicalWidth);
            totalPercentWidth += calcWidth[i];
            totalPercent += m_width[i].percent();
        } else if (m_width[i].isAuto()) {
            numAuto++;
            autoSpan += m_table->spanOfEffCol(i);
        }
    }

    int hspacing = m_table->hBorderSpacing();
    int totalWidth = totalFixedWidth + totalPercentWidth;
    if (!numAuto || totalWidth > tableLogicalWidth) {
        // If there are no auto columns, or if the total is too wide, take
        // what we have and scale it to fit as necessary.
        if (totalWidth != tableLogicalWidth) {
            // Fixed widths only scale up
            if (totalFixedWidth && totalWidth < tableLogicalWidth) {
                totalFixedWidth = 0;
                for (unsigned i = 0; i < nEffCols; i++) {
                    if (m_width[i].isFixed()) {
                        calcWidth[i] = calcWidth[i] * tableLogicalWidth / totalWidth;
                        totalFixedWidth += calcWidth[i];
                    }
                }
            }
            if (totalPercent) {
                totalPercentWidth = 0;
                for (unsigned i = 0; i < nEffCols; i++) {
                    if (m_width[i].isPercent()) {
                        calcWidth[i] = m_width[i].percent() * (tableLogicalWidth - totalFixedWidth) / totalPercent;
                        totalPercentWidth += calcWidth[i];
                    }
                }
            }
            totalWidth = totalFixedWidth + totalPercentWidth;
        }
    } else {
        // Divide the remaining width among the auto columns.
        ASSERT(autoSpan >= numAuto);
        int remainingWidth = tableLogicalWidth - totalFixedWidth - totalPercentWidth - hspacing * (autoSpan - numAuto);
        int lastAuto = 0;
        for (unsigned i = 0; i < nEffCols; i++) {
            if (m_width[i].isAuto()) {
                unsigned span = m_table->spanOfEffCol(i);
                int w = remainingWidth * span / autoSpan;
                calcWidth[i] = w + hspacing * (span - 1);
                remainingWidth -= w;
                if (!remainingWidth)
                    break;
                lastAuto = i;
                numAuto--;
                ASSERT(autoSpan >= span);
                autoSpan -= span;
            }
        }
        // Last one gets the remainder.
        if (remainingWidth)
            calcWidth[lastAuto] += remainingWidth;
        totalWidth = tableLogicalWidth;
    }

    if (totalWidth < tableLogicalWidth) {
        // Spread extra space over columns.
        int remainingWidth = tableLogicalWidth - totalWidth;
        int total = nEffCols;
        while (total) {
            int w = remainingWidth / total;
            remainingWidth -= w;
            calcWidth[--total] += w;
        }
        if (nEffCols > 0)
            calcWidth[nEffCols - 1] += remainingWidth;
    }
    
    int pos = 0;
    for (unsigned i = 0; i < nEffCols; i++) {
        m_table->columnPositions()[i] = pos;
        pos += calcWidth[i] + hspacing;
    }
    int colPositionsSize = m_table->columnPositions().size();
    if (colPositionsSize > 0)
        m_table->columnPositions()[colPositionsSize - 1] = pos;
}

} // namespace WebCore
