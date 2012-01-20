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

#include "WebGLLayerChromium.h"

#include "DrawingBuffer.h"
#include "Extensions3DChromium.h"
#include "GraphicsContext3D.h"
#include "LayerRendererChromium.h"
#include "TraceEvent.h"

namespace WebCore {

PassRefPtr<WebGLLayerChromium> WebGLLayerChromium::create()
{
    return adoptRef(new WebGLLayerChromium());
}

WebGLLayerChromium::WebGLLayerChromium()
    : CanvasLayerChromium()
    , m_hasAlpha(true)
    , m_premultipliedAlpha(true)
    , m_textureId(0)
    , m_textureChanged(true)
    , m_textureUpdated(false)
    , m_drawingBuffer(0)
{
}

WebGLLayerChromium::~WebGLLayerChromium()
{
    if (context() && layerTreeHost())
        layerTreeHost()->stopRateLimiter(context());
}

bool WebGLLayerChromium::drawsContent() const
{
    return LayerChromium::drawsContent() && context() && (context()->getExtensions()->getGraphicsResetStatusARB() == GraphicsContext3D::NO_ERROR);
}

void WebGLLayerChromium::updateCompositorResources(GraphicsContext3D* rendererContext, CCTextureUpdater&)
{
    if (!drawsContent())
        return;

    if (!m_needsDisplay)
        return;

    if (m_textureChanged) {
        rendererContext->bindTexture(GraphicsContext3D::TEXTURE_2D, m_textureId);
        // Set the min-mag filters to linear and wrap modes to GL_CLAMP_TO_EDGE
        // to get around NPOT texture limitations of GLES.
        rendererContext->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MIN_FILTER, GraphicsContext3D::LINEAR);
        rendererContext->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MAG_FILTER, GraphicsContext3D::LINEAR);
        rendererContext->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_S, GraphicsContext3D::CLAMP_TO_EDGE);
        rendererContext->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_T, GraphicsContext3D::CLAMP_TO_EDGE);
        m_textureChanged = false;
    }
    // Update the contents of the texture used by the compositor.
    if (m_needsDisplay && m_textureUpdated) {
        // publishToPlatformLayer prepares the contents of the off-screen render target for use by the compositor.
        drawingBuffer()->publishToPlatformLayer();
        context()->markLayerComposited();
        m_updateRect = FloatRect(FloatPoint(), bounds());
        m_needsDisplay = false;
        m_textureUpdated = false;
    }
}

void WebGLLayerChromium::pushPropertiesTo(CCLayerImpl* layer)
{
    CanvasLayerChromium::pushPropertiesTo(layer);

    CCCanvasLayerImpl* canvasLayer = static_cast<CCCanvasLayerImpl*>(layer);
    canvasLayer->setTextureId(m_textureId);
    canvasLayer->setHasAlpha(m_hasAlpha);
    canvasLayer->setPremultipliedAlpha(m_premultipliedAlpha);
}

bool WebGLLayerChromium::paintRenderedResultsToCanvas(ImageBuffer* imageBuffer)
{
    if (m_textureUpdated || !layerRendererContext() || !drawsContent())
        return false;

    IntSize framebufferSize = context()->getInternalFramebufferSize();
    ASSERT(layerRendererContext());

    // This would ideally be done in the webgl context, but that isn't possible yet.
    Platform3DObject framebuffer = layerRendererContext()->createFramebuffer();
    layerRendererContext()->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, framebuffer);
    layerRendererContext()->framebufferTexture2D(GraphicsContext3D::FRAMEBUFFER, GraphicsContext3D::COLOR_ATTACHMENT0, GraphicsContext3D::TEXTURE_2D, m_textureId, 0);

    Extensions3DChromium* extensions = static_cast<Extensions3DChromium*>(layerRendererContext()->getExtensions());
    extensions->paintFramebufferToCanvas(framebuffer, framebufferSize.width(), framebufferSize.height(), !context()->getContextAttributes().premultipliedAlpha, imageBuffer);
    layerRendererContext()->deleteFramebuffer(framebuffer);
    return true;
}

void WebGLLayerChromium::contentChanged()
{
    m_textureUpdated = true;
    // If WebGL commands are issued outside of a the animation callbacks, then use
    // call rateLimitOffscreenContextCHROMIUM() to keep the context from getting too far ahead.
    if (layerTreeHost())
        layerTreeHost()->startRateLimiter(context());
}

void WebGLLayerChromium::setDrawingBuffer(DrawingBuffer* drawingBuffer)
{
    bool drawingBufferChanged = (m_drawingBuffer != drawingBuffer);

    // The GraphicsContext3D used by the layer is the context associated with
    // the drawing buffer. If the drawing buffer is changing, make sure
    // to stop the rate limiter on the old context, not the new context from the
    // new drawing buffer.
    GraphicsContext3D* context3D = context();
    if (layerTreeHost() && drawingBufferChanged && context3D)
        layerTreeHost()->stopRateLimiter(context3D);

    m_drawingBuffer = drawingBuffer;
    if (!m_drawingBuffer)
        return;

    unsigned int textureId = m_drawingBuffer->platformColorBuffer();
    if (textureId != m_textureId || drawingBufferChanged) {
        m_textureChanged = true;
        m_textureUpdated = true;
    }
    m_textureId = textureId;
    GraphicsContext3D::Attributes attributes = context()->getContextAttributes();
    m_hasAlpha = attributes.alpha;
    m_premultipliedAlpha = attributes.premultipliedAlpha;
}

GraphicsContext3D* WebGLLayerChromium::context() const
{
    if (drawingBuffer())
        return drawingBuffer()->graphicsContext3D().get();

    return 0;
}

GraphicsContext3D* WebGLLayerChromium::layerRendererContext()
{
    // FIXME: In the threaded case, paintRenderedResultsToCanvas must be
    // refactored to be asynchronous. Currently this is unimplemented.
    if (!layerTreeHost() || CCProxy::hasImplThread())
        return 0;
    return layerTreeHost()->context();
}

}

#endif // USE(ACCELERATED_COMPOSITING)
