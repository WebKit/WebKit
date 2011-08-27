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

#include "cc/CCLayerTreeHost.h"

#include "LayerChromium.h"
#include "LayerPainterChromium.h"
#include "LayerRendererChromium.h"
#include "NonCompositedContentHost.h"
#include "PlatformColor.h"
#include "TraceEvent.h"
#include "cc/CCLayerTreeHostCommitter.h"
#include "cc/CCLayerTreeHostImpl.h"

namespace WebCore {

PassRefPtr<CCLayerTreeHost> CCLayerTreeHost::create(CCLayerTreeHostClient* client, const CCSettings& settings)
{
    RefPtr<CCLayerTreeHost> layerTreeHost = adoptRef(new CCLayerTreeHost(client, settings));
    if (!layerTreeHost->initialize())
        return 0;
    return layerTreeHost;
}

CCLayerTreeHost::CCLayerTreeHost(CCLayerTreeHostClient* client, const CCSettings& settings)
    : m_recreatingGraphicsContext(false)
    , m_maxTextureSize(0)
    , m_contextSupportsMapSub(false)
    , m_animating(false)
    , m_client(client)
    , m_frameNumber(0)
    , m_nonCompositedContentHost(NonCompositedContentHost::create(m_client->createRootLayerPainter()))
    , m_settings(settings)
{
}

bool CCLayerTreeHost::initialize()
{
    m_layerRenderer = createLayerRenderer();
    if (!m_layerRenderer)
        return false;

    // FIXME: In the threaded case, these values will need to be initialized
    // by something other than m_layerRenderer.
    m_maxTextureSize = m_layerRenderer->maxTextureSize();
    m_bestTextureFormat = PlatformColor::bestTextureFormat(m_layerRenderer->context());
    m_contextSupportsMapSub = m_layerRenderer->contextSupportsMapSub();

    m_rootLayer = GraphicsLayer::create(0);
#ifndef NDEBUG
    m_rootLayer->setName("root layer");
#endif
    m_rootLayer->setDrawsContent(false);

    m_rootLayer->addChild(m_nonCompositedContentHost->graphicsLayer());


#if USE(THREADED_COMPOSITING)
    m_proxy = CCLayerTreeHostImplProxy::create(this);
    ASSERT(m_proxy->isStarted());
    m_proxy->setNeedsCommitAndRedraw();
#endif

    return true;
}

CCLayerTreeHost::~CCLayerTreeHost()
{
    TRACE_EVENT("CCLayerTreeHost::~CCLayerTreeHost", this, 0);
#if USE(THREADED_COMPOSITING)
    m_proxy->stop();
    m_proxy.clear();
#endif
}

void CCLayerTreeHost::beginCommit()
{
}

void CCLayerTreeHost::commitComplete()
{
    m_frameNumber++;
}

void CCLayerTreeHost::animateAndLayout(double frameBeginTime)
{
    m_animating = true;
    m_client->animateAndLayout(frameBeginTime);
    m_animating = false;
}

PassOwnPtr<CCLayerTreeHostCommitter> CCLayerTreeHost::createLayerTreeHostCommitter()
{
    // FIXME: only called in threading case, fix when possible.
    return nullptr;
}

PassOwnPtr<CCLayerTreeHostImpl> CCLayerTreeHost::createLayerTreeHostImpl(CCLayerTreeHostImplClient* client)
{
    RefPtr<LayerRendererChromium> renderer(m_layerRenderer);
    return CCLayerTreeHostImpl::create(client, renderer);
}

GraphicsContext3D* CCLayerTreeHost::context()
{
    return m_layerRenderer->context();
}

void CCLayerTreeHost::compositeAndReadback(void *pixels, const IntRect& rect)
{
#if USE(THREADED_COMPOSITING)
    // FIXME: need to implement this.
#else
    composite(false);
    m_layerRenderer->getFramebufferPixels(pixels, rect);
#endif
}

void CCLayerTreeHost::finishAllRendering()
{
#if USE(THREADED_COMPOSITING)
    // FIXME: need to implement this.
#else
    m_layerRenderer->finish();
#endif
}

void CCLayerTreeHost::invalidateRootLayerRect(const IntRect& dirtyRect)
{
    m_nonCompositedContentHost->invalidateRect(dirtyRect);
}

void CCLayerTreeHost::setNeedsCommitAndRedraw()
{
#if USE(THREADED_COMPOSITING)
    TRACE_EVENT("CCLayerTreeHost::setNeedsRedraw", this, 0);
    m_proxy->setNeedsCommitAndRedraw();
#else
    m_client->scheduleComposite();
#endif
}

void CCLayerTreeHost::setNeedsRedraw()
{
#if USE(THREADED_COMPOSITING)
    TRACE_EVENT("CCLayerTreeHost::setNeedsRedraw", this, 0);
    m_proxy->setNeedsRedraw();
#else
    m_client->scheduleComposite();
#endif
}

void CCLayerTreeHost::setRootLayer(GraphicsLayer* layer)
{
    m_nonCompositedContentHost->graphicsLayer()->removeAllChildren();
    m_nonCompositedContentHost->invalidateEntireLayer();
    if (layer) {
        m_nonCompositedContentHost->graphicsLayer()->addChild(layer);
        layer->platformLayer()->setLayerRenderer(m_layerRenderer.get());
    } else
        layerRenderer()->clearRootCCLayerImpl();

}

void CCLayerTreeHost::setViewport(const IntSize& viewportSize, const IntSize& contentsSize, const IntPoint& scrollPosition)
{
    bool visibleRectChanged = m_viewportSize != viewportSize;

    m_viewportSize = viewportSize;
    m_nonCompositedContentHost->setScrollPosition(scrollPosition);
    m_nonCompositedContentHost->graphicsLayer()->setSize(contentsSize);

    if (visibleRectChanged) {
        m_nonCompositedContentHost->invalidateEntireLayer();
        if (m_layerRenderer)
            m_layerRenderer->viewportChanged();
    }

    setNeedsCommitAndRedraw();
}

void CCLayerTreeHost::setVisible(bool visible)
{
#if !USE(THREADED_COMPOSITING)
    if (!visible)
        m_layerRenderer->releaseTextures();
#endif
}

PassRefPtr<LayerRendererChromium> CCLayerTreeHost::createLayerRenderer()
{
    // GraphicsContext3D::create might fail and return 0, in that case fall back to software.
    RefPtr<GraphicsContext3D> context = m_client->createLayerTreeHostContext3D();
    if (!context)
        return 0;

    // Actually create the renderer.
    RefPtr<LayerRendererChromium> layerRenderer = LayerRendererChromium::create(this, context);

    // If creation failed, and we had asked for accelerated painting, disable accelerated painting
    // and try creating the renderer again.
    if (m_settings.acceleratePainting && !layerRenderer) {
        m_settings.acceleratePainting = false;
        layerRenderer = LayerRendererChromium::create(this, context);
    }

    return layerRenderer;
}

TextureManager* CCLayerTreeHost::contentsTextureManager() const
{
    // FIXME: this class should own the contents texture manager
    if (!m_layerRenderer)
        return 0;
    return m_layerRenderer->contentsTextureManager();
}

#if !USE(THREADED_COMPOSITING)
void CCLayerTreeHost::doComposite()
{
#ifndef NDEBUG
    CCLayerTreeHostImplProxy::setImplThread(true);
#endif
    ASSERT(m_layerRenderer);
    m_layerRenderer->updateLayers();
    m_layerRenderer->drawLayers();
#ifndef NDEBUG
    CCLayerTreeHostImplProxy::setImplThread(false);
#endif
}

void CCLayerTreeHost::composite(bool finish)
{
    TRACE_EVENT("CCLayerTreeHost::composite", this, 0);

    if (m_recreatingGraphicsContext) {
        // reallocateRenderer will request a repaint whether or not it succeeded
        // in creating a new context.
        reallocateRenderer();
        m_recreatingGraphicsContext = false;
        return;
    }

    // Do not composite if the compositor context is already lost.
    if (!m_layerRenderer->isCompositorContextLost()) {
        doComposite();

        // Put result onscreen.
        m_layerRenderer->present();
    }

    if (m_layerRenderer->isCompositorContextLost()) {
        // Trying to recover the context right here will not work if GPU process
        // died. This is because GpuChannelHost::OnErrorMessage will only be
        // called at the next iteration of the message loop, reverting our
        // recovery attempts here. Instead, we detach the root layer from the
        // renderer, recreate the renderer at the next message loop iteration
        // and request a repaint yet again.
        m_recreatingGraphicsContext = true;
        setNeedsCommitAndRedraw();
    }
}

void CCLayerTreeHost::loseCompositorContext()
{
    m_recreatingGraphicsContext = true;
}

void CCLayerTreeHost::reallocateRenderer()
{
    RefPtr<LayerRendererChromium> layerRenderer = createLayerRenderer();
    if (!layerRenderer) {
        m_client->didRecreateGraphicsContext(false);
        return;
    }

    layerRenderer->setLayerRendererRecursive(m_rootLayer->platformLayer());
    m_layerRenderer = layerRenderer;

    m_client->didRecreateGraphicsContext(true);
}
#endif // !USE(THREADED_COMPOSITING)

}
