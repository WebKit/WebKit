/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "cc/CCLayerTreeHostImpl.h"

#include "Extensions3D.h"
#include "GraphicsContext3D.h"
#include "LayerRendererChromium.h"
#include "TraceEvent.h"
#include "cc/CCLayerTreeHost.h"
#include "cc/CCMainThread.h"
#include "cc/CCMainThreadTask.h"
#include "cc/CCThreadTask.h"
#include <wtf/CurrentTime.h>

namespace WebCore {

PassOwnPtr<CCLayerTreeHostImpl> CCLayerTreeHostImpl::create(const CCSettings& settings)
{
    return adoptPtr(new CCLayerTreeHostImpl(settings));
}

CCLayerTreeHostImpl::CCLayerTreeHostImpl(const CCSettings& settings)
    : m_sourceFrameNumber(-1)
    , m_frameNumber(0)
    , m_settings(settings)
{
}

CCLayerTreeHostImpl::~CCLayerTreeHostImpl()
{
    TRACE_EVENT("CCLayerTreeHostImpl::~CCLayerTreeHostImpl()", this, 0);
    if (m_layerRenderer)
        m_layerRenderer->close();
}

void CCLayerTreeHostImpl::beginCommit()
{
}

void CCLayerTreeHostImpl::commitComplete()
{
}

GraphicsContext3D* CCLayerTreeHostImpl::context()
{
    return m_layerRenderer ? m_layerRenderer->context() : 0;
}

void CCLayerTreeHostImpl::drawLayers()
{
    TRACE_EVENT("CCLayerTreeHostImpl::drawLayers", this, 0);
    ASSERT(m_layerRenderer);
    if (m_layerRenderer->rootLayer())
        m_layerRenderer->drawLayers();

    ++m_frameNumber;
}

void CCLayerTreeHostImpl::finishAllRendering()
{
    m_layerRenderer->finish();
}

bool CCLayerTreeHostImpl::isContextLost()
{
    ASSERT(m_layerRenderer);
    return m_layerRenderer->isContextLost();
}

const LayerRendererCapabilities& CCLayerTreeHostImpl::layerRendererCapabilities() const
{
    return m_layerRenderer->capabilities();
}

void CCLayerTreeHostImpl::present()
{
    ASSERT(m_layerRenderer && !isContextLost());
    m_layerRenderer->present();
}

void CCLayerTreeHostImpl::readback(void* pixels, const IntRect& rect)
{
    ASSERT(m_layerRenderer && !isContextLost());
    m_layerRenderer->getFramebufferPixels(pixels, rect);
}

void CCLayerTreeHostImpl::setRootLayer(PassRefPtr<CCLayerImpl> layer)
{
    m_rootLayerImpl = layer;
}

void CCLayerTreeHostImpl::setVisible(bool visible)
{
    if (m_layerRenderer && !visible)
        m_layerRenderer->releaseRenderSurfaceTextures();
}

bool CCLayerTreeHostImpl::initializeLayerRenderer(PassRefPtr<GraphicsContext3D> context)
{
    OwnPtr<LayerRendererChromium> layerRenderer;
    layerRenderer = LayerRendererChromium::create(this, context);

    // If creation failed, and we had asked for accelerated painting, disable accelerated painting
    // and try creating the renderer again.
    if (!layerRenderer && m_settings.acceleratePainting) {
        m_settings.acceleratePainting = false;

        layerRenderer = LayerRendererChromium::create(this, context);
    }

    // If we had a previous layer renderer, then its context must have been lost along with all of its resources.
    // Let the old layer renderer known its resources are gone.
    if (m_layerRenderer) {
        m_layerRenderer->setContentsTextureMemoryUseBytes(0);
        m_layerRenderer->close();
    }

    m_layerRenderer = layerRenderer.release();
    return m_layerRenderer;
}

void CCLayerTreeHostImpl::setViewport(const IntSize& viewportSize)
{
    bool changed = viewportSize != m_viewportSize;
    m_viewportSize = viewportSize;
    if (changed)
        m_layerRenderer->viewportChanged();
}

void CCLayerTreeHostImpl::setZoomAnimatorTransform(const TransformationMatrix& zoom)
{
    m_layerRenderer->setZoomAnimatorTransform(zoom);
}

}
