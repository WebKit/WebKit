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
#include "TraceEvent.h"
#include "TreeSynchronizer.h"
#include "cc/CCLayerTreeHostImpl.h"
#include "cc/CCSingleThreadProxy.h"
#include "cc/CCThread.h"
#include "cc/CCThreadProxy.h"

namespace WebCore {

PassRefPtr<CCLayerTreeHost> CCLayerTreeHost::create(CCLayerTreeHostClient* client, const CCSettings& settings)
{
    RefPtr<CCLayerTreeHost> layerTreeHost = adoptRef(new CCLayerTreeHost(client, settings));
    if (!layerTreeHost->initialize())
        return 0;
    return layerTreeHost;
}

CCLayerTreeHost::CCLayerTreeHost(CCLayerTreeHostClient* client, const CCSettings& settings)
    : m_animating(false)
    , m_client(client)
    , m_frameNumber(0)
    , m_nonCompositedContentHost(NonCompositedContentHost::create(m_client->createRootLayerPainter()))
    , m_settings(settings)
    , m_visible(true)
{
}

bool CCLayerTreeHost::initialize()
{
    if (m_settings.enableCompositorThread) {
        // Accelerated Painting is not supported in threaded mode. Turn it off.
        m_settings.acceleratePainting = false;
        m_proxy = CCThreadProxy::create(this);
    } else
        m_proxy = CCSingleThreadProxy::create(this);
    m_proxy->start();

    if (!m_proxy->initializeLayerRenderer(this))
        return false;

    // Update m_settings based on capabilities that we got back from the renderer.
    m_settings.acceleratePainting = m_proxy->layerRendererCapabilities().usingAcceleratedPainting;

    m_rootLayer = GraphicsLayer::create(0);
#ifndef NDEBUG
    m_rootLayer->setName("root layer");
#endif
    m_rootLayer->setDrawsContent(false);

    m_rootLayer->addChild(m_nonCompositedContentHost->graphicsLayer());

    // We changed the root layer. Tell the proxy a commit is needed.
    m_proxy->setNeedsCommitAndRedraw();

    return true;
}

CCLayerTreeHost::~CCLayerTreeHost()
{
    TRACE_EVENT("CCLayerTreeHost::~CCLayerTreeHost", this, 0);
    m_proxy->stop();
    m_proxy.clear();
}

void CCLayerTreeHost::animateAndLayout(double frameBeginTime)
{
    m_animating = true;
    m_client->animateAndLayout(frameBeginTime);
    m_animating = false;
}

void CCLayerTreeHost::preCommit(CCLayerTreeHostImpl* hostImpl)
{
    hostImpl->setVisible(m_visible);
    hostImpl->setViewport(viewportSize());
}

void CCLayerTreeHost::commitTo(CCLayerTreeHostImpl* hostImpl)
{
    ASSERT(CCProxy::isImplThread());
    TRACE_EVENT("CCLayerTreeHost::commitTo", this, 0);
    hostImpl->setSourceFrameNumber(frameNumber());

    // Synchronize trees, if one exists at all...
    if (rootLayer())
        hostImpl->setRootLayer(TreeSynchronizer::synchronizeTrees(rootLayer()->platformLayer(), hostImpl->rootLayer()));
    else
        hostImpl->setRootLayer(0);

    m_frameNumber++;
}

PassOwnPtr<CCThread> CCLayerTreeHost::createCompositorThread()
{
    return m_client->createCompositorThread();
}

PassRefPtr<GraphicsContext3D> CCLayerTreeHost::createLayerTreeHostContext3D()
{
    return m_client->createLayerTreeHostContext3D();
}

PassOwnPtr<CCLayerTreeHostImpl> CCLayerTreeHost::createLayerTreeHostImpl()
{
    return CCLayerTreeHostImpl::create(m_settings);
}

void CCLayerTreeHost::didRecreateGraphicsContext(bool success)
{
    if (rootLayer())
        rootLayer()->platformLayer()->cleanupResourcesRecursive();
    m_client->didRecreateGraphicsContext(success);
}

#if !USE(THREADED_COMPOSITING)
void CCLayerTreeHost::scheduleComposite()
{
    m_client->scheduleComposite();
}
#endif

// Temporary hack until WebViewImpl context creation gets simplified
GraphicsContext3D* CCLayerTreeHost::context()
{
    ASSERT(!m_settings.enableCompositorThread);
    return m_proxy->context();
}

bool CCLayerTreeHost::compositeAndReadback(void *pixels, const IntRect& rect)
{
    return m_proxy->compositeAndReadback(pixels, rect);
}

void CCLayerTreeHost::finishAllRendering()
{
    m_proxy->finishAllRendering();
}

void CCLayerTreeHost::invalidateRootLayerRect(const IntRect& dirtyRect)
{
    m_nonCompositedContentHost->invalidateRect(dirtyRect);
}

const LayerRendererCapabilities& CCLayerTreeHost::layerRendererCapabilities() const
{
    return m_proxy->layerRendererCapabilities();
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
    if (layer)
        m_nonCompositedContentHost->graphicsLayer()->addChild(layer);
}

void CCLayerTreeHost::setViewport(const IntSize& viewportSize, const IntSize& contentsSize, const IntPoint& scrollPosition)
{
    bool visibleRectChanged = m_viewportSize != viewportSize;

    m_viewportSize = viewportSize;
    m_nonCompositedContentHost->setScrollPosition(scrollPosition);
    m_nonCompositedContentHost->graphicsLayer()->setSize(contentsSize);

    if (visibleRectChanged)
        m_nonCompositedContentHost->invalidateEntireLayer();

    setNeedsCommitAndRedraw();
}

void CCLayerTreeHost::setVisible(bool visible)
{
    m_visible = visible;
    m_proxy->setNeedsCommitAndRedraw();
}

void CCLayerTreeHost::loseCompositorContext(int numTimes)
{
    m_proxy->loseCompositorContext(numTimes);
}

TextureManager* CCLayerTreeHost::contentsTextureManager() const
{
    return m_proxy->contentsTextureManager();
}

#if !USE(THREADED_COMPOSITING)
void CCLayerTreeHost::composite()
{
    ASSERT(!m_settings.enableCompositorThread);
    static_cast<CCSingleThreadProxy*>(m_proxy.get())->compositeImmediately();
}
#endif // !USE(THREADED_COMPOSITING)

}
