/*
 * This file is part of the HTML rendering engine for KDE.
 *
 * Copyright (C) 2002 Lars Knoll (knoll@kde.org)
 *           (C) 2002 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
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
  table's m_width, the m_width of the columns, and borders or cell
  spacing.

  The table's m_width may be specified explicitly with the 'm_width'
  property. A value of 'auto' (for both 'display: table' and 'display:
  inline-table') means use the automatic table layout algorithm.

  In the fixed table layout algorithm, the m_width of each column is
  determined as follows:

    1. A column element with a value other than 'auto' for the 'm_width'
    property sets the m_width for that column.

    2. Otherwise, a cell in the first row with a value other than
    'auto' for the 'm_width' property sets the m_width for that column. If
    the cell spans more than one column, the m_width is divided over the
    columns.

    3. Any remaining columns equally divide the remaining horizontal
    table space (minus borders or cell spacing).

  The m_width of the table is then the greater of the value of the
  'm_width' property for the table element and the sum of the column
  widths (plus cell spacing or borders). If the table is wider than
  the columns, the extra space should be distributed over the columns.


  In this manner, the user agent can begin to lay out the table once
  the entire first row has been received. Cells in subsequent rows do
  not affect column widths. Any cell that has content that overflows
  uses the 'overflow' property to determine whether to clip the
  overflow content.

_____________________________________________________

  This is not quite true when comparing to IE. IE always honours
  table-layout:fixed and treats a variable table m_width as 100%. Makes
  a lot of sense, and is implemented here the same way.

*/

using namespace std;

