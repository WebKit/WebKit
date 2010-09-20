/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
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
#include "RenderTableCell.h"

#include "FloatQuad.h"
#include "GraphicsContext.h"
#include "HTMLNames.h"
#include "HTMLTableCellElement.h"
#include "RenderTableCol.h"
#include "RenderView.h"
#include "TransformState.h"

using namespace std;

namespace WebCore {

using namespace HTMLNames;

RenderTableCell::RenderTableCell(Node* node)
    : RenderBlock(node)
    , m_row(-1)
    , m_column(-1)
    , m_rowSpan(1)
    , m_columnSpan(1)
    , m_intrinsicPaddingTop(0)
    , m_intrinsicPaddingBottom(0)
    , m_percentageHeight(0)
{
    updateFromElement();
}

void RenderTableCell::destroy()
{
    RenderTableSection* recalcSection = parent() ? section() : 0;

    RenderBlock::destroy();

    if (recalcSection)
        recalcSection->setNeedsCellRecalc();
}

void RenderTableCell::updateFromElement()
{
    Node* n = node();
    if (n && (n->hasTagName(tdTag) || n->hasTagName(thTag))) {
        HTMLTableCellElement* tc = static_cast<HTMLTableCellElement*>(n);
        int oldRSpan = m_rowSpan;
        int oldCSpan = m_columnSpan;

        m_columnSpan = tc->colSpan();
        m_rowSpan = tc->rowSpan();
        if ((oldRSpan != m_rowSpan || oldCSpan != m_columnSpan) && style() && parent()) {
            setNeedsLayoutAndPrefWidthsRecalc();
            if (section())
                section()->setNeedsCellRecalc();
        }
    }
}

Length RenderTableCell::styleOrColWidth() const
{
    Length w = style()->width();
    if (!w.isAuto())
        return w;

    RenderTableCol* tableCol = table()->colElement(col());

    if (tableCol) {
        int colSpanCount = colSpan();

        Length colWidthSum = Length(0, Fixed);
        for (int i = 1; i <= colSpanCount; i++) {
            Length colWidth = tableCol->style()->width();

            // Percentage value should be returned only for colSpan == 1.
            // Otherwise we return original width for the cell.
            if (!colWidth.isFixed()) {
                if (colSpanCount > 1)
                    return w;
                return colWidth;
            }

            colWidthSum = Length(colWidthSum.value() + colWidth.value(), Fixed);

            tableCol = table()->nextColElement(tableCol);
            // If no next <col> tag found for the span we just return what we have for now.
            if (!tableCol)
                break;
        }

        // Column widths specified on <col> apply to the border box of the cell.
        // Percentages don't need to be handled since they're always treated this way (even when specified on the cells).
        // See Bugzilla bug 8126 for details.
        if (colWidthSum.isFixed() && colWidthSum.value() > 0)
            colWidthSum = Length(max(0, colWidthSum.value() - borderAndPaddingWidth()), Fixed);
        return colWidthSum;
    }

    return w;
}

void RenderTableCell::calcPrefWidths()
{
    // The child cells rely on the grids up in the sections to do their calcPrefWidths work.  Normally the sections are set up early, as table
    // cells are added, but relayout can cause the cells to be freed, leaving stale pointers in the sections'
    // grids.  We must refresh those grids before the child cells try to use them.
    table()->recalcSectionsIfNeeded();

    RenderBlock::calcPrefWidths();
    if (node() && style()->autoWrap()) {
        // See if nowrap was set.
        Length w = styleOrColWidth();
        String nowrap = static_cast<Element*>(node())->getAttribute(nowrapAttr);
        if (!nowrap.isNull() && w.isFixed())
            // Nowrap is set, but we didn't actually use it because of the
            // fixed width set on the cell.  Even so, it is a WinIE/Moz trait
            // to make the minwidth of the cell into the fixed width.  They do this
            // even in strict mode, so do not make this a quirk.  Affected the top
            // of hiptop.com.
            m_minPrefWidth = max(w.value(), m_minPrefWidth);
    }
}

void RenderTableCell::calcWidth()
{
}

void RenderTableCell::updateWidth(int w)
{
    if (w != width()) {
        setWidth(w);
        setCellWidthChanged(true);
    }
}

void RenderTableCell::layout()
{
    layoutBlock(cellWidthChanged());
    setCellWidthChanged(false);
}

int RenderTableCell::paddingTop(bool includeIntrinsicPadding) const
{
    return RenderBlock::paddingTop() + (includeIntrinsicPadding ? intrinsicPaddingTop() : 0);
}

int RenderTableCell::paddingBottom(bool includeIntrinsicPadding) const
{
    return RenderBlock::paddingBottom() + (includeIntrinsicPadding ? intrinsicPaddingBottom() : 0);
}

void RenderTableCell::setOverrideSize(int size)
{
    clearIntrinsicPadding();
    RenderBlock::setOverrideSize(size);
}

IntSize RenderTableCell::offsetFromContainer(RenderObject* o, const IntPoint& point) const
{
    ASSERT(o == container());

    IntSize offset = RenderBlock::offsetFromContainer(o, point);
    if (parent())
        offset.expand(-parentBox()->x(), -parentBox()->y());

    return offset;
}

IntRect RenderTableCell::clippedOverflowRectForRepaint(RenderBoxModelObject* repaintContainer)
{
    // If the table grid is dirty, we cannot get reliable information about adjoining cells,
    // so we ignore outside borders. This should not be a problem because it means that
    // the table is going to recalculate the grid, relayout and repaint its current rect, which
    // includes any outside borders of this cell.
    if (!table()->collapseBorders() || table()->needsSectionRecalc())
        return RenderBlock::clippedOverflowRectForRepaint(repaintContainer);

    bool rtl = table()->style()->direction() == RTL;
    int outlineSize = style()->outlineSize();
    int left = max(borderHalfLeft(true), outlineSize);
    int right = max(borderHalfRight(true), outlineSize);
    int top = max(borderHalfTop(true), outlineSize);
    int bottom = max(borderHalfBottom(true), outlineSize);
    if ((left && !rtl) || (right && rtl)) {
        if (RenderTableCell* before = table()->cellBefore(this)) {
            top = max(top, before->borderHalfTop(true));
            bottom = max(bottom, before->borderHalfBottom(true));
        }
    }
    if ((left && rtl) || (right && !rtl)) {
        if (RenderTableCell* after = table()->cellAfter(this)) {
            top = max(top, after->borderHalfTop(true));
            bottom = max(bottom, after->borderHalfBottom(true));
        }
    }
    if (top) {
        if (RenderTableCell* above = table()->cellAbove(this)) {
            left = max(left, above->borderHalfLeft(true));
            right = max(right, above->borderHalfRight(true));
        }
    }
    if (bottom) {
        if (RenderTableCell* below = table()->cellBelow(this)) {
            left = max(left, below->borderHalfLeft(true));
            right = max(right, below->borderHalfRight(true));
        }
    }
    left = max(left, -leftVisibleOverflow());
    top = max(top, -topVisibleOverflow());
    IntRect r(-left, - top, left + max(width() + right, rightVisibleOverflow()), top + max(height() + bottom, bottomVisibleOverflow()));

    if (RenderView* v = view()) {
        // FIXME: layoutDelta needs to be applied in parts before/after transforms and
        // repaint containers. https://bugs.webkit.org/show_bug.cgi?id=23308
        r.move(v->layoutDelta());
    }
    computeRectForRepaint(repaintContainer, r);
    return r;
}

void RenderTableCell::computeRectForRepaint(RenderBoxModelObject* repaintContainer, IntRect& r, bool fixed)
{
    if (repaintContainer == this)
        return;
    r.setY(r.y());
    RenderView* v = view();
    if ((!v || !v->layoutStateEnabled() || repaintContainer) && parent())
        r.move(-parentBox()->x(), -parentBox()->y()); // Rows are in the same coordinate space, so don't add their offset in.
    RenderBlock::computeRectForRepaint(repaintContainer, r, fixed);
}

int RenderTableCell::baselinePosition(bool firstLine, bool isRootLineBox) const
{
    if (isRootLineBox)
        return RenderBox::baselinePosition(firstLine, isRootLineBox);

    // <http://www.w3.org/TR/2007/CR-CSS21-20070719/tables.html#height-layout>: The baseline of a cell is the baseline of
    // the first in-flow line box in the cell, or the first in-flow table-row in the cell, whichever comes first. If there
    // is no such line box or table-row, the baseline is the bottom of content edge of the cell box.
    int firstLineBaseline = firstLineBoxBaseline();
    if (firstLineBaseline != -1)
        return firstLineBaseline;
    return paddingTop() + borderTop() + contentHeight();
}

void RenderTableCell::styleWillChange(StyleDifference diff, const RenderStyle* newStyle)
{
    if (parent() && section() && style() && style()->height() != newStyle->height())
        section()->setNeedsCellRecalc();

    ASSERT(newStyle->display() == TABLE_CELL);

    RenderBlock::styleWillChange(diff, newStyle);
}

void RenderTableCell::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBlock::styleDidChange(diff, oldStyle);
    setHasBoxDecorations(true);
}

