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

#ifndef render_layer_h
#define render_layer_h

#include <qcolor.h>
#include <qrect.h>
#include <assert.h>

#include "render_object.h"
#include <qvector.h>
#include <qscrollbar.h>

namespace khtml {
    class RenderStyle;
    class RenderTable;
    class CachedObject;
    class RenderCanvas;
    class RenderText;
    class RenderFrameSet;
    class RenderObject;

class RenderScrollMediator: public QObject
{
public:
    RenderScrollMediator(RenderLayer* layer)
    :m_layer(layer) {}

    void slotValueChanged(int);
    
private:
    RenderLayer* m_layer;
};
    
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

    void addChild(RenderLayer *newChild, RenderLayer* beforeChild = 0);
    RenderLayer* removeChild(RenderLayer *oldChild);

    RenderLayer* transparentAncestor();
    bool isTransparent();
    void updateTransparentState(QPainter* painter, RenderLayer* newLayer, RenderLayer*& currLayer);
    void beginTransparencyLayers(QPainter* painter, RenderLayer* newLayer, RenderLayer* ancestorLayer);
    void endTransparencyLayers(QPainter* painter, RenderLayer* newLayer, RenderLayer* ancestorLayer);
    
    void removeOnlyThisLayer();
    void insertOnlyThisLayer();
    
    RenderLayer* root() {
        RenderLayer* curr = this;
        while (curr->parent()) curr = curr->parent();
        return curr;
    }
    
    int xPos() const { return m_x; }
    int yPos() const { return m_y; }
    short width() const { return m_width; }
    int height() const { return m_height; }
    short scrollWidth() const { return m_scrollWidth; }
    int scrollHeight() const { return m_scrollHeight; }
    
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

    // Scrolling methods for layers that can scroll their overflow.
    void scrollOffset(int& x, int& y);
    void subtractScrollOffset(int& x, int& y);
    short scrollXOffset() { return m_scrollX; }
    int scrollYOffset() { return m_scrollY; }
    void scrollToOffset(int x, int y, bool updateScrollbars = true);
    void scrollToXOffset(int x) { scrollToOffset(x, m_scrollY); }
    void scrollToYOffset(int y) { scrollToOffset(m_scrollX, y); }
    void setHasHorizontalScrollbar(bool hasScrollbar);
    void setHasVerticalScrollbar(bool hasScrollbar);
    QWidget* horizontalScrollbar() { return m_hBar; }
    QWidget* verticalScrollbar() { return m_vBar; }
    int verticalScrollbarWidth();
    int horizontalScrollbarHeight();
    void moveScrollbarsAside();
    void positionScrollbars(const QRect& absBounds);
    void paintScrollbars(QPainter* p, int x, int y, int w, int h);
    void checkScrollbarsAfterLayout();
    void slotValueChanged(int);
    void updateScrollPositionFromScrollbars();
    
    void updateLayerPosition();
    
    // Gets the nearest enclosing positioned ancestor layer (also includes
    // the <html> layer and the root layer).
    RenderLayer* enclosingPositionedAncestor();
    
    void convertToLayerCoords(RenderLayer* ancestorLayer, int& x, int& y);
    
    bool hasAutoZIndex() { return renderer()->style()->hasAutoZIndex(); }
    int zIndex() { return renderer()->style()->zIndex(); }

    // The two main functions that use the layer system.  The paint method
    // paints the layers that intersect the damage rect from back to
    // front.  The nodeAtPoint method looks for mouse events by walking
    // layers that intersect the point from front to back.
    void paint(QPainter *p, int x, int y, int w, int h, bool selectionOnly=false);
    bool nodeAtPoint(RenderObject::NodeInfo& info, int x, int y);
    
    void clearOtherLayersHoverActiveState();
    void clearHoverAndActiveState(RenderObject* obj);
    
    void detach(RenderArena* renderArena);
    
     // Overloaded new operator.  Derived classes must override operator new
    // in order to allocate out of the RenderArena.
    void* operator new(size_t sz, RenderArena* renderArena) throw();    

    // Overridden to prevent the normal delete from being called.
    void operator delete(void* ptr, size_t sz);
        
private:
    // The normal operator new is disallowed on all render objects.
    void* operator new(size_t sz) throw();

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
      enum LayerElementType { Normal, Background, Foreground };
        
      RenderLayer* layer;
      QRect absBounds; // Our bounds in absolute coordinates relative to the root.
      QRect backgroundClipRect; // Clip rect used for our background/borders. 
      QRect clipRect; // Clip rect used for our children.
      int zindex; // Temporary z-index used for processing and sorting.
      bool zauto : 1; // Whether or not we are using auto z-indexing.
      bool clipOriginator : 1; // Whether or not we established a clip.
      int x; // The coords relative to the layer that will be using this list
             // to paint.
      int y;
      LayerElementType layerElementType; // For negative z-indices, we have to split a single layer into two
                           // RenderLayerElements, one that sits beneath the negative content, and
                           // another that sits above (denoted with values of Background and Foreground,
                           // respectively).  A normal layer that fully paints is denoted with the value Normal.
      
      RenderLayerElement(RenderLayer* l, const QRect& rect, const QRect& bgclip, 
                         const QRect& clip, bool clipOrig, int xpos, int ypos,
                         LayerElementType lType = Normal)
          :layer(l), absBounds(rect), backgroundClipRect(bgclip), clipRect(clip), 
           zindex(l->zIndex()), zauto(l->hasAutoZIndex()), clipOriginator(clipOrig),
           x(xpos), y(ypos), layerElementType(lType) {}
          
      void detach(RenderArena* renderArena);
    
      // Overloaded new operator.  Derived classes must override operator new
      // in order to allocate out of the RenderArena.
      void* operator new(size_t sz, RenderArena* renderArena) throw();    

      // Overridden to prevent the normal delete from being called.
      void operator delete(void* ptr, size_t sz);
        
      // The normal operator new is disallowed.
      void* operator new(size_t sz) throw();
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
      
      ~RenderZTreeNode() {}

      void constructLayerList(QPtrVector<RenderLayerElement>* mergeTmpBuffer,
                              QPtrVector<RenderLayerElement>* finalBuffer);
      
      void detach(RenderArena* renderArena);
          
      // Overloaded new operator.  Derived classes must override operator new
      // in order to allocate out of the RenderArena.
      void* operator new(size_t sz, RenderArena* renderArena) throw();    

      // Overridden to prevent the normal delete from being called.
      void operator delete(void* ptr, size_t sz);
        
      // The normal operator new is disallowed.
      void* operator new(size_t sz) throw();
    };

    static QWidget* gScrollBar;

    // For debugging.
    QPtrVector<RenderLayerElement> elementList(RenderZTreeNode *&node);
      
private:
    // The constructZTree function creates a z-tree for a given layer hierarchy
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
    RenderZTreeNode* constructZTree(QRect overflowClipRect,
                                    QRect clipRect,
                                    RenderLayer* rootLayer,
                                    bool eventProcessing = false, int x=0, int y=0);

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

    // Our scroll offsets if the view is scrolled.
    short m_scrollX;
    int m_scrollY;

    // The width/height of our scrolled area.
    short m_scrollWidth;
    short m_scrollHeight;
    
    // For layers with overflow, we have a pair of scrollbars.
    QScrollBar* m_hBar;
    QScrollBar* m_vBar;
    RenderScrollMediator* m_scrollMediator;
};

}; // namespace
#endif
