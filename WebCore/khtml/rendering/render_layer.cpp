/*
 * Copyright (C) 2003 Apple Computer, Inc.
 *
 * Portions are Copyright (C) 1998 Netscape Communications Corporation.
 *
 * Other contributors:
 *   Robert O'Callahan <roc+@cs.cmu.edu>
 *   David Baron <dbaron@fas.harvard.edu>
 *   Christian Biesinger <cbiesinger@web.de>
 *   Randall Jesup <rjesup@wgate.com>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *   Josh Soref <timeless@mac.com>
 *   Boris Zbarsky <bzbarsky@mit.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Alternatively, the contents of this file may be used under the terms
 * of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deletingthe provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.
 */

#include "render_layer.h"
#include <kdebug.h>
#include <assert.h>
#include "khtmlview.h"
#include "render_block.h"
#include "render_arena.h"
#include "xml/dom_docimpl.h"

using namespace DOM;
using namespace khtml;

QWidget* RenderLayer::gScrollBar = 0;

#ifndef NDEBUG
static bool inRenderLayerDetach;
static bool inRenderLayerElementDetach;
static bool inRenderZTreeNodeDetach;
#endif

void
RenderScrollMediator::slotValueChanged(int val)
{
    m_layer->updateScrollPositionFromScrollbars();
}

RenderLayer::RenderLayer(RenderObject* object)
: m_object( object ),
m_parent( 0 ),
m_previous( 0 ),
m_next( 0 ),
m_first( 0 ),
m_last( 0 ),
m_height( 0 ),
m_y( 0 ),
m_x( 0 ),
m_width( 0 ),
m_scrollX( 0 ),
m_scrollY( 0 ),
m_scrollWidth( 0 ),
m_scrollHeight( 0 ),
m_hBar( 0 ),
m_vBar( 0 ),
m_scrollMediator( 0 )
{
}

RenderLayer::~RenderLayer()
{
    // Child layers will be deleted by their corresponding render objects, so
    // our destructor doesn't have to do anything.
    m_parent = m_previous = m_next = m_first = m_last = 0;
    delete m_hBar;
    delete m_vBar;
    delete m_scrollMediator;
}

void RenderLayer::updateLayerPosition()
{
    // The canvas is sized to the docWidth/Height over in RenderCanvas::layout, so we
    // don't need to ever update our layer position here.
    if (renderer()->isCanvas())
        return;
    
    int x = m_object->xPos();
    int y = m_object->yPos();

    if (!m_object->isPositioned()) {
        // We must adjust our position by walking up the render tree looking for the
        // nearest enclosing object with a layer.
        RenderObject* curr = m_object->parent();
        while (curr && !curr->layer()) {
            x += curr->xPos();
            y += curr->yPos();
            curr = curr->parent();
        }
    }

    if (m_object->isRelPositioned())
        static_cast<RenderBox*>(m_object)->relativePositionOffset(x, y);

    // Subtract our parent's scroll offset.
    if (parent())
        parent()->subtractScrollOffset(x, y);
    
    setPos(x,y);

    setWidth(m_object->width());
    setHeight(m_object->height());

    if (!m_object->style()->hidesOverflow()) {
        if (m_object->overflowWidth() > m_object->width())
            setWidth(m_object->overflowWidth());
        if (m_object->overflowHeight() > m_object->height())
            setHeight(m_object->overflowHeight());
    }
}

RenderLayer*
RenderLayer::enclosingPositionedAncestor()
{
    RenderLayer* curr = parent();
    for ( ; curr && !curr->m_object->isCanvas() && !curr->m_object->isRoot() &&
         !curr->m_object->isPositioned() && !curr->m_object->isRelPositioned();
         curr = curr->parent());
         
    return curr;
}

void* RenderLayer::operator new(size_t sz, RenderArena* renderArena) throw()
{
    return renderArena->allocate(sz);
}

void RenderLayer::operator delete(void* ptr, size_t sz)
{
    assert(inRenderLayerDetach);
    
    // Stash size where detach can find it.
    *(size_t *)ptr = sz;
}

void RenderLayer::detach(RenderArena* renderArena)
{
#ifndef NDEBUG
    inRenderLayerDetach = true;
#endif
    delete this;
#ifndef NDEBUG
    inRenderLayerDetach = false;
#endif
    
    // Recover the size left there for us by operator delete and free the memory.
    renderArena->free(*(size_t *)this, this);
}

void RenderLayer::addChild(RenderLayer *child, RenderLayer* beforeChild)
{
    RenderLayer* prevSibling = beforeChild ? beforeChild->previousSibling() : lastChild();
    if (prevSibling) {
        child->setPreviousSibling(prevSibling);
        prevSibling->setNextSibling(child);
    }
    else
        setFirstChild(child);

    if (beforeChild) {
        beforeChild->setPreviousSibling(child);
        child->setNextSibling(beforeChild);
    }
    else
        setLastChild(child);
   
    child->setParent(this);
}

