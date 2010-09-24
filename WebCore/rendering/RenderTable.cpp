/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "RenderTable.h"

#include "AutoTableLayout.h"
#include "DeleteButtonController.h"
#include "Document.h"
#include "FixedTableLayout.h"
#include "FrameView.h"
#include "HitTestResult.h"
#include "HTMLNames.h"
#include "RenderLayer.h"
#include "RenderTableCell.h"
#include "RenderTableCol.h"
#include "RenderTableSection.h"
#include "RenderView.h"

using namespace std;

namespace WebCore {

using namespace HTMLNames;

RenderTable::RenderTable(Node* node)
    : RenderBlock(node)
    , m_caption(0)
    , m_head(0)
    , m_foot(0)
    , m_firstBody(0)
    , m_currentBorder(0)
    , m_hasColElements(false)
    , m_needsSectionRecalc(0)
    , m_hSpacing(0)
    , m_vSpacing(0)
    , m_borderLeft(0)
    , m_borderRight(0)
{
    setChildrenInline(false);
    m_columnPos.fill(0, 2);
    m_columns.fill(ColumnStruct(), 1);
    
}

void RenderTable::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBlock::styleDidChange(diff, oldStyle);

    ETableLayout oldTableLayout = oldStyle ? oldStyle->tableLayout() : TAUTO;

    // In the collapsed border model, there is no cell spacing.
    m_hSpacing = collapseBorders() ? 0 : style()->horizontalBorderSpacing();
    m_vSpacing = collapseBorders() ? 0 : style()->verticalBorderSpacing();
    m_columnPos[0] = m_hSpacing;

    if (!m_tableLayout || style()->tableLayout() != oldTableLayout) {
        // According to the CSS2 spec, you only use fixed table layout if an
        // explicit width is specified on the table.  Auto width implies auto table layout.
        if (style()->tableLayout() == TFIXED && !style()->width().isAuto())
            m_tableLayout.set(new FixedTableLayout(this));
        else
            m_tableLayout.set(new AutoTableLayout(this));
    }
}

static inline void resetSectionPointerIfNotBefore(RenderTableSection*& ptr, RenderObject* before)
{
    if (!before || !ptr)
        return;
    RenderObject* o = before->previousSibling();
    while (o && o != ptr)
        o = o->previousSibling();
    if (!o)
        ptr = 0;
}

void RenderTable::addChild(RenderObject* child, RenderObject* beforeChild)
{
    // Make sure we don't append things after :after-generated content if we have it.
    if (!beforeChild && isAfterContent(lastChild()))
        beforeChild = lastChild();

    bool wrapInAnonymousSection = !child->isPositioned();

    if (child->isRenderBlock() && child->style()->display() == TABLE_CAPTION) {
        // First caption wins.
        if (beforeChild && m_caption) {
            RenderObject* o = beforeChild->previousSibling();
            while (o && o != m_caption)
                o = o->previousSibling();
            if (!o)
                m_caption = 0;
        }
        if (!m_caption)
            m_caption = toRenderBlock(child);
        wrapInAnonymousSection = false;
    } else if (child->isTableCol()) {
        m_hasColElements = true;
        wrapInAnonymousSection = false;
    } else if (child->isTableSection()) {
        switch (child->style()->display()) {
            case TABLE_HEADER_GROUP:
                resetSectionPointerIfNotBefore(m_head, beforeChild);
                if (!m_head) {
                    m_head = toRenderTableSection(child);
                } else {
                    resetSectionPointerIfNotBefore(m_firstBody, beforeChild);
                    if (!m_firstBody) 
                        m_firstBody = toRenderTableSection(child);
                }
                wrapInAnonymousSection = false;
                break;
            case TABLE_FOOTER_GROUP:
                resetSectionPointerIfNotBefore(m_foot, beforeChild);
                if (!m_foot) {
                    m_foot = toRenderTableSection(child);
                    wrapInAnonymousSection = false;
                    break;
                }
                // Fall through.
            case TABLE_ROW_GROUP:
                resetSectionPointerIfNotBefore(m_firstBody, beforeChild);
                if (!m_firstBody)
                    m_firstBody = toRenderTableSection(child);
                wrapInAnonymousSection = false;
                break;
            default:
                ASSERT_NOT_REACHED();
        }
    } else if (child->isTableCell() || child->isTableRow())
        wrapInAnonymousSection = true;
    else
        wrapInAnonymousSection = true;

    if (!wrapInAnonymousSection) {
        // If the next renderer is actually wrapped in an anonymous table section, we need to go up and find that.
        while (beforeChild && !beforeChild->isTableSection() && !beforeChild->isTableCol() && beforeChild->style()->display() != TABLE_CAPTION)
            beforeChild = beforeChild->parent();

        RenderBox::addChild(child, beforeChild);
        return;
    }

    if (!beforeChild && lastChild() && lastChild()->isTableSection() && lastChild()->isAnonymous()) {
        lastChild()->addChild(child);
        return;
    }

    RenderObject* lastBox = beforeChild;
    while (lastBox && lastBox->parent()->isAnonymous() && !lastBox->isTableSection() && lastBox->style()->display() != TABLE_CAPTION && lastBox->style()->display() != TABLE_COLUMN_GROUP)
        lastBox = lastBox->parent();
    if (lastBox && lastBox->isAnonymous() && !isAfterContent(lastBox)) {
        lastBox->addChild(child, beforeChild);
        return;
    }

    if (beforeChild && !beforeChild->isTableSection() && beforeChild->style()->display() != TABLE_CAPTION && beforeChild->style()->display() != TABLE_COLUMN_GROUP)
        beforeChild = 0;
    RenderTableSection* section = new (renderArena()) RenderTableSection(document() /* anonymous */);
    RefPtr<RenderStyle> newStyle = RenderStyle::create();
    newStyle->inheritFrom(style());
    newStyle->setDisplay(TABLE_ROW_GROUP);
    section->setStyle(newStyle.release());
    addChild(section, beforeChild);
    section->addChild(child);
}