// The following rules apply for resolving conflicts and figuring out which border
// to use.
// (1) Borders with the 'border-style' of 'hidden' take precedence over all other conflicting 
// borders. Any border with this value suppresses all borders at this location.
// (2) Borders with a style of 'none' have the lowest priority. Only if the border properties of all 
// the elements meeting at this edge are 'none' will the border be omitted (but note that 'none' is 
// the default value for the border style.)
// (3) If none of the styles are 'hidden' and at least one of them is not 'none', then narrow borders 
// are discarded in favor of wider ones. If several have the same 'border-width' then styles are preferred 
// in this order: 'double', 'solid', 'dashed', 'dotted', 'ridge', 'outset', 'groove', and the lowest: 'inset'.
// (4) If border styles differ only in color, then a style set on a cell wins over one on a row, 
// which wins over a row group, column, column group and, lastly, table. It is undefined which color 
// is used when two elements of the same type disagree.
static int compareBorders(const CollapsedBorderValue& border1, const CollapsedBorderValue& border2)
{
    // Sanity check the values passed in. The null border have lowest priority.
    if (!border2.exists()) {
        if (!border1.exists())
            return 0;
        return 1;
    }
    if (!border1.exists())
        return -1;

    // Rule #1 above.
    if (border2.style() == BHIDDEN) {
        if (border1.style() == BHIDDEN)
            return 0;
        return -1;
    }
    if (border1.style() == BHIDDEN)
        return 1;
    
    // Rule #2 above.  A style of 'none' has lowest priority and always loses to any other border.
    if (border2.style() == BNONE) {
        if (border1.style() == BNONE)
            return 0;
        return 1;
    }
    if (border1.style() == BNONE)
        return -1;

    // The first part of rule #3 above. Wider borders win.
    if (border1.width() != border2.width())
        return border1.width() < border2.width() ? -1 : 1;
    
    // The borders have equal width.  Sort by border style.
    if (border1.style() != border2.style())
        return border1.style() < border2.style() ? -1 : 1;
    
    // The border have the same width and style.  Rely on precedence (cell over row over row group, etc.)
    if (border1.precedence() == border2.precedence())
        return 0;
    return border1.precedence() < border2.precedence() ? -1 : 1;
}