RenderLayer* RenderLayer::removeChild(RenderLayer* oldChild)
{
    // remove the child
    if (oldChild->previousSibling())
        oldChild->previousSibling()->setNextSibling(oldChild->nextSibling());
    if (oldChild->nextSibling())
        oldChild->nextSibling()->setPreviousSibling(oldChild->previousSibling());

    if (m_first == oldChild)
        m_first = oldChild->nextSibling();
    if (m_last == oldChild)
        m_last = oldChild->previousSibling();

    oldChild->setPreviousSibling(0);
    oldChild->setNextSibling(0);
    oldChild->setParent(0);
    
    return oldChild;
}

void RenderLayer::removeOnlyThisLayer()
{
    if (!m_parent)
        return;
    
    // Remove us from the parent.
    RenderLayer* parent = m_parent;
    RenderLayer* nextSib = nextSibling();
    parent->removeChild(this);
    
    // Now walk our kids and reattach them to our parent.
    RenderLayer* current = m_first;
    while (current) {
        RenderLayer* next = current->nextSibling();
        removeChild(current);
        parent->addChild(current, nextSib);
        current = next;
    }
    
    detach(renderer()->renderArena());
}

void RenderLayer::insertOnlyThisLayer()
{
    if (!m_parent && renderer()->parent()) {
        // We need to connect ourselves when our renderer() has a parent.
        // Find our enclosingLayer and add ourselves.
        RenderLayer* parentLayer = renderer()->parent()->enclosingLayer();
        if (parentLayer)
            parentLayer->addChild(this, 
                                  renderer()->parent()->findNextLayer(parentLayer, renderer()));
    }
    
    // Remove all descendant layers from the hierarchy and add them to the new position.
    for (RenderObject* curr = renderer()->firstChild(); curr; curr = curr->nextSibling())
        curr->moveLayers(m_parent, this);
}

void 
RenderLayer::convertToLayerCoords(RenderLayer* ancestorLayer, int& x, int& y)
{
    if (ancestorLayer == this)
        return;
        
    if (m_object->style()->position() == FIXED) {
        // Add in the offset of the view.  We can obtain this by calling
        // absolutePosition() on the RenderCanvas.
        int xOff, yOff;
        m_object->absolutePosition(xOff, yOff, true);
        x += xOff;
        y += yOff;
        return;
    }
 
    RenderLayer* parentLayer;
    if (m_object->style()->position() == ABSOLUTE)
        parentLayer = enclosingPositionedAncestor();
    else
        parentLayer = parent();
    
    if (!parentLayer) return;
    
    parentLayer->convertToLayerCoords(ancestorLayer, x, y);

    x += xPos();
    y += yPos();
}

void
RenderLayer::scrollOffset(int& x, int& y)
{
    x += scrollXOffset();
    y += scrollYOffset();
}

void
RenderLayer::subtractScrollOffset(int& x, int& y)
{
    x -= scrollXOffset();
    y -= scrollYOffset();
}

void
RenderLayer::scrollToOffset(int x, int y, bool updateScrollbars)
{
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    int maxX = m_scrollWidth - m_object->clientWidth();
    int maxY = m_scrollHeight - m_object->clientHeight();
    if (x > maxX) x = maxX;
    if (y > maxY) y = maxY;

    // FIXME: Eventually, we will want to perform a blit.  For now never
    // blit, since the check for blitting is going to be very
    // complicated (since it will involve testing whether our layer
    // is either occluded by another layer or clipped by an enclosing
    // layer or contains fixed backgrounds, etc.).
    m_scrollX = x;
    m_scrollY = y;

    // FIXME: Fire the onscroll DOM event.
    
    // Just schedule a full repaint of our object.
    m_object->repaint(true);
    
    if (updateScrollbars) {
        if (m_hBar)
            m_hBar->setValue(m_scrollX);
        if (m_vBar)
            m_vBar->setValue(m_scrollY);
    }
}

void
RenderLayer::updateScrollPositionFromScrollbars()
{
    bool needUpdate = false;
    int newX = m_scrollX;
    int newY = m_scrollY;
    
    if (m_hBar) {
        newX = m_hBar->value();
        if (newX != m_scrollX)
           needUpdate = true;
    }

    if (m_vBar) {
        newY = m_vBar->value();
        if (newY != m_scrollY)
           needUpdate = true;
    }

    if (needUpdate)
        scrollToOffset(newX, newY, false);
}

