/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "cc/CCRenderPass.h"

#include "cc/CCLayerImpl.h"
#include "cc/CCQuadCuller.h"
#include "cc/CCRenderSurfaceDrawQuad.h"
#include "cc/CCSharedQuadState.h"

namespace WebCore {

PassOwnPtr<CCRenderPass> CCRenderPass::create(CCRenderSurface* targetSurface)
{
    return adoptPtr(new CCRenderPass(targetSurface));
}

CCRenderPass::CCRenderPass(CCRenderSurface* targetSurface)
    : m_targetSurface(targetSurface)
{
    ASSERT(m_targetSurface);
}

void CCRenderPass::appendQuadsForLayer(CCLayerImpl* layer)
{
    OwnPtr<CCSharedQuadState> sharedQuadState = layer->createSharedQuadState();
    layer->appendQuads(m_quadList, sharedQuadState.get());
    layer->appendDebugBorderQuad(m_quadList, sharedQuadState.get());
    m_sharedQuadStateList.append(sharedQuadState.release());
}

void CCRenderPass::appendQuadsForRenderSurfaceLayer(CCLayerImpl* layer)
{
    // FIXME: render surface layers should be a CCLayerImpl-derived class and
    // not be handled specially here.
    CCRenderSurface* surface = layer->renderSurface();
    bool isOpaque = false;
    OwnPtr<CCSharedQuadState> sharedQuadState = CCSharedQuadState::create(surface->drawTransform(), surface->drawTransform(), surface->contentRect(), surface->clipRect(), surface->drawOpacity(), isOpaque);
    m_quadList.append(CCRenderSurfaceDrawQuad::create(sharedQuadState.get(), surface->contentRect(), layer, surfaceDamageRect()));
    m_sharedQuadStateList.append(sharedQuadState.release());
}

void CCRenderPass::optimizeQuads()
{
    CCQuadCuller::cullOccludedQuads(m_quadList);
}

}
