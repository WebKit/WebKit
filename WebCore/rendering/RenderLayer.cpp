/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
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

#include "config.h"
#include "RenderLayer.h"

#include "CSSPropertyNames.h"
#include "Document.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "FloatRect.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameView.h"
#include "FrameTree.h"
#include "GraphicsContext.h"
#include "HTMLMarqueeElement.h"
#include "HTMLNames.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "OverflowEvent.h"
#include "Page.h"
#include "PlatformMouseEvent.h"
#include "PlatformScrollBar.h" 
#include "RenderArena.h"
#include "RenderInline.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "SelectionController.h"

#if ENABLE(SVG)
#include "SVGNames.h"
#endif

#define MIN_INTERSECT_FOR_REVEAL 32

using namespace std;

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

#ifndef NDEBUG
static bool inRenderLayerDestroy;
#endif

const RenderLayer::ScrollAlignment RenderLayer::gAlignCenterIfNeeded = { RenderLayer::noScroll, RenderLayer::alignCenter, RenderLayer::alignToClosestEdge };
const RenderLayer::ScrollAlignment RenderLayer::gAlignToEdgeIfNeeded = { RenderLayer::noScroll, RenderLayer::alignToClosestEdge, RenderLayer::alignToClosestEdge };
const RenderLayer::ScrollAlignment RenderLayer::gAlignCenterAlways = { RenderLayer::alignCenter, RenderLayer::alignCenter, RenderLayer::alignCenter };
const RenderLayer::ScrollAlignment RenderLayer::gAlignTopAlways = { RenderLayer::alignTop, RenderLayer::alignTop, RenderLayer::alignTop };
const RenderLayer::ScrollAlignment RenderLayer::gAlignBottomAlways = { RenderLayer::alignBottom, RenderLayer::alignBottom, RenderLayer::alignBottom };

const int MinimumWidthWhileResizing = 100;
const int MinimumHeightWhileResizing = 40;

void* ClipRects::operator new(size_t sz, RenderArena* renderArena) throw()
{
    return renderArena->allocate(sz);
}

void ClipRects::operator delete(void* ptr, size_t sz)
{
    // Stash size where destroy can find it.
    *(size_t *)ptr = sz;
}

void ClipRects::destroy(RenderArena* renderArena)
{
    delete this;
    
    // Recover the size left there for us by operator delete and free the memory.
    renderArena->free(*(size_t *)this, this);
}

RenderLayer::RenderLayer(RenderObject* object)
    : m_object(object)
    , m_parent(0)
    , m_previous(0)
    , m_next(0)
    , m_first(0)
    , m_last(0)
    , m_relX(0)
    , m_relY(0)
    , m_x(0)
    , m_y(0)
    , m_width(0)
    , m_height(0)
    , m_scrollX(0)
    , m_scrollY(0)
    , m_scrollOriginX(0)
    , m_scrollLeftOverflow(0)
    , m_scrollWidth(0)
    , m_scrollHeight(0)
    , m_inResizeMode(false)
    , m_posZOrderList(0)
    , m_negZOrderList(0)
    , m_overflowList(0)
    , m_clipRects(0) 
    , m_scrollDimensionsDirty(true)
    , m_zOrderListsDirty(true)
    , m_overflowListDirty(true)
    , m_isOverflowOnly(shouldBeOverflowOnly())
    , m_usedTransparency(false)
    , m_inOverflowRelayout(false)
    , m_needsFullRepaint(false)
    , m_overflowStatusDirty(true)
    , m_visibleContentStatusDirty(true)
    , m_hasVisibleContent(false)
    , m_visibleDescendantStatusDirty(false)
    , m_hasVisibleDescendant(false)
    , m_marquee(0)
    , m_staticX(0)
    , m_staticY(0)
    , m_transform(0)
{
    if (!object->firstChild() && object->style()) {
        m_visibleContentStatusDirty = false;
        m_hasVisibleContent = object->style()->visibility() == VISIBLE;
    }
}

RenderLayer::~RenderLayer()
{
    if (inResizeMode() && !renderer()->documentBeingDestroyed()) {
        if (Frame* frame = renderer()->document()->frame())
            frame->eventHandler()->resizeLayerDestroyed();
    }

    destroyScrollbar(HorizontalScrollbar);
    destroyScrollbar(VerticalScrollbar);

    // Child layers will be deleted by their corresponding render objects, so
    // we don't need to delete them ourselves.

    delete m_posZOrderList;
    delete m_negZOrderList;
    delete m_overflowList;
    delete m_marquee;

    // Make sure we have no lingering clip rects.
    ASSERT(!m_clipRects);
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

    positionOverflowControls();

    updateVisibilityStatus();

    updateTransform();
     
    if (m_hasVisibleContent) {
        RenderView* view = m_object->view();
        ASSERT(view);
        // FIXME: Optimize using LayoutState and remove the disableLayoutState() call
        // from updateScrollInfoAfterLayout().
        ASSERT(!view->layoutState());

        IntRect newRect = m_object->absoluteClippedOverflowRect();
        IntRect newOutlineBox = m_object->absoluteOutlineBox();
        if (checkForRepaint) {
            if (view && !view->printing()) {
                if (m_needsFullRepaint) {
                    view->repaintViewRectangle(m_repaintRect);
                    if (newRect != m_repaintRect)
                        view->repaintViewRectangle(newRect);
                } else
                    m_object->repaintAfterLayoutIfNeeded(m_repaintRect, m_outlineBox);
            }
        }
        m_repaintRect = newRect;
        m_outlineBox = newOutlineBox;
    } else {
        m_repaintRect = IntRect();
        m_outlineBox = IntRect();
    }

    m_needsFullRepaint = false;
    
    for (RenderLayer* child = firstChild(); child; child = child->nextSibling())
        child->updateLayerPositions(doFullRepaint, checkForRepaint);
        
    // With all our children positioned, now update our marquee if we need to.
    if (m_marquee)
        m_marquee->updateMarqueePosition();
}

void RenderLayer::updateTransform()
{
    bool hasTransform = renderer()->hasTransform();
    bool hadTransform = m_transform;
    if (hasTransform != hadTransform) {
        if (hasTransform)
            m_transform.set(new AffineTransform);
        else
            m_transform.clear();
    }
    
    if (hasTransform) {
        m_transform->reset();
        renderer()->style()->applyTransform(*m_transform, renderer()->borderBox().size());
    }
}

void RenderLayer::setHasVisibleContent(bool b)
{ 
    if (m_hasVisibleContent == b && !m_visibleContentStatusDirty)
        return;
    m_visibleContentStatusDirty = false; 
    m_hasVisibleContent = b;
    if (m_hasVisibleContent) {
        m_repaintRect = renderer()->absoluteClippedOverflowRect();
        m_outlineBox = renderer()->absoluteOutlineBox();
        if (!isOverflowOnly()) {
            if (RenderLayer* sc = stackingContext())
                sc->dirtyZOrderLists();
        }
    }
    if (parent())
        parent()->childVisibilityChanged(m_hasVisibleContent);
}

void RenderLayer::dirtyVisibleContentStatus() 
{ 
    m_visibleContentStatusDirty = true; 
    if (parent())
        parent()->dirtyVisibleDescendantStatus();
}

void RenderLayer::childVisibilityChanged(bool newVisibility) 
{ 
    if (m_hasVisibleDescendant == newVisibility || m_visibleDescendantStatusDirty)
        return;
    if (newVisibility) {
        RenderLayer* l = this;
        while (l && !l->m_visibleDescendantStatusDirty && !l->m_hasVisibleDescendant) {
            l->m_hasVisibleDescendant = true;
            l = l->parent();
        }
    } else 
        dirtyVisibleDescendantStatus();
}

void RenderLayer::dirtyVisibleDescendantStatus()
{
    RenderLayer* l = this;
    while (l && !l->m_visibleDescendantStatusDirty) {
        l->m_visibleDescendantStatusDirty = true;
        l = l->parent();
    }
}

void RenderLayer::updateVisibilityStatus()
{
    if (m_visibleDescendantStatusDirty) {
        m_hasVisibleDescendant = false;
        for (RenderLayer* child = firstChild(); child; child = child->nextSibling()) {
            child->updateVisibilityStatus();        
            if (child->m_hasVisibleContent || child->m_hasVisibleDescendant) {
                m_hasVisibleDescendant = true;
                break;
            }
        }
        m_visibleDescendantStatusDirty = false;
    }

    if (m_visibleContentStatusDirty) {
        if (m_object->style()->visibility() == VISIBLE)
            m_hasVisibleContent = true;
        else {
            // layer may be hidden but still have some visible content, check for this
            m_hasVisibleContent = false;
            RenderObject* r = m_object->firstChild();
            while (r) {
                if (r->style()->visibility() == VISIBLE && !r->hasLayer()) {
                    m_hasVisibleContent = true;
                    break;
                }
                if (r->firstChild() && !r->hasLayer())
                    r = r->firstChild();
                else if (r->nextSibling())
                    r = r->nextSibling();
                else {
                    do {
                        r = r->parent();
                        if (r==m_object)
                            r = 0;
                    } while (r && !r->nextSibling());
                    if (r)
                        r = r->nextSibling();
                }
            }
        }    
        m_visibleContentStatusDirty = false; 
    }
}

void RenderLayer::updateLayerPosition()
{
    // Clear our cached clip rect information.
    clearClipRect();

    int x = m_object->xPos();
    int y = m_object->yPos() - m_object->borderTopExtra();

    if (!m_object->isPositioned() && m_object->parent()) {
        // We must adjust our position by walking up the render tree looking for the
        // nearest enclosing object with a layer.
        RenderObject* curr = m_object->parent();
        while (curr && !curr->hasLayer()) {
            if (!curr->isTableRow()) {
                // Rows and cells share the same coordinate space (that of the section).
                // Omit them when computing our xpos/ypos.
                x += curr->xPos();
                y += curr->yPos();
            }
            curr = curr->parent();
        }
        y += curr->borderTopExtra();
        if (curr->isTableRow()) {
            // Put ourselves into the row coordinate space.
            x -= curr->xPos();
            y -= curr->yPos();
        }
    }

    m_relX = m_relY = 0;
    if (m_object->isRelPositioned()) {
        m_relX = static_cast<RenderBox*>(m_object)->relativePositionOffsetX();
        m_relY = static_cast<RenderBox*>(m_object)->relativePositionOffsetY();
        x += m_relX; y += m_relY;
    }
    
    // Subtract our parent's scroll offset.
    if (m_object->isPositioned() && enclosingPositionedAncestor()) {
        RenderLayer* positionedParent = enclosingPositionedAncestor();

        // For positioned layers, we subtract out the enclosing positioned layer's scroll offset.
        positionedParent->subtractScrollOffset(x, y);
        
        if (m_object->isPositioned()) {
            IntSize offset = static_cast<RenderBox*>(m_object)->offsetForPositionedInContainer(positionedParent->renderer());
            x += offset.width();
            y += offset.height();
        }
    } else if (parent())
        parent()->subtractScrollOffset(x, y);
    
    setPos(x,y);

    setWidth(m_object->width());
    setHeight(m_object->height() + m_object->borderTopExtra() + m_object->borderBottomExtra());

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
    for ( ; curr && !curr->m_object->isRenderView() && !curr->m_object->isRoot() &&
          curr->m_object->style()->hasAutoZIndex();
          curr = curr->parent()) { };
    return curr;
}

