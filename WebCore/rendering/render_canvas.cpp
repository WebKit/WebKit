/**
 * This file is part of the HTML widget for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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
#include "render_canvas.h"

#include "DocumentImpl.h"
#include "FrameView.h"
#include "render_layer.h"

namespace WebCore {

//#define BOX_DEBUG

RenderCanvas::RenderCanvas(NodeImpl* node, FrameView *view)
    : RenderBlock(node)
{
    // Clear our anonymous bit, set because RenderObject assumes
    // any renderer with document as the node is anonymous.
    setIsAnonymous(false);
        
    // init RenderObject attributes
    setInline(false);

    m_view = view;
    // try to contrain the width to the views width

    m_minWidth = 0;
    m_height = 0;

    m_width = m_minWidth;
    m_maxWidth = m_minWidth;

    setPositioned(true); // to 0,0 :)

    m_printingMode = false;
    m_printImages = true;

    m_maximalOutlineSize = 0;
    
    m_selectionStart = 0;
    m_selectionEnd = 0;
    m_selectionStartPos = -1;
    m_selectionEndPos = -1;

    // Create a new root layer for our layer hierarchy.
    m_layer = new (node->getDocument()->renderArena()) RenderLayer(this);
}

RenderCanvas::~RenderCanvas()
{
}

void RenderCanvas::calcHeight()
{
    if (!m_printingMode && m_view)
        m_height = m_view->visibleHeight();
}

void RenderCanvas::calcWidth()
{
    if (!m_printingMode && m_view)
        m_width = m_view->visibleWidth();
    m_marginLeft = 0;
    m_marginRight = 0;
}

void RenderCanvas::calcMinMaxWidth()
{
    KHTMLAssert( !minMaxKnown() );

    RenderBlock::calcMinMaxWidth();

    m_maxWidth = m_minWidth;

    setMinMaxKnown();
}

void RenderCanvas::layout()
{
    KHTMLAssert(!view()->inLayout());
    
    if (m_printingMode)
        m_minWidth = m_width;

    setChildNeedsLayout(true);
    setMinMaxKnown(false);
    for (RenderObject *c = firstChild(); c; c = c->nextSibling())
        c->setChildNeedsLayout(true);

    if ( recalcMinMax() )
        recalcMinMaxWidths();

    RenderBlock::layout();

    int docw = docWidth();
    int doch = docHeight();

    if (!m_printingMode) {
        setWidth(m_view->visibleWidth());
        setHeight(m_view->visibleHeight());
    }

    // ### we could maybe do the call below better and only pass true if the docsize changed.
    layoutPositionedObjects( true );

    layer()->setHeight(kMax(doch, m_height));
    layer()->setWidth(kMax(docw, m_width));
    
    setNeedsLayout(false);
}

bool RenderCanvas::absolutePosition(int &xPos, int &yPos, bool f)
{
    if ( f && m_view) {
        xPos = m_view->contentsX();
        yPos = m_view->contentsY();
    }
    else {
        xPos = yPos = 0;
    }
    return true;
}

void RenderCanvas::paint(PaintInfo& i, int _tx, int _ty)
{
#ifdef DEBUG_LAYOUT
    kdDebug( 6040 ) << renderName() << "(RenderCanvas) " << this << " ::paintObject() w/h = (" << width() << "/" << height() << ")" << endl;
#endif
    
    // Cache the print rect because the dirty rect could get changed during painting.
    if (m_printingMode) {
        setPrintRect(i.r);
    }
    
    // 1. paint background, borders etc
    if (i.phase == PaintActionBlockBackground) {
        paintBoxDecorations(i, _tx, _ty);
        return;
    }
    
    // 2. paint contents
    for (RenderObject *child = firstChild(); child; child = child->nextSibling())
        if (!child->layer() && !child->isFloating())
            child->paint(i, _tx, _ty);

    if (m_view)
    {
        _tx += m_view->contentsX();
        _ty += m_view->contentsY();
    }
    
    // 3. paint floats.
    if (i.phase == PaintActionFloat)
        paintFloats(i, _tx, _ty);
        
#ifdef BOX_DEBUG
    outlineBox(i.p, _tx, _ty);
#endif
}

void RenderCanvas::paintBoxDecorations(PaintInfo& i, int _tx, int _ty)
{
    // Check to see if we are enclosed by a transparent layer.  If so, we cannot blit
    // when scrolling, and we need to use slow repaints.
    ElementImpl* elt = element()->getDocument()->ownerElement();
    if (view() && elt) {
        RenderLayer* layer = elt->renderer()->enclosingLayer();
        if (layer->isTransparent() || layer->transparentAncestor())
            view()->useSlowRepaints();
    }
    
    if ((firstChild() && firstChild()->style()->visibility() == VISIBLE) || !view())
        return;

    // This code typically only executes if the root element's visibility has been set to hidden.
    // Only fill with a base color (e.g., white) if we're the root document, since iframes/frames with
    // no background in the child document should show the parent's background.
    if (elt || view()->isTransparent())
        view()->useSlowRepaints(); // The parent must show behind the child.
    else
        i.p->fillRect(i.r, view()->palette().active().color(QColorGroup::Base));
}

void RenderCanvas::repaintViewRectangle(const IntRect& ur, bool immediate)
{
    if (m_printingMode || ur.width() == 0 || ur.height() == 0) return;

    IntRect vr = viewRect();
    if (m_view && ur.intersects(vr)) {
        // We always just invalidate the root view, since we could be an iframe that is clipped out
        // or even invisible.
        IntRect r = intersection(ur, vr);
        ElementImpl* elt = element()->getDocument()->ownerElement();
        if (!elt)
            m_view->repaintRectangle(r, immediate);
        else {
            // Subtract out the contentsX and contentsY offsets to get our coords within the viewing
            // rectangle.
            r.move(-m_view->contentsX(), -m_view->contentsY());

            RenderObject* obj = elt->renderer();
            // FIXME: Hardcoded offsets here are not good.
            int yFrameOffset = m_view->hasBorder() ? 2 : 0;
            int xFrameOffset = m_view->hasBorder() ? 1 : 0;
            r.move(obj->borderLeft() + obj->paddingLeft() + xFrameOffset,
                obj->borderTop() + obj->paddingTop() + yFrameOffset);
            obj->repaintRectangle(r, immediate);
        }
    }
}

IntRect RenderCanvas::getAbsoluteRepaintRect()
{
    IntRect result;
    if (m_view && !m_printingMode)
        result = IntRect(m_view->contentsX(), m_view->contentsY(),
                       m_view->visibleWidth(), m_view->visibleHeight());
    return result;
}

void RenderCanvas::computeAbsoluteRepaintRect(IntRect& r, bool f)
{
    if (m_printingMode)
        return;

    if (f && m_view)
        r.move(m_view->contentsX(), m_view->contentsY());
}

void RenderCanvas::absoluteRects(QValueList<IntRect>& rects, int _tx, int _ty)
{
    rects.append(IntRect(_tx, _ty, m_layer->width(), m_layer->height()));
}

IntRect RenderCanvas::selectionRect() const
{
    typedef HashMap<RenderObject*, SelectionInfo*> SelectionMap;
    SelectionMap selectedObjects;

    RenderObject* os = m_selectionStart;
    while (os) {
        RenderObject* no = 0;
        if (os != m_selectionEnd) {
            if (!(no = os->firstChild())) {
                if (!(no = os->nextSibling())) {
                    no = os->parent();
                    while (no && no != m_selectionEnd && !no->nextSibling())
                        no = no->parent();
                    if (no && no != m_selectionEnd)
                        no = no->nextSibling();
                }
            }
        }
        
        if ((os->canBeSelectionLeaf() || os == m_selectionStart || os == m_selectionEnd) && os->selectionState() != SelectionNone) {
            // Blocks are responsible for painting line gaps and margin gaps. They must be examined as well.
//          assert(!selectedObjects.get(os));
            selectedObjects.set(os, new SelectionInfo(os));
            RenderBlock* cb = os->containingBlock();
            while (cb && !cb->isCanvas()) {
                SelectionInfo* blockInfo = selectedObjects.get(cb);
                if (blockInfo)
                    break;
                selectedObjects.set(cb, new SelectionInfo(cb));
                cb = cb->containingBlock();
            }
        }
        
        os = no;
    }

    // Now create a single bounding box rect that encloses the whole selection.
    IntRect selRect;
    SelectionMap::iterator end = selectedObjects.end();
    for (SelectionMap::iterator i = selectedObjects.begin(); i != end; ++i) {
        SelectionInfo* info = i->second;
        selRect.unite(info->rect());
        delete info;
    }
    return selRect;
}

void RenderCanvas::setSelection(RenderObject *s, int sp, RenderObject *e, int ep)
{
    // Make sure both our start and end objects are defined. 
    // Check www.msnbc.com and try clicking around to find the case where this happened.
    if ((s && !e) || (e && !s))
        return;

    // Just return if the selection hasn't changed.
    if (m_selectionStart == s && m_selectionStartPos == sp &&
        m_selectionEnd == e && m_selectionEndPos == ep)
        return;

    // Record the old selected objects.  These will be used later
    // when we compare against the new selected objects.
    int oldStartPos = m_selectionStartPos;
    int oldEndPos = m_selectionEndPos;

    // Objects each have a single selection rect to examine.
    typedef HashMap<RenderObject*, SelectionInfo*> SelectedObjectMap;
    SelectedObjectMap oldSelectedObjects;
    SelectedObjectMap newSelectedObjects;

    // Blocks contain selected objects and fill gaps between them, either on the left, right, or in between lines and blocks.
    // In order to get the repaint rect right, we have to examine left, middle, and right rects individually, since otherwise
    // the union of those rects might remain the same even when changes have occurred.
    typedef HashMap<RenderBlock*, BlockSelectionInfo*> SelectedBlockMap;
    SelectedBlockMap oldSelectedBlocks;
    SelectedBlockMap newSelectedBlocks;

    RenderObject* os = m_selectionStart;
    while (os) {
        RenderObject* no = 0;
        if (os != m_selectionEnd) {
            if (!(no = os->firstChild())) {
                if (!(no = os->nextSibling())) {
                    no = os->parent();
                    while (no && no != m_selectionEnd && !no->nextSibling())
                        no = no->parent();
                    if (no && no != m_selectionEnd)
                        no = no->nextSibling();
                }
            }
        }

        if ((os->canBeSelectionLeaf() || os == m_selectionStart || os == m_selectionEnd) && os->selectionState() != SelectionNone) {
            // Blocks are responsible for painting line gaps and margin gaps.  They must be examined as well.
            oldSelectedObjects.set(os, new SelectionInfo(os));
            RenderBlock* cb = os->containingBlock();
            while (cb && !cb->isCanvas()) {
                BlockSelectionInfo* blockInfo = oldSelectedBlocks.get(cb);
                if (blockInfo)
                    break;
                oldSelectedBlocks.set(cb, new BlockSelectionInfo(cb));
                cb = cb->containingBlock();
            }
        }

        os = no;
    }

    // Now clear the selection.
    SelectedObjectMap::iterator oldObjectsEnd = oldSelectedObjects.end();
    for (SelectedObjectMap::iterator i = oldSelectedObjects.begin(); i != oldObjectsEnd; ++i)
        i->first->setSelectionState(SelectionNone);

    // set selection start and end
    m_selectionStart = s;
    m_selectionStartPos = sp;
    m_selectionEnd = e;
    m_selectionEndPos = ep;

    // Update the selection status of all objects between m_selectionStart and m_selectionEnd
    if (s && s == e)
        s->setSelectionState(SelectionBoth);
    else {
        if (s)
            s->setSelectionState(SelectionStart);
        if (e)
            e->setSelectionState(SelectionEnd);
    }

    RenderObject* o = s;
    while (o) {
        RenderObject* no = 0;
        if (o != s && o != e && o->canBeSelectionLeaf())
            o->setSelectionState(SelectionInside);
        
        if (o != e) {
            if (!(no = o->firstChild())) {
                if ( !(no = o->nextSibling())) {
                    no = o->parent();
                    while (no && no != e && !no->nextSibling())
                        no = no->parent();
                    if (no && no != e)
                        no = no->nextSibling();
                }
            }
        }
        
        o=no;
    }

    // Now that the selection state has been updated for the new objects, walk them again and
    // put them in the new objects list.
    o = s;
    while (o) {
        RenderObject* no = 0;
        if (o != e) {
            if (!(no = o->firstChild())) {
                if ( !(no = o->nextSibling())) {
                    no = o->parent();
                    while (no && no != e && !no->nextSibling())
                        no = no->parent();
                    if (no && no != e)
                        no = no->nextSibling();
                }
            }
        }
        
        if ((o->canBeSelectionLeaf() || o == s || o == e) && o->selectionState() != SelectionNone) {
            newSelectedObjects.set(o, new SelectionInfo(o));
            RenderBlock* cb = o->containingBlock();
            while (cb && !cb->isCanvas()) {
                BlockSelectionInfo* blockInfo = newSelectedBlocks.get(cb);
                if (blockInfo)
                    break;
                newSelectedBlocks.set(cb, new BlockSelectionInfo(cb));
                cb = cb->containingBlock();
            }
        }

        o=no;
    }

    if (!m_view)
        return;

    // Have any of the old selected objects changed compared to the new selection?
    for (SelectedObjectMap::iterator i = oldSelectedObjects.begin(); i != oldObjectsEnd; ++i) {
        RenderObject* obj = i->first;
        SelectionInfo* newInfo = newSelectedObjects.get(obj);
        SelectionInfo* oldInfo = i->second;
        if (!newInfo || oldInfo->rect() != newInfo->rect() || oldInfo->state() != newInfo->state() ||
            (m_selectionStart == obj && oldStartPos != m_selectionStartPos) ||
            (m_selectionEnd == obj && oldEndPos != m_selectionEndPos)) {
            m_view->updateContents(oldInfo->rect());
            if (newInfo) {
                m_view->updateContents(newInfo->rect());
                newSelectedObjects.remove(obj);
                delete newInfo;
            }
        }
        delete oldInfo;
    }
    
    // Any new objects that remain were not found in the old objects dict, and so they need to be updated.
    SelectedObjectMap::iterator newObjectsEnd = newSelectedObjects.end();
    for (SelectedObjectMap::iterator i = newSelectedObjects.begin(); i != newObjectsEnd; ++i) {
        SelectionInfo* newInfo = i->second;
        m_view->updateContents(newInfo->rect());
        delete newInfo;
    }

    // Have any of the old blocks changed?
    SelectedBlockMap::iterator oldBlocksEnd = oldSelectedBlocks.end();
    for (SelectedBlockMap::iterator i = oldSelectedBlocks.begin(); i != oldBlocksEnd; ++i) {
        RenderBlock* block = i->first;
        BlockSelectionInfo* newInfo = newSelectedBlocks.get(block);
        BlockSelectionInfo* oldInfo = i->second;
        if (!newInfo || oldInfo->rects() != newInfo->rects() || oldInfo->state() != newInfo->state()) {
            m_view->updateContents(oldInfo->rects());
            if (newInfo) {
                m_view->updateContents(newInfo->rects());
                newSelectedBlocks.remove(block);
                delete newInfo;
            }
        }
        delete oldInfo;
    }
    
    // Any new blocks that remain were not found in the old blocks dict, and so they need to be updated.
    SelectedBlockMap::iterator newBlocksEnd = newSelectedBlocks.end();
    for (SelectedBlockMap::iterator i = newSelectedBlocks.begin(); i != newBlocksEnd; ++i) {
        BlockSelectionInfo* newInfo = i->second;
        m_view->updateContents(newInfo->rects());
        delete newInfo;
    }
}

void RenderCanvas::clearSelection()
{
    setSelection(0, -1, 0, -1);
}

void RenderCanvas::selectionStartEnd(int& spos, int& epos)
{
    spos = m_selectionStartPos;
    epos = m_selectionEndPos;
}

void RenderCanvas::updateWidgetPositions()
{
    RenderObjectSet::iterator end = m_widgets.end();
    for (RenderObjectSet::iterator it = m_widgets.begin(); it != end; ++it) {
        (*it)->updateWidgetPosition();
    }
}

void RenderCanvas::addWidget(RenderObject *o)
{
    m_widgets.add(o);
}

void RenderCanvas::removeWidget(RenderObject *o)
{
    m_widgets.remove(o);
}



IntRect RenderCanvas::viewRect() const
{
    if (m_printingMode)
        return IntRect(0, 0, m_width, m_height);
    if (m_view)
        return IntRect(m_view->contentsX(),
            m_view->contentsY(),
            m_view->visibleWidth(),
            m_view->visibleHeight());
    return IntRect();
}

int RenderCanvas::docHeight() const
{
    int h;
    if (m_printingMode || !m_view)
        h = m_height;
    else
        h = m_view->visibleHeight();

    int lowestPos = lowestPosition();
    if( lowestPos > h )
        h = lowestPos;

    // FIXME: This doesn't do any margin collapsing.
    // Instead of this dh computation we should keep the result
    // when we call RenderBlock::layout.
    int dh = 0;
    for (RenderObject *c = firstChild(); c; c = c->nextSibling()) {
        dh += c->height() + c->marginTop() + c->marginBottom();
    }
    if( dh > h )
        h = dh;

    return h;
}

int RenderCanvas::docWidth() const
{
    int w;
    if (m_printingMode || !m_view)
        w = m_width;
    else
        w = m_view->visibleWidth();

    int rightmostPos = rightmostPosition();
    if( rightmostPos > w )
        w = rightmostPos;

    for (RenderObject *c = firstChild(); c; c = c->nextSibling()) {
        int dw = c->width() + c->marginLeft() + c->marginRight();
        if( dw > w )
            w = dw;
    }
    return w;
}

// The idea here is to take into account what object is moving the pagination point, and
// thus choose the best place to chop it.
void RenderCanvas::setBestTruncatedAt(int y, RenderObject *forRenderer, bool forcedBreak)
{
    // Nobody else can set a page break once we have a forced break.
    if (m_forcedPageBreak) return;
    
    // Forced breaks always win over unforced breaks.
    if (forcedBreak) {
        m_forcedPageBreak = true;
        m_bestTruncatedAt = y;
        return;
    }
    
    // prefer the widest object who tries to move the pagination point
    int width = forRenderer->width();
    if (width > m_truncatorWidth) {
        m_truncatorWidth = width;
        m_bestTruncatedAt = y;
    }
}

}
