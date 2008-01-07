/**
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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

#include "GraphicsContext.h"
#include "HTMLNames.h"
#include "HTMLTableCellElement.h"
#include "RenderTableCol.h"
#include "RenderView.h"
#include "TextStream.h"

using namespace std;

namespace WebCore {

using namespace HTMLNames;

RenderTableCell::RenderTableCell(Node* node)
    : RenderBlock(node)
    , m_row(-1)
    , m_column(-1)
    , m_rowSpan(1)
    , m_columnSpan(1)
    , m_topExtra(0)
    , m_bottomExtra(0)
    , m_widthChanged(false)
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
    Node* node = element();
    if (node && (node->hasTagName(tdTag) || node->hasTagName(thTag))) {
        HTMLTableCellElement* tc = static_cast<HTMLTableCellElement*>(node);
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
    if (colSpan() > 1 || !w.isAuto())
        return w;
    RenderTableCol* tableCol = table()->colElement(col());
    if (tableCol) {
        w = tableCol->style()->width();
        
        // Column widths specified on <col> apply to the border box of the cell.
        // Percentages don't need to be handled since they're always treated this way (even when specified on the cells).
        // See Bugzilla bug 8126 for details.
        if (w.isFixed() && w.value() > 0)
            w = Length(max(0, w.value() - borderLeft() - borderRight() - paddingLeft() - paddingRight()), Fixed);
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
    if (element() && style()->autoWrap()) {
        // See if nowrap was set.
        Length w = styleOrColWidth();
        String nowrap = static_cast<Element*>(element())->getAttribute(nowrapAttr);
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

void RenderTableCell::setWidth(int width)
{
    if (width != m_width) {
        m_width = width;
        m_widthChanged = true;
    }
}

void RenderTableCell::layout()
{
    layoutBlock(m_widthChanged);
    m_widthChanged = false;
}

IntRect RenderTableCell::absoluteClippedOverflowRect()
{
    // If the table grid is dirty, we cannot get reliable information about adjoining cells,
    // so we ignore outside borders. This should not be a problem because it means that
    // the table is going to recalculate the grid, relayout and repaint its current rect, which
    // includes any outside borders of this cell.
    if (table()->collapseBorders() && !table()->needsSectionRecalc()) {
        bool rtl = table()->style()->direction() == RTL;
        int outlineSize = style()->outlineSize();
        int left = max(borderHalfLeft(true), outlineSize);
        int right = max(borderHalfRight(true), outlineSize);
        int top = max(borderHalfTop(true), outlineSize);
        int bottom = max(borderHalfBottom(true), outlineSize);
        if (left && !rtl || right && rtl) {
            if (RenderTableCell* before = table()->cellBefore(this)) {
                top = max(top, before->borderHalfTop(true));
                bottom = max(bottom, before->borderHalfBottom(true));
            }
        }
        if (left && rtl || right && !rtl) {
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
        left = max(left, -overflowLeft(false));
        top = max(top, -overflowTop(false) - borderTopExtra());
        IntRect r(-left, -borderTopExtra() - top, left + max(width() + right, overflowWidth(false)), borderTopExtra() + top + max(height() + bottom + borderBottomExtra(), overflowHeight(false)));

        if (RenderView* v = view())
            r.move(v->layoutDelta());

        computeAbsoluteRepaintRect(r);
        return r;
    }
    return RenderBlock::absoluteClippedOverflowRect();
}

void RenderTableCell::computeAbsoluteRepaintRect(IntRect& r, bool fixed)
{
    r.setY(r.y() + m_topExtra);
    RenderView* v = view();
    if (!v || !v->layoutState())
        r.move(-parent()->xPos(), -parent()->yPos()); // Rows are in the same coordinate space, so don't add their offset in.
    RenderBlock::computeAbsoluteRepaintRect(r, fixed);
}

bool RenderTableCell::absolutePosition(int& xPos, int& yPos, bool fixed) const
{
    bool result = RenderBlock::absolutePosition(xPos, yPos, fixed);
    RenderView* v = view();
    if (!v || !v->layoutState()) {
        xPos -= parent()->xPos(); // Rows are in the same coordinate space, so don't add their offset in.
        yPos -= parent()->yPos();
    }
    return result;
}

short RenderTableCell::baselinePosition(bool /*firstLine*/, bool /*isRootLineBox*/) const
{
    RenderObject* o = firstChild();
    int offset = paddingTop() + borderTop();
    
    if (!o)
        return offset + contentHeight();
    while (o->firstChild() && !o->isReplaced()) {
        if (!o->isInline())
            offset += o->paddingTop() + o->borderTop();
        o = o->firstChild();
    }
    
    if (!o->isInline())
        return paddingTop() + borderTop() + contentHeight();

    offset += o->baselinePosition(true);
    return offset;
}

