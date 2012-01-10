/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "cc/CCLayerIterator.h"

#include "LayerChromium.h"
#include "RenderSurfaceChromium.h"
#include "cc/CCLayerImpl.h"
#include "cc/CCRenderSurface.h"

namespace WebCore {

template <typename LayerType, typename RenderSurfaceType>
void CCLayerIteratorActions::BackToFront::begin(CCLayerIteratorPosition<LayerType, RenderSurfaceType>& pos)
{
    pos.targetRenderSurfaceLayerIndex = 0;
    pos.currentLayerIndex = CCLayerIteratorPositionValue::LayerIndexRepresentingTargetRenderSurface;

    m_highestTargetRenderSurfaceLayer = 0;
}

template <typename LayerType, typename RenderSurfaceType>
void CCLayerIteratorActions::BackToFront::end(CCLayerIteratorPosition<LayerType, RenderSurfaceType>& pos)
{
    pos.targetRenderSurfaceLayerIndex = CCLayerIteratorPositionValue::InvalidTargetRenderSurfaceLayerIndex;
    pos.currentLayerIndex = 0;
}

template <typename LayerType, typename RenderSurfaceType>
void CCLayerIteratorActions::BackToFront::next(CCLayerIteratorPosition<LayerType, RenderSurfaceType>& pos)
{
    // If the current layer has a RS, move to its layer list. Otherwise, visit the next layer in the current RS layer list.
    if (pos.currentLayerRepresentsContributingRenderSurface()) {
        // Save our position in the childLayer list for the RenderSurface, then jump to the next RenderSurface. Save where we
        // came from in the next RenderSurface so we can get back to it.
        pos.targetRenderSurface()->m_currentLayerIndexHistory = pos.currentLayerIndex;
        int previousTargetRenderSurfaceLayer = pos.targetRenderSurfaceLayerIndex;

        pos.targetRenderSurfaceLayerIndex = ++m_highestTargetRenderSurfaceLayer;
        pos.currentLayerIndex = CCLayerIteratorPositionValue::LayerIndexRepresentingTargetRenderSurface;

        pos.targetRenderSurface()->m_targetRenderSurfaceLayerIndexHistory = previousTargetRenderSurfaceLayer;
    } else {
        ++pos.currentLayerIndex;

        int targetRenderSurfaceNumChildren = pos.targetRenderSurfaceChildren().size();
        while (pos.currentLayerIndex == targetRenderSurfaceNumChildren) {
            // Jump back to the previous RenderSurface, and get back the position where we were in that list, and move to the next position there.
            if (!pos.targetRenderSurfaceLayerIndex) {
                // End of the list
                pos.targetRenderSurfaceLayerIndex = CCLayerIteratorPositionValue::InvalidTargetRenderSurfaceLayerIndex;
                pos.currentLayerIndex = 0;
                return;
            }
            pos.targetRenderSurfaceLayerIndex = pos.targetRenderSurface()->m_targetRenderSurfaceLayerIndexHistory;
            pos.currentLayerIndex = pos.targetRenderSurface()->m_currentLayerIndexHistory + 1;

            targetRenderSurfaceNumChildren = pos.targetRenderSurfaceChildren().size();
        }
    }
}

template <typename LayerType, typename RenderSurfaceType>
void CCLayerIteratorActions::FrontToBack::begin(CCLayerIteratorPosition<LayerType, RenderSurfaceType>& pos)
{
    pos.targetRenderSurfaceLayerIndex = 0;
    pos.currentLayerIndex = pos.targetRenderSurfaceChildren().size() - 1;
    goToHighestInSubtree(pos);
}

template <typename LayerType, typename RenderSurfaceType>
void CCLayerIteratorActions::FrontToBack::end(CCLayerIteratorPosition<LayerType, RenderSurfaceType>& pos)
{
    pos.targetRenderSurfaceLayerIndex = CCLayerIteratorPositionValue::InvalidTargetRenderSurfaceLayerIndex;
    pos.currentLayerIndex = 0;
}

template <typename LayerType, typename RenderSurfaceType>
void CCLayerIteratorActions::FrontToBack::next(CCLayerIteratorPosition<LayerType, RenderSurfaceType>& pos)
{
    // Moves to the previous layer in the current RS layer list. Then we check if the
    // new current layer has its own RS, in which case there are things in that RS layer list that are higher, so
    // we find the highest layer in that subtree.
    // If we move back past the front of the list, we jump up to the previous RS layer list, picking up again where we
    // had previously recursed into the current RS layer list.

    if (!pos.currentLayerRepresentsTargetRenderSurface()) {
        // Subtracting one here will eventually cause the current layer to become that layer
        // representing the target render surface.
        --pos.currentLayerIndex;
        goToHighestInSubtree(pos);
    } else {
        while (pos.currentLayerRepresentsTargetRenderSurface()) {
            if (!pos.targetRenderSurfaceLayerIndex) {
                // End of the list
                pos.targetRenderSurfaceLayerIndex = CCLayerIteratorPositionValue::InvalidTargetRenderSurfaceLayerIndex;
                pos.currentLayerIndex = 0;
                return;
            }
            pos.targetRenderSurfaceLayerIndex = pos.targetRenderSurface()->m_targetRenderSurfaceLayerIndexHistory;
            pos.currentLayerIndex = pos.targetRenderSurface()->m_currentLayerIndexHistory;
        }
    }
}

template <typename LayerType, typename RenderSurfaceType>
void CCLayerIteratorActions::FrontToBack::goToHighestInSubtree(CCLayerIteratorPosition<LayerType, RenderSurfaceType>& pos)
{
    if (pos.currentLayerRepresentsTargetRenderSurface())
        return;
    while (pos.currentLayerRepresentsContributingRenderSurface()) {
        // Save where we were in the current target surface, move to the next one, and save the target surface that we
        // came from there so we can go back to it.
        pos.targetRenderSurface()->m_currentLayerIndexHistory = pos.currentLayerIndex;
        int previousTargetRenderSurfaceLayer = pos.targetRenderSurfaceLayerIndex;

        for (LayerType* layer = pos.currentLayer(); pos.targetRenderSurfaceLayer() != layer; ++pos.targetRenderSurfaceLayerIndex) { }
        pos.currentLayerIndex = pos.targetRenderSurfaceChildren().size() - 1;

        pos.targetRenderSurface()->m_targetRenderSurfaceLayerIndexHistory = previousTargetRenderSurfaceLayer;
    }
}

// Declare each of the above functions for LayerChromium and CCLayerImpl classes so that they are linked.
template void CCLayerIteratorActions::BackToFront::begin(CCLayerIteratorPosition<LayerChromium, RenderSurfaceChromium>&);
template void CCLayerIteratorActions::BackToFront::end(CCLayerIteratorPosition<LayerChromium, RenderSurfaceChromium>&);
template void CCLayerIteratorActions::BackToFront::next(CCLayerIteratorPosition<LayerChromium, RenderSurfaceChromium>&);

template void CCLayerIteratorActions::BackToFront::begin(CCLayerIteratorPosition<CCLayerImpl, CCRenderSurface>&);
template void CCLayerIteratorActions::BackToFront::end(CCLayerIteratorPosition<CCLayerImpl, CCRenderSurface>&);
template void CCLayerIteratorActions::BackToFront::next(CCLayerIteratorPosition<CCLayerImpl, CCRenderSurface>&);

template void CCLayerIteratorActions::FrontToBack::next(CCLayerIteratorPosition<LayerChromium, RenderSurfaceChromium>&);
template void CCLayerIteratorActions::FrontToBack::end(CCLayerIteratorPosition<LayerChromium, RenderSurfaceChromium>&);
template void CCLayerIteratorActions::FrontToBack::begin(CCLayerIteratorPosition<LayerChromium, RenderSurfaceChromium>&);
template void CCLayerIteratorActions::FrontToBack::goToHighestInSubtree(CCLayerIteratorPosition<LayerChromium, RenderSurfaceChromium>&);

template void CCLayerIteratorActions::FrontToBack::next(CCLayerIteratorPosition<CCLayerImpl, CCRenderSurface>&);
template void CCLayerIteratorActions::FrontToBack::end(CCLayerIteratorPosition<CCLayerImpl, CCRenderSurface>&);
template void CCLayerIteratorActions::FrontToBack::begin(CCLayerIteratorPosition<CCLayerImpl, CCRenderSurface>&);
template void CCLayerIteratorActions::FrontToBack::goToHighestInSubtree(CCLayerIteratorPosition<CCLayerImpl, CCRenderSurface>&);

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
