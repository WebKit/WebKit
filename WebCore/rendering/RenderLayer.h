/*
 * Copyright (C) 2003, 2009 Apple Inc. All rights reserved.
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

#include "RenderBox.h"
#include "ScrollBehavior.h"
#include "ScrollbarClient.h"
#include "Timer.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

class CachedResource;
class HitTestRequest;
class HitTestResult;
class HitTestingTransformState;
class RenderFrameSet;
class RenderMarquee;
class RenderReplica;
class RenderScrollbarPart;
class RenderStyle;
class RenderTable;
class RenderText;
class RenderView;
class Scrollbar;
class TransformationMatrix;

#if USE(ACCELERATED_COMPOSITING)
class RenderLayerBacking;
class RenderLayerCompositor;
#endif

class ClipRects {
public:
    ClipRects()
        : m_refCnt(0)
        , m_fixed(false)
    {
    }

    ClipRects(const IntRect& r)
        : m_overflowClipRect(r)
        , m_fixedClipRect(r)
        , m_posClipRect(r)
        , m_refCnt(0)
        , m_fixed(false)
    {
    }

    ClipRects(const ClipRects& other)
        : m_overflowClipRect(other.overflowClipRect())
        , m_fixedClipRect(other.fixedClipRect())
        , m_posClipRect(other.posClipRect())
        , m_refCnt(0)
        , m_fixed(other.fixed())
    {
    }

    void reset(const IntRect& r)
    {
        m_overflowClipRect = r;
        m_fixedClipRect = r;
        m_posClipRect = r;
        m_fixed = false;
    }
    
    const IntRect& overflowClipRect() const { return m_overflowClipRect; }
    void setOverflowClipRect(const IntRect& r) { m_overflowClipRect = r; }

    const IntRect& fixedClipRect() const { return m_fixedClipRect; }
    void setFixedClipRect(const IntRect&r) { m_fixedClipRect = r; }

    const IntRect& posClipRect() const { return m_posClipRect; }
    void setPosClipRect(const IntRect& r) { m_posClipRect = r; }

    bool fixed() const { return m_fixed; }
    void setFixed(bool fixed) { m_fixed = fixed; }

    void ref() { m_refCnt++; }
    void deref(RenderArena* renderArena) { if (--m_refCnt == 0) destroy(renderArena); }

    void destroy(RenderArena*);

    // Overloaded new operator.
    void* operator new(size_t, RenderArena*) throw();

    // Overridden to prevent the normal delete from being called.
    void operator delete(void*, size_t);

    bool operator==(const ClipRects& other) const
    {
        return m_overflowClipRect == other.overflowClipRect() &&
               m_fixedClipRect == other.fixedClipRect() &&
               m_posClipRect == other.posClipRect() &&
               m_fixed == other.fixed();
    }

    ClipRects& operator=(const ClipRects& other)
    {
        m_overflowClipRect = other.overflowClipRect();
        m_fixedClipRect = other.fixedClipRect();
        m_posClipRect = other.posClipRect();
        m_fixed = other.fixed();
        return *this;
    }
    
    static IntRect infiniteRect() { return IntRect(INT_MIN/2, INT_MIN/2, INT_MAX, INT_MAX); }

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

class RenderLayer : public ScrollbarClient {
public:
    friend class RenderReplica;

    RenderLayer(RenderBoxModelObject*);
    ~RenderLayer();

    RenderBoxModelObject* renderer() const { return m_renderer; }
    RenderBox* renderBox() const { return m_renderer && m_renderer->isBox() ? toRenderBox(m_renderer) : 0; }
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

#if USE(ACCELERATED_COMPOSITING)
    // Indicate that the layer contents need to be repainted. Only has an effect
    // if layer compositing is being used,
    void setBackingNeedsRepaint();
    void setBackingNeedsRepaintInRect(const IntRect& r); // r is in the coordinate space of the layer's render object
    void repaintIncludingNonCompositingDescendants(RenderBoxModelObject* repaintContainer);
#endif

    void styleChanged(StyleDifference, const RenderStyle*);

    RenderMarquee* marquee() const { return m_marquee; }

    bool isNormalFlowOnly() const { return m_isNormalFlowOnly; }
    bool isSelfPaintingLayer() const;

    bool requiresSlowRepaints() const;

    bool isTransparent() const;
    RenderLayer* transparentPaintingAncestor();
    void beginTransparencyLayers(GraphicsContext*, const RenderLayer* rootLayer);

    bool hasReflection() const { return renderer()->hasReflection(); }
    RenderReplica* reflection() const { return m_reflection; }
    RenderLayer* reflectionLayer() const;

    const RenderLayer* root() const
    {
        const RenderLayer* curr = this;
        while (curr->parent())
            curr = curr->parent();
        return curr;
    }
    
    int x() const { return m_x; }
    int y() const { return m_y; }
    void setLocation(int x, int y)
    {
        m_x = x;
        m_y = y;
    }

    int width() const { return m_width; }
    int height() const { return m_height; }
    void setWidth(int w) { m_width = w; }
    void setHeight(int h) { m_height = h; }

    int scrollWidth();
    int scrollHeight();

    void panScrollFromPoint(const IntPoint&);

    // Scrolling methods for layers that can scroll their overflow.
    void scrollByRecursively(int xDelta, int yDelta);
    void addScrolledContentOffset(int& x, int& y) const;
    void subtractScrolledContentOffset(int& x, int& y) const;
    IntSize scrolledContentOffset() const { return IntSize(scrollXOffset() + m_scrollLeftOverflow, scrollYOffset()); }

    int scrollXOffset() const { return m_scrollX + m_scrollOriginX; }
    int scrollYOffset() const { return m_scrollY; }

    void scrollToOffset(int x, int y, bool updateScrollbars = true, bool repaint = true);
    void scrollToXOffset(int x) { scrollToOffset(x, m_scrollY); }
    void scrollToYOffset(int y) { scrollToOffset(m_scrollX + m_scrollOriginX, y); }
    void scrollRectToVisible(const IntRect&, bool scrollToAnchor = false, const ScrollAlignment& alignX = ScrollAlignment::alignCenterIfNeeded, const ScrollAlignment& alignY = ScrollAlignment::alignCenterIfNeeded);

    IntRect getRectToExpose(const IntRect& visibleRect, const IntRect& exposeRect, const ScrollAlignment& alignX, const ScrollAlignment& alignY);

    void setHasHorizontalScrollbar(bool);
    void setHasVerticalScrollbar(bool);

    PassRefPtr<Scrollbar> createScrollbar(ScrollbarOrientation);
    void destroyScrollbar(ScrollbarOrientation);

    Scrollbar* horizontalScrollbar() const { return m_hBar.get(); }
    Scrollbar* verticalScrollbar() const { return m_vBar.get(); }

    int verticalScrollbarWidth() const;
    int horizontalScrollbarHeight() const;

    void positionOverflowControls(int tx, int ty);
    bool isPointInResizeControl(const IntPoint& absolutePoint) const;
    bool hitTestOverflowControls(HitTestResult&, const IntPoint& localPoint);
    IntSize offsetFromResizeCorner(const IntPoint& absolutePoint) const;

    void paintOverflowControls(GraphicsContext*, int tx, int ty, const IntRect& damageRect);
    void paintScrollCorner(GraphicsContext*, int tx, int ty, const IntRect& damageRect);
    void paintResizer(GraphicsContext*, int tx, int ty, const IntRect& damageRect);

    void updateScrollInfoAfterLayout();

    bool scroll(ScrollDirection, ScrollGranularity, float multiplier = 1.0f);
    void autoscroll();

    void resize(const PlatformMouseEvent&, const IntSize&);
    bool inResizeMode() const { return m_inResizeMode; }
    void setInResizeMode(bool b) { m_inResizeMode = b; }

    bool isRootLayer() const { return renderer()->isRenderView(); }
    
#if USE(ACCELERATED_COMPOSITING)
    RenderLayerCompositor* compositor() const;
    
    // Notification from the renderer that its content changed (e.g. current frame of image changed).
    // Allows updates of layer content without repainting.
    void rendererContentChanged();
#endif

    // Returns true if the accelerated compositing is enabled
    bool hasAcceleratedCompositing() const;
    
    void updateLayerPosition();
    
    enum UpdateLayerPositionsFlag {
        DoFullRepaint = 1,
        CheckForRepaint = 1 << 1,
        IsCompositingUpdateRoot = 1 << 2,
        UpdateCompositingLayers = 1 << 3,
    };
    typedef unsigned UpdateLayerPositionsFlags;
    void updateLayerPositions(UpdateLayerPositionsFlags = DoFullRepaint | IsCompositingUpdateRoot | UpdateCompositingLayers);

    void updateTransform();

    void relativePositionOffset(int& relX, int& relY) const { relX += m_relX; relY += m_relY; }
    IntSize relativePositionOffset() const { return IntSize(m_relX, m_relY); }

    void clearClipRectsIncludingDescendants();
    void clearClipRects();

    // Get the enclosing stacking context for this layer.  A stacking context is a layer
    // that has a non-auto z-index.
    RenderLayer* stackingContext() const;
    bool isStackingContext() const { return !hasAutoZIndex() || renderer()->isRenderView(); }

    void dirtyZOrderLists();
    void dirtyStackingContextZOrderLists();
    void updateZOrderLists();
    Vector<RenderLayer*>* posZOrderList() const { return m_posZOrderList; }
    Vector<RenderLayer*>* negZOrderList() const { return m_negZOrderList; }

    void dirtyNormalFlowList();
    void updateNormalFlowList();
    Vector<RenderLayer*>* normalFlowList() const { return m_normalFlowList; }

    bool hasVisibleContent() const { return m_hasVisibleContent; }
    bool hasVisibleDescendant() const { return m_hasVisibleDescendant; }
    void setHasVisibleContent(bool);
    void dirtyVisibleContentStatus();

    // Gets the nearest enclosing positioned ancestor layer (also includes
    // the <html> layer and the root layer).
    RenderLayer* enclosingPositionedAncestor() const;

#if USE(ACCELERATED_COMPOSITING)
    // Enclosing compositing layer; if includeSelf is true, may return this.
    RenderLayer* enclosingCompositingLayer(bool includeSelf = true) const;
    // Ancestor compositing layer, excluding this.
    RenderLayer* ancestorCompositingLayer() const { return enclosingCompositingLayer(false); }
#endif

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
                        IntRect& backgroundRect, IntRect& foregroundRect, IntRect& outlineRect, bool temporaryClipRects = false) const;

    // Compute and cache clip rects computed with the given layer as the root
    void updateClipRects(const RenderLayer* rootLayer);
    // Compute and return the clip rects. If useCached is true, will used previously computed clip rects on ancestors
    // (rather than computing them all from scratch up the parent chain).
    void calculateClipRects(const RenderLayer* rootLayer, ClipRects&, bool useCached = false) const;
    ClipRects* clipRects() const { return m_clipRects; }

    IntRect childrenClipRect() const; // Returns the foreground clip rect of the layer in the document's coordinate space.
    IntRect selfClipRect() const; // Returns the background clip rect of the layer in the document's coordinate space.

    bool intersectsDamageRect(const IntRect& layerBounds, const IntRect& damageRect, const RenderLayer* rootLayer) const;

    // Bounding box relative to some ancestor layer.
    IntRect boundingBox(const RenderLayer* rootLayer) const;
    // Bounding box in the coordinates of this layer.
    IntRect localBoundingBox() const;
    // Bounding box relative to the root.
    IntRect absoluteBoundingBox() const;

    void updateHoverActiveState(const HitTestRequest&, HitTestResult&);

    // Return a cached repaint rect, computed relative to the layer renderer's containerForRepaint.
    IntRect repaintRect() const { return m_repaintRect; }
    void computeRepaintRects();
    void setNeedsFullRepaint(bool f = true) { m_needsFullRepaint = f; }
    
    int staticX() const { return m_staticX; }
    int staticY() const { return m_staticY; }
    void setStaticX(int staticX) { m_staticX = staticX; }
    void setStaticY(int staticY);

    bool hasTransform() const { return renderer()->hasTransform(); }
    // Note that this transform has the transform-origin baked in.
    TransformationMatrix* transform() const { return m_transform.get(); }
    // currentTransform computes a transform which takes accelerated animations into account. The
    // resulting transform has transform-origin baked in. If the layer does not have a transform,
    // returns the identity matrix.
    TransformationMatrix currentTransform() const;
    
    // Get the perspective transform, which is applied to transformed sublayers.
    // Returns true if the layer has a -webkit-perspective.
    // Note that this transform has the perspective-origin baked in.
    TransformationMatrix perspectiveTransform() const;
    FloatPoint perspectiveOrigin() const;
    bool preserves3D() const { return renderer()->style()->transformStyle3D() == TransformStyle3DPreserve3D; }
    bool has3DTransform() const { return m_transform && !m_transform->isAffine(); }

     // Overloaded new operator.  Derived classes must override operator new
    // in order to allocate out of the RenderArena.
    void* operator new(size_t, RenderArena*) throw();

    // Overridden to prevent the normal delete from being called.
    void operator delete(void*, size_t);

#if USE(ACCELERATED_COMPOSITING)
    bool isComposited() const { return m_backing != 0; }
    bool hasCompositedMask() const;
    RenderLayerBacking* backing() const { return m_backing.get(); }
    RenderLayerBacking* ensureBacking();
    void clearBacking();
#else
    bool isComposited() const { return false; }
    bool hasCompositedMask() const { return false; }
#endif

    bool paintsWithTransparency() const
    {
        return isTransparent() && !isComposited();
    }

    bool paintsWithTransform() const
    {
        return transform() && !isComposited();
    }

private:
    // The normal operator new is disallowed on all render objects.
    void* operator new(size_t) throw();

private:
    void setNextSibling(RenderLayer* next) { m_next = next; }
    void setPreviousSibling(RenderLayer* prev) { m_previous = prev; }
    void setParent(RenderLayer* parent);
    void setFirstChild(RenderLayer* first) { m_first = first; }
    void setLastChild(RenderLayer* last) { m_last = last; }

    int renderBoxX() const { return renderer()->isBox() ? toRenderBox(renderer())->x() : 0; }
    int renderBoxY() const { return renderer()->isBox() ? toRenderBox(renderer())->y() : 0; }

    void collectLayers(Vector<RenderLayer*>*&, Vector<RenderLayer*>*&);

    void updateLayerListsIfNeeded();
    void updateCompositingAndLayerListsIfNeeded();
    
    enum PaintLayerFlag {
        PaintLayerHaveTransparency = 1,
        PaintLayerAppliedTransform = 1 << 1,
        PaintLayerTemporaryClipRects = 1 << 2,
        PaintLayerPaintingReflection = 1 << 3
    };
    
    typedef unsigned PaintLayerFlags;

    void paintLayer(RenderLayer* rootLayer, GraphicsContext*, const IntRect& paintDirtyRect,
                    PaintRestriction, RenderObject* paintingRoot, RenderObject::OverlapTestRequestMap* = 0,
                    PaintLayerFlags paintFlags = 0);

    RenderLayer* hitTestLayer(RenderLayer* rootLayer, RenderLayer* containerLayer, const HitTestRequest& request, HitTestResult& result,
                            const IntRect& hitTestRect, const IntPoint& hitTestPoint, bool appliedTransform,
                            const HitTestingTransformState* transformState = 0, double* zOffset = 0);

    PassRefPtr<HitTestingTransformState> createLocalTransformState(RenderLayer* rootLayer, RenderLayer* containerLayer,
                            const IntRect& hitTestRect, const IntPoint& hitTestPoint,
                            const HitTestingTransformState* containerTransformState) const;
    
    bool hitTestContents(const HitTestRequest&, HitTestResult&, const IntRect& layerBounds, const IntPoint& hitTestPoint, HitTestFilter) const;
    
    void computeScrollDimensions(bool* needHBar = 0, bool* needVBar = 0);

    bool shouldBeNormalFlowOnly() const; 

    // ScrollBarClient interface
    virtual void valueChanged(Scrollbar*);
    virtual void invalidateScrollbarRect(Scrollbar*, const IntRect&);
    virtual bool isActive() const;
    virtual bool scrollbarCornerPresent() const;
    virtual IntRect convertFromScrollbarToContainingView(const Scrollbar*, const IntRect&) const;
    virtual IntRect convertFromContainingViewToScrollbar(const Scrollbar*, const IntRect&) const;
    virtual IntPoint convertFromScrollbarToContainingView(const Scrollbar*, const IntPoint&) const;
    virtual IntPoint convertFromContainingViewToScrollbar(const Scrollbar*, const IntPoint&) const;
    
    IntSize scrollbarOffset(const Scrollbar*) const;
    
    void updateOverflowStatus(bool horizontalOverflow, bool verticalOverflow);

    void childVisibilityChanged(bool newVisibility);
    void dirtyVisibleDescendantStatus();
    void updateVisibilityStatus();

    // This flag is computed by RenderLayerCompositor, which knows more about 3d hierarchies than we do.
    void setHas3DTransformedDescendant(bool b) { m_has3DTransformedDescendant = b; }
    bool has3DTransformedDescendant() const { return m_has3DTransformedDescendant; }
    
    void dirty3DTransformedDescendantStatus();
    // Both updates the status, and returns true if descendants of this have 3d.
    bool update3DTransformedDescendantStatus();

    Node* enclosingElement() const;

    void createReflection();
    void removeReflection();

    void updateReflectionStyle();
    bool paintingInsideReflection() const { return m_paintingInsideReflection; }
    void setPaintingInsideReflection(bool b) { m_paintingInsideReflection = b; }
    
    void parentClipRects(const RenderLayer* rootLayer, ClipRects&, bool temporaryClipRects = false) const;
    IntRect backgroundClipRect(const RenderLayer* rootLayer, bool temporaryClipRects) const;

    RenderLayer* enclosingTransformedAncestor() const;

    // Convert a point in absolute coords into layer coords, taking transforms into account
    IntPoint absoluteToContents(const IntPoint&) const;

    void updateScrollCornerStyle();
    void updateResizerStyle();

#if USE(ACCELERATED_COMPOSITING)    
    bool hasCompositingDescendant() const { return m_hasCompositingDescendant; }
    void setHasCompositingDescendant(bool b)  { m_hasCompositingDescendant = b; }
    
    bool mustOverlapCompositedLayers() const { return m_mustOverlapCompositedLayers; }
    void setMustOverlapCompositedLayers(bool b) { m_mustOverlapCompositedLayers = b; }
#endif

private:
    friend class RenderLayerBacking;
    friend class RenderLayerCompositor;
    friend class RenderBoxModelObject;

    // Only safe to call from RenderBoxModelObject::destroyLayer(RenderArena*)
    void destroy(RenderArena*);

protected:
    RenderBoxModelObject* m_renderer;

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
    int m_scrollOriginX;        // only non-zero for rtl content
    int m_scrollLeftOverflow;   // only non-zero for rtl content

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

    // This list contains child layers that cannot create stacking contexts.  For now it is just
    // overflow layers, but that may change in the future.
    Vector<RenderLayer*>* m_normalFlowList;

    ClipRects* m_clipRects;      // Cached clip rects used when painting and hit testing.
#ifndef NDEBUG
    const RenderLayer* m_clipRectsRoot;   // Root layer used to compute clip rects.
#endif

    bool m_scrollDimensionsDirty : 1;
    bool m_zOrderListsDirty : 1;
    bool m_normalFlowListDirty: 1;
    bool m_isNormalFlowOnly : 1;

    bool m_usedTransparency : 1; // Tracks whether we need to close a transparent layer, i.e., whether
                                 // we ended up painting this layer or any descendants (and therefore need to
                                 // blend).
    bool m_paintingInsideReflection : 1;  // A state bit tracking if we are painting inside a replica.
    bool m_inOverflowRelayout : 1;
    bool m_needsFullRepaint : 1;

    bool m_overflowStatusDirty : 1;
    bool m_horizontalOverflow : 1;
    bool m_verticalOverflow : 1;
    bool m_visibleContentStatusDirty : 1;
    bool m_hasVisibleContent : 1;
    bool m_visibleDescendantStatusDirty : 1;
    bool m_hasVisibleDescendant : 1;

    bool m_3DTransformedDescendantStatusDirty : 1;
    bool m_has3DTransformedDescendant : 1;  // Set on a stacking context layer that has 3D descendants anywhere
                                            // in a preserves3D hierarchy. Hint to do 3D-aware hit testing.
#if USE(ACCELERATED_COMPOSITING)
    bool m_hasCompositingDescendant : 1;
    bool m_mustOverlapCompositedLayers : 1;
#endif

    RenderMarquee* m_marquee; // Used by layers with overflow:marquee
    
    // Cached normal flow values for absolute positioned elements with static left/top values.
    int m_staticX;
    int m_staticY;
    
    OwnPtr<TransformationMatrix> m_transform;
    
    // May ultimately be extended to many replicas (with their own paint order).
    RenderReplica* m_reflection;
    
    // Renderers to hold our custom scroll corner and resizer.
    RenderScrollbarPart* m_scrollCorner;
    RenderScrollbarPart* m_resizer;

#if USE(ACCELERATED_COMPOSITING)
    OwnPtr<RenderLayerBacking> m_backing;
#endif
};

} // namespace WebCore

#endif // RenderLayer_h