static CollapsedBorderValue chooseBorder(const CollapsedBorderValue& border1, const CollapsedBorderValue& border2)
{
    const CollapsedBorderValue& border = compareBorders(border1, border2) < 0 ? border2 : border1;
    return border.style() == BHIDDEN ? CollapsedBorderValue() : border;
}

CollapsedBorderValue RenderTableCell::collapsedLeftBorder(bool rtl) const
{
    RenderTable* tableElt = table();
    bool leftmostColumn;
    if (!rtl)
        leftmostColumn = col() == 0;
    else {
        int effCol = tableElt->colToEffCol(col() + colSpan() - 1);
        leftmostColumn = effCol == tableElt->numEffCols() - 1;
    }
    
    // For border left, we need to check, in order of precedence:
    // (1) Our left border.
    int left = CSSPropertyBorderLeftColor;
    int right = CSSPropertyBorderRightColor;
    CollapsedBorderValue result(&style()->borderLeft(), style()->visitedDependentColor(left), BCELL);
    
    // (2) The right border of the cell to the left.
    RenderTableCell* prevCell = rtl ? tableElt->cellAfter(this) : tableElt->cellBefore(this);
    if (prevCell) {
        CollapsedBorderValue prevCellBorder = CollapsedBorderValue(&prevCell->style()->borderRight(), prevCell->style()->visitedDependentColor(right), BCELL);
        result = rtl ? chooseBorder(result, prevCellBorder) : chooseBorder(prevCellBorder, result);
        if (!result.exists())
            return result;
    } else if (leftmostColumn) {
        // (3) Our row's left border.
        result = chooseBorder(result, CollapsedBorderValue(&parent()->style()->borderLeft(), parent()->style()->visitedDependentColor(left), BROW));
        if (!result.exists())
            return result;
        
        // (4) Our row group's left border.
        result = chooseBorder(result, CollapsedBorderValue(&section()->style()->borderLeft(), section()->style()->visitedDependentColor(left), BROWGROUP));
        if (!result.exists())
            return result;
    }
    
    // (5) Our column and column group's left borders.
    bool startColEdge;
    bool endColEdge;
    RenderTableCol* colElt = tableElt->colElement(col() + (rtl ? colSpan() - 1 : 0), &startColEdge, &endColEdge);
    if (colElt && (!rtl ? startColEdge : endColEdge)) {
        result = chooseBorder(result, CollapsedBorderValue(&colElt->style()->borderLeft(), colElt->style()->visitedDependentColor(left), BCOL));
        if (!result.exists())
            return result;
        if (colElt->parent()->isTableCol() && (!rtl ? !colElt->previousSibling() : !colElt->nextSibling())) {
            result = chooseBorder(result, CollapsedBorderValue(&colElt->parent()->style()->borderLeft(), colElt->parent()->style()->visitedDependentColor(left), BCOLGROUP));
            if (!result.exists())
                return result;
        }
    }
    
    // (6) The right border of the column to the left.
    if (!leftmostColumn) {
        colElt = tableElt->colElement(col() + (rtl ? colSpan() : -1), &startColEdge, &endColEdge);
        if (colElt && (!rtl ? endColEdge : startColEdge)) {
            CollapsedBorderValue rightBorder = CollapsedBorderValue(&colElt->style()->borderRight(), colElt->style()->visitedDependentColor(right), BCOL);
            result = rtl ? chooseBorder(result, rightBorder) : chooseBorder(rightBorder, result);
            if (!result.exists())
                return result;
        }
    } else {
        // (7) The table's left border.
        result = chooseBorder(result, CollapsedBorderValue(&tableElt->style()->borderLeft(), tableElt->style()->visitedDependentColor(left), BTABLE));
        if (!result.exists())
            return result;
    }
    
    return result;
}

