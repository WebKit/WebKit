/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "cc/CCDamageTracker.h"

#include "FilterOperations.h"
#include "cc/CCLayerImpl.h"
#include "cc/CCLayerTreeHostCommon.h"
#include "cc/CCMathUtil.h"
#include "cc/CCRenderSurface.h"

namespace WebCore {

PassOwnPtr<CCDamageTracker> CCDamageTracker::create()
{
    return adoptPtr(new CCDamageTracker());
}

CCDamageTracker::CCDamageTracker()
    : m_forceFullDamageNextUpdate(false)
{
    m_currentRectHistory = adoptPtr(new RectMap);
    m_nextRectHistory = adoptPtr(new RectMap);
}

CCDamageTracker::~CCDamageTracker()
{
}

static inline void expandDamageRectWithFilters(FloatRect& damageRect, const FilterOperations& filters)
{
    int top, right, bottom, left;
    filters.getOutsets(top, right, bottom, left);
    damageRect.move(-left, -top);
    damageRect.expand(left + right, top + bottom);
}

static inline void expandDamageRectInsideRectWithFilters(FloatRect& damageRect, const FloatRect& filterRect, const FilterOperations& filters)
{
    FloatRect expandedDamageRect = damageRect;
    expandDamageRectWithFilters(expandedDamageRect, filters);
    expandedDamageRect.intersect(filterRect);

    damageRect.unite(expandedDamageRect);
}

void CCDamageTracker::updateDamageTrackingState(const Vector<CCLayerImpl*>& layerList, int targetSurfaceLayerID, bool targetSurfacePropertyChangedOnlyFromDescendant, const IntRect& targetSurfaceContentRect, CCLayerImpl* targetSurfaceMaskLayer, const FilterOperations& filters)
{
    //
    // This function computes the "damage rect" of a target surface, and updates the state
    // that is used to correctly track damage across frames. The damage rect is the region
    // of the surface that may have changed and needs to be redrawn. This can be used to
    // scissor what is actually drawn, to save GPU computation and bandwidth.
    //
    // The surface's damage rect is computed as the union of all possible changes that
    // have happened to the surface since the last frame was drawn. This includes:
    //   - any changes for existing layers/surfaces that contribute to the target surface
    //   - layers/surfaces that existed in the previous frame, but no longer exist.
    //
    // The basic algorithm for computing the damage region is as follows:
    //
    //   1. compute damage caused by changes in active/new layers
    //       for each layer in the layerList:
    //           if the layer is actually a renderSurface:
    //               add the surface's damage to our target surface.
    //           else
    //               add the layer's damage to the target surface.
    //
    //   2. compute damage caused by the target surface's mask, if it exists.
    //
    //   3. compute damage caused by old layers/surfaces that no longer exist
    //       for each leftover layer:
    //           add the old layer/surface bounds to the target surface damage.
    //
    //   4. combine all partial damage rects to get the full damage rect.
    //
    // Additional important points:
    //
    // - This algorithm is implicitly recursive; it assumes that descendant surfaces have
    //   already computed their damage.
    //
    // - Changes to layers/surfaces indicate "damage" to the target surface; If a layer is
    //   not changed, it does NOT mean that the layer can skip drawing. All layers that
    //   overlap the damaged region still need to be drawn. For example, if a layer
    //   changed its opacity, then layers underneath must be re-drawn as well, even if
    //   they did not change.
    //
    // - If a layer/surface property changed, the old bounds and new bounds may overlap...
    //   i.e. some of the exposed region may not actually be exposing anything. But this
    //   does not artificially inflate the damage rect. If the layer changed, its entire
    //   old bounds would always need to be redrawn, regardless of how much it overlaps
    //   with the layer's new bounds, which also need to be entirely redrawn.
    //
    // - See comments in the rest of the code to see what exactly is considered a "change"
    //   in a layer/surface.
    //
    // - To correctly manage exposed rects, two RectMaps are maintained:
    //
    //      1. The "current" map contains all the layer bounds that contributed to the
    //         previous frame (even outside the previous damaged area). If a layer changes
    //         or does not exist anymore, those regions are then exposed and damage the
    //         target surface. As the algorithm progresses, entries are removed from the
    //         map until it has only leftover layers that no longer exist.
    //
    //      2. The "next" map starts out empty, and as the algorithm progresses, every
    //         layer/surface that contributes to the surface is added to the map.
    //
    //      3. After the damage rect is computed, the two maps are swapped, so that the
    //         damage tracker is ready for the next frame.
    //

    // These functions cannot be bypassed with early-exits, even if we know what the
    // damage will be for this frame, because we need to update the damage tracker state
    // to correctly track the next frame.
    FloatRect damageFromActiveLayers = trackDamageFromActiveLayers(layerList, targetSurfaceLayerID);
    FloatRect damageFromSurfaceMask = trackDamageFromSurfaceMask(targetSurfaceMaskLayer);
    FloatRect damageFromLeftoverRects = trackDamageFromLeftoverRects();

    if (m_forceFullDamageNextUpdate || targetSurfacePropertyChangedOnlyFromDescendant) {
        m_currentDamageRect = targetSurfaceContentRect;
        m_forceFullDamageNextUpdate = false;
    } else {
        // FIXME: can we clamp this damage to the surface's content rect? (affects performance, but not correctness)
        m_currentDamageRect = damageFromActiveLayers;
        m_currentDamageRect.uniteIfNonZero(damageFromSurfaceMask);
        m_currentDamageRect.uniteIfNonZero(damageFromLeftoverRects);

        if (filters.hasFilterThatMovesPixels())
            expandDamageRectWithFilters(m_currentDamageRect, filters);
    }

    // The next history map becomes the current map for the next frame.
    swap(m_currentRectHistory, m_nextRectHistory);
}

FloatRect CCDamageTracker::removeRectFromCurrentFrame(int layerID, bool& layerIsNew)
{
    layerIsNew = !m_currentRectHistory->contains(layerID);

    // take() will remove the entry from the map, or if not found, return a default (empty) rect.
    return m_currentRectHistory->take(layerID);
}

void CCDamageTracker::saveRectForNextFrame(int layerID, const FloatRect& targetSpaceRect)
{
    // This layer should not yet exist in next frame's history.
    ASSERT(m_nextRectHistory->find(layerID) == m_nextRectHistory->end());
    m_nextRectHistory->set(layerID, targetSpaceRect);
}

FloatRect CCDamageTracker::trackDamageFromActiveLayers(const Vector<CCLayerImpl*>& layerList, int targetSurfaceLayerID)
{
    FloatRect damageRect = FloatRect();

    for (unsigned layerIndex = 0; layerIndex < layerList.size(); ++layerIndex) {
        // Visit layers in back-to-front order.
        CCLayerImpl* layer = layerList[layerIndex];

        if (CCLayerTreeHostCommon::renderSurfaceContributesToTarget<CCLayerImpl>(layer, targetSurfaceLayerID))
            extendDamageForRenderSurface(layer, damageRect);
        else
            extendDamageForLayer(layer, damageRect);
    }

    return damageRect;
}

FloatRect CCDamageTracker::trackDamageFromSurfaceMask(CCLayerImpl* targetSurfaceMaskLayer)
{
    FloatRect damageRect = FloatRect();

    if (!targetSurfaceMaskLayer)
        return damageRect;

    // Currently, if there is any change to the mask, we choose to damage the entire
    // surface. This could potentially be optimized later, but it is not expected to be a
    // common case.
    if (targetSurfaceMaskLayer->layerPropertyChanged() || !targetSurfaceMaskLayer->updateRect().isEmpty())
        damageRect = FloatRect(FloatPoint::zero(), FloatSize(targetSurfaceMaskLayer->bounds()));

    return damageRect;
}

FloatRect CCDamageTracker::trackDamageFromLeftoverRects()
{
    // After computing damage for all active layers, any leftover items in the current
    // rect history correspond to layers/surfaces that no longer exist. So, these regions
    // are now exposed on the target surface.

    FloatRect damageRect = FloatRect();

    for (RectMap::iterator it = m_currentRectHistory->begin(); it != m_currentRectHistory->end(); ++it)
        damageRect.unite(it->second);

    m_currentRectHistory->clear();

    return damageRect;
}

void CCDamageTracker::extendDamageForLayer(CCLayerImpl* layer, FloatRect& targetDamageRect)
{
    // There are two ways that a layer can damage a region of the target surface:
    //   1. Property change (e.g. opacity, position, transforms):
    //        - the entire region of the layer itself damages the surface.
    //        - the old layer region also damages the surface, because this region is now exposed.
    //        - note that in many cases the old and new layer rects may overlap, which is fine.
    //
    //   2. Repaint/update: If a region of the layer that was repainted/updated, that
    //      region damages the surface.
    //
    // Property changes take priority over update rects.

    // Compute the layer's "originTransform" by translating the drawTransform.
    TransformationMatrix originTransform = layer->drawTransform();
    originTransform.translate(-0.5 * layer->bounds().width(), -0.5 * layer->bounds().height());

    bool layerIsNew = false;
    FloatRect oldLayerRect = removeRectFromCurrentFrame(layer->id(), layerIsNew);

    FloatRect layerRectInTargetSpace = CCMathUtil::mapClippedRect(originTransform, FloatRect(FloatPoint::zero(), layer->bounds()));
    saveRectForNextFrame(layer->id(), layerRectInTargetSpace);

    if (layerIsNew || layer->layerPropertyChanged()) {
        // If a layer is new or has changed, then its entire layer rect affects the target surface.
        targetDamageRect.uniteIfNonZero(layerRectInTargetSpace);

        // The layer's old region is now exposed on the target surface, too.
        // Note oldLayerRect is already in target space.
        targetDamageRect.uniteIfNonZero(oldLayerRect);
    } else if (!layer->updateRect().isEmpty()) {
        // If the layer properties havent changed, then the the target surface is only
        // affected by the layer's update area, which could be empty.
        FloatRect updateRectInTargetSpace = CCMathUtil::mapClippedRect(originTransform, layer->updateRect());
        targetDamageRect.uniteIfNonZero(updateRectInTargetSpace);
    }
}

void CCDamageTracker::extendDamageForRenderSurface(CCLayerImpl* layer, FloatRect& targetDamageRect)
{
    // There are two ways a "descendant surface" can damage regions of the "target surface":
    //   1. Property change:
    //        - a surface's geometry can change because of
    //            - changes to descendants (i.e. the subtree) that affect the surface's content rect
    //            - changes to ancestor layers that propagate their property changes to their entire subtree.
    //        - just like layers, both the old surface rect and new surface rect will
    //          damage the target surface in this case.
    //
    //   2. Damage rect: This surface may have been damaged by its own layerList as well, and that damage
    //      should propagate to the target surface.
    //

    CCRenderSurface* renderSurface = layer->renderSurface();

    bool surfaceIsNew = false;
    FloatRect oldSurfaceRect = removeRectFromCurrentFrame(layer->id(), surfaceIsNew);

    FloatRect surfaceRectInTargetSpace = renderSurface->drawableContentRect(); // already includes replica if it exists.
    saveRectForNextFrame(layer->id(), surfaceRectInTargetSpace);

    // If the layer has a background filter, this may cause pixels in our surface to be expanded, so we will need to expand any damage
    // that exists below this layer by that amount.
    if (layer->backgroundFilters().hasFilterThatMovesPixels())
        expandDamageRectInsideRectWithFilters(targetDamageRect, surfaceRectInTargetSpace, layer->backgroundFilters());

    FloatRect damageRectInLocalSpace;
    if (surfaceIsNew || renderSurface->surfacePropertyChanged()) {
        // The entire surface contributes damage.
        damageRectInLocalSpace = renderSurface->contentRect();

        // The surface's old region is now exposed on the target surface, too.
        targetDamageRect.uniteIfNonZero(oldSurfaceRect);
    } else {
        // Only the surface's damageRect will damage the target surface.
        damageRectInLocalSpace = renderSurface->damageTracker()->currentDamageRect();
    }

    // If there was damage, transform it to target space, and possibly contribute its reflection if needed.
    if (!damageRectInLocalSpace.isEmpty()) {
        const TransformationMatrix& originTransform = renderSurface->originTransform();
        FloatRect damageRectInTargetSpace = CCMathUtil::mapClippedRect(originTransform, damageRectInLocalSpace);
        targetDamageRect.uniteIfNonZero(damageRectInTargetSpace);

        if (layer->replicaLayer()) {
            const TransformationMatrix& replicaOriginTransform = renderSurface->replicaOriginTransform();
            targetDamageRect.uniteIfNonZero(CCMathUtil::mapClippedRect(replicaOriginTransform, damageRectInLocalSpace));
        }
    }

    // If there was damage on the replica's mask, then the target surface receives that damage as well.
    if (layer->replicaLayer() && layer->replicaLayer()->maskLayer()) {
        CCLayerImpl* replicaMaskLayer = layer->replicaLayer()->maskLayer();

        bool replicaIsNew = false;
        removeRectFromCurrentFrame(replicaMaskLayer->id(), replicaIsNew);

        // Compute the replica's "originTransform" that maps from the replica's origin space to the target surface origin space.
        const TransformationMatrix& replicaOriginTransform = renderSurface->replicaOriginTransform();
        FloatRect replicaMaskLayerRect = CCMathUtil::mapClippedRect(replicaOriginTransform, FloatRect(FloatPoint::zero(), FloatSize(replicaMaskLayer->bounds().width(), replicaMaskLayer->bounds().height())));
        saveRectForNextFrame(replicaMaskLayer->id(), replicaMaskLayerRect);

        // In the current implementation, a change in the replica mask damages the entire replica region.
        if (replicaIsNew || replicaMaskLayer->layerPropertyChanged() || !replicaMaskLayer->updateRect().isEmpty())
            targetDamageRect.uniteIfNonZero(replicaMaskLayerRect);
    }
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
