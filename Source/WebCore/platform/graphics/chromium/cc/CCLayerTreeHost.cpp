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
#include "ManagedTexture.h"
#include "Region.h"
#include "TraceEvent.h"
#include "TreeSynchronizer.h"
#include "cc/CCFontAtlas.h"
#include "cc/CCGraphicsContext.h"
#include "cc/CCLayerAnimationController.h"
#include "cc/CCLayerIterator.h"
#include "cc/CCLayerTreeHostCommon.h"
#include "cc/CCLayerTreeHostImpl.h"
#include "cc/CCOcclusionTracker.h"
#include "cc/CCOverdrawMetrics.h"
#include "cc/CCRenderingStats.h"
#include "cc/CCSettings.h"
#include "cc/CCSingleThreadProxy.h"
#include "cc/CCThreadProxy.h"

using namespace std;
using WebKit::WebTransformationMatrix;

namespace {
static int numLayerTreeInstances;
}

namespace WebCore {

bool CCLayerTreeHost::s_needsFilterContext = false;

bool CCLayerTreeHost::anyLayerTreeHostInstanceExists()
{
    return numLayerTreeInstances > 0;
}

PassOwnPtr<CCLayerTreeHost> CCLayerTreeHost::create(CCLayerTreeHostClient* client, const CCLayerTreeSettings& settings)
{
    OwnPtr<CCLayerTreeHost> layerTreeHost = adoptPtr(new CCLayerTreeHost(client, settings));
    if (!layerTreeHost->initialize())
        return nullptr;
    return layerTreeHost.release();
}

CCLayerTreeHost::CCLayerTreeHost(CCLayerTreeHostClient* client, const CCLayerTreeSettings& settings)
    : m_compositorIdentifier(-1)
    , m_animating(false)
    , m_needsAnimateLayers(false)
    , m_client(client)
    , m_animationFrameNumber(0)
    , m_commitNumber(0)
    , m_layerRendererInitialized(false)
    , m_contextLost(false)
    , m_numTimesRecreateShouldFail(0)
    , m_numFailedRecreateAttempts(0)
    , m_settings(settings)
    , m_deviceScaleFactor(1)
    , m_visible(true)
    , m_pageScaleFactor(1)
    , m_minPageScaleFactor(1)
    , m_maxPageScaleFactor(1)
    , m_triggerIdlePaints(true)
    , m_backgroundColor(SK_ColorWHITE)
    , m_hasTransparentBackground(false)
    , m_partialTextureUpdateRequests(0)
{
    ASSERT(CCProxy::isMainThread());
    numLayerTreeInstances++;
}

bool CCLayerTreeHost::initialize()
{
    TRACE_EVENT0("cc", "CCLayerTreeHost::initialize");

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
    TRACE_EVENT0("cc", "CCLayerTreeHost::~CCLayerTreeHost");
    ASSERT(m_proxy);
    m_proxy->stop();
    m_proxy.clear();
    numLayerTreeInstances--;
    RateLimiterMap::iterator it = m_rateLimiters.begin();
    if (it != m_rateLimiters.end())
        it->second->stop();
}

void CCLayerTreeHost::setSurfaceReady()
{
    m_proxy->setSurfaceReady();
}

void CCLayerTreeHost::initializeLayerRenderer()
{
    TRACE_EVENT0("cc", "CCLayerTreeHost::initializeLayerRenderer");
    if (!m_proxy->initializeLayerRenderer()) {
        // Uh oh, better tell the client that we can't do anything with this context.
        m_client->didRecreateContext(false);
        return;
    }

    // Update m_settings based on capabilities that we got back from the renderer.
    m_settings.acceleratePainting = m_proxy->layerRendererCapabilities().usingAcceleratedPainting;

    // Update m_settings based on partial update capability.
    m_settings.maxPartialTextureUpdates = min(m_settings.maxPartialTextureUpdates, m_proxy->maxPartialTextureUpdates());

    m_contentsTextureManager = CCPrioritizedTextureManager::create(0, m_proxy->layerRendererCapabilities().maxTextureSize);

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
        m_contentsTextureManager->clearAllMemory(allocator);
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

    m_animationFrameNumber++;
}

void CCLayerTreeHost::layout()
{
    m_client->layout();
}

void CCLayerTreeHost::beginCommitOnImplThread(CCLayerTreeHostImpl* hostImpl)
{
    ASSERT(CCProxy::isImplThread());
    TRACE_EVENT0("cc", "CCLayerTreeHost::commitTo");

    m_contentsTextureManager->reduceMemory(hostImpl->contentsTextureAllocator());
}

// This function commits the CCLayerTreeHost to an impl tree. When modifying
// this function, keep in mind that the function *runs* on the impl thread! Any
// code that is logically a main thread operation, e.g. deletion of a LayerChromium,
// should be delayed until the CCLayerTreeHost::commitComplete, which will run
// after the commit, but on the main thread.
void CCLayerTreeHost::finishCommitOnImplThread(CCLayerTreeHostImpl* hostImpl)
{
    ASSERT(CCProxy::isImplThread());

    hostImpl->setRootLayer(TreeSynchronizer::synchronizeTrees(rootLayer(), hostImpl->detachLayerTree(), hostImpl));

    // We may have added an animation during the tree sync. This will cause both layer tree hosts
    // to visit their controllers.
    if (rootLayer() && m_needsAnimateLayers)
        hostImpl->setNeedsAnimateLayers();

    hostImpl->setSourceFrameNumber(commitNumber());
    hostImpl->setViewportSize(viewportSize());
    hostImpl->setDeviceScaleFactor(deviceScaleFactor());
    hostImpl->setPageScaleFactorAndLimits(m_pageScaleFactor, m_minPageScaleFactor, m_maxPageScaleFactor);
    hostImpl->setBackgroundColor(m_backgroundColor);
    hostImpl->setHasTransparentBackground(m_hasTransparentBackground);

    m_commitNumber++;
}

void CCLayerTreeHost::commitComplete()
{
    m_deleteTextureAfterCommitList.clear();
    m_client->didCommit();
}

PassOwnPtr<CCGraphicsContext> CCLayerTreeHost::createContext()
{
    if (settings().forceSoftwareCompositing)
        return CCGraphicsContext::create2D();
    return CCGraphicsContext::create3D(m_client->createContext3D());
}

PassOwnPtr<CCLayerTreeHostImpl> CCLayerTreeHost::createLayerTreeHostImpl(CCLayerTreeHostImplClient* client)
{
    return CCLayerTreeHostImpl::create(m_settings, client);
}

void CCLayerTreeHost::didLoseContext()
{
    TRACE_EVENT0("cc", "CCLayerTreeHost::didLoseContext");
    ASSERT(CCProxy::isMainThread());
    m_contextLost = true;
    m_numFailedRecreateAttempts = 0;
    setNeedsCommit();
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
    if (!m_layerRendererInitialized)
        return;
    m_proxy->finishAllRendering();
}

void CCLayerTreeHost::renderingStats(CCRenderingStats& stats) const
{
    m_proxy->implSideRenderingStats(stats);
    stats.numAnimationFrames = animationFrameNumber();
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
    m_proxy->setNeedsCommit();
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
    m_deviceViewportSize.scale(m_deviceScaleFactor);

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
    m_proxy->setVisible(visible);
}

void CCLayerTreeHost::evictAllContentTextures()
{
    ASSERT(CCProxy::isMainThread());
    ASSERT(m_contentsTextureManager.get());
    m_contentsTextureManager->allBackingTexturesWereDeleted();
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

CCPrioritizedTextureManager* CCLayerTreeHost::contentsTextureManager() const
{
    return m_contentsTextureManager.get();
}

void CCLayerTreeHost::composite()
{
    ASSERT(!CCThreadProxy::implThread());
    static_cast<CCSingleThreadProxy*>(m_proxy.get())->compositeImmediately();
}

void CCLayerTreeHost::scheduleComposite()
{
    m_client->scheduleComposite();
}

bool CCLayerTreeHost::initializeLayerRendererIfNeeded()
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
    return true;
}


void CCLayerTreeHost::updateLayers(CCTextureUpdater& updater, size_t memoryAllocationLimitBytes)
{
    ASSERT(m_layerRendererInitialized);
    ASSERT(memoryAllocationLimitBytes);

    if (!rootLayer())
        return;

    if (viewportSize().isEmpty())
        return;

    m_contentsTextureManager->setMaxMemoryLimitBytes(memoryAllocationLimitBytes);

    updateLayers(rootLayer(), updater);
}

void CCLayerTreeHost::updateLayers(LayerChromium* rootLayer, CCTextureUpdater& updater)
{
    TRACE_EVENT0("cc", "CCLayerTreeHost::updateLayers");

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
        TRACE_EVENT0("cc", "CCLayerTreeHost::updateLayers::calcDrawEtc");
        WebTransformationMatrix identityMatrix;
        WebTransformationMatrix deviceScaleTransform;
        deviceScaleTransform.scale(m_deviceScaleFactor);
        CCLayerTreeHostCommon::calculateDrawTransforms(rootLayer, rootLayer, deviceScaleTransform, identityMatrix, updateList, rootRenderSurface->layerList(), layerRendererCapabilities().maxTextureSize);

        FloatRect rootScissorRect(FloatPoint(0, 0), viewportSize());
        CCLayerTreeHostCommon::calculateVisibleAndScissorRects(updateList, rootScissorRect);
    }