void RenderTableCell::setStyle(RenderStyle* newStyle)
{
    if (parent() && section() && style() && style()->height() != newStyle->height())
        section()->setNeedsCellRecalc();

    newStyle->setDisplay(TABLE_CELL);

    if (newStyle->whiteSpace() == KHTML_NOWRAP) {
        // Figure out if we are really nowrapping or if we should just
        // use normal instead.  If the width of the cell is fixed, then
        // we don't actually use NOWRAP.
        if (newStyle->width().isFixed())
            newStyle->setWhiteSpace(NORMAL);
        else
            newStyle->setWhiteSpace(NOWRAP);
    }

    RenderBlock::setStyle(newStyle);
    setHasBoxDecorations(true);
}

bool RenderTableCell::requiresLayer()
{
    return isPositioned() || isTransparent() || hasOverflowClip() || hasTransform();
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
static CollapsedBorderValue compareBorders(const CollapsedBorderValue& border1, const CollapsedBorderValue& border2)
{
    // Sanity check the values passed in.  If either is null, return the other.
    if (!border2.exists())
        return border1;
    if (!border1.exists())
        return border2;

    // Rule #1 above.
    if (border1.style() == BHIDDEN || border2.style() == BHIDDEN)
        return CollapsedBorderValue(); // No border should exist at this location.
    
    // Rule #2 above.  A style of 'none' has lowest priority and always loses to any other border.
    if (border2.style() == BNONE)
        return border1;
    if (border1.style() == BNONE)
        return border2;

    // The first part of rule #3 above. Wider borders win.
    if (border1.width() != border2.width())
        return border1.width() > border2.width() ? border1 : border2;
    
    // The borders have equal width.  Sort by border style.
    if (border1.style() != border2.style())
        return border1.style() > border2.style() ? border1 : border2;
    
    // The border have the same width and style.  Rely on precedence (cell over row over row group, etc.)
    return border1.precedence >= border2.precedence ? border1 : border2;
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
    CollapsedBorderValue result(&style()->borderLeft(), BCELL);
    
    // (2) The right border of the cell to the left.
    RenderTableCell* prevCell = rtl ? tableElt->cellAfter(this) : tableElt->cellBefore(this);
    if (prevCell) {
        result = compareBorders(result, CollapsedBorderValue(&prevCell->style()->borderRight(), BCELL));
        if (!result.exists())
            return result;
    } else if (leftmostColumn) {
        // (3) Our row's left border.
        result = compareBorders(result, CollapsedBorderValue(&parent()->style()->borderLeft(), BROW));
        if (!result.exists())
            return result;
        
        // (4) Our row group's left border.
        result = compareBorders(result, CollapsedBorderValue(&section()->style()->borderLeft(), BROWGROUP));
        if (!result.exists())
            return result;
    }
    
    // (5) Our column and column group's left borders.
    bool startColEdge;
    bool endColEdge;
    RenderTableCol* colElt = tableElt->colElement(col() + (rtl ? colSpan() - 1 : 0), &startColEdge, &endColEdge);
    if (colElt && (!rtl ? startColEdge : endColEdge)) {
        result = compareBorders(result, CollapsedBorderValue(&colElt->style()->borderLeft(), BCOL));
        if (!result.exists())
            return result;
        if (colElt->parent()->isTableCol() && (!rtl ? !colElt->previousSibling() : !colElt->nextSibling())) {
            result = compareBorders(result, CollapsedBorderValue(&colElt->parent()->style()->borderLeft(), BCOLGROUP));
            if (!result.exists())
                return result;
        }
    }
    
    // (6) The right border of the column to the left.
    if (!leftmostColumn) {
        colElt = tableElt->colElement(col() + (rtl ? colSpan() : -1), &startColEdge, &endColEdge);
        if (colElt && (!rtl ? endColEdge : startColEdge)) {
            result = compareBorders(result, CollapsedBorderValue(&colElt->style()->borderRight(), BCOL));
            if (!result.exists())
                return result;
        }
    } else {
        // (7) The table's left border.
        result = compareBorders(result, CollapsedBorderValue(&tableElt->style()->borderLeft(), BTABLE));
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
    CollapsedBorderValue result = CollapsedBorderValue(&style()->borderRight(), BCELL);
    
    // (2) The left border of the cell to the right.
    if (!rightmostColumn) {
        RenderTableCell* nextCell = rtl ? tableElt->cellBefore(this) : tableElt->cellAfter(this);
        if (nextCell && nextCell->style()) {
            result = compareBorders(result, CollapsedBorderValue(&nextCell->style()->borderLeft(), BCELL));
            if (!result.exists())
                return result;
        }
    } else {
        // (3) Our row's right border.
        result = compareBorders(result, CollapsedBorderValue(&parent()->style()->borderRight(), BROW));
        if (!result.exists())
            return result;
        
        // (4) Our row group's right border.
        result = compareBorders(result, CollapsedBorderValue(&section()->style()->borderRight(), BROWGROUP));
        if (!result.exists())
            return result;
    }
    
    // (5) Our column and column group's right borders.
    bool startColEdge;
    bool endColEdge;
    RenderTableCol* colElt = tableElt->colElement(col() + (rtl ? 0 : colSpan() - 1), &startColEdge, &endColEdge);
    if (colElt && (!rtl ? endColEdge : startColEdge)) {
        result = compareBorders(result, CollapsedBorderValue(&colElt->style()->borderRight(), BCOL));
        if (!result.exists())
            return result;
        if (colElt->parent()->isTableCol() && (!rtl ? !colElt->nextSibling() : !colElt->previousSibling())) {
            result = compareBorders(result, CollapsedBorderValue(&colElt->parent()->style()->borderRight(), BCOLGROUP));
            if (!result.exists())
                return result;
        }
    }
    
    // (6) The left border of the column to the right.
    if (!rightmostColumn) {
        colElt = tableElt->colElement(col() + (rtl ? -1 : colSpan()), &startColEdge, &endColEdge);
        if (colElt && (!rtl ? startColEdge : endColEdge)) {
            result = compareBorders(result, CollapsedBorderValue(&colElt->style()->borderLeft(), BCOL));
            if (!result.exists())
                return result;
        }
    } else {
        // (7) The table's right border.
        result = compareBorders(result, CollapsedBorderValue(&tableElt->style()->borderRight(), BTABLE));
        if (!result.exists())
            return result;
    }
    
    return result;
}

CollapsedBorderValue RenderTableCell::collapsedTopBorder() const
{
    // For border top, we need to check, in order of precedence:
    // (1) Our top border.
    CollapsedBorderValue result = CollapsedBorderValue(&style()->borderTop(), BCELL);
    
    RenderTableCell* prevCell = table()->cellAbove(this);
    if (prevCell) {
        // (2) A previous cell's bottom border.
        result = compareBorders(result, CollapsedBorderValue(&prevCell->style()->borderBottom(), BCELL));
        if (!result.exists()) 
            return result;
    }
    
    // (3) Our row's top border.
    result = compareBorders(result, CollapsedBorderValue(&parent()->style()->borderTop(), BROW));
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
            result = compareBorders(result, CollapsedBorderValue(&prevRow->style()->borderBottom(), BROW));
            if (!result.exists())
                return result;
        }
    }
    
    // Now check row groups.
    RenderTableSection* currSection = section();
    if (!row()) {
        // (5) Our row group's top border.
        result = compareBorders(result, CollapsedBorderValue(&currSection->style()->borderTop(), BROWGROUP));
        if (!result.exists())
            return result;
        
        // (6) Previous row group's bottom border.
        currSection = table()->sectionAbove(currSection);
        if (currSection) {
            result = compareBorders(result, CollapsedBorderValue(&currSection->style()->borderBottom(), BROWGROUP));
            if (!result.exists())
                return result;
        }
    }
    
    if (!currSection) {
        // (8) Our column and column group's top borders.
        RenderTableCol* colElt = table()->colElement(col());
        if (colElt) {
            result = compareBorders(result, CollapsedBorderValue(&colElt->style()->borderTop(), BCOL));
            if (!result.exists())
                return result;
            if (colElt->parent()->isTableCol()) {
                result = compareBorders(result, CollapsedBorderValue(&colElt->parent()->style()->borderTop(), BCOLGROUP));
                if (!result.exists())
                    return result;
            }
        }
        
        // (9) The table's top border.
        result = compareBorders(result, CollapsedBorderValue(&table()->style()->borderTop(), BTABLE));
        if (!result.exists())
            return result;
    }
    
    return result;
}