RenderLayer*
RenderLayer::enclosingPositionedAncestor() const
{
    RenderLayer* curr = parent();
    for ( ; curr && !curr->m_object->isRenderView() && !curr->m_object->isPositioned() && !curr->m_object->isRelPositioned();
         curr = curr->parent()) { };
    return curr;
}

bool
RenderLayer::isTransparent() const
{
#if ENABLE(SVG)
    if (m_object->node()->namespaceURI() == SVGNames::svgNamespaceURI)
        return false;
#endif
    return m_object->isTransparent();
}

RenderLayer*
RenderLayer::transparentAncestor()
{
    RenderLayer* curr = parent();
    for ( ; curr && !curr->isTransparent(); curr = curr->parent()) { }
    return curr;
}

static IntRect transparencyClipBox(const AffineTransform& enclosingTransform, const RenderLayer* l, const RenderLayer* rootLayer)
{
    // FIXME: Although this function completely ignores CSS-imposed clipping, we did already intersect with the
    // paintDirtyRect, and that should cut down on the amount we have to paint.  Still it
    // would be better to respect clips.
    
    AffineTransform* t = l->transform();
    if (t) {
        // The best we can do here is to use enclosed bounding boxes to establish a "fuzzy" enough clip to encompass
        // the transformed layer and all of its children.
        int x = 0;
        int y = 0;
        l->convertToLayerCoords(rootLayer, x, y);
        AffineTransform transform;
        transform.translate(x, y);
        transform = *t * transform;
        transform = transform * enclosingTransform;
       
        // We now have a transform that will produce a rectangle in our view's space.
        IntRect clipRect = transform.mapRect(l->boundingBox(l));
        
        // Now shift the root layer to be us and pass down the new enclosing transform.
        for (RenderLayer* curr = l->firstChild(); curr; curr = curr->nextSibling())
            clipRect.unite(transparencyClipBox(transform, curr, l));
            
        return clipRect;
    }
    
    // Note: we don't have to walk z-order lists since transparent elements always establish
    // a stacking context.  This means we can just walk the layer tree directly.
    IntRect clipRect = l->boundingBox(rootLayer);
    for (RenderLayer* curr = l->firstChild(); curr; curr = curr->nextSibling())
        clipRect.unite(transparencyClipBox(enclosingTransform, curr, rootLayer));
    
    return clipRect;
}

void RenderLayer::beginTransparencyLayers(GraphicsContext* p, const RenderLayer* rootLayer)
{
    if (p->paintingDisabled() || (isTransparent() && m_usedTransparency))
        return;
    
    RenderLayer* ancestor = transparentAncestor();
    if (ancestor)
        ancestor->beginTransparencyLayers(p, rootLayer);
    
    if (isTransparent()) {
        m_usedTransparency = true;
        p->save();
        p->clip(transparencyClipBox(AffineTransform(), this, rootLayer));
        p->beginTransparencyLayer(renderer()->opacity());
    }
}

void* RenderLayer::operator new(size_t sz, RenderArena* renderArena) throw()
{
    return renderArena->allocate(sz);
}

void RenderLayer::operator delete(void* ptr, size_t sz)
{
    ASSERT(inRenderLayerDestroy);
    
    // Stash size where destroy can find it.
    *(size_t *)ptr = sz;
}

void RenderLayer::destroy(RenderArena* renderArena)
{
#ifndef NDEBUG
    inRenderLayerDestroy = true;
#endif
    delete this;
#ifndef NDEBUG
    inRenderLayerDestroy = false;
#endif
    
    // Recover the size left there for us by operator delete and free the memory.
    renderArena->free(*(size_t *)this, this);
}

void RenderLayer::addChild(RenderLayer* child, RenderLayer* beforeChild)
{
    RenderLayer* prevSibling = beforeChild ? beforeChild->previousSibling() : lastChild();
    if (prevSibling) {
        child->setPreviousSibling(prevSibling);
        prevSibling->setNextSibling(child);
    } else
        setFirstChild(child);

    if (beforeChild) {
        beforeChild->setPreviousSibling(child);
        child->setNextSibling(beforeChild);
    } else
        setLastChild(child);

    child->setParent(this);

    if (child->isOverflowOnly())
        dirtyOverflowList();

    if (!child->isOverflowOnly() || child->firstChild()) {
        // Dirty the z-order list in which we are contained.  The stackingContext() can be null in the
        // case where we're building up generated content layers.  This is ok, since the lists will start
        // off dirty in that case anyway.
        RenderLayer* stackingContext = child->stackingContext();
        if (stackingContext)
            stackingContext->dirtyZOrderLists();
    }

    child->updateVisibilityStatus();
    if (child->m_hasVisibleContent || child->m_hasVisibleDescendant)
        childVisibilityChanged(true);
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

    if (oldChild->isOverflowOnly())
        dirtyOverflowList();
    if (!oldChild->isOverflowOnly() || oldChild->firstChild()) { 
        // Dirty the z-order list in which we are contained.  When called via the
        // reattachment process in removeOnlyThisLayer, the layer may already be disconnected
        // from the main layer tree, so we need to null-check the |stackingContext| value.
        RenderLayer* stackingContext = oldChild->stackingContext();
        if (stackingContext)
            stackingContext->dirtyZOrderLists();
    }

    oldChild->setPreviousSibling(0);
    oldChild->setNextSibling(0);
    oldChild->setParent(0);
    
    oldChild->updateVisibilityStatus();
    if (oldChild->m_hasVisibleContent || oldChild->m_hasVisibleDescendant)
        childVisibilityChanged(false);
    
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
        current->updateLayerPositions();
        current = next;
    }
    
    destroy(renderer()->renderArena());
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
        
    if (m_object->style()->position() == FixedPosition) {
        // Add in the offset of the view.  We can obtain this by calling
        // absolutePosition() on the RenderView.
        int xOff, yOff;
        m_object->absolutePosition(xOff, yOff, true);
        x += xOff;
        y += yOff;
        return;
    }
 
    RenderLayer* parentLayer;
    if (m_object->style()->position() == AbsolutePosition)
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
    x += scrollXOffset() + m_scrollLeftOverflow;
    y += scrollYOffset();
}

void
RenderLayer::subtractScrollOffset(int& x, int& y)
{
    x -= scrollXOffset() + m_scrollLeftOverflow;
    y -= scrollYOffset();
}

