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

class QScrollBar;
template <class T> class QPtrVector;

namespace khtml {
    class RenderStyle;
    class RenderTable;
    class CachedObject;
    class RenderCanvas;
    class RenderText;
    class RenderFrameSet;
    class RenderObject;
    class RenderScrollMediator;
    
class RenderScrollMediator: public QObject
{
public:
    RenderScrollMediator(RenderLayer* layer)
    :m_layer(layer) {}

    void slotValueChanged(int);
    
private:
    RenderLayer* m_layer;
};

class ClipRects
{
public:
    ClipRects(const QRect& r) :m_overflowClipRect(r), m_fixedClipRect(r), m_posClipRect(r), m_refCnt(0) {}
    ClipRects(const QRect& o, const QRect& f, const QRect& p)
      :m_overflowClipRect(o), m_fixedClipRect(f), m_posClipRect(p), m_refCnt(0) {}

    const QRect& overflowClipRect() { return m_overflowClipRect; }
    const QRect& fixedClipRect() { return m_fixedClipRect; }
    const QRect& posClipRect() { return m_posClipRect; }

    void ref() { m_refCnt++; }
    void deref(RenderArena* renderArena) { if (--m_refCnt == 0) detach(renderArena); }
    
    void detach(RenderArena* renderArena);

    // Overloaded new operator.
    void* operator new(size_t sz, RenderArena* renderArena) throw();    

    // Overridden to prevent the normal delete from being called.
    void operator delete(void* ptr, size_t sz);
        
private:
    // The normal operator new is disallowed on all render objects.
    void* operator new(size_t sz) throw();

private:
    QRect m_overflowClipRect;
    QRect m_fixedClipRect;
    QRect m_posClipRect;
    uint m_refCnt;
};

// This class handles the auto-scrolling of layers with overflow: marquee.
class Marquee: public QObject
{
    Q_OBJECT
    
public:
    Marquee(RenderLayer* l);

    void timerEvent(QTimerEvent*);

    int speed() const { return m_speed; }
    int marqueeSpeed() const;
    EMarqueeDirection direction() const;
    EMarqueeDirection reverseDirection() const { return static_cast<EMarqueeDirection>(-direction()); }
    bool isHorizontal() const;
    bool isUnfurlMarquee() const;
    int unfurlPos() const { return m_unfurlPos; }

    EWhiteSpace whiteSpace() { return m_whiteSpace; }
    
    int computePosition(EMarqueeDirection dir, bool stopAtClientEdge);

    void setEnd(int end) { m_end = end; }
    
    void start();
    void suspend();
    void stop();

    void updateMarqueeStyle();
    void updateMarqueePosition();

private:
    RenderLayer* m_layer;
    int m_currentLoop;
    int m_totalLoops;
    int m_timerId;
    int m_start;
    int m_end;
    int m_speed;
    int m_unfurlPos;
    Length m_height;
    bool m_reset: 1;
    bool m_suspended : 1;
    bool m_stopped : 1;
    EWhiteSpace m_whiteSpace : 2;
    EMarqueeDirection m_direction : 4;
};

class RenderLayer
{
public:
#ifdef APPLE_CHANGES
    static QScrollBar* gScrollBar;
#endif
    
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

    void removeOnlyThisLayer();
    void insertOnlyThisLayer();

    void repaintIncludingDescendants();
    
    void styleChanged();
    
    Marquee* marquee() const { return m_marquee; }
    void suspendMarquees();

#if APPLE_CHANGES
    bool isTransparent();
    RenderLayer* transparentAncestor();
    void beginTransparencyLayers(QPainter* p);
#endif
    
    RenderLayer* root() {
        RenderLayer* curr = this;
        while (curr->parent()) curr = curr->parent();
        return curr;
    }
    
    int xPos() const { return m_x; }
    int yPos() const { return m_y; }
    int width() const { return m_width; }
    int height() const { return m_height; }

    void setWidth(int w) { m_width = w; }
    void setHeight(int h) { m_height = h; }

    int scrollWidth();
    int scrollHeight();
    
    void setPos( int xPos, int yPos ) {
        m_x = xPos;
        m_y = yPos;
    }

    // Scrolling methods for layers that can scroll their overflow.
    void scrollOffset(int& x, int& y);
    void subtractScrollOffset(int& x, int& y);
    int scrollXOffset() const { return m_scrollX; }
    int scrollYOffset() const { return m_scrollY; }
    void scrollToOffset(int x, int y, bool updateScrollbars = true, bool repaint = true);
    void scrollToXOffset(int x) { scrollToOffset(x, m_scrollY); }
    void scrollToYOffset(int y) { scrollToOffset(m_scrollX, y); }
    void setHasHorizontalScrollbar(bool hasScrollbar);
    void setHasVerticalScrollbar(bool hasScrollbar);
    QScrollBar* horizontalScrollbar() { return m_hBar; }
    QScrollBar* verticalScrollbar() { return m_vBar; }
    int verticalScrollbarWidth();
    int horizontalScrollbarHeight();
    void moveScrollbarsAside();
    void positionScrollbars(const QRect& absBounds);
#ifdef APPLE_CHANGES
    void paintScrollbars(QPainter* p, const QRect& damageRect);
#endif
    void updateScrollInfoAfterLayout();
    void slotValueChanged(int);
    void updateScrollPositionFromScrollbars();
    bool scroll(KWQScrollDirection direction, KWQScrollGranularity granularity, float multiplier=1.0);
    