CollapsedBorderValue RenderTableCell::collapsedBottomBorder() const
{
    // For border top, we need to check, in order of precedence:
    // (1) Our bottom border.
    CollapsedBorderValue result = CollapsedBorderValue(&style()->borderBottom(), BCELL);
    
    RenderTableCell* nextCell = table()->cellBelow(this);
    if (nextCell) {
        // (2) A following cell's top border.
        result = compareBorders(result, CollapsedBorderValue(&nextCell->style()->borderTop(), BCELL));
        if (!result.exists())
            return result;
    }
    
    // (3) Our row's bottom border. (FIXME: Deal with rowspan!)
    result = compareBorders(result, CollapsedBorderValue(&parent()->style()->borderBottom(), BROW));
    if (!result.exists())
        return result;
    
    // (4) The next row's top border.
    if (nextCell) {
        result = compareBorders(result, CollapsedBorderValue(&nextCell->parent()->style()->borderTop(), BROW));
        if (!result.exists())
            return result;
    }
    
    // Now check row groups.
    RenderTableSection* currSection = section();
    if (row() + rowSpan() >= static_cast<RenderTableSection*>(currSection)->numRows()) {
        // (5) Our row group's bottom border.
        result = compareBorders(result, CollapsedBorderValue(&currSection->style()->borderBottom(), BROWGROUP));
        if (!result.exists())
            return result;
        
        // (6) Following row group's top border.
        currSection = table()->sectionBelow(currSection);
        if (currSection) {
            result = compareBorders(result, CollapsedBorderValue(&currSection->style()->borderTop(), BROWGROUP));
            if (!result.exists())
                return result;
        }
    }
    
    if (!currSection) {
        // (8) Our column and column group's bottom borders.
        RenderTableCol* colElt = table()->colElement(col());
        if (colElt) {
            result = compareBorders(result, CollapsedBorderValue(&colElt->style()->borderBottom(), BCOL));
            if (!result.exists()) return result;
            if (colElt->parent()->isTableCol()) {
                result = compareBorders(result, CollapsedBorderValue(&colElt->parent()->style()->borderBottom(), BCOLGROUP));
                if (!result.exists())
                    return result;
            }
        }
        
        // (9) The table's bottom border.
        result = compareBorders(result, CollapsedBorderValue(&table()->style()->borderBottom(), BTABLE));
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
    tx += m_x;
    ty += m_y;

    // check if we need to do anything at all...
    int os = 2 * maximalOutlineSize(paintInfo.phase);

    if (paintInfo.phase == PaintPhaseCollapsedTableBorders && style()->visibility() == VISIBLE) {
        if (ty - table()->outerBorderTop() >= paintInfo.rect.bottom() + os ||
                ty + m_topExtra + m_height + m_bottomExtra + table()->outerBorderBottom() <= paintInfo.rect.y() - os)
            return;
        int w = width();
        int h = height() + borderTopExtra() + borderBottomExtra();
        paintCollapsedBorder(paintInfo.context, tx, ty, w, h);
    } else {
        if (ty + overflowTop(false) >= paintInfo.rect.bottom() + os || ty + m_topExtra + overflowHeight(false) + m_bottomExtra <= paintInfo.rect.y() - os)
            return;
        RenderBlock::paintObject(paintInfo, tx, ty + m_topExtra);
    }
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
    RenderObject::BorderSide side;
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
    
    void addBorder(const CollapsedBorderValue& borderValue, RenderObject::BorderSide borderSide, bool shouldPaint,
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
    CollapsedBorderValue borderWithHigherPrecedence = compareBorders(*a, *b);
    if (*a == borderWithHigherPrecedence)
        return 1;
    return -1;
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
            drawBorder(graphicsContext, border->x1, border->y1, border->x2, border->y2, border->side, 
                       border->borderValue.color(), style()->color(), border->style, 0, 0);
    }
}

void RenderTableCell::paintBackgroundsBehindCell(PaintInfo& paintInfo, int tx, int ty, RenderObject* backgroundObject)
{
    if (!backgroundObject)
        return;

    if (style()->visibility() != VISIBLE)
        return;

    RenderTable* tableElt = table();
    if (!tableElt->collapseBorders() && style()->emptyCells() == HIDE && !firstChild())
        return;

    if (backgroundObject != this) {
        tx += m_x;
        ty += m_y + m_topExtra;
    }

    int w = width();
    int h = height() + borderTopExtra() + borderBottomExtra();
    ty -= borderTopExtra();

    int my = max(ty, paintInfo.rect.y());
    int end = min(paintInfo.rect.bottom(), ty + h);
    int mh = end - my;

    Color c = backgroundObject->style()->backgroundColor();
    const BackgroundLayer* bgLayer = backgroundObject->style()->backgroundLayers();

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
        paintBackgrounds(paintInfo.context, c, bgLayer, my, mh, tx, ty, w, h);
        if (shouldClip)
            paintInfo.context->restore();
    }
}

void RenderTableCell::paintBoxDecorations(PaintInfo& paintInfo, int tx, int ty)
{
    RenderTable* tableElt = table();
    if (!tableElt->collapseBorders() && style()->emptyCells() == HIDE && !firstChild())
        return;

    int w = width();
    int h = height() + borderTopExtra() + borderBottomExtra();
   
    if (style()->boxShadow())
        paintBoxShadow(paintInfo.context, tx, ty - borderTopExtra(), w, h, style());
    
    // Paint our cell background.
    paintBackgroundsBehindCell(paintInfo, tx, ty, this);

    if (!style()->hasBorder() || tableElt->collapseBorders())
        return;

    ty -= borderTopExtra();
    paintBorder(paintInfo.context, tx, ty, w, h, style());
}

#ifndef NDEBUG
void RenderTableCell::dump(TextStream* stream, DeprecatedString ind) const
{
    *stream << " row=" << row();
    *stream << " col=" << col();
    *stream << " rSpan=" << rowSpan();
    *stream << " cSpan=" << colSpan();

    RenderBlock::dump(stream,ind);
}
#endif

} // namespace WebCore
