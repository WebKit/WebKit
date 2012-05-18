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
#include "cc/CCFontAtlas.h"
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
    , m_backgroundColor(Color::white)
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

    // Only allocate the font atlas if we have reason to use the heads-up display.
    if (m_settings.showFPSCounter || m_settings.showPlatformLayerTree) {
        TRACE_EVENT0("cc", "CCLayerTreeHost::initialize::initializeFontAtlas");
        OwnPtr<CCFontAtlas> fontAtlas(CCFontAtlas::create());
        fontAtlas->initialize();
        m_proxy->setFontAtlas(fontAtlas.release());
    }

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
    numLayerTreeInstances--;
}

void CCLayerTreeHost::setSurfaceReady()
{
    m_proxy->setSurfaceReady();
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

    m_contentsTextureManager = TextureManager::create(TextureManager::highLimitBytes(deviceViewportSize()),
                                                      TextureManager::reclaimLimitBytes(deviceViewportSize()),
                                                      m_proxy->layerRendererCapabilities().maxTextureSize);

    m_layerRendererInitialized = true;

    m_settings.defaultTileSize = IntSize(min(m_settings.defaultTileSize.width(), m_proxy->layerRendererCapabilities().maxTextureSize),
                                         min(m_settings.defaultTileSize.height(), m_proxy->layerRendererCapabilities().maxTextureSize));
    m_settings.maxUntiledLayerSize = IntSize(min(m_settings.maxUntiledLayerSize.width(), m_proxy->layerRendererCapabilities().maxTextureSize),
                                             min(m_settings.maxUntiledLayerSize.height(), m_proxy->layerRendererCapabilities().maxTextureSize));
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
    if (m_layerRendererInitialized)
        m_contentsTextureManager->evictAndDeleteAllTextures(allocator);
}

void CCLayerTreeHost::acquireLayerTextures()
{
    ASSERT(CCProxy::isMainThread());
    m_proxy->acquireLayerTextures();
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

    m_contentsTextureManager->reduceMemoryToLimit(m_contentsTextureManager->preferredMemoryLimitBytes());
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
    if (rootLayer() && m_needsAnimateLayers)
        hostImpl->setNeedsAnimateLayers();

    hostImpl->setSourceFrameNumber(frameNumber());
    hostImpl->setViewportSize(viewportSize());
    hostImpl->setPageScaleFactorAndLimits(m_pageScaleFactor, m_minPageScaleFactor, m_maxPageScaleFactor);
    hostImpl->setBackgroundColor(m_backgroundColor);

    m_frameNumber++;
}

