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

namespace {
static int numLayerTreeInstances;
}

namespace WebCore {

bool CCLayerTreeHost::anyLayerTreeHostInstanceExists()
{
    return numLayerTreeInstances > 0;
}

PassRefPtr<CCLayerTreeHost> CCLayerTreeHost::create(CCLayerTreeHostClient* client, const CCSettings& settings)
{
    RefPtr<CCLayerTreeHost> layerTreeHost = adoptRef(new CCLayerTreeHost(client, settings));
    if (!layerTreeHost->initialize())
        return 0;
    return layerTreeHost;
}

CCLayerTreeHost::CCLayerTreeHost(CCLayerTreeHostClient* client, const CCSettings& settings)
    : m_compositorIdentifier(-1)
    , m_animating(false)
    , m_client(client)
    , m_frameNumber(0)
    , m_settings(settings)
    , m_visible(true)
    , m_haveWheelEventHandlers(false)
    , m_pageScale(1)
    , m_minPageScale(1)
    , m_maxPageScale(1)
    , m_triggerIdlePaints(true)
{
    ASSERT(CCProxy::isMainThread());
    numLayerTreeInstances++;
}

bool CCLayerTreeHost::initialize()
{
    TRACE_EVENT("CCLayerTreeHost::initialize", this, 0);
    if (CCProxy::hasImplThread()) {
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

    m_contentsTextureManager = TextureManager::create(TextureManager::highLimitBytes(viewportSize()),
                                                      TextureManager::reclaimLimitBytes(viewportSize()),
                                                      m_proxy->layerRendererCapabilities().maxTextureSize);
    return true;
}

CCLayerTreeHost::~CCLayerTreeHost()
{
    ASSERT(CCProxy::isMainThread());
    TRACE_EVENT("CCLayerTreeHost::~CCLayerTreeHost", this, 0);
    ASSERT(m_proxy);
    m_proxy->stop();
    m_proxy.clear();
    clearPendingUpdate();
    numLayerTreeInstances--;
}

void CCLayerTreeHost::deleteContentsTexturesOnImplThread(TextureAllocator* allocator)
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

void CCLayerTreeHost::beginCommitOnImplThread(CCLayerTreeHostImpl* hostImpl)
{
    ASSERT(CCProxy::isImplThread());
    TRACE_EVENT("CCLayerTreeHost::commitTo", this, 0);

    contentsTextureManager()->reduceMemoryToLimit(TextureManager::reclaimLimitBytes(viewportSize()));
    contentsTextureManager()->deleteEvictedTextures(hostImpl->contentsTextureAllocator());
}

// This function commits the CCLayerTreeHost to an impl tree. When modifying
// this function, keep in mind that the function *runs* on the impl thread! Any
// code that is logically a main thread operation, e.g. deletion of a LayerChromium,
// should be delayed until the CCLayerTreeHost::commitComplete, which will run
// after the commit, but on the main thread.
void CCLayerTreeHost::finishCommitOnImplThread(CCLayerTreeHostImpl* hostImpl)
{
    ASSERT(CCProxy::isImplThread());

    // Synchronize trees, if one exists at all...
    if (rootLayer())
        hostImpl->setRootLayer(TreeSynchronizer::synchronizeTrees(rootLayer(), hostImpl->rootLayer()));
    else
        hostImpl->setRootLayer(0);

    hostImpl->setSourceFrameNumber(frameNumber());
    hostImpl->setHaveWheelEventHandlers(m_haveWheelEventHandlers);
    hostImpl->setZoomAnimatorTransform(m_zoomAnimatorTransform);
    hostImpl->setViewport(viewportSize());
    hostImpl->setPageScaleFactorAndLimits(pageScale(), m_minPageScale, m_maxPageScale);

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

PassOwnPtr<CCLayerTreeHostImpl> CCLayerTreeHost::createLayerTreeHostImpl(CCLayerTreeHostImplClient* client)
{
    return CCLayerTreeHostImpl::create(m_settings, client);
}

void CCLayerTreeHost::didRecreateGraphicsContext(bool success)
{
    if (m_rootLayer) {
        m_rootLayer->setLayerTreeHost(0);
        m_rootLayer->setLayerTreeHost(this);
    }
    m_client->didRecreateGraphicsContext(success);
}

// Temporary hack until WebViewImpl context creation gets simplified
GraphicsContext3D* CCLayerTreeHost::context()
{
    ASSERT(!CCProxy::hasImplThread());
    return m_proxy->context();
}

bool CCLayerTreeHost::compositeAndReadback(void *pixels, const IntRect& rect)
{
    m_triggerIdlePaints = false;
    bool ret = m_proxy->compositeAndReadback(pixels, rect);
    m_triggerIdlePaints = true;
    return ret;
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
        setNeedsCommit();
}

void CCLayerTreeHost::setNeedsAnimate()
{
    ASSERT(CCProxy::hasImplThread());
    m_proxy->setNeedsAnimate();
}

void CCLayerTreeHost::setNeedsCommit()
{
    if (CCThreadProxy::implThread())
        m_proxy->setNeedsCommit();
    else
        m_client->scheduleComposite();
}

void CCLayerTreeHost::setNeedsRedraw()
{
    if (CCThreadProxy::implThread())
        m_proxy->setNeedsRedraw();
    else
        m_client->scheduleComposite();
}

void CCLayerTreeHost::setRootLayer(PassRefPtr<LayerChromium> rootLayer)
{
    if (m_rootLayer == rootLayer)
        return;

    if (m_rootLayer)
        m_rootLayer->setLayerTreeHost(0);
    m_rootLayer = rootLayer;
    if (m_rootLayer)
        m_rootLayer->setLayerTreeHost(this);
    setNeedsCommit();
}

void CCLayerTreeHost::setViewport(const IntSize& viewportSize)
{
    contentsTextureManager()->setMaxMemoryLimitBytes(TextureManager::highLimitBytes(viewportSize));
    contentsTextureManager()->setPreferredMemoryLimitBytes(TextureManager::reclaimLimitBytes(viewportSize));
    m_viewportSize = viewportSize;
    setNeedsCommit();
}

void CCLayerTreeHost::setPageScale(float pageScale)
{
    if (pageScale == m_pageScale)
        return;

    m_pageScale = pageScale;
    setNeedsCommit();
}

void CCLayerTreeHost::setPageScaleFactorLimits(float minScale, float maxScale)
{
    if (minScale == m_minPageScale && maxScale == m_maxPageScale)
        return;

    m_minPageScale = minScale;
    m_maxPageScale = maxScale;
    setNeedsCommit();
}

void CCLayerTreeHost::setVisible(bool visible)
{
    if (m_visible == visible)
        return;

    m_visible = visible;
    if (!visible) {
        m_contentsTextureManager->reduceMemoryToLimit(TextureManager::lowLimitBytes(viewportSize()));
        m_contentsTextureManager->unprotectAllTextures();
    }

    // Tells the proxy that visibility state has changed. This will in turn call
    // CCLayerTreeHost::didBecomeInvisibleOnImplThread on the appropriate thread, for
    // the case where !visible.
    m_proxy->setVisible(visible);
}

void CCLayerTreeHost::didBecomeInvisibleOnImplThread(CCLayerTreeHostImpl* hostImpl)
{
    ASSERT(CCProxy::isImplThread());
    if (m_proxy->layerRendererCapabilities().contextHasCachedFrontBuffer)
        contentsTextureManager()->evictAndDeleteAllTextures(hostImpl->contentsTextureAllocator());
    else {
        contentsTextureManager()->reduceMemoryToLimit(TextureManager::reclaimLimitBytes(viewportSize()));
        contentsTextureManager()->deleteEvictedTextures(hostImpl->contentsTextureAllocator());
    }

    // Ensure that the dropped tiles are propagated to the impl tree.
    // If the frontbuffer is cached, then clobber the impl tree. Otherwise,
    // push over the tree changes.
    if (m_proxy->layerRendererCapabilities().contextHasCachedFrontBuffer) {
        hostImpl->setRootLayer(0);
        return;
    }
    if (rootLayer())
        hostImpl->setRootLayer(TreeSynchronizer::synchronizeTrees(rootLayer(), hostImpl->rootLayer()));
    else
        hostImpl->setRootLayer(0);
}

void CCLayerTreeHost::setHaveWheelEventHandlers(bool haveWheelEventHandlers)
{
    if (m_haveWheelEventHandlers == haveWheelEventHandlers)
        return;
    m_haveWheelEventHandlers = haveWheelEventHandlers;
    m_proxy->setNeedsCommit();
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
    ASSERT(!CCThreadProxy::implThread());
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

    IntRect rootClipRect(IntPoint(), viewportSize());
    rootLayer->setClipRect(rootClipRect);

    // This assert fires if updateCompositorResources wasn't called after
    // updateLayers. Only one update can be pending at any given time.
    ASSERT(!m_updateList.size());
    m_updateList.append(rootLayer);

    RenderSurfaceChromium* rootRenderSurface = rootLayer->renderSurface();
    rootRenderSurface->clearLayerList();

    TransformationMatrix identityMatrix;
    {
        TRACE_EVENT("CCLayerTreeHost::updateLayers::calcDrawEtc", this, 0);
        CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(rootLayer, rootLayer, identityMatrix, identityMatrix, m_updateList, rootRenderSurface->layerList(), layerRendererCapabilities().maxTextureSize);
    }

    paintLayerContents(m_updateList, PaintVisible);
    if (!m_triggerIdlePaints)
        return;

    size_t preferredLimitBytes = TextureManager::reclaimLimitBytes(m_viewportSize);
    size_t maxLimitBytes = TextureManager::highLimitBytes(m_viewportSize);
    contentsTextureManager()->reduceMemoryToLimit(preferredLimitBytes);
    if (contentsTextureManager()->currentMemoryUseBytes() >= preferredLimitBytes)
        return;

    // Idle painting should fail when we hit the preferred memory limit,
    // otherwise it will always push us towards the maximum limit.
    m_contentsTextureManager->setMaxMemoryLimitBytes(preferredLimitBytes);
    // The second (idle) paint will be a no-op in layers where painting already occured above.
    paintLayerContents(m_updateList, PaintIdle);
    m_contentsTextureManager->setMaxMemoryLimitBytes(maxLimitBytes);
}

// static
void CCLayerTreeHost::paintContentsIfDirty(LayerChromium* layer, PaintType paintType)
{
    ASSERT(layer);
    ASSERT(PaintVisible == paintType || PaintIdle == paintType);
    if (PaintVisible == paintType)
        layer->paintContentsIfDirty();
    else
        layer->idlePaintContentsIfDirty();
}

void CCLayerTreeHost::paintMaskAndReplicaForRenderSurface(LayerChromium* renderSurfaceLayer, PaintType paintType)
{
    // Note: Masks and replicas only exist for layers that own render surfaces. If we reach this point
    // in code, we already know that at least something will be drawn into this render surface, so the
    // mask and replica should be painted.

    if (renderSurfaceLayer->maskLayer()) {
        renderSurfaceLayer->maskLayer()->setVisibleLayerRect(IntRect(IntPoint(), renderSurfaceLayer->contentBounds()));
        paintContentsIfDirty(renderSurfaceLayer->maskLayer(), paintType);
    }

    LayerChromium* replicaLayer = renderSurfaceLayer->replicaLayer();
    if (replicaLayer) {
        paintContentsIfDirty(replicaLayer, paintType);

        if (replicaLayer->maskLayer()) {
            replicaLayer->maskLayer()->setVisibleLayerRect(IntRect(IntPoint(), replicaLayer->maskLayer()->contentBounds()));
            paintContentsIfDirty(replicaLayer->maskLayer(), paintType);
        }
    }
}

void CCLayerTreeHost::paintLayerContents(const LayerList& renderSurfaceLayerList, PaintType paintType)
{
    for (int surfaceIndex = renderSurfaceLayerList.size() - 1; surfaceIndex >= 0 ; --surfaceIndex) {
        LayerChromium* renderSurfaceLayer = renderSurfaceLayerList[surfaceIndex].get();
        RenderSurfaceChromium* renderSurface = renderSurfaceLayer->renderSurface();
        ASSERT(renderSurface);
        ASSERT(renderSurface->drawOpacity());

        paintMaskAndReplicaForRenderSurface(renderSurfaceLayer, paintType);

        const LayerList& layerList = renderSurface->layerList();
        for (unsigned layerIndex = 0; layerIndex < layerList.size(); ++layerIndex) {
            LayerChromium* layer = layerList[layerIndex].get();

            // Layers that start a new render surface will be painted when the render
            // surface's list is processed.
            if (CCLayerTreeHostCommon::renderSurfaceContributesToTarget<LayerChromium>(layer, renderSurfaceLayer->id()))
                continue;

            ASSERT(!layer->bounds().isEmpty());

            paintContentsIfDirty(layer, paintType);
        }
    }
}

void CCLayerTreeHost::updateCompositorResources(GraphicsContext3D* context, CCTextureUpdater& updater)
{
    for (int surfaceIndex = m_updateList.size() - 1; surfaceIndex >= 0 ; --surfaceIndex) {
        LayerChromium* renderSurfaceLayer = m_updateList[surfaceIndex].get();
        RenderSurfaceChromium* renderSurface = renderSurfaceLayer->renderSurface();
        ASSERT(renderSurface);
        ASSERT(renderSurface->drawOpacity());

        if (renderSurfaceLayer->maskLayer())
            renderSurfaceLayer->maskLayer()->updateCompositorResources(context, updater);

        if (renderSurfaceLayer->replicaLayer()) {
            renderSurfaceLayer->replicaLayer()->updateCompositorResources(context, updater);
            
            if (renderSurfaceLayer->replicaLayer()->maskLayer())
                renderSurfaceLayer->replicaLayer()->maskLayer()->updateCompositorResources(context, updater);
        }
        
        const LayerList& layerList = renderSurface->layerList();
        for (unsigned layerIndex = 0; layerIndex < layerList.size(); ++layerIndex) {
            LayerChromium* layer = layerList[layerIndex].get();

            if (CCLayerTreeHostCommon::renderSurfaceContributesToTarget<LayerChromium>(layer, renderSurfaceLayer->id()))
                continue;

            layer->updateCompositorResources(context, updater);
        }
    }
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

void CCLayerTreeHost::applyScrollAndScale(const CCScrollAndScaleSet& info)
{
    // FIXME: pushing scroll offsets to non-root layers not implemented
    if (!info.scrolls.size())
        return;

    ASSERT(info.scrolls.size() == 1);
    IntSize scrollDelta = info.scrolls[0].scrollDelta;
    m_client->applyScrollAndScale(scrollDelta, info.pageScaleDelta);
}

void CCLayerTreeHost::startRateLimiter(GraphicsContext3D* context)
{
    if (animating())
        return;
    ASSERT(context);
    RateLimiterMap::iterator it = m_rateLimiters.find(context);
    if (it != m_rateLimiters.end())
        it->second->start();
    else {
        RefPtr<RateLimiter> rateLimiter = RateLimiter::create(context);
        m_rateLimiters.set(context, rateLimiter);
        rateLimiter->start();
    }
}

void CCLayerTreeHost::stopRateLimiter(GraphicsContext3D* context)
{
    RateLimiterMap::iterator it = m_rateLimiters.find(context);
    if (it != m_rateLimiters.end()) {
        it->second->stop();
        m_rateLimiters.remove(it);
    }
}

}
