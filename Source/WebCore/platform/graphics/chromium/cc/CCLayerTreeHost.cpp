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
#include "Region.h"
#include "TraceEvent.h"
#include "TreeSynchronizer.h"
#include "cc/CCLayerAnimationController.h"
#include "cc/CCLayerIterator.h"
#include "cc/CCLayerTreeHostCommon.h"
#include "cc/CCLayerTreeHostImpl.h"
#include "cc/CCOcclusionTracker.h"
#include "cc/CCSingleThreadProxy.h"
#include "cc/CCThread.h"
#include "cc/CCThreadProxy.h"

using namespace std;

namespace {
static int numLayerTreeInstances;
}

namespace WebCore {

bool CCLayerTreeHost::s_needsFilterContext = false;

bool CCLayerTreeHost::anyLayerTreeHostInstanceExists()
{
    return numLayerTreeInstances > 0;
}

PassOwnPtr<CCLayerTreeHost> CCLayerTreeHost::create(CCLayerTreeHostClient* client, const CCSettings& settings)
{
    OwnPtr<CCLayerTreeHost> layerTreeHost = adoptPtr(new CCLayerTreeHost(client, settings));
    if (!layerTreeHost->initialize())
        return nullptr;
    return layerTreeHost.release();
}

CCLayerTreeHost::CCLayerTreeHost(CCLayerTreeHostClient* client, const CCSettings& settings)
    : m_compositorIdentifier(-1)
    , m_animating(false)
    , m_needsAnimateLayers(false)
    , m_client(client)
    , m_frameNumber(0)
    , m_layerRendererInitialized(false)
    , m_contextLost(false)
    , m_numTimesRecreateShouldFail(0)
    , m_numFailedRecreateAttempts(0)
    , m_settings(settings)
    , m_visible(true)
    , m_pageScaleFactor(1)
    , m_minPageScaleFactor(1)
    , m_maxPageScaleFactor(1)
    , m_triggerIdlePaints(true)
    , m_partialTextureUpdateRequests(0)
{
    ASSERT(CCProxy::isMainThread());
    numLayerTreeInstances++;
}

bool CCLayerTreeHost::initialize()
{
    TRACE_EVENT("CCLayerTreeHost::initialize", this, 0);
    if (CCProxy::hasImplThread())
        m_proxy = CCThreadProxy::create(this);
    else
        m_proxy = CCSingleThreadProxy::create(this);
    m_proxy->start();

    if (!m_proxy->initializeContext())
        return false;

    m_compositorIdentifier = m_proxy->compositorIdentifier();
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

void CCLayerTreeHost::initializeLayerRenderer()
{
    TRACE_EVENT("CCLayerTreeHost::initializeLayerRenderer", this, 0);
    if (!m_proxy->initializeLayerRenderer()) {
        // Uh oh, better tell the client that we can't do anything with this context.
        m_client->didRecreateContext(false);
        return;
    }

    // Update m_settings based on capabilities that we got back from the renderer.
    m_settings.acceleratePainting = m_proxy->layerRendererCapabilities().usingAcceleratedPainting;

    // Update m_settings based on partial update capability.
    m_settings.maxPartialTextureUpdates = min(m_settings.maxPartialTextureUpdates, m_proxy->maxPartialTextureUpdates());

    m_contentsTextureManager = TextureManager::create(TextureManager::highLimitBytes(viewportSize()),
                                                      TextureManager::reclaimLimitBytes(viewportSize()),
                                                      m_proxy->layerRendererCapabilities().maxTextureSize);

    m_layerRendererInitialized = true;
}

CCLayerTreeHost::RecreateResult CCLayerTreeHost::recreateContext()
{
    TRACE_EVENT0("cc", "CCLayerTreeHost::recreateContext");
    ASSERT(m_contextLost);

    bool recreated = false;
    if (!m_numTimesRecreateShouldFail)
        recreated = m_proxy->recreateContext();
    else
        m_numTimesRecreateShouldFail--;

    if (recreated) {
        m_client->didRecreateContext(true);
        m_contextLost = false;
        return RecreateSucceeded;
    }

    // Tolerate a certain number of recreation failures to work around races
    // in the context-lost machinery.
    m_numFailedRecreateAttempts++;
    if (m_numFailedRecreateAttempts < 5) {
        // FIXME: The single thread does not self-schedule context
        // recreation. So force another recreation attempt to happen by requesting
        // another commit.
        if (!CCProxy::hasImplThread())
            setNeedsCommit();
        return RecreateFailedButTryAgain;
    }

    // We have tried too many times to recreate the context. Tell the host to fall
    // back to software rendering.
    m_client->didRecreateContext(false);
    return RecreateFailedAndGaveUp;
}

void CCLayerTreeHost::deleteContentsTexturesOnImplThread(TextureAllocator* allocator)
{
    ASSERT(CCProxy::isImplThread());
    if (m_contentsTextureManager)
        m_contentsTextureManager->evictAndDeleteAllTextures(allocator);
}

void CCLayerTreeHost::updateAnimations(double monotonicFrameBeginTime)
{
    m_animating = true;
    m_client->updateAnimations(monotonicFrameBeginTime);
    animateLayers(monotonicFrameBeginTime);
    m_animating = false;
}

void CCLayerTreeHost::layout()
{
    m_client->layout();
}

void CCLayerTreeHost::beginCommitOnImplThread(CCLayerTreeHostImpl* hostImpl)
{
    ASSERT(CCProxy::isImplThread());
    TRACE_EVENT("CCLayerTreeHost::commitTo", this, 0);

    m_contentsTextureManager->reduceMemoryToLimit(TextureManager::reclaimLimitBytes(viewportSize()));
    m_contentsTextureManager->deleteEvictedTextures(hostImpl->contentsTextureAllocator());
}

// This function commits the CCLayerTreeHost to an impl tree. When modifying
// this function, keep in mind that the function *runs* on the impl thread! Any
// code that is logically a main thread operation, e.g. deletion of a LayerChromium,
// should be delayed until the CCLayerTreeHost::commitComplete, which will run
// after the commit, but on the main thread.
void CCLayerTreeHost::finishCommitOnImplThread(CCLayerTreeHostImpl* hostImpl)
{
    ASSERT(CCProxy::isImplThread());

    hostImpl->setRootLayer(TreeSynchronizer::synchronizeTrees(rootLayer(), hostImpl->releaseRootLayer()));

    // We may have added an animation during the tree sync. This will cause both layer tree hosts
    // to visit their controllers.
    if (rootLayer()) {
        hostImpl->setNeedsAnimateLayers();
        m_needsAnimateLayers = true;
    }

    hostImpl->setSourceFrameNumber(frameNumber());
    hostImpl->setViewportSize(viewportSize());
    hostImpl->setPageScaleFactorAndLimits(m_pageScaleFactor, m_minPageScaleFactor, m_maxPageScaleFactor);

    m_frameNumber++;
}

void CCLayerTreeHost::commitComplete()
{
    m_deleteTextureAfterCommitList.clear();
    clearPendingUpdate();
    m_contentsTextureManager->unprotectAllTextures();
    m_client->didCommit();
}

PassRefPtr<GraphicsContext3D> CCLayerTreeHost::createContext()
{
    return m_client->createContext();
}

PassOwnPtr<CCLayerTreeHostImpl> CCLayerTreeHost::createLayerTreeHostImpl(CCLayerTreeHostImplClient* client)
{
    return CCLayerTreeHostImpl::create(m_settings, client);
}

void CCLayerTreeHost::didLoseContext()
{
    TRACE_EVENT("CCLayerTreeHost::didLoseContext", 0, this);
    ASSERT(CCProxy::isMainThread());
    m_contextLost = true;
    m_numFailedRecreateAttempts = 0;
    setNeedsCommit();
}

// Temporary hack until WebViewImpl context creation gets simplified
GraphicsContext3D* CCLayerTreeHost::context()
{
    return m_proxy->context();
}

bool CCLayerTreeHost::compositeAndReadback(void *pixels, const IntRect& rect)
{
    if (!m_layerRendererInitialized) {
        initializeLayerRenderer();
        if (!m_layerRendererInitialized)
            return false;
    }
    if (m_contextLost) {
        if (recreateContext() != RecreateSucceeded)
            return false;
    }
    m_triggerIdlePaints = false;
    bool ret = m_proxy->compositeAndReadback(pixels, rect);
    m_triggerIdlePaints = true;
    return ret;
}

void CCLayerTreeHost::finishAllRendering()
{
    if (!m_layerRendererInitialized)
        return;
    m_proxy->finishAllRendering();
}

const LayerRendererCapabilities& CCLayerTreeHost::layerRendererCapabilities() const
{
    return m_proxy->layerRendererCapabilities();
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
    m_proxy->setNeedsRedraw();
    if (!CCThreadProxy::implThread())
        m_client->scheduleComposite();
}

bool CCLayerTreeHost::commitRequested() const
{
    return m_proxy->commitRequested();
}

void CCLayerTreeHost::setAnimationEvents(PassOwnPtr<CCAnimationEventsVector> events, double wallClockTime)
{
    ASSERT(CCThreadProxy::isMainThread());
    setAnimationEventsRecursive(*events, m_rootLayer.get(), wallClockTime);
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

void CCLayerTreeHost::setViewportSize(const IntSize& viewportSize)
{
    if (viewportSize == m_viewportSize)
        return;

    if (m_contentsTextureManager) {
        m_contentsTextureManager->setMaxMemoryLimitBytes(TextureManager::highLimitBytes(viewportSize));
        m_contentsTextureManager->setPreferredMemoryLimitBytes(TextureManager::reclaimLimitBytes(viewportSize));
    }
    m_viewportSize = viewportSize;
    setNeedsCommit();
}

void CCLayerTreeHost::setPageScaleFactorAndLimits(float pageScaleFactor, float minPageScaleFactor, float maxPageScaleFactor)
{
    if (pageScaleFactor == m_pageScaleFactor && minPageScaleFactor == m_minPageScaleFactor && maxPageScaleFactor == m_maxPageScaleFactor)
        return;

    m_pageScaleFactor = pageScaleFactor;
    m_minPageScaleFactor = minPageScaleFactor;
    m_maxPageScaleFactor = maxPageScaleFactor;
    setNeedsCommit();
}

void CCLayerTreeHost::setVisible(bool visible)
{
    if (m_visible == visible)
        return;

    m_visible = visible;

    if (!visible && m_layerRendererInitialized) {
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
    if (!m_layerRendererInitialized)
        return;

    if (m_proxy->layerRendererCapabilities().contextHasCachedFrontBuffer)
        contentsTextureManager()->evictAndDeleteAllTextures(hostImpl->contentsTextureAllocator());
    else {
        contentsTextureManager()->reduceMemoryToLimit(TextureManager::reclaimLimitBytes(viewportSize()));
        contentsTextureManager()->deleteEvictedTextures(hostImpl->contentsTextureAllocator());
    }

    hostImpl->setRootLayer(TreeSynchronizer::synchronizeTrees(rootLayer(), hostImpl->releaseRootLayer()));

    // We may have added an animation during the tree sync. This will cause both layer tree hosts
    // to visit their controllers.
    if (rootLayer()) {
        hostImpl->setNeedsAnimateLayers();
        m_needsAnimateLayers = true;
    }
}

void CCLayerTreeHost::startPageScaleAnimation(const IntSize& targetPosition, bool useAnchor, float scale, double durationSec)
{
    m_proxy->startPageScaleAnimation(targetPosition, useAnchor, scale, durationSec);
}

void CCLayerTreeHost::loseContext(int numTimes)
{
    TRACE_EVENT1("cc", "CCLayerTreeHost::loseCompositorContext", "numTimes", numTimes);
    m_numTimesRecreateShouldFail = numTimes - 1;
    m_proxy->loseContext();
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

bool CCLayerTreeHost::updateLayers()
{
    if (!m_layerRendererInitialized) {
        initializeLayerRenderer();
        // If we couldn't initialize, then bail since we're returning to software mode.
        if (!m_layerRendererInitialized)
            return false;
    }
    if (m_contextLost) {
        if (recreateContext() != RecreateSucceeded)
            return false;
    }

    if (!rootLayer())
        return true;

    if (viewportSize().isEmpty())
        return true;

    updateLayers(rootLayer());
    return true;
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

    // Reset partial texture update requests.
    m_partialTextureUpdateRequests = 0;

    reserveTextures();

    paintLayerContents(m_updateList, PaintVisible);
    if (!m_triggerIdlePaints)
        return;

    size_t preferredLimitBytes = TextureManager::reclaimLimitBytes(m_viewportSize);
    size_t maxLimitBytes = TextureManager::highLimitBytes(m_viewportSize);
    m_contentsTextureManager->reduceMemoryToLimit(preferredLimitBytes);
    if (m_contentsTextureManager->currentMemoryUseBytes() >= preferredLimitBytes)
        return;

    // Idle painting should fail when we hit the preferred memory limit,
    // otherwise it will always push us towards the maximum limit.
    m_contentsTextureManager->setMaxMemoryLimitBytes(preferredLimitBytes);
    // The second (idle) paint will be a no-op in layers where painting already occured above.
    paintLayerContents(m_updateList, PaintIdle);
    m_contentsTextureManager->setMaxMemoryLimitBytes(maxLimitBytes);
}

void CCLayerTreeHost::reserveTextures()
{
    // Use BackToFront since it's cheap and this isn't order-dependent.
    typedef CCLayerIterator<LayerChromium, Vector<RefPtr<LayerChromium> >, RenderSurfaceChromium, CCLayerIteratorActions::BackToFront> CCLayerIteratorType;

    CCLayerIteratorType end = CCLayerIteratorType::end(&m_updateList);
    for (CCLayerIteratorType it = CCLayerIteratorType::begin(&m_updateList); it != end; ++it) {
        if (!it.representsItself() || !it->alwaysReserveTextures())
            continue;
        it->reserveTextures();
    }
}

// static
void CCLayerTreeHost::paintContentsIfDirty(LayerChromium* layer, PaintType paintType, const CCOcclusionTracker* occlusion)
{
    ASSERT(layer);
    ASSERT(PaintVisible == paintType || PaintIdle == paintType);
    if (PaintVisible == paintType)
        layer->paintContentsIfDirty(occlusion);
    else
        layer->idlePaintContentsIfDirty(occlusion);
}

void CCLayerTreeHost::paintMasksForRenderSurface(LayerChromium* renderSurfaceLayer, PaintType paintType)
{
    // Note: Masks and replicas only exist for layers that own render surfaces. If we reach this point
    // in code, we already know that at least something will be drawn into this render surface, so the
    // mask and replica should be painted.

    LayerChromium* maskLayer = renderSurfaceLayer->maskLayer();
    if (maskLayer) {
        maskLayer->setVisibleLayerRect(IntRect(IntPoint(), renderSurfaceLayer->contentBounds()));
        paintContentsIfDirty(maskLayer, paintType, 0);
    }

    LayerChromium* replicaMaskLayer = renderSurfaceLayer->replicaLayer() ? renderSurfaceLayer->replicaLayer()->maskLayer() : 0;
    if (replicaMaskLayer) {
        replicaMaskLayer->setVisibleLayerRect(IntRect(IntPoint(), renderSurfaceLayer->contentBounds()));
        paintContentsIfDirty(replicaMaskLayer, paintType, 0);
    }
}

void CCLayerTreeHost::paintLayerContents(const LayerList& renderSurfaceLayerList, PaintType paintType)
{
    // Use FrontToBack to allow for testing occlusion and performing culling during the tree walk.
    typedef CCLayerIterator<LayerChromium, Vector<RefPtr<LayerChromium> >, RenderSurfaceChromium, CCLayerIteratorActions::FrontToBack> CCLayerIteratorType;

    bool recordMetricsForFrame = true; // FIXME: In the future, disable this when about:tracing is off.
    CCOcclusionTracker occlusionTracker(IntRect(IntPoint(), viewportSize()), recordMetricsForFrame);
    occlusionTracker.setUsePaintTracking(true); // FIXME: Remove this after m19 branch.

    CCLayerIteratorType end = CCLayerIteratorType::end(&renderSurfaceLayerList);
    for (CCLayerIteratorType it = CCLayerIteratorType::begin(&renderSurfaceLayerList); it != end; ++it) {
        if (it.representsTargetRenderSurface()) {
            ASSERT(it->renderSurface()->drawOpacity() || it->renderSurface()->drawOpacityIsAnimating());

            occlusionTracker.finishedTargetRenderSurface(*it, it->renderSurface());
            paintMasksForRenderSurface(*it, paintType);
        } else if (it.representsItself()) {
            ASSERT(!it->bounds().isEmpty());

            occlusionTracker.enterTargetRenderSurface(it->targetRenderSurface());
            paintContentsIfDirty(*it, paintType, &occlusionTracker);
            occlusionTracker.markOccludedBehindLayer(*it);
        } else
            occlusionTracker.leaveToTargetRenderSurface(it.targetRenderSurfaceLayer()->renderSurface());
    }

    occlusionTracker.overdrawMetrics().recordMetrics(this);
}

void CCLayerTreeHost::updateCompositorResources(GraphicsContext3D* context, CCTextureUpdater& updater)
{
    // Use BackToFront since it's cheap and this isn't order-dependent.
    typedef CCLayerIterator<LayerChromium, Vector<RefPtr<LayerChromium> >, RenderSurfaceChromium, CCLayerIteratorActions::BackToFront> CCLayerIteratorType;

    CCLayerIteratorType end = CCLayerIteratorType::end(&m_updateList);
    for (CCLayerIteratorType it = CCLayerIteratorType::begin(&m_updateList); it != end; ++it) {
        if (it.representsTargetRenderSurface()) {
            ASSERT(it->renderSurface()->drawOpacity() || it->renderSurface()->drawOpacityIsAnimating());
            if (it->maskLayer())
                it->maskLayer()->updateCompositorResources(context, updater);

            if (it->replicaLayer()) {
                it->replicaLayer()->updateCompositorResources(context, updater);
                if (it->replicaLayer()->maskLayer())
                    it->replicaLayer()->maskLayer()->updateCompositorResources(context, updater);
            }
        } else if (it.representsItself())
            it->updateCompositorResources(context, updater);
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
    if (m_animating)
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

bool CCLayerTreeHost::requestPartialTextureUpdate()
{
    if (m_partialTextureUpdateRequests >= m_settings.maxPartialTextureUpdates)
        return false;

    m_partialTextureUpdateRequests++;
    return true;
}

void CCLayerTreeHost::deleteTextureAfterCommit(PassOwnPtr<ManagedTexture> texture)
{
    m_deleteTextureAfterCommitList.append(texture);
}

void CCLayerTreeHost::animateLayers(double monotonicTime)
{
    if (!m_settings.threadedAnimationEnabled || !m_needsAnimateLayers)
        return;

    TRACE_EVENT("CCLayerTreeHostImpl::animateLayers", this, 0);
    m_needsAnimateLayers = animateLayersRecursive(m_rootLayer.get(), monotonicTime);
}

bool CCLayerTreeHost::animateLayersRecursive(LayerChromium* current, double monotonicTime)
{
    if (!current)
        return false;

    bool subtreeNeedsAnimateLayers = false;
    CCLayerAnimationController* currentController = current->layerAnimationController();
    currentController->animate(monotonicTime, 0);

    // If the current controller still has an active animation, we must continue animating layers.
    if (currentController->hasActiveAnimation())
         subtreeNeedsAnimateLayers = true;

    for (size_t i = 0; i < current->children().size(); ++i) {
        if (animateLayersRecursive(current->children()[i].get(), monotonicTime))
            subtreeNeedsAnimateLayers = true;
    }

    return subtreeNeedsAnimateLayers;
}

void CCLayerTreeHost::setAnimationEventsRecursive(const CCAnimationEventsVector& events, LayerChromium* layer, double wallClockTime)
{
    if (!layer)
        return;

    for (size_t eventIndex = 0; eventIndex < events.size(); ++eventIndex) {
        if (layer->id() == events[eventIndex].layerId)
            layer->notifyAnimationStarted(events[eventIndex], wallClockTime);
    }

    for (size_t childIndex = 0; childIndex < layer->children().size(); ++childIndex)
        setAnimationEventsRecursive(events, layer->children()[childIndex].get(), wallClockTime);
}

} // namespace WebCore
