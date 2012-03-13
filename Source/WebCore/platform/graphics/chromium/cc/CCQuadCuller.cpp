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
#include "cc/CCRenderPass.h"
#include "cc/CCRenderSurfaceDrawQuad.h"

using namespace std;

namespace WebCore {

CCQuadCuller::CCQuadCuller(CCQuadList& quadList, CCLayerImpl* layer, CCOcclusionTrackerImpl* occlusionTracker, CCOverdrawCounts* overdrawCounts)
    : m_quadList(quadList)
    , m_layer(layer)
    , m_occlusionTracker(occlusionTracker)
    , m_overdrawCounts(overdrawCounts)
{
}

static float wedgeProduct(const FloatPoint& p1, const FloatPoint& p2)
{
    return p1.x() * p2.y() - p1.y() * p2.x();
}

// Computes area of quads that are possibly non-rectangular. Can
// be easily extended to polygons.
static float quadArea(const FloatQuad& quad)
{
    return fabs(0.5 * (wedgeProduct(quad.p1(), quad.p2()) +
                   wedgeProduct(quad.p2(), quad.p3()) +
                   wedgeProduct(quad.p3(), quad.p4()) +
                   wedgeProduct(quad.p4(), quad.p1())));
}

void CCQuadCuller::append(PassOwnPtr<CCDrawQuad> passDrawQuad)
{
    OwnPtr<CCDrawQuad> drawQuad(passDrawQuad);
    IntRect culledRect = m_occlusionTracker->unoccludedContentRect(m_layer, drawQuad->quadRect());
    bool keepQuad = !culledRect.isEmpty();
    if (keepQuad)
        drawQuad->setQuadVisibleRect(culledRect);

    // FIXME: Make a templated metrics class and move the logic out to there.
    // Temporary code anyways, indented to make the diff clear.
    {
        if (m_overdrawCounts) {
            // We compute the area of the transformed quad, as this should be in pixels.
            float area = quadArea(drawQuad->quadTransform().mapQuad(FloatQuad(drawQuad->quadRect())));
            if (keepQuad) {
                bool didReduceQuadSize = culledRect != drawQuad->quadRect();
                if (didReduceQuadSize) {
                    float visibleQuadRectArea = quadArea(drawQuad->quadTransform().mapQuad(FloatQuad(drawQuad->quadVisibleRect())));
                    m_overdrawCounts->m_pixelsCulled += area - visibleQuadRectArea;
                    area = visibleQuadRectArea;
                }
                IntRect visibleOpaqueRect(drawQuad->quadVisibleRect());
                visibleOpaqueRect.intersect(drawQuad->opaqueRect());
                FloatQuad visibleOpaqueQuad = drawQuad->quadTransform().mapQuad(FloatQuad(visibleOpaqueRect));
                float opaqueArea = quadArea(visibleOpaqueQuad);
                m_overdrawCounts->m_pixelsDrawnOpaque += opaqueArea;
                m_overdrawCounts->m_pixelsDrawnTransparent += area - opaqueArea;
            } else
                m_overdrawCounts->m_pixelsCulled += area;
        }
    }

    if (keepQuad)
        m_quadList.append(drawQuad.release());
}

} // namespace WebCore
#endif // USE(ACCELERATED_COMPOSITING)