namespace WebCore {

FixedTableLayout::FixedTableLayout(RenderTable* table)
    : TableLayout(table)
{
}

FixedTableLayout::~FixedTableLayout()
{
}

int FixedTableLayout::calcWidthArray(int tableWidth)
{
    int usedWidth = 0;

    // iterate over all <col> elements
    RenderObject* child = m_table->firstChild();
    int cCol = 0;
    int nEffCols = m_table->numEffCols();
    m_width.resize(nEffCols);
    m_width.fill(Length(Auto));

#ifdef DEBUG_LAYOUT
    qDebug("FixedTableLayout::calcWidthArray(%d)", tableWidth);
    qDebug("    col elements:");
#endif

    Length grpWidth;
    while (child) {
        if (child->isTableCol()) {
            RenderTableCol *col = static_cast<RenderTableCol *>(child);
            int span = col->span();
            if (col->firstChild())
                grpWidth = col->style()->width();
            else {
                Length w = col->style()->width();
                if (w.isAuto())
                    w = grpWidth;
                int effWidth = 0;
                if (w.isFixed() && w.value() > 0)
                    effWidth = w.value();
                
#ifdef DEBUG_LAYOUT
                qDebug("    col element: effCol=%d, span=%d: %d w=%d type=%d",
                       cCol, span, effWidth,  w.value, w.type());
#endif
                int usedSpan = 0;
                int i = 0;
                while (usedSpan < span) {
                    if(cCol + i >= nEffCols) {
                        m_table->appendColumn(span - usedSpan);
                        nEffCols++;
                        m_width.resize(nEffCols);
                        m_width[nEffCols-1] = Length();
                    }
                    int eSpan = m_table->spanOfEffCol(cCol+i);
                    if ((w.isFixed() || w.isPercent()) && w.value() > 0) {
                        m_width[cCol+i] = Length(w.value() * eSpan, w.type());
                        usedWidth += effWidth * eSpan;
#ifdef DEBUG_LAYOUT
                        qDebug("    setting effCol %d (span=%d) to m_width %d(type=%d)",
                               cCol+i, eSpan, m_width[cCol+i].value, m_width[cCol+i].type());
#endif
                    }
                    usedSpan += eSpan;
                    i++;
                }
                cCol += i;
            }
        } else
            break;

        RenderObject *next = child->firstChild();
        if (!next)
            next = child->nextSibling();
        if (!next && child->parent()->isTableCol()) {
            next = child->parent()->nextSibling();
            grpWidth = Length();
        }
        child = next;
    }

#ifdef DEBUG_LAYOUT
    qDebug("    first row:");
#endif
    // iterate over the first row in case some are unspecified.
    RenderTableSection *section = m_table->head;
    if (!section)
        section = m_table->firstBody;
    if (!section)
        section = m_table->foot;
    if (section) {
        cCol = 0;
        // FIXME: Technically the first row could be in an arbitrary section (e.g., an nth section
        // if the previous n-1 sections have no rows), so this check isn't good enough.
        // get the first cell in the first row
        RenderObject* firstRow = section->firstChild();
        child = firstRow ? firstRow->firstChild() : 0;
        while (child) {
            if (child->isTableCell()) {
                RenderTableCell *cell = static_cast<RenderTableCell *>(child);
                Length w = cell->styleOrColWidth();
                int span = cell->colSpan();
                int effWidth = 0;
                if ((w.isFixed() || w.isPercent()) && w.value() > 0)
                    effWidth = w.value();
                
#ifdef DEBUG_LAYOUT
                qDebug("    table cell: effCol=%d, span=%d: %d",  cCol, span, effWidth);
#endif
                int usedSpan = 0;
                int i = 0;
                while (usedSpan < span) {
                    ASSERT(cCol + i < nEffCols);
                    int eSpan = m_table->spanOfEffCol(cCol+i);
                    // only set if no col element has already set it.
                    if (m_width[cCol+i].isAuto() && w.type() != Auto) {
                        m_width[cCol+i] = Length(w.value() * eSpan, w.type());
                        usedWidth += effWidth*eSpan;
#ifdef DEBUG_LAYOUT
                        qDebug("    setting effCol %d (span=%d) to m_width %d(type=%d)",
                               cCol+i, eSpan, m_width[cCol+i].value, m_width[cCol+i].type());
#endif
                    }
#ifdef DEBUG_LAYOUT
                    else {
                        qDebug("    m_width of col %d already defined (span=%d)", cCol, m_table->spanOfEffCol(cCol));
                    }
#endif
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

void FixedTableLayout::calcMinMaxWidth()
{
    // FIXME: This entire calculation is incorrect for both minwidth and maxwidth.
    
    // we might want to wait until we have all of the first row before
    // layouting for the first time.

    // only need to calculate the minimum m_width as the sum of the
    // cols/cells with a fixed m_width.
    //
    // The maximum m_width is max(minWidth, tableWidth).
    int bs = m_table->bordersPaddingAndSpacing();
    
    int tableWidth = m_table->style()->width().isFixed() ? m_table->style()->width().value() - bs : 0;
    int mw = calcWidthArray(tableWidth) + bs;

    m_table->m_minWidth = max(mw, tableWidth);
    m_table->m_maxWidth = m_table->m_minWidth;
}

void FixedTableLayout::layout()
{
    int tableWidth = m_table->width() - m_table->bordersPaddingAndSpacing();
    int available = tableWidth;
    int nEffCols = m_table->numEffCols();
    int totalPercent = 0;
    
#ifdef DEBUG_LAYOUT
    qDebug("FixedTableLayout::layout: tableWidth=%d, numEffCols=%d",  tableWidth, nEffCols);
#endif


    Vector<int> calcWidth(nEffCols, -1);

    // assign  percent m_width
    if (available > 0) {
        for (int i = 0; i < nEffCols; i++)
            if (m_width[i].isPercent())
                totalPercent += m_width[i].value();

        // calculate how much to distribute to percent cells.
        int base = tableWidth * totalPercent / 100;
        if (base > available)
            base = available;
        else
            totalPercent = 100;

#ifdef DEBUG_LAYOUT
        qDebug("FixedTableLayout::layout: assigning percent m_width, base=%d, totalPercent=%d", base, totalPercent);
#endif
        for (int i = 0; available > 0 && i < nEffCols; i++) {
            if (m_width[i].isPercent()) {
                int w = base * m_width[i].value() / totalPercent;
                available -= w;
                calcWidth[i] = w;
            }
        }
    }
    
    // next assign fixed m_width
    for (int i = 0; i < nEffCols; i++) {
        if (m_width[i].isFixed()) {
            calcWidth[i] = m_width[i].value();
            available -= m_width[i].value();
        }
    }

    // assign variable m_width
    if (available > 0) {
        int totalAuto = 0;
        for (int i = 0; i < nEffCols; i++)
            if (m_width[i].isAuto())
                totalAuto++;

        for (int i = 0; available > 0 && i < nEffCols; i++) {
            if (m_width[i].isAuto()) {
                int w = available / totalAuto;
                available -= w;
                calcWidth[i] = w;
                totalAuto--;
            }
        }
    }

    for (int i = 0; i < nEffCols; i++)
        if (calcWidth[i] <= 0)
            calcWidth[i] = 0; // IE gives min 1 px...

    // spread extra space over columns
    if (available > 0) {
        int total = nEffCols;
        // still have some m_width to spread
        int i = nEffCols;
        while ( i--) {
            int w = available / total;
            available -= w;
            total--;
            calcWidth[i] += w;
        }
    }
    
    int pos = 0;
    int hspacing = m_table->hBorderSpacing();
    for (int i = 0; i < nEffCols; i++) {
#ifdef DEBUG_LAYOUT
        qDebug("col %d: %d (m_width %d)", i, pos, calcWidth[i]);
#endif
        m_table->columnPos[i] = pos;
        pos += calcWidth[i] + hspacing;
    }
    m_table->columnPos[m_table->columnPos.size()-1] = pos;
}

#undef DEBUG_LAYOUT

}
