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

PassRefPtr<Canvas2DLayerChromium> Canvas2DLayerChromium::create(PassRefPtr<GraphicsContext3D> context, const IntSize& size, DeferralMode deferralMode)
{
    return adoptRef(new Canvas2DLayerChromium(context, size, deferralMode));
}

Canvas2DLayerChromium::Canvas2DLayerChromium(PassRefPtr<GraphicsContext3D> context, const IntSize& size, DeferralMode deferralMode)
    : CanvasLayerChromium()
    , m_context(context)
    , m_contextLost(false)
    , m_size(size)
    , m_backTextureId(0)
    , m_canvas(0)
    , m_deferralMode(deferralMode)
{
    // FIXME: We currently turn off double buffering when canvas rendering is
    // deferred. What we should be doing is to use a smarter heuristic based
    // on GPU resource monitoring and other factors to chose between single
    // and double buffering.
    m_useDoubleBuffering = CCProxy::hasImplThread() && deferralMode == NonDeferred;

    // The threaded compositor is self rate-limiting when rendering
    // to a single buffered canvas layer.
    m_useRateLimiter = !CCProxy::hasImplThread() || m_useDoubleBuffering;
}

Canvas2DLayerChromium::~Canvas2DLayerChromium()
{
    setTextureId(0);
    if (m_context && m_useRateLimiter && layerTreeHost())
        layerTreeHost()->stopRateLimiter(m_context.get());
}

bool Canvas2DLayerChromium::drawingIntoImplThreadTexture() const
{
    return !m_useDoubleBuffering && CCProxy::hasImplThread() && layerTreeHost();
}

void Canvas2DLayerChromium::setTextureId(unsigned textureId)
{
    if (m_backTextureId == textureId)
        return;

    if (m_backTextureId && drawingIntoImplThreadTexture()) {
        // The impl tree may be referencing the old m_backTexture, which may
        // soon be deleted. We must make sure the layer tree on the impl thread
        // is not drawn until the next commit, which will re-synchronize the
        // layer trees.
        layerTreeHost()->acquireLayerTextures();
    }
    m_backTextureId = textureId;
    setNeedsCommit();
}

void Canvas2DLayerChromium::setNeedsDisplayRect(const FloatRect& dirtyRect)
{
    LayerChromium::setNeedsDisplayRect(dirtyRect);

    if (m_useRateLimiter && layerTreeHost())
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

void Canvas2DLayerChromium::update(CCTextureUpdater& updater, const CCOcclusionTracker*)
{
    TRACE_EVENT0("cc", "Canvas2DLayerChromium::update");
    if (!drawsContent())
        return;

    if (m_useDoubleBuffering && layerTreeHost()) {
        TextureManager* textureManager = layerTreeHost()->contentsTextureManager();
        if (m_frontTexture)
            m_frontTexture->setTextureManager(textureManager);
        else
            m_frontTexture = ManagedTexture::create(textureManager);
        if (m_frontTexture->reserve(m_size, GraphicsContext3D::RGBA))
            updater.appendManagedCopy(m_backTextureId, m_frontTexture.get(), m_size);
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
        TRACE_EVENT0("cc", "SkCanvas::flush");
        m_canvas->flush();
    }

    TRACE_EVENT0("cc", "GraphicsContext3D::flush");
    m_context->flush();
}

void Canvas2DLayerChromium::layerWillDraw(WillDrawCondition condition) const
{
    if (drawingIntoImplThreadTexture()) {
        if (condition == WillDrawUnconditionally || m_deferralMode == NonDeferred)
            layerTreeHost()->acquireLayerTextures();
    }
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

}

#endif // USE(ACCELERATED_COMPOSITING)
