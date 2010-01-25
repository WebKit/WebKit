/*
 * Copyright (C) 2002 Lars Knoll (knoll@kde.org)
 *           (C) 2002 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006, 2008 Apple Inc. All rights reserved.
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
        if (child->isTableCol())
            toRenderTableCol(child)->calcPrefWidths();
        else if (child->isTableSection()) {
            RenderTableSection* section = toRenderTableSection(child);
            int numRows = section->numRows();
            RenderTableCell* last = 0;
            for (int i = 0; i < numRows; i++) {
                RenderTableSection::CellStruct current = section->cellAt(i, effCol);
                RenderTableCell* cell = current.cell;
                
                bool cellHasContent = cell && (cell->firstChild() || cell->style()->hasBorder() || cell->style()->hasPadding());
                if (cellHasContent)
                    l.emptyCellsOnly = false;
                    
                if (current.inColSpan)
                    continue;
                if (cell && cell->colSpan() == 1) {
                    // A cell originates in this column.  Ensure we have
                    // a min/max width of at least 1px for this column now.
                    l.minWidth = max(l.minWidth, cellHasContent ? 1 : 0);
                    l.maxWidth = max(l.maxWidth, 1);
                    if (cell->prefWidthsDirty())
                        cell->calcPrefWidths();
                    l.minWidth = max(cell->minPrefWidth(), l.minWidth);
                    if (cell->maxPrefWidth() > l.maxWidth) {
                        l.maxWidth = cell->maxPrefWidth();
                        maxContributor = cell;
                    }

                    Length w = cell->styleOrColWidth();
                    // FIXME: What is this arbitrary value?
                    if (w.rawValue() > 32760)
                        w.setRawValue(32760);
                    if (w.isNegative())
                        w.setValue(0);
                    switch (w.type()) {
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
                        if (w.isPositive() && (!l.width.isPercent() || w.rawValue() > l.width.rawValue()))
                            l.width = w;
                        break;
                    case Relative:
                        // FIXME: Need to understand this case and whether it makes sense to compare values
                        // which are not necessarily of the same type.
                        if (w.isAuto() || (w.isRelative() && w.value() > l.width.rawValue()))
                            l.width = w;
                    default:
                        break;
                    }
                } else {
                    if (cell && (!effCol || section->cellAt(i, effCol-1).cell != cell)) {
                        // This spanning cell originates in this column.  Ensure we have
                        // a min/max width of at least 1px for this column now.
                        l.minWidth = max(l.minWidth, cellHasContent ? 1 : 0);
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
            RenderTableCol *col = toRenderTableCol(child);
            int span = col->span();
            if (col->firstChild()) {
                grpWidth = col->style()->width();
            } else {
                Length w = col->style()->width();
                if (w.isAuto())
                    w = grpWidth;
                if ((w.isFixed() || w.isPercent()) && w.isZero())
                    w = Length();
                int cEffCol = m_table->colToEffCol(cCol);
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
            while (cb && !cb->isRenderView() && !cb->isTableCell() &&
                cb->style()->width().isAuto() && !cb->isPositioned())
                cb = cb->containingBlock();

            table = 0;
            if (cb && cb->isTableCell() &&
                (cb->style()->width().isAuto() || cb->style()->width().isPercent())) {
                if (tw.isPercent())
                    scale = false;
                else {
                    RenderTableCell* cell = toRenderTableCell(cb);
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

void AutoTableLayout::calcPrefWidths(int& minWidth, int& maxWidth)
{
    fullRecalc();

    int spanMaxWidth = calcEffectiveWidth();
    minWidth = 0;
    maxWidth = 0;
    float maxPercent = 0;
    float maxNonPercent = 0;
    bool scaleColumns = shouldScaleColumns(m_table);

    // We substitute 0 percent by (epsilon / percentScaleFactor) percent in two places below to avoid division by zero.
    // FIXME: Handle the 0% cases properly.
    const int epsilon = 1;

    int remainingPercent = 100 * percentScaleFactor;
    for (unsigned int i = 0; i < m_layoutStruct.size(); i++) {
        minWidth += m_layoutStruct[i].effMinWidth;
        maxWidth += m_layoutStruct[i].effMaxWidth;
        if (scaleColumns) {
            if (m_layoutStruct[i].effWidth.isPercent()) {
                int percent = min(m_layoutStruct[i].effWidth.rawValue(), remainingPercent);
                float pw = static_cast<float>(m_layoutStruct[i].effMaxWidth) * 100 * percentScaleFactor / max(percent, epsilon);
                maxPercent = max(pw,  maxPercent);
                remainingPercent -= percent;
            } else
                maxNonPercent += m_layoutStruct[i].effMaxWidth;
        }
    }

    if (scaleColumns) {
        maxNonPercent = maxNonPercent * 100 * percentScaleFactor / max(remainingPercent, epsilon);
        maxWidth = max(maxWidth, static_cast<int>(min(maxNonPercent, INT_MAX / 2.0f)));
        maxWidth = max(maxWidth, static_cast<int>(min(maxPercent, INT_MAX / 2.0f)));
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
}

/*
  This method takes care of colspans.
  effWidth is the same as width for cells without colspans. If we have colspans, they get modified.
 */
