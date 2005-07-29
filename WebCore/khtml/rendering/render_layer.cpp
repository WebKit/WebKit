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
#include "render_canvas.h"
#include "render_arena.h"
#include "render_theme.h"
#include "xml/dom_docimpl.h"
#include "xml/dom2_eventsimpl.h"
#include "html/html_blockimpl.h"

#include <qscrollbar.h>
#include <qptrvector.h>

#if APPLE_CHANGES
#include "KWQKHTMLPart.h" // For Dashboard.
#endif

// These match the numbers we use over in WebKit (WebFrameView.m).
#define LINE_STEP   40
#define PAGE_KEEP   40

using namespace DOM;
using namespace khtml;

#ifdef APPLE_CHANGES
QScrollBar* RenderLayer::gScrollBar = 0;
#endif

#ifndef NDEBUG
static bool inRenderLayerDetach;
#endif

void* ClipRects::operator new(size_t sz, RenderArena* renderArena) throw()
{
    return renderArena->allocate(sz);
}

void ClipRects::operator delete(void* ptr, size_t sz)
{
    // Stash size where detach can find it.
    *(size_t *)ptr = sz;
}

void ClipRects::detach(RenderArena* renderArena)
{
    delete this;
    
    // Recover the size left there for us by operator delete and free the memory.
    renderArena->free(*(size_t *)this, this);
}

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
m_relX( 0 ),
m_relY( 0 ),
m_x( 0 ),
m_y( 0 ),
m_width( 0 ),
m_height( 0 ),
m_scrollX( 0 ),
m_scrollY( 0 ),
m_scrollWidth( 0 ),
m_scrollHeight( 0 ),
m_hBar( 0 ),
m_vBar( 0 ),
m_scrollMediator( 0 ),
m_posZOrderList( 0 ),
m_negZOrderList( 0 ),
m_clipRects( 0 ) ,
m_scrollDimensionsDirty( true ),
m_zOrderListsDirty( true ),
m_usedTransparency( false ),
m_marquee( 0 )
{
}

RenderLayer::~RenderLayer()
{
    // Child layers will be deleted by their corresponding render objects, so
    // our destructor doesn't have to do anything.
    delete m_hBar;
    delete m_vBar;
    delete m_scrollMediator;
    delete m_posZOrderList;
    delete m_negZOrderList;
    delete m_marquee;
    
    // Make sure we have no lingering clip rects.
    assert(!m_clipRects);
}

void RenderLayer::computeRepaintRects()
{
    // FIXME: Child object could override visibility.
    if (m_object->style()->visibility() == VISIBLE)
        m_object->getAbsoluteRepaintRectIncludingFloats(m_repaintRect, m_fullRepaintRect);
    for	(RenderLayer* child = firstChild(); child; child = child->nextSibling())
        child->computeRepaintRects();
}

void RenderLayer::updateLayerPositions(bool doFullRepaint, bool checkForRepaint)
{
    if (doFullRepaint) {
        m_object->repaint();
        checkForRepaint = doFullRepaint = false;
    }
    
    updateLayerPosition(); // For relpositioned layers or non-positioned layers,
                           // we need to keep in sync, since we may have shifted relative
                           // to our parent layer.

    if (m_hBar || m_vBar) {
        // Need to position the scrollbars.
        int x = 0;
        int y = 0;
        convertToLayerCoords(root(), x, y);
        QRect layerBounds = QRect(x,y,width(),height());
        positionScrollbars(layerBounds);
    }

    // FIXME: Child object could override visibility.
    if (checkForRepaint && (m_object->style()->visibility() == VISIBLE))
        m_object->repaintAfterLayoutIfNeeded(m_repaintRect, m_fullRepaintRect);
    
    for	(RenderLayer* child = firstChild(); child; child = child->nextSibling())
        child->updateLayerPositions(doFullRepaint, checkForRepaint);
        
    // With all our children positioned, now update our marquee if we need to.
    if (m_marquee)
        m_marquee->updateMarqueePosition();
}

void RenderLayer::updateLayerPosition()
{
    // Clear our cached clip rect information.
    clearClipRect();

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

    m_relX = m_relY = 0;
    if (m_object->isRelPositioned()) {
        static_cast<RenderBox*>(m_object)->relativePositionOffset(m_relX, m_relY);
        x += m_relX; y += m_relY;
    }
    
    // Subtract our parent's scroll offset.
    if (m_object->isPositioned() && enclosingPositionedAncestor()) {
        RenderLayer* positionedParent = enclosingPositionedAncestor();

        // For positioned layers, we subtract out the enclosing positioned layer's scroll offset.
        positionedParent->subtractScrollOffset(x, y);
        
        if (m_object->isPositioned() && positionedParent->renderer()->isRelPositioned() &&
            positionedParent->renderer()->isInlineFlow()) {
            // When we have an enclosing relpositioned inline, we need to add in the offset of the first line
            // box from the rest of the content, but only in the cases where we know we're positioned
            // relative to the inline itself.
            RenderFlow* flow = static_cast<RenderFlow*>(positionedParent->renderer());
            int sx = 0, sy = 0;
            if (flow->firstLineBox()) {
                sx = flow->firstLineBox()->xPos();
                sy = flow->firstLineBox()->yPos();
            }
            else {
                sx = flow->staticX();
                sy = flow->staticY();
            }
            bool isInlineType = m_object->style()->isOriginalDisplayInlineType();
            
            if (!m_object->hasStaticX())
                x += sx;
            
            // This is not terribly intuitive, but we have to match other browsers.  Despite being a block display type inside
            // an inline, we still keep our x locked to the left of the relative positioned inline.  Arguably the correct
            // behavior would be to go flush left to the block that contains the inline, but that isn't what other browsers
            // do.
            if (m_object->hasStaticX() && !isInlineType)
                // Avoid adding in the left border/padding of the containing block twice.  Subtract it out.
                x += sx - (m_object->containingBlock()->borderLeft() + m_object->containingBlock()->paddingLeft());
            
            if (!m_object->hasStaticY())
                y += sy;
        }
    }
    else if (parent())
        parent()->subtractScrollOffset(x, y);
    
    setPos(x,y);

    setWidth(m_object->width());
    setHeight(m_object->height());

    if (!m_object->hasOverflowClip()) {
        if (m_object->overflowWidth() > m_object->width())
            setWidth(m_object->overflowWidth());
        if (m_object->overflowHeight() > m_object->height())
            setHeight(m_object->overflowHeight());
    }    
}

