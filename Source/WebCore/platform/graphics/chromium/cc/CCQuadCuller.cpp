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

#include "cc/CCQuadCuller.h"

#include "Region.h"
#include "TransformationMatrix.h"
#include "cc/CCLayerImpl.h"
#include "cc/CCOverdrawMetrics.h"
#include "cc/CCRenderPass.h"
#include "cc/CCRenderSurfaceDrawQuad.h"

using namespace std;

namespace WebCore {

CCQuadCuller::CCQuadCuller(CCQuadList& quadList, CCLayerImpl* layer, const CCOcclusionTrackerImpl* occlusionTracker)
    : m_quadList(quadList)
    , m_layer(layer)
    , m_occlusionTracker(occlusionTracker)
{
}

static inline bool appendQuadInternal(PassOwnPtr<CCDrawQuad> passDrawQuad, const IntRect& culledRect, CCQuadList& quadList, const CCOcclusionTrackerImpl& occlusionTracker)
{
    OwnPtr<CCDrawQuad> drawQuad(passDrawQuad);
    bool keepQuad = !culledRect.isEmpty();
    if (keepQuad)
        drawQuad->setQuadVisibleRect(culledRect);

    occlusionTracker.overdrawMetrics().didCullForDrawing(drawQuad->quadTransform(), drawQuad->quadRect(), culledRect);
    occlusionTracker.overdrawMetrics().didDraw(drawQuad->quadTransform(), culledRect, drawQuad->opaqueRect());

    // Release the quad after we're done using it.
    if (keepQuad)
        quadList.append(drawQuad.release());
    return keepQuad;
}

bool CCQuadCuller::append(PassOwnPtr<CCDrawQuad> passDrawQuad)
{
    IntRect culledRect = m_occlusionTracker->unoccludedContentRect(m_layer, passDrawQuad->quadRect());
    return appendQuadInternal(passDrawQuad, culledRect, m_quadList, *m_occlusionTracker);
}

bool CCQuadCuller::appendSurface(PassOwnPtr<CCDrawQuad> passDrawQuad)
{
    IntRect culledRect = m_occlusionTracker->unoccludedContributingSurfaceContentRect(m_layer->renderSurface(), false, passDrawQuad->quadRect());
    return appendQuadInternal(passDrawQuad, culledRect, m_quadList, *m_occlusionTracker);
}

bool CCQuadCuller::appendReplica(PassOwnPtr<CCDrawQuad> passDrawQuad)
{
    IntRect culledRect = m_occlusionTracker->unoccludedContributingSurfaceContentRect(m_layer->renderSurface(), true, passDrawQuad->quadRect());
    return appendQuadInternal(passDrawQuad, culledRect, m_quadList, *m_occlusionTracker);
}

} // namespace WebCore
#endif // USE(ACCELERATED_COMPOSITING)
