/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@nypop.com)
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
#include "RenderTable.h"

#include "AutoTableLayout.h"
#include "Document.h"
#include "FixedTableLayout.h"
#include "HTMLNames.h"
#include "KWQTextStream.h"
#include "RenderTableCell.h"
#include "RenderTableCol.h"
#include "RenderTableSection.h"

using namespace std;

namespace WebCore {

using namespace HTMLNames;

RenderTable::RenderTable(Node* node)
    : RenderBlock(node)
{
    tCaption = 0;
    head = foot = firstBody = 0;
    tableLayout = 0;
    m_currentBorder = 0;
    
    rules = None;
    frame = Void;
    has_col_elems = false;
    hspacing = 0;
    vspacing = 0;
    padding = 0;
    needSectionRecalc = false;
    padding = 0;
    m_borderLeft = 0;
    m_borderRight = 0;

    columnPos.resize(2);
    columnPos.fill(0);
    columns.resize(1);
    columns.fill(ColumnStruct());

    columnPos[0] = 0;
}

RenderTable::~RenderTable()
{
    delete tableLayout;
}

void RenderTable::setStyle(RenderStyle *_style)
{
    ETableLayout oldTableLayout = style() ? style()->tableLayout() : TAUTO;
    RenderBlock::setStyle(_style);

    // In the collapsed border model, there is no cell spacing.
    hspacing = collapseBorders() ? 0 : style()->horizontalBorderSpacing();
    vspacing = collapseBorders() ? 0 : style()->verticalBorderSpacing();
    columnPos[0] = hspacing;

    if (!tableLayout || style()->tableLayout() != oldTableLayout) {
        delete tableLayout;

        // According to the CSS2 spec, you only use fixed table layout if an
        // explicit width is specified on the table.  Auto width implies auto table layout.
        if (style()->tableLayout() == TFIXED && !style()->width().isAuto())
            tableLayout = new FixedTableLayout(this);
        else
            tableLayout = new AutoTableLayout(this);
    }
}

void RenderTable::addChild(RenderObject* child, RenderObject* beforeChild)
{
    bool wrapInAnonymousSection = true;
    bool isTableElement = element() && element()->hasTagName(tableTag);

    switch (child->style()->display()) {
        case TABLE_CAPTION:
            if (child->isRenderBlock())
                tCaption = static_cast<RenderBlock *>(child);
            wrapInAnonymousSection = false;
            break;
        case TABLE_COLUMN:
        case TABLE_COLUMN_GROUP:
            has_col_elems = true;
            wrapInAnonymousSection = false;
            break;
        case TABLE_HEADER_GROUP:
            if (!head) {
                if (child->isTableSection())
                    head = static_cast<RenderTableSection *>(child);
            } else if (!firstBody) {
                if (child->isTableSection())
                    firstBody = static_cast<RenderTableSection *>(child);
            }
            wrapInAnonymousSection = false;
            break;
        case TABLE_FOOTER_GROUP:
            if (!foot) {
                if (child->isTableSection())
                    foot = static_cast<RenderTableSection *>(child);
                wrapInAnonymousSection = false;
                break;
            }
            // fall through
        case TABLE_ROW_GROUP:
            if (!firstBody)
                if (child->isTableSection())
                    firstBody = static_cast<RenderTableSection *>(child);
            wrapInAnonymousSection = false;
            break;
        case TABLE_CELL:
        case TABLE_ROW:
            wrapInAnonymousSection = true;
            break;
        case BLOCK:
        case BOX:
        case COMPACT:
        case INLINE:
        case INLINE_BLOCK:
        case INLINE_BOX:
        case INLINE_TABLE:
        case LIST_ITEM:
        case NONE:
        case RUN_IN:
        case TABLE:
            // Allow a form to just sit at the top level.
            wrapInAnonymousSection = !isTableElement || !child->element() || !child->element()->hasTagName(formTag);
            break;
        }

    if (!wrapInAnonymousSection) {
        RenderContainer::addChild(child, beforeChild);
        return;
    }

    if (!beforeChild && lastChild() && lastChild()->isTableSection() && lastChild()->isAnonymous()) {
        lastChild()->addChild(child);
        return;
    }

    RenderObject *lastBox = beforeChild;
    RenderObject *nextToLastBox = beforeChild;
    while (lastBox && lastBox->parent()->isAnonymous() &&
            !lastBox->isTableSection() && lastBox->style()->display() != TABLE_CAPTION) {
        nextToLastBox = lastBox;
        lastBox = lastBox->parent();
    }
    if (lastBox && lastBox->isAnonymous()) {
        lastBox->addChild(child, nextToLastBox);
        return;
    }

    if (beforeChild && !beforeChild->isTableSection())
        beforeChild = 0;
    RenderTableSection* section = new (renderArena()) RenderTableSection(document() /* anonymous */);
    RenderStyle* newStyle = new (renderArena()) RenderStyle();
    newStyle->inheritFrom(style());
    newStyle->setDisplay(TABLE_ROW_GROUP);
    section->setStyle(newStyle);
    addChild(section, beforeChild);
    section->addChild(child);
}

void RenderTable::calcWidth()
{
    if (isPositioned())
        calcAbsoluteHorizontal();

    RenderBlock *cb = containingBlock();
    int availableWidth = cb->contentWidth();

    LengthType widthType = style()->width().type();
    if (widthType > Relative && style()->width().value() > 0) {
        // Percent or fixed table
        m_width = style()->width().calcMinValue(availableWidth);
        if (m_minWidth > m_width)
            m_width = m_minWidth;
    } else {
        // An auto width table should shrink to fit within the line width if necessary in order to 
        // avoid overlapping floats.
        availableWidth = cb->lineWidth(m_y);
        
        // Subtract out any fixed margins from our available width for auto width tables.
        int marginTotal = 0;
        if (!style()->marginLeft().isAuto())
            marginTotal += style()->marginLeft().calcValue(availableWidth);
        if (!style()->marginRight().isAuto())
            marginTotal += style()->marginRight().calcValue(availableWidth);
            
        // Subtract out our margins to get the available content width.
        int availContentWidth = max(0, availableWidth - marginTotal);
        
        // Ensure we aren't bigger than our max width or smaller than our min width.
        m_width = min(availContentWidth, m_maxWidth);
    }
    
    m_width = max(m_width, m_minWidth);

    // Finally, with our true width determined, compute our margins for real.
    m_marginRight = 0;
    m_marginLeft = 0;
    calcHorizontalMargins(style()->marginLeft(),style()->marginRight(),availableWidth);
}

void RenderTable::layout()
{
    KHTMLAssert(needsLayout());
    KHTMLAssert(minMaxKnown());
    KHTMLAssert(!needSectionRecalc);

    if (posChildNeedsLayout() && !normalChildNeedsLayout() && !selfNeedsLayout()) {
        // All we have to is lay out our positioned objects.
        layoutPositionedObjects(true);
        setNeedsLayout(false);
        return;
    }

    IntRect oldBounds, oldFullBounds;
    bool checkForRepaint = checkForRepaintDuringLayout();
    if (checkForRepaint)
        getAbsoluteRepaintRectIncludingFloats(oldBounds, oldFullBounds);
    
    m_height = m_overflowHeight = 0;
    initMaxMarginValues();
    
    //int oldWidth = m_width;
    calcWidth();

    // the optimisation below doesn't work since the internal table
    // layout could have changed.  we need to add a flag to the table
    // layout that tells us if something has changed in the min max
    // calculations to do it correctly.
//     if ( oldWidth != m_width || columns.size() + 1 != columnPos.size() )
    tableLayout->layout();

    setCellWidths();

    // layout child objects
    int calculatedHeight = 0;

    RenderObject *child = firstChild();
    while (child) {
        // FIXME: What about a form that has a display value that makes it a table section?
        if (child->needsLayout() && !(child->element() && child->element()->hasTagName(formTag)))
            child->layout();
        if (child->isTableSection()) {
            static_cast<RenderTableSection *>(child)->calcRowHeight();
            calculatedHeight += static_cast<RenderTableSection *>(child)->layoutRows(0);
        }
        child = child->nextSibling();
    }

    m_overflowWidth = m_width + (collapseBorders() ? outerBorderRight() - borderRight() : 0);
    m_overflowLeft = collapseBorders() ? borderLeft() - outerBorderLeft() : 0;

    // ### collapse caption margin
    if (tCaption && tCaption->style()->captionSide() != CAPBOTTOM) {
        tCaption->setPos(tCaption->marginLeft(), m_height);
        m_height += tCaption->height() + tCaption->marginTop() + tCaption->marginBottom();
    }

    int bpTop = borderTop() + (collapseBorders() ? 0 : paddingTop());
    int bpBottom = borderBottom() + (collapseBorders() ? 0 : paddingBottom());
    
    m_height += bpTop;

    int oldHeight = m_height;
    calcHeight();
    int newHeight = m_height;
    m_height = oldHeight;

    Length h = style()->height();
    int th = 0;
    if (isPositioned())
        th = newHeight; // FIXME: Leave this alone for now but investigate later.
    else if (h.isFixed())
        th = h.value() - (bpTop + bpBottom);  // Tables size as though CSS height includes border/padding.
    else if (h.isPercent())
        th = calcPercentageHeight(h);
    th = max(0, th);

    // layout rows
    if (th > calculatedHeight) {
        // we have to redistribute that height to get the constraint correctly
        // just force the first body to the height needed
        // ### FIXME This should take height constraints on all table sections into account and distribute
        // accordingly. For now this should be good enough
        if (firstBody) {
            firstBody->calcRowHeight();
            firstBody->layoutRows(th - calculatedHeight);
        }
        else if (!style()->htmlHacks()) {
            // Completely empty tables (with no sections or anything) should at least honor specified height
            // in strict mode.
            m_height += th;
        }
    }
    
    int bl = borderLeft();
    if (!collapseBorders())
        bl += paddingLeft();

    // position the table sections
    if (head) {
        head->setPos(bl, m_height);
        m_height += head->height();
    }
    for (RenderObject *body = firstBody; body; body = body->nextSibling()) {
        if (body != head && body != foot && body->isTableSection()) {
            body->setPos(bl, m_height);
            m_height += body->height();
        }
    }
    if (foot) {
        foot->setPos(bl, m_height);
        m_height += foot->height();
    }

    m_height += bpBottom;
               
    if (tCaption && tCaption->style()->captionSide()==CAPBOTTOM) {
        tCaption->setPos(tCaption->marginLeft(), m_height);
        m_height += tCaption->height() + tCaption->marginTop() + tCaption->marginBottom();
    }

    // table can be containing block of positioned elements.
    // ### only pass true if width or height changed.
    layoutPositionedObjects( true );

    // Repaint with our new bounds if they are different from our old bounds.
    if (checkForRepaint)
        repaintAfterLayoutIfNeeded(oldBounds, oldFullBounds);
    
    setNeedsLayout(false);
}

void RenderTable::setCellWidths()
{
    for (RenderObject *child = firstChild(); child; child = child->nextSibling())
        if ( child->isTableSection() )
            static_cast<RenderTableSection *>(child)->setCellWidths();
}

void RenderTable::paint(PaintInfo& i, int _tx, int _ty)
{
    _tx += xPos();
    _ty += yPos();

    PaintPhase paintPhase = i.phase;
    
    int os = 2*maximalOutlineSize(paintPhase);
    if (_ty >= i.r.bottom() + os || _ty + height() <= i.r.y() - os)
        return;
    if (_tx >= i.r.right() + os || _tx + width() <= i.r.x() - os)
        return;

    if ((paintPhase == PaintPhaseBlockBackground || paintPhase == PaintPhaseChildBlockBackground)
        && shouldPaintBackgroundOrBorder() && style()->visibility() == VISIBLE)
        paintBoxDecorations(i, _tx, _ty);

    // We're done.  We don't bother painting any children.
    if (paintPhase == PaintPhaseBlockBackground)
        return;

    // We don't paint our own background, but we do let the kids paint their backgrounds.
    if (paintPhase == PaintPhaseChildBlockBackgrounds)
        paintPhase = PaintPhaseChildBlockBackground;
    PaintInfo paintInfo(i);
    paintInfo.phase = paintPhase;
    
    for (RenderObject *child = firstChild(); child; child = child->nextSibling())
        if (!child->layer() && (child->isTableSection() || child == tCaption))
            child->paint(paintInfo, _tx, _ty);

    if (collapseBorders() && paintPhase == PaintPhaseChildBlockBackground && style()->visibility() == VISIBLE) {
        // Collect all the unique border styles that we want to paint in a sorted list.  Once we
        // have all the styles sorted, we then do individual passes, painting each style of border
        // from lowest precedence to highest precedence.
        paintInfo.phase = PaintPhaseCollapsedTableBorders;
        DeprecatedValueList<CollapsedBorderValue> borderStyles;
        collectBorders(borderStyles);
        DeprecatedValueListIterator<CollapsedBorderValue> it = borderStyles.begin();
        DeprecatedValueListIterator<CollapsedBorderValue> end = borderStyles.end();
        for (; it != end; ++it) {
            m_currentBorder = &(*it);
            for (RenderObject* child = firstChild(); child; child = child->nextSibling())
                if (child->isTableSection())
                    child->paint(paintInfo, _tx, _ty);
        }
    }
        
#ifdef BOX_DEBUG
    outlineBox(i.p, _tx, _ty, "blue");
#endif
}

void RenderTable::paintBoxDecorations(PaintInfo& i, int _tx, int _ty)
{
    int w = width();
    int h = height();
    
    // Account for the caption.
    if (tCaption) {
        int captionHeight = (tCaption->height() + tCaption->marginBottom() +  tCaption->marginTop());
        h -= captionHeight;
        if (tCaption->style()->captionSide() != CAPBOTTOM)
            _ty += captionHeight;
    }

    int my = max(_ty, i.r.y());
    int mh;
    if (_ty < i.r.y())
        mh= max(0, h - (i.r.y() - _ty));
    else
        mh = min(i.r.height(), h);
    
    paintBackground(i.p, style()->backgroundColor(), style()->backgroundLayers(), my, mh, _tx, _ty, w, h);
    
    if (style()->hasBorder() && !collapseBorders())
        paintBorder(i.p, _tx, _ty, w, h, style());
}

void RenderTable::calcMinMaxWidth()
{
    KHTMLAssert(!minMaxKnown());

    if (needSectionRecalc)
        recalcSections();
    recalcHorizontalBorders();

    tableLayout->calcMinMaxWidth();

    if (tCaption && tCaption->minWidth() > m_minWidth)
        m_minWidth = tCaption->minWidth();

    setMinMaxKnown();
}

void RenderTable::splitColumn(int pos, int firstSpan)
{
    // we need to add a new columnStruct
    int oldSize = columns.size();
    columns.resize(oldSize + 1);
    int oldSpan = columns[pos].span;
    KHTMLAssert(oldSpan > firstSpan);
    columns[pos].span = firstSpan;
    memmove(columns.data() + pos + 1, columns.data() + pos, (oldSize-pos)*sizeof(ColumnStruct));
    columns[pos+1].span = oldSpan - firstSpan;

    // change width of all rows.
    for (RenderObject *child = firstChild(); child; child = child->nextSibling()) {
        if (child->isTableSection()) {
            RenderTableSection *section = static_cast<RenderTableSection *>(child);
            if (section->cCol > pos)
                section->cCol++;
            int size = section->numRows();
            for (int row = 0; row < size; ++row) {
                section->grid[row].row->resize(oldSize + 1);
                RenderTableSection::Row &r = *section->grid[row].row;
                memmove(r.data() + pos + 1, r.data() + pos, (oldSize - pos) * sizeof(RenderTableSection::CellStruct));
                r[pos + 1].cell = 0;
                r[pos + 1].inColSpan = r[pos].inColSpan || r[pos].cell;
            }
        }
    }
    columnPos.resize(numEffCols() + 1);
    setNeedsLayoutAndMinMaxRecalc();
}

void RenderTable::appendColumn(int span)
{
    // easy case.
    int pos = columns.size();
    int newSize = pos + 1;
    columns.resize(newSize);
    columns[pos].span = span;

    // change width of all rows.
    for (RenderObject *child = firstChild(); child; child = child->nextSibling()) {
        if (child->isTableSection()) {
            RenderTableSection *section = static_cast<RenderTableSection *>(child);
            int size = section->numRows();
            for (int row = 0; row < size; ++row) {
                section->grid[row].row->resize(newSize);
                RenderTableSection::CellStruct& c = section->cellAt(row, pos);
                c.cell = 0;
                c.inColSpan = false;
            }
        }
    }
    columnPos.resize(numEffCols() + 1);
    setNeedsLayoutAndMinMaxRecalc();
}

RenderTableCol *RenderTable::colElement(int col) const
{
    if (!has_col_elems)
        return 0;
    RenderObject *child = firstChild();
    int cCol = 0;
    while (child) {
        if (child->isTableCol()) {
            RenderTableCol *colElem = static_cast<RenderTableCol *>(child);
            int span = colElem->span();
            if (!colElem->firstChild()) {
                cCol += span;
                if (cCol > col)
                    return colElem;
            }

            RenderObject *next = child->firstChild();
            if (!next)
                next = child->nextSibling();
            if (!next && child->parent()->isTableCol())
                next = child->parent()->nextSibling();
            child = next;
        } else if (child == tCaption)
            child = child->nextSibling();
        else
            break;
    }
    return 0;
}

void RenderTable::recalcSectionsIfNeeded()
{
    if (needSectionRecalc)
        recalcSections();
}

void RenderTable::recalcSections()
{
    tCaption = 0;
    head = foot = firstBody = 0;
    has_col_elems = false;

    // We need to get valid pointers to caption, head, foot and firstbody again
    for (RenderObject *child = firstChild(); child; child = child->nextSibling()) {
        switch (child->style()->display()) {
            case TABLE_CAPTION:
                if (!tCaption && child->isRenderBlock()) {
                    tCaption = static_cast<RenderBlock*>(child);
                    tCaption->setNeedsLayout(true);
                }
                break;
            case TABLE_COLUMN:
            case TABLE_COLUMN_GROUP:
                has_col_elems = true;
                break;
            case TABLE_HEADER_GROUP:
                if (child->isTableSection()) {
                    RenderTableSection *section = static_cast<RenderTableSection *>(child);
                    if (!head)
                        head = section;
                    else if (!firstBody)
                        firstBody = section;
                    if (section->needCellRecalc)
                        section->recalcCells();
                }
                break;
            case TABLE_FOOTER_GROUP:
                if (child->isTableSection()) {
                    RenderTableSection *section = static_cast<RenderTableSection *>(child);
                    if (!foot)
                        foot = section;
                    else if (!firstBody)
                        firstBody = section;
                    if (section->needCellRecalc)
                        section->recalcCells();
                }
                break;
            case TABLE_ROW_GROUP:
                if (child->isTableSection()) {
                    RenderTableSection *section = static_cast<RenderTableSection *>(child);
                    if (!firstBody)
                        firstBody = section;
                    if (section->needCellRecalc)
                        section->recalcCells();
                }
                break;
            default:
                break;
        }
    }

    // repair column count (addChild can grow it too much, because it always adds elements to the last row of a section)
    int maxCols = 0;
    for (RenderObject *child = firstChild(); child; child = child->nextSibling()) {
        if (child->isTableSection()) {
            RenderTableSection *section = static_cast<RenderTableSection *>(child);
            int sectionCols = section->numColumns();
            if (sectionCols > maxCols)
                maxCols = sectionCols;
        }
    }
    
    columns.resize(maxCols);
    columnPos.resize(maxCols+1);
    
    needSectionRecalc = false;
    setNeedsLayout(true);
}

RenderObject* RenderTable::removeChildNode(RenderObject* child)
{
    setNeedSectionRecalc();
    return RenderContainer::removeChildNode(child);
}

int RenderTable::calcBorderLeft() const
{
    if (collapseBorders()) {
        // Determined by the first cell of the first row. See the CSS 2.1 spec, section 17.6.2.
        if (numEffCols() == 0)
            return 0;

        unsigned borderWidth = 0;

        const BorderValue& tb = style()->borderLeft();
        if (tb.style() == BHIDDEN)
            return 0;
        if (tb.style() > BHIDDEN)
            borderWidth = tb.width;

        int leftmostColumn = style()->direction() == RTL ? numEffCols() - 1 : 0;
        RenderTableCol *colGroup = colElement(leftmostColumn);
        if (colGroup) {
            const BorderValue& gb = style()->borderLeft();
            if (gb.style() == BHIDDEN)
                return 0;
            if (gb.style() > BHIDDEN && gb.width > borderWidth)
                borderWidth = gb.width;
        }
        
        RenderObject *child = firstChild();
        while (child && !child->isTableSection())
            child = child->nextSibling();
        if (child && child->isTableSection()) {
            RenderTableSection* section = static_cast<RenderTableSection*>(child);
            
            if (section->numRows() == 0)
                return borderWidth / 2;
            
            const BorderValue& sb = section->style()->borderLeft();
            if (sb.style() == BHIDDEN)
                return 0;
            if (sb.style() > BHIDDEN && sb.width > borderWidth)
                borderWidth = sb.width;

            const RenderTableSection::CellStruct& cs = section->cellAt(0, leftmostColumn);
            
            if (cs.cell) {
                const BorderValue& cb = cs.cell->style()->borderLeft();
                if (cb.style() == BHIDDEN)
                    return 0;
                const BorderValue& rb = cs.cell->parent()->style()->borderLeft();
                if (rb.style() == BHIDDEN)
                    return 0;

                if (cb.style() > BHIDDEN && cb.width > borderWidth)
                    borderWidth = cb.width;
                if (rb.style() > BHIDDEN && rb.width > borderWidth)
                    borderWidth = rb.width;
            }
        }
        return borderWidth / 2;
    }
    return RenderBlock::borderLeft();
}
    
int RenderTable::calcBorderRight() const
{
    if (collapseBorders()) {
        // Determined by the last cell of the first row. See the CSS 2.1 spec, section 17.6.2.
        if (numEffCols() == 0)
            return 0;

        unsigned borderWidth = 0;

        const BorderValue& tb = style()->borderRight();
        if (tb.style() == BHIDDEN)
            return 0;
        if (tb.style() > BHIDDEN)
            borderWidth = tb.width;

        int rightmostColumn = style()->direction() == RTL ? 0 : numEffCols() - 1;
        RenderTableCol *colGroup = colElement(rightmostColumn);
        if (colGroup) {
            const BorderValue& gb = style()->borderRight();
            if (gb.style() == BHIDDEN)
                return 0;
            if (gb.style() > BHIDDEN && gb.width > borderWidth)
                borderWidth = gb.width;
        }
        
        RenderObject *child = firstChild();
        while (child && !child->isTableSection())
            child = child->nextSibling();
        if (child && child->isTableSection()) {
            RenderTableSection* section = static_cast<RenderTableSection*>(child);
            
            if (section->numRows() == 0)
                return (borderWidth + 1) / 2;
            
            const BorderValue& sb = section->style()->borderRight();
            if (sb.style() == BHIDDEN)
                return 0;
            if (sb.style() > BHIDDEN && sb.width > borderWidth)
                borderWidth = sb.width;

            const RenderTableSection::CellStruct& cs = section->cellAt(0, rightmostColumn);
            
            if (cs.cell) {
                const BorderValue& cb = cs.cell->style()->borderRight();
                if (cb.style() == BHIDDEN)
                    return 0;
                const BorderValue& rb = cs.cell->parent()->style()->borderRight();
                if (rb.style() == BHIDDEN)
                    return 0;

                if (cb.style() > BHIDDEN && cb.width > borderWidth)
                    borderWidth = cb.width;
                if (rb.style() > BHIDDEN && rb.width > borderWidth)
                    borderWidth = rb.width;
            }
        }
        return (borderWidth + 1) / 2;
    }
    return RenderBlock::borderRight();
}

void RenderTable::recalcHorizontalBorders()
{
    m_borderLeft = calcBorderLeft();
    m_borderRight = calcBorderRight();
}

int RenderTable::borderTop() const
{
    if (collapseBorders())
        return outerBorderTop();
    return RenderBlock::borderTop();
}

int RenderTable::borderBottom() const
{
    if (collapseBorders())
        return outerBorderBottom();
    return RenderBlock::borderBottom();
}

int RenderTable::outerBorderTop() const
{
    if (!collapseBorders())
        return 0;
    int borderWidth = 0;
    RenderTableSection* topSection;
    if (head)
        topSection = head;
    else if (firstBody)
        topSection = firstBody;
    else if (foot)
        topSection = foot;
    else
        topSection = 0;
    if (topSection) {
        borderWidth = topSection->outerBorderTop();
        if (borderWidth == -1)
            return 0;   // Overridden by hidden
    }
    const BorderValue& tb = style()->borderTop();
    if (tb.style() == BHIDDEN)
        return 0;
    if (tb.style() > BHIDDEN && (int)(tb.width / 2) > borderWidth)
        borderWidth = tb.width / 2;
    return borderWidth;
}

int RenderTable::outerBorderBottom() const
{
    if (!collapseBorders())
        return 0;
    int borderWidth = 0;
    RenderTableSection* bottomSection;
    if (foot)
        bottomSection = foot;
    else {
        RenderObject* child;
        for (child = lastChild(); child && !child->isTableSection(); child = child->previousSibling());
        bottomSection = child ? static_cast<RenderTableSection *>(child) : 0;
    }
    if (bottomSection) {
        borderWidth = bottomSection->outerBorderBottom();
        if (borderWidth == -1)
            return 0;   // Overridden by hidden
    }
    const BorderValue& tb = style()->borderBottom();
    if (tb.style() == BHIDDEN)
        return 0;
    if (tb.style() > BHIDDEN && (int)(tb.width + 1) / 2 > borderWidth)
        borderWidth = (tb.width + 1) / 2;
    return borderWidth;
}

int RenderTable::outerBorderLeft() const
{
    if (!collapseBorders())
        return 0;

    int borderWidth = 0;

    const BorderValue& tb = style()->borderLeft();
    if (tb.style() == BHIDDEN)
        return 0;
    if (tb.style() > BHIDDEN)
        borderWidth = tb.width / 2;

    bool allHidden = true;
    for (RenderObject *child = firstChild(); child; child = child->nextSibling()) {
        if (!child->isTableSection())
            continue;
        int sw = static_cast<RenderTableSection *>(child)->outerBorderLeft();
        if (sw == -1)
            continue;
        else
            allHidden = false;
        borderWidth = max(borderWidth, sw);
    }
    if (allHidden)
        return 0;

    return borderWidth;
}

int RenderTable::outerBorderRight() const
{
    if (!collapseBorders())
        return 0;

    int borderWidth = 0;

    const BorderValue& tb = style()->borderRight();
    if (tb.style() == BHIDDEN)
        return 0;
    if (tb.style() > BHIDDEN)
        borderWidth = (tb.width + 1) / 2;

    bool allHidden = true;
    for (RenderObject *child = firstChild(); child; child = child->nextSibling()) {
        if (!child->isTableSection())
            continue;
        int sw = static_cast<RenderTableSection *>(child)->outerBorderRight();
        if (sw == -1)
            continue;
        else
            allHidden = false;
        borderWidth = max(borderWidth, sw);
    }
    if (allHidden)
        return 0;

    return borderWidth;
}

RenderTableSection* RenderTable::sectionAbove(const RenderTableSection* section, bool skipEmptySections) const
{
    if (section == head)
        return 0;
    RenderObject *prevSection = (section == foot ? lastChild() : section)->previousSibling();
    while (prevSection) {
        if (prevSection->isTableSection() && prevSection != head && prevSection != foot && (!skipEmptySections || static_cast<RenderTableSection*>(prevSection)->numRows()))
            break;
        prevSection = prevSection->previousSibling();
    }
    if (!prevSection && head && (!skipEmptySections || head->numRows()))
        prevSection = head;
    return static_cast<RenderTableSection*>(prevSection);
}

RenderTableSection* RenderTable::sectionBelow(const RenderTableSection* section, bool skipEmptySections) const
{
    if (section == foot)
        return 0;
    RenderObject *nextSection = (section == head ? firstChild() : section)->nextSibling();
    while (nextSection) {
        if (nextSection->isTableSection() && nextSection != head && nextSection != foot && (!skipEmptySections || static_cast<RenderTableSection*>(nextSection)->numRows()))
            break;
        nextSection = nextSection->nextSibling();
    }
    if (!nextSection && foot && (!skipEmptySections || foot->numRows()))
        nextSection = foot;
    return static_cast<RenderTableSection*>(nextSection);
}

RenderTableCell* RenderTable::cellAbove(const RenderTableCell* cell) const
{
    // Find the section and row to look in
    int r = cell->row();
    RenderTableSection* section = 0;
    int rAbove = 0;
    if (r > 0) {
        // cell is not in the first row, so use the above row in its own section
        section = cell->section();
        rAbove = r-1;
    } else {
        section = sectionAbove(cell->section(), true);
        if (section)
            rAbove = section->numRows() - 1;
    }

    // Look up the cell in the section's grid, which requires effective col index
    if (section) {
        int effCol = colToEffCol(cell->col());
        RenderTableSection::CellStruct aboveCell;
        // If we hit a span back up to a real cell.
        do {
            aboveCell = section->cellAt(rAbove, effCol);
            effCol--;
        } while (!aboveCell.cell && aboveCell.inColSpan && effCol >=0);
        return aboveCell.cell;
    } else
        return 0;
}

RenderTableCell* RenderTable::cellBelow(const RenderTableCell* cell) const
{
    // Find the section and row to look in
    int r = cell->row() + cell->rowSpan() - 1;
    RenderTableSection* section = 0;
    int rBelow = 0;
    if (r < cell->section()->numRows() - 1) {
        // The cell is not in the last row, so use the next row in the section.
        section = cell->section();
        rBelow= r+1;
    } else {
        section = sectionBelow(cell->section(), true);
        if (section)
            rBelow = 0;
    }

    // Look up the cell in the section's grid, which requires effective col index
    if (section) {
        int effCol = colToEffCol(cell->col());
        RenderTableSection::CellStruct belowCell;
        // If we hit a colspan back up to a real cell.
        do {
            belowCell = section->cellAt(rBelow, effCol);
            effCol--;
        } while (!belowCell.cell && belowCell.inColSpan && effCol >=0);
        return belowCell.cell;
    } else
        return 0;
}

RenderTableCell* RenderTable::cellBefore(const RenderTableCell* cell) const
{
    RenderTableSection* section = cell->section();
    int effCol = colToEffCol(cell->col());
    if (effCol == 0)
        return 0;
    
    // If we hit a colspan back up to a real cell.
    RenderTableSection::CellStruct prevCell;
    do {
        prevCell = section->cellAt(cell->row(), effCol-1);
        effCol--;
    } while (!prevCell.cell && prevCell.inColSpan && effCol >=0);
    return prevCell.cell;
}

RenderTableCell* RenderTable::cellAfter(const RenderTableCell* cell) const
{
    int effCol = colToEffCol(cell->col() + cell->colSpan());
    if (effCol >= numEffCols())
        return 0;
    return cell->section()->cellAt(cell->row(), effCol).cell;
}

RenderBlock* RenderTable::firstLineBlock() const
{
    return 0;
}

void RenderTable::updateFirstLetter()
{
}

IntRect RenderTable::getOverflowClipRect(int tx, int ty)
{
    IntRect rect = RenderBlock::getOverflowClipRect(tx, ty);
    
    // If we have a caption, expand the clip to include the caption.
    // FIXME: Technically this is wrong, but it's virtually impossible to fix this
    // for real until captions have been re-written.
    // FIXME: This code assumes (like all our other caption code) that only top/bottom are
    // supported.  When we actually support left/right and stop mapping them to top/bottom,
    // we might have to hack this code first (depending on what order we do these bug fixes in).
    if (tCaption) {
        rect.setHeight(height());
        rect.setY(ty);
    }

    return rect;
}

#ifndef NDEBUG
void RenderTable::dump(QTextStream *stream, DeprecatedString ind) const
{
    if (tCaption)
        *stream << " tCaption";
    if (head)
        *stream << " head";
    if (foot)
        *stream << " foot";

    *stream << endl << ind << "cspans:";
    for ( unsigned int i = 0; i < columns.size(); i++ )
        *stream << " " << columns[i].span;
    *stream << endl << ind;

    RenderBlock::dump(stream,ind);
}
#endif

}
