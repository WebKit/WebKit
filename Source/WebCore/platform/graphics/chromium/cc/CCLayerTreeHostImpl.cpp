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

#include "LayerRendererChromium.h"
#include "TextStream.h"
#include "TraceEvent.h"
#include "TrackingTextureAllocator.h"
#include "cc/CCActiveGestureAnimation.h"
#include "cc/CCDamageTracker.h"
#include "cc/CCDebugRectHistory.h"
#include "cc/CCDelayBasedTimeSource.h"
#include "cc/CCFontAtlas.h"
#include "cc/CCFrameRateCounter.h"
#include "cc/CCHeadsUpDisplay.h"
#include "cc/CCLayerIterator.h"
#include "cc/CCLayerTreeHost.h"
#include "cc/CCLayerTreeHostCommon.h"
#include "cc/CCOverdrawMetrics.h"
#include "cc/CCPageScaleAnimation.h"
#include "cc/CCRenderPassDrawQuad.h"
#include "cc/CCSettings.h"
#include "cc/CCSingleThreadProxy.h"
#include <wtf/CurrentTime.h>

using WebKit::WebTransformationMatrix;

namespace {

void didVisibilityChange(WebCore::CCLayerTreeHostImpl* id, bool visible)
{
    if (visible) {
        TRACE_EVENT_ASYNC_BEGIN1("webkit", "CCLayerTreeHostImpl::setVisible", id, "CCLayerTreeHostImpl", id);
        return;
    }

    TRACE_EVENT_ASYNC_END0("webkit", "CCLayerTreeHostImpl::setVisible", id);
}

} // namespace

namespace WebCore {

class CCLayerTreeHostImplTimeSourceAdapter : public CCTimeSourceClient {
    WTF_MAKE_NONCOPYABLE(CCLayerTreeHostImplTimeSourceAdapter);
public:
    static PassOwnPtr<CCLayerTreeHostImplTimeSourceAdapter> create(CCLayerTreeHostImpl* layerTreeHostImpl, PassRefPtr<CCDelayBasedTimeSource> timeSource)
    {
        return adoptPtr(new CCLayerTreeHostImplTimeSourceAdapter(layerTreeHostImpl, timeSource));
    }
    virtual ~CCLayerTreeHostImplTimeSourceAdapter()
    {
        m_timeSource->setClient(0);
        m_timeSource->setActive(false);
    }

    virtual void onTimerTick() OVERRIDE
    {
        // FIXME: We require that animate be called on the impl thread. This
        // avoids asserts in single threaded mode. Ideally background ticking
        // would be handled by the proxy/scheduler and this could be removed.
        DebugScopedSetImplThread impl;

        m_layerTreeHostImpl->animate(monotonicallyIncreasingTime(), currentTime());
    }

    void setActive(bool active)
    {
        if (active != m_timeSource->active())
            m_timeSource->setActive(active);
    }

private:
    CCLayerTreeHostImplTimeSourceAdapter(CCLayerTreeHostImpl* layerTreeHostImpl, PassRefPtr<CCDelayBasedTimeSource> timeSource)
        : m_layerTreeHostImpl(layerTreeHostImpl)
        , m_timeSource(timeSource)
    {
        m_timeSource->setClient(this);
    }