void
RenderLayer::setHasHorizontalScrollbar(bool hasScrollbar)
{
    if (hasScrollbar && !m_hBar) {
        QScrollView* scrollView = m_object->element()->getDocument()->view();
        m_hBar = new QScrollBar(Qt::Horizontal, scrollView);
        scrollView->addChild(m_hBar, 0, -50000);
        if (!m_scrollMediator)
            m_scrollMediator = new RenderScrollMediator(this);
        m_scrollMediator->connect(m_hBar, SIGNAL(valueChanged(int)), SLOT(slotValueChanged(int)));
    }
    else if (!hasScrollbar && m_hBar) {
        m_scrollMediator->disconnect(m_hBar, SIGNAL(valueChanged(int)),
                                     m_scrollMediator, SLOT(slotValueChanged(int)));
        delete m_hBar;
        m_hBar = 0;
    }
}

void
RenderLayer::setHasVerticalScrollbar(bool hasScrollbar)
{
    if (hasScrollbar && !m_vBar) {
        QScrollView* scrollView = m_object->element()->getDocument()->view();
        m_vBar = new QScrollBar(Qt::Vertical, scrollView);
        scrollView->addChild(m_vBar, 0, -50000);
        if (!m_scrollMediator)
            m_scrollMediator = new RenderScrollMediator(this);
        m_scrollMediator->connect(m_vBar, SIGNAL(valueChanged(int)), SLOT(slotValueChanged(int)));
    }
    else if (!hasScrollbar && m_vBar) {
        m_scrollMediator->disconnect(m_vBar, SIGNAL(valueChanged(int)),
                                     m_scrollMediator, SLOT(slotValueChanged(int)));
        delete m_vBar;
        m_vBar = 0;
    }
}

int
RenderLayer::verticalScrollbarWidth()
{
    if (!m_vBar)
        return 0;

    return m_vBar->width();
}

int
RenderLayer::horizontalScrollbarHeight()
{
    if (!m_hBar)
        return 0;

    return m_hBar->height();
}

void
RenderLayer::moveScrollbarsAside()
{
    if (m_hBar)
        m_hBar->move(0, -50000);
    if (m_vBar)
        m_vBar->move(0, -50000);
}

void
RenderLayer::positionScrollbars(const QRect& absBounds)
{
    if (m_vBar) {
        m_vBar->move(absBounds.x()+absBounds.width()-m_object->borderRight()-m_vBar->width(),
                     absBounds.y()+m_object->borderTop());
        m_vBar->resize(m_vBar->width(), absBounds.height() -
                       (m_object->borderTop()+m_object->borderBottom()) -
                       (m_hBar ? m_hBar->height()-1 : 0));
    }

    if (m_hBar) {
        m_hBar->move(absBounds.x()+m_object->borderLeft(),
                     absBounds.y()+absBounds.height()-m_object->borderBottom()-m_hBar->height());
        m_hBar->resize(absBounds.width() - (m_object->borderLeft()+m_object->borderRight()) -
                       (m_vBar ? m_vBar->width()-1 : 0),
                       m_hBar->height());
    }
}

#define LINE_STEP   10
#define PAGE_KEEP   40

void
RenderLayer::checkScrollbarsAfterLayout()
{
    updateLayerPosition();
    
    int rightPos = m_object->rightmostPosition();
    int bottomPos = m_object->lowestPosition();

    int clientWidth = m_object->clientWidth();
    int clientHeight = m_object->clientHeight();
    m_scrollWidth = clientWidth;
    m_scrollHeight = clientHeight;
    
    if (rightPos - m_object->borderLeft() > m_scrollWidth)
        m_scrollWidth = rightPos - m_object->borderLeft();
    if (bottomPos - m_object->borderTop() > m_scrollHeight)
        m_scrollHeight = bottomPos - m_object->borderTop();
    
    bool needHorizontalBar = rightPos > m_width;
    bool needVerticalBar = bottomPos > m_height;

    bool haveHorizontalBar = m_hBar;
    bool haveVerticalBar = m_vBar;

    // overflow:scroll should just enable/disable.
    if (m_object->style()->overflow() == OSCROLL) {
        m_hBar->setEnabled(needHorizontalBar);
        m_vBar->setEnabled(needVerticalBar);
    }

    // overflow:auto may need to lay out again if scrollbars got added/removed.
    bool scrollbarsChanged = (m_object->style()->overflow() == OAUTO) &&
        (haveHorizontalBar != needHorizontalBar || haveVerticalBar != needVerticalBar);    
    if (scrollbarsChanged) {
        setHasHorizontalScrollbar(needHorizontalBar);
        setHasVerticalScrollbar(needVerticalBar);
       
        m_object->setNeedsLayout(true);
	if (m_object->isRenderBlock())
            static_cast<RenderBlock*>(m_object)->layoutBlock(true);
        else
            m_object->layout();
	return;
    }

    // Set up the range (and page step/line step).
    if (m_hBar) {
        int pageStep = (clientWidth-PAGE_KEEP);
        if (pageStep < 0) pageStep = clientWidth;
        m_hBar->setSteps(LINE_STEP, pageStep);
#ifdef APPLE_CHANGES
        m_hBar->setKnobProportion(clientWidth, m_scrollWidth);
#else
        m_hBar->setRange(0, m_scrollWidth-clientWidth);
#endif
    }
    if (m_vBar) {
        int pageStep = (clientHeight-PAGE_KEEP);
        if (pageStep < 0) pageStep = clientHeight;
        m_vBar->setSteps(LINE_STEP, pageStep);
#ifdef APPLE_CHANGES
        m_vBar->setKnobProportion(clientHeight, m_scrollHeight);
#else
        m_vBar->setRange(0, m_scrollHeight-clientHeight);
#endif
    }
}