void RenderLayer::scrollToOffset(int x, int y, bool updateScrollbars, bool repaint)
{
    if (renderer()->style()->overflowX() != OMARQUEE) {
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
    m_scrollX = x - m_scrollOriginX;
    m_scrollY = y;

    // Update the positions of our child layers.
    for (RenderLayer* child = firstChild(); child; child = child->nextSibling())
        child->updateLayerPositions(false, false);
    
    RenderView* view = renderer()->view();
    
    // We should have a RenderView if we're trying to scroll.
    ASSERT(view);
    if (view) {
        // Update dashboard regions, scrolling may change the clip of a
        // particular region.
        view->frameView()->updateDashboardRegions();

        view->updateWidgetPositions();
    }

    // Just schedule a full repaint of our object.
    if (repaint)
        m_object->repaint();
    
    if (updateScrollbars) {
        if (m_hBar)
            m_hBar->setValue(scrollXOffset());
        if (m_vBar)
            m_vBar->setValue(m_scrollY);
    }

    // Schedule the scroll DOM event.
    if (view)
        if (FrameView* frameView = view->frameView())
            frameView->scheduleEvent(new Event(scrollEvent, true, false), EventTargetNodeCast(renderer()->element()), true);
}

void RenderLayer::scrollRectToVisible(const IntRect &rect, const ScrollAlignment& alignX, const ScrollAlignment& alignY)
{
    RenderLayer* parentLayer = 0;
    IntRect newRect = rect;
    int xOffset = 0, yOffset = 0;

    // We may end up propagating a scroll event. It is important that we suspend events until 
    // the end of the function since they could delete the layer or the layer's m_object.
    FrameView* frameView = m_object->document()->view();
    if (frameView)
        frameView->pauseScheduledEvents();

    bool restrictedByLineClamp = false;
    if (m_object->parent()) {
        parentLayer = m_object->parent()->enclosingLayer();
        restrictedByLineClamp = m_object->parent()->style()->lineClamp() >= 0;
    }

    if (m_object->hasOverflowClip() && !restrictedByLineClamp) {
        // Don't scroll to reveal an overflow layer that is restricted by the -webkit-line-clamp property.
        // This will prevent us from revealing text hidden by the slider in Safari RSS.
        int x, y;
        m_object->absolutePosition(x, y);
        x += m_object->borderLeft();
        y += m_object->borderTop();

        IntRect layerBounds = IntRect(x + scrollXOffset(), y + scrollYOffset(), m_object->clientWidth(), m_object->clientHeight());
        IntRect exposeRect = IntRect(rect.x() + scrollXOffset(), rect.y() + scrollYOffset(), rect.width(), rect.height());
        IntRect r = getRectToExpose(layerBounds, exposeRect, alignX, alignY);
        
        xOffset = r.x() - x;
        yOffset = r.y() - y;
        // Adjust offsets if they're outside of the allowable range.
        xOffset = max(0, min(scrollWidth() - layerBounds.width(), xOffset));
        yOffset = max(0, min(scrollHeight() - layerBounds.height(), yOffset));
        
        if (xOffset != scrollXOffset() || yOffset != scrollYOffset()) {
            int diffX = scrollXOffset();
            int diffY = scrollYOffset();
            scrollToOffset(xOffset, yOffset);
            diffX = scrollXOffset() - diffX;
            diffY = scrollYOffset() - diffY;
            newRect.setX(rect.x() - diffX);
            newRect.setY(rect.y() - diffY);
        }
    } else if (!parentLayer) {
        if (frameView) {
            if (m_object->document() && m_object->document()->ownerElement() && m_object->document()->ownerElement()->renderer()) {
                IntRect viewRect = enclosingIntRect(frameView->visibleContentRect());
                IntRect r = getRectToExpose(viewRect, rect, alignX, alignY);
                
                xOffset = r.x();
                yOffset = r.y();
                // Adjust offsets if they're outside of the allowable range.
                xOffset = max(0, min(frameView->contentsWidth(), xOffset));
                yOffset = max(0, min(frameView->contentsHeight(), yOffset));

                frameView->setContentsPos(xOffset, yOffset);
                parentLayer = m_object->document()->ownerElement()->renderer()->enclosingLayer();
                newRect.setX(rect.x() - frameView->contentsX() + frameView->x());
                newRect.setY(rect.y() - frameView->contentsY() + frameView->y());
            } else {
                IntRect viewRect = enclosingIntRect(frameView->visibleContentRectConsideringExternalScrollers());
                IntRect r = getRectToExpose(viewRect, rect, alignX, alignY);
                
                // If this is the outermost view that RenderLayer needs to scroll, then we should scroll the view recursively
                // Other apps, like Mail, rely on this feature.
                frameView->scrollRectIntoViewRecursively(r);
            }
        }
    }
    
    if (parentLayer)
        parentLayer->scrollRectToVisible(newRect, alignX, alignY);

    if (frameView)
        frameView->resumeScheduledEvents();
}

IntRect RenderLayer::getRectToExpose(const IntRect &visibleRect, const IntRect &exposeRect, const ScrollAlignment& alignX, const ScrollAlignment& alignY)
{
    // Determine the appropriate X behavior.
    ScrollBehavior scrollX;
    IntRect exposeRectX(exposeRect.x(), visibleRect.y(), exposeRect.width(), visibleRect.height());
    int intersectWidth = intersection(visibleRect, exposeRectX).width();
    if (intersectWidth == exposeRect.width() || intersectWidth >= MIN_INTERSECT_FOR_REVEAL)
        // If the rectangle is fully visible, use the specified visible behavior.
        // If the rectangle is partially visible, but over a certain threshold,
        // then treat it as fully visible to avoid unnecessary horizontal scrolling
        scrollX = getVisibleBehavior(alignX);
    else if (intersectWidth == visibleRect.width()) {
        // If the rect is bigger than the visible area, don't bother trying to center. Other alignments will work.
        scrollX = getVisibleBehavior(alignX);
        if (scrollX == alignCenter)
            scrollX = noScroll;
    } else if (intersectWidth > 0)
        // If the rectangle is partially visible, but not above the minimum threshold, use the specified partial behavior
        scrollX = getPartialBehavior(alignX);
    else
        scrollX = getHiddenBehavior(alignX);
    // If we're trying to align to the closest edge, and the exposeRect is further right
    // than the visibleRect, and not bigger than the visible area, then align with the right.
    if (scrollX == alignToClosestEdge && exposeRect.right() > visibleRect.right() && exposeRect.width() < visibleRect.width())
        scrollX = alignRight;

    // Given the X behavior, compute the X coordinate.
    int x;
    if (scrollX == noScroll) 
        x = visibleRect.x();
    else if (scrollX == alignRight)
        x = exposeRect.right() - visibleRect.width();
    else if (scrollX == alignCenter)
        x = exposeRect.x() + (exposeRect.width() - visibleRect.width()) / 2;
    else
        x = exposeRect.x();

    // Determine the appropriate Y behavior.
    ScrollBehavior scrollY;
    IntRect exposeRectY(visibleRect.x(), exposeRect.y(), visibleRect.width(), exposeRect.height());
    int intersectHeight = intersection(visibleRect, exposeRectY).height();
    if (intersectHeight == exposeRect.height())
        // If the rectangle is fully visible, use the specified visible behavior.
        scrollY = getVisibleBehavior(alignY);
    else if (intersectHeight == visibleRect.height()) {
        // If the rect is bigger than the visible area, don't bother trying to center. Other alignments will work.
        scrollY = getVisibleBehavior(alignY);
        if (scrollY == alignCenter)
            scrollY = noScroll;
    } else if (intersectHeight > 0)
        // If the rectangle is partially visible, use the specified partial behavior
        scrollY = getPartialBehavior(alignY);
    else
        scrollY = getHiddenBehavior(alignY);
    // If we're trying to align to the closest edge, and the exposeRect is further down
    // than the visibleRect, and not bigger than the visible area, then align with the bottom.
    if (scrollY == alignToClosestEdge && exposeRect.bottom() > visibleRect.bottom() && exposeRect.height() < visibleRect.height())
        scrollY = alignBottom;

    // Given the Y behavior, compute the Y coordinate.
    int y;
    if (scrollY == noScroll) 
        y = visibleRect.y();
    else if (scrollY == alignBottom)
        y = exposeRect.bottom() - visibleRect.height();
    else if (scrollY == alignCenter)
        y = exposeRect.y() + (exposeRect.height() - visibleRect.height()) / 2;
    else
        y = exposeRect.y();

    return IntRect(IntPoint(x, y), visibleRect.size());
}

void RenderLayer::autoscroll()
{
    Frame* frame = renderer()->document()->frame();
    if (!frame)
        return;

    FrameView* frameView = frame->view();
    if (!frameView)
        return;

    frame->eventHandler()->updateSelectionForMouseDrag();

    IntPoint currentDocumentPosition = frameView->windowToContents(frame->eventHandler()->currentMousePosition());
    scrollRectToVisible(IntRect(currentDocumentPosition, IntSize(1, 1)), gAlignToEdgeIfNeeded, gAlignToEdgeIfNeeded);    
}

void RenderLayer::resize(const PlatformMouseEvent& evt, const IntSize& oldOffset)
{
    if (!inResizeMode() || !m_object->hasOverflowClip())
        return;

    // Set the width and height of the shadow ancestor node if there is one.
    // This is necessary for textarea elements since the resizable layer is in the shadow content.
    Element* element = static_cast<Element*>(m_object->node()->shadowAncestorNode());
    RenderBox* renderer = static_cast<RenderBox*>(element->renderer());

    EResize resize = renderer->style()->resize();
    if (resize == RESIZE_NONE)
        return;

    Document* document = element->document();
    if (!document->frame()->eventHandler()->mousePressed())
        return;

    IntSize newOffset = offsetFromResizeCorner(document->view()->windowToContents(evt.pos()));

    IntSize currentSize = IntSize(renderer->width(), renderer->height());

    IntSize minimumSize = element->minimumSizeForResizing().shrunkTo(currentSize);
    element->setMinimumSizeForResizing(minimumSize);

    IntSize difference = (currentSize + newOffset - oldOffset).expandedTo(minimumSize) - currentSize;

    CSSStyleDeclaration* style = element->style();
    bool isBoxSizingBorder = renderer->style()->boxSizing() == BORDER_BOX;

    ExceptionCode ec;

    if (difference.width()) {
        if (element && element->isControl()) {
            // Make implicit margins from the theme explicit (see <http://bugs.webkit.org/show_bug.cgi?id=9547>).
            style->setProperty(CSS_PROP_MARGIN_LEFT, String::number(renderer->marginLeft()) + "px", false, ec);
            style->setProperty(CSS_PROP_MARGIN_RIGHT, String::number(renderer->marginRight()) + "px", false, ec);
        }
        int baseWidth = renderer->width() - (isBoxSizingBorder ? 0
            : renderer->borderLeft() + renderer->paddingLeft() + renderer->borderRight() + renderer->paddingRight());
        style->setProperty(CSS_PROP_WIDTH, String::number(baseWidth + difference.width()) + "px", false, ec);
    }

    if (difference.height()) {
        if (element && element->isControl()) {
            // Make implicit margins from the theme explicit (see <http://bugs.webkit.org/show_bug.cgi?id=9547>).
            style->setProperty(CSS_PROP_MARGIN_TOP, String::number(renderer->marginTop()) + "px", false, ec);
            style->setProperty(CSS_PROP_MARGIN_BOTTOM, String::number(renderer->marginBottom()) + "px", false, ec);
        }
        int baseHeight = renderer->height() - (isBoxSizingBorder ? 0
            : renderer->borderTop() + renderer->paddingTop() + renderer->borderBottom() + renderer->paddingBottom());
        style->setProperty(CSS_PROP_HEIGHT, String::number(baseHeight + difference.height()) + "px", false, ec);
    }

    document->updateLayout();

    // FIXME (Radar 4118564): We should also autoscroll the window as necessary to keep the point under the cursor in view.
}

PlatformScrollbar* RenderLayer::horizontalScrollbarWidget() const
{
    if (m_hBar && m_hBar->isWidget())
        return static_cast<PlatformScrollbar*>(m_hBar.get());
    return 0;
}

PlatformScrollbar* RenderLayer::verticalScrollbarWidget() const
{
    if (m_vBar && m_vBar->isWidget())
        return static_cast<PlatformScrollbar*>(m_vBar.get());
    return 0;
}

void RenderLayer::valueChanged(Scrollbar*)
{
    // Update scroll position from scrollbars.

    bool needUpdate = false;
    int newX = scrollXOffset();
    int newY = m_scrollY;
    
    if (m_hBar) {
        newX = m_hBar->value();
        if (newX != scrollXOffset())
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

IntRect RenderLayer::windowClipRect() const
{
    RenderView* view = renderer()->view();
    ASSERT(view);
    FrameView* frameView = view->frameView();
    if (!frameView)
        return IntRect();
    return frameView->windowClipRectForLayer(this, false);
}

bool RenderLayer::isActive() const
{
    Page* page = renderer()->document()->frame()->page();
    return page && page->focusController()->isActive();
}

PassRefPtr<Scrollbar> RenderLayer::createScrollbar(ScrollbarOrientation orientation)
{
    if (Scrollbar::hasPlatformScrollbars()) {
        RefPtr<PlatformScrollbar> widget = new PlatformScrollbar(this, orientation, RegularScrollbar);
        m_object->document()->view()->addChild(widget.get());
        return widget.release();
    }
    
    // FIXME: Create scrollbars using the engine.
    return 0;
}

void RenderLayer::destroyScrollbar(ScrollbarOrientation orientation)
{
    RefPtr<Scrollbar>& scrollbar = orientation == HorizontalScrollbar ? m_hBar : m_vBar;
    if (scrollbar) {
        if (scrollbar->isWidget())
            static_cast<PlatformScrollbar*>(scrollbar.get())->removeFromParent();
        scrollbar->setClient(0);

        // FIXME: Destroy the engine scrollbar.
        scrollbar = 0;
    }
}

void RenderLayer::setHasHorizontalScrollbar(bool hasScrollbar)
{
    if (hasScrollbar == (m_hBar != 0))
        return;

    if (hasScrollbar)
        m_hBar = createScrollbar(HorizontalScrollbar);
    else
        destroyScrollbar(HorizontalScrollbar);

    // Force an update since we know the scrollbars have changed things.
    if (m_object->document()->hasDashboardRegions())
        m_object->document()->setDashboardRegionsDirty(true);
}

void RenderLayer::setHasVerticalScrollbar(bool hasScrollbar)
{
    if (hasScrollbar == (m_vBar != 0))
        return;

    if (hasScrollbar)
        m_vBar = createScrollbar(VerticalScrollbar);
    else
        destroyScrollbar(VerticalScrollbar);

    // Force an update since we know the scrollbars have changed things.
    if (m_object->document()->hasDashboardRegions())
        m_object->document()->setDashboardRegionsDirty(true);
}

int RenderLayer::verticalScrollbarWidth() const
{
    if (!m_vBar)
        return 0;
    return m_vBar->width();
}

int RenderLayer::horizontalScrollbarHeight() const
{
    if (!m_hBar)
        return 0;
    return m_hBar->height();
}

IntSize RenderLayer::offsetFromResizeCorner(const IntPoint& p) const
{
    // Currently the resize corner is always the bottom right corner
    int x = width();
    int y = height();
    convertToLayerCoords(root(), x, y);
    return p - IntPoint(x, y);
}

static IntRect scrollCornerRect(RenderObject* renderer, const IntRect& absBounds)
{
    int resizerWidth = PlatformScrollbar::verticalScrollbarWidth();
    int resizerHeight = PlatformScrollbar::horizontalScrollbarHeight();
    return IntRect(absBounds.right() - resizerWidth - renderer->style()->borderRightWidth(), 
                   absBounds.bottom() - resizerHeight - renderer->style()->borderBottomWidth(), resizerWidth, resizerHeight);
}

void RenderLayer::positionOverflowControls()
{
    if (!m_hBar && !m_vBar && (!m_object->hasOverflowClip() || m_object->style()->resize() == RESIZE_NONE))
        return;
    
    int x = 0;
    int y = 0;
    convertToLayerCoords(root(), x, y);
    IntRect absBounds(x, y, m_object->width(), m_object->height());
    
    IntRect resizeControlRect;
    if (m_object->style()->resize() != RESIZE_NONE)
        resizeControlRect = scrollCornerRect(m_object, absBounds);
    
    int resizeControlSize = max(resizeControlRect.height(), 0);
    if (m_vBar)
        m_vBar->setRect(IntRect(absBounds.right() - m_object->borderRight() - m_vBar->width(),
                                absBounds.y() + m_object->borderTop(),
                                m_vBar->width(),
                                absBounds.height() - (m_object->borderTop() + m_object->borderBottom()) - (m_hBar ? m_hBar->height() : resizeControlSize)));

    resizeControlSize = max(resizeControlRect.width(), 0);
    if (m_hBar)
        m_hBar->setRect(IntRect(absBounds.x() + m_object->borderLeft(),
                                absBounds.bottom() - m_object->borderBottom() - m_hBar->height(),
                                absBounds.width() - (m_object->borderLeft() + m_object->borderRight()) - (m_vBar ? m_vBar->width() : resizeControlSize),
                                m_hBar->height()));
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
    
    bool ltr = m_object->style()->direction() == LTR;

    int clientWidth = m_object->clientWidth();
    int clientHeight = m_object->clientHeight();

    m_scrollLeftOverflow = ltr ? 0 : min(0, m_object->leftmostPosition(true, false) - m_object->borderLeft());

    int rightPos = ltr ?
                    m_object->rightmostPosition(true, false) - m_object->borderLeft() :
                    clientWidth - m_scrollLeftOverflow;
    int bottomPos = m_object->lowestPosition(true, false) - m_object->borderTop();

    m_scrollWidth = max(rightPos, clientWidth);
    m_scrollHeight = max(bottomPos, clientHeight);
    
    m_scrollOriginX = ltr ? 0 : m_scrollWidth - clientWidth;

    if (needHBar)
        *needHBar = rightPos > clientWidth;
    if (needVBar)
        *needVBar = bottomPos > clientHeight;
}

void RenderLayer::updateOverflowStatus(bool horizontalOverflow, bool verticalOverflow)
{
    if (m_overflowStatusDirty) {
        m_horizontalOverflow = horizontalOverflow;
        m_verticalOverflow = verticalOverflow;
        m_overflowStatusDirty = false;
        
        return;
    }
    
    bool horizontalOverflowChanged = (m_horizontalOverflow != horizontalOverflow);
    bool verticalOverflowChanged = (m_verticalOverflow != verticalOverflow);
    
    if (horizontalOverflowChanged || verticalOverflowChanged) {
        m_horizontalOverflow = horizontalOverflow;
        m_verticalOverflow = verticalOverflow;
        
        if (FrameView* frameView = m_object->document()->view())
            frameView->scheduleEvent(new OverflowEvent(horizontalOverflowChanged, horizontalOverflow, verticalOverflowChanged, verticalOverflow),
            EventTargetNodeCast(m_object->element()), true);
    }
}

void
RenderLayer::updateScrollInfoAfterLayout()
{
    m_scrollDimensionsDirty = true;

    bool horizontalOverflow, verticalOverflow;
    computeScrollDimensions(&horizontalOverflow, &verticalOverflow);

    if (m_object->style()->overflowX() != OMARQUEE) {
        // Layout may cause us to be in an invalid scroll position.  In this case we need
        // to pull our scroll offsets back to the max (or push them up to the min).
        int newX = max(0, min(scrollXOffset(), scrollWidth() - m_object->clientWidth()));
        int newY = max(0, min(m_scrollY, scrollHeight() - m_object->clientHeight()));
        if (newX != scrollXOffset() || newY != m_scrollY) {
            RenderView* view = m_object->view();
            ASSERT(view);
            // scrollToOffset() may call updateLayerPositions(), which doesn't work
            // with LayoutState.
            // FIXME: Remove the disableLayoutState/enableLayoutState if the above changes.
            if (view)
                view->disableLayoutState();
            scrollToOffset(newX, newY);
            if (view)
                view->enableLayoutState();
        }
    }

    bool haveHorizontalBar = m_hBar;
    bool haveVerticalBar = m_vBar;
    
    // overflow:scroll should just enable/disable.
    if (m_object->style()->overflowX() == OSCROLL)
        m_hBar->setEnabled(horizontalOverflow);
    if (m_object->style()->overflowY() == OSCROLL)
        m_vBar->setEnabled(verticalOverflow);

    // A dynamic change from a scrolling overflow to overflow:hidden means we need to get rid of any
    // scrollbars that may be present.
    if (m_object->style()->overflowX() == OHIDDEN && haveHorizontalBar)
        setHasHorizontalScrollbar(false);
    if (m_object->style()->overflowY() == OHIDDEN && haveVerticalBar)
        setHasVerticalScrollbar(false);
    
    // overflow:auto may need to lay out again if scrollbars got added/removed.
    bool scrollbarsChanged = (m_object->hasAutoHorizontalScrollbar() && haveHorizontalBar != horizontalOverflow) || 
                             (m_object->hasAutoVerticalScrollbar() && haveVerticalBar != verticalOverflow);    
    if (scrollbarsChanged) {
        if (m_object->hasAutoHorizontalScrollbar())
            setHasHorizontalScrollbar(horizontalOverflow);
        if (m_object->hasAutoVerticalScrollbar())
            setHasVerticalScrollbar(verticalOverflow);

        // Force an update since we know the scrollbars have changed things.
        if (m_object->document()->hasDashboardRegions())
            m_object->document()->setDashboardRegionsDirty(true);

        m_object->repaint();

        if (m_object->style()->overflowX() == OAUTO || m_object->style()->overflowY() == OAUTO) {
            if (!m_inOverflowRelayout) {
                // Our proprietary overflow: overlay value doesn't trigger a layout.
                m_inOverflowRelayout = true;
                m_object->setNeedsLayout(true);
                if (m_object->isRenderBlock())
                    static_cast<RenderBlock*>(m_object)->layoutBlock(true);
                else
                    m_object->layout();
                m_inOverflowRelayout = false;
            }
        }
    }
    
    // If overflow:scroll is turned into overflow:auto a bar might still be disabled (Bug 11985).
    if (m_hBar && m_object->hasAutoHorizontalScrollbar())
        m_hBar->setEnabled(true);
    if (m_vBar && m_object->hasAutoVerticalScrollbar())
        m_vBar->setEnabled(true);

    // Set up the range (and page step/line step).
    if (m_hBar) {
        int clientWidth = m_object->clientWidth();
        int pageStep = (clientWidth-PAGE_KEEP);
        if (pageStep < 0) pageStep = clientWidth;
        m_hBar->setSteps(LINE_STEP, pageStep);
        m_hBar->setProportion(clientWidth, m_scrollWidth);
        m_hBar->setValue(scrollXOffset());
    }
    if (m_vBar) {
        int clientHeight = m_object->clientHeight();
        int pageStep = (clientHeight-PAGE_KEEP);
        if (pageStep < 0) pageStep = clientHeight;
        m_vBar->setSteps(LINE_STEP, pageStep);
        m_vBar->setProportion(clientHeight, m_scrollHeight);
    }
 
    if (m_object->element() && m_object->document()->hasListenerType(Document::OVERFLOWCHANGED_LISTENER))
        updateOverflowStatus(horizontalOverflow, verticalOverflow);
}

void RenderLayer::paintOverflowControls(GraphicsContext* p, int tx, int ty, const IntRect& damageRect)
{
    // Don't do anything if we have no overflow.
    if (!m_object->hasOverflowClip())
        return;
    
    // Move the scrollbar widgets if necessary.  We normally move and resize widgets during layout, but sometimes
    // widgets can move without layout occurring (most notably when you scroll a document that
    // contains fixed positioned elements).
    positionOverflowControls();

    // Now that we're sure the scrollbars are in the right place, paint them.
    if (m_hBar)
        m_hBar->paint(p, damageRect);
    if (m_vBar)
        m_vBar->paint(p, damageRect);
    
    // We fill our scroll corner with white if we have a scrollbar that doesn't run all the way up to the
    // edge of the box.
    IntRect paddingBox(m_object->xPos() + m_object->borderLeft() + tx, 
                       m_object->yPos() + m_object->borderTop() + ty, 
                       m_object->width() - m_object->borderLeft() - m_object->borderRight(),
                       m_object->height() - m_object->borderTop() - m_object->borderBottom());
    IntRect hCorner;
    if (m_hBar && paddingBox.width() - m_hBar->width() > 0) {
        hCorner = IntRect(paddingBox.x() + m_hBar->width(),
                          paddingBox.y() + paddingBox.height() - m_hBar->height(),
                          paddingBox.width() - m_hBar->width(),
                          m_hBar->height());
        if (hCorner.intersects(damageRect))
            p->fillRect(hCorner, Color::white);
    }
    if (m_vBar && paddingBox.height() - m_vBar->height() > 0) {
        IntRect vCorner(paddingBox.x() + paddingBox.width() - m_vBar->width(),
                        paddingBox.y() + m_vBar->height(),
                        m_vBar->width(),
                        paddingBox.height() - m_vBar->height());
        if (vCorner != hCorner && vCorner.intersects(damageRect))
            p->fillRect(vCorner, Color::white);
    }

    if (m_object->style()->resize() != RESIZE_NONE)  {
        IntRect absBounds(m_object->xPos() + tx, m_object->yPos() + ty, m_object->width(), m_object->height());
        IntRect scrollCorner = scrollCornerRect(m_object, absBounds);
        if (!scrollCorner.intersects(damageRect))
            return;

        // Paint the resizer control.
        static Image* resizeCornerImage;
        if (!resizeCornerImage)
            resizeCornerImage = Image::loadPlatformResource("textAreaResizeCorner");
        IntPoint imagePoint(scrollCorner.right() - resizeCornerImage->width(), scrollCorner.bottom() - resizeCornerImage->height());
        p->drawImage(resizeCornerImage, imagePoint);

        // Draw a frame around the resizer (1px grey line) if there are any scrollbars present.
        // Clipping will exclude the right and bottom edges of this frame.
        if (m_hBar || m_vBar) {
            p->save();
            scrollCorner.setSize(IntSize(scrollCorner.width() + 1, scrollCorner.height() + 1));
            p->setStrokeColor(Color(makeRGB(217, 217, 217)));
            p->setStrokeThickness(1.0f);
            p->setFillColor(Color::transparent);
            p->drawRect(scrollCorner);
            p->restore();
        }
    }
}

bool RenderLayer::isPointInResizeControl(const IntPoint& point)
{
    if (!m_object->hasOverflowClip() || m_object->style()->resize() == RESIZE_NONE)
        return false;
    
    int x = 0;
    int y = 0;
    convertToLayerCoords(root(), x, y);
    IntRect absBounds(x, y, m_object->width(), m_object->height());
    return scrollCornerRect(m_object, absBounds).contains(point);
}
    
bool RenderLayer::hitTestOverflowControls(HitTestResult& result)
{
    if (!m_hBar && !m_vBar && (!renderer()->hasOverflowClip() || renderer()->style()->resize() == RESIZE_NONE))
        return false;

    int x = 0;
    int y = 0;
    convertToLayerCoords(root(), x, y);
    IntRect absBounds(x, y, renderer()->width(), renderer()->height());
    
    IntRect resizeControlRect;
    if (renderer()->style()->resize() != RESIZE_NONE) {
        resizeControlRect = scrollCornerRect(renderer(), absBounds);
        if (resizeControlRect.contains(result.point()))
            return true;
    }

    int resizeControlSize = max(resizeControlRect.height(), 0);

    if (m_vBar) {
        IntRect vBarRect(absBounds.right() - renderer()->borderRight() - m_vBar->width(), absBounds.y() + renderer()->borderTop(), m_vBar->width(), absBounds.height() - (renderer()->borderTop() + renderer()->borderBottom()) - (m_hBar ? m_hBar->height() : resizeControlSize));
        if (vBarRect.contains(result.point())) {
            result.setScrollbar(verticalScrollbarWidget());
            return true;
        }
    }

    resizeControlSize = max(resizeControlRect.width(), 0);
    if (m_hBar) {
        IntRect hBarRect(absBounds.x() + renderer()->borderLeft(), absBounds.bottom() - renderer()->borderBottom() - m_hBar->height(), absBounds.width() - (renderer()->borderLeft() + renderer()->borderRight()) - (m_vBar ? m_vBar->width() : resizeControlSize), m_hBar->height());
        if (hBarRect.contains(result.point())) {
            result.setScrollbar(horizontalScrollbarWidget());
            return true;
        }
    }

    return false;
}

bool RenderLayer::scroll(ScrollDirection direction, ScrollGranularity granularity, float multiplier)
{
    bool didHorizontalScroll = false;
    bool didVerticalScroll = false;
    
    if (m_hBar) {
        if (granularity == ScrollByDocument) {
            // Special-case for the ScrollByDocument granularity. A document scroll can only be up 
            // or down and in both cases the horizontal bar goes all the way to the left.
            didHorizontalScroll = m_hBar->scroll(ScrollLeft, ScrollByDocument, multiplier);
        } else
            didHorizontalScroll = m_hBar->scroll(direction, granularity, multiplier);
    }

    if (m_vBar)
        didVerticalScroll = m_vBar->scroll(direction, granularity, multiplier);

    return (didHorizontalScroll || didVerticalScroll);
}

void
RenderLayer::paint(GraphicsContext* p, const IntRect& damageRect, PaintRestriction paintRestriction, RenderObject *paintingRoot)
{
    paintLayer(this, p, damageRect, false, paintRestriction, paintingRoot);
}

static void setClip(GraphicsContext* p, const IntRect& paintDirtyRect, const IntRect& clipRect)
{
    if (paintDirtyRect == clipRect)
        return;
    p->save();
    p->clip(clipRect);
}

static void restoreClip(GraphicsContext* p, const IntRect& paintDirtyRect, const IntRect& clipRect)
{
    if (paintDirtyRect == clipRect)
        return;
    p->restore();
}

void
RenderLayer::paintLayer(RenderLayer* rootLayer, GraphicsContext* p,
                        const IntRect& paintDirtyRect, bool haveTransparency, PaintRestriction paintRestriction,
                        RenderObject *paintingRoot)
{
    // Avoid painting layers when stylesheets haven't loaded.  This eliminates FOUC.
    // It's ok not to draw, because later on, when all the stylesheets do load, updateStyleSelector on the Document
    // will do a full repaint().
    if (renderer()->document()->didLayoutWithPendingStylesheets() && !renderer()->isRenderView() && !renderer()->isRoot())
        return;
    
    // If this layer is totally invisible then there is nothing to paint.
    if (!m_object->opacity())
        return;

    if (isTransparent())
        haveTransparency = true;

    // Apply a transform if we have one.
    if (m_transform && rootLayer != this) {
        // If the transform can't be inverted, then don't paint anything.
        if (!m_transform->isInvertible())
            return;

        // If we have a transparency layer enclosing us and we are the root of a transform, then we need to establish the transparency
        // layer from the parent now.
        if (haveTransparency)
            parent()->beginTransparencyLayers(p, rootLayer);
  
        // Make sure the parent's clip rects have been calculated.
        parent()->calculateClipRects(rootLayer);
        IntRect clipRect = parent()->clipRects()->overflowClipRect();
        clipRect.intersect(paintDirtyRect);

        // Push the parent coordinate space's clip.
        setClip(p, paintDirtyRect, clipRect);

        // Adjust the transform such that the renderer's upper left corner will paint at (0,0) in user space.
        // This involves subtracting out the position of the layer in our current coordinate space.
        int x = 0;
        int y = 0;
        convertToLayerCoords(rootLayer, x, y);
        AffineTransform transform;
        transform.translate(x, y);
        transform = *m_transform * transform;
        
        // Apply the transform.
        p->save();
        p->concatCTM(transform);

        // Now do a paint with the root layer shifted to be us.
        paintLayer(this, p, transform.inverse().mapRect(paintDirtyRect), haveTransparency, paintRestriction, paintingRoot);
        
        p->restore();
        
        // Restore the clip.
        restoreClip(p, paintDirtyRect, clipRect);
        
        return;
    }
    
    // Calculate the clip rects we should use.
    IntRect layerBounds, damageRect, clipRectToApply, outlineRect;
    calculateRects(rootLayer, paintDirtyRect, layerBounds, damageRect, clipRectToApply, outlineRect);
    int x = layerBounds.x();
    int y = layerBounds.y();
    int tx = x - renderer()->xPos();
    int ty = y - renderer()->yPos() + renderer()->borderTopExtra();
                             
    // Ensure our lists are up-to-date.
    updateZOrderLists();
    updateOverflowList();

    bool selectionOnly = paintRestriction == PaintRestrictionSelectionOnly || paintRestriction == PaintRestrictionSelectionOnlyBlackText;
    bool forceBlackText = paintRestriction == PaintRestrictionSelectionOnlyBlackText;
    
    // If this layer's renderer is a child of the paintingRoot, we render unconditionally, which
    // is done by passing a nil paintingRoot down to our renderer (as if no paintingRoot was ever set).
    // Else, our renderer tree may or may not contain the painting root, so we pass that root along
    // so it will be tested against as we decend through the renderers.
    RenderObject *paintingRootForRenderer = 0;
    if (paintingRoot && !m_object->isDescendantOf(paintingRoot))
        paintingRootForRenderer = paintingRoot;

    // We want to paint our layer, but only if we intersect the damage rect.
    bool shouldPaint = intersectsDamageRect(layerBounds, damageRect, rootLayer) && m_hasVisibleContent;
    if (shouldPaint && !selectionOnly && !damageRect.isEmpty()) {
        // Begin transparency layers lazily now that we know we have to paint something.
        if (haveTransparency)
            beginTransparencyLayers(p, rootLayer);
        
        // Paint our background first, before painting any child layers.
        // Establish the clip used to paint our background.
        setClip(p, paintDirtyRect, damageRect);

        // Paint the background.
        RenderObject::PaintInfo paintInfo(p, damageRect, PaintPhaseBlockBackground, false, paintingRootForRenderer, 0);
        renderer()->paint(paintInfo, tx, ty);

        // Our scrollbar widgets paint exactly when we tell them to, so that they work properly with
        // z-index.  We paint after we painted the background/border, so that the scrollbars will
        // sit above the background/border.
        paintOverflowControls(p, tx, ty, damageRect);
        
        // Restore the clip.
        restoreClip(p, paintDirtyRect, damageRect);
    }

    // Now walk the sorted list of children with negative z-indices.
    if (m_negZOrderList)
        for (Vector<RenderLayer*>::iterator it = m_negZOrderList->begin(); it != m_negZOrderList->end(); ++it)
            it[0]->paintLayer(rootLayer, p, paintDirtyRect, haveTransparency, paintRestriction, paintingRoot);
    
    // Now establish the appropriate clip and paint our child RenderObjects.
    if (shouldPaint && !clipRectToApply.isEmpty()) {
        // Begin transparency layers lazily now that we know we have to paint something.
        if (haveTransparency)
            beginTransparencyLayers(p, rootLayer);

        // Set up the clip used when painting our children.
        setClip(p, paintDirtyRect, clipRectToApply);
        RenderObject::PaintInfo paintInfo(p, clipRectToApply, 
                                          selectionOnly ? PaintPhaseSelection : PaintPhaseChildBlockBackgrounds,
                                          forceBlackText, paintingRootForRenderer, 0);
        renderer()->paint(paintInfo, tx, ty);
        if (!selectionOnly) {
            paintInfo.phase = PaintPhaseFloat;
            renderer()->paint(paintInfo, tx, ty);
            paintInfo.phase = PaintPhaseForeground;
            renderer()->paint(paintInfo, tx, ty);
            paintInfo.phase = PaintPhaseChildOutlines;
            renderer()->paint(paintInfo, tx, ty);
        }

        // Now restore our clip.
        restoreClip(p, paintDirtyRect, clipRectToApply);
    }
    
    if (!outlineRect.isEmpty()) {
        // Paint our own outline
        RenderObject::PaintInfo paintInfo(p, outlineRect, PaintPhaseSelfOutline, false, paintingRootForRenderer, 0);
        setClip(p, paintDirtyRect, outlineRect);
        renderer()->paint(paintInfo, tx, ty);
        restoreClip(p, paintDirtyRect, outlineRect);
    }
    
    // Paint any child layers that have overflow.
    if (m_overflowList)
        for (Vector<RenderLayer*>::iterator it = m_overflowList->begin(); it != m_overflowList->end(); ++it)
            it[0]->paintLayer(rootLayer, p, paintDirtyRect, haveTransparency, paintRestriction, paintingRoot);
    
    // Now walk the sorted list of children with positive z-indices.
    if (m_posZOrderList)
        for (Vector<RenderLayer*>::iterator it = m_posZOrderList->begin(); it != m_posZOrderList->end(); ++it)
            it[0]->paintLayer(rootLayer, p, paintDirtyRect, haveTransparency, paintRestriction, paintingRoot);
    
    // End our transparency layer
    if (isTransparent() && m_usedTransparency) {
        p->endTransparencyLayer();
        p->restore();
        m_usedTransparency = false;
    }
}

static inline IntRect frameVisibleRect(RenderObject* renderer)
{
    FrameView* frameView = renderer->document()->view();
    if (!frameView)
        return IntRect();

    return enclosingIntRect(frameView->visibleContentRect());
}

bool RenderLayer::hitTest(const HitTestRequest& request, HitTestResult& result)
{
    renderer()->document()->updateLayout();
    
    IntRect boundsRect(m_x, m_y, width(), height());
    boundsRect.intersect(frameVisibleRect(renderer()));

    RenderLayer* insideLayer = hitTestLayer(this, request, result, boundsRect, result.point());

    // Now determine if the result is inside an anchor; make sure an image map wins if
    // it already set URLElement and only use the innermost.
    Node* node = result.innerNode();
    while (node) {
        // for imagemaps, URLElement is the associated area element not the image itself
        if (node->isLink() && !result.URLElement() && !node->hasTagName(imgTag))
            result.setURLElement(static_cast<Element*>(node));
        node = node->eventParentNode();
    }

    // Next set up the correct :hover/:active state along the new chain.
    updateHoverActiveState(request, result);
    
    // Now return whether we were inside this layer (this will always be true for the root
    // layer).
    return insideLayer;
}

Node* RenderLayer::enclosingElement() const
{
    for (RenderObject* r = renderer(); r; r = r->parent()) {
        if (Node* e = r->element())
            return e;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

RenderLayer* RenderLayer::hitTestLayer(RenderLayer* rootLayer, const HitTestRequest& request, HitTestResult& result, const IntRect& hitTestRect, const IntPoint& hitTestPoint)
{
    // Apply a transform if we have one.
    if (m_transform && rootLayer != this) {
        // If the transform can't be inverted, then don't hit test this layer at all.
        if (!m_transform->isInvertible())
            return 0;
  
        // Make sure the parent's clip rects have been calculated.
        parent()->calculateClipRects(rootLayer);
        
        // Go ahead and test the enclosing clip now.
        IntRect clipRect = parent()->clipRects()->overflowClipRect();
        if (!clipRect.contains(hitTestPoint))
            return 0;

        // Adjust the transform such that the renderer's upper left corner is at (0,0) in user space.
        // This involves subtracting out the position of the layer in our current coordinate space.
        int x = 0;
        int y = 0;
        convertToLayerCoords(rootLayer, x, y);
        AffineTransform transform;
        transform.translate(x, y);
        transform = *m_transform * transform;
        
        // Map the hit test point into the transformed space and then do a hit test with the root layer shifted to be us.
        return hitTestLayer(this, request, result, transform.inverse().mapRect(hitTestRect), transform.inverse().mapPoint(hitTestPoint));
    }

    // Calculate the clip rects we should use.
    IntRect layerBounds;
    IntRect bgRect;
    IntRect fgRect;
    IntRect outlineRect;
    calculateRects(rootLayer, hitTestRect, layerBounds, bgRect, fgRect, outlineRect);
    
    // Ensure our lists are up-to-date.
    updateZOrderLists();
    updateOverflowList();

    // This variable tracks which layer the mouse ends up being inside.  The minute we find an insideLayer,
    // we are done and can return it.
    RenderLayer* insideLayer = 0;
    
    // Begin by walking our list of positive layers from highest z-index down to the lowest
    // z-index.
    if (m_posZOrderList) {
        for (int i = m_posZOrderList->size() - 1; i >= 0; --i) {
            insideLayer = m_posZOrderList->at(i)->hitTestLayer(rootLayer, request, result, hitTestRect, hitTestPoint);
            if (insideLayer)
                return insideLayer;
        }
    }

    // Now check our overflow objects.
    if (m_overflowList) {
        for (int i = m_overflowList->size() - 1; i >= 0; --i) {
            insideLayer = m_overflowList->at(i)->hitTestLayer(rootLayer, request, result, hitTestRect, hitTestPoint);
            if (insideLayer)
                return insideLayer;
        }
    }

    // Next we want to see if the mouse pos is inside the child RenderObjects of the layer.
    if (fgRect.contains(hitTestPoint) && 
        renderer()->hitTest(request, result, hitTestPoint,
                            layerBounds.x() - renderer()->xPos(),
                            layerBounds.y() - renderer()->yPos() + m_object->borderTopExtra(), 
                            HitTestDescendants)) {
        // For positioned generated content, we might still not have a
        // node by the time we get to the layer level, since none of
        // the content in the layer has an element. So just walk up
        // the tree.
        if (!result.innerNode() || !result.innerNonSharedNode()) {
            Node* e = enclosingElement();
            if (!result.innerNode())
                result.setInnerNode(e);
            if (!result.innerNonSharedNode())
                result.setInnerNonSharedNode(e);
        }

        return this;
    }
        
    // Now check our negative z-index children.
    if (m_negZOrderList) {
        for (int i = m_negZOrderList->size() - 1; i >= 0; --i) {
            insideLayer = m_negZOrderList->at(i)->hitTestLayer(rootLayer, request, result, hitTestRect, hitTestPoint);
            if (insideLayer)
                return insideLayer;
        }
    }

    // Next we want to see if the mouse is inside this layer but not any of its children.
    if (bgRect.contains(hitTestPoint) &&
        renderer()->hitTest(request, result, hitTestPoint,
                            layerBounds.x() - renderer()->xPos(),
                            layerBounds.y() - renderer()->yPos() + m_object->borderTopExtra(),
                            HitTestSelf)) {
        if (!result.innerNode() || !result.innerNonSharedNode()) {
            Node* e = enclosingElement();
            if (!result.innerNode())
                result.setInnerNode(e);
            if (!result.innerNonSharedNode())
                result.setInnerNonSharedNode(e);
        }

        return this;
    }

    // We didn't hit any layer. If we are the root layer and the mouse is -- or just was -- down, 
    // return ourselves. We do this so mouse events continue getting delivered after a drag has 
    // exited the WebView, and so hit testing over a scrollbar hits the content document.
    if ((request.active || request.mouseUp) && renderer()->isRenderView()) {
        renderer()->updateHitTestResult(result, hitTestPoint);
        return this;
    }

    return 0;
}

void RenderLayer::calculateClipRects(const RenderLayer* rootLayer)
{
    if (m_clipRects)
        return; // We have the correct cached value.

    if (rootLayer == this || !parent()) {
        // The root layer's clip rect is always infinite.
        m_clipRects = new (m_object->renderArena()) ClipRects(IntRect(INT_MIN/2, INT_MIN/2, INT_MAX, INT_MAX));
        m_clipRects->ref();
        return;
    }

    // Ensure that our parent's clip has been calculated so that we can examine the values.
    parent()->calculateClipRects(rootLayer);

    // Set up our three rects to initially match the parent rects.
    IntRect posClipRect(parent()->clipRects()->posClipRect());
    IntRect overflowClipRect(parent()->clipRects()->overflowClipRect());
    IntRect fixedClipRect(parent()->clipRects()->fixedClipRect());
    bool fixed = parent()->clipRects()->fixed();

    // A fixed object is essentially the root of its containing block hierarchy, so when
    // we encounter such an object, we reset our clip rects to the fixedClipRect.
    if (m_object->style()->position() == FixedPosition) {
        posClipRect = fixedClipRect;
        overflowClipRect = fixedClipRect;
        fixed = true;
    }
    else if (m_object->style()->position() == RelativePosition)
        posClipRect = overflowClipRect;
    else if (m_object->style()->position() == AbsolutePosition)
        overflowClipRect = posClipRect;
    
    // Update the clip rects that will be passed to child layers.
    if (m_object->hasOverflowClip() || m_object->hasClip()) {
        // This layer establishes a clip of some kind.
        int x = 0;
        int y = 0;
        convertToLayerCoords(rootLayer, x, y);
        RenderView* view = renderer()->view();
        ASSERT(view);
        if (view && fixed && rootLayer->renderer() == view) {
            x -= view->frameView()->contentsX();
            y -= view->frameView()->contentsY();
        }
        
        if (m_object->hasOverflowClip()) {
            IntRect newOverflowClip = m_object->getOverflowClipRect(x,y);
            overflowClipRect.intersect(newOverflowClip);
            if (m_object->isPositioned() || m_object->isRelPositioned())
                posClipRect.intersect(newOverflowClip);
        }
        if (m_object->hasClip()) {
            IntRect newPosClip = m_object->getClipRect(x,y);
            posClipRect.intersect(newPosClip);
            overflowClipRect.intersect(newPosClip);
            fixedClipRect.intersect(newPosClip);
        }
    }
    
    // If our clip rects match our parent's clip, then we can just share its data structure and
    // ref count.
    if (fixed == parent()->clipRects()->fixed() &&
        posClipRect == parent()->clipRects()->posClipRect() &&
        overflowClipRect == parent()->clipRects()->overflowClipRect() &&
        fixedClipRect == parent()->clipRects()->fixedClipRect())
        m_clipRects = parent()->clipRects();
    else
        m_clipRects = new (m_object->renderArena()) ClipRects(overflowClipRect, fixedClipRect, posClipRect, fixed);
    m_clipRects->ref();
}

void RenderLayer::calculateRects(const RenderLayer* rootLayer, const IntRect& paintDirtyRect, IntRect& layerBounds,
                                 IntRect& backgroundRect, IntRect& foregroundRect, IntRect& outlineRect) const
{
    if (rootLayer != this && parent()) {
        parent()->calculateClipRects(rootLayer);

        backgroundRect = m_object->style()->position() == FixedPosition ? parent()->clipRects()->fixedClipRect() :
                         (m_object->isPositioned() ? parent()->clipRects()->posClipRect() : 
                                                     parent()->clipRects()->overflowClipRect());
        RenderView* view = renderer()->view();
        ASSERT(view);
        if (view && parent()->clipRects()->fixed() && rootLayer->renderer() == view)
            backgroundRect.move(view->frameView()->contentsX(), view->frameView()->contentsY());

        backgroundRect.intersect(paintDirtyRect);
    } else
        backgroundRect = paintDirtyRect;
    foregroundRect = backgroundRect;
    outlineRect = backgroundRect;
    
    int x = 0;
    int y = 0;
    convertToLayerCoords(rootLayer, x, y);
    layerBounds = IntRect(x,y,width(),height());
    
    // Update the clip rects that will be passed to child layers.
    if (m_object->hasOverflowClip() || m_object->hasClip()) {
        // This layer establishes a clip of some kind.
        if (m_object->hasOverflowClip())
            foregroundRect.intersect(m_object->getOverflowClipRect(x,y));
        if (m_object->hasClip()) {
            // Clip applies to *us* as well, so go ahead and update the damageRect.
            IntRect newPosClip = m_object->getClipRect(x,y);
            backgroundRect.intersect(newPosClip);
            foregroundRect.intersect(newPosClip);
            outlineRect.intersect(newPosClip);
        }

        // If we establish a clip at all, then go ahead and make sure our background
        // rect is intersected with our layer's bounds.
        if (ShadowData* boxShadow = renderer()->style()->boxShadow()) {
            IntRect shadowRect = layerBounds;
            shadowRect.move(boxShadow->x, boxShadow->y);
            shadowRect.inflate(boxShadow->blur);
            shadowRect.unite(layerBounds);
            backgroundRect.intersect(shadowRect);
        } else
            backgroundRect.intersect(layerBounds);
    }
}

IntRect RenderLayer::childrenClipRect() const
{
    RenderLayer* rootLayer = renderer()->document()->renderer()->layer();
    IntRect layerBounds, backgroundRect, foregroundRect, outlineRect;
    calculateRects(rootLayer, rootLayer->boundingBox(rootLayer), layerBounds, backgroundRect, foregroundRect, outlineRect);
    return foregroundRect;
}

IntRect RenderLayer::selfClipRect() const
{
    RenderLayer* rootLayer = renderer()->document()->renderer()->layer();
    IntRect layerBounds, backgroundRect, foregroundRect, outlineRect;
    calculateRects(rootLayer, rootLayer->boundingBox(rootLayer), layerBounds, backgroundRect, foregroundRect, outlineRect);
    return backgroundRect;
}

bool RenderLayer::intersectsDamageRect(const IntRect& layerBounds, const IntRect& damageRect, const RenderLayer* rootLayer) const
{
    // Always examine the canvas and the root.
    // FIXME: Could eliminate the isRoot() check if we fix background painting so that the RenderView
    // paints the root's background.
    if (renderer()->isRenderView() || renderer()->isRoot())
        return true;

    // If we aren't an inline flow, and our layer bounds do intersect the damage rect, then we 
    // can go ahead and return true.
    RenderView* view = renderer()->view();
    ASSERT(view);
    if (view && !renderer()->isInlineFlow()) {
        IntRect b = layerBounds;
        b.inflate(view->maximalOutlineSize());
        if (b.intersects(damageRect))
            return true;
    }
        
    // Otherwise we need to compute the bounding box of this single layer and see if it intersects
    // the damage rect.
    return boundingBox(rootLayer).intersects(damageRect);
}

IntRect RenderLayer::boundingBox(const RenderLayer* rootLayer) const
{
    // There are three special cases we need to consider.
    // (1) Inline Flows.  For inline flows we will create a bounding box that fully encompasses all of the lines occupied by the
    // inline.  In other words, if some <span> wraps to three lines, we'll create a bounding box that fully encloses the root
    // line boxes of all three lines (including overflow on those lines).
    // (2) Left/Top Overflow.  The width/height of layers already includes right/bottom overflow.  However, in the case of left/top
    // overflow, we have to create a bounding box that will extend to include this overflow.
    // (3) Floats.  When a layer has overhanging floats that it paints, we need to make sure to include these overhanging floats
    // as part of our bounding box.  We do this because we are the responsible layer for both hit testing and painting those
    // floats.
    IntRect result;
    if (renderer()->isInlineFlow()) {
        // Go from our first line box to our last line box.
        RenderInline* inlineFlow = static_cast<RenderInline*>(renderer());
        InlineFlowBox* firstBox = inlineFlow->firstLineBox();
        if (!firstBox)
            return result;
        int top = firstBox->root()->topOverflow();
        int bottom = inlineFlow->lastLineBox()->root()->bottomOverflow();
        int left = firstBox->xPos();
        for (InlineRunBox* curr = firstBox->nextLineBox(); curr; curr = curr->nextLineBox())
            left = min(left, curr->xPos());
        result = IntRect(m_x + left, m_y + (top - renderer()->yPos()), width(), bottom - top);
    } else if (renderer()->isTableRow()) {
        // Our bounding box is just the union of all of our cells' border/overflow rects.
        for (RenderObject* child = renderer()->firstChild(); child; child = child->nextSibling()) {
            if (child->isTableCell()) {
                IntRect bbox = child->borderBox();
                bbox.move(0, child->borderTopExtra());
                result.unite(bbox);
                IntRect overflowRect = renderer()->overflowRect(false);
                overflowRect.move(0, child->borderTopExtra());
                if (bbox != overflowRect)
                    result.unite(overflowRect);
            }
        }
        result.move(m_x, m_y);
    } else {
        IntRect bbox = renderer()->borderBox();
        result = bbox;
        IntRect overflowRect = renderer()->overflowRect(false);
        if (bbox != overflowRect)
            result.unite(overflowRect);
        
        // We have to adjust the x/y of this result so that it is in the coordinate space of the layer.
        // We also have to add in borderTopExtra here, since borderBox(), in order to play well with methods like
        // floatRect that deal with child content, uses an origin of (0,0) that is at the child content box (so
        // border box returns a y coord of -borderTopExtra().  The layer, however, uses the outer box.  This is all
        // really confusing.
        result.move(m_x, m_y + renderer()->borderTopExtra());
    }
    
    // Convert the bounding box to an absolute position.  We can do this easily by looking at the delta
    // between the bounding box's xpos and our layer's xpos and then applying that to the absolute layerBounds
    // passed in.
    int absX = 0, absY = 0;
    convertToLayerCoords(rootLayer, absX, absY);
    result.move(absX - m_x, absY - m_y);
    RenderView* view = renderer()->view();
    ASSERT(view);
    if (view)
        result.inflate(view->maximalOutlineSize());
    return result;
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

static RenderObject* commonAncestor(RenderObject* obj1, RenderObject* obj2)
{
    if (!obj1 || !obj2)
        return 0;

    for (RenderObject* currObj1 = obj1; currObj1; currObj1 = currObj1->hoverAncestor())
        for (RenderObject* currObj2 = obj2; currObj2; currObj2 = currObj2->hoverAncestor())
            if (currObj1 == currObj2)
                return currObj1;

    return 0;
}

void RenderLayer::updateHoverActiveState(const HitTestRequest& request, HitTestResult& result)
{
    // We don't update :hover/:active state when the result is marked as readonly.
    if (request.readonly)
        return;

    Document* doc = renderer()->document();

    Node* activeNode = doc->activeNode();
    if (activeNode && !request.active) {
        // We are clearing the :active chain because the mouse has been released.
        for (RenderObject* curr = activeNode->renderer(); curr; curr = curr->parent()) {
            if (curr->element() && !curr->isText())
                curr->element()->setInActiveChain(false);
        }
        doc->setActiveNode(0);
    } else {
        Node* newActiveNode = result.innerNode();
        if (!activeNode && newActiveNode && request.active) {
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
    bool mustBeInActiveChain = request.active && request.mouseMove;

    // Check to see if the hovered node has changed.  If not, then we don't need to
    // do anything.  
    RefPtr<Node> oldHoverNode = doc->hoverNode();
    Node* newHoverNode = result.innerNode();

    // Update our current hover node.
    doc->setHoverNode(newHoverNode);

    // We have two different objects.  Fetch their renderers.
    RenderObject* oldHoverObj = oldHoverNode ? oldHoverNode->renderer() : 0;
    RenderObject* newHoverObj = newHoverNode ? newHoverNode->renderer() : 0;
    
    // Locate the common ancestor render object for the two renderers.
    RenderObject* ancestor = commonAncestor(oldHoverObj, newHoverObj);

    if (oldHoverObj != newHoverObj) {
        // The old hover path only needs to be cleared up to (and not including) the common ancestor;
        for (RenderObject* curr = oldHoverObj; curr && curr != ancestor; curr = curr->hoverAncestor()) {
            if (curr->element() && !curr->isText() && (!mustBeInActiveChain || curr->element()->inActiveChain())) {
                curr->element()->setActive(false);
                curr->element()->setHovered(false);
            }
        }
    }

    // Now set the hover state for our new object up to the root.
    for (RenderObject* curr = newHoverObj; curr; curr = curr->hoverAncestor()) {
        if (curr->element() && !curr->isText() && (!mustBeInActiveChain || curr->element()->inActiveChain())) {
            curr->element()->setActive(request.active);
            curr->element()->setHovered(true);
        }
    }
}

// Helper for the sorting of layers by z-index.
static inline bool compareZIndex(RenderLayer* first, RenderLayer* second)
{
    return first->zIndex() < second->zIndex();
}

void RenderLayer::dirtyZOrderLists()
{
    if (m_posZOrderList)
        m_posZOrderList->clear();
    if (m_negZOrderList)
        m_negZOrderList->clear();
    m_zOrderListsDirty = true;
}

void RenderLayer::dirtyOverflowList()
{
    if (m_overflowList)
        m_overflowList->clear();
    m_overflowListDirty = true;
}

void RenderLayer::updateZOrderLists()
{
    if (!isStackingContext() || !m_zOrderListsDirty)
        return;
        
    for (RenderLayer* child = firstChild(); child; child = child->nextSibling())
        child->collectLayers(m_posZOrderList, m_negZOrderList);

    // Sort the two lists.
    if (m_posZOrderList)
        std::stable_sort(m_posZOrderList->begin(), m_posZOrderList->end(), compareZIndex);
    if (m_negZOrderList)
        std::stable_sort(m_negZOrderList->begin(), m_negZOrderList->end(), compareZIndex);

    m_zOrderListsDirty = false;
}

void RenderLayer::updateOverflowList()
{
    if (!m_overflowListDirty)
        return;
        
    for (RenderLayer* child = firstChild(); child; child = child->nextSibling()) {
        if (child->isOverflowOnly()) {
            if (!m_overflowList)
                m_overflowList = new Vector<RenderLayer*>;
            m_overflowList->append(child);
        }
    }
    
    m_overflowListDirty = false;
}

void RenderLayer::collectLayers(Vector<RenderLayer*>*& posBuffer, Vector<RenderLayer*>*& negBuffer)
{
    updateVisibilityStatus();
        
    // Overflow layers are just painted by their enclosing layers, so they don't get put in zorder lists.
    if ((m_hasVisibleContent || (m_hasVisibleDescendant && isStackingContext())) && !isOverflowOnly()) {
        // Determine which buffer the child should be in.
        Vector<RenderLayer*>*& buffer = (zIndex() >= 0) ? posBuffer : negBuffer;

        // Create the buffer if it doesn't exist yet.
        if (!buffer)
            buffer = new Vector<RenderLayer*>;
        
        // Append ourselves at the end of the appropriate buffer.
        buffer->append(this);
    }

    // Recur into our children to collect more layers, but only if we don't establish
    // a stacking context.
    if (m_hasVisibleDescendant && !isStackingContext())
        for (RenderLayer* child = firstChild(); child; child = child->nextSibling())
            child->collectLayers(posBuffer, negBuffer);
}

void RenderLayer::repaintIncludingDescendants()
{
    m_object->repaint();
    for (RenderLayer* curr = firstChild(); curr; curr = curr->nextSibling())
        curr->repaintIncludingDescendants();
}

bool RenderLayer::shouldBeOverflowOnly() const
{
    return renderer()->hasOverflowClip() && 
           !renderer()->isPositioned() &&
           !renderer()->isRelPositioned() &&
           !isTransparent();
}

void RenderLayer::styleChanged()
{
    bool isOverflowOnly = shouldBeOverflowOnly();
    if (isOverflowOnly != m_isOverflowOnly) {
        m_isOverflowOnly = isOverflowOnly;
        RenderLayer* p = parent();
        RenderLayer* sc = stackingContext();
        if (p)
            p->dirtyOverflowList();
        if (sc)
            sc->dirtyZOrderLists();
    }

    if (m_object->style()->overflowX() == OMARQUEE && m_object->style()->marqueeBehavior() != MNONE) {
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
    : m_layer(l), m_currentLoop(0)
    , m_totalLoops(0)
    , m_timer(this, &Marquee::timerFired)
    , m_start(0), m_end(0), m_speed(0), m_reset(false)
    , m_suspended(false), m_stopped(false), m_direction(MAUTO)
{
}

int Marquee::marqueeSpeed() const
{
    int result = m_layer->renderer()->style()->marqueeSpeed();
    Node* elt = m_layer->renderer()->element();
    if (elt && elt->hasTagName(marqueeTag)) {
        HTMLMarqueeElement* marqueeElt = static_cast<HTMLMarqueeElement*>(elt);
        result = max(result, marqueeElt->minimumDelay());
    }
    return result;
}

EMarqueeDirection Marquee::direction() const
{
    // FIXME: Support the CSS3 "auto" value for determining the direction of the marquee.
    // For now just map MAUTO to MBACKWARD
    EMarqueeDirection result = m_layer->renderer()->style()->marqueeDirection();
    TextDirection dir = m_layer->renderer()->style()->direction();
    if (result == MAUTO)
        result = MBACKWARD;
    if (result == MFORWARD)
        result = (dir == LTR) ? MRIGHT : MLEFT;
    if (result == MBACKWARD)
        result = (dir == LTR) ? MLEFT : MRIGHT;
    
    // Now we have the real direction.  Next we check to see if the increment is negative.
    // If so, then we reverse the direction.
    Length increment = m_layer->renderer()->style()->marqueeIncrement();
    if (increment.isNegative())
        result = static_cast<EMarqueeDirection>(-result);
    
    return result;
}

bool Marquee::isHorizontal() const
{
    return direction() == MLEFT || direction() == MRIGHT;
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
                return max(0, ltr ? (contentWidth - clientWidth) : (clientWidth - contentWidth));
            else
                return ltr ? contentWidth : clientWidth;
        }
        else {
            if (stopAtContentEdge)
                return min(0, ltr ? (contentWidth - clientWidth) : (clientWidth - contentWidth));
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
                 return min(contentHeight - clientHeight, 0);
            else
                return -clientHeight;
        }
        else {
            if (stopAtContentEdge)
                return max(contentHeight - clientHeight, 0);
            else 
                return contentHeight;
        }
    }    
}

void Marquee::start()
{
    if (m_timer.isActive() || m_layer->renderer()->style()->marqueeIncrement().isZero())
        return;

    // We may end up propagating a scroll event. It is important that we suspend events until 
    // the end of the function since they could delete the layer, including the marquee.
    FrameView* frameView = m_layer->renderer()->document()->view();
    if (frameView)
        frameView->pauseScheduledEvents();

    if (!m_suspended && !m_stopped) {
        if (isHorizontal())
            m_layer->scrollToOffset(m_start, 0, false, false);
        else
            m_layer->scrollToOffset(0, m_start, false, false);
    }
    else {
        m_suspended = false;
        m_stopped = false;
    }

    m_timer.startRepeating(speed() * 0.001);

    if (frameView)
        frameView->resumeScheduledEvents();
}

void Marquee::suspend()
{
    m_timer.stop();
    m_suspended = true;
}

void Marquee::stop()
{
    m_timer.stop();
    m_stopped = true;
}

void Marquee::updateMarqueePosition()
{
    bool activate = (m_totalLoops <= 0 || m_currentLoop < m_totalLoops);
    if (activate) {
        EMarqueeBehavior behavior = m_layer->renderer()->style()->marqueeBehavior();
        m_start = computePosition(direction(), behavior == MALTERNATE);
        m_end = computePosition(reverseDirection(), behavior == MALTERNATE || behavior == MSLIDE);
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
    
    if (m_layer->renderer()->isHTMLMarquee()) {
        // Hack for WinIE.  In WinIE, a value of 0 or lower for the loop count for SLIDE means to only do
        // one loop.
        if (m_totalLoops <= 0 && s->marqueeBehavior() == MSLIDE)
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
    
    // Marquee height hack!! Make sure that, if it is a horizontal marquee, the height attribute is overridden 
    // if it is smaller than the font size. If it is a vertical marquee and height is not specified, we default
    // to a marquee of 200px.
    if (isHorizontal()) {
        if (s->height().isFixed() && s->height().value() < s->fontSize())
            s->setHeight(Length(s->fontSize(),Fixed));
    } else if (s->height().isAuto())  //vertical marquee with no specified height
        s->setHeight(Length(200, Fixed)); 
   
    if (speed() != marqueeSpeed()) {
        m_speed = marqueeSpeed();
        if (m_timer.isActive())
            m_timer.startRepeating(speed() * 0.001);
    }
    
    // Check the loop count to see if we should now stop.
    bool activate = (m_totalLoops <= 0 || m_currentLoop < m_totalLoops);
    if (activate && !m_timer.isActive())
        m_layer->renderer()->setNeedsLayout(true);
    else if (!activate && m_timer.isActive())
        m_timer.stop();
}

void Marquee::timerFired(Timer<Marquee>*)
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
        if (isReversed) {
            // We're going in the reverse direction.
            endPoint = m_start;
            range = -range;
            addIncrement = !addIncrement;
        }
        bool positive = range > 0;
        int clientSize = (isHorizontal() ? m_layer->renderer()->clientWidth() : m_layer->renderer()->clientHeight());
        int increment = max(1, abs(m_layer->renderer()->style()->marqueeIncrement().calcValue(clientSize)));
        int currentPos = (isHorizontal() ? m_layer->scrollXOffset() : m_layer->scrollYOffset());
        newPos =  currentPos + (addIncrement ? increment : -increment);
        if (positive)
            newPos = min(newPos, endPoint);
        else
            newPos = max(newPos, endPoint);
    }

    if (newPos == endPoint) {
        m_currentLoop++;
        if (m_totalLoops > 0 && m_currentLoop >= m_totalLoops)
            m_timer.stop();
        else if (s->marqueeBehavior() != MALTERNATE)
            m_reset = true;
    }
    
    if (isHorizontal())
        m_layer->scrollToXOffset(newPos);
    else
        m_layer->scrollToYOffset(newPos);
}

} // namespace WebCore
