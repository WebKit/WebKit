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
#include "TraceEvent.h"
#include "TreeSynchronizer.h"
#include "cc/CCLayerTreeHostCommon.h"
#include "cc/CCLayerTreeHostImpl.h"
#include "cc/CCSingleThreadProxy.h"
#include "cc/CCThread.h"
#include "cc/CCThreadProxy.h"

namespace WebCore {

PassRefPtr<CCLayerTreeHost> CCLayerTreeHost::create(CCLayerTreeHostClient* client, PassRefPtr<LayerChromium> rootLayer, const CCSettings& settings)
{
    RefPtr<CCLayerTreeHost> layerTreeHost = adoptRef(new CCLayerTreeHost(client, rootLayer, settings));
    if (!layerTreeHost->initialize())
        return 0;
    return layerTreeHost;
}

CCLayerTreeHost::CCLayerTreeHost(CCLayerTreeHostClient* client, PassRefPtr<LayerChromium> rootLayer, const CCSettings& settings)
    : m_compositorIdentifier(-1)
    , m_animating(false)
    , m_client(client)
    , m_frameNumber(0)
    , m_rootLayer(rootLayer)
    , m_settings(settings)
    , m_visible(true)
{
    ASSERT(CCProxy::isMainThread());
}

bool CCLayerTreeHost::initialize()
{
    TRACE_EVENT("CCLayerTreeHost::initialize", this, 0);
    if (m_settings.enableCompositorThread) {
        // Accelerated Painting is not supported in threaded mode. Turn it off.
        m_settings.acceleratePainting = false;
        // The HUD does not work in threaded mode. Turn it off.
        m_settings.showFPSCounter = false;
        m_settings.showPlatformLayerTree = false;

        m_proxy = CCThreadProxy::create(this);
    } else
        m_proxy = CCSingleThreadProxy::create(this);
    m_proxy->start();

    if (!m_proxy->initializeLayerRenderer())
        return false;

    m_compositorIdentifier = m_proxy->compositorIdentifier();

    // Update m_settings based on capabilities that we got back from the renderer.
    m_settings.acceleratePainting = m_proxy->layerRendererCapabilities().usingAcceleratedPainting;

    setNeedsCommitThenRedraw();

    m_contentsTextureManager = TextureManager::create(TextureManager::highLimitBytes(), m_proxy->layerRendererCapabilities().maxTextureSize);
    return true;
}

CCLayerTreeHost::~CCLayerTreeHost()
{
    ASSERT(CCProxy::isMainThread());
    TRACE_EVENT("CCLayerTreeHost::~CCLayerTreeHost", this, 0);
    m_proxy->stop();
    m_proxy.clear();
    clearPendingUpdate();
}

void CCLayerTreeHost::deleteContentsTexturesOnCCThread(TextureAllocator* allocator)
{
    ASSERT(CCProxy::isImplThread());
    if (m_contentsTextureManager)
        m_contentsTextureManager->evictAndDeleteAllTextures(allocator);
}

void CCLayerTreeHost::animateAndLayout(double frameBeginTime)
{
    m_animating = true;
    m_client->animateAndLayout(frameBeginTime);
    m_animating = false;
}

// This function commits the CCLayerTreeHost to an impl tree. When modifying
// this function, keep in mind that the function *runs* on the impl thread! Any
// code that is logically a main thread operation, e.g. deletion of a LayerChromium,
// should be delayed until the CCLayerTreeHost::commitComplete, which will run
// after the commit, but on the main thread.
void CCLayerTreeHost::commitToOnCCThread(CCLayerTreeHostImpl* hostImpl)
{
    ASSERT(CCProxy::isImplThread());
    TRACE_EVENT("CCLayerTreeHost::commitTo", this, 0);
    hostImpl->setSourceFrameNumber(frameNumber());

    contentsTextureManager()->reduceMemoryToLimit(TextureManager::reclaimLimitBytes());
    contentsTextureManager()->deleteEvictedTextures(hostImpl->contentsTextureAllocator());

    updateCompositorResources(m_updateList, hostImpl->context(), hostImpl->contentsTextureAllocator());

    hostImpl->setVisible(m_visible);
    hostImpl->setZoomAnimatorTransform(m_zoomAnimatorTransform);
    hostImpl->setViewport(viewportSize());

    // Synchronize trees, if one exists at all...
    if (rootLayer())
        hostImpl->setRootLayer(TreeSynchronizer::synchronizeTrees(rootLayer(), hostImpl->rootLayer()));
    else
        hostImpl->setRootLayer(0);

    m_frameNumber++;
}

void CCLayerTreeHost::commitComplete()
{
    clearPendingUpdate();
    m_contentsTextureManager->unprotectAllTextures();
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
        rootLayer()->cleanupResourcesRecursive();
    m_client->didRecreateGraphicsContext(success);
}

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

const LayerRendererCapabilities& CCLayerTreeHost::layerRendererCapabilities() const
{
    return m_proxy->layerRendererCapabilities();
}

void CCLayerTreeHost::setZoomAnimatorTransform(const TransformationMatrix& zoom)
{
    bool zoomChanged = m_zoomAnimatorTransform != zoom;

    m_zoomAnimatorTransform = zoom;

    if (zoomChanged)
        setNeedsCommitThenRedraw();
}

void CCLayerTreeHost::setNeedsCommitThenRedraw()
{
    if (m_settings.enableCompositorThread) {
        TRACE_EVENT("CCLayerTreeHost::setNeedsRedraw", this, 0);
        m_proxy->setNeedsCommitThenRedraw();
    } else
        m_client->scheduleComposite();
}

void CCLayerTreeHost::setNeedsRedraw()
{
    if (m_settings.enableCompositorThread)
        m_proxy->setNeedsRedraw();
    else
        m_client->scheduleComposite();
}

void CCLayerTreeHost::setViewport(const IntSize& viewportSize)
{
    m_viewportSize = viewportSize;
    setNeedsCommitThenRedraw();
}

void CCLayerTreeHost::setVisible(bool visible)
{
    m_visible = visible;
    if (visible)
        setNeedsCommitThenRedraw();
    else {
        m_contentsTextureManager->reduceMemoryToLimit(TextureManager::lowLimitBytes());
        m_contentsTextureManager->unprotectAllTextures();
        m_proxy->setNeedsCommit();
    }
}

void CCLayerTreeHost::loseCompositorContext(int numTimes)
{
    m_proxy->loseCompositorContext(numTimes);
}

TextureManager* CCLayerTreeHost::contentsTextureManager() const
{
    return m_contentsTextureManager.get();
}

void CCLayerTreeHost::composite()
{
    ASSERT(!m_settings.enableCompositorThread);
    static_cast<CCSingleThreadProxy*>(m_proxy.get())->compositeImmediately();
}

void CCLayerTreeHost::updateLayers()
{
    if (!rootLayer())
        return;

    if (viewportSize().isEmpty())
        return;

    updateLayers(rootLayer());
}

void CCLayerTreeHost::updateLayers(LayerChromium* rootLayer)
{
    TRACE_EVENT("CCLayerTreeHost::updateLayers", this, 0);

    if (!rootLayer->renderSurface())
        rootLayer->createRenderSurface();
    rootLayer->renderSurface()->setContentRect(IntRect(IntPoint(0, 0), viewportSize()));

    IntRect rootScissorRect(IntPoint(), viewportSize());
    rootLayer->setScissorRect(rootScissorRect);

    // This assert fires if updateCompositorResources wasn't called after
    // updateLayers. Only one update can be pending at any given time.
    ASSERT(!m_updateList.size());
    m_updateList.append(rootLayer);

    RenderSurfaceChromium* rootRenderSurface = rootLayer->renderSurface();
    rootRenderSurface->clearLayerList();

    {
        TRACE_EVENT("CCLayerTreeHost::updateLayers::calcDrawEtc", this, 0);
        CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(rootLayer, rootLayer, m_zoomAnimatorTransform, m_zoomAnimatorTransform, m_updateList, rootRenderSurface->layerList(), layerRendererCapabilities().maxTextureSize);
    }

    paintLayerContents(m_updateList);
}

static void paintContentsIfDirty(LayerChromium* layer, const IntRect& visibleLayerRect)
{
    if (layer->drawsContent()) {
        layer->setVisibleLayerRect(visibleLayerRect);
        layer->paintContentsIfDirty();
    }
}

void CCLayerTreeHost::paintLayerContents(const LayerList& renderSurfaceLayerList)
{
    for (int surfaceIndex = renderSurfaceLayerList.size() - 1; surfaceIndex >= 0 ; --surfaceIndex) {
        LayerChromium* renderSurfaceLayer = renderSurfaceLayerList[surfaceIndex].get();
        RenderSurfaceChromium* renderSurface = renderSurfaceLayer->renderSurface();
        ASSERT(renderSurface);

        renderSurfaceLayer->setLayerTreeHost(this);

        // Render surfaces whose drawable area has zero width or height
        // will have no layers associated with them and should be skipped.
        if (!renderSurface->layerList().size())
            continue;

        if (!renderSurface->drawOpacity())
            continue;

        const LayerList& layerList = renderSurface->layerList();
        ASSERT(layerList.size());
        for (unsigned layerIndex = 0; layerIndex < layerList.size(); ++layerIndex) {
            LayerChromium* layer = layerList[layerIndex].get();

            // Layers that start a new render surface will be painted when the render
            // surface's list is processed.
            if (layer->renderSurface() && layer->renderSurface() != renderSurface)
                continue;

            layer->setLayerTreeHost(this);

            if (!layer->opacity())
                continue;

            if (layer->maskLayer())
                layer->maskLayer()->setLayerTreeHost(this);
            if (layer->replicaLayer()) {
                layer->replicaLayer()->setLayerTreeHost(this);
                if (layer->replicaLayer()->maskLayer())
                    layer->replicaLayer()->maskLayer()->setLayerTreeHost(this);
            }

            if (layer->bounds().isEmpty())
                continue;

            IntRect defaultContentRect = IntRect(rootLayer()->scrollPosition(), viewportSize());

            IntRect targetSurfaceRect = layer->targetRenderSurface() ? layer->targetRenderSurface()->contentRect() : defaultContentRect;
            if (layer->usesLayerScissor())
                targetSurfaceRect.intersect(layer->scissorRect());
            IntRect visibleLayerRect = CCLayerTreeHostCommon::calculateVisibleLayerRect(targetSurfaceRect, layer->bounds(), layer->contentBounds(), layer->drawTransform());

            visibleLayerRect.move(toSize(layer->scrollPosition()));
            paintContentsIfDirty(layer, visibleLayerRect);

            if (LayerChromium* maskLayer = layer->maskLayer())
                paintContentsIfDirty(maskLayer, IntRect(IntPoint(), maskLayer->contentBounds()));

            if (LayerChromium* replicaLayer = layer->replicaLayer()) {
                paintContentsIfDirty(replicaLayer, visibleLayerRect);

                if (LayerChromium* replicaMaskLayer = replicaLayer->maskLayer())
                    paintContentsIfDirty(replicaMaskLayer, IntRect(IntPoint(), replicaMaskLayer->contentBounds()));
            }
        }
    }
}

void CCLayerTreeHost::updateCompositorResources(const LayerList& renderSurfaceLayerList, GraphicsContext3D* context, TextureAllocator* allocator)
{
    for (int surfaceIndex = renderSurfaceLayerList.size() - 1; surfaceIndex >= 0 ; --surfaceIndex) {
        LayerChromium* renderSurfaceLayer = renderSurfaceLayerList[surfaceIndex].get();
        RenderSurfaceChromium* renderSurface = renderSurfaceLayer->renderSurface();
        ASSERT(renderSurface);

        if (!renderSurface->layerList().size() || !renderSurface->drawOpacity())
            continue;

        const LayerList& layerList = renderSurface->layerList();
        ASSERT(layerList.size());
        for (unsigned layerIndex = 0; layerIndex < layerList.size(); ++layerIndex) {
            LayerChromium* layer = layerList[layerIndex].get();
            if (layer->renderSurface() && layer->renderSurface() != renderSurface)
                continue;

            updateCompositorResources(layer, context, allocator);
        }
    }
}

void CCLayerTreeHost::updateCompositorResources(LayerChromium* layer, GraphicsContext3D* context, TextureAllocator* allocator)
{
    if (layer->bounds().isEmpty())
        return;

    if (!layer->opacity())
        return;

    if (layer->maskLayer())
        updateCompositorResources(layer->maskLayer(), context, allocator);
    if (layer->replicaLayer())
        updateCompositorResources(layer->replicaLayer(), context, allocator);

    if (layer->drawsContent())
        layer->updateCompositorResources(context, allocator);
}

void CCLayerTreeHost::clearPendingUpdate()
{
    for (size_t surfaceIndex = 0; surfaceIndex < m_updateList.size(); ++surfaceIndex) {
        LayerChromium* layer = m_updateList[surfaceIndex].get();
        ASSERT(layer->renderSurface());
        layer->clearRenderSurface();
    }
    m_updateList.clear();
}

void CCLayerTreeHost::applyScrollDeltas(const CCScrollUpdateSet& info)
{
    for (size_t i = 0; i < info.size(); ++i) {
        int layerId = info[i].layerId;
        IntSize scrollDelta = info[i].scrollDelta;

        // FIXME: pushing scroll offsets to non-root layers not implemented
        if (rootLayer()->id() == layerId)
            m_client->applyScrollDelta(scrollDelta);
        else
            ASSERT_NOT_REACHED();
    }
}

}
