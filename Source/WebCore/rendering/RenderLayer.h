/*
 * Copyright (C) 2003, 2009, 2012, 2015 Apple Inc. All rights reserved.
 * Copyright (c) 2020 Igalia S.L.
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
#include "LayoutRect.h"
#include "PaintFrequencyTracker.h"
#include "PaintInfo.h"
#include "RenderBox.h"
#include "RenderPtr.h"
#include "RenderSVGModelObject.h"
#include "ScrollBehavior.h"
#include <memory>
#include <wtf/Markable.h>
#include <wtf/WeakPtr.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

class CSSFilter;
class ClipRects;
class ClipRectsCache;
class EventRegionContext;
class HitTestRequest;
class HitTestResult;
class HitTestingTransformState;
class Region;
class RenderFragmentedFlow;
class RenderGeometryMap;
class RenderLayerBacking;
class RenderLayerCompositor;
class RenderLayerFilters;
class RenderLayerScrollableArea;
class RenderMarquee;
class RenderReplica;
class RenderScrollbarPart;
class RenderStyle;
class RenderView;
class Scrollbar;
class TransformationMatrix;

enum BorderRadiusClippingRule { IncludeSelfForBorderRadius, DoNotIncludeSelfForBorderRadius };
enum IncludeSelfOrNot { IncludeSelf, ExcludeSelf };
enum CrossFrameBoundaries { No, Yes };

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

enum class RequestState {
    Unknown,
    DontCare,
    False,
    True,
    Undetermined
};

enum class IndirectCompositingReason {
    None,
    Clipping,
    Stacking,
    OverflowScrollPositioning,
    Overlap,
    BackgroundLayer,
    GraphicalEffect, // opacity, mask, filter, transform etc.
    Perspective,
    Preserve3D
};

enum class ShouldAllowCrossOriginScrolling { No, Yes };

struct ScrollRectToVisibleOptions {
    SelectionRevealMode revealMode { SelectionRevealMode::Reveal };
    const ScrollAlignment& alignX { ScrollAlignment::alignCenterIfNeeded };
    const ScrollAlignment& alignY { ScrollAlignment::alignCenterIfNeeded };
    ShouldAllowCrossOriginScrolling shouldAllowCrossOriginScrolling { ShouldAllowCrossOriginScrolling::No };
    ScrollBehavior behavior { ScrollBehavior::Auto };
    std::optional<LayoutRect> visibilityCheckRect { std::nullopt };
};

using ScrollingScope = uint64_t;

class RenderLayer : public CanMakeWeakPtr<RenderLayer> {
    WTF_MAKE_ISO_ALLOCATED(RenderLayer);
public:
    friend class RenderReplica;
    friend class RenderLayerFilters;
    friend class RenderLayerBacking;
    friend class RenderLayerCompositor;
    friend class RenderLayerScrollableArea;

    explicit RenderLayer(RenderLayerModelObject&);
    ~RenderLayer();

    WEBCORE_EXPORT RenderLayerScrollableArea* scrollableArea() const;
    WEBCORE_EXPORT RenderLayerScrollableArea* ensureLayerScrollableArea();

#if PLATFORM(IOS_FAMILY)
    // Called before the renderer's widget (if any) has been nulled out.
    void willBeDestroyed();
#endif
    String name() const;

    Page& page() const { return renderer().page(); }
    RenderLayerModelObject& renderer() const { return m_renderer; }
    RenderBox* renderBox() const { return dynamicDowncast<RenderBox>(renderer()); }

    RenderLayer* parent() const { return m_parent; }
    RenderLayer* previousSibling() const { return m_previous; }
    RenderLayer* nextSibling() const { return m_next; }
    RenderLayer* firstChild() const { return m_first; }
    RenderLayer* lastChild() const { return m_last; }
    bool isDescendantOf(const RenderLayer&) const;
    RenderLayer* commonAncestorWithLayer(const RenderLayer&) const;

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

    enum class LayerChangeTiming {
        StyleChange,
        RenderTreeConstruction,
    };
    void insertOnlyThisLayer(LayerChangeTiming);
    void removeOnlyThisLayer(LayerChangeTiming);

    bool isNormalFlowOnly() const { return m_isNormalFlowOnly; }

    // isStackingContext is true for layers that we've determined should be stacking contexts for painting.
    // Not all stacking contexts are CSS stacking contexts.
    bool isStackingContext() const { return isCSSStackingContext() || m_isOpportunisticStackingContext; }

    // isCSSStackingContext is true for layers that are stacking contexts from a CSS perspective.
    // isCSSStackingContext() => isStackingContext().
    // FIXME: m_forcedStackingContext should affect isStackingContext(), not isCSSStackingContext(), but doing so breaks media control mix-blend-mode.
    bool isCSSStackingContext() const { return m_isCSSStackingContext || m_forcedStackingContext; }

    // Gets the enclosing stacking context for this layer, excluding this layer itself.
    RenderLayer* stackingContext() const;

    // Gets the enclosing stacking container for this layer, possibly the layer
    // itself, if it is a stacking container.
    RenderLayer* enclosingStackingContext() { return isStackingContext() ? this : stackingContext(); }

    RenderLayer* paintOrderParent() const;

    std::optional<LayerRepaintRects> repaintRects() const
    {
        if (m_repaintRectsValid)
            return m_repaintRects;

        return { };
    }

    void dirtyNormalFlowList();
    void dirtyZOrderLists();
    void dirtyStackingContextZOrderLists();

    bool normalFlowListDirty() const { return m_normalFlowListDirty; }
    bool zOrderListsDirty() const { return m_zOrderListsDirty; }

#if ASSERT_ENABLED
    bool layerListMutationAllowed() const { return m_layerListMutationAllowed; }
    void setLayerListMutationAllowed(bool flag) { m_layerListMutationAllowed = flag; }
#endif

    bool willCompositeClipPath() const;

protected:
    void destroy();

private:
    // These flags propagate in paint order (z-order tree).
    enum class Compositing {
        HasDescendantNeedingRequirementsTraversal           = 1 << 0, // Need to do the overlap-testing tree walk because hierarchy or geometry changed.
        HasDescendantNeedingBackingOrHierarchyTraversal     = 1 << 1, // Need to update geometry, configuration and update the GraphicsLayer tree.

        // Things that trigger HasDescendantNeedingRequirementsTraversal
        NeedsPaintOrderChildrenUpdate                       = 1 << 2, // The paint order children of this layer changed (gained/lost child, order change).
        NeedsPostLayoutUpdate                               = 1 << 3, // Needs compositing to be re-evaluated after layout (it depends on geometry).
        DescendantsNeedRequirementsTraversal                = 1 << 4, // Something changed that forces computeCompositingRequirements to traverse all descendant layers.
        SubsequentLayersNeedRequirementsTraversal           = 1 << 5, // Something changed that forces computeCompositingRequirements to traverse all layers later in paint order.

        // Things that trigger HasDescendantNeedingBackingOrHierarchyTraversal
        NeedsGeometryUpdate                                 = 1 << 6, // This layer needs a geometry update.
        NeedsConfigurationUpdate                            = 1 << 7, // This layer needs a configuration update (updating its internal compositing hierarchy).
        NeedsScrollingTreeUpdate                            = 1 << 8, // Something changed that requires this layer's scrolling tree node to be updated.
        NeedsLayerConnection                                = 1 << 9, // This layer needs hookup with its parents or children.
        ChildrenNeedGeometryUpdate                          = 1 << 10, // This layer's composited children need a geometry update.
        DescendantsNeedBackingAndHierarchyTraversal         = 1 << 11, // Something changed that forces us to traverse all descendant layers in updateBackingAndHierarchy.
    };

    static constexpr OptionSet<Compositing> computeCompositingRequirementsFlags()
    {
        return {
            Compositing::NeedsPaintOrderChildrenUpdate,
            Compositing::NeedsPostLayoutUpdate,
            Compositing::DescendantsNeedRequirementsTraversal,
            Compositing::SubsequentLayersNeedRequirementsTraversal,
        };
    }

    static constexpr OptionSet<Compositing> updateBackingOrHierarchyFlags()
    {
        return {
            Compositing::NeedsLayerConnection,
            Compositing::NeedsGeometryUpdate,
            Compositing::NeedsConfigurationUpdate,
            Compositing::NeedsScrollingTreeUpdate,
            Compositing::ChildrenNeedGeometryUpdate,
            Compositing::DescendantsNeedBackingAndHierarchyTraversal,
        };
    }

    void setAncestorsHaveCompositingDirtyFlag(Compositing);

public:
    bool hasDescendantNeedingCompositingRequirementsTraversal() const { return m_compositingDirtyBits.contains(Compositing::HasDescendantNeedingRequirementsTraversal); }
    bool hasDescendantNeedingUpdateBackingOrHierarchyTraversal() const { return m_compositingDirtyBits.contains(Compositing::HasDescendantNeedingBackingOrHierarchyTraversal); }

    bool needsCompositingPaintOrderChildrenUpdate() const { return m_compositingDirtyBits.contains(Compositing::NeedsPaintOrderChildrenUpdate); }
    bool needsPostLayoutCompositingUpdate() const { return m_compositingDirtyBits.contains(Compositing::NeedsPostLayoutUpdate); }
    bool descendantsNeedCompositingRequirementsTraversal() const { return m_compositingDirtyBits.contains(Compositing::DescendantsNeedRequirementsTraversal); }
    bool subsequentLayersNeedCompositingRequirementsTraversal() const { return m_compositingDirtyBits.contains(Compositing::SubsequentLayersNeedRequirementsTraversal); }

    bool needsCompositingLayerConnection() const { return m_compositingDirtyBits.contains(Compositing::NeedsLayerConnection); }
    bool needsCompositingGeometryUpdate() const { return m_compositingDirtyBits.contains(Compositing::NeedsGeometryUpdate); }
    bool needsCompositingConfigurationUpdate() const { return m_compositingDirtyBits.contains(Compositing::NeedsConfigurationUpdate); }
    bool needsScrollingTreeUpdate() const { return m_compositingDirtyBits.contains(Compositing::NeedsScrollingTreeUpdate); }
    bool childrenNeedCompositingGeometryUpdate() const { return m_compositingDirtyBits.contains(Compositing::ChildrenNeedGeometryUpdate); }
    bool descendantsNeedUpdateBackingAndHierarchyTraversal() const { return m_compositingDirtyBits.contains(Compositing::DescendantsNeedBackingAndHierarchyTraversal); }

    template<Compositing V>
    void setRequirementsTraversalDirtyBit()
    {
        m_compositingDirtyBits.add(V);
        setAncestorsHaveCompositingDirtyFlag(Compositing::HasDescendantNeedingRequirementsTraversal);
    }

    void setNeedsCompositingPaintOrderChildrenUpdate() { setRequirementsTraversalDirtyBit<Compositing::NeedsPaintOrderChildrenUpdate>(); }
    void setNeedsPostLayoutCompositingUpdate() { setRequirementsTraversalDirtyBit<Compositing::NeedsPostLayoutUpdate>(); }
    void setDescendantsNeedCompositingRequirementsTraversal() { setRequirementsTraversalDirtyBit<Compositing::DescendantsNeedRequirementsTraversal>(); }
    void setSubsequentLayersNeedCompositingRequirementsTraversal() { setRequirementsTraversalDirtyBit<Compositing::SubsequentLayersNeedRequirementsTraversal>(); }

    void setNeedsPostLayoutCompositingUpdateOnAncestors() { setAncestorsHaveCompositingDirtyFlag(Compositing::NeedsPostLayoutUpdate); }

    template<Compositing V>
    void setBackingAndHierarchyTraversalDirtyBit()
    {
        m_compositingDirtyBits.add(V);
        setAncestorsHaveCompositingDirtyFlag(Compositing::HasDescendantNeedingBackingOrHierarchyTraversal);
    }

    void setNeedsCompositingLayerConnection() { setBackingAndHierarchyTraversalDirtyBit<Compositing::NeedsLayerConnection>(); }
    void setNeedsCompositingGeometryUpdate() { setBackingAndHierarchyTraversalDirtyBit<Compositing::NeedsGeometryUpdate>(); }
    void setNeedsCompositingConfigurationUpdate() { setBackingAndHierarchyTraversalDirtyBit<Compositing::NeedsConfigurationUpdate>(); }
    void setNeedsScrollingTreeUpdate() { setBackingAndHierarchyTraversalDirtyBit<Compositing::NeedsScrollingTreeUpdate>(); }
    void setChildrenNeedCompositingGeometryUpdate() { setBackingAndHierarchyTraversalDirtyBit<Compositing::ChildrenNeedGeometryUpdate>(); }
    void setDescendantsNeedUpdateBackingAndHierarchyTraversal() { setBackingAndHierarchyTraversalDirtyBit<Compositing::DescendantsNeedBackingAndHierarchyTraversal>(); }

    void setNeedsCompositingGeometryUpdateOnAncestors() { setAncestorsHaveCompositingDirtyFlag(Compositing::NeedsGeometryUpdate); }

    bool needsCompositingRequirementsTraversal() const { return m_compositingDirtyBits.containsAny(computeCompositingRequirementsFlags()); }
    void clearCompositingRequirementsTraversalState()
    {
        m_compositingDirtyBits.remove(Compositing::HasDescendantNeedingRequirementsTraversal);
        m_compositingDirtyBits.remove(computeCompositingRequirementsFlags());
    }

    bool needsUpdateBackingOrHierarchyTraversal() const { return m_compositingDirtyBits.containsAny(updateBackingOrHierarchyFlags()); }
    void clearUpdateBackingOrHierarchyTraversalState()
    {
        m_compositingDirtyBits.remove(Compositing::HasDescendantNeedingBackingOrHierarchyTraversal);
        m_compositingDirtyBits.remove(updateBackingOrHierarchyFlags());
    }

    bool needsAnyCompositingTraversal() const { return !m_compositingDirtyBits.isEmpty(); }
    void clearCompositingPaintOrderState() { m_compositingDirtyBits = { }; }

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
    void updateDescendantDependentFlags();
    bool descendantDependentFlagsAreDirty() const
    {
        return m_visibleDescendantStatusDirty || m_visibleContentStatusDirty || m_hasSelfPaintingLayerDescendantDirty
#if ENABLE(CSS_COMPOSITING)
            || m_hasNotIsolatedBlendingDescendantsStatusDirty
#endif
        ;
    }

    void repaintIncludingDescendants();

    // Indicate that the layer contents need to be repainted. Only has an effect
    // if layer compositing is being used.
    void setBackingNeedsRepaint(GraphicsLayer::ShouldClipToLayer = GraphicsLayer::ClipToLayer);

    // The rect is in the coordinate space of the layer's render object.
    void setBackingNeedsRepaintInRect(const LayoutRect&, GraphicsLayer::ShouldClipToLayer = GraphicsLayer::ClipToLayer);
    void repaintIncludingNonCompositingDescendants(const RenderLayerModelObject* repaintContainer);

    void styleChanged(StyleDifference, const RenderStyle* oldStyle);

    bool isSelfPaintingLayer() const { return m_isSelfPaintingLayer; }

    bool cannotBlitToWindow() const;

    bool isTransparent() const { return renderer().isTransparent() || renderer().hasMask(); }

    bool hasReflection() const { return renderer().hasReflection(); }
    bool isReflection() const { return renderer().isReplica(); }
    RenderLayer* reflectionLayer() const;
    bool isReflectionLayer(const RenderLayer&) const;

    const LayoutPoint& location() const { return m_topLeft; }
    void setLocation(const LayoutPoint& p) { m_topLeft = p; }

    const IntSize& size() const { return m_layerSize; }
    void setSize(const IntSize& size) { m_layerSize = size; } // Only public for RenderTreeAsText.

    LayoutRect rect() const { return LayoutRect(location(), size()); }

    IntSize visibleSize() const;

    int scrollWidth() const;
    int scrollHeight() const;

    // Returns the nearest enclosing layer that is scrollable.
    // FIXME: This can return the RenderView's layer when callers probably want the FrameView as a ScrollableArea.
    RenderLayer* enclosingScrollableLayer(IncludeSelfOrNot, CrossFrameBoundaries) const;

    // Returns true when the layer could do touch scrolling, but doesn't look at whether there is actually scrollable overflow.
    bool canUseCompositedScrolling() const;
    // Returns true when there is actually scrollable overflow (requires layout to be up-to-date).
    bool hasCompositedScrollableOverflow() const;

    bool hasOverlayScrollbars() const;

    bool isPointInResizeControl(IntPoint localPoint) const;
    IntSize offsetFromResizeCorner(const IntPoint& localPoint) const;

    void updateScrollInfoAfterLayout();
    void updateScrollbarSteps();

    // Returns true if this RenderLayer is a candidate for scrolling during scrollIntoView operations.
    bool shouldTryToScrollForScrollIntoView() const;
    void autoscroll(const IntPoint&);

    bool canResize() const;
    LayoutSize minimumSizeForResizing(float zoomFactor) const;
    void resize(const PlatformMouseEvent&, const LayoutSize&);
    bool inResizeMode() const { return m_inResizeMode; }
    void setInResizeMode(bool b) { m_inResizeMode = b; }

    bool isRenderViewLayer() const { return m_isRenderViewLayer; }
    bool isForcedStackingContext() const { return m_forcedStackingContext; }
    bool isOpportunisticStackingContext() const { return m_isOpportunisticStackingContext; }

    RenderLayerCompositor& compositor() const;
    
    // Notification from the renderer that its content changed (e.g. current frame of image changed).
    // Allows updates of layer content without repainting.
    void contentChanged(ContentChangeType);

    bool canRender3DTransforms() const;

    void updateLayerPositionsAfterStyleChange();
    void updateLayerPositionsAfterLayout(bool isRelayoutingSubtree, bool didFullRepaint);
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

    void setReferenceBoxForPathOperations();
    void updateTransform();
    
#if ENABLE(CSS_COMPOSITING)
    void updateBlendMode();
    void willRemoveChildWithBlendMode();
#endif

    const LayoutSize& offsetForInFlowPosition() const { return m_offsetForPosition; }

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
    
    bool behavesAsFixed() const { return m_behavesAsFixed; }

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

    bool ancestorLayerIsInContainingBlockChain(const RenderLayer& ancestor, const RenderLayer* checkLimit = nullptr) const;

    // Gets the nearest enclosing positioned ancestor layer (also includes
    // the <html> layer and the root layer).
    RenderLayer* enclosingAncestorForPosition(PositionType) const;
    
    RenderLayer* enclosingLayerInContainingBlockOrder() const;
    RenderLayer* enclosingContainingBlockLayer(CrossFrameBoundaries) const;
    RenderLayer* enclosingFrameRenderLayer() const;

    // The layer relative to which clipping rects for this layer are computed.
    RenderLayer* clippingRootForPainting() const;

    RenderLayer* enclosingOverflowClipLayer(IncludeSelfOrNot) const;

    // Enclosing compositing layer; if includeSelf is true, may return this.
    RenderLayer* enclosingCompositingLayer(IncludeSelfOrNot = IncludeSelf) const;
    struct EnclosingCompositingLayerStatus {
        bool fullRepaintAlreadyScheduled { false };
        RenderLayer* layer { nullptr };
    };
    EnclosingCompositingLayerStatus enclosingCompositingLayerForRepaint(IncludeSelfOrNot = IncludeSelf) const;
    // Ancestor compositing layer, excluding this.
    RenderLayer* ancestorCompositingLayer() const { return enclosingCompositingLayer(ExcludeSelf); }

    RenderLayer* enclosingFilterLayer(IncludeSelfOrNot = IncludeSelf) const;
    RenderLayer* enclosingFilterRepaintLayer() const;
    void setFilterBackendNeedsRepaintingInRect(const LayoutRect&);
    bool hasAncestorWithFilterOutsets() const;

    bool canUseOffsetFromAncestor() const
    {
        // FIXME: This really needs to know if there are transforms on this layer and any of the layers
        // between it and the ancestor in question.
        return !renderer().hasTransform() && !renderer().isSVGRootOrLegacySVGRoot();
    }

    // FIXME: adjustForColumns allows us to position compositing layers in columns correctly, but eventually they need to be split across columns too.
    enum ColumnOffsetAdjustment { DontAdjustForColumns, AdjustForColumns };
    void convertToPixelSnappedLayerCoords(const RenderLayer* ancestorLayer, IntPoint& location, ColumnOffsetAdjustment adjustForColumns = DontAdjustForColumns) const;
    LayoutPoint convertToLayerCoords(const RenderLayer* ancestorLayer, const LayoutPoint&, ColumnOffsetAdjustment adjustForColumns = DontAdjustForColumns) const;
    LayoutSize offsetFromAncestor(const RenderLayer*, ColumnOffsetAdjustment = DontAdjustForColumns) const;

    int zIndex() const { return renderer().style().usedZIndex(); }

    enum class PaintLayerFlag : uint16_t {
        HaveTransparency                      = 1 << 0,
        AppliedTransform                      = 1 << 1,
        TemporaryClipRects                    = 1 << 2,
        PaintingReflection                    = 1 << 3,
        PaintingOverlayScrollbars             = 1 << 4,
        PaintingCompositingBackgroundPhase    = 1 << 5,
        PaintingCompositingForegroundPhase    = 1 << 6,
        PaintingCompositingMaskPhase          = 1 << 7,
        PaintingCompositingClipPathPhase      = 1 << 8,
        PaintingCompositingScrollingPhase     = 1 << 9,
        PaintingOverflowContents              = 1 << 10,
        PaintingRootBackgroundOnly            = 1 << 11,
        PaintingSkipRootBackground            = 1 << 12,
        PaintingChildClippingMaskPhase        = 1 << 13,
        CollectingEventRegion                 = 1 << 14,
    };
    static constexpr OptionSet<PaintLayerFlag> paintLayerPaintingCompositingAllPhasesFlags() { return { PaintLayerFlag::PaintingCompositingBackgroundPhase, PaintLayerFlag::PaintingCompositingForegroundPhase }; }

    enum class SecurityOriginPaintPolicy { AnyOrigin, AccessibleOriginOnly };

    // The two main functions that use the layer system.  The paint method
    // paints the layers that intersect the damage rect from back to
    // front.  The hitTest method looks for mouse events by walking
    // layers that intersect the point from front to back.
    void paint(GraphicsContext&, const LayoutRect& damageRect, const LayoutSize& subpixelOffset = LayoutSize(), OptionSet<PaintBehavior> = PaintBehavior::Normal,
        RenderObject* subtreePaintRoot = nullptr, OptionSet<PaintLayerFlag> = { }, SecurityOriginPaintPolicy = SecurityOriginPaintPolicy::AnyOrigin, EventRegionContext* = nullptr);
    bool hitTest(const HitTestRequest&, HitTestResult&);
    bool hitTest(const HitTestRequest&, const HitTestLocation&, HitTestResult&);

    enum class ClipRectsOption : uint8_t {
        RespectOverflowClip         = 1 << 0,
        IncludeOverlayScrollbarSize = 1 << 1,
    };

    static constexpr OptionSet<ClipRectsOption> clipRectOptionsForPaintingOverflowControls = { };
    static constexpr OptionSet<ClipRectsOption> clipRectDefaultOptions = { ClipRectsOption::RespectOverflowClip };

    struct ClipRectsContext {
        ClipRectsContext(const RenderLayer* inRootLayer, ClipRectsType inClipRectsType, OptionSet<ClipRectsOption> inOptions = clipRectDefaultOptions)
            : rootLayer(inRootLayer)
            , clipRectsType(inClipRectsType)
            , options(inOptions)
        { }
        const RenderLayer* rootLayer;
        ClipRectsType clipRectsType;
        OptionSet<ClipRectsOption> options;
        
        bool respectOverflowClip() const { return options.contains(ClipRectsOption::RespectOverflowClip); }
        OverlayScrollbarSizeRelevancy overlayScrollbarSizeRelevancy() const { return options.contains(ClipRectsOption::IncludeOverlayScrollbarSize) ? IncludeOverlayScrollbarSize : IgnoreOverlayScrollbarSize; }
    };

    // This method figures out our layerBounds in coordinates relative to
    // |rootLayer|. It also computes our background and foreground clip rects
    // for painting/event handling.
    // Pass offsetFromRoot if known.
    void calculateRects(const ClipRectsContext&, const LayoutRect& paintDirtyRect, LayoutRect& layerBounds, ClipRect& backgroundRect, ClipRect& foregroundRect, const LayoutSize& offsetFromRoot) const;

    // Public just for RenderTreeAsText.
    void collectFragments(LayerFragments&, const RenderLayer* rootLayer, const LayoutRect& dirtyRect,
        PaginationInclusionMode, ClipRectsType, OptionSet<ClipRectsOption>, const LayoutSize& offsetFromRoot,
        const LayoutRect* layerBoundingBox = nullptr, ShouldApplyRootOffsetToFragments = IgnoreRootOffsetForFragments);
        
    LayoutRect childrenClipRect() const; // Returns the foreground clip rect of the layer in the document's coordinate space.
    LayoutRect selfClipRect() const; // Returns the background clip rect of the layer in the document's coordinate space.
    LayoutRect localClipRect(bool& clipExceedsBounds) const; // Returns the background clip rect of the layer in the local coordinate space.

    bool clipCrossesPaintingBoundary() const;

    // Pass offsetFromRoot if known.
    bool intersectsDamageRect(const LayoutRect& layerBounds, const LayoutRect& damageRect, const RenderLayer* rootLayer, const LayoutSize& offsetFromRoot, const std::optional<LayoutRect>& cachedBoundingBox = std::nullopt) const;

    enum CalculateLayerBoundsFlag {
        IncludeSelfTransform                    = 1 << 0,
        UseLocalClipRectIfPossible              = 1 << 1,
        IncludeFilterOutsets                    = 1 << 2,
        IncludePaintedFilterOutsets             = 1 << 3,
        ExcludeHiddenDescendants                = 1 << 4,
        DontConstrainForMask                    = 1 << 5,
        IncludeCompositedDescendants            = 1 << 6,
        UseFragmentBoxesExcludingCompositing    = 1 << 7,
        UseFragmentBoxesIncludingCompositing    = 1 << 8,
    };
    static constexpr OptionSet<CalculateLayerBoundsFlag> defaultCalculateLayerBoundsFlags() { return { IncludeSelfTransform, UseLocalClipRectIfPossible, IncludePaintedFilterOutsets, UseFragmentBoxesExcludingCompositing }; }

    // Bounding box relative to some ancestor layer. Pass offsetFromRoot if known.
    LayoutRect boundingBox(const RenderLayer* rootLayer, const LayoutSize& offsetFromRoot = LayoutSize(), OptionSet<CalculateLayerBoundsFlag> = { }) const;
    // Bounding box in the coordinates of this layer.
    LayoutRect localBoundingBox(OptionSet<CalculateLayerBoundsFlag> = { }) const;
    // Deprecated: Pixel snapped bounding box relative to the root.
    WEBCORE_EXPORT IntRect absoluteBoundingBox() const;
    // Device pixel snapped bounding box relative to the root. absoluteBoundingBox() callers will be directed to this.
    FloatRect absoluteBoundingBoxForPainting() const;
    // Returns the 'reference box' used for clip-path handling (different rules for inlines, wrt. to boxes).
    FloatRect referenceBoxRectForClipPath(CSSBoxType, const LayoutSize& offsetFromRoot, const LayoutRect& rootRelativeBounds) const;

    // Bounds used for layer overlap testing in RenderLayerCompositor.
    LayoutRect overlapBounds() const;
    
    // Takes transform animations into account, returning true if they could be cheaply computed.
    // Unlike overlapBounds, these bounds include descendant layers.
    bool getOverlapBoundsIncludingChildrenAccountingForTransformAnimations(LayoutRect&, OptionSet<CalculateLayerBoundsFlag> additionalFlags = { }) const;

    // If true, this layer's children are included in its bounds for overlap testing.
    // We can't rely on the children's positions if this layer has a filter that could have moved the children's pixels around.
    bool overlapBoundsIncludeChildren() const { return hasFilter() && renderer().style().filter().hasFilterThatMovesPixels(); }

    // Can pass offsetFromRoot if known.
    LayoutRect calculateLayerBounds(const RenderLayer* ancestorLayer, const LayoutSize& offsetFromRoot, OptionSet<CalculateLayerBoundsFlag> = defaultCalculateLayerBoundsFlags()) const;
    
    LayoutRect repaintRectIncludingNonCompositingDescendants() const;

    void setRepaintStatus(RepaintStatus status) { m_repaintStatus = status; }
    RepaintStatus repaintStatus() const { return static_cast<RepaintStatus>(m_repaintStatus); }
    bool needsFullRepaint() const { return m_repaintStatus == NeedsFullRepaint || m_repaintStatus == NeedsFullRepaintForPositionedMovementLayout; }

    LayoutUnit staticInlinePosition() const { return m_offsetForPosition.width(); }
    LayoutUnit staticBlockPosition() const { return m_offsetForPosition.height(); }
   
    void setStaticInlinePosition(LayoutUnit position) { m_offsetForPosition.setWidth(position); }
    void setStaticBlockPosition(LayoutUnit position) { m_offsetForPosition.setHeight(position); }

    bool hasTransform() const { return renderer().hasTransform(); }
    // Note that this transform has the transform-origin baked in.
    TransformationMatrix* transform() const { return m_transform.get(); }
    // updateTransformFromStyle computes a transform according to the passed options (e.g. transform-origin baked in or excluded) and the given style.
    void updateTransformFromStyle(TransformationMatrix&, const RenderStyle&, OptionSet<RenderStyle::TransformOperationOption>) const;
    // currentTransform computes a transform which takes accelerated animations into account. The
    // resulting transform has transform-origin baked in, unless non-default options are given. If
    // the layer does not have a transform, the identity matrix is returned.
    TransformationMatrix currentTransform(OptionSet<RenderStyle::TransformOperationOption> = RenderStyle::allTransformOperations) const;
    TransformationMatrix renderableTransform(OptionSet<PaintBehavior>) const;
    
    // Get the children transform (to apply a perspective on children), which is applied to transformed sublayers, but not this layer.
    // Returns true if the layer has a perspective.
    // Note that this transform has the perspective-origin baked in.
    TransformationMatrix perspectiveTransform() const;
    FloatPoint perspectiveOrigin() const;
    FloatPoint3D transformOriginPixelSnappedIfNeeded() const;
    bool preserves3D() const { return renderer().style().preserves3D(); }
    bool has3DTransform() const { return m_transform && !m_transform->isAffine(); }
    bool hasTransformedAncestor() const { return m_hasTransformedAncestor; }

    bool hasFixedContainingBlockAncestor() const { return m_hasFixedContainingBlockAncestor; }

    bool hasFilter() const { return renderer().hasFilter(); }
    bool hasFilterOutsets() const { return !filterOutsets().isZero(); }
    IntOutsets filterOutsets() const;
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

    bool isolatesCompositedBlending() const { return m_hasNotIsolatedCompositedBlendingDescendants && isCSSStackingContext(); }
    bool hasNotIsolatedCompositedBlendingDescendants() const { return m_hasNotIsolatedCompositedBlendingDescendants; }
    void setHasNotIsolatedCompositedBlendingDescendants(bool hasNotIsolatedCompositedBlendingDescendants)
    {
        m_hasNotIsolatedCompositedBlendingDescendants = hasNotIsolatedCompositedBlendingDescendants;
    }

    bool isolatesBlending() const { return hasNotIsolatedBlendingDescendants() && isCSSStackingContext(); }
    
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
    bool hasCompositedNonContainedDescendants() const { return m_hasCompositedNonContainedDescendants; }

    // If non-null, a non-ancestor composited layer that this layer paints into (it is sharing its backing store with this layer).
    RenderLayer* backingProviderLayer() const { return m_backingProviderLayer.get(); }
    void setBackingProviderLayer(RenderLayer*);
    void disconnectFromBackingProviderLayer();

    bool paintsIntoProvidedBacking() const { return !!m_backingProviderLayer; }

    RenderLayerBacking* backing() const { return m_backing.get(); }
    RenderLayerBacking* ensureBacking();
    void clearBacking(bool layerBeingDestroyed = false);

    bool hasCompositedScrollingAncestor() const { return m_hasCompositedScrollingAncestor; }
    void setHasCompositedScrollingAncestor(bool hasCompositedScrollingAncestor) { m_hasCompositedScrollingAncestor = hasCompositedScrollingAncestor; }

    bool usesCompositedScrolling() const;

    // Layers with the same ScrollingScope are scrolled by some common ancestor scroller. Used for async scrolling.
    std::optional<ScrollingScope> boxScrollingScope() const { return m_boxScrollingScope; }
    std::optional<ScrollingScope> contentsScrollingScope() const { return m_contentsScrollingScope; }

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

    bool paintsWithFilters() const;
    bool requiresFullLayerImageForFilters() const;

    Element* enclosingElement() const;

    static Vector<RenderLayer*> topLayerRenderLayers(const RenderView&);

    bool establishesTopLayer() const;
    void establishesTopLayerWillChange();
    void establishesTopLayerDidChange();

    enum ViewportConstrainedNotCompositedReason {
        NoNotCompositedReason,
        NotCompositedForBoundsOutOfView,
        NotCompositedForNonViewContainer,
        NotCompositedForNoVisibleContent,
    };

    void setViewportConstrainedNotCompositedReason(ViewportConstrainedNotCompositedReason reason) { m_viewportConstrainedNotCompositedReason = reason; }
    ViewportConstrainedNotCompositedReason viewportConstrainedNotCompositedReason() const { return static_cast<ViewportConstrainedNotCompositedReason>(m_viewportConstrainedNotCompositedReason); }

    IndirectCompositingReason indirectCompositingReason() const { return static_cast<IndirectCompositingReason>(m_indirectCompositingReason); }

    bool isRenderFragmentedFlow() const { return renderer().isRenderFragmentedFlow(); }
    bool isInsideFragmentedFlow() const { return renderer().fragmentedFlowState() != RenderObject::NotInsideFragmentedFlow; }
    bool isDirtyRenderFragmentedFlow() const
    {
        ASSERT(isRenderFragmentedFlow());
        return zOrderListsDirty() || normalFlowListDirty();
    }

    RenderLayer* enclosingFragmentedFlowAncestor() const;

    WEBCORE_EXPORT void simulateFrequentPaint();
    bool paintingFrequently() const { return m_paintFrequencyTracker.paintingFrequently(); }

    WEBCORE_EXPORT bool isTransparentRespectingParentFrames() const;

    // Invalidation can fail if there is no enclosing compositing layer (e.g. nested iframe)
    // or the layer does not maintain an event region.
    enum class EventRegionInvalidationReason { Paint, SettingDidChange, Style, NonCompositedFrame };
    bool invalidateEventRegion(EventRegionInvalidationReason);

    String debugDescription() const;

    bool setIsOpportunisticStackingContext(bool);

private:

    void setNextSibling(RenderLayer* next) { m_next = next; }
    void setPreviousSibling(RenderLayer* prev) { m_previous = prev; }
    void setParent(RenderLayer*);
    void setFirstChild(RenderLayer* first) { m_first = first; }
    void setLastChild(RenderLayer* last) { m_last = last; }

    void updateAncestorDependentState();

    void dirtyPaintOrderListsOnChildChange(RenderLayer&);

    bool shouldBeNormalFlowOnly() const;
    bool shouldBeCSSStackingContext() const;

    // Return true if changed.
    bool setIsNormalFlowOnly(bool);

    bool setIsCSSStackingContext(bool);
    
    void isStackingContextChanged();

    bool isDirtyStackingContext() const { return m_zOrderListsDirty && isStackingContext(); }

    void updateZOrderLists();
    void rebuildZOrderLists();
    void rebuildZOrderLists(std::unique_ptr<Vector<RenderLayer*>>&, std::unique_ptr<Vector<RenderLayer*>>&, OptionSet<Compositing>&);
    void collectLayers(bool includeHiddenLayers, std::unique_ptr<Vector<RenderLayer*>>&, std::unique_ptr<Vector<RenderLayer*>>&, OptionSet<Compositing>&);
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
        EventRegionContext* eventRegionContext { nullptr };
    };

    LayoutPoint paintOffsetForRenderer(const LayerFragment& fragment, const LayerPaintingInfo& paintingInfo) const
    {
        return toLayoutPoint(fragment.layerBounds.location() - rendererLocation() + paintingInfo.subpixelOffset);
    }

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

    void setRepaintRects(const LayerRepaintRects&);
    void clearRepaintRects();

    LayoutRect clipRectRelativeToAncestor(RenderLayer* ancestor, LayoutSize offsetFromAncestor, const LayoutRect& constrainingRect) const;

    void clipToRect(GraphicsContext&, GraphicsContextStateSaver&, EventRegionContextStateSaver&, const LayerPaintingInfo&, OptionSet<PaintBehavior>, const ClipRect&, BorderRadiusClippingRule = IncludeSelfForBorderRadius);

    bool shouldRepaintAfterLayout() const;

    void updateSelfPaintingLayer();

    void willUpdateLayerPositions();

    enum UpdateLayerPositionsFlag {
        CheckForRepaint                     = 1 << 0,
        NeedsFullRepaintInBacking           = 1 << 1,
        ContainingClippingLayerChangedSize  = 1 << 2,
        UpdatePagination                    = 1 << 3,
        SeenFixedLayer                      = 1 << 4,
        SeenFixedContainingBlockLayer       = 1 << 5,
        SeenTransformedLayer                = 1 << 6,
        Seen3DTransformedLayer              = 1 << 7,
        SeenCompositedScrollingLayer        = 1 << 8,
    };
    static OptionSet<UpdateLayerPositionsFlag> flagsForUpdateLayerPositions(RenderLayer& startingLayer);

    // Returns true if the position changed.
    bool updateLayerPosition(OptionSet<UpdateLayerPositionsFlag>* = nullptr);

    void recursiveUpdateLayerPositions(RenderGeometryMap*, OptionSet<UpdateLayerPositionsFlag>);

    enum UpdateLayerPositionsAfterScrollFlag {
        IsOverflowScroll                        = 1 << 0,
        HasSeenViewportConstrainedAncestor      = 1 << 1,
        HasSeenAncestorWithOverflowClip         = 1 << 2,
        HasChangedAncestor                      = 1 << 3,
    };
    void recursiveUpdateLayerPositionsAfterScroll(RenderGeometryMap*, OptionSet<UpdateLayerPositionsAfterScrollFlag> = { });

    RenderLayer* enclosingPaginationLayerInSubtree(const RenderLayer* rootLayer, PaginationInclusionMode) const;

    LayoutPoint rendererLocation() const
    {
        if (auto* box = dynamicDowncast<RenderBox>(renderer()))
            return box->location();
#if ENABLE(LAYER_BASED_SVG_ENGINE)
        if (auto* svgModelObject = dynamicDowncast<RenderSVGModelObject>(renderer()))
            return svgModelObject->currentSVGLayoutLocation();
#endif
        return { };
    }

    LayoutRect rendererBorderBoxRect() const
    {
        if (auto* box = dynamicDowncast<RenderBox>(renderer()))
            return box->borderBoxRect();
#if ENABLE(LAYER_BASED_SVG_ENGINE)
        if (auto* svgModelObject = dynamicDowncast<RenderSVGModelObject>(renderer()))
            return svgModelObject->borderBoxRectEquivalent();
#endif
        return { };
    }

    LayoutRect rendererBorderBoxRectInFragment(RenderFragmentContainer* fragment, RenderBox::RenderBoxFragmentInfoFlags flags = RenderBox::CacheRenderBoxFragmentInfo) const
    {
        if (auto* box = dynamicDowncast<RenderBox>(renderer()))
            return box->borderBoxRectInFragment(fragment, flags);
#if ENABLE(LAYER_BASED_SVG_ENGINE)
        if (auto* svgModelObject = dynamicDowncast<RenderSVGModelObject>(renderer()))
            return svgModelObject->borderBoxRectInFragmentEquivalent(fragment, flags);
#endif
        return { };
    }

    LayoutRect rendererVisualOverflowRect() const
    {
        if (auto* box = dynamicDowncast<RenderBox>(renderer()))
            return box->visualOverflowRect();
#if ENABLE(LAYER_BASED_SVG_ENGINE)
        if (auto* svgModelObject = dynamicDowncast<RenderSVGModelObject>(renderer()))
            return svgModelObject->visualOverflowRectEquivalent();
#endif
        return { };
    }

    LayoutRect rendererOverflowClipRect(const LayoutPoint& location, RenderFragmentContainer* fragment, OverlayScrollbarSizeRelevancy relevancy) const
    {
        if (auto* box = dynamicDowncast<RenderBox>(renderer()))
            return box->overflowClipRect(location, fragment, relevancy);
#if ENABLE(LAYER_BASED_SVG_ENGINE)
        if (auto* svgModelObject = dynamicDowncast<RenderSVGModelObject>(renderer()))
            return svgModelObject->overflowClipRect(location, fragment, relevancy);
#endif
        return { };
    }

    LayoutRect rendererOverflowClipRectForChildLayers(const LayoutPoint& location, RenderFragmentContainer* fragment, OverlayScrollbarSizeRelevancy relevancy) const
    {
        if (auto* box = dynamicDowncast<RenderBox>(renderer()))
            return box->overflowClipRectForChildLayers(location, fragment, relevancy);
#if ENABLE(LAYER_BASED_SVG_ENGINE)
        if (auto* svgModelObject = dynamicDowncast<RenderSVGModelObject>(renderer()))
            return svgModelObject->overflowClipRectForChildLayers(location, fragment, relevancy);
#endif
        return { };
    }

    bool rendererHasVisualOverflow() const
    {
        if (auto* box = dynamicDowncast<RenderBox>(renderer()))
            return box->hasVisualOverflow();
#if ENABLE(LAYER_BASED_SVG_ENGINE)
        if (auto* svgModelObject = dynamicDowncast<RenderSVGModelObject>(renderer()))
            return svgModelObject->hasVisualOverflow();
#endif
        return false;
    }

    bool setupFontSubpixelQuantization(GraphicsContext&, bool& didQuantizeFonts);

    std::pair<Path, WindRule> computeClipPath(const LayoutSize& offsetFromRoot, const LayoutRect& rootRelativeBoundsForNonBoxes) const;

    void setupClipPath(GraphicsContext&, GraphicsContextStateSaver&, const LayerPaintingInfo&, const LayoutSize& offsetFromRoot);

    void ensureLayerFilters();
    void clearLayerFilters();

    void updateLayerScrollableArea();
    void clearLayerScrollableArea();

    RenderLayerFilters* filtersForPainting(GraphicsContext&, OptionSet<PaintLayerFlag>) const;
    GraphicsContext* setupFilters(GraphicsContext& destinationContext, LayerPaintingInfo&, OptionSet<PaintLayerFlag>, const LayoutSize& offsetFromRoot);
    void applyFilters(GraphicsContext& originalContext, const LayerPaintingInfo&, OptionSet<PaintBehavior>, const LayerFragments&);

    void paintLayer(GraphicsContext&, const LayerPaintingInfo&, OptionSet<PaintLayerFlag>);
    void paintLayerWithEffects(GraphicsContext&, const LayerPaintingInfo&, OptionSet<PaintLayerFlag>);

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
    void collectEventRegionForFragments(const LayerFragments&, GraphicsContext&, const LayerPaintingInfo&, OptionSet<PaintBehavior>);

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
    RenderLayer* hitTestTransformedLayerInFragments(RenderLayer* rootLayer, RenderLayer* containerLayer, const HitTestRequest&, HitTestResult&,
        const LayoutRect& hitTestRect, const HitTestLocation&, const HitTestingTransformState* = nullptr, double* zOffset = nullptr);

    bool listBackgroundIsKnownToBeOpaqueInRect(const LayerList&, const LayoutRect&) const;

    bool shouldBeSelfPaintingLayer() const;

    void dirtyAncestorChainVisibleDescendantStatus();
    void setAncestorChainHasVisibleDescendant();
    
    bool computeHasVisibleContent() const;

    bool has3DTransformedDescendant() const { return m_has3DTransformedDescendant; }
    bool has3DTransformedAncestor() const { return m_has3DTransformedAncestor; }

    void dirty3DTransformedDescendantStatus();
    // Both updates the status, and returns true if descendants of this have 3d.
    bool update3DTransformedDescendantStatus();
    
    bool isInsideSVGForeignObject() const { return m_insideSVGForeignObject; }

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

    // Convert a point in absolute coords into layer coords, taking transforms into account
    LayoutPoint absoluteToContents(const LayoutPoint&) const;

    void updatePagination();

    void setHasCompositingDescendant(bool b)  { m_hasCompositingDescendant = b; }
    void setHasCompositedNonContainedDescendants(bool value) { m_hasCompositedNonContainedDescendants = value; }

    void setIndirectCompositingReason(IndirectCompositingReason reason) { m_indirectCompositingReason = static_cast<unsigned>(reason); }
    bool mustCompositeForIndirectReasons() const { return m_indirectCompositingReason; }

    struct OverflowControlRects {
        IntRect horizontalScrollbar;
        IntRect verticalScrollbar;
        IntRect scrollCorner;
        IntRect resizer;
        IntRect scrollCornerOrResizerRect() const
        {
            return !scrollCorner.isEmpty() ? scrollCorner : resizer;
        }
    };
    OverflowControlRects overflowControlsRects() const;

    OptionSet<Compositing> m_compositingDirtyBits;

    const bool m_isRenderViewLayer : 1;
    const bool m_forcedStackingContext : 1;

    bool m_isNormalFlowOnly : 1;
    bool m_isCSSStackingContext : 1;
    bool m_isOpportunisticStackingContext : 1;

    bool m_zOrderListsDirty : 1;
    bool m_normalFlowListDirty: 1;
    bool m_hadNegativeZOrderList : 1;

    // Keeps track of whether the layer is currently resizing, so events can cause resizing to start and stop.
    bool m_inResizeMode : 1;

    bool m_isSelfPaintingLayer : 1;

    // If have no self-painting descendants, we don't have to walk our children during painting. This can lead to
    // significant savings, especially if the tree has lots of non-self-painting layers grouped together (e.g. table cells).
    bool m_hasSelfPaintingLayerDescendant : 1;
    bool m_hasSelfPaintingLayerDescendantDirty : 1;

    bool m_usedTransparency : 1; // Tracks whether we need to close a transparent layer, i.e., whether
                                 // we ended up painting this layer or any descendants (and therefore need to
                                 // blend).
    bool m_paintingInsideReflection : 1;  // A state bit tracking if we are painting inside a replica.
    unsigned m_repaintStatus : 2; // RepaintStatus

    bool m_visibleContentStatusDirty : 1;
    bool m_hasVisibleContent : 1;
    bool m_visibleDescendantStatusDirty : 1;
    bool m_hasVisibleDescendant : 1;
    bool m_isFixedIntersectingViewport : 1;
    bool m_behavesAsFixed : 1;

    bool m_3DTransformedDescendantStatusDirty : 1;
    bool m_has3DTransformedDescendant : 1;  // Set on a stacking context layer that has 3D descendants anywhere
                                            // in a preserves3D hierarchy. Hint to do 3D-aware hit testing.
    bool m_hasCompositingDescendant : 1; // In the z-order tree.
    bool m_hasCompositedNonContainedDescendants : 1; // Set when a layer has a composited descendant in z-order which is not a descendant in containing block order (e.g. opacity layer with an abspos descendant).

    bool m_hasCompositedScrollingAncestor : 1; // In the layer-order tree.

    bool m_hasFixedContainingBlockAncestor : 1;
    bool m_hasTransformedAncestor : 1;
    bool m_has3DTransformedAncestor : 1;

    bool m_insideSVGForeignObject : 1;

    unsigned m_indirectCompositingReason : 4; // IndirectCompositingReason
    unsigned m_viewportConstrainedNotCompositedReason : 2; // ViewportConstrainedNotCompositedReason

#if ASSERT_ENABLED
    bool m_layerListMutationAllowed : 1;
#endif

#if ENABLE(CSS_COMPOSITING)
    unsigned m_blendMode : 5; // BlendMode
    bool m_hasNotIsolatedCompositedBlendingDescendants : 1;
    bool m_hasNotIsolatedBlendingDescendants : 1;
    bool m_hasNotIsolatedBlendingDescendantsStatusDirty : 1;
#endif
    bool m_repaintRectsValid : 1;

    RenderLayerModelObject& m_renderer;

    RenderLayer* m_parent { nullptr };
    RenderLayer* m_previous { nullptr };
    RenderLayer* m_next { nullptr };
    RenderLayer* m_first { nullptr };
    RenderLayer* m_last { nullptr };

    WeakPtr<RenderLayer> m_backingProviderLayer;

    // For layers that establish stacking contexts, m_posZOrderList holds a sorted list of all the
    // descendant layers within the stacking context that have z-indices of 0 or greater
    // (auto will count as 0). m_negZOrderList holds descendants within our stacking context with negative
    // z-indices.
    std::unique_ptr<Vector<RenderLayer*>> m_posZOrderList;
    std::unique_ptr<Vector<RenderLayer*>> m_negZOrderList;

    // This list contains child layers that cannot create stacking contexts and appear in normal flow order.
    std::unique_ptr<Vector<RenderLayer*>> m_normalFlowList;

    // Only valid if m_repaintRectsValid is set (std::optional<> not used to avoid padding).
    LayerRepaintRects m_repaintRects;

    // Our current relative or absolute position offset.
    LayoutSize m_offsetForPosition;

    // Our (x,y) coordinates are in our parent layer's coordinate space.
    LayoutPoint m_topLeft;

    // The layer's width/height
    IntSize m_layerSize;

    std::unique_ptr<ClipRectsCache> m_clipRectsCache;

    Markable<ScrollingScope, IntegralMarkableTraits<ScrollingScope, 0>> m_boxScrollingScope;
    Markable<ScrollingScope, IntegralMarkableTraits<ScrollingScope, 0>> m_contentsScrollingScope;

    std::unique_ptr<TransformationMatrix> m_transform;
    
    // May ultimately be extended to many replicas (with their own paint order).
    RenderPtr<RenderReplica> m_reflection;

    // Pointer to the enclosing RenderLayer that caused us to be paginated. It is 0 if we are not paginated.
    WeakPtr<RenderLayer> m_enclosingPaginationLayer;

    IntRect m_blockSelectionGapsBounds;

    std::unique_ptr<RenderLayerFilters> m_filters;
    std::unique_ptr<RenderLayerBacking> m_backing;
    std::unique_ptr<RenderLayerScrollableArea> m_scrollableArea;

    PaintFrequencyTracker m_paintFrequencyTracker;
};

inline void RenderLayer::clearZOrderLists()
{
    ASSERT(!isStackingContext());
    ASSERT(layerListMutationAllowed());

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

inline RenderLayer* RenderLayer::paintOrderParent() const
{
    return m_isNormalFlowOnly ? m_parent : stackingContext();
}

#if ASSERT_ENABLED
class LayerListMutationDetector {
public:
    LayerListMutationDetector(RenderLayer& layer)
        : m_layer(layer)
        , m_previousMutationAllowedState(layer.layerListMutationAllowed())
    {
        m_layer.setLayerListMutationAllowed(false);
    }
    
    ~LayerListMutationDetector()
    {
        m_layer.setLayerListMutationAllowed(m_previousMutationAllowedState);
    }

private:
    RenderLayer& m_layer;
    bool m_previousMutationAllowedState;
};
#endif // ASSERT_ENABLED

void makeMatrixRenderable(TransformationMatrix&, bool has3DRendering);

bool compositedWithOwnBackingStore(const RenderLayer&);

WTF::TextStream& operator<<(WTF::TextStream&, ClipRectsType);
WTF::TextStream& operator<<(WTF::TextStream&, const RenderLayer&);
WTF::TextStream& operator<<(WTF::TextStream&, const RenderLayer::ClipRectsContext&);
WTF::TextStream& operator<<(WTF::TextStream&, IndirectCompositingReason);
WTF::TextStream& operator<<(WTF::TextStream&, PaintBehavior);

} // namespace WebCore

#if ENABLE(TREE_DEBUGGING)
// Outside the WebCore namespace for ease of invocation from lldb.
void showLayerTree(const WebCore::RenderLayer*);
void showLayerTree(const WebCore::RenderObject*);
void showPaintOrderTree(const WebCore::RenderLayer*);
void showPaintOrderTree(const WebCore::RenderObject*);
#endif

