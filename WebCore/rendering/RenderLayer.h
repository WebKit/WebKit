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

#ifndef RenderLayer_h
#define RenderLayer_h

#include "RenderObject.h"
#include "ScrollBar.h"
#include "Timer.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

class AffineTransform;
class CachedResource;
class HitTestResult;
class PlatformScrollbar;
class RenderFrameSet;
class RenderObject;
class RenderStyle;
class RenderTable;
class RenderText;
class RenderView;
class Scrollbar;

struct HitTestRequest;

class ClipRects {
public:
    ClipRects(const IntRect& r)
        : m_overflowClipRect(r)
        , m_fixedClipRect(r)
        , m_posClipRect(r)
        , m_refCnt(0)
        , m_fixed(false)
    {
    }

    ClipRects(const IntRect& overflowRect, const IntRect& fixedRect, const IntRect& posRect, bool fixed)
        : m_overflowClipRect(overflowRect)
        , m_fixedClipRect(fixedRect)
        , m_posClipRect(posRect)
        , m_refCnt(0)
        , m_fixed(fixed)
    {
    }

    const IntRect& overflowClipRect() { return m_overflowClipRect; }
    const IntRect& fixedClipRect() { return m_fixedClipRect; }
    const IntRect& posClipRect() { return m_posClipRect; }
    bool fixed() const { return m_fixed; }

    void ref() { m_refCnt++; }
    void deref(RenderArena* renderArena) { if (--m_refCnt == 0) destroy(renderArena); }

    void destroy(RenderArena*);

    // Overloaded new operator.
    void* operator new(size_t, RenderArena*) throw();

    // Overridden to prevent the normal delete from being called.
    void operator delete(void*, size_t);
        
private:
    // The normal operator new is disallowed on all render objects.
    void* operator new(size_t) throw();

private:
    IntRect m_overflowClipRect;
    IntRect m_fixedClipRect;
    IntRect m_posClipRect;
    unsigned m_refCnt : 31;
    bool m_fixed : 1;
};


// FIXME: move this to its own file
// This class handles the auto-scrolling of layers with overflow: marquee.
class Marquee {
public:
    Marquee(RenderLayer*);

    int speed() const { return m_speed; }
    int marqueeSpeed() const;

    EMarqueeDirection reverseDirection() const { return static_cast<EMarqueeDirection>(-direction()); }
    EMarqueeDirection direction() const;

    bool isHorizontal() const;

    int computePosition(EMarqueeDirection, bool stopAtClientEdge);

    void setEnd(int end) { m_end = end; }
    
    void start();
    void suspend();
    void stop();

    void updateMarqueeStyle();
    void updateMarqueePosition();

private:
    void timerFired(Timer<Marquee>*);

    RenderLayer* m_layer;
    int m_currentLoop;
    int m_totalLoops;
    Timer<Marquee> m_timer;
    int m_start;
    int m_end;
    int m_speed;
    Length m_height;
    bool m_reset: 1;
    bool m_suspended : 1;
    bool m_stopped : 1;
    EMarqueeDirection m_direction : 4;
};

class RenderLayer : public ScrollbarClient {
public:
    enum ScrollBehavior {
        noScroll,
        alignCenter,
        alignTop,
        alignBottom, 
        alignLeft,
        alignRight,
        alignToClosestEdge
    };

    struct ScrollAlignment {
        ScrollBehavior m_rectVisible;
        ScrollBehavior m_rectHidden;
        ScrollBehavior m_rectPartial;
    };

    static const ScrollAlignment gAlignCenterIfNeeded;
    static const ScrollAlignment gAlignToEdgeIfNeeded;
    static const ScrollAlignment gAlignCenterAlways;
    static const ScrollAlignment gAlignTopAlways;
    static const ScrollAlignment gAlignBottomAlways;

    static ScrollBehavior getVisibleBehavior(const ScrollAlignment& s) { return s.m_rectVisible; }
    static ScrollBehavior getPartialBehavior(const ScrollAlignment& s) { return s.m_rectPartial; }
    static ScrollBehavior getHiddenBehavior(const ScrollAlignment& s) { return s.m_rectHidden; }

    RenderLayer(RenderObject*);
    ~RenderLayer();

    RenderObject* renderer() const { return m_object; }
    RenderLayer* parent() const { return m_parent; }
    RenderLayer* previousSibling() const { return m_previous; }
    RenderLayer* nextSibling() const { return m_next; }
    RenderLayer* firstChild() const { return m_first; }
    RenderLayer* lastChild() const { return m_last; }

    void addChild(RenderLayer* newChild, RenderLayer* beforeChild = 0);
    RenderLayer* removeChild(RenderLayer*);

    void removeOnlyThisLayer();
    void insertOnlyThisLayer();

    void repaintIncludingDescendants();

    void styleChanged();

    Marquee* marquee() const { return m_marquee; }
    void suspendMarquees();

    bool isOverflowOnly() const { return m_isOverflowOnly; }

