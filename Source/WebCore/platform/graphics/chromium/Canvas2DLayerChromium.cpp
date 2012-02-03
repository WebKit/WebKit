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

#include "cc/CCCanvasLayerImpl.h"
#include "cc/CCLayerTreeHost.h"
#include "cc/CCTextureUpdater.h"
#include "Extensions3DChromium.h"
#include "GraphicsContext3D.h"
#include "LayerRendererChromium.h" // For the GLC() macro

#include "SkCanvas.h"

namespace WebCore {

PassRefPtr<Canvas2DLayerChromium> Canvas2DLayerChromium::create(GraphicsContext3D* context, const IntSize& size)
{
    return adoptRef(new Canvas2DLayerChromium(context, size));
}

Canvas2DLayerChromium::Canvas2DLayerChromium(GraphicsContext3D* context, const IntSize& size)
    : CanvasLayerChromium()
    , m_context(context)
    , m_size(size)
    , m_backTextureId(0)
    , m_fbo(0)
    , m_useDoubleBuffering(CCProxy::hasImplThread())
    , m_canvas(0)
{
    if (m_useDoubleBuffering)
        GLC(m_context, m_fbo = m_context->createFramebuffer());
}

Canvas2DLayerChromium::~Canvas2DLayerChromium()
{
    if (m_useDoubleBuffering && m_fbo)
       GLC(m_context, m_context->deleteFramebuffer(m_fbo));
}

void Canvas2DLayerChromium::setTextureId(unsigned textureId)
{
    m_backTextureId = textureId;
    setNeedsCommit();
}

void Canvas2DLayerChromium::contentChanged()
{
    if (layerTreeHost())
        layerTreeHost()->startRateLimiter(m_context);

    m_updateRect = FloatRect(FloatPoint(), contentBounds());
    setNeedsDisplay();
}

bool Canvas2DLayerChromium::drawsContent() const
{
    return LayerChromium::drawsContent() && m_backTextureId && !m_size.isEmpty()
        && m_context && (m_context->getExtensions()->getGraphicsResetStatusARB() == GraphicsContext3D::NO_ERROR);
}

void Canvas2DLayerChromium::setCanvas(SkCanvas* canvas)
{
    m_canvas = canvas;
}

void Canvas2DLayerChromium::paintContentsIfDirty(const Region& /* occludedScreenSpace */)
{
    if (!drawsContent())
        return;

    if (m_useDoubleBuffering)
        m_frontTexture->reserve(m_size, GraphicsContext3D::RGBA);

    if (!needsDisplay())
        return;

    m_needsDisplay = false;

    bool success = m_context->makeContextCurrent();
    ASSERT_UNUSED(success, success);

    if (m_canvas)
        m_canvas->flush();

    m_context->flush();
}

void Canvas2DLayerChromium::setLayerTreeHost(CCLayerTreeHost* host)
{
    if (layerTreeHost() != host)
        setTextureManager(host ? host->contentsTextureManager() : 0);

    CanvasLayerChromium::setLayerTreeHost(host);
}

void Canvas2DLayerChromium::setTextureManager(TextureManager* textureManager)
{
    if (textureManager && m_useDoubleBuffering)
        m_frontTexture = ManagedTexture::create(textureManager);
    else
        m_frontTexture.clear();
}

void Canvas2DLayerChromium::updateCompositorResources(GraphicsContext3D* context, CCTextureUpdater& updater)
{
    if (!m_backTextureId || !m_frontTexture || !m_frontTexture->isValid(m_size, GraphicsContext3D::RGBA))
        return;

    m_frontTexture->bindTexture(context, updater.allocator());

    GLC(context, context->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_fbo));
    GLC(context, context->framebufferTexture2D(GraphicsContext3D::FRAMEBUFFER, GraphicsContext3D::COLOR_ATTACHMENT0, GraphicsContext3D::TEXTURE_2D, m_backTextureId, 0));
    // FIXME: The copy operation will fail if the m_backTexture is allocated as BGRA since glCopyTex(Sub)Image2D doesn't
    //        support the BGRA format. See bug https://bugs.webkit.org/show_bug.cgi?id=75142
    GLC(context, context->copyTexSubImage2D(GraphicsContext3D::TEXTURE_2D, 0, 0, 0, 0, 0, m_size.width(), m_size.height()));
    GLC(context, context->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, 0));
    GLC(context, context->flush());
}

void Canvas2DLayerChromium::pushPropertiesTo(CCLayerImpl* layer)
{
    CanvasLayerChromium::pushPropertiesTo(layer);

    CCCanvasLayerImpl* canvasLayer = static_cast<CCCanvasLayerImpl*>(layer);
    if (m_useDoubleBuffering)
        canvasLayer->setTextureId(m_frontTexture->textureId());
    else
        canvasLayer->setTextureId(m_backTextureId);
}

void Canvas2DLayerChromium::unreserveContentsTexture()
{
    if (m_useDoubleBuffering)
        m_frontTexture->unreserve();
}

void Canvas2DLayerChromium::cleanupResources()
{
    if (m_useDoubleBuffering)
        m_frontTexture.clear();
}

}

#endif // USE(ACCELERATED_COMPOSITING)
