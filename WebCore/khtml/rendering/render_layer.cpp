/**
 * This file is part of the html renderer for KDE.
 *
 * Copyright (C) 2002 (hyatt@apple.com)
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
 *
 */

#include "render_layer.h"
#include <kdebug.h>
#include <assert.h>
#include "khtmlview.h"
#include "render_box.h"

using namespace DOM;
using namespace khtml;

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
    RenderLayer::RenderZTreeNode* node = constructZTree(QRect(x, y, w, h), this);
    if (!node)
        return;

    // Flatten the tree into a back-to-front list for painting.
    QPtrVector<RenderLayer::RenderLayerElement> layerList;
    constructLayerList(node, &layerList);

    // Walk the list and paint each layer, adding in the appropriate offset.
    QRect paintRect(x, y, w, h);
    QRect currRect(paintRect);
    
    uint count = layerList.count();
    for (uint i = 0; i < count; i++) {
        RenderLayer::RenderLayerElement* elt = layerList.at(i);

        // Elements add in their own positions as a translation factor.  This forces
        // us to subtract that out, so that when it's added back in, we get the right
        // bounds.  This is really disgusting (that print only sets up the right paint
        // position after you call into it). -dwh
        //printf("Painting layer at %d %d\n", elt->absBounds.x(), elt->absBounds.y());
    
        if (elt->clipRect != currRect) {
            if (currRect != paintRect)
                p->restore(); // Pop the clip.
            currRect = elt->clipRect;
            if (currRect != paintRect) {
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
        
        elt->layer->renderer()->print(p, x, y, w, h,
                                      elt->absBounds.x() - elt->layer->renderer()->xPos(),
                                      elt->absBounds.y() - elt->layer->renderer()->yPos());
    }
    
    if (currRect != paintRect)
        p->restore(); // Pop the clip.
        
    delete node;
}

bool
RenderLayer::nodeAtPoint(RenderObject::NodeInfo& info, int x, int y)
{
    bool inside = false;
    RenderLayer::RenderZTreeNode* node = constructZTree(QRect(x, y, 0, 0), this, true);
    if (!node)
        return false;

    // Flatten the tree into a back-to-front list for painting.
    QPtrVector<RenderLayer::RenderLayerElement> layerList;
    constructLayerList(node, &layerList);

    // Walk the list and test each layer, adding in the appropriate offset.
    uint count = layerList.count();
    for (int i = count-1; i >= 0; i--) {
        RenderLayer::RenderLayerElement* elt = layerList.at(i);

        // Elements add in their own positions as a translation factor.  This forces
        // us to subtract that out, so that when it's added back in, we get the right
        // bounds.  This is really disgusting (that print only sets up the right paint
        // position after you call into it). -dwh
        //printf("Painting layer at %d %d\n", elt->absBounds.x(), elt->absBounds.y());

        inside = elt->layer->renderer()->nodeAtPoint(info, x, y,
                                      elt->absBounds.x() - elt->layer->renderer()->xPos(),
                                      elt->absBounds.y() - elt->layer->renderer()->yPos());
        if (inside)
            break;
    }
    delete node;

    return inside;
}

RenderLayer::RenderZTreeNode*
RenderLayer::constructZTree(QRect damageRect, 
                            RenderLayer* rootLayer,
                            bool eventProcessing)
{
    // This variable stores the result we will hand back.
    RenderLayer::RenderZTreeNode* returnNode = 0;
    
    // If a layer isn't visible, then none of its child layers are visible either.
    // Don't build this branch of the z-tree, since these layers should not be painted.
    if (renderer()->style()->visibility() != VISIBLE)
        return 0;
    
    // Compute this layer's absolute position, so that we can compare it with our
    // damage rect and avoid repainting the layer if it falls outside that rect.
    // An exception to this rule is the root layer, which always paints (hence the
    // m_parent null check below).
    int x = 0;
    int y = 0;
    convertToLayerCoords(rootLayer, x, y);
    QRect layerBounds(x, y, width(), height());
     
    returnNode = new RenderLayer::RenderZTreeNode(this);

    // If we establish a clip rect, then we want to intersect that rect
    // with the damage rect to form a new damage rect.
    if (m_object->style()->overflow() == OHIDDEN || m_object->style()->hasClip()) {
        QRect clipRect = m_object->getClipRect(x, y);
        if (!clipRect.intersects(damageRect))
            return 0; // We don't overlap at all. 
        damageRect = damageRect.intersect(clipRect);
    }
    
    // Walk our list of child layers looking only for those layers that have a 
    // non-negative z-index (a z-index >= 0).
    RenderZTreeNode* lastChildNode = 0;
    for (RenderLayer* child = firstChild(); child; child = child->nextSibling()) {
        if (child->zIndex() < 0)
            continue; // Ignore negative z-indices in this first pass.

        RenderZTreeNode* childNode = child->constructZTree(damageRect, rootLayer, eventProcessing);
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
        (renderer()->isInline() && !renderer()->isReplaced()) ||
        (eventProcessing && layerBounds.contains(x,y)) ||
        (!eventProcessing && layerBounds.intersects(damageRect))) {
        RenderLayerElement* layerElt = new RenderLayerElement(this, layerBounds, 
                                                              damageRect, x, y);
        if (returnNode->child) {
            RenderZTreeNode* leaf = new RenderZTreeNode(layerElt);
            leaf->next = returnNode->child;
            returnNode->child = leaf;
        }
        else
            returnNode->layerElement = layerElt;
    }
    
    // Now look for children that have a negative z-index.
    for (RenderLayer* child = firstChild(); child; child = child->nextSibling()) {
        if (child->zIndex() >= 0)
            continue; // Ignore non-negative z-indices in this second pass.

        RenderZTreeNode* childNode = child->constructZTree(damageRect, rootLayer, eventProcessing);
        if (childNode) {
            // Deal with the case where all our children views had negative z-indices.
            // Demote our leaf node and make a new interior node that can hold these
            // children.
            if (returnNode->layerElement) {
                RenderZTreeNode* leaf = returnNode;
                returnNode = new RenderLayer::RenderZTreeNode(this);
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
RenderLayer::constructLayerList(RenderZTreeNode* ztree, QPtrVector<RenderLayer::RenderLayerElement>* result)
{
    // This merge buffer is just a temporary used during computation as we do merge sorting.
    QPtrVector<RenderLayer::RenderLayerElement> mergeBuffer;
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
        layerElement->zindex = explicitZIndex;
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
        RenderLayer::RenderLayerElement* elt = buffer->at(i);
        elt->zindex = explicitZIndex;
    }
}