CollapsedBorderValue RenderTableCell::collapsedRightBorder(bool rtl) const
{
    RenderTable* tableElt = table();
    bool rightmostColumn;
    if (rtl)
        rightmostColumn = col() == 0;
    else {
        int effCol = tableElt->colToEffCol(col() + colSpan() - 1);
        rightmostColumn = effCol == tableElt->numEffCols() - 1;
    }
    
    // For border right, we need to check, in order of precedence:
    // (1) Our right border.
    int left = CSSPropertyBorderLeftColor;
    int right = CSSPropertyBorderRightColor;
    CollapsedBorderValue result = CollapsedBorderValue(&style()->borderRight(), style()->visitedDependentColor(right), BCELL);
    
    // (2) The left border of the cell to the right.
    if (!rightmostColumn) {
        RenderTableCell* nextCell = rtl ? tableElt->cellBefore(this) : tableElt->cellAfter(this);
        if (nextCell && nextCell->style()) {
            CollapsedBorderValue leftBorder = CollapsedBorderValue(&nextCell->style()->borderLeft(), nextCell->style()->visitedDependentColor(left), BCELL);
            result = rtl ? chooseBorder(leftBorder, result) : chooseBorder(result, leftBorder);
            if (!result.exists())
                return result;
        }
    } else {
        // (3) Our row's right border.
        result = chooseBorder(result, CollapsedBorderValue(&parent()->style()->borderRight(), parent()->style()->visitedDependentColor(right), BROW));
        if (!result.exists())
            return result;
        
        // (4) Our row group's right border.
        result = chooseBorder(result, CollapsedBorderValue(&section()->style()->borderRight(), section()->style()->visitedDependentColor(right), BROWGROUP));
        if (!result.exists())
            return result;
    }
    
    // (5) Our column and column group's right borders.
    bool startColEdge;
    bool endColEdge;
    RenderTableCol* colElt = tableElt->colElement(col() + (rtl ? 0 : colSpan() - 1), &startColEdge, &endColEdge);
    if (colElt && (!rtl ? endColEdge : startColEdge)) {
        result = chooseBorder(result, CollapsedBorderValue(&colElt->style()->borderRight(), colElt->style()->visitedDependentColor(right), BCOL));
        if (!result.exists())
            return result;
        if (colElt->parent()->isTableCol() && (!rtl ? !colElt->nextSibling() : !colElt->previousSibling())) {
            result = chooseBorder(result, CollapsedBorderValue(&colElt->parent()->style()->borderRight(), colElt->parent()->style()->visitedDependentColor(right), BCOLGROUP));
            if (!result.exists())
                return result;
        }
    }
    
    // (6) The left border of the column to the right.
    if (!rightmostColumn) {
        colElt = tableElt->colElement(col() + (rtl ? -1 : colSpan()), &startColEdge, &endColEdge);
        if (colElt && (!rtl ? startColEdge : endColEdge)) {
            CollapsedBorderValue leftBorder = CollapsedBorderValue(&colElt->style()->borderLeft(), colElt->style()->visitedDependentColor(left), BCOL);
            result = rtl ? chooseBorder(leftBorder, result) : chooseBorder(result, leftBorder);
            if (!result.exists())
                return result;
        }
    } else {
        // (7) The table's right border.
        result = chooseBorder(result, CollapsedBorderValue(&tableElt->style()->borderRight(), tableElt->style()->visitedDependentColor(right), BTABLE));
        if (!result.exists())
            return result;
    }
    
    return result;
}