void RenderTable::removeChild(RenderObject* oldChild)
{
    RenderBox::removeChild(oldChild);
    setNeedsSectionRecalc();
}

void RenderTable::computeLogicalWidth()
{
    if (isPositioned())
        computePositionedLogicalWidth();

    RenderBlock* cb = containingBlock();
    int availableWidth = cb->availableWidth();

    LengthType widthType = style()->width().type();
    if (widthType > Relative && style()->width().isPositive()) {
        // Percent or fixed table
        setWidth(style()->width().calcMinValue(availableWidth));
        setWidth(max(minPreferredLogicalWidth(), width()));
    } else {
        // An auto width table should shrink to fit within the line width if necessary in order to 
        // avoid overlapping floats.
        availableWidth = cb->availableLogicalWidthForLine(y(), false);
        
        // Subtract out any fixed margins from our available width for auto width tables.
        int marginTotal = 0;
        if (!style()->marginLeft().isAuto())
            marginTotal += style()->marginLeft().calcValue(availableWidth);
        if (!style()->marginRight().isAuto())
            marginTotal += style()->marginRight().calcValue(availableWidth);
            
        // Subtract out our margins to get the available content width.
        int availContentWidth = max(0, availableWidth - marginTotal);
        
        // Ensure we aren't bigger than our max width or smaller than our min width.
        setWidth(min(availContentWidth, maxPreferredLogicalWidth()));
    }
    
    setWidth(max(width(), minPreferredLogicalWidth()));

    // Finally, with our true width determined, compute our margins for real.
    m_marginRight = 0;
    m_marginLeft = 0;
    computeInlineDirectionMargins(style()->marginLeft(), style()->marginRight(), availableWidth);
}