    // Reset partial texture update requests.
    m_partialTextureUpdateRequests = 0;

    prioritizeTextures(updateList);

    paintLayerContents(updateList, PaintVisible, updater);

    if (m_triggerIdlePaints) {
        // The second (idle) paint will be a no-op in layers where painting already occured above.
        // FIXME: This pass can be merged with the visible pass now that textures
        //        are prioritized above.
        paintLayerContents(updateList, PaintIdle, updater);
    }

    for (size_t i = 0; i < updateList.size(); ++i)
        updateList[i]->clearRenderSurface();
}

void CCLayerTreeHost::prioritizeTextures(const LayerList& updateList)
{
    // Use BackToFront since it's cheap and this isn't order-dependent.
    typedef CCLayerIterator<LayerChromium, Vector<RefPtr<LayerChromium> >, RenderSurfaceChromium, CCLayerIteratorActions::BackToFront> CCLayerIteratorType;

    m_contentsTextureManager->clearPriorities();

    CCPriorityCalculator calculator;
    CCLayerIteratorType end = CCLayerIteratorType::end(&updateList);
    for (CCLayerIteratorType it = CCLayerIteratorType::begin(&updateList); it != end; ++it) {
        if (it.representsItself())
            it->setTexturePriorities(calculator);
        else if (it.representsTargetRenderSurface()) {
            if (it->maskLayer())
                it->maskLayer()->setTexturePriorities(calculator);
            if (it->replicaLayer() && it->replicaLayer()->maskLayer())
                it->replicaLayer()->maskLayer()->setTexturePriorities(calculator);
        }
    }

    size_t readbackBytes = 0;
    size_t maxBackgroundTextureBytes = 0;
    size_t contentsTextureBytes = 0;

    // Start iteration at 1 to skip the root surface as it does not have a texture cost.
    for (size_t i = 1; i < updateList.size(); ++i) {
        LayerChromium* renderSurfaceLayer = updateList[i].get();
        RenderSurfaceChromium* renderSurface = renderSurfaceLayer->renderSurface();

        size_t bytes = TextureManager::memoryUseBytes(renderSurface->contentRect().size(), GraphicsContext3D::RGBA);
        contentsTextureBytes += bytes;

        if (renderSurface->backgroundFilters().isEmpty())
            continue;

        if (bytes > maxBackgroundTextureBytes)
            maxBackgroundTextureBytes = bytes;
        if (!readbackBytes)
            readbackBytes = TextureManager::memoryUseBytes(m_deviceViewportSize, GraphicsContext3D::RGBA);
    }
    size_t renderSurfacesBytes = readbackBytes + maxBackgroundTextureBytes + contentsTextureBytes;

    m_contentsTextureManager->prioritizeTextures(renderSurfacesBytes);
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
    if (maskLayer)
        update(maskLayer, paintType, updater, 0);

    LayerChromium* replicaMaskLayer = renderSurfaceLayer->replicaLayer() ? renderSurfaceLayer->replicaLayer()->maskLayer() : 0;
    if (replicaMaskLayer)
        update(replicaMaskLayer, paintType, updater, 0);
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

    occlusionTracker.overdrawMetrics().didUseContentsTextureMemoryBytes(m_contentsTextureManager->memoryAboveCutoffBytes());
    occlusionTracker.overdrawMetrics().didUseRenderSurfaceTextureMemoryBytes(m_contentsTextureManager->memoryForRenderSurfacesBytes());
    occlusionTracker.overdrawMetrics().recordMetrics(this);
}

