/*
 * This file contains the implementation of the layering system for the compositing
 * of stacked layers (and for proper event handling with stacked layers).
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
 
#ifndef render_layer_h
#define render_layer_h

#include <qcolor.h>
#include <qrect.h>
#include <assert.h>

#include "render_object.h"
#include <qvector.h>

namespace khtml {
    class RenderFlow;
    class RenderStyle;
    class RenderTable;
    class CachedObject;
    class RenderRoot;
    class RenderText;
    class RenderFrameSet;
    class RenderObject;
    
class RenderLayer
{
public:
    RenderLayer(RenderObject* object);
    ~RenderLayer();
    
    RenderObject* renderer() const { return m_object; }
    RenderLayer *parent() const { return m_parent; }
    RenderLayer *previousSibling() const { return m_previous; }
    RenderLayer *nextSibling() const { return m_next; }

    RenderLayer *firstChild() const { return m_first; }
    RenderLayer *lastChild() const { return m_last; }

    void addChild(RenderLayer *newChild);
    RenderLayer* removeChild(RenderLayer *oldChild);

    RenderLayer* root() {
        RenderLayer* curr = this;
        while (curr->parent()) curr = curr->parent();
        return curr;
    }
    
    int xPos() const { return m_x; }
    int yPos() const { return m_y; }
    short width() const { return m_width; }
    int height() const { return m_height; }

    void setWidth( int width ) {
        m_width = width;
    }
    void setHeight( int height ) {
        m_height = height;
    }
    void setPos( int xPos, int yPos ) {
        m_x = xPos;
        m_y = yPos;
    }
    
    void updateLayerPosition();
    
    RenderLayer* enclosingAncestor();
    
    void convertToLayerCoords(RenderLayer* ancestorLayer, int& x, int& y);
    
    bool hasAutoZIndex() { return renderer()->style()->hasAutoZIndex(); }
    int zIndex() { return renderer()->style()->zIndex(); }

    void paint(QPainter *p, int x, int y, int w, int h, int tx, int ty);
    
public:
    // Z-Index Implementation Notes
    //
    // In order to properly handle mouse events as well as painting,
    // we must compute a correct list of layers that should be painted
    // from back to front (and for mouse events walked from front to
    // back).
    //
    // Positioned elements in the render tree (e.g., relative positioned
    // divs and absolute positioned divs) have a corresponding layer
    // that holds them and all children that reside in the same layer.
    //
    // When painting is performed on a layer, all render objects in that
    // layer are painted.  If the render object has descendants in another
    // layer, those will be dealt with separately.
    //
    // A RenderLayerElement represents a single entry in our list of
    // layers that should be painted.  We perform computations as we
    // build up this list so that we have the correct translation factor
    // for painting.  We also use a temporary z-index variable for storage
    // (more on this below).
    // 
    struct RenderLayerElement {
      RenderLayer* layer;
      QRect absBounds; // Our bounds in absolute coordinates relative to the root.
      int zindex; // Temporary z-index used for processing and sorting.
      bool zauto; // Whether or not we are using auto z-indexing.
      int x; // The coords relative to the layer that will be using this list
             // to paint.
      int y;

      RenderLayerElement(RenderLayer* l, const QRect& rect, int xpos, int ypos)
          :layer(l), absBounds(rect), zindex(l->zIndex()), zauto(l->hasAutoZIndex()),
          x(xpos), y(ypos) {}
    };

    // The list of layer elements is built through a recursive examination
    // of a tree of z nodes. This tree structure mimics the layer 
    // hierarchy itself, but only leaf nodes represent items that will
    // end up in the layer list for painting.
    //
    // Every leaf layer in the layer hierarchy will have a corresponding
    // leaf node in the z-tree.  Layers with children have an
    // interior z-tree node that contains the tree nodes for the child
    // layers as well as a leaf node that represents the containing layer.
    //
    // Sibling z-tree nodes match the same order as the layers in the
    // layer hierarchy, which will have been arranged in document order
    // when the render tree was constructed (since the render tree
    // constructed the layers).  An exception is if a negative z-index
    // is specified on a child (see below).
    
    struct RenderZTreeNode {
      RenderLayer* layer;
      RenderZTreeNode* next;

      // Only one of these will ever be defined.
      RenderZTreeNode* child; // Defined for interior nodes.
      RenderLayerElement* layerElement; // Defined for leaf nodes.

      RenderZTreeNode(RenderLayer* l)
          :layer(l), next(0), child(0), layerElement(0) {}

      RenderZTreeNode(RenderLayerElement* layerElt)
          :layer(layerElt->layer), next(0), child(0), layerElement(layerElt) {}
      
      ~RenderZTreeNode() { delete next; delete child; delete layerElement; }

      void constructLayerList(QPtrVector<RenderLayerElement>* mergeTmpBuffer,
                              QPtrVector<RenderLayerElement>* finalBuffer);
      
    };
      
private:
    // The createZTree function creates a z-tree for a given layer hierarchy
    // rooted on this layer.  It will ensure that immediate child
    // elements of a given z-tree node are at least initially sorted
    // into <negative z-index children>, <this layer>, <non-negative z-index
    // children>.
    //
    // Here is a concrete example (lifted from Gecko's view system,
    // which is analogous to our layer system and works the same way):
    // z-index values as specified by CSS are shown in parentheses.
    //
    // L0(auto) --> L1(0) --> L2(auto) --> L3(0)
    // |        |    +------> L4(2)
    // |        +-----------> L5(1)
    // +--------------------> L6(1)
    //
    // The corresponding z-tree for this layer hierarchy will be
    // the following, where |I| represents an interior node, and |L|
    // represents a leaf RenderLayerElement.
    //
    // I(L0) --> L(L0)
    // +-------> I(L1) --------> L(L1)
    // |           |   +-------> I(L2) ------> L(L2)
    // |           |               +---------> L(L3)
    // |           +-----------> L(L4)
    // +-------> L(L5)
    // +-------> L(L6)
    //
    RenderZTreeNode* constructZTree(const QRect& damageRect, 
                                    RenderLayer* rootLayer);

    // Once the z-tree has been constructed, we call constructLayerList
    // to produce a flattened layer list for rendering/event handling.
    // This function recursively computes a layer list for each z-tree
    // node by computing lists for each child node.  It then concatenates
    // them and sorts them by z-index.
    //
    // Z-indices are updated during this computation.  After a list is
    // computed for one z-tree node, the elements of the layer list are
    // all changed so that their z-indices match the specified z-index
    // of the tree node's layer (unless that layer doesn't establish
    // a z-index, e.g., it just has z-index: auto).
    //
    // Continuing the above example, the computation of the list for
    // L0 would be as follows:
    //
    // I(L2) has a list [ L(L2)(0), L(L3)(0), L(L4)(2) ]
    // I(L2) is auto so the z-indices of the child layer elements remain
    // unaltered.
    // I(L1) has a list [ L(L1)(0), L(L2)(0), L(L3)(0), L(L4)(2), L(L5)(1) ]
    // The nodes are sorted and then reassigned a z-index of 0, so this
    // list becomes:
    // [ L(L1)(0), L(L2)(0), L(L3)(0), L(L5)(0), L(L4)(0) ]
    // Finally we end up with the list for L0, which sorted becomes:
    // [ L(L0)(0), L(L1)(0), L(L2)(0), L(L3)(0), L(L5)(0), L(L4)(0), L(L6)(1) ]
    void constructLayerList(RenderZTreeNode* ztree,
                            QPtrVector<RenderLayerElement>* result);
    
private:
    void setNextSibling(RenderLayer* next) { m_next = next; }
    void setPreviousSibling(RenderLayer* prev) { m_previous = prev; }
    void setParent(RenderLayer* parent) { m_parent = parent; }
    void setFirstChild(RenderLayer* first) { m_first = first; }
    void setLastChild(RenderLayer* last) { m_last = last; }

protected:   
    RenderObject* m_object;
    
    RenderLayer *m_parent;
    RenderLayer *m_previous;
    RenderLayer *m_next;

    RenderLayer *m_first;
    RenderLayer *m_last;
    
    // Our (x,y) coordinates are in our parent layer's coordinate space.
    int m_height;
    int m_y;
    short m_x;
    short m_width;
};

}; // namespace
#endif