    void updateLayerPosition();
    void updateLayerPositions(bool doFullRepaint = false, bool checkForRepaint=true);
    void computeRepaintRects();
    void relativePositionOffset(int& relX, int& relY) {
        relX += m_relX; relY += m_relY;
    }
     
    void clearClipRects();
    void clearClipRect();

    // Get the enclosing stacking context for this layer.  A stacking context is a layer
    // that has a non-auto z-index.
    RenderLayer* stackingContext() const;
    bool isStackingContext() const { return !hasAutoZIndex() || renderer()->isCanvas(); }

    void dirtyZOrderLists();
    void updateZOrderLists();
    QPtrVector<RenderLayer>* posZOrderList() const { return m_posZOrderList; }
    QPtrVector<RenderLayer>* negZOrderList() const { return m_negZOrderList; }
    
    // Gets the nearest enclosing positioned ancestor layer (also includes
    // the <html> layer and the root layer).
    RenderLayer* enclosingPositionedAncestor() const;
    
    void convertToLayerCoords(const RenderLayer* ancestorLayer, int& x, int& y) const;
    
    bool hasAutoZIndex() const { return renderer()->style()->hasAutoZIndex(); }
    int zIndex() const { return renderer()->style()->zIndex(); }

    // The two main functions that use the layer system.  The paint method
    // paints the layers that intersect the damage rect from back to
    // front.  The hitTest method looks for mouse events by walking
    // layers that intersect the point from front to back.
    void paint(QPainter *p, const QRect& damageRect, bool selectionOnly=false, RenderObject *paintingRoot=0);
    bool hitTest(RenderObject::NodeInfo& info, int x, int y);

    // This method figures out our layerBounds in coordinates relative to
    // |rootLayer}.  It also computes our background and foreground clip rects
    // for painting/event handling.
    void calculateRects(const RenderLayer* rootLayer, const QRect& paintDirtyRect, QRect& layerBounds,
                        QRect& backgroundRect, QRect& foregroundRect);
    void calculateClipRects(const RenderLayer* rootLayer);
    ClipRects* clipRects() const { return m_clipRects; }

    bool intersectsDamageRect(const QRect& layerBounds, const QRect& damageRect) const;
    bool containsPoint(int x, int y, const QRect& damageRect) const;
    
    void updateHoverActiveState(RenderObject::NodeInfo& info);
    
    QRect repaintRect() const { return m_repaintRect; }

    void detach(RenderArena* renderArena);

     // Overloaded new operator.  Derived classes must override operator new
    // in order to allocate out of the RenderArena.
    void* operator new(size_t sz, RenderArena* renderArena) throw();    

    // Overridden to prevent the normal delete from being called.
    void operator delete(void* ptr, size_t sz);
        
private:
    // The normal operator new is disallowed on all render objects.
    void* operator new(size_t sz) throw();

private:
    void setNextSibling(RenderLayer* next) { m_next = next; }
    void setPreviousSibling(RenderLayer* prev) { m_previous = prev; }
    void setParent(RenderLayer* parent) { m_parent = parent; }
    void setFirstChild(RenderLayer* first) { m_first = first; }
    void setLastChild(RenderLayer* last) { m_last = last; }

    void collectLayers(QPtrVector<RenderLayer>*&, QPtrVector<RenderLayer>*&);

    void paintLayer(RenderLayer* rootLayer, QPainter *p, const QRect& paintDirtyRect, 
                    bool haveTransparency, bool selectionOnly, RenderObject *paintingRoot);
    RenderLayer* hitTestLayer(RenderLayer* rootLayer, RenderObject::NodeInfo& info,
                              int x, int y, const QRect& hitTestRect);
    void computeScrollDimensions(bool* needHBar = 0, bool* needVBar = 0);

protected:   
    RenderObject* m_object;
    
    RenderLayer* m_parent;
    RenderLayer* m_previous;
    RenderLayer* m_next;

    RenderLayer* m_first;
    RenderLayer* m_last;

    QRect m_repaintRect; // Cached repaint rects. Used by layout.
    QRect m_fullRepaintRect;

    // Our current relative position offset.
    int m_relX;
    int m_relY;

    // Our (x,y) coordinates are in our parent layer's coordinate space.
    int m_x;
    int m_y;

    // The layer's width/height
    int m_width;
    int m_height;
    
    // Our scroll offsets if the view is scrolled.
    int m_scrollX;
    int m_scrollY;
    
    // The width/height of our scrolled area.
    int m_scrollWidth;
    int m_scrollHeight;
    
    // For layers with overflow, we have a pair of scrollbars.
    QScrollBar* m_hBar;
    QScrollBar* m_vBar;
    RenderScrollMediator* m_scrollMediator;

    // For layers that establish stacking contexts, m_posZOrderList holds a sorted list of all the
    // descendant layers within the stacking context that have z-indices of 0 or greater
    // (auto will count as 0).  m_negZOrderList holds descendants within our stacking context with negative
    // z-indices.
    QPtrVector<RenderLayer>* m_posZOrderList;
    QPtrVector<RenderLayer>* m_negZOrderList;
    
    ClipRects* m_clipRects;      // Cached clip rects used when painting and hit testing.

    bool m_scrollDimensionsDirty : 1;
    bool m_zOrderListsDirty : 1;

#if APPLE_CHANGES
    bool m_usedTransparency : 1; // Tracks whether we need to close a transparent layer, i.e., whether
                                 // we ended up painting this layer or any descendants (and therefore need to
                                 // blend).
#endif

    Marquee* m_marquee; // Used by layers with overflow:marquee
};

}; // namespace
#endif