RenderLayer *RenderLayer::stackingContext() const
{
    RenderLayer* curr = parent();
    for ( ; curr && !curr->m_object->isCanvas() && !curr->m_object->isRoot() &&
          curr->m_object->style()->hasAutoZIndex();
          curr = curr->parent());
    return curr;
}

RenderLayer*
RenderLayer::enclosingPositionedAncestor() const
{
    RenderLayer* curr = parent();
    for ( ; curr && !curr->m_object->isCanvas() && !curr->m_object->isRoot() &&
         !curr->m_object->isPositioned() && !curr->m_object->isRelPositioned();
         curr = curr->parent());
         
    return curr;
}

#if APPLE_CHANGES
bool
RenderLayer::isTransparent()
{
    return m_object->style()->opacity() < 1.0f;
}

RenderLayer*
RenderLayer::transparentAncestor()
{
    RenderLayer* curr = parent();
    for ( ; curr && curr->m_object->style()->opacity() == 1.0f; curr = curr->parent());
    return curr;
}

void RenderLayer::beginTransparencyLayers(QPainter* p)
{
    if (isTransparent() && m_usedTransparency)
        return;
    
    RenderLayer* ancestor = transparentAncestor();
    if (ancestor)
        ancestor->beginTransparencyLayers(p);
    
    if (isTransparent()) {
        m_usedTransparency = true;
        p->beginTransparencyLayer(renderer()->style()->opacity());
    }
}

#endif

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

    // Dirty the z-order list in which we are contained.  The stackingContext() can be null in the
    // case where we're building up generated content layers.  This is ok, since the lists will start
    // off dirty in that case anyway.
    RenderLayer* stackingContext = child->stackingContext();
    if (stackingContext)
        stackingContext->dirtyZOrderLists();
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

    // Dirty the z-order list in which we are contained.  When called via the
    // reattachment process in removeOnlyThisLayer, the layer may already be disconnected
    // from the main layer tree, so we need to null-check the |stackingContext| value.
    RenderLayer* stackingContext = oldChild->stackingContext();
    if (stackingContext)
        oldChild->stackingContext()->dirtyZOrderLists();
    
    oldChild->setPreviousSibling(0);
    oldChild->setNextSibling(0);
    oldChild->setParent(0);
    
    return oldChild;
}

void RenderLayer::removeOnlyThisLayer()
{
    if (!m_parent)
        return;
    
    // Dirty the clip rects.
    clearClipRects();

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
        
    // Clear out all the clip rects.
    clearClipRects();
}

