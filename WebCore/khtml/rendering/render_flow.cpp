/**
 * This file is part of the html renderer for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003 Apple Computer, Inc.
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
// -------------------------------------------------------------------------

#include <kdebug.h>
#include <assert.h>
#include <qpainter.h>
#include <kglobal.h>

#include "rendering/render_flow.h"
#include "rendering/render_text.h"
#include "rendering/render_table.h"
#include "rendering/render_canvas.h"
#include "xml/dom_nodeimpl.h"
#include "xml/dom_docimpl.h"
#include "html/html_formimpl.h"
#include "render_inline.h"
#include "render_block.h"
#include "render_arena.h"
#include "render_line.h"

#include "khtmlview.h"
#include "htmltags.h"

using namespace DOM;
using namespace khtml;

RenderFlow* RenderFlow::createFlow(DOM::NodeImpl* node, RenderStyle* style, RenderArena* arena)
{
    RenderFlow* result;
    if (style->display() == INLINE)
        result = new (arena) RenderInline(node);
    else
        result = new (arena) RenderBlock(node);
    result->setStyle(style);
    return result;
}

RenderFlow* RenderFlow::continuationBefore(RenderObject* beforeChild)
{
    if (beforeChild && beforeChild->parent() == this)
        return this;
    
    RenderFlow* curr = continuation();
    RenderFlow* nextToLast = this;
    RenderFlow* last = this;
    while (curr) {
        if (beforeChild && beforeChild->parent() == curr) {
            if (curr->firstChild() == beforeChild)
                return last;
            return curr;
        }
        
        nextToLast = last;
        last = curr;
        curr = curr->continuation();
    }
    
    if (!beforeChild && !last->firstChild())
        return nextToLast;
    return last;
}

void RenderFlow::addChildWithContinuation(RenderObject* newChild, RenderObject* beforeChild)
{
    RenderFlow* flow = continuationBefore(beforeChild);
    KHTMLAssert(!beforeChild || beforeChild->parent()->isRenderBlock() ||
                beforeChild->parent()->isRenderInline());
    RenderFlow* beforeChildParent = beforeChild ? static_cast<RenderFlow*>(beforeChild->parent()) : 
                                    (flow->continuation() ? flow->continuation() : flow);
    
    if (newChild->isFloatingOrPositioned())
        return beforeChildParent->addChildToFlow(newChild, beforeChild);
    
    // A continuation always consists of two potential candidates: an inline or an anonymous
    // block box holding block children.
    bool childInline = newChild->isInline();
    bool bcpInline = beforeChildParent->isInline();
    bool flowInline = flow->isInline();
    
    if (flow == beforeChildParent)
        return flow->addChildToFlow(newChild, beforeChild);
    else {
        // The goal here is to match up if we can, so that we can coalesce and create the
        // minimal # of continuations needed for the inline.
        if (childInline == bcpInline)
            return beforeChildParent->addChildToFlow(newChild, beforeChild);
        else if (flowInline == childInline)
            return flow->addChildToFlow(newChild, 0); // Just treat like an append.
        else 
            return beforeChildParent->addChildToFlow(newChild, beforeChild);
    }
}

void RenderFlow::addChild(RenderObject *newChild, RenderObject *beforeChild)
{
#ifdef DEBUG_LAYOUT
    kdDebug( 6040 ) << renderName() << "(RenderFlow)::addChild( " << newChild->renderName() <<
                       ", " << (beforeChild ? beforeChild->renderName() : "0") << " )" << endl;
    kdDebug( 6040 ) << "current height = " << m_height << endl;
#endif

    if (continuation())
        return addChildWithContinuation(newChild, beforeChild);
    return addChildToFlow(newChild, beforeChild);
}

void RenderFlow::deleteLineBoxes(RenderArena* arena)
{
    if (m_firstLineBox) {
        if (!arena)
            arena = renderArena();
        InlineRunBox *curr=m_firstLineBox, *next=0;
        while (curr) {
            next = curr->nextLineBox();
            curr->detach(arena);
            curr = next;
        }
        m_firstLineBox = 0;
        m_lastLineBox = 0;
    }
}

void RenderFlow::detach(RenderArena* renderArena)
{
    deleteLineBoxes(renderArena);
    RenderBox::detach(renderArena);
}

InlineBox* RenderFlow::createInlineBox(bool makePlaceHolderBox, bool isRootLineBox)
{
    if (!isRootLineBox &&
	(isReplaced() || makePlaceHolderBox))                     // Inline tables and inline blocks
        return RenderBox::createInlineBox(false, isRootLineBox);  // (or positioned element placeholders).

    InlineFlowBox* flowBox = 0;
    if (isInlineFlow())
        flowBox = new (renderArena()) InlineFlowBox(this);
    else
        flowBox = new (renderArena()) RootInlineBox(this);
    
    if (!m_firstLineBox)
        m_firstLineBox = m_lastLineBox = flowBox;
    else {
        m_lastLineBox->setNextLineBox(flowBox);
        flowBox->setPreviousLineBox(m_lastLineBox);
        m_lastLineBox = flowBox;
    }

    return flowBox;
}

void RenderFlow::paintLineBoxBackgroundBorder(QPainter *p, int _x, int _y,
                                int _w, int _h, int _tx, int _ty, PaintAction paintAction)
{
    if (!firstLineBox())
        return;
    
    if (style()->visibility() == VISIBLE && paintAction == PaintActionForeground) {
        // We can check the first box and last box and avoid painting if we don't
        // intersect.
        int yPos = _ty + firstLineBox()->yPos();
        int h = lastLineBox()->yPos() + lastLineBox()->height() - firstLineBox()->yPos();
        if( (yPos > _y + _h) || (yPos + h < _y))
            return;

        // See if our boxes intersect with the dirty rect.  If so, then we paint
        // them.  Note that boxes can easily overlap, so we can't make any assumptions
        // based off positions of our first line box or our last line box.
        int xOffsetWithinLineBoxes = 0;
        for (InlineRunBox* curr = firstLineBox(); curr; curr = curr->nextLineBox()) {
            yPos = _ty + curr->yPos();
            h = curr->height();
            if ((yPos <= _y + _h) && (yPos + h >= _y))
                curr->paintBackgroundAndBorder(p, _x, _y, _w, _h, _tx, _ty, xOffsetWithinLineBoxes);
            xOffsetWithinLineBoxes += curr->width();
        }
    }
}

void RenderFlow::paintLineBoxDecorations(QPainter *p, int _x, int _y,
                                         int _w, int _h, int _tx, int _ty, PaintAction paintAction)
{
    if (!firstLineBox())
        return;

    if (style()->visibility() == VISIBLE && paintAction == PaintActionForeground) {
        // We only paint line box decorations in strict or almost strict mode.
        // Otherwise we let the TextRuns paint their own decorations.
        if (style()->htmlHacks())
            return;
        
        // We can check the first box and last box and avoid painting if we don't
        // intersect.
        int yPos = _ty + firstLineBox()->yPos();;
        int h = lastLineBox()->yPos() + lastLineBox()->height() - firstLineBox()->yPos();
        if( (yPos > _y + _h) || (yPos + h < _y))
            return;

        // See if our boxes intersect with the dirty rect.  If so, then we paint
        // them.  Note that boxes can easily overlap, so we can't make any assumptions
        // based off positions of our first line box or our last line box.
        for (InlineRunBox* curr = firstLineBox(); curr; curr = curr->nextLineBox()) {
            yPos = _ty + curr->yPos();
            h = curr->height();
            if ((yPos <= _y + _h) && (yPos + h >= _y))
                curr->paintDecorations(p, _x, _y, _w, _h, _tx, _ty);
        }
    }
}

void RenderFlow::repaint(bool immediate)
{
    if (isInlineFlow()) {
        // Find our leftmost position.
        int left = 0;
        int top = firstLineBox() ? firstLineBox()->yPos() : 0;
        for (InlineRunBox* curr = firstLineBox(); curr; curr = curr->nextLineBox())
            if (curr == firstLineBox() || curr->xPos() < left)
                left = curr->xPos();

        // Now invalidate a rectangle.
        int ow = style() ? style()->outlineWidth() : 0;
        if (isCompact())
            left -= m_x;
        containingBlock()->repaintRectangle(-ow+left, -ow+top,
                                            width()+ow*2, height()+ow*2, immediate);
    }
    else {
        if (firstLineBox() && firstLineBox()->topOverflow() < 0) {
            int ow = style() ? style()->outlineWidth() : 0;
            repaintRectangle(-ow, -ow+firstLineBox()->topOverflow(),
                             overflowWidth(false)+ow*2,
                             overflowHeight(false)+ow*2-firstLineBox()->topOverflow(), immediate);
        }
        else
            return RenderBox::repaint(immediate);
    }
}

int
RenderFlow::lowestPosition(bool includeOverflowInterior) const
{
    int bottom = RenderBox::lowestPosition(includeOverflowInterior);
    if (!includeOverflowInterior && style()->hidesOverflow())
        return bottom;

    // FIXME: Come up with a way to use the layer tree to avoid visiting all the kids.
    // For now, we have to descend into all the children, since we may have a huge abs div inside
    // a tiny rel div buried somewhere deep in our child tree.  In this case we have to get to
    // the abs div.
    for (RenderObject *c = firstChild(); c; c = c->nextSibling()) {
        if (!c->isFloatingOrPositioned()) {
            int lp = c->yPos() + c->lowestPosition(false);
            bottom = QMAX(bottom, lp);
        }
    }
    
    return bottom;
}

int RenderFlow::rightmostPosition(bool includeOverflowInterior) const
{
    int right = RenderBox::rightmostPosition(includeOverflowInterior);
    if (!includeOverflowInterior && style()->hidesOverflow())
        return right;

    // FIXME: Come up with a way to use the layer tree to avoid visiting all the kids.
    // For now, we have to descend into all the children, since we may have a huge abs div inside
    // a tiny rel div buried somewhere deep in our child tree.  In this case we have to get to
    // the abs div.
    for (RenderObject *c = firstChild(); c; c = c->nextSibling()) {
        if (!c->isFloatingOrPositioned()) {
            int rp = c->xPos() + c->rightmostPosition(false);
            right = QMAX(right, rp);
        }
    }
    
    return right;
}