void
RenderLayer::paintScrollbars(QPainter* p, int x, int y, int w, int h)
{
#if APPLE_CHANGES
    if (m_hBar)
        m_hBar->paint(p, QRect(x, y, w, h));
    if (m_vBar)
        m_vBar->paint(p, QRect(x, y, w, h));
#endif
}

void
RenderLayer::paint(QPainter *p, int x, int y, int w, int h, bool selectionOnly)
{
    // Create the z-tree of layers that should be displayed.
    QRect damageRect(x,y,w,h);
    RenderZTreeNode* node = constructZTree(damageRect, damageRect, this);
    if (!node)
        return;

    // Flatten the tree into a back-to-front list for painting.
    QPtrVector<RenderLayerElement> layerList;
    constructLayerList(node, &layerList);

    // Walk the list and paint each layer, adding in the appropriate offset.
    QRect paintRect(x, y, w, h);
    QRect currRect(paintRect);
    
    uint count = layerList.count();
    for (uint i = 0; i < count; i++) {
        RenderLayerElement* elt = layerList.at(i);

        // Elements add in their own positions as a translation factor.  This forces
        // us to subtract that out, so that when it's added back in, we get the right
        // bounds.  This is really disgusting (that paint only sets up the right paint
        // position after you call into it). -dwh
        //printf("Painting layer at %d %d\n", elt->absBounds.x(), elt->absBounds.y());
    
        if (elt->clipOriginator) {
            // We originated a clip (we're either positioned or an element with
            // overflow: hidden).  We need to paint our background and border, subject
            // to clip regions established by our parent layers.
            if (elt->backgroundClipRect != currRect) {
                if (currRect != paintRect)
                    p->restore(); // Pop the clip.
                    
                currRect = elt->backgroundClipRect;
                
                // Now apply the clip rect.
                QRect clippedRect = p->xForm(currRect);
#if APPLE_CHANGES
                p->save();
                p->addClip(clippedRect);
#else
                QRegion creg(cr);
                QRegion old = p->clipRegion();
                if (!old.isNull())
                    creg = old.intersect(creg);
            
                p->save();
                p->setClipRegion(creg);
#endif
            }
            
            // A clip is in effect.  The clip is never allowed to clip our render object's
            // background, borders or scrollbars.  Go ahead and draw those now without our clip (that will
            // be used for our children) in effect.
            elt->layer->renderer()->paintBoxDecorations(p, x, y, w, h,
                                elt->absBounds.x(),
                                elt->absBounds.y());

            // Position our scrollbars prior to painting.
            elt->layer->positionScrollbars(elt->absBounds);

#if APPLE_CHANGES
            // Our scrollbar widgets paint exactly when we tell them to, so that they work properly with
            // z-index.
            elt->layer->paintScrollbars(p, x, y, w, h);
#endif
        }
        
        if (elt->clipRect != currRect) {
            if (currRect != paintRect)
                p->restore(); // Pop the clip.
            
            currRect = elt->clipRect;
            if (currRect != paintRect) {
                                
                // Now apply the clip rect.
                QRect clippedRect = p->xForm(currRect);
#if APPLE_CHANGES
                p->save();
                p->addClip(clippedRect);
#else
                QRegion creg(cr);
                QRegion old = p->clipRegion();
                if (!old.isNull())
                    creg = old.intersect(creg);
            
                p->save();
                p->setClipRegion(creg);
#endif
            }
        }

        if (currRect.isEmpty())
            continue;
        
        if (selectionOnly) {
            if (elt->layerElementType == RenderLayerElement::Normal ||
                elt->layerElementType == RenderLayerElement::Foreground)
                elt->layer->renderer()->paint(p, x, y, w, h,
                                            elt->absBounds.x() - elt->layer->renderer()->xPos(),
                                            elt->absBounds.y() - elt->layer->renderer()->yPos(),
                                            PaintActionSelection);
        } else {
            if (elt->layerElementType == RenderLayerElement::Normal ||
                elt->layerElementType == RenderLayerElement::Background)
                elt->layer->renderer()->paint(p, x, y, w, h,
                                            elt->absBounds.x() - elt->layer->renderer()->xPos(),
                                            elt->absBounds.y() - elt->layer->renderer()->yPos(),
                                            PaintActionElementBackground);

            if (elt->layerElementType == RenderLayerElement::Normal ||
                elt->layerElementType == RenderLayerElement::Foreground) {
                elt->layer->renderer()->paint(p, x, y, w, h,
                                              elt->absBounds.x() - elt->layer->renderer()->xPos(),
                                              elt->absBounds.y() - elt->layer->renderer()->yPos(),
                                              PaintActionChildBackgrounds);
                elt->layer->renderer()->paint(p, x, y, w, h,
                                            elt->absBounds.x() - elt->layer->renderer()->xPos(),
                                            elt->absBounds.y() - elt->layer->renderer()->yPos(),
                                            PaintActionFloat);
                elt->layer->renderer()->paint(p, x, y, w, h,
                                            elt->absBounds.x() - elt->layer->renderer()->xPos(),
                                            elt->absBounds.y() - elt->layer->renderer()->yPos(),
                                            PaintActionForeground);
            }
        }
    }
    
    if (currRect != paintRect)
        p->restore(); // Pop the clip.
        
    node->detach(renderer()->renderArena());
}