void RenderTable::layout()
{
    ASSERT(needsLayout());

    if (layoutOnlyPositionedObjects())
        return;

    recalcSectionsIfNeeded();
        
    LayoutRepainter repainter(*this, checkForRepaintDuringLayout());
    LayoutStateMaintainer statePusher(view(), this, IntSize(x(), y()));

    setHeight(0);
    m_overflow.clear();

    initMaxMarginValues();
    
    int oldWidth = width();
    computeLogicalWidth();

    if (m_caption && width() != oldWidth)
        m_caption->setNeedsLayout(true, false);

    // FIXME: The optimisation below doesn't work since the internal table
    // layout could have changed.  we need to add a flag to the table
    // layout that tells us if something has changed in the min max
    // calculations to do it correctly.
//     if ( oldWidth != width() || columns.size() + 1 != columnPos.size() )
    m_tableLayout->layout();

    setCellWidths();

    // layout child objects
    int calculatedHeight = 0;
    int oldTableTop = m_caption ? m_caption->height() + m_caption->marginTop() + m_caption->marginBottom() : 0;

    bool collapsing = collapseBorders();

    for (RenderObject* child = firstChild(); child; child = child->nextSibling()) {
        if (child->isTableSection()) {
            child->layoutIfNeeded();
            RenderTableSection* section = toRenderTableSection(child);
            calculatedHeight += section->calcRowHeight();
            if (collapsing)
                section->recalcOuterBorder();
            ASSERT(!section->needsLayout());
        } else if (child->isTableCol()) {
            child->layoutIfNeeded();
            ASSERT(!child->needsLayout());
        }
    }

    // Only lay out one caption, since it's the only one we're going to end up painting.
    if (m_caption)
        m_caption->layoutIfNeeded();

    // If any table section moved vertically, we will just repaint everything from that
    // section down (it is quite unlikely that any of the following sections
    // did not shift).
    bool sectionMoved = false;
    int movedSectionTop = 0;

    // FIXME: Collapse caption margin.
    if (m_caption && m_caption->style()->captionSide() != CAPBOTTOM) {
        IntRect captionRect(m_caption->x(), m_caption->y(), m_caption->width(), m_caption->height());

        m_caption->setLocation(m_caption->marginLeft(), height());
        if (!selfNeedsLayout() && m_caption->checkForRepaintDuringLayout())
            m_caption->repaintDuringLayoutIfMoved(captionRect);

        setHeight(height() + m_caption->height() + m_caption->marginTop() + m_caption->marginBottom());

        if (height() != oldTableTop) {
            sectionMoved = true;
            movedSectionTop = min(height(), oldTableTop);
        }
    }

    int bpTop = borderTop() + (collapsing ? 0 : paddingTop());
    int bpBottom = borderBottom() + (collapsing ? 0 : paddingBottom());
    
    setHeight(height() + bpTop);

    if (!isPositioned())
        computeLogicalHeight();

    Length h = style()->height();
    int th = 0;
    if (h.isFixed())
        // Tables size as though CSS height includes border/padding.
        th = h.value() - (bpTop + bpBottom);
    else if (h.isPercent())
        th = computePercentageLogicalHeight(h);
    th = max(0, th);

    for (RenderObject* child = firstChild(); child; child = child->nextSibling()) {
        if (child->isTableSection())
            // FIXME: Distribute extra height between all table body sections instead of giving it all to the first one.
            toRenderTableSection(child)->layoutRows(child == m_firstBody ? max(0, th - calculatedHeight) : 0);
    }

    if (!m_firstBody && th > calculatedHeight && !document()->inQuirksMode()) {
        // Completely empty tables (with no sections or anything) should at least honor specified height
        // in strict mode.
        setHeight(height() + th);
    }
    
    int bl = borderLeft();
    if (!collapsing)
        bl += paddingLeft();

    // position the table sections
    RenderTableSection* section = m_head ? m_head : (m_firstBody ? m_firstBody : m_foot);
    while (section) {
        if (!sectionMoved && section->y() != height()) {
            sectionMoved = true;
            movedSectionTop = min(height(), section->y()) + section->topVisibleOverflow();
        }
        section->setLocation(bl, height());

        setHeight(height() + section->height());
        section = sectionBelow(section);
    }

    setHeight(height() + bpBottom);

    if (m_caption && m_caption->style()->captionSide() == CAPBOTTOM) {
        IntRect captionRect(m_caption->x(), m_caption->y(), m_caption->width(), m_caption->height());

        m_caption->setLocation(m_caption->marginLeft(), height());
        if (!selfNeedsLayout() && m_caption->checkForRepaintDuringLayout())
            m_caption->repaintDuringLayoutIfMoved(captionRect);

        setHeight(height() + m_caption->height() + m_caption->marginTop() + m_caption->marginBottom());
    }

    if (isPositioned())
        computeLogicalHeight();

    // table can be containing block of positioned elements.
    // FIXME: Only pass true if width or height changed.
    layoutPositionedObjects(true);

    // Add overflow from borders.
    int rightBorderOverflow = width() + (collapsing ? outerBorderRight() - borderRight() : 0);
    int leftBorderOverflow = collapsing ? borderLeft() - outerBorderLeft() : 0;
    int bottomBorderOverflow = height() + (collapsing ? outerBorderBottom() - borderBottom() : 0);
    int topBorderOverflow = collapsing ? borderTop() - outerBorderTop() : 0;
    addLayoutOverflow(IntRect(leftBorderOverflow, topBorderOverflow, rightBorderOverflow - leftBorderOverflow, bottomBorderOverflow - topBorderOverflow));
    
    // Add visual overflow from box-shadow and reflections.
    addShadowOverflow();
    
    // Add overflow from our caption.
    if (m_caption)
        addOverflowFromChild(m_caption);

    // Add overflow from our sections.
    for (RenderObject* child = firstChild(); child; child = child->nextSibling()) {
        if (child->isTableSection()) {
            RenderTableSection* section = toRenderTableSection(child);
            addOverflowFromChild(section);
        }
    }

    statePusher.pop();

    if (view()->layoutState()->m_pageHeight)
        setPageY(view()->layoutState()->pageY(y()));

    bool didFullRepaint = repainter.repaintAfterLayout();
    // Repaint with our new bounds if they are different from our old bounds.
    if (!didFullRepaint && sectionMoved)
        repaintRectangle(IntRect(leftVisibleOverflow(), movedSectionTop, rightVisibleOverflow() - leftVisibleOverflow(), bottomVisibleOverflow() - movedSectionTop));
    
    setNeedsLayout(false);
}