void CCLayerTreeHost::commitComplete()
{
    m_deleteTextureAfterCommitList.clear();
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

void CCLayerTreeHost::didAddAnimation()
{
    m_needsAnimateLayers = true;
    m_proxy->didAddAnimation();
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

    m_viewportSize = viewportSize;

    m_deviceViewportSize = viewportSize;
    m_deviceViewportSize.scale(m_settings.deviceScaleFactor);

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

    if (m_proxy->layerRendererCapabilities().contextHasCachedFrontBuffer) {
        // Unprotect and delete all textures.
        m_contentsTextureManager->unprotectAllTextures();
        m_contentsTextureManager->reduceMemoryToLimit(0);
    } else {
        // Delete all unprotected textures, and only save textures that fit in the preferred memory limit.
        m_contentsTextureManager->reduceMemoryToLimit(0);
        m_contentsTextureManager->unprotectAllTextures();
        m_contentsTextureManager->reduceMemoryToLimit(m_contentsTextureManager->preferredMemoryLimitBytes());
    }
    m_contentsTextureManager->deleteEvictedTextures(hostImpl->contentsTextureAllocator());

    hostImpl->setRootLayer(TreeSynchronizer::synchronizeTrees(rootLayer(), hostImpl->releaseRootLayer()));

    // We may have added an animation during the tree sync. This will cause both layer tree hosts
    // to visit their controllers.
    if (rootLayer() && m_needsAnimateLayers)
        hostImpl->setNeedsAnimateLayers();
}

void CCLayerTreeHost::setContentsMemoryAllocationLimitBytes(size_t bytes)
{
    ASSERT(CCProxy::isMainThread());
    if (!m_layerRendererInitialized)
        return;

    m_contentsTextureManager->setMemoryAllocationLimitBytes(bytes);
    setNeedsCommit();
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

bool CCLayerTreeHost::updateLayers(CCTextureUpdater& updater)
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

    updateLayers(rootLayer(), updater);
    return true;
}

void CCLayerTreeHost::updateLayers(LayerChromium* rootLayer, CCTextureUpdater& updater)
{
    TRACE_EVENT("CCLayerTreeHost::updateLayers", this, 0);

    if (!rootLayer->renderSurface())
        rootLayer->createRenderSurface();
    rootLayer->renderSurface()->setContentRect(IntRect(IntPoint(0, 0), deviceViewportSize()));

    IntRect rootClipRect(IntPoint(), deviceViewportSize());
    rootLayer->setClipRect(rootClipRect);

    LayerList updateList;
    updateList.append(rootLayer);

    RenderSurfaceChromium* rootRenderSurface = rootLayer->renderSurface();
    rootRenderSurface->clearLayerList();

    {
        TRACE_EVENT("CCLayerTreeHost::updateLayers::calcDrawEtc", this, 0);
        TransformationMatrix identityMatrix;
        TransformationMatrix deviceScaleTransform;
        deviceScaleTransform.scale(m_settings.deviceScaleFactor);
        CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(rootLayer, rootLayer, deviceScaleTransform, identityMatrix, updateList, rootRenderSurface->layerList(), layerRendererCapabilities().maxTextureSize);
    }

    // Reset partial texture update requests.
    m_partialTextureUpdateRequests = 0;

    reserveTextures(updateList);

    paintLayerContents(updateList, PaintVisible, updater);
    if (!m_triggerIdlePaints)
        return;

    size_t preferredLimitBytes = m_contentsTextureManager->preferredMemoryLimitBytes();
    size_t maxLimitBytes = m_contentsTextureManager->maxMemoryLimitBytes();
    m_contentsTextureManager->reduceMemoryToLimit(preferredLimitBytes);
    if (m_contentsTextureManager->currentMemoryUseBytes() >= preferredLimitBytes)
        return;

    // Idle painting should fail when we hit the preferred memory limit,
    // otherwise it will always push us towards the maximum limit.
    m_contentsTextureManager->setMaxMemoryLimitBytes(preferredLimitBytes);
    // The second (idle) paint will be a no-op in layers where painting already occured above.
    paintLayerContents(updateList, PaintIdle, updater);
    m_contentsTextureManager->setMaxMemoryLimitBytes(maxLimitBytes);

    for (size_t i = 0; i < updateList.size(); ++i)
        updateList[i]->clearRenderSurface();
}

void CCLayerTreeHost::reserveTextures(const LayerList& updateList)
{
    // Use BackToFront since it's cheap and this isn't order-dependent.
    typedef CCLayerIterator<LayerChromium, Vector<RefPtr<LayerChromium> >, RenderSurfaceChromium, CCLayerIteratorActions::BackToFront> CCLayerIteratorType;

    CCLayerIteratorType end = CCLayerIteratorType::end(&updateList);
    for (CCLayerIteratorType it = CCLayerIteratorType::begin(&updateList); it != end; ++it) {
        if (!it.representsItself() || !it->alwaysReserveTextures())
            continue;
        it->reserveTextures();
    }
}

// static
void CCLayerTreeHost::update(LayerChromium* layer, PaintType paintType, CCTextureUpdater& updater, const CCOcclusionTracker* occlusion)
{
    ASSERT(layer);
    ASSERT(PaintVisible == paintType || PaintIdle == paintType);
    if (PaintVisible == paintType)
        layer->update(updater, occlusion);
    else
        layer->idleUpdate(updater, occlusion);
}

void CCLayerTreeHost::paintMasksForRenderSurface(LayerChromium* renderSurfaceLayer, PaintType paintType, CCTextureUpdater& updater)
{
    // Note: Masks and replicas only exist for layers that own render surfaces. If we reach this point
    // in code, we already know that at least something will be drawn into this render surface, so the
    // mask and replica should be painted.

    LayerChromium* maskLayer = renderSurfaceLayer->maskLayer();
    if (maskLayer) {
        maskLayer->setVisibleLayerRect(IntRect(IntPoint(), renderSurfaceLayer->contentBounds()));
        update(maskLayer, paintType, updater, 0);
    }

    LayerChromium* replicaMaskLayer = renderSurfaceLayer->replicaLayer() ? renderSurfaceLayer->replicaLayer()->maskLayer() : 0;
    if (replicaMaskLayer) {
        replicaMaskLayer->setVisibleLayerRect(IntRect(IntPoint(), renderSurfaceLayer->contentBounds()));
        update(replicaMaskLayer, paintType, updater, 0);
    }
}

void CCLayerTreeHost::paintLayerContents(const LayerList& renderSurfaceLayerList, PaintType paintType, CCTextureUpdater& updater)
{
    // Use FrontToBack to allow for testing occlusion and performing culling during the tree walk.
    typedef CCLayerIterator<LayerChromium, Vector<RefPtr<LayerChromium> >, RenderSurfaceChromium, CCLayerIteratorActions::FrontToBack> CCLayerIteratorType;

    bool recordMetricsForFrame = true; // FIXME: In the future, disable this when about:tracing is off.
    CCOcclusionTracker occlusionTracker(IntRect(IntPoint(), deviceViewportSize()), recordMetricsForFrame);
    occlusionTracker.setMinimumTrackingSize(CCOcclusionTracker::preferredMinimumTrackingSize());

    CCLayerIteratorType end = CCLayerIteratorType::end(&renderSurfaceLayerList);
    for (CCLayerIteratorType it = CCLayerIteratorType::begin(&renderSurfaceLayerList); it != end; ++it) {
        occlusionTracker.enterLayer(it);

        if (it.representsTargetRenderSurface()) {
            ASSERT(it->renderSurface()->drawOpacity() || it->renderSurface()->drawOpacityIsAnimating());
            paintMasksForRenderSurface(*it, paintType, updater);
        } else if (it.representsItself()) {
            ASSERT(!it->bounds().isEmpty());
            update(*it, paintType, updater, &occlusionTracker);
        }

        occlusionTracker.leaveLayer(it);
    }

    occlusionTracker.overdrawMetrics().recordMetrics(this);
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
        RefPtr<RateLimiter> rateLimiter = RateLimiter::create(context, this);
        m_rateLimiters.set(context, rateLimiter);
        rateLimiter->start();
    }
}

void CCLayerTreeHost::rateLimit()
{
    // Force a no-op command on the compositor context, so that any ratelimiting commands will wait for the compositing
    // context, and therefore for the SwapBuffers.
    m_proxy->forceSerializeOnSwapBuffers();
}

void CCLayerTreeHost::stopRateLimiter(GraphicsContext3D* context)
{
    RateLimiterMap::iterator it = m_rateLimiters.find(context);
    if (it != m_rateLimiters.end()) {
        it->second->stop();
        m_rateLimiters.remove(it);
    }
}

bool CCLayerTreeHost::bufferedUpdates()
{
    return m_settings.maxPartialTextureUpdates != numeric_limits<size_t>::max();
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
        if (layer->id() == events[eventIndex].layerId) {
            if (events[eventIndex].type == CCAnimationEvent::Started)
                layer->notifyAnimationStarted(events[eventIndex], wallClockTime);
            else
                layer->notifyAnimationFinished(wallClockTime);
        }
    }

    for (size_t childIndex = 0; childIndex < layer->children().size(); ++childIndex)
        setAnimationEventsRecursive(events, layer->children()[childIndex].get(), wallClockTime);
}

} // namespace WebCore
