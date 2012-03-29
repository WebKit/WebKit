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

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "cc/CCOcclusionTracker.h"

#include "LayerChromium.h"
#include "cc/CCLayerImpl.h"

#include <algorithm>

using namespace std;

namespace WebCore {

template<typename LayerType, typename RenderSurfaceType>
CCOcclusionTrackerBase<LayerType, RenderSurfaceType>::CCOcclusionTrackerBase(IntRect scissorRectInScreenSpace, bool recordMetricsForFrame)
    : m_scissorRectInScreenSpace(scissorRectInScreenSpace)
    , m_overdrawMetrics(CCOverdrawMetrics::create(recordMetricsForFrame))
    , m_usePaintTracking(true) // FIXME: Remove this when paint tracking is on for paint culling.
{
}

template<typename LayerType, typename RenderSurfaceType>
void CCOcclusionTrackerBase<LayerType, RenderSurfaceType>::enterTargetRenderSurface(const RenderSurfaceType* newTarget)
{
    if (!m_stack.isEmpty() && m_stack.last().surface == newTarget)
        return;

    const RenderSurfaceType* oldTarget = m_stack.isEmpty() ? 0 : m_stack.last().surface;
    const RenderSurfaceType* oldAncestorThatMovesPixels = !oldTarget ? 0 : oldTarget->nearestAncestorThatMovesPixels();
    const RenderSurfaceType* newAncestorThatMovesPixels = newTarget->nearestAncestorThatMovesPixels();

    m_stack.append(StackObject(newTarget));

    // We copy the screen occlusion into the new RenderSurface subtree, but we never copy in the
    // target occlusion, since we are looking at a new RenderSurface target.

    // If we are entering a subtree that is going to move pixels around, then the occlusion we've computed
    // so far won't apply to the pixels we're drawing here in the same way. We discard the occlusion thus
    // far to be safe, and ensure we don't cull any pixels that are moved such that they become visible.
    bool enteringSubtreeThatMovesPixels = newAncestorThatMovesPixels && newAncestorThatMovesPixels != oldAncestorThatMovesPixels;

    bool copyScreenOcclusionForward = m_stack.size() > 1 && !enteringSubtreeThatMovesPixels;
    if (copyScreenOcclusionForward) {
        int lastIndex = m_stack.size() - 1;
        m_stack[lastIndex].occlusionInScreen = m_stack[lastIndex - 1].occlusionInScreen;
    }
}

static inline bool layerOpacityKnown(const LayerChromium* layer) { return !layer->drawOpacityIsAnimating(); }
static inline bool layerOpacityKnown(const CCLayerImpl*) { return true; }
static inline bool layerTransformsToTargetKnown(const LayerChromium* layer) { return !layer->drawTransformIsAnimating(); }
static inline bool layerTransformsToTargetKnown(const CCLayerImpl*) { return true; }
static inline bool layerTransformsToScreenKnown(const LayerChromium* layer) { return !layer->screenSpaceTransformIsAnimating(); }
static inline bool layerTransformsToScreenKnown(const CCLayerImpl*) { return true; }

static inline bool surfaceOpacityKnown(const RenderSurfaceChromium* surface) { return !surface->drawOpacityIsAnimating(); }
static inline bool surfaceOpacityKnown(const CCRenderSurface*) { return true; }
static inline bool surfaceTransformsToTargetKnown(const RenderSurfaceChromium* surface) { return !surface->targetSurfaceTransformsAreAnimating(); }
static inline bool surfaceTransformsToTargetKnown(const CCRenderSurface*) { return true; }
static inline bool surfaceTransformsToScreenKnown(const RenderSurfaceChromium* surface) { return !surface->screenSpaceTransformsAreAnimating(); }
static inline bool surfaceTransformsToScreenKnown(const CCRenderSurface*) { return true; }

template<typename LayerType, typename RenderSurfaceType>
void CCOcclusionTrackerBase<LayerType, RenderSurfaceType>::finishedTargetRenderSurface(const LayerType* owningLayer, const RenderSurfaceType* finishedTarget)
{
    // FIXME: Remove the owningLayer parameter when we can get all the info from the surface.
    ASSERT(owningLayer->renderSurface() == finishedTarget);

    // Make sure we know about the target surface.
    enterTargetRenderSurface(finishedTarget);

    // If the occlusion within the surface can not be applied to things outside of the surface's subtree, then clear the occlusion here so it won't be used.
    if (owningLayer->maskLayer() || !surfaceOpacityKnown(finishedTarget) || finishedTarget->drawOpacity() < 1 || finishedTarget->filters().hasFilterThatAffectsOpacity()) {
        m_stack.last().occlusionInScreen = Region();
        m_stack.last().occlusionInTarget = Region();
    } else {
        if (!surfaceTransformsToTargetKnown(finishedTarget))
            m_stack.last().occlusionInTarget = Region();
        if (!surfaceTransformsToScreenKnown(finishedTarget))
            m_stack.last().occlusionInScreen = Region();
    }
}

template<typename RenderSurfaceType>
static inline Region transformSurfaceOpaqueRegion(const RenderSurfaceType* surface, const Region& region, const TransformationMatrix& transform)
{
    // Verify that rects within the |surface| will remain rects in its target surface after applying |transform|. If this is true, then
    // apply |transform| to each rect within |region| in order to transform the entire Region.

    FloatQuad transformedBoundsQuad = transform.mapQuad(FloatQuad(region.bounds()));
    if (!transformedBoundsQuad.isRectilinear())
        return Region();

    Region transformedRegion;

    Vector<IntRect> rects = region.rects();
    for (size_t i = 0; i < rects.size(); ++i)
        transformedRegion.unite(enclosedIntRect(transform.mapRect(FloatRect(rects[i]))));
    return transformedRegion;
}

template<typename LayerType, typename RenderSurfaceType>
void CCOcclusionTrackerBase<LayerType, RenderSurfaceType>::leaveToTargetRenderSurface(const RenderSurfaceType* newTarget)
{
    int lastIndex = m_stack.size() - 1;
    bool surfaceWillBeAtTopAfterPop = m_stack.size() > 1 && m_stack[lastIndex - 1].surface == newTarget;

    // We merge the screen occlusion from the current RenderSurface subtree out to its parent target RenderSurface.
    // The target occlusion can be merged out as well but needs to be transformed to the new target.

    const RenderSurfaceType* oldTarget = m_stack[lastIndex].surface;
    Region oldTargetOcclusionInNewTarget = transformSurfaceOpaqueRegion<RenderSurfaceType>(oldTarget, m_stack[lastIndex].occlusionInTarget, oldTarget->originTransform());

    if (surfaceWillBeAtTopAfterPop) {
        // Merge the top of the stack down.

        m_stack[lastIndex - 1].occlusionInScreen.unite(m_stack[lastIndex].occlusionInScreen);
        m_stack[lastIndex - 1].occlusionInTarget.unite(oldTargetOcclusionInNewTarget);
        m_stack.removeLast();
    } else {
        // Replace the top of the stack with the new pushed surface. Copy the occluded screen region to the top.
        m_stack.last().surface = newTarget;
        m_stack.last().occlusionInTarget = oldTargetOcclusionInNewTarget;
    }
}

template<typename LayerType>
static inline TransformationMatrix contentToScreenSpaceTransform(const LayerType* layer)
{
    ASSERT(layerTransformsToScreenKnown(layer));
    IntSize boundsInLayerSpace = layer->bounds();
    IntSize boundsInContentSpace = layer->contentBounds();

    TransformationMatrix transform = layer->screenSpaceTransform();

    if (boundsInContentSpace.isEmpty())
        return transform;

    // Scale from content space to layer space
    transform.scaleNonUniform(boundsInLayerSpace.width() / static_cast<double>(boundsInContentSpace.width()),
                              boundsInLayerSpace.height() / static_cast<double>(boundsInContentSpace.height()));

    return transform;
}

template<typename LayerType>
static inline TransformationMatrix contentToTargetSurfaceTransform(const LayerType* layer)
{
    ASSERT(layerTransformsToTargetKnown(layer));
    IntSize boundsInLayerSpace = layer->bounds();
    IntSize boundsInContentSpace = layer->contentBounds();

    TransformationMatrix transform = layer->drawTransform();

    if (boundsInContentSpace.isEmpty())
        return transform;

    // Scale from content space to layer space
    transform.scaleNonUniform(boundsInLayerSpace.width() / static_cast<double>(boundsInContentSpace.width()),
                              boundsInLayerSpace.height() / static_cast<double>(boundsInContentSpace.height()));

    // The draw transform expects the origin to be in the middle of the layer.
    transform.translate(-boundsInContentSpace.width() / 2.0, -boundsInContentSpace.height() / 2.0);

    return transform;
}

// FIXME: Remove usePaintTracking when paint tracking is on for paint culling.
template<typename LayerType>
static inline Region computeOcclusionBehindLayer(const LayerType* layer, const TransformationMatrix& transform, bool usePaintTracking)
{
    Region opaqueRegion;

    FloatQuad unoccludedQuad = transform.mapQuad(FloatQuad(layer->visibleLayerRect()));
    bool isPaintedAxisAligned = unoccludedQuad.isRectilinear();
    if (!isPaintedAxisAligned)
        return opaqueRegion;

    if (layer->opaque())
        opaqueRegion = enclosedIntRect(unoccludedQuad.boundingBox());
    else if (usePaintTracking && transform.isIdentity())
        opaqueRegion = layer->visibleContentOpaqueRegion();
    else if (usePaintTracking) {
        Region contentRegion = layer->visibleContentOpaqueRegion();
        Vector<IntRect> contentRects = contentRegion.rects();
        for (size_t i = 0; i < contentRects.size(); ++i)
            opaqueRegion.unite(enclosedIntRect(transform.mapRect(FloatRect(contentRects[i]))));
    }
    return opaqueRegion;
}

template<typename LayerType, typename RenderSurfaceType>
void CCOcclusionTrackerBase<LayerType, RenderSurfaceType>::markOccludedBehindLayer(const LayerType* layer)
{
    ASSERT(!m_stack.isEmpty());
    ASSERT(layer->targetRenderSurface() == m_stack.last().surface);
    if (m_stack.isEmpty())
        return;

    if (!layerOpacityKnown(layer) || layer->drawOpacity() < 1)
        return;

    // FIXME: Remove m_usePaintTracking when paint tracking is on for paint culling.
    if (layerTransformsToScreenKnown(layer))
        m_stack.last().occlusionInScreen.unite(computeOcclusionBehindLayer<LayerType>(layer, contentToScreenSpaceTransform<LayerType>(layer), m_usePaintTracking));
    if (layerTransformsToTargetKnown(layer))
        m_stack.last().occlusionInTarget.unite(computeOcclusionBehindLayer<LayerType>(layer, contentToTargetSurfaceTransform<LayerType>(layer), m_usePaintTracking));
}

static inline bool testContentRectOccluded(const IntRect& contentRect, const TransformationMatrix& contentSpaceTransform, const IntRect& scissorRect, const Region& occlusion)
{
    FloatRect transformedRect = contentSpaceTransform.mapRect(FloatRect(contentRect));
    // Take the enclosingIntRect, as we want to include partial pixels in the test.
    IntRect targetRect = intersection(enclosingIntRect(transformedRect), scissorRect);
    return targetRect.isEmpty() || occlusion.contains(targetRect);
}

template<typename LayerType, typename RenderSurfaceType>
bool CCOcclusionTrackerBase<LayerType, RenderSurfaceType>::occluded(const LayerType* layer, const IntRect& contentRect) const
{
    ASSERT(!m_stack.isEmpty());
    if (m_stack.isEmpty())
        return false;
    if (contentRect.isEmpty())
        return true;

    ASSERT(layer->targetRenderSurface() == m_stack.last().surface);

    if (layerTransformsToScreenKnown(layer) && testContentRectOccluded(contentRect, contentToScreenSpaceTransform<LayerType>(layer), m_scissorRectInScreenSpace, m_stack.last().occlusionInScreen))
        return true;
    if (layerTransformsToTargetKnown(layer) && testContentRectOccluded(contentRect, contentToTargetSurfaceTransform<LayerType>(layer), layerScissorRectInTargetSurface(layer), m_stack.last().occlusionInTarget))
        return true;
    return false;
}

// Determines what portion of rect, if any, is unoccluded (not occluded by region). If
// the resulting unoccluded region is not rectangular, we return a rect containing it.
static inline IntRect rectSubtractRegion(const IntRect& rect, const Region& region)
{
    Region rectRegion(rect);
    Region intersectRegion(intersect(region, rectRegion));

    if (intersectRegion.isEmpty())
        return rect;

    rectRegion.subtract(intersectRegion);
    IntRect boundsRect = rectRegion.bounds();
    return boundsRect;
}

static FloatQuad projectQuad(const TransformationMatrix& transform, const FloatQuad& q, bool& clamped)
{
    FloatQuad projectedQuad;
    bool clampedPoint;
    projectedQuad.setP1(transform.projectPoint(q.p1(), &clampedPoint));
    clamped = clampedPoint;
    projectedQuad.setP2(transform.projectPoint(q.p2(), &clampedPoint));
    clamped |= clampedPoint;
    projectedQuad.setP3(transform.projectPoint(q.p3(), &clampedPoint));
    clamped |= clampedPoint;
    projectedQuad.setP4(transform.projectPoint(q.p4(), &clampedPoint));
    clamped |= clampedPoint;

    return projectedQuad;
}

static inline IntRect computeUnoccludedContentRect(const IntRect& contentRect, const TransformationMatrix& contentSpaceTransform, const IntRect& scissorRect, const Region& occlusion)
{
    if (!contentSpaceTransform.isInvertible())
        return contentRect;

    FloatRect transformedRect = contentSpaceTransform.mapRect(FloatRect(contentRect));
    // Take the enclosingIntRect at each step, as we want to contain any unoccluded partial pixels in the resulting IntRect.
    IntRect shrunkRect = rectSubtractRegion(intersection(enclosingIntRect(transformedRect), scissorRect), occlusion);
    bool clamped; // FIXME: projectQuad returns invalid results when a point gets clamped. To be fixed in bug https://bugs.webkit.org/show_bug.cgi?id=80806.
    IntRect unoccludedRect = enclosingIntRect(projectQuad(contentSpaceTransform.inverse(), FloatQuad(FloatRect(shrunkRect)), clamped).boundingBox());
    if (clamped)
        return contentRect;
    // The rect back in content space is a bounding box and may extend outside of the original contentRect, so clamp it to the contentRectBounds.
    return intersection(unoccludedRect, contentRect);
}

template<typename LayerType, typename RenderSurfaceType>
IntRect CCOcclusionTrackerBase<LayerType, RenderSurfaceType>::unoccludedContentRect(const LayerType* layer, const IntRect& contentRect) const
{
    ASSERT(!m_stack.isEmpty());
    if (m_stack.isEmpty())
        return contentRect;
    if (contentRect.isEmpty())
        return contentRect;

    ASSERT(layer->targetRenderSurface() == m_stack.last().surface);

    // We want to return a rect that contains all the visible parts of |contentRect| in both screen space and in the target surface.
    // So we find the visible parts of |contentRect| in each space, and take the intersection.

    IntRect unoccludedInScreen = contentRect;
    if (layerTransformsToScreenKnown(layer))
        unoccludedInScreen = computeUnoccludedContentRect(contentRect, contentToScreenSpaceTransform<LayerType>(layer), m_scissorRectInScreenSpace, m_stack.last().occlusionInScreen);

    if (unoccludedInScreen.isEmpty())
        return unoccludedInScreen;

    IntRect unoccludedInTarget = contentRect;
    if (layerTransformsToTargetKnown(layer))
        unoccludedInTarget = computeUnoccludedContentRect(contentRect, contentToTargetSurfaceTransform<LayerType>(layer), layerScissorRectInTargetSurface(layer), m_stack.last().occlusionInTarget);

    return intersection(unoccludedInScreen, unoccludedInTarget);
}

template<typename LayerType, typename RenderSurfaceType>
IntRect CCOcclusionTrackerBase<LayerType, RenderSurfaceType>::layerScissorRectInTargetSurface(const LayerType* layer) const
{
    const RenderSurfaceType* targetSurface = m_stack.last().surface;
    FloatRect totalScissor = targetSurface->contentRect();
    if (layer->usesLayerClipping())
        totalScissor.intersect(layer->clipRect());
    return enclosingIntRect(totalScissor);
}

// Declare the possible functions here for the linker.
template CCOcclusionTrackerBase<LayerChromium, RenderSurfaceChromium>::CCOcclusionTrackerBase(IntRect scissorRectInScreenSpace, bool recordMetricsForFrame);
template void CCOcclusionTrackerBase<LayerChromium, RenderSurfaceChromium>::enterTargetRenderSurface(const RenderSurfaceChromium* newTarget);
template void CCOcclusionTrackerBase<LayerChromium, RenderSurfaceChromium>::finishedTargetRenderSurface(const LayerChromium* owningLayer, const RenderSurfaceChromium* finishedTarget);
template void CCOcclusionTrackerBase<LayerChromium, RenderSurfaceChromium>::leaveToTargetRenderSurface(const RenderSurfaceChromium* newTarget);
template void CCOcclusionTrackerBase<LayerChromium, RenderSurfaceChromium>::markOccludedBehindLayer(const LayerChromium*);
template bool CCOcclusionTrackerBase<LayerChromium, RenderSurfaceChromium>::occluded(const LayerChromium*, const IntRect& contentRect) const;
template IntRect CCOcclusionTrackerBase<LayerChromium, RenderSurfaceChromium>::unoccludedContentRect(const LayerChromium*, const IntRect& contentRect) const;
template IntRect CCOcclusionTrackerBase<LayerChromium, RenderSurfaceChromium>::layerScissorRectInTargetSurface(const LayerChromium*) const;

template CCOcclusionTrackerBase<CCLayerImpl, CCRenderSurface>::CCOcclusionTrackerBase(IntRect scissorRectInScreenSpace, bool recordMetricsForFrame);
template void CCOcclusionTrackerBase<CCLayerImpl, CCRenderSurface>::enterTargetRenderSurface(const CCRenderSurface* newTarget);
template void CCOcclusionTrackerBase<CCLayerImpl, CCRenderSurface>::finishedTargetRenderSurface(const CCLayerImpl* owningLayer, const CCRenderSurface* finishedTarget);
template void CCOcclusionTrackerBase<CCLayerImpl, CCRenderSurface>::leaveToTargetRenderSurface(const CCRenderSurface* newTarget);
template void CCOcclusionTrackerBase<CCLayerImpl, CCRenderSurface>::markOccludedBehindLayer(const CCLayerImpl*);
template bool CCOcclusionTrackerBase<CCLayerImpl, CCRenderSurface>::occluded(const CCLayerImpl*, const IntRect& contentRect) const;
template IntRect CCOcclusionTrackerBase<CCLayerImpl, CCRenderSurface>::unoccludedContentRect(const CCLayerImpl*, const IntRect& contentRect) const;
template IntRect CCOcclusionTrackerBase<CCLayerImpl, CCRenderSurface>::layerScissorRectInTargetSurface(const CCLayerImpl*) const;


} // namespace WebCore
#endif // USE(ACCELERATED_COMPOSITING)