void RenderTable::setCellWidths()
{
    for (RenderObject* child = firstChild(); child; child = child->nextSibling()) {
        if (child->isTableSection())
            toRenderTableSection(child)->setCellWidths();
    }
}

void RenderTable::paint(PaintInfo& paintInfo, int tx, int ty)
{
    tx += x();
    ty += y();

    PaintPhase paintPhase = paintInfo.phase;

    int os = 2 * maximalOutlineSize(paintPhase);
    if (ty + topVisibleOverflow() >= paintInfo.rect.bottom() + os || ty + bottomVisibleOverflow() <= paintInfo.rect.y() - os)
        return;
    if (tx + leftVisibleOverflow() >= paintInfo.rect.right() + os || tx + rightVisibleOverflow() <= paintInfo.rect.x() - os)
        return;

    bool pushedClip = pushContentsClip(paintInfo, tx, ty);    
    paintObject(paintInfo, tx, ty);
    if (pushedClip)
        popContentsClip(paintInfo, paintPhase, tx, ty);
}

void RenderTable::paintObject(PaintInfo& paintInfo, int tx, int ty)
{
    PaintPhase paintPhase = paintInfo.phase;
    if ((paintPhase == PaintPhaseBlockBackground || paintPhase == PaintPhaseChildBlockBackground) && hasBoxDecorations() && style()->visibility() == VISIBLE)
        paintBoxDecorations(paintInfo, tx, ty);

    if (paintPhase == PaintPhaseMask) {
        paintMask(paintInfo, tx, ty);
        return;
    }

    // We're done.  We don't bother painting any children.
    if (paintPhase == PaintPhaseBlockBackground)
        return;
    
    // We don't paint our own background, but we do let the kids paint their backgrounds.
    if (paintPhase == PaintPhaseChildBlockBackgrounds)
        paintPhase = PaintPhaseChildBlockBackground;

    PaintInfo info(paintInfo);
    info.phase = paintPhase;
    info.updatePaintingRootForChildren(this);

    for (RenderObject* child = firstChild(); child; child = child->nextSibling()) {
        if (child->isBox() && !toRenderBox(child)->hasSelfPaintingLayer() && (child->isTableSection() || child == m_caption))
            child->paint(info, tx, ty);
    }
    
    if (collapseBorders() && paintPhase == PaintPhaseChildBlockBackground && style()->visibility() == VISIBLE) {
        // Collect all the unique border styles that we want to paint in a sorted list.  Once we
        // have all the styles sorted, we then do individual passes, painting each style of border
        // from lowest precedence to highest precedence.
        info.phase = PaintPhaseCollapsedTableBorders;
        RenderTableCell::CollapsedBorderStyles borderStyles;
        RenderObject* stop = nextInPreOrderAfterChildren();
        for (RenderObject* o = firstChild(); o && o != stop; o = o->nextInPreOrder())
            if (o->isTableCell())
                toRenderTableCell(o)->collectBorderStyles(borderStyles);
        RenderTableCell::sortBorderStyles(borderStyles);
        size_t count = borderStyles.size();
        for (size_t i = 0; i < count; ++i) {
            m_currentBorder = &borderStyles[i];
            for (RenderObject* child = firstChild(); child; child = child->nextSibling())
                if (child->isTableSection())
                    child->paint(info, tx, ty);
        }
        m_currentBorder = 0;
    }
}

void RenderTable::paintBoxDecorations(PaintInfo& paintInfo, int tx, int ty)
{
    if (!paintInfo.shouldPaintWithinRoot(this))
        return;

    int w = width();
    int h = height();

    // Account for the caption.
    if (m_caption) {
        int captionHeight = (m_caption->height() + m_caption->marginBottom() +  m_caption->marginTop());
        h -= captionHeight;
        if (m_caption->style()->captionSide() != CAPBOTTOM)
            ty += captionHeight;
    }

    paintBoxShadow(paintInfo.context, tx, ty, w, h, style(), Normal);
    
    paintFillLayers(paintInfo, style()->visitedDependentColor(CSSPropertyBackgroundColor), style()->backgroundLayers(), tx, ty, w, h);
    paintBoxShadow(paintInfo.context, tx, ty, w, h, style(), Inset);

    if (style()->hasBorder() && !collapseBorders())
        paintBorder(paintInfo.context, tx, ty, w, h, style());
}

