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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "RenderView.h"

#include "Document.h"
#include "Element.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "RenderLayer.h"

namespace WebCore {

RenderView::RenderView(Node* node, FrameView* view)
    : RenderBlock(node)
    , m_frameView(view)
    , m_selectionStart(0)
    , m_selectionEnd(0)
    , m_selectionStartPos(-1)
    , m_selectionEndPos(-1)
    , m_printImages(true)
    , m_maximalOutlineSize(0)
    , m_layoutState(0)
    , m_layoutStateDisableCount(0)
{
    // Clear our anonymous bit, set because RenderObject assumes
    // any renderer with document as the node is anonymous.
    setIsAnonymous(false);

    // init RenderObject attributes
    setInline(false);

    // try to contrain the width to the views width
    m_width = 0;
    m_height = 0;
    m_minPrefWidth = 0;
    m_maxPrefWidth = 0;

    setPrefWidthsDirty(true, false);
    
    setPositioned(true); // to 0,0 :)

    // Create a new root layer for our layer hierarchy.
    m_layer = new (node->document()->renderArena()) RenderLayer(this);
    setHasLayer(true);
}

RenderView::~RenderView()
{
}

void RenderView::calcHeight()
{
    if (!printing() && m_frameView)
        m_height = m_frameView->visibleHeight();
}

void RenderView::calcWidth()
{
    if (!printing() && m_frameView)
        m_width = m_frameView->visibleWidth();
    m_marginLeft = 0;
    m_marginRight = 0;
}

void RenderView::calcPrefWidths()
{
    ASSERT(prefWidthsDirty());

    RenderBlock::calcPrefWidths();

    m_maxPrefWidth = m_minPrefWidth;
}

void RenderView::layout()
{
    if (printing())
        m_minPrefWidth = m_maxPrefWidth = m_width;

    bool relayoutChildren = !printing() && (!m_frameView || m_width != m_frameView->visibleWidth() || m_height != m_frameView->visibleHeight());
    if (relayoutChildren)
        setChildNeedsLayout(true, false);
    
    ASSERT(!m_layoutState);
    LayoutState state;
    // FIXME: May be better to push a clip and avoid issuing offscreen repaints.
    state.m_clipped = false;
    m_layoutState = &state;

    if (needsLayout())
        RenderBlock::layout();

    // Ensure that docWidth() >= width() and docHeight() >= height().
    setOverflowWidth(m_width);
    setOverflowHeight(m_height);

    setOverflowWidth(docWidth());
    setOverflowHeight(docHeight());

    ASSERT(m_layoutStateDisableCount == 0);
    ASSERT(m_layoutState == &state);
    m_layoutState = 0;
    setNeedsLayout(false);
}

bool RenderView::absolutePosition(int& xPos, int& yPos, bool fixed) const
{
    if (fixed && m_frameView) {
        xPos = m_frameView->contentsX();
        yPos = m_frameView->contentsY();
    } else
        xPos = yPos = 0;
    return true;
}

void RenderView::paint(PaintInfo& paintInfo, int tx, int ty)
{
    // If we ever require layout but receive a paint anyway, something has gone horribly wrong.
    ASSERT(!needsLayout());

    // Cache the print rect because the dirty rect could get changed during painting.
    if (printing())
        setPrintRect(paintInfo.rect);
    else
        setPrintRect(IntRect());
    paintObject(paintInfo, tx, ty);
}

void RenderView::paintBoxDecorations(PaintInfo& paintInfo, int tx, int ty)
{
    // Check to see if we are enclosed by a transparent layer.  If so, we cannot blit
    // when scrolling, and we need to use slow repaints.
    Element* elt = document()->ownerElement();
    if (view() && elt && elt->renderer()) {
        RenderLayer* layer = elt->renderer()->enclosingLayer();
        if (layer->isTransparent() || layer->transparentAncestor())
            frameView()->setUseSlowRepaints();
    }

    if (elt || (firstChild() && firstChild()->style()->visibility() == VISIBLE) || !view())
        return;

    // This code typically only executes if the root element's visibility has been set to hidden.
    // Only fill with the base background color (typically white) if we're the root document, 
    // since iframes/frames with no background in the child document should show the parent's background.
    if (view()->isTransparent())
        frameView()->setUseSlowRepaints(); // The parent must show behind the child.
    else {
        Color baseColor = frameView()->baseBackgroundColor();
        if (baseColor.alpha() > 0) {
            paintInfo.context->save();
            paintInfo.context->setCompositeOperation(CompositeCopy);
            paintInfo.context->fillRect(paintInfo.rect, baseColor);
            paintInfo.context->restore();
        } else
            paintInfo.context->clearRect(paintInfo.rect);
    }
}

void RenderView::repaintViewRectangle(const IntRect& ur, bool immediate)
{
    if (printing() || ur.width() == 0 || ur.height() == 0)
        return;

    if (!m_frameView)
        return;

    // We always just invalidate the root view, since we could be an iframe that is clipped out
    // or even invisible.
    Element* elt = document()->ownerElement();
    if (!elt)
        m_frameView->repaintRectangle(ur, immediate);
    else if (RenderObject* obj = elt->renderer()) {
        IntRect vr = viewRect();
        IntRect r = intersection(ur, vr);
        
        // Subtract out the contentsX and contentsY offsets to get our coords within the viewing
        // rectangle.
        r.move(-vr.x(), -vr.y());
        
        // FIXME: Hardcoded offsets here are not good.
        r.move(obj->borderLeft() + obj->paddingLeft(),
               obj->borderTop() + obj->paddingTop());
        obj->repaintRectangle(r, immediate);
    }
}

void RenderView::computeAbsoluteRepaintRect(IntRect& rect, bool fixed)
{
    if (printing())
        return;

    if (fixed && m_frameView)
        rect.move(m_frameView->contentsX(), m_frameView->contentsY());
}

void RenderView::absoluteRects(Vector<IntRect>& rects, int tx, int ty, bool)
{
    rects.append(IntRect(tx, ty, m_layer->width(), m_layer->height()));
}

RenderObject* rendererAfterPosition(RenderObject* object, unsigned offset)
{
    if (!object)
        return 0;

    RenderObject* child = object->childAt(offset);
    return child ? child : object->nextInPreOrderAfterChildren();
}

IntRect RenderView::selectionRect(bool clipToVisibleContent) const
{
    document()->updateRendering();

    typedef HashMap<RenderObject*, SelectionInfo*> SelectionMap;
    SelectionMap selectedObjects;

    RenderObject* os = m_selectionStart;
    RenderObject* stop = rendererAfterPosition(m_selectionEnd, m_selectionEndPos);
    while (os && os != stop) {
        if ((os->canBeSelectionLeaf() || os == m_selectionStart || os == m_selectionEnd) && os->selectionState() != SelectionNone) {
            // Blocks are responsible for painting line gaps and margin gaps. They must be examined as well.
            selectedObjects.set(os, new SelectionInfo(os, clipToVisibleContent));
            RenderBlock* cb = os->containingBlock();
            while (cb && !cb->isRenderView()) {
                SelectionInfo* blockInfo = selectedObjects.get(cb);
                if (blockInfo)
                    break;
                selectedObjects.set(cb, new SelectionInfo(cb, clipToVisibleContent));
                cb = cb->containingBlock();
            }
        }

        os = os->nextInPreOrder();
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

void RenderView::setSelection(RenderObject* start, int startPos, RenderObject* end, int endPos)
{
    // Make sure both our start and end objects are defined.
    // Check www.msnbc.com and try clicking around to find the case where this happened.
    if ((start && !end) || (end && !start))
        return;

    // Just return if the selection hasn't changed.
    if (m_selectionStart == start && m_selectionStartPos == startPos &&
        m_selectionEnd == end && m_selectionEndPos == endPos)
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
    RenderObject* stop = rendererAfterPosition(m_selectionEnd, m_selectionEndPos);
    while (os && os != stop) {
        if ((os->canBeSelectionLeaf() || os == m_selectionStart || os == m_selectionEnd) && os->selectionState() != SelectionNone) {
            // Blocks are responsible for painting line gaps and margin gaps.  They must be examined as well.
            oldSelectedObjects.set(os, new SelectionInfo(os, true));
            RenderBlock* cb = os->containingBlock();
            while (cb && !cb->isRenderView()) {
                BlockSelectionInfo* blockInfo = oldSelectedBlocks.get(cb);
                if (blockInfo)
                    break;
                oldSelectedBlocks.set(cb, new BlockSelectionInfo(cb));
                cb = cb->containingBlock();
            }
        }

        os = os->nextInPreOrder();
    }

    // Now clear the selection.
    SelectedObjectMap::iterator oldObjectsEnd = oldSelectedObjects.end();
    for (SelectedObjectMap::iterator i = oldSelectedObjects.begin(); i != oldObjectsEnd; ++i)
        i->first->setSelectionState(SelectionNone);

    // set selection start and end
    m_selectionStart = start;
    m_selectionStartPos = startPos;
    m_selectionEnd = end;
    m_selectionEndPos = endPos;

    // Update the selection status of all objects between m_selectionStart and m_selectionEnd
    if (start && start == end)
        start->setSelectionState(SelectionBoth);
    else {
        if (start)
            start->setSelectionState(SelectionStart);
        if (end)
            end->setSelectionState(SelectionEnd);
    }

    RenderObject* o = start;
    stop = rendererAfterPosition(end, endPos);

    while (o && o != stop) {
        if (o != start && o != end && o->canBeSelectionLeaf())
            o->setSelectionState(SelectionInside);
        o = o->nextInPreOrder();
    }

    // Now that the selection state has been updated for the new objects, walk them again and
    // put them in the new objects list.
    o = start;
    while (o && o != stop) {
        if ((o->canBeSelectionLeaf() || o == start || o == end) && o->selectionState() != SelectionNone) {
            newSelectedObjects.set(o, new SelectionInfo(o, true));
            RenderBlock* cb = o->containingBlock();
            while (cb && !cb->isRenderView()) {
                BlockSelectionInfo* blockInfo = newSelectedBlocks.get(cb);
                if (blockInfo)
                    break;
                newSelectedBlocks.set(cb, new BlockSelectionInfo(cb));
                cb = cb->containingBlock();
            }
        }

        o = o->nextInPreOrder();
    }

    if (!m_frameView) {
        // We built the maps, but we aren't going to use them.
        // We need to delete the values, otherwise they'll all leak!
        deleteAllValues(oldSelectedObjects);
        deleteAllValues(newSelectedObjects);
        deleteAllValues(oldSelectedBlocks);
        deleteAllValues(newSelectedBlocks);
        return;
    }

    // Have any of the old selected objects changed compared to the new selection?
    for (SelectedObjectMap::iterator i = oldSelectedObjects.begin(); i != oldObjectsEnd; ++i) {
        RenderObject* obj = i->first;
        SelectionInfo* newInfo = newSelectedObjects.get(obj);
        SelectionInfo* oldInfo = i->second;
        if (!newInfo || oldInfo->rect() != newInfo->rect() || oldInfo->state() != newInfo->state() ||
            (m_selectionStart == obj && oldStartPos != m_selectionStartPos) ||
            (m_selectionEnd == obj && oldEndPos != m_selectionEndPos)) {
            m_frameView->updateContents(oldInfo->rect());
            if (newInfo) {
                m_frameView->updateContents(newInfo->rect());
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
        m_frameView->updateContents(newInfo->rect());
        delete newInfo;
    }

    // Have any of the old blocks changed?
    SelectedBlockMap::iterator oldBlocksEnd = oldSelectedBlocks.end();
    for (SelectedBlockMap::iterator i = oldSelectedBlocks.begin(); i != oldBlocksEnd; ++i) {
        RenderBlock* block = i->first;
        BlockSelectionInfo* newInfo = newSelectedBlocks.get(block);
        BlockSelectionInfo* oldInfo = i->second;
        if (!newInfo || oldInfo->rects() != newInfo->rects() || oldInfo->state() != newInfo->state()) {
            m_frameView->updateContents(oldInfo->rects());
            if (newInfo) {
                m_frameView->updateContents(newInfo->rects());
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
        m_frameView->updateContents(newInfo->rects());
        delete newInfo;
    }
}

void RenderView::clearSelection()
{
    setSelection(0, -1, 0, -1);
}

void RenderView::selectionStartEnd(int& startPos, int& endPos) const
{
    startPos = m_selectionStartPos;
    endPos = m_selectionEndPos;
}

bool RenderView::printing() const
{
    return document()->printing();
}

void RenderView::updateWidgetPositions()
{
    RenderObjectSet::iterator end = m_widgets.end();
    for (RenderObjectSet::iterator it = m_widgets.begin(); it != end; ++it)
        (*it)->updateWidgetPosition();
}

void RenderView::addWidget(RenderObject* o)
{
    m_widgets.add(o);
}

void RenderView::removeWidget(RenderObject* o)
{
    m_widgets.remove(o);
}

IntRect RenderView::viewRect() const
{
    if (printing())
        return IntRect(0, 0, m_width, m_height);
    if (m_frameView)
        return enclosingIntRect(m_frameView->visibleContentRect());
    return IntRect();
}

int RenderView::docHeight() const
{
    int h;
    if (printing() || !m_frameView)
        h = m_height;
    else
        h = m_frameView->visibleHeight();

    int lowestPos = lowestPosition();
    if (lowestPos > h)
        h = lowestPos;

    // FIXME: This doesn't do any margin collapsing.
    // Instead of this dh computation we should keep the result
    // when we call RenderBlock::layout.
    int dh = 0;
    for (RenderObject* c = firstChild(); c; c = c->nextSibling())
        dh += c->height() + c->marginTop() + c->marginBottom();

    if (dh > h)
        h = dh;

    return h;
}

int RenderView::docWidth() const
{
    int w;
    if (printing() || !m_frameView)
        w = m_width;
    else
        w = m_frameView->visibleWidth();

    int rightmostPos = rightmostPosition();
    if (rightmostPos > w)
        w = rightmostPos;

    for (RenderObject *c = firstChild(); c; c = c->nextSibling()) {
        int dw = c->width() + c->marginLeft() + c->marginRight();
        if (dw > w)
            w = dw;
    }

    return w;
}

// The idea here is to take into account what object is moving the pagination point, and
// thus choose the best place to chop it.
void RenderView::setBestTruncatedAt(int y, RenderObject* forRenderer, bool forcedBreak)
{
    // Nobody else can set a page break once we have a forced break.
    if (m_forcedPageBreak)
        return;

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

void RenderView::pushLayoutState(RenderObject* root)
{
    ASSERT(!m_frameView->needsFullRepaint());
    ASSERT(m_layoutStateDisableCount == 0);
    ASSERT(m_layoutState == 0);

    m_layoutState = new (renderArena()) LayoutState(root);
}

} // namespace WebCore