static LayerChromium* findFirstScrollableLayer(LayerChromium* layer)
{
    if (!layer)
        return 0;

    if (layer->scrollable())
        return layer;

    for (size_t i = 0; i < layer->children().size(); ++i) {
        LayerChromium* found = findFirstScrollableLayer(layer->children()[i].get());
        if (found)
            return found;
    }

    return 0;
}

void CCLayerTreeHost::applyScrollAndScale(const CCScrollAndScaleSet& info)
{
    if (!m_rootLayer)
        return;

    LayerChromium* rootScrollLayer = findFirstScrollableLayer(m_rootLayer.get());
    IntSize rootScrollDelta;

    for (size_t i = 0; i < info.scrolls.size(); ++i) {
        LayerChromium* layer = CCLayerTreeHostCommon::findLayerInSubtree(m_rootLayer.get(), info.scrolls[i].layerId);
        if (!layer)
            continue;
        if (layer == rootScrollLayer)
            rootScrollDelta += info.scrolls[i].scrollDelta;
        else
            layer->scrollBy(info.scrolls[i].scrollDelta);
    }
    if (!rootScrollDelta.isZero() || info.pageScaleDelta != 1)
        m_client->applyScrollAndScale(rootScrollDelta, info.pageScaleDelta);
}

void CCLayerTreeHost::startRateLimiter(WebKit::WebGraphicsContext3D* context)
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