void 
RenderLayer::convertToLayerCoords(const RenderLayer* ancestorLayer, int& x, int& y) const
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
RenderLayer::scrollToOffset(int x, int y, bool updateScrollbars, bool repaint)
{
    if (renderer()->style()->overflow() != OMARQUEE) {
        if (x < 0) x = 0;
        if (y < 0) y = 0;
    
        // Call the scrollWidth/Height functions so that the dimensions will be computed if they need
        // to be (for overflow:hidden blocks).
        int maxX = scrollWidth() - m_object->clientWidth();
        int maxY = scrollHeight() - m_object->clientHeight();
        
        if (x > maxX) x = maxX;
        if (y > maxY) y = maxY;
    }
    
    // FIXME: Eventually, we will want to perform a blit.  For now never
    // blit, since the check for blitting is going to be very
    // complicated (since it will involve testing whether our layer
    // is either occluded by another layer or clipped by an enclosing
    // layer or contains fixed backgrounds, etc.).
    m_scrollX = x;
    m_scrollY = y;

    // Update the positions of our child layers.
    for (RenderLayer* child = firstChild(); child; child = child->nextSibling())
        child->updateLayerPositions(false, false);
    
#if APPLE_CHANGES
    // Move our widgets.
    m_object->updateWidgetPositions();
    
    // Update dashboard regions, scrolling may change the clip of a
    // particular region.
    RenderCanvas *canvas = renderer()->canvas();
    if (canvas)
        canvas->view()->updateDashboardRegions();
#endif

    // Fire the scroll DOM event.
    m_object->element()->dispatchHTMLEvent(EventImpl::SCROLL_EVENT, true, false);

    // Just schedule a full repaint of our object.
    if (repaint)
        m_object->repaint();
    
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
        m_hBar = new QScrollBar(Qt::Horizontal, 0);
        scrollView->addChild(m_hBar, 0, -50000);
        if (!m_scrollMediator)
            m_scrollMediator = new RenderScrollMediator(this);
        m_scrollMediator->connect(m_hBar, SIGNAL(valueChanged(int)), SLOT(slotValueChanged(int)));
    }
    else if (!hasScrollbar && m_hBar) {
        QScrollView* scrollView = m_object->element()->getDocument()->view();
        scrollView->removeChild (m_hBar);

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
        m_vBar = new QScrollBar(Qt::Vertical, 0);
        scrollView->addChild(m_vBar, 0, -50000);
        if (!m_scrollMediator)
            m_scrollMediator = new RenderScrollMediator(this);
        m_scrollMediator->connect(m_vBar, SIGNAL(valueChanged(int)), SLOT(slotValueChanged(int)));
    }
    else if (!hasScrollbar && m_vBar) {
        QScrollView* scrollView = m_object->element()->getDocument()->view();
        scrollView->removeChild (m_vBar);
	
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

int RenderLayer::scrollWidth()
{
    if (m_scrollDimensionsDirty)
        computeScrollDimensions();
    return m_scrollWidth;
}

int RenderLayer::scrollHeight()
{
    if (m_scrollDimensionsDirty)
        computeScrollDimensions();
    return m_scrollHeight;
}

void RenderLayer::computeScrollDimensions(bool* needHBar, bool* needVBar)
{
    m_scrollDimensionsDirty = false;

    int rightPos = m_object->rightmostPosition(true, false) - m_object->borderLeft();
    int bottomPos = m_object->lowestPosition(true, false) - m_object->borderTop();

    int clientWidth = m_object->clientWidth();
    int clientHeight = m_object->clientHeight();

    m_scrollWidth = kMax(rightPos, clientWidth);
    m_scrollHeight = kMax(bottomPos, clientHeight);

    if (needHBar)
        *needHBar = rightPos > clientWidth;
    if (needVBar)
        *needVBar = bottomPos > clientHeight;
}

void
RenderLayer::updateScrollInfoAfterLayout()
{
    m_scrollDimensionsDirty = true;
    if (m_object->style()->overflow() == OHIDDEN)
        return; // All we had to do was dirty.

    bool needHorizontalBar, needVerticalBar;
    computeScrollDimensions(&needHorizontalBar, &needVerticalBar);

    if (m_object->style()->overflow() != OMARQUEE) {
        // Layout may cause us to be in an invalid scroll position.  In this case we need
        // to pull our scroll offsets back to the max (or push them up to the min).
        int newX = kMax(0, kMin(m_scrollX, scrollWidth() - m_object->clientWidth()));
        int newY = kMax(0, kMin(m_scrollY, scrollHeight() - m_object->clientHeight()));
        if (newX != m_scrollX || newY != m_scrollY)
            scrollToOffset(newX, newY);
    }

    bool haveHorizontalBar = m_hBar;
    bool haveVerticalBar = m_vBar;

    // overflow:scroll should just enable/disable.
    if (m_object->style()->overflow() == OSCROLL) {
        m_hBar->setEnabled(needHorizontalBar);
        m_vBar->setEnabled(needVerticalBar);
    }

    // overflow:auto may need to lay out again if scrollbars got added/removed.
    bool scrollbarsChanged = (m_object->hasAutoScrollbars()) &&
        (haveHorizontalBar != needHorizontalBar || haveVerticalBar != needVerticalBar);    
    if (scrollbarsChanged) {
        setHasHorizontalScrollbar(needHorizontalBar);
        setHasVerticalScrollbar(needVerticalBar);
       
#if APPLE_CHANGES
	// Force an update since we know the scrollbars have changed things.
	if (m_object->document()->hasDashboardRegions())
	    m_object->document()->setDashboardRegionsDirty(true);
#endif

        m_object->repaint();

        if (m_object->style()->overflow() == OAUTO) {
            // Our proprietary overflow: overlay value doesn't trigger a layout.
            m_object->setNeedsLayout(true);
            if (m_object->isRenderBlock())
                static_cast<RenderBlock*>(m_object)->layoutBlock(true);
            else
                m_object->layout();
        }
    }

    // Set up the range (and page step/line step).
    if (m_hBar) {
        int clientWidth = m_object->clientWidth();
        int pageStep = (clientWidth-PAGE_KEEP);
        if (pageStep < 0) pageStep = clientWidth;
        m_hBar->setSteps(LINE_STEP, pageStep);
#ifdef APPLE_CHANGES
        m_hBar->setKnobProportion(clientWidth, m_scrollWidth);
#else
        m_hBar->setRange(0, m_scrollWidth-clientWidth);
        m_object->repaintRectangle(QRect(m_object->borderLeft(), m_object->borderTop() + clientHeight(),
                                   horizontalScrollbarHeight(),
                                   m_object->width() - m_object->borderLeft() - m_object->borderRight()));
#endif
    }
    if (m_vBar) {
        int clientHeight = m_object->clientHeight();
        int pageStep = (clientHeight-PAGE_KEEP);
        if (pageStep < 0) pageStep = clientHeight;
        m_vBar->setSteps(LINE_STEP, pageStep);
#ifdef APPLE_CHANGES
        m_vBar->setKnobProportion(clientHeight, m_scrollHeight);
#else
        m_vBar->setRange(0, m_scrollHeight-clientHeight);
#endif
        m_object->repaintRectangle(QRect(m_object->borderLeft() + m_object->clientWidth(),
                                   m_object->borderTop(), verticalScrollbarWidth(), 
                                   m_object->height() - m_object->borderTop() - m_object->borderBottom()));
    }
    
#if APPLE_CHANGES
    // Force an update since we know the scrollbars have changed things.
    if (m_object->document()->hasDashboardRegions())
	m_object->document()->setDashboardRegionsDirty(true);
#endif

    m_object->repaint();
}

#if APPLE_CHANGES
void
RenderLayer::paintScrollbars(QPainter* p, const QRect& damageRect)
{
    // Move the widgets if necessary.  We normally move and resize widgets during layout, but sometimes
    // widgets can move without layout occurring (most notably when you scroll a document that
    // contains fixed positioned elements).
    if (m_hBar || m_vBar) {
        int x = 0;
        int y = 0;
        convertToLayerCoords(root(), x, y);
        QRect layerBounds = QRect(x, y, width(), height());
        positionScrollbars(layerBounds);
    }

    // Now that we're sure the scrollbars are in the right place, paint them.
    if (m_hBar)
        m_hBar->paint(p, damageRect);
    if (m_vBar)
        m_vBar->paint(p, damageRect);
}
#endif

bool RenderLayer::scroll(KWQScrollDirection direction, KWQScrollGranularity granularity, float multiplier)
{
    bool didHorizontalScroll = false;
    bool didVerticalScroll = false;
    
    if (m_hBar != 0) {
        if (granularity == KWQScrollDocument) {
            // Special-case for the KWQScrollDocument granularity. A document scroll can only be up 
            // or down and in both cases the horizontal bar goes all the way to the left.
            didHorizontalScroll = m_hBar->scroll(KWQScrollLeft, KWQScrollDocument, multiplier);
        } else {
            didHorizontalScroll = m_hBar->scroll(direction, granularity, multiplier);
        }
    }
    if (m_vBar != 0) {
        didVerticalScroll = m_vBar->scroll(direction, granularity, multiplier);
    }

    return (didHorizontalScroll || didVerticalScroll);
}

void
RenderLayer::paint(QPainter *p, const QRect& damageRect, bool selectionOnly, RenderObject *paintingRoot)
{
    paintLayer(this, p, damageRect, false, selectionOnly, paintingRoot);
}

static void setClip(QPainter* p, const QRect& paintDirtyRect, const QRect& clipRect)
{
    if (paintDirtyRect == clipRect)
        return;

    p->save();
    
#if APPLE_CHANGES
    p->addClip(clipRect);
#else
    QRect clippedRect = p->xForm(clipRect);
    QRegion creg(clippedRect);
    QRegion old = p->clipRegion();
    if (!old.isNull())
        creg = old.intersect(creg);
    p->setClipRegion(creg);
#endif
    
}

static void restoreClip(QPainter* p, const QRect& paintDirtyRect, const QRect& clipRect)
{
    if (paintDirtyRect == clipRect)
        return;
    p->restore();
}

void
RenderLayer::paintLayer(RenderLayer* rootLayer, QPainter *p,
                        const QRect& paintDirtyRect, bool haveTransparency, bool selectionOnly,
                        RenderObject *paintingRoot)
{
    // Calculate the clip rects we should use.
    QRect layerBounds, damageRect, clipRectToApply;
    calculateRects(rootLayer, paintDirtyRect, layerBounds, damageRect, clipRectToApply);
    int x = layerBounds.x();
    int y = layerBounds.y();
                             
    // Ensure our z-order lists are up-to-date.
    updateZOrderLists();

#if APPLE_CHANGES
    if (isTransparent())
        haveTransparency = true;
#endif

    // If this layer's renderer is a child of the paintingRoot, we render unconditionally, which
    // is done by passing a nil paintingRoot down to our renderer (as if no paintingRoot was ever set).
    // Else, our renderer tree may or may not contain the painting root, so we pass that root along
    // so it will be tested against as we decend through the renderers.
    RenderObject *paintingRootForRenderer = 0;
    if (paintingRoot && !m_object->hasAncestor(paintingRoot)) {
        paintingRootForRenderer = paintingRoot;
    }
    
    // We want to paint our layer, but only if we intersect the damage rect.
    bool shouldPaint = intersectsDamageRect(layerBounds, damageRect);
    if (shouldPaint && !selectionOnly && !damageRect.isEmpty()) {
#if APPLE_CHANGES
        // Begin transparency layers lazily now that we know we have to paint something.
        if (haveTransparency)
            beginTransparencyLayers(p);
#endif
        
        // Paint our background first, before painting any child layers.
        // Establish the clip used to paint our background.
        setClip(p, paintDirtyRect, damageRect);

        // Paint the background.
        RenderObject::PaintInfo info(p, damageRect, PaintActionBlockBackground, paintingRootForRenderer);
        renderer()->paint(info, x - renderer()->xPos(), y - renderer()->yPos());        
#if APPLE_CHANGES
        // Our scrollbar widgets paint exactly when we tell them to, so that they work properly with
        // z-index.  We paint after we painted the background/border, so that the scrollbars will
        // sit above the background/border.
        paintScrollbars(p, damageRect);
#endif
        // Restore the clip.
        restoreClip(p, paintDirtyRect, damageRect);
    }

    // Now walk the sorted list of children with negative z-indices.
    if (m_negZOrderList) {
        uint count = m_negZOrderList->count();
        for (uint i = 0; i < count; i++) {
            RenderLayer* child = m_negZOrderList->at(i);
            child->paintLayer(rootLayer, p, paintDirtyRect, haveTransparency, selectionOnly, paintingRoot);
        }
    }
    
    // Now establish the appropriate clip and paint our child RenderObjects.
    if (shouldPaint && !clipRectToApply.isEmpty()) {
#if APPLE_CHANGES
        // Begin transparency layers lazily now that we know we have to paint something.
        if (haveTransparency)
            beginTransparencyLayers(p);
#endif

        // Set up the clip used when painting our children.
        setClip(p, paintDirtyRect, clipRectToApply);

        int tx = x - renderer()->xPos();
        int ty = y - renderer()->yPos();
        RenderObject::PaintInfo info(p, clipRectToApply, 
                                     selectionOnly ? PaintActionSelection : PaintActionChildBlockBackgrounds,
                                     paintingRootForRenderer);
        renderer()->paint(info, tx, ty);
        if (!selectionOnly) {
            info.phase = PaintActionFloat;
            renderer()->paint(info, tx, ty);
            info.phase = PaintActionForeground;
            renderer()->paint(info, tx, ty);
            info.phase = PaintActionOutline;
            renderer()->paint(info, tx, ty);
        }

        // Now restore our clip.
        restoreClip(p, paintDirtyRect, clipRectToApply);
    }
    
    // Now walk the sorted list of children with positive z-indices.
    if (m_posZOrderList) {
        uint count = m_posZOrderList->count();
        for (uint i = 0; i < count; i++) {
            RenderLayer* child = m_posZOrderList->at(i);
            child->paintLayer(rootLayer, p, paintDirtyRect, haveTransparency, selectionOnly, paintingRoot);
        }
    }
    
#if APPLE_CHANGES
    // End our transparency layer
    if (isTransparent() && m_usedTransparency) {
        p->endTransparencyLayer();
        m_usedTransparency = false;
    }
#endif
}

bool
RenderLayer::hitTest(RenderObject::NodeInfo& info, int x, int y)
{
#if APPLE_CHANGES
    // Clear our our scrollbar variable
    RenderLayer::gScrollBar = 0;
#endif
    
    QRect damageRect(m_x, m_y, width(), height());
    RenderLayer* insideLayer = hitTestLayer(this, info, x, y, damageRect);

    // Now determine if the result is inside an anchor; make sure an image map wins if
    // it already set URLElement and only use the innermost.
    NodeImpl* node = info.innerNode();
    while (node) {
        if (node->isLink() && !info.URLElement())
            info.setURLElement(node);
        node = node->parentNode();
    }

    // Next set up the correct :hover/:active state along the new chain.
    updateHoverActiveState(info);
    
    // Now return whether we were inside this layer (this will always be true for the root
    // layer).
    return insideLayer;
}

RenderLayer*
RenderLayer::hitTestLayer(RenderLayer* rootLayer, RenderObject::NodeInfo& info,
                          int xMousePos, int yMousePos, const QRect& hitTestRect)
{
    // Calculate the clip rects we should use.
    QRect layerBounds, bgRect, fgRect;
    calculateRects(rootLayer, hitTestRect, layerBounds, bgRect, fgRect);
    
    // Ensure our z-order lists are up-to-date.
    updateZOrderLists();

    // This variable tracks which layer the mouse ends up being inside.  The minute we find an insideLayer,
    // we are done and can return it.
    RenderLayer* insideLayer = 0;
    
    // Begin by walking our list of positive layers from highest z-index down to the lowest
    // z-index.
    if (m_posZOrderList) {
        uint count = m_posZOrderList->count();
        for (int i = count-1; i >= 0; i--) {
            RenderLayer* child = m_posZOrderList->at(i);
            insideLayer = child->hitTestLayer(rootLayer, info, xMousePos, yMousePos, hitTestRect);
            if (insideLayer)
                return insideLayer;
        }
    }

    // Next we want to see if the mouse pos is inside the child RenderObjects of the layer.
    if (containsPoint(xMousePos, yMousePos, fgRect) && 
        renderer()->hitTest(info, xMousePos, yMousePos,
                            layerBounds.x() - renderer()->xPos(),
                            layerBounds.y() - renderer()->yPos(), HitTestDescendants)) {
        // for positioned generated content, we might still not have a
        // node by the time we get to the layer level, since none of
        // the content in the layer has an element. So just walk up
        // the tree.
        if (!info.innerNode()) {
            for (RenderObject *r = renderer(); r != NULL; r = r->parent()) { 
                if (r->element()) {
                    info.setInnerNode(r->element());
                    break;
                }
            }
        }

        if (!info.innerNonSharedNode()) {
             for (RenderObject *r = renderer(); r != NULL; r = r->parent()) { 
                 if (r->element()) {
                     info.setInnerNonSharedNode(r->element());
                     break;
                 }
             }
        }
        return this;
    }
        
    // Now check our negative z-index children.
    if (m_negZOrderList) {
        uint count = m_negZOrderList->count();
        for (int i = count-1; i >= 0; i--) {
            RenderLayer* child = m_negZOrderList->at(i);
            insideLayer = child->hitTestLayer(rootLayer, info, xMousePos, yMousePos, hitTestRect);
            if (insideLayer)
                return insideLayer;
        }
    }

    // Next we want to see if the mouse pos is inside this layer but not any of its children.
    if (containsPoint(xMousePos, yMousePos, bgRect) &&
        renderer()->hitTest(info, xMousePos, yMousePos,
                            layerBounds.x() - renderer()->xPos(),
                            layerBounds.y() - renderer()->yPos(),
                            HitTestSelf))
        return this;

    // No luck.
    return 0;
}

void RenderLayer::calculateClipRects(const RenderLayer* rootLayer)
{
    if (m_clipRects)
        return; // We have the correct cached value.

    if (!parent()) {
        // The root layer's clip rect is always just its dimensions.
        m_clipRects = new (m_object->renderArena()) ClipRects(QRect(0,0,width(),height()));
        m_clipRects->ref();
        return;
    }

    // Ensure that our parent's clip has been calculated so that we can examine the values.
    parent()->calculateClipRects(rootLayer);

    // Set up our three rects to initially match the parent rects.
    QRect posClipRect(parent()->clipRects()->posClipRect());
    QRect overflowClipRect(parent()->clipRects()->overflowClipRect());
    QRect fixedClipRect(parent()->clipRects()->fixedClipRect());

    // A fixed object is essentially the root of its containing block hierarchy, so when
    // we encounter such an object, we reset our clip rects to the fixedClipRect.
    if (m_object->style()->position() == FIXED) {
        posClipRect = fixedClipRect;
        overflowClipRect = fixedClipRect;
    }
    else if (m_object->style()->position() == RELATIVE)
        posClipRect = overflowClipRect;
    
    // Update the clip rects that will be passed to child layers.
    if (m_object->hasOverflowClip() || m_object->hasClip()) {
        // This layer establishes a clip of some kind.
        int x = 0;
        int y = 0;
        convertToLayerCoords(rootLayer, x, y);
        
        if (m_object->hasOverflowClip()) {
            QRect newOverflowClip = m_object->getOverflowClipRect(x,y);
            overflowClipRect  = newOverflowClip.intersect(overflowClipRect);
            if (m_object->isPositioned() || m_object->isRelPositioned())
                posClipRect = newOverflowClip.intersect(posClipRect);
        }
        if (m_object->hasClip()) {
            QRect newPosClip = m_object->getClipRect(x,y);
            posClipRect = posClipRect.intersect(newPosClip);
            overflowClipRect = overflowClipRect.intersect(newPosClip);
            fixedClipRect = fixedClipRect.intersect(newPosClip);
        }
    }
    
    // If our clip rects match our parent's clip, then we can just share its data structure and
    // ref count.
    if (posClipRect == parent()->clipRects()->posClipRect() &&
        overflowClipRect == parent()->clipRects()->overflowClipRect() &&
        fixedClipRect == parent()->clipRects()->fixedClipRect())
        m_clipRects = parent()->clipRects();
    else
        m_clipRects = new (m_object->renderArena()) ClipRects(overflowClipRect, fixedClipRect, posClipRect);
    m_clipRects->ref();
}

void RenderLayer::calculateRects(const RenderLayer* rootLayer, const QRect& paintDirtyRect, QRect& layerBounds,
                                 QRect& backgroundRect, QRect& foregroundRect)
{
    if (parent()) {
        parent()->calculateClipRects(rootLayer);
        backgroundRect = m_object->style()->position() == FIXED ? parent()->clipRects()->fixedClipRect() :
                         (m_object->isPositioned() ? parent()->clipRects()->posClipRect() : 
                                                     parent()->clipRects()->overflowClipRect());
        backgroundRect = backgroundRect.intersect(paintDirtyRect);
    } else
        backgroundRect = paintDirtyRect;
    foregroundRect = backgroundRect;
    
    int x = 0;
    int y = 0;
    convertToLayerCoords(rootLayer, x, y);
    layerBounds = QRect(x,y,width(),height());
    
    // Update the clip rects that will be passed to child layers.
    if (m_object->hasOverflowClip() || m_object->hasClip()) {
        // This layer establishes a clip of some kind.
        if (m_object->hasOverflowClip())
            foregroundRect = foregroundRect.intersect(m_object->getOverflowClipRect(x,y));
        if (m_object->hasClip()) {
            // Clip applies to *us* as well, so go ahead and update the damageRect.
            QRect newPosClip = m_object->getClipRect(x,y);
            backgroundRect = backgroundRect.intersect(newPosClip);
            foregroundRect = foregroundRect.intersect(newPosClip);
        }

        // If we establish a clip at all, then go ahead and make sure our background
        // rect is intersected with our layer's bounds.
        backgroundRect = backgroundRect.intersect(layerBounds);
    }
}

static bool mustExamineRenderer(RenderObject* renderer)
{
    if (renderer->isCanvas() || renderer->isRoot() || renderer->isInlineFlow())
        return true;
    
    QRect bbox = renderer->borderBox();
    QRect overflowRect = renderer->overflowRect(false);
    if (bbox != overflowRect)
        return true;
    QRect floatRect = renderer->floatRect();
    if (bbox != floatRect)
        return true;

    return false;
}

bool RenderLayer::intersectsDamageRect(const QRect& layerBounds, const QRect& damageRect) const
{
    return mustExamineRenderer(renderer()) || layerBounds.intersects(damageRect);
}

bool RenderLayer::containsPoint(int x, int y, const QRect& damageRect) const
{
    return mustExamineRenderer(renderer()) || damageRect.contains(x, y);
}

void RenderLayer::clearClipRects()
{
    if (!m_clipRects)
        return;

    clearClipRect();
    
    for (RenderLayer* l = firstChild(); l; l = l->nextSibling())
        l->clearClipRects();
}

void RenderLayer::clearClipRect()
{
    if (m_clipRects) {
        m_clipRects->deref(m_object->renderArena());
        m_clipRects = 0;
    }
}

// This code has been written to anticipate the addition of CSS3-::outside and ::inside generated
// content (and perhaps XBL).  That's why it uses the render tree and not the DOM tree.
static RenderObject* hoverAncestor(RenderObject* obj)
{
    return (!obj->isInline() && obj->continuation()) ? obj->continuation() : obj->parent();
}

static RenderObject* commonAncestor(RenderObject* obj1, RenderObject* obj2)
{
    if (!obj1 || !obj2)
        return 0;

    for (RenderObject* currObj1 = obj1; currObj1; currObj1 = hoverAncestor(currObj1))
        for (RenderObject* currObj2 = obj2; currObj2; currObj2 = hoverAncestor(currObj2))
            if (currObj1 == currObj2)
                return currObj1;

    return 0;
}

void RenderLayer::updateHoverActiveState(RenderObject::NodeInfo& info)
{
    // We don't update :hover/:active state when the info is marked as readonly.
    if (info.readonly())
        return;

    DocumentImpl* doc = renderer()->document();
    if (!doc) return;

    NodeImpl* activeNode = doc->activeNode();
    if (activeNode && !info.active()) {
        // We are clearing the :active chain because the mouse has been released.
        for (RenderObject* curr = activeNode->renderer(); curr; curr = curr->parent()) {
            if (curr->element() && !curr->isText())
                curr->element()->setInActiveChain(false);
        }
        doc->setActiveNode(0);
    } else {
        NodeImpl* newActiveNode = info.innerNode();
        if (!activeNode && newActiveNode && info.active()) {
            // We are setting the :active chain and freezing it. If future moves happen, they
            // will need to reference this chain.
            for (RenderObject* curr = newActiveNode->renderer(); curr; curr = curr->parent()) {
                if (curr->element() && !curr->isText()) {
                    curr->element()->setInActiveChain(true);
                }
            }
            doc->setActiveNode(newActiveNode);
        }
    }

    // If the mouse is down and if this is a mouse move event, we want to restrict changes in 
    // :hover/:active to only apply to elements that are in the :active chain that we froze
    // at the time the mouse went down.
    bool mustBeInActiveChain = info.active() && info.mouseMove();

    // Check to see if the hovered node has changed.  If not, then we don't need to
    // do anything.  
    DOM::NodeImpl* oldHoverNode = doc->hoverNode();
    DOM::NodeImpl* newHoverNode = info.innerNode();

    // Update our current hover node.
    doc->setHoverNode(newHoverNode);

    // We have two different objects.  Fetch their renderers.
    RenderObject* oldHoverObj = oldHoverNode ? oldHoverNode->renderer() : 0;
    RenderObject* newHoverObj = newHoverNode ? newHoverNode->renderer() : 0;
    
    // Locate the common ancestor render object for the two renderers.
    RenderObject* ancestor = commonAncestor(oldHoverObj, newHoverObj);

    if (oldHoverObj != newHoverObj) {
        // The old hover path only needs to be cleared up to (and not including) the common ancestor;
        for (RenderObject* curr = oldHoverObj; curr && curr != ancestor; curr = hoverAncestor(curr)) {
            if (curr->element() && !curr->isText() && (!mustBeInActiveChain || curr->element()->inActiveChain())) {
                curr->element()->setActive(false);
                curr->element()->setHovered(false);
            }
        }
    }

    // Now set the hover state for our new object up to the root.
    for (RenderObject* curr = newHoverObj; curr; curr = hoverAncestor(curr)) {
        if (curr->element() && !curr->isText() && (!mustBeInActiveChain || curr->element()->inActiveChain())) {
            curr->element()->setActive(info.active());
            curr->element()->setHovered(true);
        }
    }
}

// Sort the buffer from lowest z-index to highest.  The common scenario will have
// most z-indices equal, so we optimize for that case (i.e., the list will be mostly
// sorted already).
static void sortByZOrder(QPtrVector<RenderLayer>* buffer,
                         QPtrVector<RenderLayer>* mergeBuffer,
                         uint start, uint end)
{
    if (start >= end)
        return; // Sanity check.

    if (end - start <= 6) {
        // Apply a bubble sort for smaller lists.
        for (uint i = end-1; i > start; i--) {
            bool sorted = true;
            for (uint j = start; j < i; j++) {
                RenderLayer* elt = buffer->at(j);
                RenderLayer* elt2 = buffer->at(j+1);
                if (elt->zIndex() > elt2->zIndex()) {
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

        RenderLayer* elt = buffer->at(mid-1);
        RenderLayer* elt2 = buffer->at(mid);

        // Handle the fast common case (of equal z-indices).  The list may already
        // be completely sorted.
        if (elt->zIndex() <= elt2->zIndex())
            return;

        // We have to merge sort.  Ensure our merge buffer is big enough to hold
        // all the items.
        mergeBuffer->resize(end - start);
        uint i1 = start;
        uint i2 = mid;

        elt = buffer->at(i1);
        elt2 = buffer->at(i2);

        while (i1 < mid || i2 < end) {
            if (i1 < mid && (i2 == end || elt->zIndex() <= elt2->zIndex())) {
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

void RenderLayer::dirtyZOrderLists()
{
    if (m_posZOrderList)
        m_posZOrderList->clear();
    if (m_negZOrderList)
        m_negZOrderList->clear();
    m_zOrderListsDirty = true;
}
    
void RenderLayer::updateZOrderLists()
{
    if (!isStackingContext() || !m_zOrderListsDirty)
        return;
    
    for (RenderLayer* child = firstChild(); child; child = child->nextSibling())
        child->collectLayers(m_posZOrderList, m_negZOrderList);

    // Sort the two lists.
    if (m_posZOrderList) {
        QPtrVector<RenderLayer> mergeBuffer;
        sortByZOrder(m_posZOrderList, &mergeBuffer, 0, m_posZOrderList->count());
    }
    if (m_negZOrderList) {
        QPtrVector<RenderLayer> mergeBuffer;
        sortByZOrder(m_negZOrderList, &mergeBuffer, 0, m_negZOrderList->count());
    }

    m_zOrderListsDirty = false;
}

void RenderLayer::collectLayers(QPtrVector<RenderLayer>*& posBuffer, QPtrVector<RenderLayer>*& negBuffer)
{
    // FIXME: A child render object or layer could override visibility.  Don't remove this
    // optimization though until RenderObject's nodeAtPoint is patched to understand what to do
    // when visibility is overridden by a child.
    if (renderer()->style()->visibility() != VISIBLE)
        return;
    
    // Determine which buffer the child should be in.
    QPtrVector<RenderLayer>*& buffer = (zIndex() >= 0) ? posBuffer : negBuffer;

    // Create the buffer if it doesn't exist yet.
    if (!buffer)
        buffer = new QPtrVector<RenderLayer>();
    
    // Resize by a power of 2 when our buffer fills up.
    if (buffer->count() == buffer->size())
        buffer->resize(2*(buffer->size()+1));

    // Append ourselves at the end of the appropriate buffer.
    buffer->insert(buffer->count(), this);

    // Recur into our children to collect more layers, but only if we don't establish
    // a stacking context.
    if (!isStackingContext()) {
        for (RenderLayer* child = firstChild(); child; child = child->nextSibling())
            child->collectLayers(posBuffer, negBuffer);
    }
}

void RenderLayer::repaintIncludingDescendants()
{
    m_object->repaint();
    for (RenderLayer* curr = firstChild(); curr; curr = curr->nextSibling())
        curr->repaintIncludingDescendants();
}

void RenderLayer::styleChanged()
{
    if (m_object->style()->overflow() == OMARQUEE && m_object->style()->marqueeBehavior() != MNONE) {
        if (!m_marquee)
            m_marquee = new Marquee(this);
        m_marquee->updateMarqueeStyle();
    }
    else if (m_marquee) {
        delete m_marquee;
        m_marquee = 0;
    }
}

void RenderLayer::suspendMarquees()
{
    if (m_marquee)
        m_marquee->suspend();
    
    for (RenderLayer* curr = firstChild(); curr; curr = curr->nextSibling())
        curr->suspendMarquees();
}

// --------------------------------------------------------------------------
// Marquee implementation

Marquee::Marquee(RenderLayer* l)
:m_layer(l), m_currentLoop(0), m_timerId(0), m_start(0), m_end(0), m_speed(0), m_unfurlPos(0), m_reset(false),
 m_suspended(false), m_stopped(false), m_whiteSpace(NORMAL), m_direction(MAUTO)
{
}

int Marquee::marqueeSpeed() const
{
    int result = m_layer->renderer()->style()->marqueeSpeed();
    DOM::NodeImpl* elt = m_layer->renderer()->element();
    if (elt && elt->hasTagName(HTMLTags::marquee())) {
        HTMLMarqueeElementImpl* marqueeElt = static_cast<HTMLMarqueeElementImpl*>(elt);
        result = kMax(result, marqueeElt->minimumDelay());
    }
    return result;
}

EMarqueeDirection Marquee::direction() const
{
    // FIXME: Support the CSS3 "auto" value for determining the direction of the marquee.
    // For now just map MAUTO to MBACKWARD
    EMarqueeDirection result = m_layer->renderer()->style()->marqueeDirection();
    EDirection dir =  m_layer->renderer()->style()->direction();
    if (result == MAUTO)
        result = MBACKWARD;
    if (result == MFORWARD)
        result = (dir == LTR) ? MRIGHT : MLEFT;
    if (result == MBACKWARD)
        result = (dir == LTR) ? MLEFT : MRIGHT;
    
    // Now we have the real direction.  Next we check to see if the increment is negative.
    // If so, then we reverse the direction.
    Length increment = m_layer->renderer()->style()->marqueeIncrement();
    if (increment.value < 0)
        result = static_cast<EMarqueeDirection>(-result);
    
    return result;
}

bool Marquee::isHorizontal() const
{
    return direction() == MLEFT || direction() == MRIGHT;
}

bool Marquee::isUnfurlMarquee() const
{
    EMarqueeBehavior behavior = m_layer->renderer()->style()->marqueeBehavior();
    return (behavior == MUNFURL);
}

int Marquee::computePosition(EMarqueeDirection dir, bool stopAtContentEdge)
{
    RenderObject* o = m_layer->renderer();
    RenderStyle* s = o->style();
    if (isHorizontal()) {
        bool ltr = s->direction() == LTR;
        int clientWidth = o->clientWidth();
        int contentWidth = ltr ? o->rightmostPosition(true, false) : o->leftmostPosition(true, false);
        if (ltr)
            contentWidth += (o->paddingRight() - o->borderLeft());
        else {
            contentWidth = o->width() - contentWidth;
            contentWidth += (o->paddingLeft() - o->borderRight());
        }
        if (dir == MRIGHT) {
            if (stopAtContentEdge)
                return kMax(0, ltr ? (contentWidth - clientWidth) : (clientWidth - contentWidth));
            else
                return ltr ? contentWidth : clientWidth;
        }
        else {
            if (stopAtContentEdge)
                return kMin(0, ltr ? (contentWidth - clientWidth) : (clientWidth - contentWidth));
            else
                return ltr ? -clientWidth : -contentWidth;
        }
    }
    else {
        int contentHeight = m_layer->renderer()->lowestPosition(true, false) - 
                            m_layer->renderer()->borderTop() + m_layer->renderer()->paddingBottom();
        int clientHeight = m_layer->renderer()->clientHeight();
        if (dir == MUP) {
            if (stopAtContentEdge)
                 return kMin(contentHeight - clientHeight, 0);
            else
                return -clientHeight;
        }
        else {
            if (stopAtContentEdge)
                return kMax(contentHeight - clientHeight, 0);
            else 
                return contentHeight;
        }
    }    
}

void Marquee::start()
{
    if (m_timerId || m_layer->renderer()->style()->marqueeIncrement().value == 0)
        return;
    
    if (!m_suspended && !m_stopped) {
        if (isUnfurlMarquee()) {
            bool forward = direction() == MDOWN || direction() == MRIGHT;
            bool isReversed = (forward && m_currentLoop % 2) || (!forward && !(m_currentLoop % 2));
            m_unfurlPos = isReversed ? m_end : m_start;
            m_layer->renderer()->setChildNeedsLayout(true);
        }
        else {
            if (isHorizontal())
                m_layer->scrollToOffset(m_start, 0, false, false);
            else
                m_layer->scrollToOffset(0, m_start, false, false);
        }
    }
    else {
        m_suspended = false;
	m_stopped = false;
    }

    m_timerId = startTimer(speed());
}

void Marquee::suspend()
{
    if (m_timerId) {
        killTimer(m_timerId);
        m_timerId = 0;
    }
    
    m_suspended = true;
}

void Marquee::stop()
{
    if (m_timerId) {
        killTimer(m_timerId);
        m_timerId = 0;
    }
    
    m_stopped = true;
}

void Marquee::updateMarqueePosition()
{
    bool activate = (m_totalLoops <= 0 || m_currentLoop < m_totalLoops);
    if (activate) {
        if (isUnfurlMarquee()) {
            if (m_unfurlPos < m_start) {
                m_unfurlPos = m_start;
                m_layer->renderer()->setChildNeedsLayout(true);
            }
            else if (m_unfurlPos > m_end) {
                m_unfurlPos = m_end;
                m_layer->renderer()->setChildNeedsLayout(true);
            }
        }
        else {
            EMarqueeBehavior behavior = m_layer->renderer()->style()->marqueeBehavior();
            m_start = computePosition(direction(), behavior == MALTERNATE);
            m_end = computePosition(reverseDirection(), behavior == MALTERNATE || behavior == MSLIDE);
        }
        if (!m_stopped)
            start();
    }
}

void Marquee::updateMarqueeStyle()
{
    RenderStyle* s = m_layer->renderer()->style();
    
    if (m_direction != s->marqueeDirection() || (m_totalLoops != s->marqueeLoopCount() && m_currentLoop >= m_totalLoops))
        m_currentLoop = 0; // When direction changes or our loopCount is a smaller number than our current loop, reset our loop.
    
    m_totalLoops = s->marqueeLoopCount();
    m_direction = s->marqueeDirection();
    m_whiteSpace = s->whiteSpace();
    
    if (m_layer->renderer()->isHTMLMarquee()) {
        // Hack for WinIE.  In WinIE, a value of 0 or lower for the loop count for SLIDE means to only do
        // one loop.
        if (m_totalLoops <= 0 && (s->marqueeBehavior() == MSLIDE || s->marqueeBehavior() == MUNFURL))
            m_totalLoops = 1;
        
        // Hack alert: Set the white-space value to nowrap for horizontal marquees with inline children, thus ensuring
        // all the text ends up on one line by default.  Limit this hack to the <marquee> element to emulate
        // WinIE's behavior.  Someone using CSS3 can use white-space: nowrap on their own to get this effect.
        // Second hack alert: Set the text-align back to auto.  WinIE completely ignores text-align on the
        // marquee element.
        // FIXME: Bring these up with the CSS WG.
        if (isHorizontal() && m_layer->renderer()->childrenInline()) {
            s->setWhiteSpace(NOWRAP);
            s->setTextAlign(TAAUTO);
        }
    }
    
    //Marquee height hack!! Make sure that, if it is a horizontal marquee, the height attribute is overridden 
    //if it is smaller than the font size. If it is a vertical marquee and height is not specified, we default
    //to a marquee of 200px.
    if (isHorizontal()) {
        if (s->height().isFixed() && (s->height().value < s->fontSize())) 
            s->setHeight(Length(s->fontSize(),Fixed));
    } else if (s->height().isVariable())  //vertical marquee with no specified height
        s->setHeight(Length(200,Fixed)); 
   
    if (speed() != marqueeSpeed()) {
        m_speed = marqueeSpeed();
        if (m_timerId) {
            killTimer(m_timerId);
            m_timerId = startTimer(speed());
        }
    }
    
    // Check the loop count to see if we should now stop.
    bool activate = (m_totalLoops <= 0 || m_currentLoop < m_totalLoops);
    if (activate && !m_timerId)
        m_layer->renderer()->setNeedsLayout(true);
    else if (!activate && m_timerId) {
        // Destroy the timer.
        killTimer(m_timerId);
        m_timerId = 0;
    }
}

void Marquee::timerEvent(QTimerEvent* evt)
{
    if (m_layer->renderer()->needsLayout())
        return;
    
    if (m_reset) {
        m_reset = false;
        if (isHorizontal())
            m_layer->scrollToXOffset(m_start);
        else
            m_layer->scrollToYOffset(m_start);
        return;
    }
    
    RenderStyle* s = m_layer->renderer()->style();
    
    int endPoint = m_end;
    int range = m_end - m_start;
    int newPos;
    if (range == 0)
        newPos = m_end;
    else {  
        bool addIncrement = direction() == MUP || direction() == MLEFT;
        bool isReversed = s->marqueeBehavior() == MALTERNATE && m_currentLoop % 2;
        if (isUnfurlMarquee()) {
            isReversed = (!addIncrement && m_currentLoop % 2) || (addIncrement && !(m_currentLoop % 2));
            addIncrement = !isReversed;
        }
        if (isReversed) {
            // We're going in the reverse direction.
            endPoint = m_start;
            range = -range;
            if (!isUnfurlMarquee())
                addIncrement = !addIncrement;
        }
        bool positive = range > 0;
        int clientSize = isUnfurlMarquee() ? abs(range) :
            (isHorizontal() ? m_layer->renderer()->clientWidth() : m_layer->renderer()->clientHeight());
        int increment = kMax(1, abs(m_layer->renderer()->style()->marqueeIncrement().width(clientSize)));
        int currentPos = isUnfurlMarquee() ? m_unfurlPos : 
            (isHorizontal() ? m_layer->scrollXOffset() : m_layer->scrollYOffset());
        newPos =  currentPos + (addIncrement ? increment : -increment);
        if (positive)
            newPos = kMin(newPos, endPoint);
        else
            newPos = kMax(newPos, endPoint);
    }

    if (newPos == endPoint) {
        m_currentLoop++;
        if (m_totalLoops > 0 && m_currentLoop >= m_totalLoops) {
            killTimer(m_timerId);
            m_timerId = 0;
        }
        else if (s->marqueeBehavior() != MALTERNATE && s->marqueeBehavior() != MUNFURL)
            m_reset = true;
    }
    
    if (isUnfurlMarquee()) {
        m_unfurlPos = newPos;
        m_layer->renderer()->setChildNeedsLayout(true);
    }
    else {
        if (isHorizontal())
            m_layer->scrollToXOffset(newPos);
        else
            m_layer->scrollToYOffset(newPos);
    }
}

