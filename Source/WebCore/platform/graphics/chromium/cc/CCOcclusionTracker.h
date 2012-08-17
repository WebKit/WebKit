/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CCOcclusionTracker_h
#define CCOcclusionTracker_h

#include "CCLayerIterator.h"
#include "FloatQuad.h"
#include "Region.h"

namespace WebCore {
class CCOverdrawMetrics;
class CCLayerImpl;
class CCRenderSurface;
class LayerChromium;
class RenderSurfaceChromium;

// This class is used to track occlusion of layers while traversing them in a front-to-back order. As each layer is visited, one of the
// methods in this class is called to notify it about the current target surface.
// Then, occlusion in the content space of the current layer may be queried, via methods such as occluded() and unoccludedContentRect().
// If the current layer owns a RenderSurface, then occlusion on that RenderSurface may also be queried via surfaceOccluded() and surfaceUnoccludedContentRect().
// Finally, once finished with the layer, occlusion behind the layer should be marked by calling markOccludedBehindLayer().
template<typename LayerType, typename RenderSurfaceType>
class CCOcclusionTrackerBase {
    WTF_MAKE_NONCOPYABLE(CCOcclusionTrackerBase);
public:
    CCOcclusionTrackerBase(IntRect rootTargetRect, bool recordMetricsForFrame);

    // Called at the beginning of each step in the CCLayerIterator's front-to-back traversal.
    void enterLayer(const CCLayerIteratorPosition<LayerType>&);
    // Called at the end of each step in the CCLayerIterator's front-to-back traversal.
    void leaveLayer(const CCLayerIteratorPosition<LayerType>&);

    // Returns true if the given rect in content space for the layer is fully occluded in either screen space or the layer's target surface.
    bool occluded(const LayerType*, const IntRect& contentRect, bool* hasOcclusionFromOutsideTargetSurface = 0) const;
    // Gives an unoccluded sub-rect of |contentRect| in the content space of the layer. Used when considering occlusion for a layer that paints/draws something.
    IntRect unoccludedContentRect(const LayerType*, const IntRect& contentRect, bool* hasOcclusionFromOutsideTargetSurface = 0) const;

    // Gives an unoccluded sub-rect of |contentRect| in the content space of the renderTarget owned by the layer.
    // Used when considering occlusion for a contributing surface that is rendering into another target.
    IntRect unoccludedContributingSurfaceContentRect(const LayerType*, bool forReplica, const IntRect& contentRect, bool* hasOcclusionFromOutsideTargetSurface = 0) const;

    // Report operations for recording overdraw metrics.
    CCOverdrawMetrics& overdrawMetrics() const { return *m_overdrawMetrics.get(); }

    // Gives the region of the screen that is not occluded by something opaque.
    Region computeVisibleRegionInScreen() const { return subtract(Region(m_rootTargetRect), m_stack.last().occlusionInScreen); }

    void setMinimumTrackingSize(const IntSize& size) { m_minimumTrackingSize = size; }

    // The following is used for visualization purposes.
    void setOccludingScreenSpaceRectsContainer(Vector<IntRect>* rects) { m_occludingScreenSpaceRects = rects; }

protected:
    struct StackObject {
        StackObject() : target(0) { }
        StackObject(const LayerType* target) : target(target) { }
        const LayerType* target;
        Region occlusionInScreen;
        Region occlusionInTarget;
    };

    // The stack holds occluded regions for subtrees in the RenderSurface-Layer tree, so that when we leave a subtree we may
    // apply a mask to it, but not to the parts outside the subtree.
    // - The first time we see a new subtree under a target, we add that target to the top of the stack. This can happen as a layer representing itself, or as a target surface.
    // - When we visit a target surface, we apply its mask to its subtree, which is at the top of the stack.
    // - When we visit a layer representing itself, we add its occlusion to the current subtree, which is at the top of the stack.
    // - When we visit a layer representing a contributing surface, the current target will never be the top of the stack since we just came from the contributing surface.
    // We merge the occlusion at the top of the stack with the new current subtree. This new target is pushed onto the stack if not already there.
    Vector<StackObject, 1> m_stack;

    // Allow tests to override this.
    virtual IntRect layerClipRectInTarget(const LayerType*) const;

private:
    // Called when visiting a layer representing itself. If the target was not already current, then this indicates we have entered a new surface subtree.
    void enterRenderTarget(const LayerType* newTarget);

    // Called when visiting a layer representing a target surface. This indicates we have visited all the layers within the surface, and we may
    // perform any surface-wide operations.
    void finishedRenderTarget(const LayerType* finishedTarget);

    // Called when visiting a layer representing a contributing surface. This indicates that we are leaving our current surface, and
    // entering the new one. We then perform any operations required for merging results from the child subtree into its parent.
    void leaveToRenderTarget(const LayerType* newTarget);

    // Add the layer's occlusion to the tracked state.
    void markOccludedBehindLayer(const LayerType*);

    IntRect m_rootTargetRect;
    OwnPtr<CCOverdrawMetrics> m_overdrawMetrics;
    IntSize m_minimumTrackingSize;

    // This is used for visualizing the occlusion tracking process.
    Vector<IntRect>* m_occludingScreenSpaceRects;
};

typedef CCOcclusionTrackerBase<LayerChromium, RenderSurfaceChromium> CCOcclusionTracker;
typedef CCOcclusionTrackerBase<CCLayerImpl, CCRenderSurface> CCOcclusionTrackerImpl;

}
#endif // CCOcclusionTracker_h
