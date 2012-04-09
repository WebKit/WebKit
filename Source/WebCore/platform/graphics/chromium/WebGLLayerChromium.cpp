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
#include "TextureCopier.h"
#include "TraceEvent.h"
#include "cc/CCLayerTreeHost.h"
#include "cc/CCTextureLayerImpl.h"
#include "cc/CCTextureUpdater.h"

namespace WebCore {

PassRefPtr<WebGLLayerChromium> WebGLLayerChromium::create()
{
    return adoptRef(new WebGLLayerChromium());
}

WebGLLayerChromium::WebGLLayerChromium()
    : CanvasLayerChromium()
    , m_hasAlpha(true)
    , m_premultipliedAlpha(true)
    , m_contextLost(false)
    , m_textureId(0)
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
    return LayerChromium::drawsContent() && !m_contextLost;
}

void WebGLLayerChromium::paintContentsIfDirty(const CCOcclusionTracker* /* occlusion */)
{
    if (!drawsContent() || !m_needsDisplay)
        return;

    m_drawingBuffer->prepareBackBuffer();

    context()->flush();
    context()->markLayerComposited();
    m_needsDisplay = false;
    m_contextLost = context()->getExtensions()->getGraphicsResetStatusARB() != GraphicsContext3D::NO_ERROR;
}

void WebGLLayerChromium::updateCompositorResources(GraphicsContext3D*, CCTextureUpdater& updater)
{
    if (!m_drawingBuffer)
        return;

    m_textureId = m_drawingBuffer->frontColorBuffer();
    if (m_drawingBuffer->requiresCopyFromBackToFrontBuffer())
        updater.appendCopy(m_drawingBuffer->colorBuffer(), m_textureId, bounds());
}

void WebGLLayerChromium::pushPropertiesTo(CCLayerImpl* layer)
{
    CanvasLayerChromium::pushPropertiesTo(layer);

    CCTextureLayerImpl* textureLayer = static_cast<CCTextureLayerImpl*>(layer);
    textureLayer->setTextureId(m_textureId);
    textureLayer->setHasAlpha(m_hasAlpha);
    textureLayer->setPremultipliedAlpha(m_premultipliedAlpha);
}

bool WebGLLayerChromium::paintRenderedResultsToCanvas(ImageBuffer* imageBuffer)
{
    if (!m_drawingBuffer || !drawsContent())
        return false;

    IntSize framebufferSize = context()->getInternalFramebufferSize();

    // Since we're using the same context as WebGL, we have to restore any state we change (in this case, just the framebuffer binding).
    // FIXME: The WebGLRenderingContext tracks the current framebuffer binding, it would be slightly more efficient to use this value
    // rather than querying it off of the context.
    GC3Dint previousFramebuffer = 0;
    context()->getIntegerv(GraphicsContext3D::FRAMEBUFFER_BINDING, &previousFramebuffer);

    Platform3DObject framebuffer = context()->createFramebuffer();
    context()->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, framebuffer);
    context()->framebufferTexture2D(GraphicsContext3D::FRAMEBUFFER, GraphicsContext3D::COLOR_ATTACHMENT0, GraphicsContext3D::TEXTURE_2D, m_textureId, 0);

    Extensions3DChromium* extensions = static_cast<Extensions3DChromium*>(context()->getExtensions());
    extensions->paintFramebufferToCanvas(framebuffer, framebufferSize.width(), framebufferSize.height(), !context()->getContextAttributes().premultipliedAlpha, imageBuffer);
    context()->deleteFramebuffer(framebuffer);

    context()->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, previousFramebuffer);
    return true;
}

void WebGLLayerChromium::setNeedsDisplayRect(const FloatRect& dirtyRect)
{
    LayerChromium::setNeedsDisplayRect(dirtyRect);

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

    GraphicsContext3D::Attributes attributes = context()->getContextAttributes();
    m_hasAlpha = attributes.alpha;
    m_premultipliedAlpha = attributes.premultipliedAlpha;
    m_contextLost = context()->getExtensions()->getGraphicsResetStatusARB() != GraphicsContext3D::NO_ERROR;
}

GraphicsContext3D* WebGLLayerChromium::context() const
{
    if (drawingBuffer())
        return drawingBuffer()->graphicsContext3D().get();

    return 0;
}

}

#endif // USE(ACCELERATED_COMPOSITING)
