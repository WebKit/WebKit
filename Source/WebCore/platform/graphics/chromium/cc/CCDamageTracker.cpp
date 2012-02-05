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

#include "cc/CCLayerImpl.h"
#include "cc/CCLayerTreeHostCommon.h"
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

void CCDamageTracker::updateDamageRectForNextFrame(const Vector<RefPtr<CCLayerImpl> >& layerList, int targetSurfaceLayerID, CCLayerImpl* targetSurfaceMaskLayer)
{
    //
    // This function computes the "damage rect" of a target surface. The damage
    // rect is the region of the surface that may have changed and needs to be redrawn.
    // This can be used to scissor what is actually drawn, to save GPU computation and
    // bandwidth.
    //
    // The surface's damage rect is computed as the union of all possible changes that
    // have happened to the surface since the last frame was drawn. This includes:
    //   - any changes for existing layers/surfaces that contribute to the target surface
    //   - layers/surfaces that existed in the previous frame, but no longer exist.
    //
    // The basic algorithm for computing the damage region is as follows:
    //
    //       // 1. compute damage caused by changes in active/new layers
    //       for each layer in the layerList:
    //           if the layer is actually a renderSurface:
    //               add the surface's damage to our target surface.
    //           else
    //               add the layer's damage to the target surface.
    //
    //       // 2. compute damage caused by the target surface's mask, if it exists.
    //
    //       // 3. compute damage caused by old layers/surfaces that no longer exist
    //       for each leftover layer:
    //           add the old layer/surface bounds to the target surface damage.
    //
    //       // 4. combine all partial damage rects to get the full damage rect.
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

    // If the target surface already knows its entire region is damaged, we can return early.
    // FIXME: this should go away, or will be cleaner, after refactoring into RenderPass/RenderSchedule.
    CCLayerImpl* layer = layerList[0].get();
    if (layer->targetRenderSurface()->surfacePropertyChangedOnlyFromDescendant()) {
        m_currentDamageRect = FloatRect(layer->targetRenderSurface()->contentRect());
        // FIXME: this early exit is incorrect: https://bugs.webkit.org/show_bug.cgi?id=76924
        return;
    }

    FloatRect damageFromActiveLayers = computeDamageFromActiveLayers(layerList, targetSurfaceLayerID);
    FloatRect damageFromSurfaceMask = computeDamageFromSurfaceMask(targetSurfaceMaskLayer);
    FloatRect damageFromLeftoverRects = computeDamageFromLeftoverRects();

    if (m_forceFullDamageNextUpdate) {
        m_currentDamageRect = FloatRect(layer->targetRenderSurface()->contentRect());
        m_forceFullDamageNextUpdate = false;
    } else {
        m_currentDamageRect = damageFromActiveLayers;
        m_currentDamageRect.uniteIfNonZero(damageFromSurfaceMask);
        m_currentDamageRect.uniteIfNonZero(damageFromLeftoverRects);
    }

    // The next history map becomes the current map for the next frame.
    swap(m_currentRectHistory, m_nextRectHistory);
}

FloatRect CCDamageTracker::removeRectFromCurrentFrame(int layerID)
{
    // This function exists mainly for readability of the code.
    // take() will remove the entry from the map, or if not found, return a default (empty) rect.
    return m_currentRectHistory->take(layerID);
}

void CCDamageTracker::saveRectForNextFrame(int layerID, const FloatRect& targetSpaceRect)
{
    // This layer should not yet exist in next frame's history.
    ASSERT(m_nextRectHistory->find(layerID) == m_nextRectHistory->end());
    m_nextRectHistory->set(layerID, targetSpaceRect);
}

FloatRect CCDamageTracker::computeDamageFromActiveLayers(const Vector<RefPtr<CCLayerImpl> >& layerList, int targetSurfaceLayerID)
{
    FloatRect damageRect = FloatRect();

    for (unsigned layerIndex = 0; layerIndex < layerList.size(); ++layerIndex) {
        CCLayerImpl* layer = layerList[layerIndex].get();

        if (CCLayerTreeHostCommon::renderSurfaceContributesToTarget<CCLayerImpl>(layer, targetSurfaceLayerID))
            extendDamageForRenderSurface(layer, damageRect);
        else
            extendDamageForLayer(layer, damageRect);
    }

    return damageRect;
}