    bool isTransparent() const;
    RenderLayer* transparentAncestor();
    void beginTransparencyLayers(GraphicsContext*, const RenderLayer* rootLayer);

    const RenderLayer* root() const
    {
        const RenderLayer* curr = this;
        while (curr->parent())
            curr = curr->parent();
        return curr;
    }
    
    int xPos() const { return m_x; }
    int yPos() const { return m_y; }
    void setPos(int xPos, int yPos)
    {
        m_x = xPos;
        m_y = yPos;
    }

    int width() const { return m_width; }
    int height() const { return m_height; }
    void setWidth(int w) { m_width = w; }
    void setHeight(int h) { m_height = h; }

    int scrollWidth();
    int scrollHeight();

    // Scrolling methods for layers that can scroll their overflow.
    void scrollOffset(int& x, int& y);
    void subtractScrollOffset(int& x, int& y);
    int scrollXOffset() const { return m_scrollX + m_scrollOriginX; }
    int scrollYOffset() const { return m_scrollY; }
    void scrollToOffset(int x, int y, bool updateScrollbars = true, bool repaint = true);
    void scrollToXOffset(int x) { scrollToOffset(x, m_scrollY); }
    void scrollToYOffset(int y) { scrollToOffset(m_scrollX + m_scrollOriginX, y); }
    void scrollRectToVisible(const IntRect&, const ScrollAlignment& alignX = gAlignCenterIfNeeded, const ScrollAlignment& alignY = gAlignCenterIfNeeded);

    IntRect getRectToExpose(const IntRect& visibleRect, const IntRect& exposeRect, const ScrollAlignment& alignX, const ScrollAlignment& alignY);    

    void setHasHorizontalScrollbar(bool);
    void setHasVerticalScrollbar(bool);

    PassRefPtr<Scrollbar> createScrollbar(ScrollbarOrientation);
    void destroyScrollbar(ScrollbarOrientation);

    Scrollbar* horizontalScrollbar() { return m_hBar.get(); }
    Scrollbar* verticalScrollbar() { return m_vBar.get(); }

    PlatformScrollbar* horizontalScrollbarWidget() const;
    PlatformScrollbar* verticalScrollbarWidget() const;

    int verticalScrollbarWidth() const;
    int horizontalScrollbarHeight() const;

    void positionOverflowControls();
    bool isPointInResizeControl(const IntPoint&);
    bool hitTestOverflowControls(HitTestResult&);
    IntSize offsetFromResizeCorner(const IntPoint&) const;

    void paintOverflowControls(GraphicsContext*, int tx, int ty, const IntRect& damageRect);

    void updateScrollInfoAfterLayout();

    bool scroll(ScrollDirection, ScrollGranularity, float multiplier = 1.0f);
    void autoscroll();

    void resize(const PlatformMouseEvent&, const IntSize&);
    bool inResizeMode() const { return m_inResizeMode; }
    void setInResizeMode(bool b) { m_inResizeMode = b; }
    
    void updateLayerPosition();
    void updateLayerPositions(bool doFullRepaint = false, bool checkForRepaint = true);

    void updateTransform();

    void relativePositionOffset(int& relX, int& relY) { relX += m_relX; relY += m_relY; }

    void clearClipRects();
    void clearClipRect();

    // Get the enclosing stacking context for this layer.  A stacking context is a layer
    // that has a non-auto z-index.
    RenderLayer* stackingContext() const;
    bool isStackingContext() const { return !hasAutoZIndex() || renderer()->isRenderView(); }

    void dirtyZOrderLists();
    void updateZOrderLists();
    Vector<RenderLayer*>* posZOrderList() const { return m_posZOrderList; }
    Vector<RenderLayer*>* negZOrderList() const { return m_negZOrderList; }

    void dirtyOverflowList();
    void updateOverflowList();
    Vector<RenderLayer*>* overflowList() const { return m_overflowList; }

    bool hasVisibleContent() const { return m_hasVisibleContent; }
    void setHasVisibleContent(bool);
    void dirtyVisibleContentStatus();

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
    void paint(GraphicsContext*, const IntRect& damageRect, PaintRestriction = PaintRestrictionNone, RenderObject* paintingRoot = 0);
    bool hitTest(const HitTestRequest&, HitTestResult&);

    // This method figures out our layerBounds in coordinates relative to
    // |rootLayer}.  It also computes our background and foreground clip rects
    // for painting/event handling.
    void calculateRects(const RenderLayer* rootLayer, const IntRect& paintDirtyRect, IntRect& layerBounds,
                        IntRect& backgroundRect, IntRect& foregroundRect, IntRect& outlineRect) const;
    void calculateClipRects(const RenderLayer* rootLayer);
    ClipRects* clipRects() const { return m_clipRects; }
    IntRect childrenClipRect() const; // Returns the foreground clip rect of the layer in the document's coordinate space.
    IntRect selfClipRect() const; // Returns the background clip rect of the layer in the document's coordinate space.

