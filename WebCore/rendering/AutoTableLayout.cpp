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
#include "AutoTableLayout.h"

#include "RenderTable.h"
#include "RenderTableCell.h"
#include "RenderTableCol.h"
#include "RenderTableSection.h"

using namespace std;

namespace WebCore {

AutoTableLayout::AutoTableLayout(RenderTable* table)
    : TableLayout(table)
    , m_hasPercent(false)
    , m_percentagesDirty(true)
    , m_effWidthDirty(true)
    , m_totalPercent(0)
{
}

AutoTableLayout::~AutoTableLayout()
{
}

/* recalculates the full structure needed to do layouting and minmax calculations.
   This is usually calculated on the fly, but needs to be done fully when table cells change
   dynamically
*/
void AutoTableLayout::recalcColumn(int effCol)
{
    Layout &l = m_layoutStruct[effCol];

    RenderObject* child = m_table->firstChild();
    // first we iterate over all rows.

    RenderTableCell* fixedContributor = 0;
    RenderTableCell* maxContributor = 0;

    while (child) {
        if (child->isTableSection()) {
            RenderTableSection* section = static_cast<RenderTableSection*>(child);
            int numRows = section->numRows();
            RenderTableCell* last = 0;
            for (int i = 0; i < numRows; i++) {
                RenderTableSection::CellStruct current = section->cellAt(i, effCol);
                RenderTableCell* cell = current.cell;
                
                if (current.inColSpan)
                    continue;
                if (cell && cell->colSpan() == 1) {
                    // A cell originates in this column.  Ensure we have
                    // a min/max width of at least 1px for this column now.
                    l.minWidth = max(l.minWidth, 1);
                    l.maxWidth = max(l.maxWidth, 1);
                    if (!cell->minMaxKnown())
                        cell->calcMinMaxWidth();
                    if (cell->minWidth() > l.minWidth)
                        l.minWidth = cell->minWidth();
                    if (cell->maxWidth() > l.maxWidth) {
                        l.maxWidth = cell->maxWidth();
                        maxContributor = cell;
                    }

                    Length w = cell->styleOrColWidth();
                    if (w.value() > 32760)
                        w.setValue(32760);
                    if (w.value() < 0)
                        w.setValue(0);
                    switch(w.type()) {
                    case Fixed:
                        // ignore width=0
                        if (w.value() > 0 && (int)l.width.type() != Percent) {
                            int wval = cell->calcBorderBoxWidth(w.value());
                            if (l.width.isFixed()) {
                                // Nav/IE weirdness
                                if ((wval > l.width.value()) ||
                                    ((l.width.value() == wval) && (maxContributor == cell))) {
                                    l.width.setValue(wval);
                                    fixedContributor = cell;
                                }
                            } else {
                                l.width.setValue(Fixed, wval);
                                fixedContributor = cell;
                            }
                        }
                        break;
                    case Percent:
                        m_hasPercent = true;
                        if (w.value() > 0 && (!l.width.isPercent() || w.value() > l.width.value()))
                            l.width = w;
                        break;
                    case Relative:
                        if (w.isAuto() || (w.isRelative() && w.value() > l.width.value()))
                            l.width = w;
                    default:
                        break;
                    }
                } else {
                    if (cell && (!effCol || section->cellAt(i, effCol-1).cell != cell)) {
                        // This spanning cell originates in this column.  Ensure we have
                        // a min/max width of at least 1px for this column now.
                        l.minWidth = max(l.minWidth, 1);
                        l.maxWidth = max(l.maxWidth, 1);
                        insertSpanCell(cell);
                    }
                    last = cell;
                }
            }
        }
        child = child->nextSibling();
    }

    // Nav/IE weirdness
    if (l.width.isFixed()) {
        if (m_table->style()->htmlHacks() && l.maxWidth > l.width.value() && fixedContributor != maxContributor) {
            l.width = Length();
            fixedContributor = 0;
        }
    }

    l.maxWidth = max(l.maxWidth, l.minWidth);
#ifdef DEBUG_LAYOUT
    qDebug("col %d, final min=%d, max=%d, width=%d(%d)", effCol, l.minWidth, l.maxWidth, l.width.value,  l.width.type);
#endif

    // ### we need to add col elements as well
}

void AutoTableLayout::fullRecalc()
{
    m_percentagesDirty = true;
    m_hasPercent = false;
    m_effWidthDirty = true;

    int nEffCols = m_table->numEffCols();
    m_layoutStruct.resize(nEffCols);
    m_layoutStruct.fill(Layout());
    m_spanCells.fill(0);

    RenderObject *child = m_table->firstChild();
    Length grpWidth;
    int cCol = 0;
    while (child) {
        if (child->isTableCol()) {
            RenderTableCol *col = static_cast<RenderTableCol*>(child);
            int span = col->span();
            if (col->firstChild()) {
                grpWidth = col->style()->width();
            } else {
                Length w = col->style()->width();
                if (w.isAuto())
                    w = grpWidth;
                if ((w.isFixed() && w.value() == 0) || (w.isPercent() && w.value() == 0))
                    w = Length();
                int cEffCol = m_table->colToEffCol(cCol);
#ifdef DEBUG_LAYOUT
                qDebug("    col element %d (eff=%d): Length=%d(%d), span=%d, effColSpan=%d",  cCol, cEffCol, w.value, w.type, span, m_table->spanOfEffCol(cEffCol));
#endif
                if (!w.isAuto() && span == 1 && cEffCol < nEffCols) {
                    if (m_table->spanOfEffCol(cEffCol) == 1) {
                        m_layoutStruct[cEffCol].width = w;
                        if (w.isFixed() && m_layoutStruct[cEffCol].maxWidth < w.value())
                            m_layoutStruct[cEffCol].maxWidth = w.value();
                    }
                }
                cCol += span;
            }
        } else {
            break;
        }

        RenderObject *next = child->firstChild();
        if (!next)
            next = child->nextSibling();
        if (!next && child->parent()->isTableCol()) {
            next = child->parent()->nextSibling();
            grpWidth = Length();
        }
        child = next;
    }


    for (int i = 0; i < nEffCols; i++)
        recalcColumn(i);
}

static bool shouldScaleColumns(RenderTable* table)
{
    // A special case.  If this table is not fixed width and contained inside
    // a cell, then don't bloat the maxwidth by examining percentage growth.
    bool scale = true;
    while (table) {
        Length tw = table->style()->width();
        if ((tw.isAuto() || tw.isPercent()) && !table->isPositioned()) {
            RenderBlock* cb = table->containingBlock();
            while (cb && !cb->isCanvas() && !cb->isTableCell() &&
                cb->style()->width().isAuto() && !cb->isPositioned())
                cb = cb->containingBlock();

            table = 0;
            if (cb && cb->isTableCell() &&
                (cb->style()->width().isAuto() || cb->style()->width().isPercent())) {
                if (tw.isPercent())
                    scale = false;
                else {
                    RenderTableCell* cell = static_cast<RenderTableCell*>(cb);
                    if (cell->colSpan() > 1 || cell->table()->style()->width().isAuto())
                        scale = false;
                    else
                        table = cell->table();
                }
            }
        }
        else
            table = 0;
    }
    return scale;
}

void AutoTableLayout::calcMinMaxWidth()
{
#ifdef DEBUG_LAYOUT
    qDebug("AutoTableLayout::calcMinMaxWidth");
#endif
    fullRecalc();

    int spanMaxWidth = calcEffectiveWidth();
    int minWidth = 0;
    int maxWidth = 0;
    int maxPercent = 0;
    int maxNonPercent = 0;

    int remainingPercent = 100;
    for (unsigned int i = 0; i < m_layoutStruct.size(); i++) {
        minWidth += m_layoutStruct[i].effMinWidth;
        maxWidth += m_layoutStruct[i].effMaxWidth;
        if (m_layoutStruct[i].effWidth.isPercent()) {
            int percent = min(m_layoutStruct[i].effWidth.value(), remainingPercent);
            int pw = (m_layoutStruct[i].effMaxWidth * 100) / max(percent, 1);
            remainingPercent -= percent;
            maxPercent = max(pw,  maxPercent);
        } else {
            maxNonPercent += m_layoutStruct[i].effMaxWidth;
        }
    }

    if (shouldScaleColumns(m_table)) {
        maxNonPercent = (maxNonPercent * 100 + 50) / max(remainingPercent, 1);
        maxWidth = max(maxNonPercent,  maxWidth);
        maxWidth = max(maxWidth, maxPercent);
    }

    maxWidth = max(maxWidth, spanMaxWidth);
    
    int bs = m_table->bordersPaddingAndSpacing();
    minWidth += bs;
    maxWidth += bs;

    Length tw = m_table->style()->width();
    if (tw.isFixed() && tw.value() > 0) {
        minWidth = max(minWidth, tw.value());
        maxWidth = minWidth;
    }

    m_table->m_maxWidth = maxWidth;
    m_table->m_minWidth = minWidth;
#ifdef DEBUG_LAYOUT
    qDebug("    minWidth=%d, maxWidth=%d", m_table->m_minWidth, m_table->m_maxWidth);
#endif
}

/*
  This method takes care of colspans.
  effWidth is the same as width for cells without colspans. If we have colspans, they get modified.
 */
int AutoTableLayout::calcEffectiveWidth()
{
    int tMaxWidth = 0;

    unsigned int nEffCols = m_layoutStruct.size();
    int hspacing = m_table->hBorderSpacing();
#ifdef DEBUG_LAYOUT
    qDebug("AutoTableLayout::calcEffectiveWidth for %d cols", nEffCols);
#endif
    for (unsigned int i = 0; i < nEffCols; i++) {
        m_layoutStruct[i].effWidth = m_layoutStruct[i].width;
        m_layoutStruct[i].effMinWidth = m_layoutStruct[i].minWidth;
        m_layoutStruct[i].effMaxWidth = m_layoutStruct[i].maxWidth;
    }

    for (unsigned int i = 0; i < m_spanCells.size(); i++) {
        RenderTableCell *cell = m_spanCells[i];
        if (!cell)
            break;
        int span = cell->colSpan();

        Length w = cell->styleOrColWidth();
        if (!w.isRelative() && w.value() == 0)
            w = Length(); // make it Auto

        int col = m_table->colToEffCol(cell->col());
        unsigned int lastCol = col;
        int cMinWidth = cell->minWidth() + hspacing;
        int cMaxWidth = cell->maxWidth() + hspacing;
        int totalPercent = 0;
        int minWidth = 0;
        int maxWidth = 0;
        bool allColsArePercent = true;
        bool allColsAreFixed = true;
        bool haveAuto = false;
        int fixedWidth = 0;
#ifdef DEBUG_LAYOUT
        int cSpan = span;
#endif
        while (lastCol < nEffCols && span > 0) {
            switch (m_layoutStruct[lastCol].width.type()) {
            case Percent:
                totalPercent += m_layoutStruct[lastCol].width.value();
                allColsAreFixed = false;
                break;
            case Fixed:
                if (m_layoutStruct[lastCol].width.value() > 0) {
                    fixedWidth += m_layoutStruct[lastCol].width.value();
                    allColsArePercent = false;
                    // IE resets effWidth to Auto here, but this breaks the konqueror about page and seems to be some bad
                    // legacy behaviour anyway. mozilla doesn't do this so I decided we don't neither.
                    break;
                }
                // fall through
            case Auto:
                haveAuto = true;
                // fall through
            default:
                // If the column is a percentage width, do not let the spanning cell overwrite the
                // width value.  This caused a mis-rendering on amazon.com.
                // Sample snippet:
                // <table border=2 width=100%><
                //   <tr><td>1</td><td colspan=2>2-3</tr>
                //   <tr><td>1</td><td colspan=2 width=100%>2-3</td></tr>
                // </table>
                if (!m_layoutStruct[lastCol].effWidth.isPercent()) {
                    m_layoutStruct[lastCol].effWidth = Length();
                    allColsArePercent = false;
                }
                else
                    totalPercent += m_layoutStruct[lastCol].effWidth.value();
                allColsAreFixed = false;
            }
            span -= m_table->spanOfEffCol(lastCol);
            minWidth += m_layoutStruct[lastCol].effMinWidth;
            maxWidth += m_layoutStruct[lastCol].effMaxWidth;
            lastCol++;
            cMinWidth -= hspacing;
            cMaxWidth -= hspacing;
        }
#ifdef DEBUG_LAYOUT
        qDebug("    colspan cell %p at effCol %d, span %d, type %d, value %d cmin=%d min=%d fixedwidth=%d", cell, col, cSpan, w.type, w.value, cMinWidth, minWidth, fixedWidth);
#endif

        // adjust table max width if needed
        if (w.isPercent()) {
            if (totalPercent > w.value() || allColsArePercent) {
                // can't satify this condition, treat as variable
                w = Length();
            } else {
                int spanMax = max(maxWidth, cMaxWidth);
#ifdef DEBUG_LAYOUT
                qDebug("    adjusting tMaxWidth (%d): spanMax=%d, value=%d, totalPercent=%d", tMaxWidth, spanMax, w.value, totalPercent);
#endif
                tMaxWidth = max(tMaxWidth, spanMax * 100 / w.value());

                // all non percent columns in the span get percent vlaues to sum up correctly.
                int percentMissing = w.value() - totalPercent;
                int totalWidth = 0;
                for (unsigned int pos = col; pos < lastCol; pos++) {
                    if (!(m_layoutStruct[pos].width.isPercent()))
                        totalWidth += m_layoutStruct[pos].effMaxWidth;
                }

                for (unsigned int pos = col; pos < lastCol && totalWidth > 0; pos++) {
                    if (!(m_layoutStruct[pos].width.isPercent())) {
                        int percent = percentMissing * m_layoutStruct[pos].effMaxWidth / totalWidth;
#ifdef DEBUG_LAYOUT
                        qDebug("   col %d: setting percent value %d effMaxWidth=%d totalWidth=%d", pos, percent, m_layoutStruct[pos].effMaxWidth, totalWidth);
#endif
                        totalWidth -= m_layoutStruct[pos].effMaxWidth;
                        percentMissing -= percent;
                        if (percent > 0)
                            m_layoutStruct[pos].effWidth = Length(percent, Percent);
                        else
                            m_layoutStruct[pos].effWidth = Length();
                    }
                }

            }
        }

        // make sure minWidth and maxWidth of the spanning cell are honoured
        if (cMinWidth > minWidth) {
            if (allColsAreFixed) {
#ifdef DEBUG_LAYOUT
                qDebug("extending minWidth of cols %d-%d to %dpx currentMin=%d accroding to fixed sum %d", col, lastCol-1, cMinWidth, minWidth, fixedWidth);
#endif
                for (unsigned int pos = col; fixedWidth > 0 && pos < lastCol; pos++) {
                    int w = max(m_layoutStruct[pos].effMinWidth, cMinWidth * m_layoutStruct[pos].width.value() / fixedWidth);
#ifdef DEBUG_LAYOUT
                    qDebug("   col %d: min=%d, effMin=%d, new=%d", pos, m_layoutStruct[pos].effMinWidth, m_layoutStruct[pos].effMinWidth, w);
#endif
                    fixedWidth -= m_layoutStruct[pos].width.value();
                    cMinWidth -= w;
                    m_layoutStruct[pos].effMinWidth = w;
                }

            } else {
#ifdef DEBUG_LAYOUT
                qDebug("extending minWidth of cols %d-%d to %dpx currentMin=%d", col, lastCol-1, cMinWidth, minWidth);
#endif
                int maxw = maxWidth;
                int minw = minWidth;
                
                // Give min to variable first, to fixed second, and to others third.
                for (unsigned int pos = col; maxw >= 0 && pos < lastCol; pos++) {
                    if (m_layoutStruct[pos].width.isFixed() && haveAuto && fixedWidth <= cMinWidth) {
                        int w = max(m_layoutStruct[pos].effMinWidth, m_layoutStruct[pos].width.value());
                        fixedWidth -= m_layoutStruct[pos].width.value();
                        minw -= m_layoutStruct[pos].effMinWidth;
#ifdef DEBUG_LAYOUT
                        qDebug("   col %d: min=%d, effMin=%d, new=%d", pos, m_layoutStruct[pos].effMinWidth, m_layoutStruct[pos].effMinWidth, w);
#endif
                        maxw -= m_layoutStruct[pos].effMaxWidth;
                        cMinWidth -= w;
                        m_layoutStruct[pos].effMinWidth = w;
                    }
                }

                for (unsigned int pos = col; maxw >= 0 && pos < lastCol && minw < cMinWidth; pos++) {
                    if (!(m_layoutStruct[pos].width.isFixed() && haveAuto && fixedWidth <= cMinWidth)) {
                        int w = max(m_layoutStruct[pos].effMinWidth, maxw ? (cMinWidth * m_layoutStruct[pos].effMaxWidth / maxw) : cMinWidth);
                        w = min(m_layoutStruct[pos].effMinWidth+(cMinWidth-minw), w);
                                                
#ifdef DEBUG_LAYOUT
                        qDebug("   col %d: min=%d, effMin=%d, new=%d", pos, m_layoutStruct[pos].effMinWidth, m_layoutStruct[pos].effMinWidth, w);
#endif
                        maxw -= m_layoutStruct[pos].effMaxWidth;
                        minw -= m_layoutStruct[pos].effMinWidth;
                        cMinWidth -= w;
                        m_layoutStruct[pos].effMinWidth = w;
                    }
                }
            }
        }
        if (!(w.isPercent())) {
            if (cMaxWidth > maxWidth) {
#ifdef DEBUG_LAYOUT
                qDebug("extending maxWidth of cols %d-%d to %dpx", col, lastCol-1, cMaxWidth);
#endif
                for (unsigned int pos = col; maxWidth >= 0 && pos < lastCol; pos++) {
                    int w = max(m_layoutStruct[pos].effMaxWidth, maxWidth ? (cMaxWidth * m_layoutStruct[pos].effMaxWidth / maxWidth) : cMaxWidth);
#ifdef DEBUG_LAYOUT
                    qDebug("   col %d: max=%d, effMax=%d, new=%d", pos, m_layoutStruct[pos].effMaxWidth, m_layoutStruct[pos].effMaxWidth, w);
#endif
                    maxWidth -= m_layoutStruct[pos].effMaxWidth;
                    cMaxWidth -= w;
                    m_layoutStruct[pos].effMaxWidth = w;
                }
            }
        } else {
            for (unsigned int pos = col; pos < lastCol; pos++)
                m_layoutStruct[pos].maxWidth = max(m_layoutStruct[pos].maxWidth, m_layoutStruct[pos].minWidth);
        }
    }
    m_effWidthDirty = false;

    return tMaxWidth;
}

/* gets all cells that originate in a column and have a cellspan > 1
   Sorts them by increasing cellspan
*/
void AutoTableLayout::insertSpanCell(RenderTableCell *cell)
{
    if (!cell || cell->colSpan() == 1)
        return;

    int size = m_spanCells.size();
    if (!size || m_spanCells[size-1] != 0) {
        m_spanCells.resize(size + 10);
        for (int i = 0; i < 10; i++)
            m_spanCells[size+i] = 0;
        size += 10;
    }

    // add them in sort. This is a slow algorithm, and a binary search or a fast sorting after collection would be better
    unsigned int pos = 0;
    int span = cell->colSpan();
    while (pos < m_spanCells.size() && m_spanCells[pos] && span > m_spanCells[pos]->colSpan())
        pos++;
    memmove(m_spanCells.data()+pos+1, m_spanCells.data()+pos, (size-pos-1)*sizeof(RenderTableCell *));
    m_spanCells[pos] = cell;
}


void AutoTableLayout::layout()
{
    // table layout based on the values collected in the layout structure.
    int tableWidth = m_table->width() - m_table->bordersPaddingAndSpacing();
    int available = tableWidth;
    int nEffCols = m_table->numEffCols();

    if (nEffCols != (int)m_layoutStruct.size()) {
        fullRecalc();
        nEffCols = m_table->numEffCols();
    }
#ifdef DEBUG_LAYOUT
    qDebug("AutoTableLayout::layout()");
#endif

    if (m_effWidthDirty)
        calcEffectiveWidth();

#ifdef DEBUG_LAYOUT
    qDebug("    tableWidth=%d,  nEffCols=%d", tableWidth,  nEffCols);
    for (int i = 0; i < nEffCols; i++) {
        qDebug("    effcol %d is of type %d value %d, minWidth=%d, maxWidth=%d",
               i, m_layoutStruct[i].width.type, m_layoutStruct[i].width.value,
               m_layoutStruct[i].minWidth, m_layoutStruct[i].maxWidth);
        qDebug("        effective: type %d value %d, minWidth=%d, maxWidth=%d",
               m_layoutStruct[i].effWidth.type, m_layoutStruct[i].effWidth.value,
               m_layoutStruct[i].effMinWidth, m_layoutStruct[i].effMaxWidth);
    }
#endif

    bool havePercent = false;
    bool haveRelative = false;
    int totalRelative = 0;
    int numAuto = 0;
    int numFixed = 0;
    int totalAuto = 0;
    int totalFixed = 0;
    int totalPercent = 0;
    int allocAuto = 0;

    // fill up every cell with its minWidth
    for (int i = 0; i < nEffCols; i++) {
        int w = m_layoutStruct[i].effMinWidth;
        m_layoutStruct[i].calcWidth = w;
        available -= w;
        Length& width = m_layoutStruct[i].effWidth;
        switch (width.type()) {
        case Percent:
            havePercent = true;
            totalPercent += width.value();
            break;
        case Relative:
            haveRelative = true;
            totalRelative += width.value();
            break;
        case Fixed:
            numFixed++;
            totalFixed += m_layoutStruct[i].effMaxWidth;
            // fall through
            break;
        case Auto:
        case Static:
            numAuto++;
            totalAuto += m_layoutStruct[i].effMaxWidth;
            allocAuto += w;
        default:
            break;
        }
    }

    // allocate width to percent cols
    if (available > 0 && havePercent) {
        for (int i = 0; i < nEffCols; i++) {
            Length &width = m_layoutStruct[i].effWidth;
            if (width.isPercent()) {
                int w = max(int(m_layoutStruct[i].effMinWidth), width.calcMinValue(tableWidth));
                available += m_layoutStruct[i].calcWidth - w;
                m_layoutStruct[i].calcWidth = w;
            }
        }
        if (totalPercent > 100) {
            // remove overallocated space from the last columns
            int excess = tableWidth*(totalPercent-100)/100;
            for (int i = nEffCols-1; i >= 0; i--) {
                if (m_layoutStruct[i].effWidth.isPercent()) {
                    int w = m_layoutStruct[i].calcWidth;
                    int reduction = min(w,  excess);
                    // the lines below might look inconsistent, but that's the way it's handled in mozilla
                    excess -= reduction;
                    int newWidth = max(int (m_layoutStruct[i].effMinWidth), w - reduction);
                    available += w - newWidth;
                    m_layoutStruct[i].calcWidth = newWidth;
                }
            }
        }
    }
#ifdef DEBUG_LAYOUT
    qDebug("percent satisfied: available is %d", available);
#endif
    
    // then allocate width to fixed cols
    if (available > 0) {
        for (int i = 0; i < nEffCols; ++i) {
            Length &width = m_layoutStruct[i].effWidth;
            if (width.isFixed() && width.value() > m_layoutStruct[i].calcWidth) {
                available += m_layoutStruct[i].calcWidth - width.value();
                m_layoutStruct[i].calcWidth = width.value();
            }
        }
    }
#ifdef DEBUG_LAYOUT
    qDebug("fixed satisfied: available is %d", available);
#endif

    // now satisfy relative
    if (available > 0) {
        for (int i = 0; i < nEffCols; i++) {
            Length &width = m_layoutStruct[i].effWidth;
            if (width.isRelative() && width.value() != 0) {
                // width=0* gets effMinWidth.
                int w = width.value() * tableWidth / totalRelative;
                available += m_layoutStruct[i].calcWidth - w;
                m_layoutStruct[i].calcWidth = w;
            }
        }
    }

    // now satisfy variable
    if (available > 0 && numAuto) {
        available += allocAuto; // this gets redistributed
        for (int i = 0; i < nEffCols; i++) {
            Length &width = m_layoutStruct[i].effWidth;
            if (width.isAuto() && totalAuto != 0) {
                int w = max(int(m_layoutStruct[i].calcWidth), available * m_layoutStruct[i].effMaxWidth / totalAuto);
                available -= w;
                totalAuto -= m_layoutStruct[i].effMaxWidth;
                m_layoutStruct[i].calcWidth = w;
            }
        }
    }
#ifdef DEBUG_LAYOUT
    qDebug("variable satisfied: available is %d",  available);
#endif

    // spread over fixed columns
    if (available > 0 && numFixed) {
        // still have some width to spread, distribute to fixed columns
        for (int i = 0; i < nEffCols; i++) {
            Length &width = m_layoutStruct[i].effWidth;
            if (width.isFixed()) {
                int w = available * m_layoutStruct[i].effMaxWidth / totalFixed;
                available -= w;
                totalFixed -= m_layoutStruct[i].effMaxWidth;
                m_layoutStruct[i].calcWidth += w;
            }
        }
    }
    
#ifdef DEBUG_LAYOUT
    qDebug("after fixed distribution: available=%d",  available);
#endif
    
    // spread over percent colums
    if (available > 0 && m_hasPercent && totalPercent < 100) {
        // still have some width to spread, distribute weighted to percent columns
        for (int i = 0; i < nEffCols; i++) {
            Length &width = m_layoutStruct[i].effWidth;
            if (width.isPercent()) {
                int w = available * width.value() / totalPercent;
                available -= w;
                totalPercent -= width.value();
                m_layoutStruct[i].calcWidth += w;
                if (!available || !totalPercent) break;
            }
        }
    }

#ifdef DEBUG_LAYOUT
    qDebug("after percent distribution: available=%d",  available);
#endif

    // spread over the rest
    if (available > 0) {
        int total = nEffCols;
        // still have some width to spread
        int i = nEffCols;
        while ( i--) {
            int w = available / total;
            available -= w;
            total--;
            m_layoutStruct[i].calcWidth += w;
        }
    }

#ifdef DEBUG_LAYOUT
    qDebug("after equal distribution: available=%d",  available);
#endif
    // if we have overallocated, reduce every cell according to the difference between desired width and minwidth
    // this seems to produce to the pixel exaxt results with IE. Wonder is some of this also holds for width distributing.
    if (available < 0) {
        // Need to reduce cells with the following prioritization:
        // (1) Auto
        // (2) Relative
        // (3) Fixed
        // (4) Percent
        // This is basically the reverse of how we grew the cells.
        if (available < 0) {
            int mw = 0;
            for (int i = nEffCols-1; i >= 0; i--) {
                Length &width = m_layoutStruct[i].effWidth;
                if (width.isAuto())
                    mw += m_layoutStruct[i].calcWidth - m_layoutStruct[i].effMinWidth;
            }
            
            for (int i = nEffCols-1; i >= 0 && mw > 0; i--) {
                Length &width = m_layoutStruct[i].effWidth;
                if (width.isAuto()) {
                    int minMaxDiff = m_layoutStruct[i].calcWidth-m_layoutStruct[i].effMinWidth;
                    int reduce = available * minMaxDiff / mw;
                    m_layoutStruct[i].calcWidth += reduce;
                    available -= reduce;
                    mw -= minMaxDiff;
                    if (available >= 0)
                        break;
                }
            }
        }

        if (available < 0) {
            int mw = 0;
            for (int i = nEffCols-1; i >= 0; i--) {
                Length& width = m_layoutStruct[i].effWidth;
                if (width.isRelative())
                    mw += m_layoutStruct[i].calcWidth - m_layoutStruct[i].effMinWidth;
            }
            
            for (int i = nEffCols-1; i >= 0 && mw > 0; i--) {
                Length& width = m_layoutStruct[i].effWidth;
                if (width.isRelative()) {
                    int minMaxDiff = m_layoutStruct[i].calcWidth-m_layoutStruct[i].effMinWidth;
                    int reduce = available * minMaxDiff / mw;
                    m_layoutStruct[i].calcWidth += reduce;
                    available -= reduce;
                    mw -= minMaxDiff;
                    if (available >= 0)
                        break;
                }
            }
        }

        if (available < 0) {
            int mw = 0;
            for (int i = nEffCols-1; i >= 0; i--) {
                Length& width = m_layoutStruct[i].effWidth;
                if (width.isFixed())
                    mw += m_layoutStruct[i].calcWidth - m_layoutStruct[i].effMinWidth;
            }
            
            for (int i = nEffCols-1; i >= 0 && mw > 0; i--) {
                Length& width = m_layoutStruct[i].effWidth;
                if (width.isFixed()) {
                    int minMaxDiff = m_layoutStruct[i].calcWidth-m_layoutStruct[i].effMinWidth;
                    int reduce = available * minMaxDiff / mw;
                    m_layoutStruct[i].calcWidth += reduce;
                    available -= reduce;
                    mw -= minMaxDiff;
                    if (available >= 0)
                        break;
                }
            }
        }

        if (available < 0) {
            int mw = 0;
            for (int i = nEffCols-1; i >= 0; i--) {
                Length& width = m_layoutStruct[i].effWidth;
                if (width.isPercent())
                    mw += m_layoutStruct[i].calcWidth - m_layoutStruct[i].effMinWidth;
            }
            
            for (int i = nEffCols-1; i >= 0 && mw > 0; i--) {
                Length& width = m_layoutStruct[i].effWidth;
                if (width.isPercent()) {
                    int minMaxDiff = m_layoutStruct[i].calcWidth-m_layoutStruct[i].effMinWidth;
                    int reduce = available * minMaxDiff / mw;
                    m_layoutStruct[i].calcWidth += reduce;
                    available -= reduce;
                    mw -= minMaxDiff;
                    if (available >= 0)
                        break;
                }
            }
        }
    }

    int pos = 0;
    for (int i = 0; i < nEffCols; i++) {
#ifdef DEBUG_LAYOUT
        qDebug("col %d: %d (width %d)", i, pos, m_layoutStruct[i].calcWidth);
#endif
        m_table->columnPos[i] = pos;
        pos += m_layoutStruct[i].calcWidth + m_table->hBorderSpacing();
    }
    m_table->columnPos[m_table->columnPos.size()-1] = pos;
}


void AutoTableLayout::calcPercentages() const
{
    m_totalPercent = 0;
    for (unsigned i = 0; i < m_layoutStruct.size(); i++) {
        if (m_layoutStruct[i].width.isPercent())
            m_totalPercent += m_layoutStruct[i].width.value();
    }
    m_percentagesDirty = false;
}

#undef DEBUG_LAYOUT

}
