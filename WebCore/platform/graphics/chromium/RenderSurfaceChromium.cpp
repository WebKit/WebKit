/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "RenderSurfaceChromium.h"

#include "GraphicsContext3D.h"
#include "LayerRendererChromium.h"

namespace WebCore {

RenderSurfaceChromium::RenderSurfaceChromium(LayerChromium* owningLayer)
    : m_owningLayer(owningLayer)
    , m_contentsTextureId(0)
{
}

RenderSurfaceChromium::~RenderSurfaceChromium()
{
    cleanupResources();
}

void RenderSurfaceChromium::cleanupResources()
{
    if (!m_contentsTextureId)
        return;

    ASSERT(layerRenderer());

    layerRenderer()->deleteLayerTexture(m_contentsTextureId);
    m_contentsTextureId = 0;
    m_allocatedTextureSize = IntSize();
}

LayerRendererChromium* RenderSurfaceChromium::layerRenderer()
{
    ASSERT(m_owningLayer);
    return m_owningLayer->layerRenderer();
}

void RenderSurfaceChromium::prepareContentsTexture()
{
    ASSERT(m_owningLayer);

    if (!m_contentsTextureId) {
        m_contentsTextureId = layerRenderer()->createLayerTexture();
        ASSERT(m_contentsTextureId);
        m_allocatedTextureSize = IntSize();
    }

    IntSize requiredSize(m_contentRect.width(), m_contentRect.height());
    if (m_allocatedTextureSize != requiredSize) {
        GraphicsContext3D* context = m_owningLayer->layerRenderer()->context();
        GLC(context, context->bindTexture(GraphicsContext3D::TEXTURE_2D, m_contentsTextureId));
        GLC(context, context->texImage2D(GraphicsContext3D::TEXTURE_2D, 0, GraphicsContext3D::RGBA,
                                             requiredSize.width(), requiredSize.height(), 0, GraphicsContext3D::RGBA,
                                             GraphicsContext3D::UNSIGNED_BYTE, 0));
        m_allocatedTextureSize = requiredSize;
    }
}

}
#endif // USE(ACCELERATED_COMPOSITING)