void RenderTable::paintMask(PaintInfo& paintInfo, int tx, int ty)
{
    if (style()->visibility() != VISIBLE || paintInfo.phase != PaintPhaseMask)
        return;

    int w = width();
    int h = height();

    // Account for the caption.
    if (m_caption) {
        int captionHeight = (m_caption->height() + m_caption->marginBottom() +  m_caption->marginTop());
        h -= captionHeight;
        if (m_caption->style()->captionSide() != CAPBOTTOM)
            ty += captionHeight;
    }

    paintMaskImages(paintInfo, tx, ty, w, h);
}

void RenderTable::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());

    recalcSectionsIfNeeded();
    recalcHorizontalBorders();

    m_tableLayout->computePreferredLogicalWidths(m_minPreferredLogicalWidth, m_maxPreferredLogicalWidth);

    if (m_caption)
        m_minPreferredLogicalWidth = max(m_minPreferredLogicalWidth, m_caption->minPreferredLogicalWidth());

    setPreferredLogicalWidthsDirty(false);
}

void RenderTable::splitColumn(int pos, int firstSpan)
{
    // we need to add a new columnStruct
    int oldSize = m_columns.size();
    m_columns.grow(oldSize + 1);
    int oldSpan = m_columns[pos].span;
    ASSERT(oldSpan > firstSpan);
    m_columns[pos].span = firstSpan;
    memmove(m_columns.data() + pos + 1, m_columns.data() + pos, (oldSize - pos) * sizeof(ColumnStruct));
    m_columns[pos + 1].span = oldSpan - firstSpan;

    // change width of all rows.
    for (RenderObject* child = firstChild(); child; child = child->nextSibling()) {
        if (child->isTableSection())
            toRenderTableSection(child)->splitColumn(pos, firstSpan);
    }

    m_columnPos.grow(numEffCols() + 1);
    setNeedsLayoutAndPrefWidthsRecalc();
}

void RenderTable::appendColumn(int span)
{
    // easy case.
    int pos = m_columns.size();
    int newSize = pos + 1;
    m_columns.grow(newSize);
    m_columns[pos].span = span;

    // change width of all rows.
    for (RenderObject* child = firstChild(); child; child = child->nextSibling()) {
        if (child->isTableSection())
            toRenderTableSection(child)->appendColumn(pos);
    }

    m_columnPos.grow(numEffCols() + 1);
    setNeedsLayoutAndPrefWidthsRecalc();
}

RenderTableCol* RenderTable::nextColElement(RenderTableCol* current) const
{
    RenderObject* next = current->firstChild();
    if (!next)
        next = current->nextSibling();
    if (!next && current->parent()->isTableCol())
        next = current->parent()->nextSibling();

    while (next) {
        if (next->isTableCol())
            return toRenderTableCol(next);
        if (next != m_caption)
            return 0;
        next = next->nextSibling();
    }
    
    return 0;
}

RenderTableCol* RenderTable::colElement(int col, bool* startEdge, bool* endEdge) const
{
    if (!m_hasColElements)
        return 0;
    RenderObject* child = firstChild();
    int cCol = 0;

    while (child) {
        if (child->isTableCol())
            break;
        if (child != m_caption)
            return 0;
        child = child->nextSibling();
    }
    if (!child)
        return 0;

    RenderTableCol* colElem = toRenderTableCol(child);
    while (colElem) {
        int span = colElem->span();
        if (!colElem->firstChild()) {
            int startCol = cCol;
            int endCol = cCol + span - 1;
            cCol += span;
            if (cCol > col) {
                if (startEdge)
                    *startEdge = startCol == col;
                if (endEdge)
                    *endEdge = endCol == col;
                return colElem;
            }
        }
        colElem = nextColElement(colElem);
    }

    return 0;
}