CollapsedBorderValue RenderTableCell::collapsedTopBorder() const
{
    // For border top, we need to check, in order of precedence:
    // (1) Our top border.
    int top = CSSPropertyBorderTopColor;
    int bottom = CSSPropertyBorderBottomColor;
    CollapsedBorderValue result = CollapsedBorderValue(&style()->borderTop(), style()->visitedDependentColor(top), BCELL);
    
    RenderTableCell* prevCell = table()->cellAbove(this);
    if (prevCell) {
        // (2) A previous cell's bottom border.
        result = chooseBorder(CollapsedBorderValue(&prevCell->style()->borderBottom(), prevCell->style()->visitedDependentColor(bottom), BCELL), result);
        if (!result.exists()) 
            return result;
    }
    
    // (3) Our row's top border.
    result = chooseBorder(result, CollapsedBorderValue(&parent()->style()->borderTop(), parent()->style()->visitedDependentColor(top), BROW));
    if (!result.exists())
        return result;
    
    // (4) The previous row's bottom border.
    if (prevCell) {
        RenderObject* prevRow = 0;
        if (prevCell->section() == section())
            prevRow = parent()->previousSibling();
        else
            prevRow = prevCell->section()->lastChild();
    
        if (prevRow) {
            result = chooseBorder(CollapsedBorderValue(&prevRow->style()->borderBottom(), prevRow->style()->visitedDependentColor(bottom), BROW), result);
            if (!result.exists())
                return result;
        }
    }
    
    // Now check row groups.
    RenderTableSection* currSection = section();
    if (!row()) {
        // (5) Our row group's top border.
        result = chooseBorder(result, CollapsedBorderValue(&currSection->style()->borderTop(), currSection->style()->visitedDependentColor(top), BROWGROUP));
        if (!result.exists())
            return result;
        
        // (6) Previous row group's bottom border.
        currSection = table()->sectionAbove(currSection);
        if (currSection) {
            result = chooseBorder(CollapsedBorderValue(&currSection->style()->borderBottom(), currSection->style()->visitedDependentColor(bottom), BROWGROUP), result);
            if (!result.exists())
                return result;
        }
    }
    
    if (!currSection) {
        // (8) Our column and column group's top borders.
        RenderTableCol* colElt = table()->colElement(col());
        if (colElt) {
            result = chooseBorder(result, CollapsedBorderValue(&colElt->style()->borderTop(), colElt->style()->visitedDependentColor(top), BCOL));
            if (!result.exists())
                return result;
            if (colElt->parent()->isTableCol()) {
                result = chooseBorder(result, CollapsedBorderValue(&colElt->parent()->style()->borderTop(), colElt->parent()->style()->visitedDependentColor(top), BCOLGROUP));
                if (!result.exists())
                    return result;
            }
        }
        
        // (9) The table's top border.
        RenderTable* enclosingTable = table();
        result = chooseBorder(result, CollapsedBorderValue(&enclosingTable->style()->borderTop(), enclosingTable->style()->visitedDependentColor(top), BTABLE));
        if (!result.exists())
            return result;
    }
    
    return result;
}