void
RenderLayer::clearOtherLayersHoverActiveState()
{
    if (!m_parent)
        return;
        
    for (RenderLayer* curr = m_parent->firstChild(); curr; curr = curr->nextSibling()) {
        if (curr == this)
            continue;
        curr->clearHoverAndActiveState(curr->renderer());
    }
    
    m_parent->clearOtherLayersHoverActiveState();
}

void
RenderLayer::clearHoverAndActiveState(RenderObject* obj)
{
    if (!obj->mouseInside())
        return;
    
    obj->setMouseInside(false);
    if (obj->element()) {
        obj->element()->setActive(false);
        if (obj->style()->affectedByHoverRules() || obj->style()->affectedByActiveRules())
            obj->element()->setChanged(true);
    }
    
    for (RenderObject* child = obj->firstChild(); child; child = child->nextSibling())
        if (child->mouseInside())
            clearHoverAndActiveState(child);
}

bool
RenderLayer::nodeAtPoint(RenderObject::NodeInfo& info, int x, int y)
{
    // Clear out our global scrollbar tracking variable.
    gScrollBar = 0;
    
    bool inside = false;
    RenderLayer* insideLayer = 0;
    QRect damageRect(m_x, m_y, m_width, m_height);
    RenderZTreeNode* node = constructZTree(damageRect, damageRect, this, true, x, y);
    if (!node)
        return false;

    // Flatten the tree into a back-to-front list for painting.
    QPtrVector<RenderLayerElement> layerList;
    constructLayerList(node, &layerList);

    // Walk the list and test each layer, adding in the appropriate offset.
    uint count = layerList.count();
    for (int i = count-1; i >= 0; i--) {
        RenderLayerElement* elt = layerList.at(i);
        
        // Elements add in their own positions as a translation factor.  This forces
        // us to subtract that out, so that when it's added back in, we get the right
        // bounds.  This is really disgusting (that paint only sets up the right paint
        // position after you call into it). -dwh
        //printf("Painting layer at %d %d\n", elt->absBounds.x(), elt->absBounds.y());

        inside = elt->layer->renderer()->nodeAtPoint(info, x, y,
                                      elt->absBounds.x() - elt->layer->renderer()->xPos(),
                                      elt->absBounds.y() - elt->layer->renderer()->yPos());
        if (inside) {
            // For foreground layer elements where the layer has been split into two, we
            // are only considered to be inside the foreground layer if we hit content other
            // than ourselves.
            if (elt->layerElementType == RenderLayerElement::Foreground &&
                info.innerNode() == elt->layer->renderer()->element()) {
                inside = false;
                info.setInnerNode(0);
                info.setInnerNonSharedNode(0);
                info.setURLElement(0);
                continue;
            }
            // Otherwise the mouse is inside this layer, and we can stop looking.
            insideLayer = elt->layer;
            break;
        }
    }
    node->detach(renderer()->renderArena());

    if (insideLayer) {
        // Clear out the other layers' hover/active state
        insideLayer->clearOtherLayersHoverActiveState();
    
        // Now clear out our descendant layers
        for (RenderLayer* child = insideLayer->firstChild();
             child; child = child->nextSibling())
            child->clearHoverAndActiveState(child->renderer());
    }
    
    return inside;
}