void CCLayerTreeHost::stopRateLimiter(WebKit::WebGraphicsContext3D* context)
{
    RateLimiterMap::iterator it = m_rateLimiters.find(context);
    if (it != m_rateLimiters.end()) {
        it->second->stop();
        m_rateLimiters.remove(it);
    }
}

void CCLayerTreeHost::rateLimit()
{
    // Force a no-op command on the compositor context, so that any ratelimiting commands will wait for the compositing
    // context, and therefore for the SwapBuffers.
    m_proxy->forceSerializeOnSwapBuffers();
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

void CCLayerTreeHost::deleteTextureAfterCommit(PassOwnPtr<CCPrioritizedTexture> texture)
{
    m_deleteTextureAfterCommitList.append(texture);
}

void CCLayerTreeHost::setDeviceScaleFactor(float deviceScaleFactor)
{
    if (deviceScaleFactor ==  m_deviceScaleFactor)
        return;
    m_deviceScaleFactor = deviceScaleFactor;

    m_deviceViewportSize = m_viewportSize;
    m_deviceViewportSize.scale(m_deviceScaleFactor);
    setNeedsCommit();
}

void CCLayerTreeHost::animateLayers(double monotonicTime)
{
    if (!CCSettings::acceleratedAnimationEnabled() || !m_needsAnimateLayers)
        return;

    TRACE_EVENT0("cc", "CCLayerTreeHostImpl::animateLayers");
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
