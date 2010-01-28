/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef RenderLayerCompositor_h
#define RenderLayerCompositor_h

#include "RenderLayer.h"
#include "RenderLayerBacking.h"

namespace WebCore {

#define PROFILE_LAYER_REBUILD 0

class GraphicsLayer;
#if ENABLE(VIDEO)
class RenderVideo;
#endif

enum CompositingUpdateType {
    CompositingUpdateAfterLayoutOrStyleChange,
    CompositingUpdateOnPaitingOrHitTest,
    CompositingUpdateOnScroll
};

// RenderLayerCompositor manages the hierarchy of
// composited RenderLayers. It determines which RenderLayers
// become compositing, and creates and maintains a hierarchy of
// GraphicsLayers based on the RenderLayer painting order.
// 
// There is one RenderLayerCompositor per RenderView.

class RenderLayerCompositor {
public:
    RenderLayerCompositor(RenderView*);
    ~RenderLayerCompositor();
    
    // Return true if this RenderView is in "compositing mode" (i.e. has one or more
    // composited RenderLayers)
    bool inCompositingMode() const { return m_compositing; }
    // This will make a compositing layer at the root automatically, and hook up to
    // the native view/window system.
    void enableCompositingMode(bool enable = true);
    
    // Returns true if the accelerated compositing is enabled
    bool hasAcceleratedCompositing() const { return m_hasAcceleratedCompositing; }
    
    bool showDebugBorders() const { return m_showDebugBorders; }
    bool showRepaintCounter() const { return m_showRepaintCounter; }
    
    // Copy the accelerated compositing related flags from Settings
    void cacheAcceleratedCompositingFlags();

    // Called when the layer hierarchy needs to be updated (compositing layers have been
    // created, destroyed or re-parented).
    void setCompositingLayersNeedRebuild(bool needRebuild = true);
    bool compositingLayersNeedRebuild() const { return m_compositingLayersNeedRebuild; }

    // Controls whether or not to consult geometry when deciding which layers need
    // to be composited. Defaults to true.
    void setCompositingConsultsOverlap(bool b) { m_compositingConsultsOverlap = b; }
    bool compositingConsultsOverlap() const { return m_compositingConsultsOverlap; }
    
    void scheduleSync();
    
    // Rebuild the tree of compositing layers
    void updateCompositingLayers(CompositingUpdateType = CompositingUpdateAfterLayoutOrStyleChange, RenderLayer* updateRoot = 0);

    // Update the compositing state of the given layer. Returns true if that state changed.
    enum CompositingChangeRepaint { CompositingChangeRepaintNow, CompositingChangeWillRepaintLater };
    bool updateLayerCompositingState(RenderLayer*, CompositingChangeRepaint = CompositingChangeRepaintNow);

    // Update the geometry for compositing children of compositingAncestor.
    void updateCompositingDescendantGeometry(RenderLayer* compositingAncestor, RenderLayer* layer, RenderLayerBacking::UpdateDepth);
    
    // Whether layer's backing needs a graphics layer to do clipping by an ancestor (non-stacking-context parent with overflow).
    bool clippedByAncestor(RenderLayer*) const;
    // Whether layer's backing needs a graphics layer to clip z-order children of the given layer.
    bool clipsCompositingDescendants(const RenderLayer*) const;

    // Whether the given layer needs an extra 'contents' layer.
    bool needsContentsCompositingLayer(const RenderLayer*) const;
    // Return the bounding box required for compositing layer and its childern, relative to ancestorLayer.
    // If layerBoundingBox is not 0, on return it contains the bounding box of this layer only.
    IntRect calculateCompositedBounds(const RenderLayer* layer, const RenderLayer* ancestorLayer);

    // Repaint the appropriate layers when the given RenderLayer starts or stops being composited.
    void repaintOnCompositingChange(RenderLayer*);
    