RenderLayer::RenderZTreeNode*
RenderLayer::constructZTree(QRect overflowClipRect, QRect posClipRect,
                            RenderLayer* rootLayer,
                            bool eventProcessing, int xMousePos, int yMousePos)
{
    // The arena we use for allocating our temporary ztree elements.
    RenderArena* renderArena = renderer()->renderArena();
    
    // This variable stores the result we will hand back.
    RenderZTreeNode* returnNode = 0;

    // FIXME: A child render object or layer could override visibility.  Don't remove this
    // optimization though until nodeAtPoint is patched as well.
    //
    // If a layer isn't visible, then none of its child layers are visible either.
    // Don't build this branch of the z-tree, since these layers should not be painted.
    if (renderer()->style()->visibility() != VISIBLE)
        return 0;
    
    // Compute this layer's absolute position, so that we can compare it with our
    // damage rect and avoid repainting the layer if it falls outside that rect.
    // An exception to this rule is the root layer, which always paints (hence the
    // m_parent null check below).
    updateLayerPosition(); // For relpositioned layers or non-positioned layers,
                            // we need to keep in sync, since we may have shifted relative
                            // to our parent layer.
                               
    int x = 0;
    int y = 0;
    convertToLayerCoords(rootLayer, x, y);
    QRect layerBounds(x, y, width(), height());
     
    returnNode = new (renderArena) RenderZTreeNode(this);

    // Positioned elements are clipped according to the posClipRect.  All other
    // layers are clipped according to the overflowClipRect.
    QRect clipRectToApply = m_object->isPositioned() ? posClipRect : overflowClipRect;
    QRect damageRect = clipRectToApply.intersect(layerBounds);
    
    // Clip applies to *us* as well, so go ahead and update the damageRect.
    if (m_object->hasClip())
        damageRect = damageRect.intersect(m_object->getClipRect(x,y));
        
    // If we establish a clip rect, then we want to intersect that rect
    // with the damage rect to form a new damage rect.
    bool clipOriginator = false;
    
    // Update the clip rects that will be passed to children layers.
    if (m_object->hasOverflowClip() || m_object->hasClip()) {
        // This layer establishes a clip of some kind.
        clipOriginator = true;
        if (m_object->hasOverflowClip()) {
            QRect newOverflowClip = m_object->getOverflowClipRect(x,y);
            overflowClipRect  = newOverflowClip.intersect(overflowClipRect);
            clipRectToApply = clipRectToApply.intersect(newOverflowClip);
            if (m_object->isPositioned() || m_object->isRelPositioned())
                posClipRect = newOverflowClip.intersect(posClipRect);
        }
        if (m_object->hasClip()) {
            QRect newPosClip = m_object->getClipRect(x,y);
            posClipRect = newPosClip.intersect(posClipRect);
            overflowClipRect = overflowClipRect.intersect(posClipRect);
            clipRectToApply = clipRectToApply.intersect(newPosClip);
        }
    }
    
    // Walk our list of child layers looking only for those layers that have a 
    // non-negative z-index (a z-index >= 0).
    RenderZTreeNode* lastChildNode = 0;
    for (RenderLayer* child = firstChild(); child; child = child->nextSibling()) {
        if (child->zIndex() < 0)
            continue; // Ignore negative z-indices in this first pass.

        RenderZTreeNode* childNode = child->constructZTree(overflowClipRect, posClipRect, 
                                                           rootLayer, eventProcessing, 
                                                           xMousePos, yMousePos);
        if (childNode) {
            // Put the new node into the tree at the front of the parent's list.
            if (lastChildNode)
                lastChildNode->next = childNode;
            else
                returnNode->child = childNode;
            lastChildNode = childNode;
        }
    }

    // Now add a leaf node for ourselves, but only if we intersect the damage
    // rect.  This intersection test is valid only for replaced elements or
    // block elements, since inline non-replaced elements have a width of 0 (and
    // thus the layer does too).  We also exclude the root from this test, since
    // the HTML can be much taller than the root (because of scrolling).
    if (renderer()->isCanvas() || renderer()->isRoot() || renderer()->isBody() ||
        renderer()->hasOverhangingFloats() || 
        (renderer()->isInline() && !renderer()->isReplaced()) ||
        (eventProcessing && damageRect.contains(xMousePos,yMousePos)) ||
        (!eventProcessing && layerBounds.intersects(damageRect))) {
        RenderLayerElement* layerElt = new (renderArena) RenderLayerElement(this, layerBounds, 
                                                              damageRect, clipRectToApply,
                                                              clipOriginator, x, y);
        if (returnNode->child) {
            RenderZTreeNode* leaf = new (renderArena) RenderZTreeNode(layerElt);
            leaf->next = returnNode->child;
            returnNode->child = leaf;
            
            // We are an interior node and have other child layers.  Our layer
            // will need to be sorted with the other layers as though it has
            // a z-index of 0.
            if (!layerElt->zauto)
                layerElt->zindex = 0;
        }
        else
            returnNode->layerElement = layerElt;
    } 
    
    // Now look for children that have a negative z-index.
    for (RenderLayer* child = firstChild(); child; child = child->nextSibling()) {
        if (child->zIndex() >= 0)
            continue; // Ignore non-negative z-indices in this second pass.

        RenderZTreeNode* childNode = child->constructZTree(overflowClipRect, posClipRect,
                                                           rootLayer, eventProcessing,
                                                           xMousePos, yMousePos);
        if (childNode) {
            // Deal with the case where all our children views had negative z-indices.
            // Demote our leaf node and make a new interior node that can hold these
            // children.
            if (returnNode->layerElement) {
                RenderZTreeNode* leaf = returnNode;
                returnNode = new (renderArena) RenderZTreeNode(this);
                returnNode->child = leaf;
            }
            
            // Put the new node into the tree at the front of the parent's list.
            childNode->next = returnNode->child;
            returnNode->child = childNode;
        }
    }
    
    return returnNode;
}