void RenderTable::recalcSections() const
{
    m_caption = 0;
    m_head = 0;
    m_foot = 0;
    m_firstBody = 0;
    m_hasColElements = false;

    // We need to get valid pointers to caption, head, foot and first body again
    for (RenderObject* child = firstChild(); child; child = child->nextSibling()) {
        switch (child->style()->display()) {
            case TABLE_CAPTION:
                if (!m_caption && child->isRenderBlock()) {
                    m_caption = toRenderBlock(child);
                    m_caption->setNeedsLayout(true);
                }
                break;
            case TABLE_COLUMN:
            case TABLE_COLUMN_GROUP:
                m_hasColElements = true;
                break;
            case TABLE_HEADER_GROUP:
                if (child->isTableSection()) {
                    RenderTableSection* section = toRenderTableSection(child);
                    if (!m_head)
                        m_head = section;
                    else if (!m_firstBody)
                        m_firstBody = section;
                    section->recalcCellsIfNeeded();
                }
                break;
            case TABLE_FOOTER_GROUP:
                if (child->isTableSection()) {
                    RenderTableSection* section = toRenderTableSection(child);
                    if (!m_foot)
                        m_foot = section;
                    else if (!m_firstBody)
                        m_firstBody = section;
                    section->recalcCellsIfNeeded();
                }
                break;
            case TABLE_ROW_GROUP:
                if (child->isTableSection()) {
                    RenderTableSection* section = toRenderTableSection(child);
                    if (!m_firstBody)
                        m_firstBody = section;
                    section->recalcCellsIfNeeded();
                }
                break;
            default:
                break;
        }
    }

    // repair column count (addChild can grow it too much, because it always adds elements to the last row of a section)
    int maxCols = 0;
    for (RenderObject* child = firstChild(); child; child = child->nextSibling()) {
        if (child->isTableSection()) {
            RenderTableSection* section = toRenderTableSection(child);
            int sectionCols = section->numColumns();
            if (sectionCols > maxCols)
                maxCols = sectionCols;
        }
    }
    
    m_columns.resize(maxCols);
    m_columnPos.resize(maxCols + 1);

    ASSERT(selfNeedsLayout());

    m_needsSectionRecalc = false;
}

