/*
 * Copyright (C) 2002, 2003 Apple Computer, Inc.
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
#include "render_box.h"
#include "render_arena.h"
#include "xml/dom_docimpl.h"

using namespace DOM;
using namespace khtml;

#ifndef NDEBUG
static bool inRenderLayerDetach;
static bool inRenderLayerElementDetach;
static bool inRenderZTreeNodeDetach;
#endif

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
m_width( 0 )
{
}

RenderLayer::~RenderLayer()
{
    // Child layers will be deleted by their corresponding render objects, so
    // our destructor doesn't have to do anything.
    m_parent = m_previous = m_next = m_first = m_last = 0;
}

void RenderLayer::updateLayerPosition()
{
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
    
    setPos(x,y);
    
    if (m_object->overflowWidth() > m_object->width())
        setWidth(m_object->overflowWidth());
    if (m_object->overflowHeight() > m_object->height())
        setHeight(m_object->overflowHeight());
}

RenderLayer*
RenderLayer::enclosingPositionedAncestor()
{
    RenderLayer* curr = parent();
    for ( ; curr && !curr->m_object->isRoot() && !curr->m_object->isHtml() &&
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

void RenderLayer::addChild(RenderLayer *child)
{
    if (!firstChild()) {
        setFirstChild(child);
        setLastChild(child);
    } else {
        RenderLayer* last = lastChild();
        setLastChild(child);
        child->setPreviousSibling(last);
        last->setNextSibling(child);
    }

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

void 
RenderLayer::convertToLayerCoords(RenderLayer* ancestorLayer, int& x, int& y)
{
    if (ancestorLayer == this)
        return;
        
    if (m_object->style()->position() == FIXED) {
        // Add in the offset of the view.  We can obtain this by calling
        // absolutePosition() on the RenderRoot.
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
RenderLayer::paint(QPainter *p, int x, int y, int w, int h)
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
            // background or borders.  Go ahead and draw those now without our clip (that will
            // be used for our children) in effect.
            elt->layer->renderer()->paintBoxDecorations(p, x, y, w, h,
                                elt->absBounds.x(),
                                elt->absBounds.y());
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
              
        elt->layer->renderer()->paint(p, x, y, w, h,
                                      elt->absBounds.x() - elt->layer->renderer()->xPos(),
                                      elt->absBounds.y() - elt->layer->renderer()->yPos(),
                                      BACKGROUND_PHASE);
        elt->layer->renderer()->paint(p, x, y, w, h,
                                      elt->absBounds.x() - elt->layer->renderer()->xPos(),
                                      elt->absBounds.y() - elt->layer->renderer()->yPos(),
                                      FLOAT_PHASE);
        elt->layer->renderer()->paint(p, x, y, w, h,
                                      elt->absBounds.x() - elt->layer->renderer()->xPos(),
                                      elt->absBounds.y() - elt->layer->renderer()->yPos(),
                                      FOREGROUND_PHASE);
    }
    
    if (currRect != paintRect)
        p->restore(); // Pop the clip.
        
    node->detach(renderer()->renderArena());
}

bool
RenderLayer::nodeAtPoint(RenderObject::NodeInfo& info, int x, int y)
{
    bool inside = false;
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
        if (inside)
            break;
    }
    node->detach(renderer()->renderArena());

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
        }
        if (m_object->hasClip()) {
            QRect newPosClip = m_object->getClipRect(x,y);
            if (m_object->hasOverflowClip())
                newPosClip = newPosClip.intersect(m_object->getOverflowClipRect(x,y));
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
    if (renderer()->isRoot() || renderer()->isHtml() || renderer()->isBody() ||
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

    if (autoZIndex)
        return; // We just had to collect the kids.  We don't apply a sort to them, since
                // they will actually be layered in some ancestor layer's stacking context.
    
    sortByZOrder(buffer, mergeTmpBuffer, startIndex, endIndex);

    // Now set all of the elements' z-indices to match the parent's explicit z-index, so that
    // they will be layered properly in the ancestor layer's stacking context.
    for (uint i = startIndex; i < endIndex; i++) {
        RenderLayerElement* elt = buffer->at(i);
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
    if (next)
        next->detach(renderArena); 
    if (child)
        child->detach(renderArena);
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