void
RenderLayer::constructLayerList(RenderZTreeNode* ztree, QPtrVector<RenderLayerElement>* result)
{
    // This merge buffer is just a temporary used during computation as we do merge sorting.
    QPtrVector<RenderLayerElement> mergeBuffer;
    ztree->constructLayerList(&mergeBuffer, result);
}

// Sort the buffer from lowest z-index to highest.  The common scenario will have
// most z-indices equal, so we optimize for that case (i.e., the list will be mostly
// sorted already).
static void sortByZOrder(QPtrVector<RenderLayer::RenderLayerElement>* buffer,
                         QPtrVector<RenderLayer::RenderLayerElement>* mergeBuffer,
                         uint start,
                         uint end)
{
    if (start >= end)
        return; // Sanity check.

    if (end - start <= 6) {
        // Apply a bubble sort for smaller lists.
        for (uint i = end-1; i > start; i--) {
            bool sorted = true;
            for (uint j = start; j < i; j++) {
                RenderLayer::RenderLayerElement* elt = buffer->at(j);
                RenderLayer::RenderLayerElement* elt2 = buffer->at(j+1);
                if (elt->zindex > elt2->zindex) {
                    sorted = false;
                    buffer->insert(j, elt2);
                    buffer->insert(j+1, elt);
                }
            }
            if (sorted)
                return;
        }
    }
    else {
        // Peform a merge sort for larger lists.
        uint mid = (start+end)/2;
        sortByZOrder(buffer, mergeBuffer, start, mid);
        sortByZOrder(buffer, mergeBuffer, mid, end);

        RenderLayer::RenderLayerElement* elt = buffer->at(mid-1);
        RenderLayer::RenderLayerElement* elt2 = buffer->at(mid);

        // Handle the fast common case (of equal z-indices).  The list may already
        // be completely sorted.
        if (elt->zindex <= elt2->zindex)
            return;

        // We have to merge sort.  Ensure our merge buffer is big enough to hold
        // all the items.
        mergeBuffer->resize(end - start);
        uint i1 = start;
        uint i2 = mid;

        elt = buffer->at(i1);
        elt2 = buffer->at(i2);

        while (i1 < mid || i2 < end) {
            if (i1 < mid && (i2 == end || elt->zindex <= elt2->zindex)) {
                mergeBuffer->insert(mergeBuffer->count(), elt);
                i1++;
                if (i1 < mid)
                    elt = buffer->at(i1);
            }
            else {
                mergeBuffer->insert(mergeBuffer->count(), elt2);
                i2++;
                if (i2 < end)
                    elt2 = buffer->at(i2);
            }
        }

        for (uint i = start; i < end; i++)
            buffer->insert(i, mergeBuffer->at(i-start));

        mergeBuffer->clear();
    }
}