    // Notify us that a layer has been added or removed
    void layerWasAdded(RenderLayer* parent, RenderLayer* child);
    void layerWillBeRemoved(RenderLayer* parent, RenderLayer* child);

    // Get the nearest ancestor layer that has overflow or clip, but is not a stacking context
    RenderLayer* enclosingNonStackingClippingLayer(const RenderLayer* layer) const;

    // Repaint parts of all composited layers that intersect the given absolute rectangle.
    void repaintCompositedLayersAbsoluteRect(const IntRect&);

    RenderLayer* rootRenderLayer() const;
    GraphicsLayer* rootPlatformLayer() const;

    void didMoveOnscreen();
    void willMoveOffscreen();

    void updateRootLayerPosition();
    
    void didStartAcceleratedAnimation();
    
#if ENABLE(VIDEO)
    // Use by RenderVideo to ask if it should try to use accelerated compositing.
    bool canAccelerateVideoRendering(RenderVideo*) const;
#endif

    // Walk the tree looking for layers with 3d transforms. Useful in case you need
    // to know if there is non-affine content, e.g. for drawing into an image.
    bool has3DContent() const;

private:
    // Whether the given RL needs a compositing layer.
    bool needsToBeComposited(const RenderLayer*) const;
    // Whether the layer has an intrinsic need for compositing layer.
    bool requiresCompositingLayer(const RenderLayer*) const;

    // Make or destroy the backing for this layer; returns true if backing changed.
    bool updateBacking(RenderLayer*, CompositingChangeRepaint shouldRepaint);

    // Repaint the given rect (which is layer's coords), and regions of child layers that intersect that rect.
    void recursiveRepaintLayerRect(RenderLayer* layer, const IntRect& rect);

    typedef HashMap<RenderLayer*, IntRect> OverlapMap;
    static void addToOverlapMap(OverlapMap&, RenderLayer*, IntRect& layerBounds, bool& boundsComputed);
    static bool overlapsCompositedLayers(OverlapMap&, const IntRect& layerBounds);

    // Returns true if any layer's compositing changed
    void computeCompositingRequirements(RenderLayer*, OverlapMap*, struct CompositingState&, bool& layersChanged);
    
    // Recurses down the tree, parenting descendant compositing layers and collecting an array of child layers for the current compositing layer.
    void rebuildCompositingLayerTree(RenderLayer* layer, const struct CompositingState&, Vector<GraphicsLayer*>& childGraphicsLayersOfEnclosingLayer);

    // Recurses down the tree, updating layer geometry only.
    void updateLayerTreeGeometry(RenderLayer*);
    
    // Hook compositing layers together
    void setCompositingParent(RenderLayer* childLayer, RenderLayer* parentLayer);
    void removeCompositedChildren(RenderLayer*);

    void parentInRootLayer(RenderLayer*);

    bool layerHas3DContent(const RenderLayer*) const;

    void ensureRootPlatformLayer();
    void destroyRootPlatformLayer();
    
    // Whether a running transition or animation enforces the need for a compositing layer.
    bool requiresCompositingForAnimation(RenderObject*) const;
    bool requiresCompositingForTransform(RenderObject*) const;
    bool requiresCompositingForVideo(RenderObject*) const;
    bool requiresCompositingForCanvas(RenderObject*) const;
    bool requiresCompositingForPlugin(RenderObject*) const;
    bool requiresCompositingWhenDescendantsAreCompositing(RenderObject*) const;

private:
    RenderView* m_renderView;
    OwnPtr<GraphicsLayer> m_rootPlatformLayer;
    bool m_hasAcceleratedCompositing;
    bool m_showDebugBorders;
    bool m_showRepaintCounter;
    bool m_compositingConsultsOverlap;
    bool m_compositing;
    bool m_rootLayerAttached;
    bool m_compositingLayersNeedRebuild;
    
#if PROFILE_LAYER_REBUILD
    int m_rootLayerUpdateCount;
#endif
};


} // namespace WebCore

#endif // RenderLayerCompositor_h