CollapsedBorderValue RenderTableCell::collapsedBottomBorder() const
{
    // For border top, we need to check, in order of precedence:
    // (1) Our bottom border.
    int top = CSSPropertyBorderTopColor;
    int bottom = CSSPropertyBorderBottomColor;
    CollapsedBorderValue result = CollapsedBorderValue(&style()->borderBottom(), style()->visitedDependentColor(bottom), BCELL);
    
    RenderTableCell* nextCell = table()->cellBelow(this);
    if (nextCell) {
        // (2) A following cell's top border.
        result = chooseBorder(result, CollapsedBorderValue(&nextCell->style()->borderTop(), nextCell->style()->visitedDependentColor(top), BCELL));
        if (!result.exists())
            return result;
    }
    
    // (3) Our row's bottom border. (FIXME: Deal with rowspan!)
    result = chooseBorder(result, CollapsedBorderValue(&parent()->style()->borderBottom(), parent()->style()->visitedDependentColor(bottom), BROW));
    if (!result.exists())
        return result;
    
    // (4) The next row's top border.
    if (nextCell) {
        result = chooseBorder(result, CollapsedBorderValue(&nextCell->parent()->style()->borderTop(), nextCell->parent()->style()->visitedDependentColor(top), BROW));
        if (!result.exists())
            return result;
    }
    
    // Now check row groups.
    RenderTableSection* currSection = section();
    if (row() + rowSpan() >= currSection->numRows()) {
        // (5) Our row group's bottom border.
        result = chooseBorder(result, CollapsedBorderValue(&currSection->style()->borderBottom(), currSection->style()->visitedDependentColor(bottom), BROWGROUP));
        if (!result.exists())
            return result;
        
        // (6) Following row group's top border.
        currSection = table()->sectionBelow(currSection);
        if (currSection) {
            result = chooseBorder(result, CollapsedBorderValue(&currSection->style()->borderTop(), currSection->style()->visitedDependentColor(top), BROWGROUP));
            if (!result.exists())
                return result;
        }
    }
    
    if (!currSection) {
        // (8) Our column and column group's bottom borders.
        RenderTableCol* colElt = table()->colElement(col());
        if (colElt) {
            result = chooseBorder(result, CollapsedBorderValue(&colElt->style()->borderBottom(), colElt->style()->visitedDependentColor(bottom), BCOL));
            if (!result.exists()) return result;
            if (colElt->parent()->isTableCol()) {
                result = chooseBorder(result, CollapsedBorderValue(&colElt->parent()->style()->borderBottom(), colElt->parent()->style()->visitedDependentColor(bottom), BCOLGROUP));
                if (!result.exists())
                    return result;
            }
        }
        
        // (9) The table's bottom border.
        RenderTable* enclosingTable = table();
        result = chooseBorder(result, CollapsedBorderValue(&enclosingTable->style()->borderBottom(), enclosingTable->style()->visitedDependentColor(bottom), BTABLE));
        if (!result.exists())
            return result;
    }
    
    return result;    
}

int RenderTableCell::borderLeft() const
{
    return table()->collapseBorders() ? borderHalfLeft(false) : RenderBlock::borderLeft();
}

int RenderTableCell::borderRight() const
{
    return table()->collapseBorders() ? borderHalfRight(false) : RenderBlock::borderRight();
}

int RenderTableCell::borderTop() const
{
    return table()->collapseBorders() ? borderHalfTop(false) : RenderBlock::borderTop();
}

int RenderTableCell::borderBottom() const
{
    return table()->collapseBorders() ? borderHalfBottom(false) : RenderBlock::borderBottom();
}

int RenderTableCell::borderHalfLeft(bool outer) const
{
    CollapsedBorderValue border = collapsedLeftBorder(table()->style()->direction() == RTL);
    if (border.exists())
        return (border.width() + (outer ? 0 : 1)) / 2; // Give the extra pixel to top and left.
    return 0;
}
    
int RenderTableCell::borderHalfRight(bool outer) const
{
    CollapsedBorderValue border = collapsedRightBorder(table()->style()->direction() == RTL);
    if (border.exists())
        return (border.width() + (outer ? 1 : 0)) / 2;
    return 0;
}

int RenderTableCell::borderHalfTop(bool outer) const
{
    CollapsedBorderValue border = collapsedTopBorder();
    if (border.exists())
        return (border.width() + (outer ? 0 : 1)) / 2; // Give the extra pixel to top and left.
    return 0;
}

int RenderTableCell::borderHalfBottom(bool outer) const
{
    CollapsedBorderValue border = collapsedBottomBorder();
    if (border.exists())
        return (border.width() + (outer ? 1 : 0)) / 2;
    return 0;
}

void RenderTableCell::paint(PaintInfo& paintInfo, int tx, int ty)
{
    if (paintInfo.phase == PaintPhaseCollapsedTableBorders && style()->visibility() == VISIBLE) {
        if (!paintInfo.shouldPaintWithinRoot(this))
            return;

        tx += x();
        ty += y();
        int os = 2 * maximalOutlineSize(paintInfo.phase);
        if (ty - table()->outerBorderTop() < paintInfo.rect.bottom() + os &&
            ty + height() + table()->outerBorderBottom() > paintInfo.rect.y() - os)
            paintCollapsedBorder(paintInfo.context, tx, ty, width(), height());
        return;
    } 
    
    RenderBlock::paint(paintInfo, tx, ty);
}

