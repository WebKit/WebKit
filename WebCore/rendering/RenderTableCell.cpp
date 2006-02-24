/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "RenderTableCell.h"
#include "RenderTableCol.h"
#include "htmlnames.h"
#include "html_tableimpl.h"
#include <qtextstream.h>

namespace WebCore {

using namespace HTMLNames;

RenderTableCell::RenderTableCell(NodeImpl* _node)
  : RenderBlock(_node)
{
  _col = -1;
  _row = -1;
  cSpan = rSpan = 1;
  updateFromElement();
  setShouldPaintBackgroundOrBorder(true);
  _topExtra = 0;
  _bottomExtra = 0;
  m_percentageHeight = 0;
}

void RenderTableCell::destroy()
{
    if (parent() && section())
        section()->setNeedCellRecalc();

    RenderBlock::destroy();
}

void RenderTableCell::updateFromElement()
{
    int oldRSpan = rSpan;
    int oldCSpan = cSpan;
    NodeImpl* node = element();
    if (node && (node->hasTagName(tdTag) || node->hasTagName(thTag))) {
        HTMLTableCellElementImpl *tc = static_cast<HTMLTableCellElementImpl *>(node);
        cSpan = tc->colSpan();
        rSpan = tc->rowSpan();
    }
    if ((oldRSpan != rSpan || oldCSpan != cSpan) && style() && parent())
        setNeedsLayoutAndMinMaxRecalc();
}

Length RenderTableCell::styleOrColWidth()
{
    Length w = style()->width();
    if (colSpan() > 1 || !w.isAuto())
        return w;
    RenderTableCol* col = table()->colElement(_col);
    if (col)
        w = col->style()->width();
    return w;
}

void RenderTableCell::calcMinMaxWidth()
{
    RenderBlock::calcMinMaxWidth();
    if (element() && style()->autoWrap()) {
        // See if nowrap was set.
        Length w = styleOrColWidth();
        DOMString nowrap = static_cast<ElementImpl*>(element())->getAttribute(nowrapAttr);
        if (!nowrap.isNull() && w.isFixed())
            // Nowrap is set, but we didn't actually use it because of the
            // fixed width set on the cell.  Even so, it is a WinIE/Moz trait
            // to make the minwidth of the cell into the fixed width.  They do this
            // even in strict mode, so do not make this a quirk.  Affected the top
            // of hiptop.com.
            if (m_minWidth < w.value())
                m_minWidth = w.value();
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

void RenderTableCell::computeAbsoluteRepaintRect(IntRect& r, bool f)
{
    r.setY(r.y() + _topExtra);
    RenderBlock::computeAbsoluteRepaintRect(r, f);
}

bool RenderTableCell::absolutePosition(int &xPos, int &yPos, bool f)
{
    bool ret = RenderBlock::absolutePosition(xPos, yPos, f);
    if (ret)
      yPos += _topExtra;
    return ret;
}

short RenderTableCell::baselinePosition(bool) const
{
    RenderObject* o = firstChild();
    int offset = paddingTop() + borderTop();
    
    if (!o)
        return offset + contentHeight();
    while (o->firstChild()) {
	if (!o->isInline())
	    offset += o->paddingTop() + o->borderTop();
	o = o->firstChild();
    }
    
    if (!o->isInline())
        return paddingTop() + borderTop() + contentHeight();

    offset += o->baselinePosition(true);
    return offset;
}

void RenderTableCell::setStyle(RenderStyle *style)
{
    style->setDisplay(TABLE_CELL);

    if (style->whiteSpace() == KHTML_NOWRAP) {
        // Figure out if we are really nowrapping or if we should just
        // use normal instead.  If the width of the cell is fixed, then
        // we don't actually use NOWRAP. 
        if (style->width().isFixed())
            style->setWhiteSpace(NORMAL);
        else
            style->setWhiteSpace(NOWRAP);
    }

    RenderBlock::setStyle(style);
    setShouldPaintBackgroundOrBorder(true);
}

bool RenderTableCell::requiresLayer() {
    return isPositioned() || style()->opacity() < 1.0f || hasOverflowClip();
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
static CollapsedBorderValue compareBorders(const CollapsedBorderValue& border1, 
                                           const CollapsedBorderValue& border2)
{
    // Sanity check the values passed in.  If either is null, return the other.
    if (!border2.exists()) return border1;
    if (!border1.exists()) return border2;
    
    // Rule #1 above.
    if (border1.style() == BHIDDEN || border2.style() == BHIDDEN)
        return CollapsedBorderValue(); // No border should exist at this location.
    
    // Rule #2 above.  A style of 'none' has lowest priority and always loses to any other border.
    if (border2.style() == BNONE) return border1;
    if (border1.style() == BNONE) return border2;
    
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
    
    // (5) Our column's left border.
    RenderTableCol* colElt = tableElt->colElement(col() + (rtl ? colSpan() - 1 : 0));
    if (colElt) {
        result = compareBorders(result, CollapsedBorderValue(&colElt->style()->borderLeft(), BCOL));
        if (!result.exists())
            return result;
    }
    
    // (6) The right border of the column to the left.
    if (!leftmostColumn) {
        colElt = tableElt->colElement(col() + (rtl ? colSpan() : -1));
        if (colElt) {
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
    
    // (5) Our column's right border.
    RenderTableCol* colElt = tableElt->colElement(col() + (rtl ? 0 : colSpan() - 1));
    if (colElt) {
        result = compareBorders(result, CollapsedBorderValue(&colElt->style()->borderRight(), BCOL));
        if (!result.exists())
            return result;
    }
    
    // (6) The left border of the column to the right.
    if (!rightmostColumn) {
        colElt = tableElt->colElement(col() + (rtl ? -1 : colSpan()));
        if (colElt) {
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
    RenderObject* currSection = parent()->parent();
    if (row() == 0) {
        // (5) Our row group's top border.
        result = compareBorders(result, CollapsedBorderValue(&currSection->style()->borderTop(), BROWGROUP));
        if (!result.exists())
            return result;
        
        // (6) Previous row group's bottom border.
        for (currSection = currSection->previousSibling(); currSection;
             currSection = currSection->previousSibling()) {
            if (currSection->isTableSection()) {
                RenderTableSection* section = static_cast<RenderTableSection*>(currSection);
                result = compareBorders(result, CollapsedBorderValue(&section->style()->borderBottom(), BROWGROUP));
                if (!result.exists())
                    return result;
            }
        }
    }
    
    if (!currSection) {
        // (8) Our column's top border.
        RenderTableCol* colElt = table()->colElement(col());
        if (colElt) {
            result = compareBorders(result, CollapsedBorderValue(&colElt->style()->borderTop(), BCOL));
            if (!result.exists())
                return result;
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
    RenderObject* currSection = parent()->parent();
    if (row() + rowSpan() >= static_cast<RenderTableSection*>(currSection)->numRows()) {
        // (5) Our row group's bottom border.
        result = compareBorders(result, CollapsedBorderValue(&currSection->style()->borderBottom(), BROWGROUP));
        if (!result.exists())
            return result;
        
        // (6) Following row group's top border.
        for (currSection = currSection->nextSibling(); currSection;
             currSection = currSection->nextSibling()) {
            if (currSection->isTableSection()) {
                RenderTableSection* section = static_cast<RenderTableSection*>(currSection);
                result = compareBorders(result, CollapsedBorderValue(&section->style()->borderTop(), BROWGROUP));
                if (!result.exists())
                    return result;
            }
        }
    }
    
    if (!currSection) {
        // (8) Our column's bottom border.
        RenderTableCol* colElt = table()->colElement(col());
        if (colElt) {
            result = compareBorders(result, CollapsedBorderValue(&colElt->style()->borderBottom(), BCOL));
            if (!result.exists()) return result;
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
    if (table()->collapseBorders()) {
        CollapsedBorderValue border = collapsedLeftBorder(table()->style()->direction() == RTL);
        if (border.exists())
            return int(border.width() / 2.0 + 0.5); // Give the extra pixel to top and left.
        return 0;
    }
    return RenderBlock::borderLeft();
}
    
int RenderTableCell::borderRight() const
{
    if (table()->collapseBorders()) {
        CollapsedBorderValue border = collapsedRightBorder(table()->style()->direction() == RTL);
        if (border.exists())
            return border.width() / 2;
        return 0;
    }
    return RenderBlock::borderRight();
}

int RenderTableCell::borderTop() const
{
    if (table()->collapseBorders()) {
        CollapsedBorderValue border = collapsedTopBorder();
        if (border.exists())
            return int(border.width() / 2.0 + 0.5); // Give the extra pixel to top and left.
        return 0;
    }
    return RenderBlock::borderTop();
}

int RenderTableCell::borderBottom() const
{
    if (table()->collapseBorders()) {
        CollapsedBorderValue border = collapsedBottomBorder();
        if (border.exists())
            return border.width() / 2;
        return 0;
    }
    return RenderBlock::borderBottom();
}

#ifdef BOX_DEBUG
static void outlineBox(QPainter *p, int _tx, int _ty, int w, int h)
{
    p->setPen(Pen(Color("yellow"), 3, Qt::DotLine));
    p->setBrush(Qt::NoBrush);
    p->drawRect(_tx, _ty, w, h);
}
#endif

void RenderTableCell::paint(PaintInfo& i, int _tx, int _ty)
{
    _tx += m_x;
    _ty += m_y;

    // check if we need to do anything at all...
    int os = 2*maximalOutlineSize(i.phase);
    if (_ty >= i.r.bottom() + os || _ty + _topExtra + m_height + _bottomExtra <= i.r.y() - os)
        return;

    if (i.phase == PaintActionCollapsedTableBorders && style()->visibility() == VISIBLE) {
        int w = width();
        int h = height() + borderTopExtra() + borderBottomExtra();
        paintCollapsedBorder(i.p, _tx, _ty, w, h);
    } else
        RenderBlock::paintObject(i, _tx, _ty + _topExtra);

#ifdef BOX_DEBUG
    ::outlineBox( i.p, _tx, _ty, width(), height() + borderTopExtra() + borderBottomExtra());
#endif
}

static EBorderStyle collapsedBorderStyle(EBorderStyle style)
{
    if (style == OUTSET)
        style = GROOVE;
    else if (style == INSET)
        style = RIDGE;
    return style;
}

struct CollapsedBorder {
    CollapsedBorder(){}
    
    CollapsedBorderValue border;
    RenderObject::BorderSide side;
    bool shouldPaint;
    int x1;
    int y1;
    int x2;
    int y2;
    EBorderStyle style;
};

class CollapsedBorders
{
public:
    CollapsedBorders(int i) :count(0) {}
    
    void addBorder(const CollapsedBorderValue& b, RenderObject::BorderSide s, bool paint, 
                   int _x1, int _y1, int _x2, int _y2,
                   EBorderStyle _style)
    {
        if (b.exists() && paint) {
            borders[count].border = b;
            borders[count].side = s;
            borders[count].shouldPaint = paint;
            borders[count].x1 = _x1;
            borders[count].x2 = _x2;
            borders[count].y1 = _y1;
            borders[count].y2 = _y2;
            borders[count].style = _style;
            count++;
        }
    }

    CollapsedBorder* nextBorder() {
        for (int i = 0; i < count; i++) {
            if (borders[i].border.exists() && borders[i].shouldPaint) {
                borders[i].shouldPaint = false;
                return &borders[i];
            }
        }
        
        return 0;
    }
    
    CollapsedBorder borders[4];
    int count;
};

static void addBorderStyle(QValueList<CollapsedBorderValue>& borderStyles, CollapsedBorderValue borderValue)
{
    if (!borderValue.exists() || borderStyles.contains(borderValue))
        return;
    
    QValueListIterator<CollapsedBorderValue> it = borderStyles.begin();
    QValueListIterator<CollapsedBorderValue> end = borderStyles.end();
    for (; it != end; ++it) {
        CollapsedBorderValue result = compareBorders(*it, borderValue);
        if (result == *it) {
            borderStyles.insert(it, borderValue);
            return;
        }
    }

    borderStyles.append(borderValue);
}

void RenderTableCell::collectBorders(QValueList<CollapsedBorderValue>& borderStyles)
{
    bool rtl = table()->style()->direction() == RTL;
    addBorderStyle(borderStyles, collapsedLeftBorder(rtl));
    addBorderStyle(borderStyles, collapsedRightBorder(rtl));
    addBorderStyle(borderStyles, collapsedTopBorder());
    addBorderStyle(borderStyles, collapsedBottomBorder());
}

void RenderTableCell::paintCollapsedBorder(QPainter* p, int _tx, int _ty, int w, int h)
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
    
    _tx -= leftWidth / 2;
    _ty -= topWidth / 2;
    w += leftWidth / 2 + int(rightWidth / 2.0 + 0.5);
    h += topWidth / 2 + int(bottomWidth / 2.0 + 0.5);
    
    bool tt = topVal.isTransparent();
    bool bt = bottomVal.isTransparent();
    bool rt = rightVal.isTransparent();
    bool lt = leftVal.isTransparent();
    
    EBorderStyle ts = collapsedBorderStyle(topVal.style());
    EBorderStyle bs = collapsedBorderStyle(bottomVal.style());
    EBorderStyle ls = collapsedBorderStyle(leftVal.style());
    EBorderStyle rs = collapsedBorderStyle(rightVal.style());
    
    bool render_t = ts > BHIDDEN && !tt;
    bool render_l = ls > BHIDDEN && !lt;
    bool render_r = rs > BHIDDEN && !rt;
    bool render_b = bs > BHIDDEN && !bt;

    // We never paint diagonals at the joins.  We simply let the border with the highest
    // precedence paint on top of borders with lower precedence.  
    CollapsedBorders borders(4);
    borders.addBorder(topVal, BSTop, render_t, _tx, _ty, _tx + w, _ty + topWidth, ts);
    borders.addBorder(bottomVal, BSBottom, render_b, _tx, _ty + h - bottomWidth, _tx + w, _ty + h, bs);
    borders.addBorder(leftVal, BSLeft, render_l, _tx, _ty, _tx + leftWidth, _ty + h, ls);
    borders.addBorder(rightVal, BSRight, render_r, _tx + w - rightWidth, _ty, _tx + w, _ty + h, rs);
    
    for (CollapsedBorder* border = borders.nextBorder(); border; border = borders.nextBorder()) {
        if (border->border == *table()->currentBorderStyle())
            drawBorder(p, border->x1, border->y1, border->x2, border->y2, border->side, 
                       border->border.color(), style()->color(), border->style, 0, 0);
    }
}

IntRect RenderTableCell::getAbsoluteRepaintRect()
{
    int ow = style() ? style()->outlineSize() : 0;
    IntRect r(-ow, -ow - borderTopExtra(), 
            overflowWidth(false) + ow * 2, overflowHeight(false) + borderTopExtra() + borderBottomExtra() + ow * 2);
    computeAbsoluteRepaintRect(r);
    return r;
}

void RenderTableCell::paintBoxDecorations(PaintInfo& i, int _tx, int _ty)
{
    RenderTable* tableElt = table();
    if (!tableElt->collapseBorders() && style()->emptyCells() == HIDE && !firstChild())
        return;
    
    int w = width();
    int h = height() + borderTopExtra() + borderBottomExtra();
    _ty -= borderTopExtra();

    Color c = style()->backgroundColor();
    if (!c.isValid() && parent()) // take from row
        c = parent()->style()->backgroundColor();
    if (!c.isValid() && parent() && parent()->parent()) // take from rowgroup
        c = parent()->parent()->style()->backgroundColor();
    if (!c.isValid()) {
	// see if we have a col or colgroup for this
	RenderTableCol *col = table()->colElement(_col);
	if (col) {
	    c = col->style()->backgroundColor();
	    if (!c.isValid()) {
		// try column group
		RenderStyle *style = col->parent()->style();
		if (style->display() == TABLE_COLUMN_GROUP)
		    c = style->backgroundColor();
	    }
	}
    }

    // FIXME: This code is just plain wrong.  Rows and columns should paint their backgrounds
    // independent from the cell.
    // ### get offsets right in case the bgimage is inherited.
    const BackgroundLayer* bgLayer = style()->backgroundLayers();
    if (!bgLayer->hasImage() && parent())
        bgLayer = parent()->style()->backgroundLayers();
    if (!bgLayer->hasImage() && parent() && parent()->parent())
        bgLayer = parent()->parent()->style()->backgroundLayers();
    if (!bgLayer->hasImage()) {
	// see if we have a col or colgroup for this
	RenderTableCol* col = table()->colElement(_col);
	if (col) {
	    bgLayer = col->style()->backgroundLayers();
	    if (!bgLayer->hasImage()) {
		// try column group
		RenderStyle *style = col->parent()->style();
		if (style->display() == TABLE_COLUMN_GROUP)
		    bgLayer = style->backgroundLayers();
	    }
	}
    }

    int my = kMax(_ty, i.r.y());
    int end = kMin(i.r.bottom(), _ty + h);
    int mh = end - my;

    if (bgLayer->hasImage() || c.isValid()) {
	// We have to clip here because the backround would paint
        // on top of the borders otherwise.
        if (m_layer && tableElt->collapseBorders()) {
            IntRect clipRect(_tx + borderLeft(), _ty + borderTop(), w - borderLeft() - borderRight(), h - borderTop() - borderBottom());
            clipRect = i.p->xForm(clipRect);
            i.p->save();
            i.p->addClip(clipRect);
        }
        paintBackground(i.p, c, bgLayer, my, mh, _tx, _ty, w, h);
        if (m_layer && tableElt->collapseBorders())
            i.p->restore();
    }

    if (style()->hasBorder() && !tableElt->collapseBorders())
        paintBorder(i.p, _tx, _ty, w, h, style());
}

#ifndef NDEBUG
void RenderTableCell::dump(QTextStream *stream, QString ind) const
{
    *stream << " row=" << _row;
    *stream << " col=" << _col;
    *stream << " rSpan=" << rSpan;
    *stream << " cSpan=" << cSpan;
//    *stream << " nWrap=" << nWrap;

    RenderBlock::dump(stream,ind);
}
#endif

}