    CCLayerTreeHostImpl* m_layerTreeHostImpl;
    RefPtr<CCDelayBasedTimeSource> m_timeSource;
};

PassOwnPtr<CCLayerTreeHostImpl> CCLayerTreeHostImpl::create(const CCLayerTreeSettings& settings, CCLayerTreeHostImplClient* client)
{
    return adoptPtr(new CCLayerTreeHostImpl(settings, client));
}

CCLayerTreeHostImpl::CCLayerTreeHostImpl(const CCLayerTreeSettings& settings, CCLayerTreeHostImplClient* client)
    : m_client(client)
    , m_sourceFrameNumber(-1)
    , m_sourceAnimationFrameNumber(0)
    , m_rootScrollLayerImpl(0)
    , m_currentlyScrollingLayerImpl(0)
    , m_scrollingLayerIdFromPreviousTree(-1)
    , m_settings(settings)
    , m_deviceScaleFactor(1)
    , m_visible(true)
    , m_contentsTexturesWerePurgedSinceLastCommit(false)
    , m_memoryAllocationLimitBytes(TextureManager::highLimitBytes(viewportSize()))
    , m_headsUpDisplay(CCHeadsUpDisplay::create())
    , m_pageScale(1)
    , m_pageScaleDelta(1)
    , m_sentPageScaleDelta(1)
    , m_minPageScale(0)
    , m_maxPageScale(0)
    , m_backgroundColor(0)
    , m_hasTransparentBackground(false)
    , m_needsAnimateLayers(false)
    , m_pinchGestureActive(false)
    , m_fpsCounter(CCFrameRateCounter::create())
    , m_debugRectHistory(CCDebugRectHistory::create())
{
    ASSERT(CCProxy::isImplThread());
    didVisibilityChange(this, m_visible);
}

CCLayerTreeHostImpl::~CCLayerTreeHostImpl()
{
    ASSERT(CCProxy::isImplThread());
    TRACE_EVENT0("cc", "CCLayerTreeHostImpl::~CCLayerTreeHostImpl()");

    if (m_rootLayerImpl)
        clearRenderSurfaces();
}

void CCLayerTreeHostImpl::beginCommit()
{
}

void CCLayerTreeHostImpl::commitComplete()
{
    TRACE_EVENT0("cc", "CCLayerTreeHostImpl::commitComplete");
    // Recompute max scroll position; must be after layer content bounds are
    // updated.
    updateMaxScrollPosition();
    m_contentsTexturesWerePurgedSinceLastCommit = false;
}

bool CCLayerTreeHostImpl::canDraw()
{
    if (!m_rootLayerImpl) {
        TRACE_EVENT_INSTANT0("cc", "CCLayerTreeHostImpl::canDraw no root layer");
        return false;
    }
    if (viewportSize().isEmpty()) {
        TRACE_EVENT_INSTANT0("cc", "CCLayerTreeHostImpl::canDraw empty viewport");
        return false;
    }
    if (!m_layerRenderer) {
        TRACE_EVENT_INSTANT0("cc", "CCLayerTreeHostImpl::canDraw no layerRenderer");
        return false;
    }
    if (m_contentsTexturesWerePurgedSinceLastCommit) {
        TRACE_EVENT_INSTANT0("cc", "CCLayerTreeHostImpl::canDraw contents textures purged");
        return false;
    }
    return true;
}

CCGraphicsContext* CCLayerTreeHostImpl::context() const
{
    return m_context.get();
}

void CCLayerTreeHostImpl::animate(double monotonicTime, double wallClockTime)
{
    animatePageScale(monotonicTime);
    animateLayers(monotonicTime, wallClockTime);
    animateGestures(monotonicTime);
}

void CCLayerTreeHostImpl::startPageScaleAnimation(const IntSize& targetPosition, bool anchorPoint, float pageScale, double startTime, double duration)
{
    if (!m_rootScrollLayerImpl)
        return;

    IntSize scrollTotal = flooredIntSize(m_rootScrollLayerImpl->scrollPosition() + m_rootScrollLayerImpl->scrollDelta());
    scrollTotal.scale(m_pageScaleDelta);
    float scaleTotal = m_pageScale * m_pageScaleDelta;
    IntSize scaledContentSize = contentSize();
    scaledContentSize.scale(m_pageScaleDelta);

    m_pageScaleAnimation = CCPageScaleAnimation::create(scrollTotal, scaleTotal, m_viewportSize, scaledContentSize, startTime);

    if (anchorPoint) {
        IntSize windowAnchor(targetPosition);
        windowAnchor.scale(scaleTotal / pageScale);
        windowAnchor -= scrollTotal;
        m_pageScaleAnimation->zoomWithAnchor(windowAnchor, pageScale, duration);
    } else
        m_pageScaleAnimation->zoomTo(targetPosition, pageScale, duration);

    m_client->setNeedsRedrawOnImplThread();
    m_client->setNeedsCommitOnImplThread();
}

void CCLayerTreeHostImpl::setActiveGestureAnimation(PassOwnPtr<CCActiveGestureAnimation> gestureAnimation)
{
    m_activeGestureAnimation = gestureAnimation;

    if (m_activeGestureAnimation)
        m_client->setNeedsRedrawOnImplThread();
}

void CCLayerTreeHostImpl::scheduleAnimation()
{
    m_client->setNeedsRedrawOnImplThread();
}

void CCLayerTreeHostImpl::trackDamageForAllSurfaces(CCLayerImpl* rootDrawLayer, const CCLayerList& renderSurfaceLayerList)
{
    // For now, we use damage tracking to compute a global scissor. To do this, we must
    // compute all damage tracking before drawing anything, so that we know the root
    // damage rect. The root damage rect is then used to scissor each surface.

    for (int surfaceIndex = renderSurfaceLayerList.size() - 1; surfaceIndex >= 0 ; --surfaceIndex) {
        CCLayerImpl* renderSurfaceLayer = renderSurfaceLayerList[surfaceIndex];
        CCRenderSurface* renderSurface = renderSurfaceLayer->renderSurface();
        ASSERT(renderSurface);
        renderSurface->damageTracker()->updateDamageTrackingState(renderSurface->layerList(), renderSurfaceLayer->id(), renderSurface->surfacePropertyChangedOnlyFromDescendant(), renderSurface->contentRect(), renderSurfaceLayer->maskLayer(), renderSurfaceLayer->filters());
    }
}

void CCLayerTreeHostImpl::calculateRenderSurfaceLayerList(CCLayerList& renderSurfaceLayerList)
{
    ASSERT(renderSurfaceLayerList.isEmpty());
    ASSERT(m_rootLayerImpl);

    renderSurfaceLayerList.append(m_rootLayerImpl.get());

    if (!m_rootLayerImpl->renderSurface())
        m_rootLayerImpl->createRenderSurface();
    m_rootLayerImpl->renderSurface()->clearLayerList();
    m_rootLayerImpl->renderSurface()->setContentRect(IntRect(IntPoint(), deviceViewportSize()));

    m_rootLayerImpl->setClipRect(IntRect(IntPoint(), deviceViewportSize()));

    {
        TRACE_EVENT0("cc", "CCLayerTreeHostImpl::calcDrawEtc");
        WebTransformationMatrix identityMatrix;
        WebTransformationMatrix deviceScaleTransform;
        deviceScaleTransform.scale(m_deviceScaleFactor);
        CCLayerTreeHostCommon::calculateDrawTransforms(m_rootLayerImpl.get(), m_rootLayerImpl.get(), deviceScaleTransform, identityMatrix, renderSurfaceLayerList, m_rootLayerImpl->renderSurface()->layerList(), &m_layerSorter, layerRendererCapabilities().maxTextureSize);

        trackDamageForAllSurfaces(m_rootLayerImpl.get(), renderSurfaceLayerList);

        if (layerRendererCapabilities().usingPartialSwap)
            m_rootScissorRect = m_rootLayerImpl->renderSurface()->damageTracker()->currentDamageRect();
        else
            m_rootScissorRect = FloatRect(FloatPoint(0, 0), deviceViewportSize());

        CCLayerTreeHostCommon::calculateVisibleAndScissorRects(renderSurfaceLayerList, m_rootScissorRect);
    }
}

bool CCLayerTreeHostImpl::calculateRenderPasses(FrameData& frame)
{
    ASSERT(frame.renderPasses.isEmpty());

    calculateRenderSurfaceLayerList(*frame.renderSurfaceLayerList);

    TRACE_EVENT1("cc", "CCLayerTreeHostImpl::calculateRenderPasses", "renderSurfaceLayerList.size()", static_cast<long long unsigned>(frame.renderSurfaceLayerList->size()));

    m_rootLayerImpl->setScissorRect(enclosingIntRect(m_rootScissorRect));

    // Create the render passes in dependency order.
    HashMap<CCRenderSurface*, CCRenderPass*> surfacePassMap;
    for (int surfaceIndex = frame.renderSurfaceLayerList->size() - 1; surfaceIndex >= 0 ; --surfaceIndex) {
        CCLayerImpl* renderSurfaceLayer = (*frame.renderSurfaceLayerList)[surfaceIndex];
        CCRenderSurface* renderSurface = renderSurfaceLayer->renderSurface();

        // FIXME: Make this unique across all CCLayerTreeHostImpls.
        int globalRenderPassId = renderSurfaceLayer->id();

        OwnPtr<CCRenderPass> pass = CCRenderPass::create(renderSurface, globalRenderPassId);
        surfacePassMap.add(renderSurface, pass.get());
        frame.renderPasses.append(pass.release());
    }

    bool recordMetricsForFrame = true; // FIXME: In the future, disable this when about:tracing is off.
    CCOcclusionTrackerImpl occlusionTracker(enclosingIntRect(m_rootScissorRect), recordMetricsForFrame);
    occlusionTracker.setMinimumTrackingSize(CCOcclusionTrackerImpl::preferredMinimumTrackingSize());

    if (settings().showOccludingRects)
        occlusionTracker.setOccludingScreenSpaceRectsContainer(&frame.occludingScreenSpaceRects);

    // Add quads to the Render passes in FrontToBack order to allow for testing occlusion and performing culling during the tree walk.
    typedef CCLayerIterator<CCLayerImpl, Vector<CCLayerImpl*>, CCRenderSurface, CCLayerIteratorActions::FrontToBack> CCLayerIteratorType;

    // Typically when we are missing a texture and use a checkerboard quad, we still draw the frame. However when the layer being
    // checkerboarded is moving due to an impl-animation, we drop the frame to avoid flashing due to the texture suddenly appearing
    // in the future.
    bool drawFrame = true;

    CCLayerIteratorType end = CCLayerIteratorType::end(frame.renderSurfaceLayerList);
    for (CCLayerIteratorType it = CCLayerIteratorType::begin(frame.renderSurfaceLayerList); it != end; ++it) {
        CCRenderSurface* renderSurface = it.targetRenderSurfaceLayer()->renderSurface();
        CCRenderPass* pass = surfacePassMap.get(renderSurface);
        bool hadMissingTiles = false;

        occlusionTracker.enterLayer(it);

        if (it.representsContributingRenderSurface() && !it->renderSurface()->scissorRect().isEmpty()) {
            CCRenderPass* contributingRenderPass = surfacePassMap.get(it->renderSurface());
            pass->appendQuadsForRenderSurfaceLayer(*it, contributingRenderPass, &occlusionTracker);
        } else if (it.representsItself() && !occlusionTracker.occluded(*it, it->visibleLayerRect()) && !it->visibleLayerRect().isEmpty() && !it->scissorRect().isEmpty()) {
            it->willDraw(m_layerRenderer.get(), context());
            frame.willDrawLayers.append(*it);

            pass->appendQuadsForLayer(*it, &occlusionTracker, hadMissingTiles);
        }

        if (hadMissingTiles) {
            bool layerHasAnimatingTransform = it->screenSpaceTransformIsAnimating() || it->drawTransformIsAnimating();
            if (layerHasAnimatingTransform)
                drawFrame = false;
        }

        occlusionTracker.leaveLayer(it);
    }

    if (!m_hasTransparentBackground) {
        frame.renderPasses.last()->setHasTransparentBackground(false);
        frame.renderPasses.last()->appendQuadsToFillScreen(m_rootLayerImpl.get(), m_backgroundColor, occlusionTracker);
    }

    if (drawFrame)
        occlusionTracker.overdrawMetrics().recordMetrics(this);

    m_layerRenderer->decideRenderPassAllocationsForFrame(frame.renderPasses);
    removePassesWithCachedTextures(frame.renderPasses, frame.skippedPasses, m_layerRenderer.get());

    return drawFrame;
}

void CCLayerTreeHostImpl::animateLayersRecursive(CCLayerImpl* current, double monotonicTime, double wallClockTime, CCAnimationEventsVector* events, bool& didAnimate, bool& needsAnimateLayers)
{
    bool subtreeNeedsAnimateLayers = false;

    CCLayerAnimationController* currentController = current->layerAnimationController();

    bool hadActiveAnimation = currentController->hasActiveAnimation();
    currentController->animate(monotonicTime, events);
    bool startedAnimation = events->size() > 0;

    // We animated if we either ticked a running animation, or started a new animation.
    if (hadActiveAnimation || startedAnimation)
        didAnimate = true;

    // If the current controller still has an active animation, we must continue animating layers.
    if (currentController->hasActiveAnimation())
         subtreeNeedsAnimateLayers = true;

    for (size_t i = 0; i < current->children().size(); ++i) {
        bool childNeedsAnimateLayers = false;
        animateLayersRecursive(current->children()[i].get(), monotonicTime, wallClockTime, events, didAnimate, childNeedsAnimateLayers);
        if (childNeedsAnimateLayers)
            subtreeNeedsAnimateLayers = true;
    }

    needsAnimateLayers = subtreeNeedsAnimateLayers;
}

void CCLayerTreeHostImpl::setBackgroundTickingEnabled(bool enabled)
{
    // Lazily create the timeSource adapter so that we can vary the interval for testing.
    if (!m_timeSourceClientAdapter)
        m_timeSourceClientAdapter = CCLayerTreeHostImplTimeSourceAdapter::create(this, CCDelayBasedTimeSource::create(lowFrequencyAnimationInterval(), CCProxy::currentThread()));

    m_timeSourceClientAdapter->setActive(enabled);
}

IntSize CCLayerTreeHostImpl::contentSize() const
{
    // TODO(aelias): Hardcoding the first child here is weird. Think of
    // a cleaner way to get the contentBounds on the Impl side.
    if (!m_rootScrollLayerImpl || m_rootScrollLayerImpl->children().isEmpty())
        return IntSize();
    return m_rootScrollLayerImpl->children()[0]->contentBounds();
}

void CCLayerTreeHostImpl::removeRenderPassesRecursive(CCRenderPassList& passes, size_t bottomPass, const CCRenderPass* firstToRemove, CCRenderPassList& skippedPasses)
{
    size_t removeIndex = passes.find(firstToRemove);

    // The pass was already removed by another quad - probably the original, and we are the replica.
    if (removeIndex == notFound)
        return;

    OwnPtr<CCRenderPass> removedPass = passes[removeIndex].release();
    passes.remove(removeIndex);

    // Now follow up for all RenderPass quads and remove their render passes recursively.
    const CCQuadList& quadList = removedPass->quadList();
    CCQuadList::constBackToFrontIterator quadListIterator = quadList.backToFrontBegin();
    for (; quadListIterator != quadList.backToFrontEnd(); ++quadListIterator) {
        CCDrawQuad* currentQuad = (*quadListIterator).get();
        if (currentQuad->material() != CCDrawQuad::RenderPass)
            continue;

        CCRenderPassDrawQuad* renderPassQuad = static_cast<CCRenderPassDrawQuad*>(currentQuad);
        const CCRenderPass* nextRenderPass = renderPassQuad->renderPass();

        // Our search is now limited up to the pass that we just removed.
        // Substitute removeIndex for bottomPass now.
        removeRenderPassesRecursive(passes, removeIndex, nextRenderPass, skippedPasses);
    }
    skippedPasses.append(removedPass.release());
}

void CCLayerTreeHostImpl::removePassesWithCachedTextures(CCRenderPassList& passes, CCRenderPassList& skippedPasses, const CCRenderer* renderer)
{
    for (int passIndex = passes.size() - 1; passIndex >= 0; --passIndex) {
        CCRenderPass* currentPass = passes[passIndex].get();
        const CCQuadList& quadList = currentPass->quadList();
        CCQuadList::constBackToFrontIterator quadListIterator = quadList.backToFrontBegin();

        for (; quadListIterator != quadList.backToFrontEnd(); ++quadListIterator) {
            CCDrawQuad* currentQuad = quadListIterator->get();

            if (currentQuad->material() != CCDrawQuad::RenderPass)
                continue;

            CCRenderPassDrawQuad* renderPassQuad = static_cast<CCRenderPassDrawQuad*>(currentQuad);
            if (!renderPassQuad->contentsChangedSinceLastFrame().isEmpty())
                continue;
            if (!renderer->haveCachedResourcesForRenderPassId(renderPassQuad->renderPassId()))
                continue;

            // We are changing the vector in the middle of reverse iteration.
            // We are guaranteed that any data from iterator to the end will not change.
            // Capture the iterator position from the end, and restore it after the change.
            int positionFromEnd = passes.size() - passIndex;
            removeRenderPassesRecursive(passes, passIndex, renderPassQuad->renderPass(), skippedPasses);
            passIndex = passes.size() - positionFromEnd;
            ASSERT(passIndex >= 0);
        }
    }
}

bool CCLayerTreeHostImpl::prepareToDraw(FrameData& frame)
{
    TRACE_EVENT0("cc", "CCLayerTreeHostImpl::prepareToDraw");
    ASSERT(canDraw());

    frame.renderSurfaceLayerList = &m_renderSurfaceLayerList;
    frame.renderPasses.clear();
    frame.renderSurfaceLayerList->clear();
    frame.willDrawLayers.clear();

    if (!calculateRenderPasses(frame))
        return false;

    // If we return true, then we expect drawLayers() to be called before this function is called again.
    return true;
}

void CCLayerTreeHostImpl::releaseContentsTextures()
{
    contentsTextureAllocator()->deleteAllTextures();
    m_contentsTexturesWerePurgedSinceLastCommit = true;
    m_client->setNeedsCommitOnImplThread();
}

void CCLayerTreeHostImpl::setMemoryAllocationLimitBytes(size_t bytes)
{
    if (m_memoryAllocationLimitBytes == bytes)
        return;
    m_memoryAllocationLimitBytes = bytes;

    ASSERT(bytes);
    m_client->setNeedsCommitOnImplThread();
}

void CCLayerTreeHostImpl::drawLayers(const FrameData& frame)
{
    TRACE_EVENT0("cc", "CCLayerTreeHostImpl::drawLayers");
    ASSERT(canDraw());
    ASSERT(!frame.renderPasses.isEmpty());

    // FIXME: use the frame begin time from the overall compositor scheduler.
    // This value is currently inaccessible because it is up in Chromium's
    // RenderWidget.

    // The root RenderPass is the last one to be drawn.
    CCRenderPass* rootRenderPass = frame.renderPasses.last().get();

    m_fpsCounter->markBeginningOfFrame(currentTime());
    m_layerRenderer->beginDrawingFrame(rootRenderPass);

    for (size_t i = 0; i < frame.renderPasses.size(); ++i) {
        CCRenderPass* renderPass = frame.renderPasses[i].get();

        FloatRect rootScissorRectInCurrentSurface = renderPass->targetSurface()->computeRootScissorRectInCurrentSurface(m_rootScissorRect);
        m_layerRenderer->drawRenderPass(renderPass, rootScissorRectInCurrentSurface);

        renderPass->targetSurface()->damageTracker()->didDrawDamagedArea();
    }

    if (m_debugRectHistory->enabled(settings()))
        m_debugRectHistory->saveDebugRectsForCurrentFrame(m_rootLayerImpl.get(), *frame.renderSurfaceLayerList, frame.occludingScreenSpaceRects, settings());

    if (m_headsUpDisplay->enabled(settings()))
        m_headsUpDisplay->draw(this);

    m_layerRenderer->finishDrawingFrame();

    ++m_sourceAnimationFrameNumber;

    // The next frame should start by assuming nothing has changed, and changes are noted as they occur.
    m_rootLayerImpl->resetAllChangeTrackingForSubtree();
}

void CCLayerTreeHostImpl::didDrawAllLayers(const FrameData& frame)
{
    for (size_t i = 0; i < frame.willDrawLayers.size(); ++i)
        frame.willDrawLayers[i]->didDraw();
}

void CCLayerTreeHostImpl::finishAllRendering()
{
    if (m_layerRenderer)
        m_layerRenderer->finish();
}

bool CCLayerTreeHostImpl::isContextLost()
{
    return m_layerRenderer && m_layerRenderer->isContextLost();
}

const LayerRendererCapabilities& CCLayerTreeHostImpl::layerRendererCapabilities() const
{
    return m_layerRenderer->capabilities();
}

TextureAllocator* CCLayerTreeHostImpl::contentsTextureAllocator() const
{
    return m_layerRenderer ? m_layerRenderer->contentsTextureAllocator() : 0;
}

bool CCLayerTreeHostImpl::swapBuffers()
{
    ASSERT(m_layerRenderer);

    m_fpsCounter->markEndOfFrame();
    return m_layerRenderer->swapBuffers(enclosingIntRect(m_rootScissorRect));
}

void CCLayerTreeHostImpl::didLoseContext()
{
    m_client->didLoseContextOnImplThread();
}

void CCLayerTreeHostImpl::onSwapBuffersComplete()
{
    m_client->onSwapBuffersCompleteOnImplThread();
}

void CCLayerTreeHostImpl::readback(void* pixels, const IntRect& rect)
{
    ASSERT(m_layerRenderer);
    m_layerRenderer->getFramebufferPixels(pixels, rect);
}

static CCLayerImpl* findRootScrollLayer(CCLayerImpl* layer)
{
    if (!layer)
        return 0;

    if (layer->scrollable())
        return layer;

    for (size_t i = 0; i < layer->children().size(); ++i) {
        CCLayerImpl* found = findRootScrollLayer(layer->children()[i].get());
        if (found)
            return found;
    }

    return 0;
}

// Content layers can be either directly scrollable or contained in an outer
// scrolling layer which applies the scroll transform. Given a content layer,
// this function returns the associated scroll layer if any.
static CCLayerImpl* findScrollLayerForContentLayer(CCLayerImpl* layerImpl)
{
    if (!layerImpl)
        return 0;

    if (layerImpl->scrollable())
        return layerImpl;

    if (layerImpl->drawsContent() && layerImpl->parent() && layerImpl->parent()->scrollable())
        return layerImpl->parent();

    return 0;
}

void CCLayerTreeHostImpl::setRootLayer(PassOwnPtr<CCLayerImpl> layer)
{
    m_rootLayerImpl = layer;
    m_rootScrollLayerImpl = findRootScrollLayer(m_rootLayerImpl.get());
    m_currentlyScrollingLayerImpl = 0;

    if (m_rootLayerImpl && m_scrollingLayerIdFromPreviousTree != -1)
        m_currentlyScrollingLayerImpl = CCLayerTreeHostCommon::findLayerInSubtree(m_rootLayerImpl.get(), m_scrollingLayerIdFromPreviousTree);

    m_scrollingLayerIdFromPreviousTree = -1;
}

PassOwnPtr<CCLayerImpl> CCLayerTreeHostImpl::detachLayerTree()
{
    // Clear all data structures that have direct references to the layer tree.
    m_scrollingLayerIdFromPreviousTree = m_currentlyScrollingLayerImpl ? m_currentlyScrollingLayerImpl->id() : -1;
    m_currentlyScrollingLayerImpl = 0;
    m_renderSurfaceLayerList.clear();

    return m_rootLayerImpl.release();
}

void CCLayerTreeHostImpl::setVisible(bool visible)
{
    ASSERT(CCProxy::isImplThread());

    if (m_visible == visible)
        return;
    m_visible = visible;
    didVisibilityChange(this, m_visible);

    if (!m_layerRenderer)
        return;

    m_layerRenderer->setVisible(visible);

    setBackgroundTickingEnabled(!m_visible && m_needsAnimateLayers);
}

bool CCLayerTreeHostImpl::initializeLayerRenderer(PassOwnPtr<CCGraphicsContext> context, TextureUploaderOption textureUploader)
{
    WebKit::WebGraphicsContext3D* context3d = context->context3D();
    if (!context3d) {
        // FIXME: Implement this path for software compositing.
        return false;
    }

    OwnPtr<LayerRendererChromium> layerRenderer;
    layerRenderer = LayerRendererChromium::create(this, context3d, textureUploader);

    // Since we now have a new context/layerRenderer, we cannot continue to use the old
    // resources (i.e. renderSurfaces and texture IDs).
    if (m_rootLayerImpl) {
        clearRenderSurfaces();
        sendDidLoseContextRecursive(m_rootLayerImpl.get());
    }

    m_layerRenderer = layerRenderer.release();
    if (m_layerRenderer)
        m_context = context;

    if (!m_visible && m_layerRenderer)
         m_layerRenderer->setVisible(m_visible);

    return m_layerRenderer;
}

void CCLayerTreeHostImpl::setViewportSize(const IntSize& viewportSize)
{
    if (viewportSize == m_viewportSize)
        return;

    m_viewportSize = viewportSize;

    m_deviceViewportSize = viewportSize;
    m_deviceViewportSize.scale(m_deviceScaleFactor);

    updateMaxScrollPosition();

    if (m_layerRenderer)
        m_layerRenderer->viewportChanged();
}

static void adjustScrollsForPageScaleChange(CCLayerImpl* layerImpl, float pageScaleChange)
{
    if (!layerImpl)
        return;

    if (layerImpl->scrollable()) {
        // We need to convert impl-side scroll deltas to pageScale space.
        FloatSize scrollDelta = layerImpl->scrollDelta();
        scrollDelta.scale(pageScaleChange);
        layerImpl->setScrollDelta(scrollDelta);
    }

    for (size_t i = 0; i < layerImpl->children().size(); ++i)
        adjustScrollsForPageScaleChange(layerImpl->children()[i].get(), pageScaleChange);
}

static void applyPageScaleDeltaToScrollLayers(CCLayerImpl* layerImpl, float pageScaleDelta)
{
    if (!layerImpl)
        return;

    if (layerImpl->scrollable())
        layerImpl->setPageScaleDelta(pageScaleDelta);

    for (size_t i = 0; i < layerImpl->children().size(); ++i)
        applyPageScaleDeltaToScrollLayers(layerImpl->children()[i].get(), pageScaleDelta);
}

void CCLayerTreeHostImpl::setDeviceScaleFactor(float deviceScaleFactor)
{
    if (deviceScaleFactor == m_deviceScaleFactor)
        return;
    m_deviceScaleFactor = deviceScaleFactor;

    m_deviceViewportSize = viewportSize();
    m_deviceViewportSize.scale(m_deviceScaleFactor);
    updateMaxScrollPosition();
    if (m_layerRenderer)
        m_layerRenderer->viewportChanged();
}


void CCLayerTreeHostImpl::setPageScaleFactorAndLimits(float pageScale, float minPageScale, float maxPageScale)
{
    if (!pageScale)
        return;

    if (m_sentPageScaleDelta == 1 && pageScale == m_pageScale && minPageScale == m_minPageScale && maxPageScale == m_maxPageScale)
        return;

    m_minPageScale = minPageScale;
    m_maxPageScale = maxPageScale;

    float pageScaleChange = pageScale / m_pageScale;
    m_pageScale = pageScale;

    if (pageScaleChange != 1)
        adjustScrollsForPageScaleChange(m_rootScrollLayerImpl, pageScaleChange);

    // Clamp delta to limits and refresh display matrix.
    setPageScaleDelta(m_pageScaleDelta / m_sentPageScaleDelta);
    m_sentPageScaleDelta = 1;
    applyPageScaleDeltaToScrollLayers(m_rootScrollLayerImpl, m_pageScaleDelta);
}

void CCLayerTreeHostImpl::setPageScaleDelta(float delta)
{
    // Clamp to the current min/max limits.
    float finalMagnifyScale = m_pageScale * delta;
    if (m_minPageScale && finalMagnifyScale < m_minPageScale)
        delta = m_minPageScale / m_pageScale;
    else if (m_maxPageScale && finalMagnifyScale > m_maxPageScale)
        delta = m_maxPageScale / m_pageScale;

    if (delta == m_pageScaleDelta)
        return;

    m_pageScaleDelta = delta;

    updateMaxScrollPosition();
    applyPageScaleDeltaToScrollLayers(m_rootScrollLayerImpl, m_pageScaleDelta);
}

void CCLayerTreeHostImpl::updateMaxScrollPosition()
{
    if (!m_rootScrollLayerImpl || !m_rootScrollLayerImpl->children().size())
        return;

    FloatSize viewBounds = m_viewportSize;
    if (CCLayerImpl* clipLayer = m_rootScrollLayerImpl->parent()) {
        if (clipLayer->masksToBounds())
            viewBounds = clipLayer->bounds();
    }
    viewBounds.scale(1 / m_pageScaleDelta);
    viewBounds.scale(m_deviceScaleFactor);

    // maxScroll is computed in physical pixels, but scroll positions are in layout pixels.
    IntSize maxScroll = contentSize() - expandedIntSize(viewBounds);
    maxScroll.scale(1 / m_deviceScaleFactor);
    // The viewport may be larger than the contents in some cases, such as
    // having a vertical scrollbar but no horizontal overflow.
    maxScroll.clampNegativeToZero();

    m_rootScrollLayerImpl->setMaxScrollPosition(maxScroll);
}

void CCLayerTreeHostImpl::setNeedsRedraw()
{
    m_client->setNeedsRedrawOnImplThread();
}

bool CCLayerTreeHostImpl::ensureRenderSurfaceLayerList()
{
    if (!m_rootLayerImpl)
        return false;

    // We need both a non-empty render surface layer list and a root render
    // surface to be able to iterate over the visible layers.
    if (m_renderSurfaceLayerList.size() && m_rootLayerImpl->renderSurface())
        return true;

    // If we are called after setRootLayer() but before prepareToDraw(), we need
    // to recalculate the visible layers. This prevents being unable to scroll
    // during part of a commit.
    m_renderSurfaceLayerList.clear();
    calculateRenderSurfaceLayerList(m_renderSurfaceLayerList);

    return m_renderSurfaceLayerList.size();
}

CCInputHandlerClient::ScrollStatus CCLayerTreeHostImpl::scrollBegin(const IntPoint& viewportPoint, CCInputHandlerClient::ScrollInputType type)
{
    TRACE_EVENT0("cc", "CCLayerTreeHostImpl::scrollBegin");

    ASSERT(!m_currentlyScrollingLayerImpl);
    clearCurrentlyScrollingLayer();

    if (!ensureRenderSurfaceLayerList())
        return ScrollIgnored;

    IntPoint deviceViewportPoint = viewportPoint;
    deviceViewportPoint.scale(m_deviceScaleFactor, m_deviceScaleFactor);

    // First find out which layer was hit from the saved list of visible layers
    // in the most recent frame.
    CCLayerImpl* layerImpl = CCLayerTreeHostCommon::findLayerThatIsHitByPoint(viewportPoint, m_renderSurfaceLayerList);

    // Walk up the hierarchy and look for a scrollable layer.
    CCLayerImpl* potentiallyScrollingLayerImpl = 0;
    for (; layerImpl; layerImpl = layerImpl->parent()) {
        // The content layer can also block attempts to scroll outside the main thread.
        if (layerImpl->tryScroll(deviceViewportPoint, type) == ScrollOnMainThread)
            return ScrollOnMainThread;

        CCLayerImpl* scrollLayerImpl = findScrollLayerForContentLayer(layerImpl);
        if (!scrollLayerImpl)
            continue;

        ScrollStatus status = scrollLayerImpl->tryScroll(viewportPoint, type);

        // If any layer wants to divert the scroll event to the main thread, abort.
        if (status == ScrollOnMainThread)
            return ScrollOnMainThread;

        if (status == ScrollStarted && !potentiallyScrollingLayerImpl)
            potentiallyScrollingLayerImpl = scrollLayerImpl;
    }

    if (potentiallyScrollingLayerImpl) {
        m_currentlyScrollingLayerImpl = potentiallyScrollingLayerImpl;
        return ScrollStarted;
    }
    return ScrollIgnored;
}

void CCLayerTreeHostImpl::scrollBy(const IntSize& scrollDelta)
{
    TRACE_EVENT0("cc", "CCLayerTreeHostImpl::scrollBy");
    if (!m_currentlyScrollingLayerImpl)
        return;

    FloatSize pendingDelta(scrollDelta);
    pendingDelta.scale(1 / m_pageScaleDelta);

    for (CCLayerImpl* layerImpl = m_currentlyScrollingLayerImpl; layerImpl && !pendingDelta.isZero(); layerImpl = layerImpl->parent()) {
        if (!layerImpl->scrollable())
            continue;
        FloatSize previousDelta(layerImpl->scrollDelta());
        layerImpl->scrollBy(pendingDelta);
        // Reset the pending scroll delta to zero if the layer was able to move along the requested
        // axis. This is to ensure it is possible to scroll exactly to the beginning or end of a
        // scroll area regardless of the scroll step. For diagonal scrolls this also avoids applying
        // the scroll on one axis to multiple layers.
        if (previousDelta.width() != layerImpl->scrollDelta().width())
            pendingDelta.setWidth(0);
        if (previousDelta.height() != layerImpl->scrollDelta().height())
            pendingDelta.setHeight(0);
    }

    if (!scrollDelta.isZero() && pendingDelta.isEmpty()) {
        m_client->setNeedsCommitOnImplThread();
        m_client->setNeedsRedrawOnImplThread();
    }
}

void CCLayerTreeHostImpl::clearCurrentlyScrollingLayer()
{
    m_currentlyScrollingLayerImpl = 0;
    m_scrollingLayerIdFromPreviousTree = -1;
}

void CCLayerTreeHostImpl::scrollEnd()
{
    clearCurrentlyScrollingLayer();
}

void CCLayerTreeHostImpl::pinchGestureBegin()
{
    m_pinchGestureActive = true;
    m_previousPinchAnchor = IntPoint();
}

void CCLayerTreeHostImpl::pinchGestureUpdate(float magnifyDelta,
                                             const IntPoint& anchor)
{
    TRACE_EVENT0("cc", "CCLayerTreeHostImpl::pinchGestureUpdate");

    if (!m_rootScrollLayerImpl)
        return;

    if (m_previousPinchAnchor == IntPoint::zero())
        m_previousPinchAnchor = anchor;

    // Keep the center-of-pinch anchor specified by (x, y) in a stable
    // position over the course of the magnify.
    FloatPoint previousScaleAnchor(m_previousPinchAnchor.x() / m_pageScaleDelta, m_previousPinchAnchor.y() / m_pageScaleDelta);
    setPageScaleDelta(m_pageScaleDelta * magnifyDelta);
    FloatPoint newScaleAnchor(anchor.x() / m_pageScaleDelta, anchor.y() / m_pageScaleDelta);
    FloatSize move = previousScaleAnchor - newScaleAnchor;

    m_previousPinchAnchor = anchor;

    m_rootScrollLayerImpl->scrollBy(roundedIntSize(move));
    m_client->setNeedsCommitOnImplThread();
    m_client->setNeedsRedrawOnImplThread();
}

void CCLayerTreeHostImpl::pinchGestureEnd()
{
    m_pinchGestureActive = false;

    m_client->setNeedsCommitOnImplThread();
}

void CCLayerTreeHostImpl::computeDoubleTapZoomDeltas(CCScrollAndScaleSet* scrollInfo)
{
    float pageScale = m_pageScaleAnimation->finalPageScale();
    IntSize scrollOffset = m_pageScaleAnimation->finalScrollOffset();
    scrollOffset.scale(m_pageScale / pageScale);
    makeScrollAndScaleSet(scrollInfo, scrollOffset, pageScale);
}

void CCLayerTreeHostImpl::computePinchZoomDeltas(CCScrollAndScaleSet* scrollInfo)
{
    if (!m_rootScrollLayerImpl)
        return;

    // Only send fake scroll/zoom deltas if we're pinch zooming out by a
    // significant amount. This also ensures only one fake delta set will be
    // sent.
    const float pinchZoomOutSensitivity = 0.95;
    if (m_pageScaleDelta > pinchZoomOutSensitivity)
        return;

    // Compute where the scroll offset/page scale would be if fully pinch-zoomed
    // out from the anchor point.
    IntSize scrollBegin = flooredIntSize(m_rootScrollLayerImpl->scrollPosition() + m_rootScrollLayerImpl->scrollDelta());
    scrollBegin.scale(m_pageScaleDelta);
    float scaleBegin = m_pageScale * m_pageScaleDelta;
    float pageScaleDeltaToSend = m_minPageScale / m_pageScale;
    FloatSize scaledContentsSize = contentSize();
    scaledContentsSize.scale(pageScaleDeltaToSend);

    FloatSize anchor = toSize(m_previousPinchAnchor);
    FloatSize scrollEnd = scrollBegin + anchor;
    scrollEnd.scale(m_minPageScale / scaleBegin);
    scrollEnd -= anchor;
    scrollEnd = scrollEnd.shrunkTo(roundedIntSize(scaledContentsSize - m_deviceViewportSize)).expandedTo(FloatSize(0, 0));
    scrollEnd.scale(1 / pageScaleDeltaToSend);
    scrollEnd.scale(m_deviceScaleFactor);

    makeScrollAndScaleSet(scrollInfo, roundedIntSize(scrollEnd), m_minPageScale);
}

void CCLayerTreeHostImpl::makeScrollAndScaleSet(CCScrollAndScaleSet* scrollInfo, const IntSize& scrollOffset, float pageScale)
{
    if (!m_rootScrollLayerImpl)
        return;

    CCLayerTreeHostCommon::ScrollUpdateInfo scroll;
    scroll.layerId = m_rootScrollLayerImpl->id();
    scroll.scrollDelta = scrollOffset - toSize(m_rootScrollLayerImpl->scrollPosition());
    scrollInfo->scrolls.append(scroll);
    m_rootScrollLayerImpl->setSentScrollDelta(scroll.scrollDelta);
    m_sentPageScaleDelta = scrollInfo->pageScaleDelta = pageScale / m_pageScale;
}

static void collectScrollDeltas(CCScrollAndScaleSet* scrollInfo, CCLayerImpl* layerImpl)
{
    if (!layerImpl)
        return;

    if (!layerImpl->scrollDelta().isZero()) {
        IntSize scrollDelta = flooredIntSize(layerImpl->scrollDelta());
        CCLayerTreeHostCommon::ScrollUpdateInfo scroll;
        scroll.layerId = layerImpl->id();
        scroll.scrollDelta = scrollDelta;
        scrollInfo->scrolls.append(scroll);
        layerImpl->setSentScrollDelta(scrollDelta);
    }

    for (size_t i = 0; i < layerImpl->children().size(); ++i)
        collectScrollDeltas(scrollInfo, layerImpl->children()[i].get());
}

PassOwnPtr<CCScrollAndScaleSet> CCLayerTreeHostImpl::processScrollDeltas()
{
    OwnPtr<CCScrollAndScaleSet> scrollInfo = adoptPtr(new CCScrollAndScaleSet());

    if (m_pinchGestureActive || m_pageScaleAnimation) {
        m_sentPageScaleDelta = scrollInfo->pageScaleDelta = 1;
        if (m_pinchGestureActive)
            computePinchZoomDeltas(scrollInfo.get());
        else if (m_pageScaleAnimation.get())
            computeDoubleTapZoomDeltas(scrollInfo.get());
        return scrollInfo.release();
    }

    collectScrollDeltas(scrollInfo.get(), m_rootLayerImpl.get());
    m_sentPageScaleDelta = scrollInfo->pageScaleDelta = m_pageScaleDelta;

    return scrollInfo.release();
}

void CCLayerTreeHostImpl::setFullRootLayerDamage()
{
    if (m_rootLayerImpl) {
        CCRenderSurface* renderSurface = m_rootLayerImpl->renderSurface();
        if (renderSurface)
            renderSurface->damageTracker()->forceFullDamageNextUpdate();
    }
}

void CCLayerTreeHostImpl::animatePageScale(double monotonicTime)
{
    if (!m_pageScaleAnimation || !m_rootScrollLayerImpl)
        return;

    IntSize scrollTotal = flooredIntSize(m_rootScrollLayerImpl->scrollPosition() + m_rootScrollLayerImpl->scrollDelta());

    setPageScaleDelta(m_pageScaleAnimation->pageScaleAtTime(monotonicTime) / m_pageScale);
    IntSize nextScroll = m_pageScaleAnimation->scrollOffsetAtTime(monotonicTime);
    nextScroll.scale(1 / m_pageScaleDelta);
    m_rootScrollLayerImpl->scrollBy(nextScroll - scrollTotal);
    m_client->setNeedsRedrawOnImplThread();

    if (m_pageScaleAnimation->isAnimationCompleteAtTime(monotonicTime)) {
        m_pageScaleAnimation.clear();
        m_client->setNeedsCommitOnImplThread();
    }
}

void CCLayerTreeHostImpl::animateLayers(double monotonicTime, double wallClockTime)
{
    if (!CCSettings::acceleratedAnimationEnabled() || !m_needsAnimateLayers || !m_rootLayerImpl)
        return;

    TRACE_EVENT0("cc", "CCLayerTreeHostImpl::animateLayers");

    OwnPtr<CCAnimationEventsVector> events(adoptPtr(new CCAnimationEventsVector));

    bool didAnimate = false;
    animateLayersRecursive(m_rootLayerImpl.get(), monotonicTime, wallClockTime, events.get(), didAnimate, m_needsAnimateLayers);

    if (!events->isEmpty())
        m_client->postAnimationEventsToMainThreadOnImplThread(events.release(), wallClockTime);

    if (didAnimate)
        m_client->setNeedsRedrawOnImplThread();

    setBackgroundTickingEnabled(!m_visible && m_needsAnimateLayers);
}

double CCLayerTreeHostImpl::lowFrequencyAnimationInterval() const
{
    return 1;
}

void CCLayerTreeHostImpl::sendDidLoseContextRecursive(CCLayerImpl* current)
{
    ASSERT(current);
    current->didLoseContext();
    if (current->maskLayer())
        sendDidLoseContextRecursive(current->maskLayer());
    if (current->replicaLayer())
        sendDidLoseContextRecursive(current->replicaLayer());
    for (size_t i = 0; i < current->children().size(); ++i)
        sendDidLoseContextRecursive(current->children()[i].get());
}

static void clearRenderSurfacesOnCCLayerImplRecursive(CCLayerImpl* current)
{
    ASSERT(current);
    for (size_t i = 0; i < current->children().size(); ++i)
        clearRenderSurfacesOnCCLayerImplRecursive(current->children()[i].get());
    current->clearRenderSurface();
}

void CCLayerTreeHostImpl::clearRenderSurfaces()
{
    clearRenderSurfacesOnCCLayerImplRecursive(m_rootLayerImpl.get());
    m_renderSurfaceLayerList.clear();
}

String CCLayerTreeHostImpl::layerTreeAsText() const
{
    TextStream ts;
    if (m_rootLayerImpl) {
        ts << m_rootLayerImpl->layerTreeAsText();
        ts << "RenderSurfaces:\n";
        dumpRenderSurfaces(ts, 1, m_rootLayerImpl.get());
    }
    return ts.release();
}

void CCLayerTreeHostImpl::setFontAtlas(PassOwnPtr<CCFontAtlas> fontAtlas)
{
    m_headsUpDisplay->setFontAtlas(fontAtlas);
}

void CCLayerTreeHostImpl::dumpRenderSurfaces(TextStream& ts, int indent, const CCLayerImpl* layer) const
{
    if (layer->renderSurface())
        layer->renderSurface()->dumpSurface(ts, indent);

    for (size_t i = 0; i < layer->children().size(); ++i)
        dumpRenderSurfaces(ts, indent, layer->children()[i].get());
}


void CCLayerTreeHostImpl::animateGestures(double monotonicTime)
{
    if (!m_activeGestureAnimation)
        return;

    bool isContinuing = m_activeGestureAnimation->animate(monotonicTime);
    if (isContinuing)
        m_client->setNeedsRedrawOnImplThread();
    else
        m_activeGestureAnimation.clear();
}

} // namespace WebCore