static EBorderStyle collapsedBorderStyle(EBorderStyle style)
{
    if (style == OUTSET)
        return GROOVE;
    if (style == INSET)
        return RIDGE;
    return style;
}

struct CollapsedBorder {
    CollapsedBorderValue borderValue;
    BoxSide side;
    bool shouldPaint;
    int x1;
    int y1;
    int x2;
    int y2;
    EBorderStyle style;
};

class CollapsedBorders {
public:
    CollapsedBorders()
        : m_count(0)
    {
    }
    
    void addBorder(const CollapsedBorderValue& borderValue, BoxSide borderSide, bool shouldPaint,
                   int x1, int y1, int x2, int y2, EBorderStyle borderStyle)
    {
        if (borderValue.exists() && shouldPaint) {
            m_borders[m_count].borderValue = borderValue;
            m_borders[m_count].side = borderSide;
            m_borders[m_count].shouldPaint = shouldPaint;
            m_borders[m_count].x1 = x1;
            m_borders[m_count].x2 = x2;
            m_borders[m_count].y1 = y1;
            m_borders[m_count].y2 = y2;
            m_borders[m_count].style = borderStyle;
            m_count++;
        }
    }

    CollapsedBorder* nextBorder()
    {
        for (int i = 0; i < m_count; i++) {
            if (m_borders[i].borderValue.exists() && m_borders[i].shouldPaint) {
                m_borders[i].shouldPaint = false;
                return &m_borders[i];
            }
        }
        
        return 0;
    }
    
    CollapsedBorder m_borders[4];
    int m_count;
};

static void addBorderStyle(RenderTableCell::CollapsedBorderStyles& borderStyles, CollapsedBorderValue borderValue)
{
    if (!borderValue.exists())
        return;
    size_t count = borderStyles.size();
    for (size_t i = 0; i < count; ++i)
        if (borderStyles[i] == borderValue)
            return;
    borderStyles.append(borderValue);
}

void RenderTableCell::collectBorderStyles(CollapsedBorderStyles& borderStyles) const
{
    bool rtl = table()->style()->direction() == RTL;
    addBorderStyle(borderStyles, collapsedLeftBorder(rtl));
    addBorderStyle(borderStyles, collapsedRightBorder(rtl));
    addBorderStyle(borderStyles, collapsedTopBorder());
    addBorderStyle(borderStyles, collapsedBottomBorder());
}

static int compareBorderStylesForQSort(const void* pa, const void* pb)
{
    const CollapsedBorderValue* a = static_cast<const CollapsedBorderValue*>(pa);
    const CollapsedBorderValue* b = static_cast<const CollapsedBorderValue*>(pb);
    if (*a == *b)
        return 0;
    return compareBorders(*a, *b);
}

void RenderTableCell::sortBorderStyles(CollapsedBorderStyles& borderStyles)
{
    qsort(borderStyles.data(), borderStyles.size(), sizeof(CollapsedBorderValue),
        compareBorderStylesForQSort);
}

