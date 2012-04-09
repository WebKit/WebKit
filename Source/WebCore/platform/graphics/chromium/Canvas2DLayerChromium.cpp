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
#include "LayerRendererChromium.h" // For the GLC() macro
#include "TextureCopier.h"
#include "cc/CCLayerTreeHost.h"
#include "cc/CCTextureLayerImpl.h"
#include "cc/CCTextureUpdater.h"

#include "SkCanvas.h"

namespace WebCore {

PassRefPtr<Canvas2DLayerChromium> Canvas2DLayerChromium::create(PassRefPtr<GraphicsContext3D> context, const IntSize& size)
{
    return adoptRef(new Canvas2DLayerChromium(context, size));
}

Canvas2DLayerChromium::Canvas2DLayerChromium(PassRefPtr<GraphicsContext3D> context, const IntSize& size)
    : CanvasLayerChromium()
    , m_context(context)
    , m_contextLost(false)
    , m_size(size)
    , m_backTextureId(0)
    , m_useDoubleBuffering(CCProxy::hasImplThread())
    , m_canvas(0)
{
}

Canvas2DLayerChromium::~Canvas2DLayerChromium()
{
    if (m_context && layerTreeHost())
        layerTreeHost()->stopRateLimiter(m_context.get());
}

void Canvas2DLayerChromium::setTextureId(unsigned textureId)
{
    m_backTextureId = textureId;
    setNeedsCommit();
}

void Canvas2DLayerChromium::setNeedsDisplayRect(const FloatRect& dirtyRect)
{
    LayerChromium::setNeedsDisplayRect(dirtyRect);

    if (layerTreeHost())
        layerTreeHost()->startRateLimiter(m_context.get());
}

bool Canvas2DLayerChromium::drawsContent() const
{
    return LayerChromium::drawsContent() && m_backTextureId && !m_size.isEmpty()
        && !m_contextLost;
}

void Canvas2DLayerChromium::setCanvas(SkCanvas* canvas)
{
    m_canvas = canvas;
}

void Canvas2DLayerChromium::paintContentsIfDirty(const CCOcclusionTracker* /* occlusion */)
{
    TRACE_EVENT("Canvas2DLayerChromium::paintContentsIfDirty", this, 0);
    if (!drawsContent())
        return;

    if (m_useDoubleBuffering && layerTreeHost()) {
        TextureManager* textureManager = layerTreeHost()->contentsTextureManager();
        if (m_frontTexture)
            m_frontTexture->setTextureManager(textureManager);
        else
            m_frontTexture = ManagedTexture::create(textureManager);
        m_frontTexture->reserve(m_size, GraphicsContext3D::RGBA);
    }

    if (!needsDisplay())
        return;

    m_needsDisplay = false;

    bool success = m_context->makeContextCurrent();
    ASSERT_UNUSED(success, success);

    m_contextLost = m_context->getExtensions()->getGraphicsResetStatusARB() != GraphicsContext3D::NO_ERROR;
    if (m_contextLost)
        return;

    if (m_canvas) {
        TRACE_EVENT("SkCanvas::flush", m_canvas, 0);
        m_canvas->flush();
    }

    TRACE_EVENT("GraphicsContext3D::flush", m_context, 0);
    m_context->flush();
}

void Canvas2DLayerChromium::updateCompositorResources(GraphicsContext3D* context, CCTextureUpdater& updater)
{
    if (!m_backTextureId || !m_frontTexture || !m_frontTexture->isValid(m_size, GraphicsContext3D::RGBA))
        return;

    m_frontTexture->allocate(updater.allocator());
    updater.appendCopy(m_backTextureId, m_frontTexture->textureId(), m_size);
}

void Canvas2DLayerChromium::pushPropertiesTo(CCLayerImpl* layer)
{
    CanvasLayerChromium::pushPropertiesTo(layer);

    CCTextureLayerImpl* textureLayer = static_cast<CCTextureLayerImpl*>(layer);
    if (m_useDoubleBuffering) {
        if (m_frontTexture && m_frontTexture->isValid(m_size, GraphicsContext3D::RGBA))
            textureLayer->setTextureId(m_frontTexture->textureId());
        else
            textureLayer->setTextureId(0);
    } else
        textureLayer->setTextureId(m_backTextureId);
}

void Canvas2DLayerChromium::unreserveContentsTexture()
{
    if (m_useDoubleBuffering)
        m_frontTexture->unreserve();
}

}

#endif // USE(ACCELERATED_COMPOSITING)
