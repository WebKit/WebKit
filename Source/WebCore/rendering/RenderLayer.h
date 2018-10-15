/*
 * Copyright (C) 2003, 2009, 2012, 2015 Apple Inc. All rights reserved.
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

#pragma once

#include "ClipRect.h"
#include "GraphicsLayer.h"
#include "LayerFragment.h"
#include "PaintFrequencyTracker.h"
#include "PaintInfo.h"
#include "RenderBox.h"
#include "RenderPtr.h"
#include "ScrollableArea.h"
#include <memory>
#include <wtf/WeakPtr.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

class CSSFilter;
class ClipRects;
class ClipRectsCache;
class HitTestRequest;
class HitTestResult;
class HitTestingTransformState;
class RenderFragmentedFlow;
class RenderGeometryMap;
class RenderLayerBacking;
class RenderLayerCompositor;
class RenderLayerFilters;
class RenderMarquee;
class RenderReplica;
class RenderScrollbarPart;
class RenderStyle;
class RenderView;
class Scrollbar;
class TransformationMatrix;

enum BorderRadiusClippingRule { IncludeSelfForBorderRadius, DoNotIncludeSelfForBorderRadius };
enum IncludeSelfOrNot { IncludeSelf, ExcludeSelf };

enum RepaintStatus {
    NeedsNormalRepaint,
    NeedsFullRepaint,
    NeedsFullRepaintForPositionedMovementLayout
};

enum ClipRectsType {
    PaintingClipRects, // Relative to painting ancestor. Used for painting.
    RootRelativeClipRects, // Relative to the ancestor treated as the root (e.g. transformed layer). Used for hit testing.
    AbsoluteClipRects, // Relative to the RenderView's layer. Used for compositing overlap testing.
    NumCachedClipRectsTypes,
    AllClipRectTypes,
    TemporaryClipRects
};

enum ShouldRespectOverflowClip {
    IgnoreOverflowClip,
    RespectOverflowClip
};

enum ShouldApplyRootOffsetToFragments {
    ApplyRootOffsetToFragments,
    IgnoreRootOffsetForFragments
};

enum LayerScrollCoordinationRole {
    ViewportConstrained = 1 << 0,
    Scrolling           = 1 << 1
};

enum class RequestState {
    Unknown,
    DontCare,
    False,
    True,
    Undetermined
};

struct ScrollRectToVisibleOptions {
    SelectionRevealMode revealMode { SelectionRevealMode::Reveal };
    const ScrollAlignment& alignX { ScrollAlignment::alignCenterIfNeeded };
    const ScrollAlignment& alignY { ScrollAlignment::alignCenterIfNeeded };
    ShouldAllowCrossOriginScrolling shouldAllowCrossOriginScrolling { ShouldAllowCrossOriginScrolling::No };
};

class RenderLayer final : public ScrollableArea {
    WTF_MAKE_FAST_ALLOCATED;
public:
    friend class RenderReplica;
    friend class RenderLayerFilters;

    explicit RenderLayer(RenderLayerModelObject&);
    virtual ~RenderLayer();

#if PLATFORM(IOS)
    // Called before the renderer's widget (if any) has been nulled out.
    void willBeDestroyed();
#endif
    String name() const;

    Page& page() const { return renderer().page(); }
    RenderLayerModelObject& renderer() const { return m_renderer; }
    RenderBox* renderBox() const { return is<RenderBox>(renderer()) ? &downcast<RenderBox>(renderer()) : nullptr; }

    RenderLayer* parent() const { return m_parent; }
    RenderLayer* previousSibling() const { return m_previous; }
    RenderLayer* nextSibling() const { return m_next; }
    RenderLayer* firstChild() const { return m_first; }
    RenderLayer* lastChild() const { return m_last; }
    bool isDescendantOf(const RenderLayer&) const;

    // This does an ancestor tree walk. Avoid it!
    const RenderLayer* root() const
    {
        const RenderLayer* curr = this;
        while (curr->parent())
            curr = curr->parent();
        return curr;
    }

    void addChild(RenderLayer& newChild, RenderLayer* beforeChild = nullptr);
    void removeChild(RenderLayer&);

    void insertOnlyThisLayer();
    void removeOnlyThisLayer();

    bool isNormalFlowOnly() const { return m_isNormalFlowOnly; }
    bool isStackingContext() const { return m_isStackingContext; }

    // Gets the enclosing stacking context for this layer, excluding this layer itself.
    RenderLayer* stackingContext() const;

    // Gets the enclosing stacking container for this layer, possibly the layer
    // itself, if it is a stacking container.
    RenderLayer* enclosingStackingContext() { return isStackingContext() ? this : stackingContext(); }

    void dirtyNormalFlowList();
    void dirtyZOrderLists();
    void dirtyStackingContextZOrderLists();

    class LayerList {
        friend class RenderLayer;
    public:
        using iterator = RenderLayer**;
        using const_iterator = RenderLayer * const *;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        iterator begin() { return m_layerList ? m_layerList->begin() : nullptr; }
        iterator end() { return m_layerList ? m_layerList->end() : nullptr; }

        reverse_iterator rbegin() { return reverse_iterator(end()); }
        reverse_iterator rend() { return reverse_iterator(begin()); }

        const_iterator begin() const { return m_layerList ? m_layerList->begin() : nullptr; }
        const_iterator end() const { return m_layerList ? m_layerList->end() : nullptr; }

        const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
        const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }

        size_t size() const { return m_layerList ? m_layerList->size() : 0; }

    private:
        LayerList(Vector<RenderLayer*>* layerList)
            : m_layerList(layerList)
        {
        }
        
        Vector<RenderLayer*>* m_layerList;
    };

    LayerList normalFlowLayers() const
    {
        ASSERT(!m_normalFlowListDirty);
        return LayerList(m_normalFlowList.get());
    }

    LayerList positiveZOrderLayers() const
    {
        ASSERT(!m_zOrderListsDirty);
        ASSERT(isStackingContext() || !m_posZOrderList);
        return LayerList(m_posZOrderList.get());
    }

    bool hasNegativeZOrderLayers() const
    {
        return m_negZOrderList && m_negZOrderList->size();
    }

    LayerList negativeZOrderLayers() const
    {
        ASSERT(!m_zOrderListsDirty);
        ASSERT(isStackingContext() || !m_negZOrderList);
        return LayerList(m_negZOrderList.get());
    }

    // Update our normal and z-index lists.
    void updateLayerListsIfNeeded();

    void repaintIncludingDescendants();

    // Indicate that the layer contents need to be repainted. Only has an effect
    // if layer compositing is being used.
    void setBackingNeedsRepaint(GraphicsLayer::ShouldClipToLayer = GraphicsLayer::ClipToLayer);

    // The rect is in the coordinate space of the layer's render object.
    void setBackingNeedsRepaintInRect(const LayoutRect&, GraphicsLayer::ShouldClipToLayer = GraphicsLayer::ClipToLayer);
    void repaintIncludingNonCompositingDescendants(RenderLayerModelObject* repaintContainer);

    void styleChanged(StyleDifference, const RenderStyle* oldStyle);

    RenderMarquee* marquee() const { return m_marquee.get(); }

    bool isSelfPaintingLayer() const { return m_isSelfPaintingLayer; }

    bool cannotBlitToWindow() const;

    bool isTransparent() const { return renderer().isTransparent() || renderer().hasMask(); }

    bool hasReflection() const { return renderer().hasReflection(); }
    bool isReflection() const { return renderer().isReplica(); }
    RenderReplica* reflection() const { return m_reflection.get(); }
    RenderLayer* reflectionLayer() const;
    bool isReflectionLayer(const RenderLayer&) const;

    const LayoutPoint& location() const { return m_topLeft; }
    void setLocation(const LayoutPoint& p) { m_topLeft = p; }

    const IntSize& size() const { return m_layerSize; }
    void setSize(const IntSize& size) { m_layerSize = size; }

    LayoutRect rect() const { return LayoutRect(location(), size()); }

    int scrollWidth() const;
    int scrollHeight() const;

    void panScrollFromPoint(const IntPoint&);

    // Scrolling methods for layers that can scroll their overflow.
    void scrollByRecursively(const IntSize& delta, ScrollableArea** scrolledArea = nullptr);

    WEBCORE_EXPORT void scrollToOffset(const ScrollOffset&, ScrollClamping = ScrollClamping::Clamped);
    void scrollToXOffset(int x, ScrollClamping clamping = ScrollClamping::Clamped) { scrollToOffset(ScrollOffset(x, scrollOffset().y()), clamping); }
    void scrollToYOffset(int y, ScrollClamping clamping = ScrollClamping::Clamped) { scrollToOffset(ScrollOffset(scrollOffset().x(), y), clamping); }

    void scrollToXPosition(int x, ScrollClamping = ScrollClamping::Clamped);
    void scrollToYPosition(int y, ScrollClamping = ScrollClamping::Clamped);

    void setPostLayoutScrollPosition(std::optional<ScrollPosition>);
    void applyPostLayoutScrollPositionIfNeeded();

    ScrollOffset scrollOffset() const { return scrollOffsetFromPosition(m_scrollPosition); }
    IntSize scrollableContentsSize() const;

    void availableContentSizeChanged(AvailableSizeChangeReason) override;

    // "absoluteRect" is in scaled document coordinates.
    void scrollRectToVisible(const LayoutRect& absoluteRect, bool insideFixed, const ScrollRectToVisibleOptions&);

    bool scrollsOverflow() const;
    bool hasScrollbars() const { return m_hBar || m_vBar; }
    void setHasHorizontalScrollbar(bool);
    void setHasVerticalScrollbar(bool);

    Ref<Scrollbar> createScrollbar(ScrollbarOrientation);
    void destroyScrollbar(ScrollbarOrientation);

    bool hasHorizontalScrollbar() const { return horizontalScrollbar(); }
    bool hasVerticalScrollbar() const { return verticalScrollbar(); }

    // ScrollableArea overrides
    ScrollPosition scrollPosition() const override { return m_scrollPosition; }

    Scrollbar* horizontalScrollbar() const override { return m_hBar.get(); }
    Scrollbar* verticalScrollbar() const override { return m_vBar.get(); }
    ScrollableArea* enclosingScrollableArea() const override;
    bool isScrollableOrRubberbandable() override;
    bool hasScrollableOrRubberbandableAncestor() override;
#if ENABLE(CSS_SCROLL_SNAP)
    void updateSnapOffsets() override;
#endif

#if PLATFORM(IOS)
#if ENABLE(IOS_TOUCH_EVENTS)
    bool handleTouchEvent(const PlatformTouchEvent&) override;
#endif
    
    void didStartScroll() override;
    void didEndScroll() override;
    void didUpdateScroll() override;
    void setIsUserScroll(bool isUserScroll) override { m_inUserScroll = isUserScroll; }

    bool isInUserScroll() const { return m_inUserScroll; }

    bool requiresScrollBoundsOriginUpdate() const { return m_requiresScrollBoundsOriginUpdate; }
    void setRequiresScrollBoundsOriginUpdate(bool requiresUpdate = true) { m_requiresScrollBoundsOriginUpdate = requiresUpdate; }
#endif

    // Returns true when the layer could do touch scrolling, but doesn't look at whether there is actually scrollable overflow.
    bool canUseAcceleratedTouchScrolling() const;
    // Returns true when there is actually scrollable overflow (requires layout to be up-to-date).
    bool hasTouchScrollableOverflow() const;

    int verticalScrollbarWidth(OverlayScrollbarSizeRelevancy = IgnoreOverlayScrollbarSize) const;
    int horizontalScrollbarHeight(OverlayScrollbarSizeRelevancy = IgnoreOverlayScrollbarSize) const;

    bool hasOverflowControls() const;
    bool isPointInResizeControl(const IntPoint& absolutePoint) const;
    bool hitTestOverflowControls(HitTestResult&, const IntPoint& localPoint);
    IntSize offsetFromResizeCorner(const IntPoint& absolutePoint) const;

    void paintOverflowControls(GraphicsContext&, const IntPoint&, const IntRect& damageRect, bool paintingOverlayControls = false);
    void paintScrollCorner(GraphicsContext&, const IntPoint&, const IntRect& damageRect);
    void paintResizer(GraphicsContext&, const LayoutPoint&, const LayoutRect& damageRect);

    void updateScrollInfoAfterLayout();

    bool scroll(ScrollDirection, ScrollGranularity, float multiplier = 1);
    void autoscroll(const IntPoint&);

    bool canResize() const;
    void resize(const PlatformMouseEvent&, const LayoutSize&);
    bool inResizeMode() const { return m_inResizeMode; }
    void setInResizeMode(bool b) { m_inResizeMode = b; }

    bool isRenderViewLayer() const { return m_isRenderViewLayer; }
    bool isForcedStackingContext() const { return m_forcedStackingContext; }

    RenderLayerCompositor& compositor() const;
    
    // Notification from the renderer that its content changed (e.g. current frame of image changed).
    // Allows updates of layer content without repainting.
    void contentChanged(ContentChangeType);

    bool canRender3DTransforms() const;

    enum UpdateLayerPositionsFlag {
        CheckForRepaint                 = 1 << 0,
        NeedsFullRepaintInBacking       = 1 << 1,
        IsCompositingUpdateRoot         = 1 << 2,
        UpdateCompositingLayers         = 1 << 3,
        UpdatePagination                = 1 << 4,
        SeenTransformedLayer            = 1 << 5,
        Seen3DTransformedLayer          = 1 << 6,
    };
    static constexpr OptionSet<UpdateLayerPositionsFlag> updateLayerPositionsDefaultFlags() { return { CheckForRepaint, IsCompositingUpdateRoot, UpdateCompositingLayers }; }

    void updateLayerPositionsAfterLayout(const RenderLayer* rootLayer, OptionSet<UpdateLayerPositionsFlag>);

    void updateLayerPositionsAfterOverflowScroll();
    void updateLayerPositionsAfterDocumentScroll();

    bool hasCompositedLayerInEnclosingPaginationChain() const;
    enum PaginationInclusionMode { ExcludeCompositedPaginatedLayers, IncludeCompositedPaginatedLayers };
    RenderLayer* enclosingPaginationLayer(PaginationInclusionMode mode) const
    {
        if (mode == ExcludeCompositedPaginatedLayers && hasCompositedLayerInEnclosingPaginationChain())
            return nullptr;
        return m_enclosingPaginationLayer.get();
    }

    void updateTransform();
    
#if ENABLE(CSS_COMPOSITING)
    void updateBlendMode();
#endif

    const LayoutSize& offsetForInFlowPosition() const { return m_offsetForInFlowPosition; }

    void clearClipRectsIncludingDescendants(ClipRectsType typeToClear = AllClipRectTypes);
    void clearClipRects(ClipRectsType typeToClear = AllClipRectTypes);

    void addBlockSelectionGapsBounds(const LayoutRect&);
    void clearBlockSelectionGapsBounds();
    void repaintBlockSelectionGaps();

    // FIXME: We should ASSERT(!m_visibleContentStatusDirty) here, but see https://bugs.webkit.org/show_bug.cgi?id=71044
    // ditto for hasVisibleDescendant(), see https://bugs.webkit.org/show_bug.cgi?id=71277
    bool hasVisibleContent() const { return m_hasVisibleContent; }
    bool hasVisibleDescendant() const { return m_hasVisibleDescendant; }

    void setHasVisibleContent();
    void dirtyVisibleContentStatus();

    bool hasVisibleBoxDecorationsOrBackground() const;
    bool hasVisibleBoxDecorations() const;

    struct PaintedContentRequest {
        void makeStatesUndetermined()
        {
            if (hasPaintedContent == RequestState::Unknown)
                hasPaintedContent = RequestState::Undetermined;

            if (hasSubpixelAntialiasedText == RequestState::Unknown)
                hasSubpixelAntialiasedText = RequestState::Undetermined;
        }

        void setHasPaintedContent() { hasPaintedContent = RequestState::True; }
        void setHasSubpixelAntialiasedText() { hasSubpixelAntialiasedText = RequestState::True; }

        bool needToDeterminePaintedContentState() const { return hasPaintedContent == RequestState::Unknown; }
        bool needToDetermineSubpixelAntialiasedTextState() const { return hasSubpixelAntialiasedText == RequestState::Unknown; }

        bool probablyHasPaintedContent() const { return hasPaintedContent == RequestState::True || hasPaintedContent == RequestState::Undetermined; }
        bool probablyHasSubpixelAntialiasedText() const { return hasSubpixelAntialiasedText == RequestState::True || hasSubpixelAntialiasedText == RequestState::Undetermined; }
        
        bool isSatisfied() const { return hasPaintedContent != RequestState::Unknown && hasSubpixelAntialiasedText != RequestState::Unknown; }

        RequestState hasPaintedContent { RequestState::Unknown };
        RequestState hasSubpixelAntialiasedText { RequestState::DontCare };
    };

    // Returns true if this layer has visible content (ignoring any child layers).
    bool isVisuallyNonEmpty(PaintedContentRequest* = nullptr) const;
    // True if this layer container renderers that paint.
    bool hasNonEmptyChildRenderers(PaintedContentRequest&) const;

    // FIXME: We should ASSERT(!m_hasSelfPaintingLayerDescendantDirty); here but we hit the same bugs as visible content above.
    // Part of the issue is with subtree relayout: we don't check if our ancestors have some descendant flags dirty, missing some updates.
    bool hasSelfPaintingLayerDescendant() const { return m_hasSelfPaintingLayerDescendant; }

    // Gets the nearest enclosing positioned ancestor layer (also includes
    // the <html> layer and the root layer).
    RenderLayer* enclosingAncestorForPosition(PositionType) const;

    // Returns the nearest enclosing layer that is scrollable.
    RenderLayer* enclosingScrollableLayer() const;

    // The layer relative to which clipping rects for this layer are computed.
    RenderLayer* clippingRootForPainting() const;

    RenderLayer* enclosingOverflowClipLayer(IncludeSelfOrNot) const;

    // Enclosing compositing layer; if includeSelf is true, may return this.
    RenderLayer* enclosingCompositingLayer(IncludeSelfOrNot = IncludeSelf) const;
    RenderLayer* enclosingCompositingLayerForRepaint(IncludeSelfOrNot = IncludeSelf) const;
    // Ancestor compositing layer, excluding this.
    RenderLayer* ancestorCompositingLayer() const { return enclosingCompositingLayer(ExcludeSelf); }

    RenderLayer* enclosingFilterLayer(IncludeSelfOrNot = IncludeSelf) const;
    RenderLayer* enclosingFilterRepaintLayer() const;
    void setFilterBackendNeedsRepaintingInRect(const LayoutRect&);
    bool hasAncestorWithFilterOutsets() const;

    bool canUseConvertToLayerCoords() const
    {
        // These RenderObject have an impact on their layers' without them knowing about it.
        return !renderer().hasTransform() && !renderer().isSVGRoot();
    }

    // FIXME: adjustForColumns allows us to position compositing layers in columns correctly, but eventually they need to be split across columns too.
    enum ColumnOffsetAdjustment { DontAdjustForColumns, AdjustForColumns };
    void convertToPixelSnappedLayerCoords(const RenderLayer* ancestorLayer, IntPoint& location, ColumnOffsetAdjustment adjustForColumns = DontAdjustForColumns) const;
    LayoutPoint convertToLayerCoords(const RenderLayer* ancestorLayer, const LayoutPoint&, ColumnOffsetAdjustment adjustForColumns = DontAdjustForColumns) const;
    LayoutSize offsetFromAncestor(const RenderLayer*, ColumnOffsetAdjustment = DontAdjustForColumns) const;

    int zIndex() const { return renderer().style().zIndex(); }

    enum PaintLayerFlag {
        PaintLayerHaveTransparency                      = 1 << 0,
        PaintLayerAppliedTransform                      = 1 << 1,
        PaintLayerTemporaryClipRects                    = 1 << 2,
        PaintLayerPaintingReflection                    = 1 << 3,
        PaintLayerPaintingOverlayScrollbars             = 1 << 4,
        PaintLayerPaintingCompositingBackgroundPhase    = 1 << 5,
        PaintLayerPaintingCompositingForegroundPhase    = 1 << 6,
        PaintLayerPaintingCompositingMaskPhase          = 1 << 7,
        PaintLayerPaintingCompositingClipPathPhase      = 1 << 8,
        PaintLayerPaintingCompositingScrollingPhase     = 1 << 9,
        PaintLayerPaintingOverflowContents              = 1 << 10,
        PaintLayerPaintingRootBackgroundOnly            = 1 << 11,
        PaintLayerPaintingSkipRootBackground            = 1 << 12,
        PaintLayerPaintingChildClippingMaskPhase        = 1 << 13,
    };
    static constexpr OptionSet<PaintLayerFlag> paintLayerPaintingCompositingAllPhasesFlags() { return { PaintLayerPaintingCompositingBackgroundPhase,  PaintLayerPaintingCompositingForegroundPhase }; }

    enum class SecurityOriginPaintPolicy { AnyOrigin, AccessibleOriginOnly };

    // The two main functions that use the layer system.  The paint method
    // paints the layers that intersect the damage rect from back to
    // front.  The hitTest method looks for mouse events by walking
    // layers that intersect the point from front to back.
    void paint(GraphicsContext&, const LayoutRect& damageRect, const LayoutSize& subpixelOffset = LayoutSize(), OptionSet<PaintBehavior> = PaintBehavior::Normal,
        RenderObject* subtreePaintRoot = nullptr, OptionSet<PaintLayerFlag> = { }, SecurityOriginPaintPolicy = SecurityOriginPaintPolicy::AnyOrigin);
    bool hitTest(const HitTestRequest&, HitTestResult&);
    bool hitTest(const HitTestRequest&, const HitTestLocation&, HitTestResult&);
    void paintOverlayScrollbars(GraphicsContext&, const LayoutRect& damageRect, OptionSet<PaintBehavior>, RenderObject* subtreePaintRoot = nullptr);

    struct ClipRectsContext {
        ClipRectsContext(const RenderLayer* inRootLayer, ClipRectsType inClipRectsType, OverlayScrollbarSizeRelevancy inOverlayScrollbarSizeRelevancy = IgnoreOverlayScrollbarSize, ShouldRespectOverflowClip inRespectOverflowClip = RespectOverflowClip)
            : rootLayer(inRootLayer)
            , clipRectsType(inClipRectsType)
            , overlayScrollbarSizeRelevancy(inOverlayScrollbarSizeRelevancy)
            , respectOverflowClip(inRespectOverflowClip)
        { }
        const RenderLayer* rootLayer;
        ClipRectsType clipRectsType;
        OverlayScrollbarSizeRelevancy overlayScrollbarSizeRelevancy;
        ShouldRespectOverflowClip respectOverflowClip;
    };

    // This method figures out our layerBounds in coordinates relative to
    // |rootLayer}.  It also computes our background and foreground clip rects
    // for painting/event handling.
    // Pass offsetFromRoot if known.
    void calculateRects(const ClipRectsContext&, const LayoutRect& paintDirtyRect, LayoutRect& layerBounds,
        ClipRect& backgroundRect, ClipRect& foregroundRect, const LayoutSize& offsetFromRoot) const;

    // Public just for RenderTreeAsText.
    void collectFragments(LayerFragments&, const RenderLayer* rootLayer, const LayoutRect& dirtyRect,
        PaginationInclusionMode,
        ClipRectsType, OverlayScrollbarSizeRelevancy inOverlayScrollbarSizeRelevancy, ShouldRespectOverflowClip, const LayoutSize& offsetFromRoot,
        const LayoutRect* layerBoundingBox = nullptr, ShouldApplyRootOffsetToFragments = IgnoreRootOffsetForFragments);
        
    LayoutRect childrenClipRect() const; // Returns the foreground clip rect of the layer in the document's coordinate space.
    LayoutRect selfClipRect() const; // Returns the background clip rect of the layer in the document's coordinate space.
    LayoutRect localClipRect(bool& clipExceedsBounds) const; // Returns the background clip rect of the layer in the local coordinate space.

    // Pass offsetFromRoot if known.
    bool intersectsDamageRect(const LayoutRect& layerBounds, const LayoutRect& damageRect, const RenderLayer* rootLayer, const LayoutSize& offsetFromRoot, const LayoutRect* cachedBoundingBox = nullptr) const;

    enum CalculateLayerBoundsFlag {
        IncludeSelfTransform                    = 1 << 0,
        UseLocalClipRectIfPossible              = 1 << 1,
        IncludeLayerFilterOutsets               = 1 << 2,
        ExcludeHiddenDescendants                = 1 << 3,
        DontConstrainForMask                    = 1 << 4,
        IncludeCompositedDescendants            = 1 << 5,
        UseFragmentBoxesExcludingCompositing    = 1 << 6,
        UseFragmentBoxesIncludingCompositing    = 1 << 7,

    };
    static constexpr OptionSet<CalculateLayerBoundsFlag> defaultCalculateLayerBoundsFlags() { return { IncludeSelfTransform, UseLocalClipRectIfPossible, IncludeLayerFilterOutsets, UseFragmentBoxesExcludingCompositing }; }

    // Bounding box relative to some ancestor layer. Pass offsetFromRoot if known.
    LayoutRect boundingBox(const RenderLayer* rootLayer, const LayoutSize& offsetFromRoot = LayoutSize(), OptionSet<CalculateLayerBoundsFlag> = { }) const;
    // Bounding box in the coordinates of this layer.
    LayoutRect localBoundingBox(OptionSet<CalculateLayerBoundsFlag> = { }) const;
    // Deprecated: Pixel snapped bounding box relative to the root.
    WEBCORE_EXPORT IntRect absoluteBoundingBox() const;
    // Device pixel snapped bounding box relative to the root. absoluteBoundingBox() callers will be directed to this.
    FloatRect absoluteBoundingBoxForPainting() const;

    // Bounds used for layer overlap testing in RenderLayerCompositor.
    LayoutRect overlapBounds() const { return overlapBoundsIncludeChildren() ? calculateLayerBounds(this, LayoutSize()) : localBoundingBox(); }
    
    // Takes transform animations into account, returning true if they could be cheaply computed.
    // Unlike overlapBounds, these bounds include descendant layers.
    bool getOverlapBoundsIncludingChildrenAccountingForTransformAnimations(LayoutRect&, OptionSet<CalculateLayerBoundsFlag> additionalFlags = { }) const;

    // If true, this layer's children are included in its bounds for overlap testing.
    // We can't rely on the children's positions if this layer has a filter that could have moved the children's pixels around.
    bool overlapBoundsIncludeChildren() const { return hasFilter() && renderer().style().filter().hasFilterThatMovesPixels(); }

    // Can pass offsetFromRoot if known.
    LayoutRect calculateLayerBounds(const RenderLayer* ancestorLayer, const LayoutSize& offsetFromRoot, OptionSet<CalculateLayerBoundsFlag> = defaultCalculateLayerBoundsFlags()) const;
    
    // Return a cached repaint rect, computed relative to the layer renderer's containerForRepaint.
    bool hasComputedRepaintRects() const { return renderer().hasRepaintLayoutRects(); }
    LayoutRect repaintRectIncludingNonCompositingDescendants() const;

    void setRepaintStatus(RepaintStatus status) { m_repaintStatus = status; }
    RepaintStatus repaintStatus() const { return static_cast<RepaintStatus>(m_repaintStatus); }

    LayoutUnit staticInlinePosition() const { return m_staticInlinePosition; }
    LayoutUnit staticBlockPosition() const { return m_staticBlockPosition; }
   
    void setStaticInlinePosition(LayoutUnit position) { m_staticInlinePosition = position; }
    void setStaticBlockPosition(LayoutUnit position) { m_staticBlockPosition = position; }

#if PLATFORM(IOS)
    bool adjustForIOSCaretWhenScrolling() const { return m_adjustForIOSCaretWhenScrolling; }
    void setAdjustForIOSCaretWhenScrolling(bool adjustForIOSCaretWhenScrolling) { m_adjustForIOSCaretWhenScrolling = adjustForIOSCaretWhenScrolling; }
#endif

    bool hasTransform() const { return renderer().hasTransform(); }
    // Note that this transform has the transform-origin baked in.
    TransformationMatrix* transform() const { return m_transform.get(); }
    // currentTransform computes a transform which takes accelerated animations into account. The
    // resulting transform has transform-origin baked in. If the layer does not have a transform,
    // returns the identity matrix.
    TransformationMatrix currentTransform(RenderStyle::ApplyTransformOrigin = RenderStyle::IncludeTransformOrigin) const;
    TransformationMatrix renderableTransform(OptionSet<PaintBehavior>) const;
    
    // Get the perspective transform, which is applied to transformed sublayers.
    // Returns true if the layer has a -webkit-perspective.
    // Note that this transform has the perspective-origin baked in.
    TransformationMatrix perspectiveTransform() const;
    FloatPoint perspectiveOrigin() const;
    bool preserves3D() const { return renderer().style().transformStyle3D() == TransformStyle3D::Preserve3D; }
    bool has3DTransform() const { return m_transform && !m_transform->isAffine(); }

    void filterNeedsRepaint();
    bool hasFilter() const { return renderer().hasFilter(); }
    bool hasBackdropFilter() const
    {
#if ENABLE(FILTERS_LEVEL_2)
        return renderer().hasBackdropFilter();
#else
        return false;
#endif
    }

#if ENABLE(CSS_COMPOSITING)
    bool hasBlendMode() const { return renderer().hasBlendMode(); }
    BlendMode blendMode() const { return static_cast<BlendMode>(m_blendMode); }

    bool isolatesCompositedBlending() const { return m_hasNotIsolatedCompositedBlendingDescendants && isStackingContext(); }
    bool hasNotIsolatedCompositedBlendingDescendants() const { return m_hasNotIsolatedCompositedBlendingDescendants; }
    void setHasNotIsolatedCompositedBlendingDescendants(bool hasNotIsolatedCompositedBlendingDescendants)
    {
        m_hasNotIsolatedCompositedBlendingDescendants = hasNotIsolatedCompositedBlendingDescendants;
    }

    bool isolatesBlending() const { return hasNotIsolatedBlendingDescendants() && isStackingContext(); }
    
    // FIXME: We should ASSERT(!m_hasNotIsolatedBlendingDescendantsStatusDirty); here but we hit the same bugs as visible content above.
    bool hasNotIsolatedBlendingDescendants() const { return m_hasNotIsolatedBlendingDescendants; }
    bool hasNotIsolatedBlendingDescendantsStatusDirty() const { return m_hasNotIsolatedBlendingDescendantsStatusDirty; }
#else
    bool hasBlendMode() const { return false; }
    bool isolatesCompositedBlending() const { return false; }
    bool isolatesBlending() const { return false; }
    bool hasNotIsolatedBlendingDescendantsStatusDirty() const { return false; }
#endif

    bool isComposited() const { return m_backing != nullptr; }
    bool hasCompositingDescendant() const { return m_hasCompositingDescendant; }
    bool hasCompositedMask() const;

    RenderLayerBacking* backing() const { return m_backing.get(); }
    RenderLayerBacking* ensureBacking();
    void clearBacking(bool layerBeingDestroyed = false);

    GraphicsLayer* layerForScrolling() const override;
    GraphicsLayer* layerForHorizontalScrollbar() const override;
    GraphicsLayer* layerForVerticalScrollbar() const override;
    GraphicsLayer* layerForScrollCorner() const override;

    bool usesCompositedScrolling() const override;
    bool usesAsyncScrolling() const override;

    bool paintsWithTransparency(OptionSet<PaintBehavior> paintBehavior) const
    {
        return (isTransparent() || hasBlendMode() || (isolatesBlending() && !renderer().isDocumentElementRenderer())) && ((paintBehavior & PaintBehavior::FlattenCompositingLayers) || !isComposited());
    }

    bool paintsWithTransform(OptionSet<PaintBehavior>) const;
    bool shouldPaintMask(OptionSet<PaintBehavior>, OptionSet<PaintLayerFlag>) const;
    bool shouldApplyClipPath(OptionSet<PaintBehavior>, OptionSet<PaintLayerFlag>) const;

    // Returns true if background phase is painted opaque in the given rect.
    // The query rect is given in local coordinates.
    bool backgroundIsKnownToBeOpaqueInRect(const LayoutRect&) const;

    bool scrollingMayRevealBackground() const;

    bool containsDirtyOverlayScrollbars() const { return m_containsDirtyOverlayScrollbars; }
    void setContainsDirtyOverlayScrollbars(bool dirtyScrollbars) { m_containsDirtyOverlayScrollbars = dirtyScrollbars; }

    bool paintsWithFilters() const;
    bool requiresFullLayerImageForFilters() const;

#if !ASSERT_DISABLED
    bool layerListMutationAllowed() const { return m_layerListMutationAllowed; }
    void setLayerListMutationAllowed(bool flag) { m_layerListMutationAllowed = flag; }
#endif

    Element* enclosingElement() const;

    enum ViewportConstrainedNotCompositedReason {
        NoNotCompositedReason,
        NotCompositedForBoundsOutOfView,
        NotCompositedForNonViewContainer,
        NotCompositedForNoVisibleContent,
    };

    void setViewportConstrainedNotCompositedReason(ViewportConstrainedNotCompositedReason reason) { m_viewportConstrainedNotCompositedReason = reason; }
    ViewportConstrainedNotCompositedReason viewportConstrainedNotCompositedReason() const { return static_cast<ViewportConstrainedNotCompositedReason>(m_viewportConstrainedNotCompositedReason); }
    
    bool isRenderFragmentedFlow() const { return renderer().isRenderFragmentedFlow(); }
    bool isOutOfFlowRenderFragmentedFlow() const { return renderer().isOutOfFlowRenderFragmentedFlow(); }
    bool isInsideFragmentedFlow() const { return renderer().fragmentedFlowState() != RenderObject::NotInsideFragmentedFlow; }
    bool isDirtyRenderFragmentedFlow() const
    {
        ASSERT(isRenderFragmentedFlow());
        return m_zOrderListsDirty || m_normalFlowListDirty;
    }

    RenderLayer* enclosingFragmentedFlowAncestor() const;

    bool shouldPlaceBlockDirectionScrollbarOnLeft() const final { return renderer().shouldPlaceBlockDirectionScrollbarOnLeft(); }

    void simulateFrequentPaint() { SinglePaintFrequencyTracking { m_paintFrequencyTracker }; }
    bool paintingFrequently() const { return m_paintFrequencyTracker.paintingFrequently(); }

private:

    void setNextSibling(RenderLayer* next) { m_next = next; }
    void setPreviousSibling(RenderLayer* prev) { m_previous = prev; }
    void setParent(RenderLayer*);
    void setFirstChild(RenderLayer* first) { m_first = first; }
    void setLastChild(RenderLayer* last) { m_last = last; }

    bool shouldBeNormalFlowOnly() const;
    bool shouldBeStackingContext() const;
    
    // Return true if changed.
    bool setIsNormalFlowOnly(bool);
    bool setIsStackingContext(bool);

    bool isDirtyStackingContext() const { return m_zOrderListsDirty && isStackingContext(); }

    void updateZOrderLists();
    void rebuildZOrderLists();
    void rebuildZOrderLists(std::unique_ptr<Vector<RenderLayer*>>&, std::unique_ptr<Vector<RenderLayer*>>&);
    void collectLayers(bool includeHiddenLayers, std::unique_ptr<Vector<RenderLayer*>>&, std::unique_ptr<Vector<RenderLayer*>>&);
    void clearZOrderLists();

    void updateNormalFlowList();

    struct LayerPaintingInfo {
        LayerPaintingInfo(RenderLayer* inRootLayer, const LayoutRect& inDirtyRect, OptionSet<PaintBehavior> inPaintBehavior, const LayoutSize& inSubpixelOffset, RenderObject* inSubtreePaintRoot = nullptr, OverlapTestRequestMap* inOverlapTestRequests = nullptr, bool inRequireSecurityOriginAccessForWidgets = false)
            : rootLayer(inRootLayer)
            , subtreePaintRoot(inSubtreePaintRoot)
            , paintDirtyRect(inDirtyRect)
            , subpixelOffset(inSubpixelOffset)
            , overlapTestRequests(inOverlapTestRequests)
            , paintBehavior(inPaintBehavior)
            , requireSecurityOriginAccessForWidgets(inRequireSecurityOriginAccessForWidgets)
        { }

        RenderLayer* rootLayer;
        RenderObject* subtreePaintRoot; // Only paint descendants of this object.
        LayoutRect paintDirtyRect; // Relative to rootLayer;
        LayoutSize subpixelOffset;
        OverlapTestRequestMap* overlapTestRequests; // May be null.
        OptionSet<PaintBehavior> paintBehavior;
        bool requireSecurityOriginAccessForWidgets;
        bool clipToDirtyRect { true };
    };

    // Compute, cache and return clip rects computed with the given layer as the root.
    Ref<ClipRects> updateClipRects(const ClipRectsContext&);
    // Compute and return the clip rects. If useCached is true, will used previously computed clip rects on ancestors
    // (rather than computing them all from scratch up the parent chain).
    void calculateClipRects(const ClipRectsContext&, ClipRects&) const;
    ClipRects* clipRects(const ClipRectsContext&) const;


    void setAncestorChainHasSelfPaintingLayerDescendant();
    void dirtyAncestorChainHasSelfPaintingLayerDescendantStatus();

    void computeRepaintRects(const RenderLayerModelObject* repaintContainer, const RenderGeometryMap* = nullptr);
    void computeRepaintRectsIncludingDescendants();
    void clearRepaintRects();

    LayoutRect clipRectRelativeToAncestor(RenderLayer* ancestor, LayoutSize offsetFromAncestor, const LayoutRect& constrainingRect) const;

    void clipToRect(GraphicsContext&, const LayerPaintingInfo&, const ClipRect&, BorderRadiusClippingRule = IncludeSelfForBorderRadius);
    void restoreClip(GraphicsContext&, const LayerPaintingInfo&, const ClipRect&);

    bool shouldRepaintAfterLayout() const;

    void updateSelfPaintingLayer();

    void updateScrollbarsAfterStyleChange(const RenderStyle* oldStyle);
    void updateScrollbarsAfterLayout();

    // Returns true if the position changed.
    bool updateLayerPosition();

    void updateLayerPositions(RenderGeometryMap* = nullptr, OptionSet<UpdateLayerPositionsFlag> = updateLayerPositionsDefaultFlags());

    enum UpdateLayerPositionsAfterScrollFlag {
        IsOverflowScroll                        = 1 << 0,
        HasSeenViewportConstrainedAncestor      = 1 << 1,
        HasSeenAncestorWithOverflowClip         = 1 << 2,
        HasChangedAncestor                      = 1 << 3,
    };
    void updateLayerPositionsAfterScroll(RenderGeometryMap*, OptionSet<UpdateLayerPositionsAfterScrollFlag> = { });

    ScrollOffset clampScrollOffset(const ScrollOffset&) const;

    RenderLayer* enclosingPaginationLayerInSubtree(const RenderLayer* rootLayer, PaginationInclusionMode) const;

    LayoutPoint renderBoxLocation() const { return is<RenderBox>(renderer()) ? downcast<RenderBox>(renderer()).location() : LayoutPoint(); }

    void updateCompositingAndLayerListsIfNeeded();

    bool setupFontSubpixelQuantization(GraphicsContext&, bool& didQuantizeFonts);

    Path computeClipPath(const LayoutSize& offsetFromRoot, LayoutRect& rootRelativeBounds, WindRule&) const;

    bool setupClipPath(GraphicsContext&, const LayerPaintingInfo&, const LayoutSize& offsetFromRoot, LayoutRect& rootRelativeBounds, bool& rootRelativeBoundsComputed);

    void ensureLayerFilters();
    void clearLayerFilters();

    RenderLayerFilters* filtersForPainting(GraphicsContext&, OptionSet<PaintLayerFlag>) const;
    GraphicsContext* setupFilters(GraphicsContext& destinationContext, LayerPaintingInfo&, OptionSet<PaintLayerFlag>, const LayoutSize& offsetFromRoot, LayoutRect& rootRelativeBounds, bool& rootRelativeBoundsComputed);
    void applyFilters(GraphicsContext& originalContext, const LayerPaintingInfo&, const LayerFragments&);

    void paintLayer(GraphicsContext&, const LayerPaintingInfo&, OptionSet<PaintLayerFlag>);
    void paintLayerContentsAndReflection(GraphicsContext&, const LayerPaintingInfo&, OptionSet<PaintLayerFlag>);
    void paintLayerByApplyingTransform(GraphicsContext&, const LayerPaintingInfo&, OptionSet<PaintLayerFlag>, const LayoutSize& translationOffset = LayoutSize());
    void paintLayerContents(GraphicsContext&, const LayerPaintingInfo&, OptionSet<PaintLayerFlag>);
    void paintList(LayerList, GraphicsContext&, const LayerPaintingInfo&, OptionSet<PaintLayerFlag>);

    void updatePaintingInfoForFragments(LayerFragments&, const LayerPaintingInfo&, OptionSet<PaintLayerFlag>, bool shouldPaintContent, const LayoutSize& offsetFromRoot);
    void paintBackgroundForFragments(const LayerFragments&, GraphicsContext&, GraphicsContext& transparencyLayerContext,
        const LayoutRect& transparencyPaintDirtyRect, bool haveTransparency, const LayerPaintingInfo&, OptionSet<PaintBehavior>, RenderObject* paintingRootForRenderer);
    void paintForegroundForFragments(const LayerFragments&, GraphicsContext&, GraphicsContext& transparencyLayerContext,
        const LayoutRect& transparencyPaintDirtyRect, bool haveTransparency, const LayerPaintingInfo&, OptionSet<PaintBehavior>, RenderObject* paintingRootForRenderer);
    void paintForegroundForFragmentsWithPhase(PaintPhase, const LayerFragments&, GraphicsContext&, const LayerPaintingInfo&, OptionSet<PaintBehavior>, RenderObject* paintingRootForRenderer);
    void paintOutlineForFragments(const LayerFragments&, GraphicsContext&, const LayerPaintingInfo&, OptionSet<PaintBehavior>, RenderObject* paintingRootForRenderer);
    void paintOverflowControlsForFragments(const LayerFragments&, GraphicsContext&, const LayerPaintingInfo&);
    void paintMaskForFragments(const LayerFragments&, GraphicsContext&, const LayerPaintingInfo&, OptionSet<PaintBehavior>, RenderObject* paintingRootForRenderer);
    void paintChildClippingMaskForFragments(const LayerFragments&, GraphicsContext&, const LayerPaintingInfo&, OptionSet<PaintBehavior>, RenderObject* paintingRootForRenderer);
    void paintTransformedLayerIntoFragments(GraphicsContext&, const LayerPaintingInfo&, OptionSet<PaintLayerFlag>);

    RenderLayer* transparentPaintingAncestor();
    void beginTransparencyLayers(GraphicsContext&, const LayerPaintingInfo&, const LayoutRect& dirtyRect);

    RenderLayer* hitTestLayer(RenderLayer* rootLayer, RenderLayer* containerLayer, const HitTestRequest&, HitTestResult&,
        const LayoutRect& hitTestRect, const HitTestLocation&, bool appliedTransform,
        const HitTestingTransformState* = nullptr, double* zOffset = nullptr);
    RenderLayer* hitTestLayerByApplyingTransform(RenderLayer* rootLayer, RenderLayer* containerLayer, const HitTestRequest&, HitTestResult&,
        const LayoutRect& hitTestRect, const HitTestLocation&, const HitTestingTransformState* = nullptr, double* zOffset = nullptr,
        const LayoutSize& translationOffset = LayoutSize());
    RenderLayer* hitTestList(LayerList, RenderLayer* rootLayer, const HitTestRequest&, HitTestResult&,
        const LayoutRect& hitTestRect, const HitTestLocation&,
        const HitTestingTransformState*, double* zOffsetForDescendants, double* zOffset,
        const HitTestingTransformState* unflattenedTransformState, bool depthSortDescendants);

    Ref<HitTestingTransformState> createLocalTransformState(RenderLayer* rootLayer, RenderLayer* containerLayer,
        const LayoutRect& hitTestRect, const HitTestLocation&,
        const HitTestingTransformState* containerTransformState,
        const LayoutSize& translationOffset = LayoutSize()) const;
    
    bool hitTestContents(const HitTestRequest&, HitTestResult&, const LayoutRect& layerBounds, const HitTestLocation&, HitTestFilter) const;
    bool hitTestContentsForFragments(const LayerFragments&, const HitTestRequest&, HitTestResult&, const HitTestLocation&, HitTestFilter, bool& insideClipRect) const;
    bool hitTestResizerInFragments(const LayerFragments&, const HitTestLocation&) const;
    RenderLayer* hitTestTransformedLayerInFragments(RenderLayer* rootLayer, RenderLayer* containerLayer, const HitTestRequest&, HitTestResult&,
        const LayoutRect& hitTestRect, const HitTestLocation&, const HitTestingTransformState* = nullptr, double* zOffset = nullptr);

    bool listBackgroundIsKnownToBeOpaqueInRect(const LayerList&, const LayoutRect&) const;

    void computeScrollDimensions();
    bool hasHorizontalOverflow() const;
    bool hasVerticalOverflow() const;
    bool hasScrollableHorizontalOverflow() const;
    bool hasScrollableVerticalOverflow() const;

    bool showsOverflowControls() const;

    bool shouldBeSelfPaintingLayer() const;

    int scrollOffset(ScrollbarOrientation) const override;
    
    // ScrollableArea interface
    void invalidateScrollbarRect(Scrollbar&, const IntRect&) override;
    void invalidateScrollCornerRect(const IntRect&) override;
    bool isActive() const override;
    bool isScrollCornerVisible() const override;
    IntRect scrollCornerRect() const override;
    IntRect convertFromScrollbarToContainingView(const Scrollbar&, const IntRect&) const override;
    IntRect convertFromContainingViewToScrollbar(const Scrollbar&, const IntRect&) const override;
    IntPoint convertFromScrollbarToContainingView(const Scrollbar&, const IntPoint&) const override;
    IntPoint convertFromContainingViewToScrollbar(const Scrollbar&, const IntPoint&) const override;
    int scrollSize(ScrollbarOrientation) const override;
    void setScrollOffset(const ScrollOffset&) override;

    IntRect visibleContentRectInternal(VisibleContentRectIncludesScrollbars, VisibleContentRectBehavior) const override;
    IntSize visibleSize() const override;
    IntSize contentsSize() const override;
    IntSize overhangAmount() const override;
    IntPoint lastKnownMousePosition() const override;
    bool isHandlingWheelEvent() const override;
    bool shouldSuspendScrollAnimations() const override;
    IntRect scrollableAreaBoundingBox(bool* isInsideFixed = nullptr) const override;
    bool isRubberBandInProgress() const override;
    bool forceUpdateScrollbarsOnMainThreadForPerformanceTesting() const override;
#if ENABLE(CSS_SCROLL_SNAP)
    bool isScrollSnapInProgress() const override;
#endif
    bool usesMockScrollAnimator() const override;
    void logMockScrollAnimatorMessage(const String&) const override;

#if ENABLE(IOS_TOUCH_EVENTS)
    void registerAsTouchEventListenerForScrolling();
    void unregisterAsTouchEventListenerForScrolling();
#endif

    // Rectangle encompassing the scroll corner and resizer rect.
    LayoutRect scrollCornerAndResizerRect() const;

    // NOTE: This should only be called by the overridden setScrollOffset from ScrollableArea.
    void scrollTo(const ScrollPosition&);
    void updateCompositingLayersAfterScroll();

    IntSize scrollbarOffset(const Scrollbar&) const;
    
    void updateScrollableAreaSet(bool hasOverflow);
    
    bool allowsCurrentScroll() const;

    void dirtyAncestorChainVisibleDescendantStatus();
    void setAncestorChainHasVisibleDescendant();

    void updateDescendantDependentFlags();

    bool has3DTransformedDescendant() const { return m_has3DTransformedDescendant; }

    bool hasTransformedAncestor() const { return m_hasTransformedAncestor; }
    bool has3DTransformedAncestor() const { return m_has3DTransformedAncestor; }

    void dirty3DTransformedDescendantStatus();
    // Both updates the status, and returns true if descendants of this have 3d.
    bool update3DTransformedDescendantStatus();

    void createReflection();
    void removeReflection();

    RenderStyle createReflectionStyle();
    bool paintingInsideReflection() const { return m_paintingInsideReflection; }
    void setPaintingInsideReflection(bool b) { m_paintingInsideReflection = b; }

    void updateFiltersAfterStyleChange();
    void updateFilterPaintingStrategy();

#if ENABLE(CSS_COMPOSITING)
    void updateAncestorChainHasBlendingDescendants();
    void dirtyAncestorChainHasBlendingDescendants();
#endif

    Ref<ClipRects> parentClipRects(const ClipRectsContext&) const;
    ClipRect backgroundClipRect(const ClipRectsContext&) const;

    RenderLayer* enclosingTransformedAncestor() const;

    LayoutRect getRectToExpose(const LayoutRect& visibleRect, const LayoutRect& exposeRect, bool insideFixed, const ScrollAlignment& alignX, const ScrollAlignment& alignY) const;

    // Convert a point in absolute coords into layer coords, taking transforms into account
    LayoutPoint absoluteToContents(const LayoutPoint&) const;

    void positionOverflowControls(const IntSize&);
    void updateScrollCornerStyle();
    void clearScrollCorner();
    void updateResizerStyle();
    void clearResizer();

    void drawPlatformResizerImage(GraphicsContext&, const LayoutRect& resizerCornerRect);

    void updatePagination();

    void setHasCompositingDescendant(bool b)  { m_hasCompositingDescendant = b; }
    
    enum class IndirectCompositingReason {
        None,
        Stacking,
        Overlap,
        BackgroundLayer,
        GraphicalEffect, // opacity, mask, filter, transform etc.
        Perspective,
        Preserve3D
    };
    
    void setIndirectCompositingReason(IndirectCompositingReason reason) { m_indirectCompositingReason = static_cast<unsigned>(reason); }
    IndirectCompositingReason indirectCompositingReason() const { return static_cast<IndirectCompositingReason>(m_indirectCompositingReason); }
    bool mustCompositeForIndirectReasons() const { return m_indirectCompositingReason; }

    friend class RenderLayerBacking;
    friend class RenderLayerCompositor;
    friend class RenderLayerModelObject;

    LayoutUnit overflowTop() const;
    LayoutUnit overflowBottom() const;
    LayoutUnit overflowLeft() const;
    LayoutUnit overflowRight() const;

    IntRect rectForHorizontalScrollbar(const IntRect& borderBoxRect) const;
    IntRect rectForVerticalScrollbar(const IntRect& borderBoxRect) const;

    LayoutUnit verticalScrollbarStart(int minX, int maxX) const;
    LayoutUnit horizontalScrollbarStart(int minX) const;

    bool overflowControlsIntersectRect(const IntRect& localRect) const;

    // The bitfields are up here so they will fall into the padding from ScrollableArea on 64-bit.

    const bool m_isRenderViewLayer : 1;
    const bool m_forcedStackingContext : 1;

    bool m_isNormalFlowOnly : 1;
    bool m_isStackingContext : 1;

    bool m_zOrderListsDirty : 1;
    bool m_normalFlowListDirty: 1;

    // Keeps track of whether the layer is currently resizing, so events can cause resizing to start and stop.
    bool m_inResizeMode : 1;

    bool m_scrollDimensionsDirty : 1;
    bool m_isSelfPaintingLayer : 1;

    // If have no self-painting descendants, we don't have to walk our children during painting. This can lead to
    // significant savings, especially if the tree has lots of non-self-painting layers grouped together (e.g. table cells).
    bool m_hasSelfPaintingLayerDescendant : 1;
    bool m_hasSelfPaintingLayerDescendantDirty : 1;

    bool m_usedTransparency : 1; // Tracks whether we need to close a transparent layer, i.e., whether
                                 // we ended up painting this layer or any descendants (and therefore need to
                                 // blend).
    bool m_paintingInsideReflection : 1;  // A state bit tracking if we are painting inside a replica.
    bool m_inOverflowRelayout : 1;
    unsigned m_repaintStatus : 2; // RepaintStatus

    bool m_visibleContentStatusDirty : 1;
    bool m_hasVisibleContent : 1;
    bool m_visibleDescendantStatusDirty : 1;
    bool m_hasVisibleDescendant : 1;
    bool m_registeredScrollableArea : 1;

    bool m_3DTransformedDescendantStatusDirty : 1;
    bool m_has3DTransformedDescendant : 1;  // Set on a stacking context layer that has 3D descendants anywhere
                                            // in a preserves3D hierarchy. Hint to do 3D-aware hit testing.
    bool m_hasCompositingDescendant : 1; // In the z-order tree.

    bool m_hasTransformedAncestor : 1;
    bool m_has3DTransformedAncestor : 1;

    unsigned m_indirectCompositingReason : 3;
    unsigned m_viewportConstrainedNotCompositedReason : 2;

#if PLATFORM(IOS)
    bool m_adjustForIOSCaretWhenScrolling : 1;
#if ENABLE(IOS_TOUCH_EVENTS)
    bool m_registeredAsTouchEventListenerForScrolling : 1;
#endif
    bool m_inUserScroll : 1;
    bool m_requiresScrollBoundsOriginUpdate : 1;
#endif

    bool m_containsDirtyOverlayScrollbars : 1;
    bool m_updatingMarqueePosition : 1;

#if !ASSERT_DISABLED
    bool m_layerListMutationAllowed : 1;
#endif

#if ENABLE(CSS_COMPOSITING)
    unsigned m_blendMode : 5;
    bool m_hasNotIsolatedCompositedBlendingDescendants : 1;
    bool m_hasNotIsolatedBlendingDescendants : 1;
    bool m_hasNotIsolatedBlendingDescendantsStatusDirty : 1;
#endif

    RenderLayerModelObject& m_renderer;

    RenderLayer* m_parent { nullptr };
    RenderLayer* m_previous { nullptr };
    RenderLayer* m_next { nullptr };
    RenderLayer* m_first { nullptr };
    RenderLayer* m_last { nullptr };

    // For layers that establish stacking contexts, m_posZOrderList holds a sorted list of all the
    // descendant layers within the stacking context that have z-indices of 0 or greater
    // (auto will count as 0). m_negZOrderList holds descendants within our stacking context with negative
    // z-indices.
    std::unique_ptr<Vector<RenderLayer*>> m_posZOrderList;
    std::unique_ptr<Vector<RenderLayer*>> m_negZOrderList;

    // This list contains child layers that cannot create stacking contexts and appear in normal flow order.
    std::unique_ptr<Vector<RenderLayer*>> m_normalFlowList;

    // Our current relative position offset.
    LayoutSize m_offsetForInFlowPosition;

    // Our (x,y) coordinates are in our parent layer's coordinate space.
    LayoutPoint m_topLeft;

    // The layer's width/height
    IntSize m_layerSize;

    ScrollPosition m_scrollPosition;
    std::optional<ScrollPosition> m_postLayoutScrollPosition;

    // The width/height of our scrolled area.
    IntSize m_scrollSize;

    // For layers with overflow, we have a pair of scrollbars.
    RefPtr<Scrollbar> m_hBar;
    RefPtr<Scrollbar> m_vBar;

    std::unique_ptr<ClipRectsCache> m_clipRectsCache;
    
    IntPoint m_cachedOverlayScrollbarOffset;

    std::unique_ptr<RenderMarquee> m_marquee; // Used for <marquee>.
    
    // Cached normal flow values for absolute positioned elements with static left/top values.
    LayoutUnit m_staticInlinePosition;
    LayoutUnit m_staticBlockPosition;

    std::unique_ptr<TransformationMatrix> m_transform;
    
    // May ultimately be extended to many replicas (with their own paint order).
    RenderPtr<RenderReplica> m_reflection;

    // Renderers to hold our custom scroll corner and resizer.
    RenderPtr<RenderScrollbarPart> m_scrollCorner;
    RenderPtr<RenderScrollbarPart> m_resizer;

    // Pointer to the enclosing RenderLayer that caused us to be paginated. It is 0 if we are not paginated.
    WeakPtr<RenderLayer> m_enclosingPaginationLayer;

    IntRect m_blockSelectionGapsBounds;

    std::unique_ptr<RenderLayerFilters> m_filters;
    std::unique_ptr<RenderLayerBacking> m_backing;
    
    PaintFrequencyTracker m_paintFrequencyTracker;
};

inline void RenderLayer::clearZOrderLists()
{
    ASSERT(!isStackingContext());
    ASSERT(m_layerListMutationAllowed);

    m_posZOrderList = nullptr;
    m_negZOrderList = nullptr;
}

inline void RenderLayer::updateZOrderLists()
{
    if (!m_zOrderListsDirty)
        return;

    if (!isStackingContext()) {
        clearZOrderLists();
        m_zOrderListsDirty = false;
        return;
    }

    rebuildZOrderLists();
}

#if !ASSERT_DISABLED
class LayerListMutationDetector {
public:
    LayerListMutationDetector(RenderLayer* layer)
        : m_layer(layer)
        , m_previousMutationAllowedState(layer->layerListMutationAllowed())
    {
        m_layer->setLayerListMutationAllowed(false);
    }
    
    ~LayerListMutationDetector()
    {
        m_layer->setLayerListMutationAllowed(m_previousMutationAllowedState);
    }

private:
    RenderLayer* m_layer;
    bool m_previousMutationAllowedState;
};
#endif

void makeMatrixRenderable(TransformationMatrix&, bool has3DRendering);

bool compositedWithOwnBackingStore(const RenderLayer&);

WTF::TextStream& operator<<(WTF::TextStream&, const RenderLayer&);

} // namespace WebCore

#if ENABLE(TREE_DEBUGGING)
// Outside the WebCore namespace for ease of invocation from lldb.
void showLayerTree(const WebCore::RenderLayer*);
void showLayerTree(const WebCore::RenderObject*);
#endif