int AutoTableLayout::calcEffectiveWidth()
{
    float tMaxWidth = 0;

    unsigned int nEffCols = m_layoutStruct.size();
    int hspacing = m_table->hBorderSpacing();

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
        if (!w.isRelative() && w.isZero())
            w = Length(); // make it Auto

        int col = m_table->colToEffCol(cell->col());
        unsigned int lastCol = col;
        int cMinWidth = cell->minPrefWidth() + hspacing;
        float cMaxWidth = cell->maxPrefWidth() + hspacing;
        int totalPercent = 0;
        int minWidth = 0;
        float maxWidth = 0;
        bool allColsArePercent = true;
        bool allColsAreFixed = true;
        bool haveAuto = false;
        bool spanHasEmptyCellsOnly = true;
        int fixedWidth = 0;
        while (lastCol < nEffCols && span > 0) {
            switch (m_layoutStruct[lastCol].width.type()) {
            case Percent:
                totalPercent += m_layoutStruct[lastCol].width.rawValue();
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
                    totalPercent += m_layoutStruct[lastCol].effWidth.rawValue();
                allColsAreFixed = false;
            }
            if (!m_layoutStruct[lastCol].emptyCellsOnly)
                spanHasEmptyCellsOnly = false;
            span -= m_table->spanOfEffCol(lastCol);
            minWidth += m_layoutStruct[lastCol].effMinWidth;
            maxWidth += m_layoutStruct[lastCol].effMaxWidth;
            lastCol++;
            cMinWidth -= hspacing;
            cMaxWidth -= hspacing;
        }

        // adjust table max width if needed
        if (w.isPercent()) {
            if (totalPercent > w.rawValue() || allColsArePercent) {
                // can't satify this condition, treat as variable
                w = Length();
            } else {
                float spanMax = max(maxWidth, cMaxWidth);
                tMaxWidth = max(tMaxWidth, spanMax * 100 * percentScaleFactor / w.rawValue());

                // all non percent columns in the span get percent values to sum up correctly.
                int percentMissing = w.rawValue() - totalPercent;
                float totalWidth = 0;
                for (unsigned int pos = col; pos < lastCol; pos++) {
                    if (!(m_layoutStruct[pos].effWidth.isPercent()))
                        totalWidth += m_layoutStruct[pos].effMaxWidth;
                }

                for (unsigned int pos = col; pos < lastCol && totalWidth > 0; pos++) {
                    if (!(m_layoutStruct[pos].effWidth.isPercent())) {
                        int percent = static_cast<int>(percentMissing * static_cast<float>(m_layoutStruct[pos].effMaxWidth) / totalWidth);
                        totalWidth -= m_layoutStruct[pos].effMaxWidth;
                        percentMissing -= percent;
                        if (percent > 0)
                            m_layoutStruct[pos].effWidth.setRawValue(Percent, percent);
                        else
                            m_layoutStruct[pos].effWidth = Length();
                    }
                }

            }
        }

        // make sure minWidth and maxWidth of the spanning cell are honoured
        if (cMinWidth > minWidth) {
            if (allColsAreFixed) {
                for (unsigned int pos = col; fixedWidth > 0 && pos < lastCol; pos++) {
                    int w = max(m_layoutStruct[pos].effMinWidth, cMinWidth * m_layoutStruct[pos].width.value() / fixedWidth);
                    fixedWidth -= m_layoutStruct[pos].width.value();
                    cMinWidth -= w;
                    m_layoutStruct[pos].effMinWidth = w;
                }

            } else {
                float maxw = maxWidth;
                int minw = minWidth;
                
                // Give min to variable first, to fixed second, and to others third.
                for (unsigned int pos = col; maxw >= 0 && pos < lastCol; pos++) {
                    if (m_layoutStruct[pos].width.isFixed() && haveAuto && fixedWidth <= cMinWidth) {
                        int w = max(m_layoutStruct[pos].effMinWidth, m_layoutStruct[pos].width.value());
                        fixedWidth -= m_layoutStruct[pos].width.value();
                        minw -= m_layoutStruct[pos].effMinWidth;
                        maxw -= m_layoutStruct[pos].effMaxWidth;
                        cMinWidth -= w;
                        m_layoutStruct[pos].effMinWidth = w;
                    }
                }

                for (unsigned int pos = col; maxw >= 0 && pos < lastCol && minw < cMinWidth; pos++) {
                    if (!(m_layoutStruct[pos].width.isFixed() && haveAuto && fixedWidth <= cMinWidth)) {
                        int w = max(m_layoutStruct[pos].effMinWidth, static_cast<int>(maxw ? cMinWidth * static_cast<float>(m_layoutStruct[pos].effMaxWidth) / maxw : cMinWidth));
                        w = min(m_layoutStruct[pos].effMinWidth+(cMinWidth-minw), w);
                                                
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
                for (unsigned int pos = col; maxWidth >= 0 && pos < lastCol; pos++) {
                    int w = max(m_layoutStruct[pos].effMaxWidth, static_cast<int>(maxWidth ? cMaxWidth * static_cast<float>(m_layoutStruct[pos].effMaxWidth) / maxWidth : cMaxWidth));
                    maxWidth -= m_layoutStruct[pos].effMaxWidth;
                    cMaxWidth -= w;
                    m_layoutStruct[pos].effMaxWidth = w;
                }
            }
        } else {
            for (unsigned int pos = col; pos < lastCol; pos++)
                m_layoutStruct[pos].maxWidth = max(m_layoutStruct[pos].maxWidth, m_layoutStruct[pos].minWidth);
        }
        // treat span ranges consisting of empty cells only as if they had content
        if (spanHasEmptyCellsOnly)
            for (unsigned int pos = col; pos < lastCol; pos++)
                m_layoutStruct[pos].emptyCellsOnly = false;
    }
    m_effWidthDirty = false;

    return static_cast<int>(min(tMaxWidth, INT_MAX / 2.0f));
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
        m_spanCells.grow(size + 10);
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

    if (m_effWidthDirty)
        calcEffectiveWidth();

    bool havePercent = false;
    bool haveRelative = false;
    int totalRelative = 0;
    int numAuto = 0;
    int numFixed = 0;
    float totalAuto = 0;
    float totalFixed = 0;
    int totalPercent = 0;
    int allocAuto = 0;
    int numAutoEmptyCellsOnly = 0;

    // fill up every cell with its minWidth
    for (int i = 0; i < nEffCols; i++) {
        int w = m_layoutStruct[i].effMinWidth;
        m_layoutStruct[i].calcWidth = w;
        available -= w;
        Length& width = m_layoutStruct[i].effWidth;
        switch (width.type()) {
        case Percent:
            havePercent = true;
            totalPercent += width.rawValue();
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
            if (m_layoutStruct[i].emptyCellsOnly) 
                numAutoEmptyCellsOnly++;            
            else {
                numAuto++;
                totalAuto += m_layoutStruct[i].effMaxWidth;
                allocAuto += w;
            }
            break;
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
        if (totalPercent > 100 * percentScaleFactor) {
            // remove overallocated space from the last columns
            int excess = tableWidth*(totalPercent - 100 * percentScaleFactor) / (100 * percentScaleFactor);
            for (int i = nEffCols-1; i >= 0; i--) {
                if (m_layoutStruct[i].effWidth.isPercent()) {
                    int w = m_layoutStruct[i].calcWidth;
                    int reduction = min(w,  excess);
                    // the lines below might look inconsistent, but that's the way it's handled in mozilla
                    excess -= reduction;
                    int newWidth = max(static_cast<int>(m_layoutStruct[i].effMinWidth), w - reduction);
                    available += w - newWidth;
                    m_layoutStruct[i].calcWidth = newWidth;
                }
            }
        }
    }
    
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
            if (width.isAuto() && totalAuto != 0 && !m_layoutStruct[i].emptyCellsOnly) {
                int w = max(m_layoutStruct[i].calcWidth, static_cast<int>(available * static_cast<float>(m_layoutStruct[i].effMaxWidth) / totalAuto));
                available -= w;
                totalAuto -= m_layoutStruct[i].effMaxWidth;
                m_layoutStruct[i].calcWidth = w;
            }
        }
    }

    // spread over fixed columns
    if (available > 0 && numFixed) {
        // still have some width to spread, distribute to fixed columns
        for (int i = 0; i < nEffCols; i++) {
            Length &width = m_layoutStruct[i].effWidth;
            if (width.isFixed()) {
                int w = static_cast<int>(available * static_cast<float>(m_layoutStruct[i].effMaxWidth) / totalFixed);
                available -= w;
                totalFixed -= m_layoutStruct[i].effMaxWidth;
                m_layoutStruct[i].calcWidth += w;
            }
        }
    }

    // spread over percent colums
    if (available > 0 && m_hasPercent && totalPercent < 100 * percentScaleFactor) {
        // still have some width to spread, distribute weighted to percent columns
        for (int i = 0; i < nEffCols; i++) {
            Length &width = m_layoutStruct[i].effWidth;
            if (width.isPercent()) {
                int w = available * width.rawValue() / totalPercent;
                available -= w;
                totalPercent -= width.rawValue();
                m_layoutStruct[i].calcWidth += w;
                if (!available || !totalPercent) break;
            }
        }
    }

    // spread over the rest
    if (available > 0 && nEffCols > numAutoEmptyCellsOnly) {
        int total = nEffCols - numAutoEmptyCellsOnly;
        // still have some width to spread
        int i = nEffCols;
        while (i--) {
            // variable columns with empty cells only don't get any width
            if (m_layoutStruct[i].effWidth.isAuto() && m_layoutStruct[i].emptyCellsOnly)
                continue;
            int w = available / total;
            available -= w;
            total--;
            m_layoutStruct[i].calcWidth += w;
        }
    }

    // If we have overallocated, reduce every cell according to the difference between desired width and minwidth
    // this seems to produce to the pixel exact results with IE. Wonder is some of this also holds for width distributing.
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
        m_table->columnPositions()[i] = pos;
        pos += m_layoutStruct[i].calcWidth + m_table->hBorderSpacing();
    }
    m_table->columnPositions()[m_table->columnPositions().size() - 1] = pos;
}


void AutoTableLayout::calcPercentages() const
{
    unsigned totalPercent = 0;
    for (unsigned i = 0; i < m_layoutStruct.size(); i++) {
        if (m_layoutStruct[i].width.isPercent())
            totalPercent += m_layoutStruct[i].width.rawValue();
    }
    m_totalPercent = totalPercent / percentScaleFactor;
    m_percentagesDirty = false;
}

#undef DEBUG_LAYOUT

}