int RenderTable::calcBorderLeft() const
{
    if (collapseBorders()) {
        // Determined by the first cell of the first row. See the CSS 2.1 spec, section 17.6.2.
        if (!numEffCols())
            return 0;

        unsigned borderWidth = 0;

        const BorderValue& tb = style()->borderLeft();
        if (tb.style() == BHIDDEN)
            return 0;
        if (tb.style() > BHIDDEN)
            borderWidth = tb.width();

        int leftmostColumn = style()->direction() == RTL ? numEffCols() - 1 : 0;
        RenderTableCol* colGroup = colElement(leftmostColumn);
        if (colGroup) {
            const BorderValue& gb = style()->borderLeft();
            if (gb.style() == BHIDDEN)
                return 0;
            if (gb.style() > BHIDDEN)
                borderWidth = max(borderWidth, static_cast<unsigned>(gb.width()));
        }
        
        RenderTableSection* firstNonEmptySection = m_head ? m_head : (m_firstBody ? m_firstBody : m_foot);
        if (firstNonEmptySection && !firstNonEmptySection->numRows())
            firstNonEmptySection = sectionBelow(firstNonEmptySection, true);
        
        if (firstNonEmptySection) {
            const BorderValue& sb = firstNonEmptySection->style()->borderLeft();
            if (sb.style() == BHIDDEN)
                return 0;

            if (sb.style() > BHIDDEN)
                borderWidth = max(borderWidth, static_cast<unsigned>(sb.width()));

            const RenderTableSection::CellStruct& cs = firstNonEmptySection->cellAt(0, leftmostColumn);
            
            if (cs.hasCells()) {
                const BorderValue& cb = cs.primaryCell()->style()->borderLeft();
                if (cb.style() == BHIDDEN)
                    return 0;

                const BorderValue& rb = cs.primaryCell()->parent()->style()->borderLeft();
                if (rb.style() == BHIDDEN)
                    return 0;

                if (cb.style() > BHIDDEN)
                    borderWidth = max(borderWidth, static_cast<unsigned>(cb.width()));
                if (rb.style() > BHIDDEN)
                    borderWidth = max(borderWidth, static_cast<unsigned>(rb.width()));
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
        if (!numEffCols())
            return 0;

        unsigned borderWidth = 0;

        const BorderValue& tb = style()->borderRight();
        if (tb.style() == BHIDDEN)
            return 0;
        if (tb.style() > BHIDDEN)
            borderWidth = tb.width();

        int rightmostColumn = style()->direction() == RTL ? 0 : numEffCols() - 1;
        RenderTableCol* colGroup = colElement(rightmostColumn);
        if (colGroup) {
            const BorderValue& gb = style()->borderRight();
            if (gb.style() == BHIDDEN)
                return 0;
            if (gb.style() > BHIDDEN)
                borderWidth = max(borderWidth, static_cast<unsigned>(gb.width()));
        }
        
        RenderTableSection* firstNonEmptySection = m_head ? m_head : (m_firstBody ? m_firstBody : m_foot);
        if (firstNonEmptySection && !firstNonEmptySection->numRows())
            firstNonEmptySection = sectionBelow(firstNonEmptySection, true);
        
        if (firstNonEmptySection) {
            const BorderValue& sb = firstNonEmptySection->style()->borderRight();
            if (sb.style() == BHIDDEN)
                return 0;

            if (sb.style() > BHIDDEN)
                borderWidth = max(borderWidth, static_cast<unsigned>(sb.width()));

            const RenderTableSection::CellStruct& cs = firstNonEmptySection->cellAt(0, rightmostColumn);
            
            if (cs.hasCells()) {
                const BorderValue& cb = cs.primaryCell()->style()->borderRight();
                if (cb.style() == BHIDDEN)
                    return 0;

                const BorderValue& rb = cs.primaryCell()->parent()->style()->borderRight();
                if (rb.style() == BHIDDEN)
                    return 0;

                if (cb.style() > BHIDDEN)
                    borderWidth = max(borderWidth, static_cast<unsigned>(cb.width()));
                if (rb.style() > BHIDDEN)
                    borderWidth = max(borderWidth, static_cast<unsigned>(rb.width()));
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
    if (m_head)
        topSection = m_head;
    else if (m_firstBody)
        topSection = m_firstBody;
    else if (m_foot)
        topSection = m_foot;
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
    if (tb.style() > BHIDDEN)
        borderWidth = max(borderWidth, static_cast<int>(tb.width() / 2));
    return borderWidth;
}

int RenderTable::outerBorderBottom() const
{
    if (!collapseBorders())
        return 0;
    int borderWidth = 0;
    RenderTableSection* bottomSection;
    if (m_foot)
        bottomSection = m_foot;
    else {
        RenderObject* child;
        for (child = lastChild(); child && !child->isTableSection(); child = child->previousSibling()) { }
        bottomSection = child ? toRenderTableSection(child) : 0;
    }
    if (bottomSection) {
        borderWidth = bottomSection->outerBorderBottom();
        if (borderWidth == -1)
            return 0;   // Overridden by hidden
    }
    const BorderValue& tb = style()->borderBottom();
    if (tb.style() == BHIDDEN)
        return 0;
    if (tb.style() > BHIDDEN)
        borderWidth = max(borderWidth, static_cast<int>((tb.width() + 1) / 2));
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
        borderWidth = tb.width() / 2;

    bool allHidden = true;
    for (RenderObject* child = firstChild(); child; child = child->nextSibling()) {
        if (!child->isTableSection())
            continue;
        int sw = toRenderTableSection(child)->outerBorderLeft();
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
        borderWidth = (tb.width() + 1) / 2;

    bool allHidden = true;
    for (RenderObject* child = firstChild(); child; child = child->nextSibling()) {
        if (!child->isTableSection())
            continue;
        int sw = toRenderTableSection(child)->outerBorderRight();
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
    recalcSectionsIfNeeded();

    if (section == m_head)
        return 0;

    RenderObject* prevSection = section == m_foot ? lastChild() : section->previousSibling();
    while (prevSection) {
        if (prevSection->isTableSection() && prevSection != m_head && prevSection != m_foot && (!skipEmptySections || toRenderTableSection(prevSection)->numRows()))
            break;
        prevSection = prevSection->previousSibling();
    }
    if (!prevSection && m_head && (!skipEmptySections || m_head->numRows()))
        prevSection = m_head;
    return toRenderTableSection(prevSection);
}

RenderTableSection* RenderTable::sectionBelow(const RenderTableSection* section, bool skipEmptySections) const
{
    recalcSectionsIfNeeded();

    if (section == m_foot)
        return 0;

    RenderObject* nextSection = section == m_head ? firstChild() : section->nextSibling();
    while (nextSection) {
        if (nextSection->isTableSection() && nextSection != m_head && nextSection != m_foot && (!skipEmptySections || toRenderTableSection(nextSection)->numRows()))
            break;
        nextSection = nextSection->nextSibling();
    }
    if (!nextSection && m_foot && (!skipEmptySections || m_foot->numRows()))
        nextSection = m_foot;
    return toRenderTableSection(nextSection);
}

RenderTableCell* RenderTable::cellAbove(const RenderTableCell* cell) const
{
    recalcSectionsIfNeeded();

    // Find the section and row to look in
    int r = cell->row();
    RenderTableSection* section = 0;
    int rAbove = 0;
    if (r > 0) {
        // cell is not in the first row, so use the above row in its own section
        section = cell->section();
        rAbove = r - 1;
    } else {
        section = sectionAbove(cell->section(), true);
        if (section)
            rAbove = section->numRows() - 1;
    }

    // Look up the cell in the section's grid, which requires effective col index
    if (section) {
        int effCol = colToEffCol(cell->col());
        RenderTableSection::CellStruct& aboveCell = section->cellAt(rAbove, effCol);
        return aboveCell.primaryCell();
    } else
        return 0;
}

RenderTableCell* RenderTable::cellBelow(const RenderTableCell* cell) const
{
    recalcSectionsIfNeeded();

    // Find the section and row to look in
    int r = cell->row() + cell->rowSpan() - 1;
    RenderTableSection* section = 0;
    int rBelow = 0;
    if (r < cell->section()->numRows() - 1) {
        // The cell is not in the last row, so use the next row in the section.
        section = cell->section();
        rBelow = r + 1;
    } else {
        section = sectionBelow(cell->section(), true);
        if (section)
            rBelow = 0;
    }

    // Look up the cell in the section's grid, which requires effective col index
    if (section) {
        int effCol = colToEffCol(cell->col());
        RenderTableSection::CellStruct& belowCell = section->cellAt(rBelow, effCol);
        return belowCell.primaryCell();
    } else
        return 0;
}

RenderTableCell* RenderTable::cellBefore(const RenderTableCell* cell) const
{
    recalcSectionsIfNeeded();

    RenderTableSection* section = cell->section();
    int effCol = colToEffCol(cell->col());
    if (!effCol)
        return 0;
    
    // If we hit a colspan back up to a real cell.
    RenderTableSection::CellStruct& prevCell = section->cellAt(cell->row(), effCol - 1);
    return prevCell.primaryCell();
}

RenderTableCell* RenderTable::cellAfter(const RenderTableCell* cell) const
{
    recalcSectionsIfNeeded();

    int effCol = colToEffCol(cell->col() + cell->colSpan());
    if (effCol >= numEffCols())
        return 0;
    return cell->section()->primaryCellAt(cell->row(), effCol);
}

RenderBlock* RenderTable::firstLineBlock() const
{
    return 0;
}

void RenderTable::updateFirstLetter()
{
}

int RenderTable::firstLineBoxBaseline() const
{
    RenderTableSection* firstNonEmptySection = m_head ? m_head : (m_firstBody ? m_firstBody : m_foot);
    if (firstNonEmptySection && !firstNonEmptySection->numRows())
        firstNonEmptySection = sectionBelow(firstNonEmptySection, true);

    if (!firstNonEmptySection)
        return -1;

    return firstNonEmptySection->y() + firstNonEmptySection->firstLineBoxBaseline();
}

IntRect RenderTable::overflowClipRect(int tx, int ty)
{
    IntRect rect = RenderBlock::overflowClipRect(tx, ty);
    
    // If we have a caption, expand the clip to include the caption.
    // FIXME: Technically this is wrong, but it's virtually impossible to fix this
    // for real until captions have been re-written.
    // FIXME: This code assumes (like all our other caption code) that only top/bottom are
    // supported.  When we actually support left/right and stop mapping them to top/bottom,
    // we might have to hack this code first (depending on what order we do these bug fixes in).
    if (m_caption) {
        rect.setHeight(height());
        rect.setY(ty);
    }

    return rect;
}

bool RenderTable::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, int xPos, int yPos, int tx, int ty, HitTestAction action)
{
    tx += x();
    ty += y();

    // Check kids first.
    if (!hasOverflowClip() || overflowClipRect(tx, ty).intersects(result.rectFromPoint(xPos, yPos))) {
        for (RenderObject* child = lastChild(); child; child = child->previousSibling()) {
            if (child->isBox() && !toRenderBox(child)->hasSelfPaintingLayer() && (child->isTableSection() || child == m_caption) &&
                child->nodeAtPoint(request, result, xPos, yPos, tx, ty, action)) {
                updateHitTestResult(result, IntPoint(xPos - tx, yPos - ty));
                return true;
            }
        }
    }

    // Check our bounds next.
    IntRect boundsRect = IntRect(tx, ty, width(), height());
    if (visibleToHitTesting() && (action == HitTestBlockBackground || action == HitTestChildBlockBackground) && boundsRect.intersects(result.rectFromPoint(xPos, yPos))) {
        updateHitTestResult(result, IntPoint(xPos - tx, yPos - ty));
        if (!result.addNodeToRectBasedTestResult(node(), xPos, yPos, boundsRect))
            return true;
    }

    return false;
}

}