    bool intersectsDamageRect(const IntRect& layerBounds, const IntRect& damageRect, const RenderLayer* rootLayer) const;

    // Returns a bounding box for this layer only.
    IntRect boundingBox(const RenderLayer* rootLayer) const;

    void updateHoverActiveState(const HitTestRequest&, HitTestResult&);

    IntRect repaintRect() const { return m_repaintRect; }
    void setNeedsFullRepaint(bool f = true) { m_needsFullRepaint = f; }
    
    int staticX() const { return m_staticX; }
    int staticY() const { return m_staticY; }
    void setStaticX(int staticX) { m_staticX = staticX; }
    void setStaticY(int staticY) { m_staticY = staticY; }

    AffineTransform* transform() const { return m_transform.get(); }

    void destroy(RenderArena*);

     // Overloaded new operator.  Derived classes must override operator new
    // in order to allocate out of the RenderArena.
    void* operator new(size_t, RenderArena*) throw();

    // Overridden to prevent the normal delete from being called.
    void operator delete(void*, size_t);

private:
    // The normal operator new is disallowed on all render objects.
    void* operator new(size_t) throw();

private:
    void setNextSibling(RenderLayer* next) { m_next = next; }
    void setPreviousSibling(RenderLayer* prev) { m_previous = prev; }
    void setParent(RenderLayer* parent) { m_parent = parent; }
    void setFirstChild(RenderLayer* first) { m_first = first; }
    void setLastChild(RenderLayer* last) { m_last = last; }

    void collectLayers(Vector<RenderLayer*>*&, Vector<RenderLayer*>*&);

    void paintLayer(RenderLayer* rootLayer, GraphicsContext*, const IntRect& paintDirtyRect,
                    bool haveTransparency, PaintRestriction, RenderObject* paintingRoot);
    RenderLayer* hitTestLayer(RenderLayer* rootLayer, const HitTestRequest&, HitTestResult&, const IntRect& hitTestRect, const IntPoint& hitTestPoint);
    void computeScrollDimensions(bool* needHBar = 0, bool* needVBar = 0);

    bool shouldBeOverflowOnly() const;

    virtual void valueChanged(Scrollbar*);
    virtual IntRect windowClipRect() const;
    virtual bool isActive() const;

    void updateOverflowStatus(bool horizontalOverflow, bool verticalOverflow);

    void childVisibilityChanged(bool newVisibility);
    void dirtyVisibleDescendantStatus();
    void updateVisibilityStatus();

    Node* enclosingElement() const;

protected:   
    RenderObject* m_object;

    RenderLayer* m_parent;
    RenderLayer* m_previous;
    RenderLayer* m_next;
    RenderLayer* m_first;
    RenderLayer* m_last;

    IntRect m_repaintRect; // Cached repaint rects. Used by layout.
    IntRect m_outlineBox;

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
    int m_scrollOriginX;
    int m_scrollLeftOverflow;

    // The width/height of our scrolled area.
    int m_scrollWidth;
    int m_scrollHeight;

    // For layers with overflow, we have a pair of scrollbars.
    RefPtr<Scrollbar> m_hBar;
    RefPtr<Scrollbar> m_vBar;

    // Keeps track of whether the layer is currently resizing, so events can cause resizing to start and stop.
    bool m_inResizeMode;

    // For layers that establish stacking contexts, m_posZOrderList holds a sorted list of all the
    // descendant layers within the stacking context that have z-indices of 0 or greater
    // (auto will count as 0).  m_negZOrderList holds descendants within our stacking context with negative
    // z-indices.
    Vector<RenderLayer*>* m_posZOrderList;
    Vector<RenderLayer*>* m_negZOrderList;

    // This list contains our overflow child layers.
    Vector<RenderLayer*>* m_overflowList;

    ClipRects* m_clipRects;      // Cached clip rects used when painting and hit testing.

    bool m_scrollDimensionsDirty : 1;
    bool m_zOrderListsDirty : 1;
    bool m_overflowListDirty: 1;
    bool m_isOverflowOnly: 1;

    bool m_usedTransparency : 1; // Tracks whether we need to close a transparent layer, i.e., whether
                                 // we ended up painting this layer or any descendants (and therefore need to
                                 // blend).
    bool m_inOverflowRelayout : 1;
    bool m_needsFullRepaint : 1;

    bool m_overflowStatusDirty : 1;
    bool m_horizontalOverflow : 1;
    bool m_verticalOverflow : 1;
    bool m_visibleContentStatusDirty : 1;
    bool m_hasVisibleContent : 1;
    bool m_visibleDescendantStatusDirty : 1;
    bool m_hasVisibleDescendant : 1;

    Marquee* m_marquee; // Used by layers with overflow:marquee
    
    // Cached normal flow values for absolute positioned elements with static left/top values.
    int m_staticX;
    int m_staticY;
    
    OwnPtr<AffineTransform> m_transform;
};

} // namespace WebCore

#endif // RenderLayer_h