void RenderLayer::RenderZTreeNode::constructLayerList(QPtrVector<RenderLayerElement>* mergeTmpBuffer,
                                                      QPtrVector<RenderLayerElement>* buffer)
{
    // The root always establishes a stacking context.  We could add a rule for this
    // to the UA sheet, but this code guarantees that nobody can do anything wacky
    // in CSS to prevent the root from establishing a stacking context.
    bool autoZIndex = layer->parent() ? layer->hasAutoZIndex() : false;
    int explicitZIndex = layer->zIndex();

    if (layerElement) {
        // We are a leaf node of the ztree, and so we just place our layer element into
        // the buffer.
        if (buffer->count() == buffer->size())
            // Resize by a power of 2.
            buffer->resize(2*(buffer->size()+1));
        
        buffer->insert(buffer->count(), layerElement);
        return;
    }

    uint startIndex = buffer->count();
    for (RenderZTreeNode* current = child; current; current = current->next)
        current->constructLayerList(mergeTmpBuffer, buffer);
    uint endIndex = buffer->count();

    if (autoZIndex || !(endIndex-startIndex))
        return; // We just had to collect the kids.  We don't apply a sort to them, since
                // they will actually be layered in some ancestor layer's stacking context.
    
    sortByZOrder(buffer, mergeTmpBuffer, startIndex, endIndex);

    // Find out if we have any elements with negative z-indices in this stacking context.
    // If so, then we need to split our layer in two (a background layer and a foreground
    // layer).  We then put the background layer before the negative z-index objects, and
    // leave the foreground layer in the position previously occupied by the unsplit original.
    RenderLayerElement* elt = buffer->at(startIndex);
    if (elt->zindex < 0) {
        // Locate our layer in the layer list.
        for (uint i = startIndex; i < endIndex; i++) {
            elt = buffer->at(i);
            if (elt->layer == layer) {
                // Clone the layer element.
                RenderLayerElement* bgLayer =
                  new (layer->renderer()->renderArena()) RenderLayerElement(*elt);

                // Set the layer types (foreground and background) on the two layer elements.
                elt->layerElementType = RenderLayerElement::Foreground;
                bgLayer->layerElementType = RenderLayerElement::Background;

                // Ensure our buffer is big enough to hold a new layer element.
                if (buffer->count() == buffer->size())
                    // Resize by a power of 2.
                    buffer->resize(2*(buffer->size()+1));

                // Insert the background layer element at the front of our sorted list.
                for (uint j = buffer->count(); j > startIndex; j--)
                    buffer->insert(j, buffer->at(j-1));
                buffer->insert(startIndex, bgLayer);

                // Augment endIndex since we added a layer element.
                endIndex++;
                break;
            }
        }
    }
    
    // Now set all of the elements' z-indices to match the parent's explicit z-index, so that
    // they will be layered properly in the ancestor layer's stacking context.
    for (uint i = startIndex; i < endIndex; i++) {
        elt = buffer->at(i);
        elt->zindex = explicitZIndex;
    }
}

void* RenderLayer::RenderLayerElement::operator new(size_t sz, RenderArena* renderArena) throw()
{
    void* result = renderArena->allocate(sz);
    if (result)
        memset(result, 0, sz);
    return result;
}

void RenderLayer::RenderLayerElement::operator delete(void* ptr, size_t sz)
{
    assert(inRenderLayerElementDetach);
    
    // Stash size where detach can find it.
    *(size_t *)ptr = sz;
}

void RenderLayer::RenderLayerElement::detach(RenderArena* renderArena)
{
#ifndef NDEBUG
    inRenderLayerElementDetach = true;
#endif
    delete this;
#ifndef NDEBUG
    inRenderLayerElementDetach = false;
#endif
    
    // Recover the size left there for us by operator delete and free the memory.
    renderArena->free(*(size_t *)this, this);
}

void* RenderLayer::RenderZTreeNode::operator new(size_t sz, RenderArena* renderArena) throw()
{
    void* result = renderArena->allocate(sz);
    if (result)
        memset(result, 0, sz);
    return result;
}

void RenderLayer::RenderZTreeNode::operator delete(void* ptr, size_t sz)
{
    assert(inRenderZTreeNodeDetach);
    
    // Stash size where detach can find it.
    *(size_t *)ptr = sz;
}

void RenderLayer::RenderZTreeNode::detach(RenderArena* renderArena)
{
    assert(!next);
    
    RenderZTreeNode *n;
    for (RenderZTreeNode *c = child; c; c = n) {
        n = c->next;
        c->next = 0;
        c->detach(renderArena);
    }
    if (layerElement)
        layerElement->detach(renderArena);

#ifndef NDEBUG
    inRenderZTreeNodeDetach = true;
#endif
    delete this;
#ifndef NDEBUG
    inRenderZTreeNodeDetach = false;
#endif
    
    // Recover the size left there for us by operator delete and free the memory.
    renderArena->free(*(size_t *)this, this);
}

QPtrVector<RenderLayer::RenderLayerElement> RenderLayer::elementList(RenderZTreeNode *&node)
{
    QPtrVector<RenderLayerElement> list;
    
    QRect damageRect(m_x, m_y, m_width, m_height);
    node = constructZTree(damageRect, damageRect, this);
    if (node) {
        constructLayerList(node, &list);
    }
    
    return list;
}