FloatRect CCDamageTracker::computeDamageFromSurfaceMask(CCLayerImpl* targetSurfaceMaskLayer)
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

FloatRect CCDamageTracker::computeDamageFromLeftoverRects()
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

    FloatRect oldLayerRect = removeRectFromCurrentFrame(layer->id());

    FloatRect layerRectInTargetSpace = originTransform.mapRect(FloatRect(FloatPoint::zero(), layer->bounds()));
    saveRectForNextFrame(layer->id(), layerRectInTargetSpace);

    if (layer->layerPropertyChanged()) {
        // If a layer has changed, then its entire layer rect affects the target surface.
        targetDamageRect.uniteIfNonZero(layerRectInTargetSpace);

        // The layer's old region is now exposed on the target surface, too.
        // Note oldLayerRect is already in target space.
        targetDamageRect.uniteIfNonZero(oldLayerRect);
    } else if (!layer->updateRect().isEmpty()) {
        // If the layer properties havent changed, then the the target surface is only
        // affected by the layer's update area, which could be empty.
        FloatRect updateRectInTargetSpace = originTransform.mapRect(layer->updateRect());
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

    FloatRect oldSurfaceRect = removeRectFromCurrentFrame(layer->id());

    FloatRect surfaceRectInTargetSpace = renderSurface->drawableContentRect(); // already includes replica if it exists.
    saveRectForNextFrame(layer->id(), surfaceRectInTargetSpace);

    FloatRect damageRectInLocalSpace;
    if (renderSurface->surfacePropertyChanged()) {
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
        FloatRect damageRectInTargetSpace = originTransform.mapRect(damageRectInLocalSpace);
        targetDamageRect.uniteIfNonZero(damageRectInTargetSpace);

        if (layer->replicaLayer()) {
            // Compute the replica's "originTransform" that maps from the replica's origin space to the target surface origin space.
            TransformationMatrix replicaOriginTransform = layer->renderSurface()->originTransform();
            replicaOriginTransform.translate(layer->replicaLayer()->position().x(), layer->replicaLayer()->position().y());
            replicaOriginTransform.multiply(layer->replicaLayer()->transform());
            replicaOriginTransform.translate(-layer->replicaLayer()->position().x(), -layer->replicaLayer()->position().y());

            targetDamageRect.uniteIfNonZero(replicaOriginTransform.mapRect(damageRectInLocalSpace));
        }
    }

    // If there was damage on the replica's mask, then the target surface receives that damage as well.
    if (layer->replicaLayer() && layer->replicaLayer()->maskLayer()) {
        CCLayerImpl* replicaMaskLayer = layer->replicaLayer()->maskLayer();

        removeRectFromCurrentFrame(replicaMaskLayer->id());

        // Compute the replica's "originTransform" that maps from the replica's origin space to the target surface origin space.
        TransformationMatrix replicaOriginTransform = layer->renderSurface()->originTransform();
        replicaOriginTransform.translate(layer->replicaLayer()->position().x(), layer->replicaLayer()->position().y());
        replicaOriginTransform.multiply(layer->replicaLayer()->transform());
        replicaOriginTransform.translate(-layer->replicaLayer()->position().x(), -layer->replicaLayer()->position().y());

        FloatRect replicaMaskLayerRect = replicaOriginTransform.mapRect(FloatRect(FloatPoint::zero(), FloatSize(replicaMaskLayer->bounds().width(), replicaMaskLayer->bounds().height())));
        saveRectForNextFrame(replicaMaskLayer->id(), replicaMaskLayerRect);

        // In the current implementation, a change in the replica mask damages the entire replica region.
        if (replicaMaskLayer->layerPropertyChanged() || !replicaMaskLayer->updateRect().isEmpty())
            targetDamageRect.uniteIfNonZero(replicaMaskLayerRect);
    }
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
