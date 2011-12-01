/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "Canvas2DLayerChromium.h"

#include "Extensions3DChromium.h"
#include "GraphicsContext3D.h"
#include "cc/CCLayerTreeHost.h"

#if USE(SKIA)
#include "GrContext.h"
#endif

namespace WebCore {

PassRefPtr<Canvas2DLayerChromium> Canvas2DLayerChromium::create(GraphicsContext3D* context)
{
    return adoptRef(new Canvas2DLayerChromium(context));
}

Canvas2DLayerChromium::Canvas2DLayerChromium(GraphicsContext3D* context)
    : CanvasLayerChromium(0)
    , m_context(context)
{
}

Canvas2DLayerChromium::~Canvas2DLayerChromium()
{
}

bool Canvas2DLayerChromium::drawsContent() const
{
    return m_textureId && (m_context
            && (m_context->getExtensions()->getGraphicsResetStatusARB() == GraphicsContext3D::NO_ERROR));
}

void Canvas2DLayerChromium::updateCompositorResources(GraphicsContext3D*, CCTextureUpdater&)
{
    if (!m_needsDisplay || !drawsContent())
        return;

    if (m_context) {
#if USE(SKIA)
        GrContext* grContext = m_context->grContext();
        if (grContext) {
            m_context->makeContextCurrent();
            grContext->flush();
        }
#endif
        m_context->flush();
    }

    m_updateRect = FloatRect(FloatPoint(), bounds());
    m_needsDisplay = false;
}

void Canvas2DLayerChromium::contentChanged()
{
    if (layerTreeHost())
        layerTreeHost()->startRateLimiter(m_context);
}

}
#endif // USE(ACCELERATED_COMPOSITING)