void RenderTableCell::paintCollapsedBorder(GraphicsContext* graphicsContext, int tx, int ty, int w, int h)
{
    if (!table()->currentBorderStyle())
        return;
    
    bool rtl = table()->style()->direction() == RTL;
    CollapsedBorderValue leftVal = collapsedLeftBorder(rtl);
    CollapsedBorderValue rightVal = collapsedRightBorder(rtl);
    CollapsedBorderValue topVal = collapsedTopBorder();
    CollapsedBorderValue bottomVal = collapsedBottomBorder();
     
    // Adjust our x/y/width/height so that we paint the collapsed borders at the correct location.
    int topWidth = topVal.width();
    int bottomWidth = bottomVal.width();
    int leftWidth = leftVal.width();
    int rightWidth = rightVal.width();
    
    tx -= leftWidth / 2;
    ty -= topWidth / 2;
    w += leftWidth / 2 + (rightWidth + 1) / 2;
    h += topWidth / 2 + (bottomWidth + 1) / 2;
    
    EBorderStyle topStyle = collapsedBorderStyle(topVal.style());
    EBorderStyle bottomStyle = collapsedBorderStyle(bottomVal.style());
    EBorderStyle leftStyle = collapsedBorderStyle(leftVal.style());
    EBorderStyle rightStyle = collapsedBorderStyle(rightVal.style());
    
    bool renderTop = topStyle > BHIDDEN && !topVal.isTransparent();
    bool renderBottom = bottomStyle > BHIDDEN && !bottomVal.isTransparent();
    bool renderLeft = leftStyle > BHIDDEN && !leftVal.isTransparent();
    bool renderRight = rightStyle > BHIDDEN && !rightVal.isTransparent();

    // We never paint diagonals at the joins.  We simply let the border with the highest
    // precedence paint on top of borders with lower precedence.  
    CollapsedBorders borders;
    borders.addBorder(topVal, BSTop, renderTop, tx, ty, tx + w, ty + topWidth, topStyle);
    borders.addBorder(bottomVal, BSBottom, renderBottom, tx, ty + h - bottomWidth, tx + w, ty + h, bottomStyle);
    borders.addBorder(leftVal, BSLeft, renderLeft, tx, ty, tx + leftWidth, ty + h, leftStyle);
    borders.addBorder(rightVal, BSRight, renderRight, tx + w - rightWidth, ty, tx + w, ty + h, rightStyle);
    
    for (CollapsedBorder* border = borders.nextBorder(); border; border = borders.nextBorder()) {
        if (border->borderValue == *table()->currentBorderStyle())
            drawLineForBoxSide(graphicsContext, border->x1, border->y1, border->x2, border->y2, border->side, 
                               border->borderValue.color(), border->style, 0, 0);
    }
}

void RenderTableCell::paintBackgroundsBehindCell(PaintInfo& paintInfo, int tx, int ty, RenderObject* backgroundObject)
{
    if (!paintInfo.shouldPaintWithinRoot(this))
        return;

    if (!backgroundObject)
        return;

    if (style()->visibility() != VISIBLE)
        return;

    RenderTable* tableElt = table();
    if (!tableElt->collapseBorders() && style()->emptyCells() == HIDE && !firstChild())
        return;

    if (backgroundObject != this) {
        tx += x();
        ty += y();
    }

    int w = width();
    int h = height();

    Color c = backgroundObject->style()->visitedDependentColor(CSSPropertyBackgroundColor);
    const FillLayer* bgLayer = backgroundObject->style()->backgroundLayers();

    if (bgLayer->hasImage() || c.isValid()) {
        // We have to clip here because the background would paint
        // on top of the borders otherwise.  This only matters for cells and rows.
        bool shouldClip = backgroundObject->hasLayer() && (backgroundObject == this || backgroundObject == parent()) && tableElt->collapseBorders();
        if (shouldClip) {
            IntRect clipRect(tx + borderLeft(), ty + borderTop(),
                w - borderLeft() - borderRight(), h - borderTop() - borderBottom());
            paintInfo.context->save();
            paintInfo.context->clip(clipRect);
        }
        paintFillLayers(paintInfo, c, bgLayer, tx, ty, w, h, CompositeSourceOver, backgroundObject);
        if (shouldClip)
            paintInfo.context->restore();
    }
}

void RenderTableCell::paintBoxDecorations(PaintInfo& paintInfo, int tx, int ty)
{
    if (!paintInfo.shouldPaintWithinRoot(this))
        return;

    RenderTable* tableElt = table();
    if (!tableElt->collapseBorders() && style()->emptyCells() == HIDE && !firstChild())
        return;

    int w = width();
    int h = height();
   
    if (style()->boxShadow())
        paintBoxShadow(paintInfo.context, tx, ty, w, h, style(), Normal);
    
    // Paint our cell background.
    paintBackgroundsBehindCell(paintInfo, tx, ty, this);
    if (style()->boxShadow())
        paintBoxShadow(paintInfo.context, tx, ty, w, h, style(), Inset);

    if (!style()->hasBorder() || tableElt->collapseBorders())
        return;

    paintBorder(paintInfo.context, tx, ty, w, h, style());
}

void RenderTableCell::paintMask(PaintInfo& paintInfo, int tx, int ty)
{
    if (style()->visibility() != VISIBLE || paintInfo.phase != PaintPhaseMask)
        return;

    RenderTable* tableElt = table();
    if (!tableElt->collapseBorders() && style()->emptyCells() == HIDE && !firstChild())
        return;

    int w = width();
    int h = height();
   
    paintMaskImages(paintInfo, tx, ty, w, h);
}

} // namespace WebCore
