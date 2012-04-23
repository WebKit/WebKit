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
#include "cc/CCActiveGestureAnimation.h"
#include "cc/CCDamageTracker.h"
#include "cc/CCDebugRectHistory.h"
#include "cc/CCDelayBasedTimeSource.h"
#include "cc/CCFrameRateCounter.h"
#include "cc/CCGestureCurve.h"
#include "cc/CCLayerIterator.h"
#include "cc/CCLayerTreeHost.h"
#include "cc/CCLayerTreeHostCommon.h"
#include "cc/CCPageScaleAnimation.h"
#include "cc/CCRenderSurfaceDrawQuad.h"
#include "cc/CCThreadTask.h"
#include <wtf/CurrentTime.h>

namespace {
const double lowFrequencyAnimationInterval = 1;

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

PassOwnPtr<CCLayerTreeHostImpl> CCLayerTreeHostImpl::create(const CCSettings& settings, CCLayerTreeHostImplClient* client)
{
    return adoptPtr(new CCLayerTreeHostImpl(settings, client));
}

CCLayerTreeHostImpl::CCLayerTreeHostImpl(const CCSettings& settings, CCLayerTreeHostImplClient* client)
    : m_client(client)
    , m_sourceFrameNumber(-1)
    , m_frameNumber(0)
    , m_rootScrollLayerImpl(0)
    , m_currentlyScrollingLayerImpl(0)
    , m_settings(settings)
    , m_visible(true)
    , m_headsUpDisplay(CCHeadsUpDisplay::create())
    , m_pageScale(1)
    , m_pageScaleDelta(1)
    , m_sentPageScaleDelta(1)
    , m_minPageScale(0)
    , m_maxPageScale(0)
    , m_needsAnimateLayers(false)
    , m_pinchGestureActive(false)
    , m_timeSourceClientAdapter(CCLayerTreeHostImplTimeSourceAdapter::create(this, CCDelayBasedTimeSource::create(lowFrequencyAnimationInterval * 1000.0, CCProxy::currentThread())))
    , m_fpsCounter(CCFrameRateCounter::create())
    , m_debugRectHistory(CCDebugRectHistory::create())
{
    ASSERT(CCProxy::isImplThread());
    didVisibilityChange(this, m_visible);
}

CCLayerTreeHostImpl::~CCLayerTreeHostImpl()
{
    ASSERT(CCProxy::isImplThread());
    TRACE_EVENT("CCLayerTreeHostImpl::~CCLayerTreeHostImpl()", this, 0);

    if (m_rootLayerImpl)
        clearRenderSurfacesOnCCLayerImplRecursive(m_rootLayerImpl.get());
}

void CCLayerTreeHostImpl::beginCommit()
{
}

void CCLayerTreeHostImpl::commitComplete()
{
    // Recompute max scroll position; must be after layer content bounds are
    // updated.
    updateMaxScrollPosition();
}

bool CCLayerTreeHostImpl::canDraw()
{
    if (!m_rootLayerImpl)
        return false;
    if (viewportSize().isEmpty())
        return false;
    return true;
}

GraphicsContext3D* CCLayerTreeHostImpl::context()
{
    return m_layerRenderer ? m_layerRenderer->context() : 0;
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

static FloatRect damageInSurfaceSpace(CCLayerImpl* renderSurfaceLayer, const FloatRect& rootDamageRect)
{
    FloatRect surfaceDamageRect;
    // For now, we conservatively use the root damage as the damage for
    // all surfaces, except perspective transforms.
    const TransformationMatrix& screenSpaceTransform = renderSurfaceLayer->renderSurface()->screenSpaceTransform();
    if (screenSpaceTransform.hasPerspective()) {
        // Perspective projections do not play nice with mapRect of
        // inverse transforms. In this uncommon case, its simpler to
        // just redraw the entire surface.
        // FIXME: use calculateVisibleRect to handle projections.
        CCRenderSurface* renderSurface = renderSurfaceLayer->renderSurface();
        surfaceDamageRect = renderSurface->contentRect();
    } else {
        TransformationMatrix inverseScreenSpaceTransform = screenSpaceTransform.inverse();
        surfaceDamageRect = inverseScreenSpaceTransform.mapRect(rootDamageRect);
    }
    return surfaceDamageRect;
}

bool CCLayerTreeHostImpl::calculateRenderPasses(CCRenderPassList& passes, CCLayerList& renderSurfaceLayerList)
{
    ASSERT(passes.isEmpty());
    ASSERT(renderSurfaceLayerList.isEmpty());
    ASSERT(m_rootLayerImpl);

    renderSurfaceLayerList.append(m_rootLayerImpl.get());

    if (!m_rootLayerImpl->renderSurface())
        m_rootLayerImpl->createRenderSurface();
    m_rootLayerImpl->renderSurface()->clearLayerList();
    m_rootLayerImpl->renderSurface()->setContentRect(IntRect(IntPoint(), viewportSize()));

    m_rootLayerImpl->setClipRect(IntRect(IntPoint(), viewportSize()));

    {
        TransformationMatrix identityMatrix;
        TRACE_EVENT("CCLayerTreeHostImpl::calcDrawEtc", this, 0);
        CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(m_rootLayerImpl.get(), m_rootLayerImpl.get(), identityMatrix, identityMatrix, renderSurfaceLayerList, m_rootLayerImpl->renderSurface()->layerList(), &m_layerSorter, layerRendererCapabilities().maxTextureSize);
    }

    TRACE_EVENT1("webkit", "CCLayerTreeHostImpl::calculateRenderPasses", "renderSurfaceLayerList.size()", static_cast<long long unsigned>(renderSurfaceLayerList.size()));

    if (layerRendererCapabilities().usingPartialSwap || settings().showSurfaceDamageRects)
        trackDamageForAllSurfaces(m_rootLayerImpl.get(), renderSurfaceLayerList);
    m_rootDamageRect = m_rootLayerImpl->renderSurface()->damageTracker()->currentDamageRect();

    // Create the render passes in dependency order.
    HashMap<CCRenderSurface*, CCRenderPass*> surfacePassMap;
    for (int surfaceIndex = renderSurfaceLayerList.size() - 1; surfaceIndex >= 0 ; --surfaceIndex) {
        CCLayerImpl* renderSurfaceLayer = renderSurfaceLayerList[surfaceIndex];
        CCRenderSurface* renderSurface = renderSurfaceLayer->renderSurface();

        OwnPtr<CCRenderPass> pass = CCRenderPass::create(renderSurface);

        FloatRect surfaceDamageRect;
        if (layerRendererCapabilities().usingPartialSwap)
            surfaceDamageRect = damageInSurfaceSpace(renderSurfaceLayer, m_rootDamageRect);
        pass->setSurfaceDamageRect(surfaceDamageRect);

        surfacePassMap.add(renderSurface, pass.get());
        passes.append(pass.release());
    }

    IntRect scissorRect;
    if (layerRendererCapabilities().usingPartialSwap)
        scissorRect = enclosingIntRect(m_rootDamageRect);
    else
        scissorRect = IntRect(IntPoint(), viewportSize());

    bool recordMetricsForFrame = true; // FIXME: In the future, disable this when about:tracing is off.
    CCOcclusionTrackerImpl occlusionTracker(scissorRect, recordMetricsForFrame);

    // Add quads to the Render passes in FrontToBack order to allow for testing occlusion and performing culling during the tree walk.
    typedef CCLayerIterator<CCLayerImpl, Vector<CCLayerImpl*>, CCRenderSurface, CCLayerIteratorActions::FrontToBack> CCLayerIteratorType;

    // Typically when we are missing a texture and use a checkerboard quad, we still draw the frame. However when the layer being
    // checkerboarded is moving due to an impl-animation, we drop the frame to avoid flashing due to the texture suddenly appearing
    // in the future.
    bool drawFrame = true;

    CCLayerIteratorType end = CCLayerIteratorType::end(&renderSurfaceLayerList);
    for (CCLayerIteratorType it = CCLayerIteratorType::begin(&renderSurfaceLayerList); it != end; ++it) {
        CCRenderSurface* renderSurface = it.targetRenderSurfaceLayer()->renderSurface();
        CCRenderPass* pass = surfacePassMap.get(renderSurface);
        bool hadMissingTiles = false;

        occlusionTracker.enterLayer(it);

        if (it.representsContributingRenderSurface())
            pass->appendQuadsForRenderSurfaceLayer(*it, &occlusionTracker);
        else if (it.representsItself() && !it->visibleLayerRect().isEmpty()) {
            it->willDraw(m_layerRenderer.get());
            pass->appendQuadsForLayer(*it, &occlusionTracker, hadMissingTiles);
        }

        if (hadMissingTiles) {
            bool layerHasAnimatingTransform = it->screenSpaceTransformIsAnimating() || it->drawTransformIsAnimating();
            if (layerHasAnimatingTransform)
                drawFrame = false;
        }

        occlusionTracker.leaveLayer(it);
    }

    passes.last()->appendQuadsToFillScreen(m_rootLayerImpl.get(), m_backgroundColor, occlusionTracker);

    if (drawFrame)
        occlusionTracker.overdrawMetrics().recordMetrics(this);
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

IntSize CCLayerTreeHostImpl::contentSize() const
{
    // TODO(aelias): Hardcoding the first child here is weird. Think of
    // a cleaner way to get the contentBounds on the Impl side.
    if (!m_rootScrollLayerImpl || m_rootScrollLayerImpl->children().isEmpty())
        return IntSize();
    return m_rootScrollLayerImpl->children()[0]->contentBounds();
}

bool CCLayerTreeHostImpl::prepareToDraw(FrameData& frame)
{
    TRACE_EVENT("CCLayerTreeHostImpl::prepareToDraw", this, 0);

    frame.renderPasses.clear();
    frame.renderSurfaceLayerList.clear();
    m_mostRecentRenderSurfaceLayerList.clear();

    if (!m_rootLayerImpl)
        return false;

    if (!calculateRenderPasses(frame.renderPasses, frame.renderSurfaceLayerList)) {
        // We're missing textures on an animating layer. Request a commit.
        m_client->setNeedsCommitOnImplThread();
        return false;
    }

    m_mostRecentRenderSurfaceLayerList = frame.renderSurfaceLayerList;

    // If we return true, then we expect drawLayers() to be called before this function is called again.
    return true;
}

void CCLayerTreeHostImpl::drawLayers(const FrameData& frame)
{
    TRACE_EVENT("CCLayerTreeHostImpl::drawLayers", this, 0);
    ASSERT(m_layerRenderer);

    if (!m_rootLayerImpl)
        return;

    // FIXME: use the frame begin time from the overall compositor scheduler.
    // This value is currently inaccessible because it is up in Chromium's
    // RenderWidget.

    m_fpsCounter->markBeginningOfFrame(currentTime());
    m_layerRenderer->beginDrawingFrame(m_rootLayerImpl->renderSurface());

    for (size_t i = 0; i < frame.renderPasses.size(); ++i)
        m_layerRenderer->drawRenderPass(frame.renderPasses[i].get());

    typedef CCLayerIterator<CCLayerImpl, Vector<CCLayerImpl*>, CCRenderSurface, CCLayerIteratorActions::BackToFront> CCLayerIteratorType;

    CCLayerIteratorType end = CCLayerIteratorType::end(&frame.renderSurfaceLayerList);
    for (CCLayerIteratorType it = CCLayerIteratorType::begin(&frame.renderSurfaceLayerList); it != end; ++it) {
        if (it.representsItself() && !it->visibleLayerRect().isEmpty())
            it->didDraw();
    }

    if (m_debugRectHistory->enabled(settings()))
        m_debugRectHistory->saveDebugRectsForCurrentFrame(m_rootLayerImpl.get(), frame.renderSurfaceLayerList, settings());

    if (m_headsUpDisplay->enabled(settings()))
        m_headsUpDisplay->draw(this);

    m_layerRenderer->finishDrawingFrame();

    ++m_frameNumber;

    // The next frame should start by assuming nothing has changed, and changes are noted as they occur.
    m_rootLayerImpl->resetAllChangeTrackingForSubtree();
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
    return m_layerRenderer->swapBuffers(enclosingIntRect(m_rootDamageRect));
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

    if (!m_currentlyScrollingLayerImpl)
        return;

    if (m_rootLayerImpl && CCLayerTreeHostCommon::findLayerInSubtree(m_rootLayerImpl.get(), m_currentlyScrollingLayerImpl->id()))
        return;

    m_currentlyScrollingLayerImpl = 0;
}

PassOwnPtr<CCLayerImpl> CCLayerTreeHostImpl::releaseRootLayer()
{
    m_mostRecentRenderSurfaceLayerList.clear();
    return m_rootLayerImpl.release();
}

void CCLayerTreeHostImpl::setVisible(bool visible)
{
    if (m_visible == visible)
        return;
    m_visible = visible;
    didVisibilityChange(this, m_visible);

    if (!m_layerRenderer)
        return;

    m_layerRenderer->setVisible(visible);

    const bool shouldTickInBackground = !visible && m_needsAnimateLayers;
    m_timeSourceClientAdapter->setActive(shouldTickInBackground);
}

bool CCLayerTreeHostImpl::initializeLayerRenderer(PassRefPtr<GraphicsContext3D> context)
{
    OwnPtr<LayerRendererChromium> layerRenderer;
    layerRenderer = LayerRendererChromium::create(this, context);

    // Since we now have a new context/layerRenderer, we cannot continue to use the old
    // resources (i.e. renderSurfaces and texture IDs).
    if (m_rootLayerImpl) {
        clearRenderSurfacesOnCCLayerImplRecursive(m_rootLayerImpl.get());
        sendDidLoseContextRecursive(m_rootLayerImpl.get());
    }

    m_layerRenderer = layerRenderer.release();

    if (!m_visible && m_layerRenderer)
         m_layerRenderer->setVisible(m_visible);

    return m_layerRenderer;
}

void CCLayerTreeHostImpl::setViewportSize(const IntSize& viewportSize)
{
    if (viewportSize == m_viewportSize)
        return;

    m_viewportSize = viewportSize;
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

    IntSize maxScroll = contentSize() - expandedIntSize(viewBounds);
    // The viewport may be larger than the contents in some cases, such as
    // having a vertical scrollbar but no horizontal overflow.
    maxScroll.clampNegativeToZero();

    m_rootScrollLayerImpl->setMaxScrollPosition(maxScroll);
}

void CCLayerTreeHostImpl::setNeedsRedraw()
{
    m_client->setNeedsRedrawOnImplThread();
}

bool CCLayerTreeHostImpl::ensureMostRecentRenderSurfaceLayerList()
{
    if (m_mostRecentRenderSurfaceLayerList.size())
        return true;

    if (!m_rootLayerImpl)
        return false;

    // If we are called after setRootLayer() but before prepareToDraw(), we need to recalculate
    // the visible layers. The return value is ignored because we don't care about checkerboarding.
    CCRenderPassList passes;
    calculateRenderPasses(passes, m_mostRecentRenderSurfaceLayerList);

    return m_mostRecentRenderSurfaceLayerList.size();
}

CCInputHandlerClient::ScrollStatus CCLayerTreeHostImpl::scrollBegin(const IntPoint& viewportPoint, CCInputHandlerClient::ScrollInputType type)
{
    TRACE_EVENT("CCLayerTreeHostImpl::scrollBegin", this, 0);

    m_currentlyScrollingLayerImpl = 0;

    if (!ensureMostRecentRenderSurfaceLayerList())
        return ScrollIgnored;

    // First find out which layer was hit by walking the saved list of visible layers from the most recent frame.
    CCLayerImpl* layerImpl = 0;
    typedef CCLayerIterator<CCLayerImpl, CCLayerList, CCRenderSurface, CCLayerIteratorActions::FrontToBack> CCLayerIteratorType;
    CCLayerIteratorType end = CCLayerIteratorType::end(&m_mostRecentRenderSurfaceLayerList);
    for (CCLayerIteratorType it = CCLayerIteratorType::begin(&m_mostRecentRenderSurfaceLayerList); it != end; ++it) {
        CCLayerImpl* renderSurfaceLayerImpl = (*it);
        if (!renderSurfaceLayerImpl->screenSpaceTransform().isInvertible())
            continue;
        IntPoint contentPoint(renderSurfaceLayerImpl->screenSpaceTransform().inverse().mapPoint(viewportPoint));
        if (!renderSurfaceLayerImpl->visibleLayerRect().contains(contentPoint))
            continue;
        layerImpl = renderSurfaceLayerImpl;
        break;
    }

    // Walk up the hierarchy and look for a scrollable layer.
    CCLayerImpl* potentiallyScrollingLayerImpl = 0;
    for (; layerImpl; layerImpl = layerImpl->parent()) {
        // The content layer can also block attempts to scroll outside the main thread.
        if (layerImpl->tryScroll(viewportPoint, type) == ScrollFailed)
            return ScrollFailed;

        CCLayerImpl* scrollLayerImpl = findScrollLayerForContentLayer(layerImpl);
        if (!scrollLayerImpl)
            continue;

        ScrollStatus status = scrollLayerImpl->tryScroll(viewportPoint, type);

        // If any layer wants to divert the scroll event to the main thread, abort.
        if (status == ScrollFailed)
            return ScrollFailed;

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
    TRACE_EVENT("CCLayerTreeHostImpl::scrollBy", this, 0);
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

    if (pendingDelta != scrollDelta) {
        m_client->setNeedsCommitOnImplThread();
        m_client->setNeedsRedrawOnImplThread();
    }
}

void CCLayerTreeHostImpl::scrollEnd()
{
    m_currentlyScrollingLayerImpl = 0;
}

void CCLayerTreeHostImpl::pinchGestureBegin()
{
    m_pinchGestureActive = true;
    m_previousPinchAnchor = IntPoint();
}

void CCLayerTreeHostImpl::pinchGestureUpdate(float magnifyDelta,
                                             const IntPoint& anchor)
{
    TRACE_EVENT("CCLayerTreeHostImpl::pinchGestureUpdate", this, 0);

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
    scrollEnd = scrollEnd.shrunkTo(roundedIntSize(scaledContentsSize - m_viewportSize)).expandedTo(FloatSize(0, 0));
    scrollEnd.scale(1 / pageScaleDeltaToSend);

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
    collectScrollDeltas(scrollInfo.get(), m_rootLayerImpl.get());

    bool didMove = scrollInfo->scrolls.size() || m_pageScaleDelta != 1;
    if (!didMove || m_pinchGestureActive || m_pageScaleAnimation) {
        m_sentPageScaleDelta = scrollInfo->pageScaleDelta = 1;
        if (m_pinchGestureActive)
            computePinchZoomDeltas(scrollInfo.get());
        else if (m_pageScaleAnimation.get())
            computeDoubleTapZoomDeltas(scrollInfo.get());
        return scrollInfo.release();
    }

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
    if (!m_settings.threadedAnimationEnabled || !m_needsAnimateLayers || !m_rootLayerImpl)
        return;

    TRACE_EVENT("CCLayerTreeHostImpl::animateLayers", this, 0);

    OwnPtr<CCAnimationEventsVector> events(adoptPtr(new CCAnimationEventsVector));

    bool didAnimate = false;
    animateLayersRecursive(m_rootLayerImpl.get(), monotonicTime, wallClockTime, events.get(), didAnimate, m_needsAnimateLayers);

    if (!events->isEmpty())
        m_client->postAnimationEventsToMainThreadOnImplThread(events.release(), wallClockTime);

    if (didAnimate)
        m_client->setNeedsRedrawOnImplThread();

    const bool shouldTickInBackground = m_needsAnimateLayers && !m_visible;
    m_timeSourceClientAdapter->setActive(shouldTickInBackground);
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

void CCLayerTreeHostImpl::clearRenderSurfacesOnCCLayerImplRecursive(CCLayerImpl* current)
{
    ASSERT(current);
    for (size_t i = 0; i < current->children().size(); ++i)
        clearRenderSurfacesOnCCLayerImplRecursive(current->children()[i].get());
    current->clearRenderSurface();
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
