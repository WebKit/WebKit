/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "TiledCoreAnimationDrawingArea.h"

#if !PLATFORM(IOS)

#import "ColorSpaceData.h"
#import "DrawingAreaProxyMessages.h"
#import "LayerHostingContext.h"
#import "LayerTreeContext.h"
#import "ViewGestureControllerMessages.h"
#import "WebFrame.h"
#import "WebPage.h"
#import "WebPageCreationParameters.h"
#import "WebPageProxyMessages.h"
#import "WebProcess.h"
#import <QuartzCore/QuartzCore.h>
#import <WebCore/FrameView.h>
#import <WebCore/GraphicsContext.h>
#import <WebCore/GraphicsLayerCA.h>
#import <WebCore/MainFrame.h>
#import <WebCore/Page.h>
#import <WebCore/RenderLayerBacking.h>
#import <WebCore/RenderLayerCompositor.h>
#import <WebCore/RenderView.h>
#import <WebCore/ScrollbarTheme.h>
#import <WebCore/Settings.h>
#import <WebCore/TiledBacking.h>
#import <wtf/MainThread.h>

#if ENABLE(ASYNC_SCROLLING)
#import <WebCore/AsyncScrollingCoordinator.h>
#import <WebCore/ScrollingThread.h>
#import <WebCore/ScrollingTree.h>
#endif

@interface CATransaction (Details)
+ (void)synchronize;
@end

using namespace WebCore;

namespace WebKit {

TiledCoreAnimationDrawingArea::TiledCoreAnimationDrawingArea(WebPage* webPage, const WebPageCreationParameters& parameters)
    : DrawingArea(DrawingAreaTypeTiledCoreAnimation, webPage)
    , m_layerTreeStateIsFrozen(false)
    , m_layerFlushScheduler(this)
    , m_isPaintingSuspended(!(parameters.viewState & ViewState::IsVisible))
    , m_exposedRect(FloatRect::infiniteRect())
    , m_scrolledExposedRect(FloatRect::infiniteRect())
    , m_updateIntrinsicContentSizeTimer(this, &TiledCoreAnimationDrawingArea::updateIntrinsicContentSizeTimerFired)
    , m_transientZoomScale(1)
{
    m_webPage->corePage()->settings().setForceCompositingMode(true);

    m_hostingLayer = [CALayer layer];
    [m_hostingLayer setFrame:m_webPage->bounds()];
    [m_hostingLayer setOpaque:YES];
    [m_hostingLayer setGeometryFlipped:YES];

    updateLayerHostingContext();
    setColorSpace(parameters.colorSpace);

    LayerTreeContext layerTreeContext;
    layerTreeContext.contextID = m_layerHostingContext->contextID();
    m_webPage->send(Messages::DrawingAreaProxy::EnterAcceleratedCompositingMode(0, layerTreeContext));
}

TiledCoreAnimationDrawingArea::~TiledCoreAnimationDrawingArea()
{
    m_layerFlushScheduler.invalidate();
}

void TiledCoreAnimationDrawingArea::setNeedsDisplay()
{
}

void TiledCoreAnimationDrawingArea::setNeedsDisplayInRect(const IntRect& rect)
{
}

void TiledCoreAnimationDrawingArea::scroll(const IntRect& scrollRect, const IntSize& scrollDelta)
{
    updateScrolledExposedRect();
}

void TiledCoreAnimationDrawingArea::invalidateAllPageOverlays()
{
    for (PageOverlayLayerMap::iterator it = m_pageOverlayLayers.begin(), end = m_pageOverlayLayers.end(); it != end; ++it)
        it->value->setNeedsDisplay();
}

void TiledCoreAnimationDrawingArea::didChangeScrollOffsetForAnyFrame()
{
    invalidateAllPageOverlays();
}

void TiledCoreAnimationDrawingArea::setRootCompositingLayer(GraphicsLayer* graphicsLayer)
{
    CALayer *rootLayer = graphicsLayer ? graphicsLayer->platformLayer() : nil;

    if (m_layerTreeStateIsFrozen) {
        m_pendingRootLayer = rootLayer;
        return;
    }

    setRootCompositingLayer(rootLayer);
}

void TiledCoreAnimationDrawingArea::forceRepaint()
{
    if (m_layerTreeStateIsFrozen)
        return;

    for (Frame* frame = &m_webPage->corePage()->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        FrameView* frameView = frame->view();
        if (!frameView || !frameView->tiledBacking())
            continue;

        frameView->tiledBacking()->forceRepaint();
    }

    flushLayers();
    [CATransaction flush];
    [CATransaction synchronize];
}

bool TiledCoreAnimationDrawingArea::forceRepaintAsync(uint64_t callbackID)
{
    if (m_layerTreeStateIsFrozen)
        return false;

    dispatchAfterEnsuringUpdatedScrollPosition(bind(^{
        m_webPage->drawingArea()->forceRepaint();
        m_webPage->send(Messages::WebPageProxy::VoidCallback(callbackID));
    }));
    return true;
}

void TiledCoreAnimationDrawingArea::setLayerTreeStateIsFrozen(bool layerTreeStateIsFrozen)
{
    if (m_layerTreeStateIsFrozen == layerTreeStateIsFrozen)
        return;

    m_layerTreeStateIsFrozen = layerTreeStateIsFrozen;
    if (m_layerTreeStateIsFrozen)
        m_layerFlushScheduler.suspend();
    else
        m_layerFlushScheduler.resume();
}

bool TiledCoreAnimationDrawingArea::layerTreeStateIsFrozen() const
{
    return m_layerTreeStateIsFrozen;
}

void TiledCoreAnimationDrawingArea::scheduleCompositingLayerFlush()
{
    m_layerFlushScheduler.schedule();
}

void TiledCoreAnimationDrawingArea::didInstallPageOverlay(PageOverlay* pageOverlay)
{
#if ENABLE(ASYNC_SCROLLING)
    if (ScrollingCoordinator* scrollingCoordinator = m_webPage->corePage()->scrollingCoordinator())
        scrollingCoordinator->setForceSynchronousScrollLayerPositionUpdates(true);
#endif

    createPageOverlayLayer(pageOverlay);
}

void TiledCoreAnimationDrawingArea::didUninstallPageOverlay(PageOverlay* pageOverlay)
{
    destroyPageOverlayLayer(pageOverlay);
    scheduleCompositingLayerFlush();

    if (m_pageOverlayLayers.size())
        return;

#if ENABLE(ASYNC_SCROLLING)
    if (Page* page = m_webPage->corePage()) {
        if (ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator())
            scrollingCoordinator->setForceSynchronousScrollLayerPositionUpdates(false);
    }
#endif
}

void TiledCoreAnimationDrawingArea::setPageOverlayNeedsDisplay(PageOverlay* pageOverlay, const IntRect& rect)
{
    GraphicsLayer* layer = m_pageOverlayLayers.get(pageOverlay);

    if (!layer)
        return;

    if (!layer->drawsContent()) {
        layer->setDrawsContent(true);
        layer->setSize(expandedIntSize(FloatSize(m_hostingLayer.get().frame.size)));
    }

    layer->setNeedsDisplayInRect(rect);
    scheduleCompositingLayerFlush();
}

void TiledCoreAnimationDrawingArea::setPageOverlayOpacity(PageOverlay* pageOverlay, float opacity)
{
    GraphicsLayer* layer = m_pageOverlayLayers.get(pageOverlay);

    if (!layer)
        return;

    layer->setOpacity(opacity);
    scheduleCompositingLayerFlush();
}

void TiledCoreAnimationDrawingArea::clearPageOverlay(PageOverlay* pageOverlay)
{
    GraphicsLayer* layer = m_pageOverlayLayers.get(pageOverlay);

    if (!layer)
        return;

    layer->setDrawsContent(false);
    layer->setSize(IntSize());
    scheduleCompositingLayerFlush();
}

void TiledCoreAnimationDrawingArea::updatePreferences(const WebPreferencesStore&)
{
    Settings& settings = m_webPage->corePage()->settings();

#if ENABLE(ASYNC_SCROLLING)
    if (AsyncScrollingCoordinator* scrollingCoordinator = toAsyncScrollingCoordinator(m_webPage->corePage()->scrollingCoordinator())) {
        bool scrollingPerformanceLoggingEnabled = m_webPage->scrollingPerformanceLoggingEnabled();
        ScrollingThread::dispatch(bind(&ScrollingTree::setScrollingPerformanceLoggingEnabled, scrollingCoordinator->scrollingTree(), scrollingPerformanceLoggingEnabled));
    }
#endif

    if (TiledBacking* tiledBacking = mainFrameTiledBacking())
        tiledBacking->setAggressivelyRetainsTiles(settings.aggressiveTileRetentionEnabled());

    for (PageOverlayLayerMap::iterator it = m_pageOverlayLayers.begin(), end = m_pageOverlayLayers.end(); it != end; ++it) {
        it->value->setAcceleratesDrawing(settings.acceleratedDrawingEnabled());
        it->value->setShowDebugBorder(settings.showDebugBorders());
        it->value->setShowRepaintCounter(settings.showRepaintCounter());
    }

    // Fixed position elements need to be composited and create stacking contexts
    // in order to be scrolled by the ScrollingCoordinator. We also want to keep
    // Settings:setFixedPositionCreatesStackingContext() enabled for iOS. See
    // <rdar://problem/9813262> for more details.
    settings.setAcceleratedCompositingForFixedPositionEnabled(true);
    settings.setFixedPositionCreatesStackingContext(true);

    bool showTiledScrollingIndicator = settings.showTiledScrollingIndicator();
    if (showTiledScrollingIndicator == !!m_debugInfoLayer)
        return;

    updateDebugInfoLayer(showTiledScrollingIndicator);
}

void TiledCoreAnimationDrawingArea::mainFrameContentSizeChanged(const IntSize&)
{
    if (!m_webPage->minimumLayoutSize().width())
        return;

    if (m_inUpdateGeometry)
        return;

    if (!m_updateIntrinsicContentSizeTimer.isActive())
        m_updateIntrinsicContentSizeTimer.startOneShot(0);
}

void TiledCoreAnimationDrawingArea::updateIntrinsicContentSizeTimerFired(Timer<TiledCoreAnimationDrawingArea>*)
{
    FrameView* frameView = m_webPage->corePage()->mainFrame().view();
    if (!frameView)
        return;

    IntSize contentSize = frameView->autoSizingIntrinsicContentSize();

    if (m_lastSentIntrinsicContentSize == contentSize)
        return;

    m_lastSentIntrinsicContentSize = contentSize;
    m_webPage->send(Messages::DrawingAreaProxy::IntrinsicContentSizeDidChange(contentSize));
}

void TiledCoreAnimationDrawingArea::dispatchAfterEnsuringUpdatedScrollPosition(std::function<void ()> function)
{
#if ENABLE(ASYNC_SCROLLING)
    if (!m_webPage->corePage()->scrollingCoordinator()) {
        function();
        return;
    }

    m_webPage->ref();
    m_webPage->corePage()->scrollingCoordinator()->commitTreeStateIfNeeded();

    if (!m_layerTreeStateIsFrozen)
        m_layerFlushScheduler.suspend();

    // It is possible for the drawing area to be destroyed before the bound block
    // is invoked, so grab a reference to the web page here so we can access the drawing area through it.
    // (The web page is already kept alive by dispatchAfterEnsuringUpdatedScrollPosition).
    WebPage* webPage = m_webPage;

    ScrollingThread::dispatchBarrier(bind(^{
        DrawingArea* drawingArea = webPage->drawingArea();
        if (!drawingArea)
            return;

        function();

        if (!m_layerTreeStateIsFrozen)
            m_layerFlushScheduler.resume();

        webPage->deref();
    }));
#else
    function();
#endif
}

void TiledCoreAnimationDrawingArea::notifyAnimationStarted(const GraphicsLayer*, double)
{
}

void TiledCoreAnimationDrawingArea::notifyFlushRequired(const GraphicsLayer*)
{
}

void TiledCoreAnimationDrawingArea::paintContents(const GraphicsLayer* graphicsLayer, GraphicsContext& graphicsContext, GraphicsLayerPaintingPhase, const FloatRect& clipRect)
{
    for (auto it = m_pageOverlayLayers.begin(), end = m_pageOverlayLayers.end(); it != end; ++it) {
        if (it->value.get() == graphicsLayer) {
            m_webPage->drawPageOverlay(it->key, graphicsContext, enclosingIntRect(clipRect));
            break;
        }
    }
}

float TiledCoreAnimationDrawingArea::deviceScaleFactor() const
{
    return m_webPage->corePage()->deviceScaleFactor();
}

bool TiledCoreAnimationDrawingArea::flushLayers()
{
    ASSERT(!m_layerTreeStateIsFrozen);

    // This gets called outside of the normal event loop so wrap in an autorelease pool
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    m_webPage->layoutIfNeeded();

    if (m_pendingRootLayer) {
        setRootCompositingLayer(m_pendingRootLayer.get());
        m_pendingRootLayer = nullptr;
    }

    FloatRect visibleRect = [m_hostingLayer frame];
    visibleRect.intersect(m_scrolledExposedRect);

    for (PageOverlayLayerMap::iterator it = m_pageOverlayLayers.begin(), end = m_pageOverlayLayers.end(); it != end; ++it) {
        GraphicsLayer* layer = it->value.get();
        layer->flushCompositingState(visibleRect);
    }

    bool returnValue = m_webPage->corePage()->mainFrame().view()->flushCompositingStateIncludingSubframes();

    [pool drain];
    return returnValue;
}

void TiledCoreAnimationDrawingArea::viewStateDidChange(ViewState::Flags changed)
{
    if (changed & ViewState::IsVisible) {
        if (m_webPage->isVisible())
            resumePainting();
        else
            suspendPainting();
    }
}

void TiledCoreAnimationDrawingArea::suspendPainting()
{
    ASSERT(!m_isPaintingSuspended);
    m_isPaintingSuspended = true;

    [m_hostingLayer setValue:@YES forKey:@"NSCAViewRenderPaused"];
    [[NSNotificationCenter defaultCenter] postNotificationName:@"NSCAViewRenderDidPauseNotification" object:nil userInfo:[NSDictionary dictionaryWithObject:m_hostingLayer.get() forKey:@"layer"]];
}

void TiledCoreAnimationDrawingArea::resumePainting()
{
    if (!m_isPaintingSuspended) {
        // FIXME: We can get a call to resumePainting when painting is not suspended.
        // This happens when sending a synchronous message to create a new page. See <rdar://problem/8976531>.
        return;
    }
    m_isPaintingSuspended = false;

    [m_hostingLayer setValue:@NO forKey:@"NSCAViewRenderPaused"];
    [[NSNotificationCenter defaultCenter] postNotificationName:@"NSCAViewRenderDidResumeNotification" object:nil userInfo:[NSDictionary dictionaryWithObject:m_hostingLayer.get() forKey:@"layer"]];
}

void TiledCoreAnimationDrawingArea::setExposedRect(const FloatRect& exposedRect)
{
    m_exposedRect = exposedRect;
    updateScrolledExposedRect();
}

void TiledCoreAnimationDrawingArea::updateScrolledExposedRect()
{
    FrameView* frameView = m_webPage->corePage()->mainFrame().view();
    if (!frameView)
        return;

    m_scrolledExposedRect = m_exposedRect;

#if !PLATFORM(IOS)
    if (!m_exposedRect.isInfinite()) {
        IntPoint scrollPositionWithOrigin = frameView->scrollPosition() + toIntSize(frameView->scrollOrigin());
        m_scrolledExposedRect.moveBy(scrollPositionWithOrigin);
    }
#endif

    frameView->setExposedRect(m_scrolledExposedRect);

    for (const auto& layer : m_pageOverlayLayers.values()) {
        if (TiledBacking* tiledBacking = layer->tiledBacking())
            tiledBacking->setExposedRect(m_scrolledExposedRect);
    }

    frameView->adjustTiledBackingCoverage();
}

void TiledCoreAnimationDrawingArea::updateGeometry(const IntSize& viewSize, const IntSize& layerPosition)
{
    m_inUpdateGeometry = true;

    IntSize size = viewSize;
    IntSize contentSize = IntSize(-1, -1);

    if (!m_webPage->minimumLayoutSize().width() || m_webPage->autoSizingShouldExpandToViewHeight())
        m_webPage->setSize(size);

    FrameView* frameView = m_webPage->mainFrameView();

    if (m_webPage->autoSizingShouldExpandToViewHeight() && frameView)
        frameView->setAutoSizeFixedMinimumHeight(viewSize.height());

    m_webPage->layoutIfNeeded();

    if (m_webPage->minimumLayoutSize().width() && frameView) {
        contentSize = frameView->autoSizingIntrinsicContentSize();
        size = contentSize;
    }

    for (PageOverlayLayerMap::iterator it = m_pageOverlayLayers.begin(), end = m_pageOverlayLayers.end(); it != end; ++it) {
        GraphicsLayer* layer = it->value.get();
        if (layer->drawsContent())
            layer->setSize(viewSize);
    }

    if (!m_layerTreeStateIsFrozen)
        flushLayers();

    invalidateAllPageOverlays();

    [CATransaction begin];
    [CATransaction setDisableActions:YES];

    [m_hostingLayer setFrame:CGRectMake(layerPosition.width(), layerPosition.height(), viewSize.width(), viewSize.height())];

    [CATransaction commit];
    
    [CATransaction flush];
    [CATransaction synchronize];

    m_webPage->send(Messages::DrawingAreaProxy::DidUpdateGeometry());

    if (m_webPage->minimumLayoutSize().width() && !m_updateIntrinsicContentSizeTimer.isActive())
        m_updateIntrinsicContentSizeTimer.startOneShot(0);

    m_inUpdateGeometry = false;
}

void TiledCoreAnimationDrawingArea::setDeviceScaleFactor(float deviceScaleFactor)
{
    m_webPage->setDeviceScaleFactor(deviceScaleFactor);

    for (PageOverlayLayerMap::iterator it = m_pageOverlayLayers.begin(), end = m_pageOverlayLayers.end(); it != end; ++it)
        it->value->noteDeviceOrPageScaleFactorChangedIncludingDescendants();
}

void TiledCoreAnimationDrawingArea::setLayerHostingMode(LayerHostingMode)
{
    updateLayerHostingContext();

    // Finally, inform the UIProcess that the context has changed.
    LayerTreeContext layerTreeContext;
    layerTreeContext.contextID = m_layerHostingContext->contextID();
    m_webPage->send(Messages::DrawingAreaProxy::UpdateAcceleratedCompositingMode(0, layerTreeContext));
}

void TiledCoreAnimationDrawingArea::setColorSpace(const ColorSpaceData& colorSpace)
{
    m_layerHostingContext->setColorSpace(colorSpace.cgColorSpace.get());
}

void TiledCoreAnimationDrawingArea::updateLayerHostingContext()
{
    RetainPtr<CGColorSpaceRef> colorSpace;

    // Invalidate the old context.
    if (m_layerHostingContext) {
        colorSpace = m_layerHostingContext->colorSpace();
        m_layerHostingContext->invalidate();
        m_layerHostingContext = nullptr;
    }

    // Create a new context and set it up.
    switch (m_webPage->layerHostingMode()) {
    case LayerHostingMode::InProcess:
        m_layerHostingContext = LayerHostingContext::createForPort(WebProcess::shared().compositingRenderServerPort());
        break;
#if HAVE(OUT_OF_PROCESS_LAYER_HOSTING)
    case LayerHostingMode::OutOfProcess:
        m_layerHostingContext = LayerHostingContext::createForExternalHostingProcess();
        break;
#endif
    }

    if (m_rootLayer)
        m_layerHostingContext->setRootLayer(m_hostingLayer.get());

    if (colorSpace)
        m_layerHostingContext->setColorSpace(colorSpace.get());
}

void TiledCoreAnimationDrawingArea::setRootCompositingLayer(CALayer *layer)
{
    ASSERT(!m_layerTreeStateIsFrozen);

    [CATransaction begin];
    [CATransaction setDisableActions:YES];

    [m_hostingLayer setSublayers:layer ? @[ layer ] : @[ ]];

    bool hadRootLayer = !!m_rootLayer;
    m_rootLayer = layer;
    [m_rootLayer setSublayerTransform:m_transform];

    if (hadRootLayer != !!layer)
        m_layerHostingContext->setRootLayer(layer ? m_hostingLayer.get() : 0);

    for (PageOverlayLayerMap::iterator it = m_pageOverlayLayers.begin(), end = m_pageOverlayLayers.end(); it != end; ++it)
        [m_hostingLayer addSublayer:it->value->platformLayer()];

    if (TiledBacking* tiledBacking = mainFrameTiledBacking())
        tiledBacking->setAggressivelyRetainsTiles(m_webPage->corePage()->settings().aggressiveTileRetentionEnabled());

    updateDebugInfoLayer(m_webPage->corePage()->settings().showTiledScrollingIndicator());

    [CATransaction commit];
}

void TiledCoreAnimationDrawingArea::createPageOverlayLayer(PageOverlay* pageOverlay)
{
    std::unique_ptr<GraphicsLayer> layer = GraphicsLayer::create(graphicsLayerFactory(), this);
#ifndef NDEBUG
    layer->setName("page overlay content");
#endif

    layer->setAcceleratesDrawing(m_webPage->corePage()->settings().acceleratedDrawingEnabled());
    layer->setShowDebugBorder(m_webPage->corePage()->settings().showDebugBorders());
    layer->setShowRepaintCounter(m_webPage->corePage()->settings().showRepaintCounter());

    m_pageOverlayPlatformLayers.set(layer.get(), layer->platformLayer());

    [CATransaction begin];
    [CATransaction setDisableActions:YES];

    [m_hostingLayer addSublayer:layer->platformLayer()];

    [CATransaction commit];

    m_pageOverlayLayers.add(pageOverlay, std::move(layer));
}

void TiledCoreAnimationDrawingArea::destroyPageOverlayLayer(PageOverlay* pageOverlay)
{
    std::unique_ptr<GraphicsLayer> layer = m_pageOverlayLayers.take(pageOverlay);
    ASSERT(layer);

    [CATransaction begin];
    [CATransaction setDisableActions:YES];

    [layer->platformLayer() removeFromSuperlayer];

    [CATransaction commit];

    m_pageOverlayPlatformLayers.remove(layer.get());
}

void TiledCoreAnimationDrawingArea::didCommitChangesForLayer(const GraphicsLayer* layer) const
{
    RetainPtr<CALayer> oldPlatformLayer = m_pageOverlayPlatformLayers.get(layer);

    if (!oldPlatformLayer)
        return;

    if (oldPlatformLayer.get() == layer->platformLayer())
        return;

    [CATransaction begin];
    [CATransaction setDisableActions:YES];

    [m_hostingLayer insertSublayer:layer->platformLayer() above:oldPlatformLayer.get()];
    [oldPlatformLayer removeFromSuperlayer];

    [CATransaction commit];

    m_pageOverlayPlatformLayers.set(layer, layer->platformLayer());
}

TiledBacking* TiledCoreAnimationDrawingArea::mainFrameTiledBacking() const
{
    FrameView* frameView = m_webPage->corePage()->mainFrame().view();
    return frameView ? frameView->tiledBacking() : 0;
}

void TiledCoreAnimationDrawingArea::updateDebugInfoLayer(bool showLayer)
{
    if (showLayer) {
        if (TiledBacking* tiledBacking = mainFrameTiledBacking()) {
            if (PlatformCALayer* indicatorLayer = tiledBacking->tiledScrollingIndicatorLayer())
                m_debugInfoLayer = indicatorLayer->platformLayer();
        }

        if (m_debugInfoLayer) {
#ifndef NDEBUG
            [m_debugInfoLayer setName:@"Debug Info"];
#endif
            [m_hostingLayer addSublayer:m_debugInfoLayer.get()];
        }
    } else if (m_debugInfoLayer) {
        [m_debugInfoLayer removeFromSuperlayer];
        m_debugInfoLayer = nullptr;
    }
}

bool TiledCoreAnimationDrawingArea::shouldUseTiledBackingForFrameView(const FrameView* frameView)
{
    return frameView && frameView->frame().isMainFrame();
}

void TiledCoreAnimationDrawingArea::adjustTransientZoom(double scale, FloatPoint origin)
{
    // FIXME: Scrollbars should stay in-place and change height while zooming.
    // FIXME: Keep around pageScale=1 tiles so we can zoom out without gaps.
    // FIXME: Bring in unparented-but-painted tiles when zooming out, to fill in any gaps.

    if (!m_hostingLayer)
        return;

    TransformationMatrix transform;
    transform.translate(origin.x(), origin.y());
    transform.scale(scale);

    RenderView* renderView = m_webPage->mainFrameView()->renderView();
    PlatformCALayer* renderViewLayer = toGraphicsLayerCA(renderView->layer()->backing()->graphicsLayer())->platformCALayer();
    renderViewLayer->setTransform(transform);
    renderViewLayer->setAnchorPoint(FloatPoint3D());
    renderViewLayer->setPosition(FloatPoint3D());

    GraphicsLayerCA* shadowGraphicsLayer = toGraphicsLayerCA(renderView->compositor().layerForContentShadow());
    if (shadowGraphicsLayer) {
        PlatformCALayer* shadowLayer = shadowGraphicsLayer->platformCALayer();

        FloatRect shadowBounds = FloatRect(FloatPoint(), toFloatSize(renderView->layoutOverflowRect().maxXMaxYCorner()));
        shadowBounds.scale(scale);

        shadowLayer->setBounds(shadowBounds);
        shadowLayer->setPosition(origin + shadowBounds.center());
    }

    m_transientZoomScale = scale;
    m_transientZoomOrigin = origin;
}

static RetainPtr<CABasicAnimation> transientZoomSnapAnimationForKeyPath(String keyPath)
{
    const float transientZoomSnapBackDuration = 0.25;

    RetainPtr<CABasicAnimation> animation = [CABasicAnimation animationWithKeyPath:keyPath];
    [animation setDuration:transientZoomSnapBackDuration];
    [animation setFillMode:kCAFillModeForwards];
    [animation setRemovedOnCompletion:false];
    [animation setTimingFunction:[CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseInEaseOut]];

    return animation;
}

void TiledCoreAnimationDrawingArea::commitTransientZoom(double scale, FloatPoint origin)
{
    FrameView* frameView = m_webPage->mainFrameView();
    RenderView* renderView = frameView->renderView();
    PlatformCALayer* renderViewLayer = static_cast<GraphicsLayerCA*>(renderView->layer()->backing()->graphicsLayer())->platformCALayer();

    FloatRect visibleContentRect = frameView->visibleContentRectIncludingScrollbars();

    FloatPoint constrainedOrigin = visibleContentRect.location();
    constrainedOrigin.moveBy(-origin);

    IntRect documentRect = frameView->renderView()->unscaledDocumentRect();
    documentRect.scale(scale);

    // Scaling may have exposed the overhang area, so we need to constrain the final
    // layer position exactly like scrolling will once it's committed, to ensure that
    // scrolling doesn't make the view jump.
    constrainedOrigin = ScrollableArea::constrainScrollPositionForOverhang(roundedIntRect(visibleContentRect), documentRect.size(), roundedIntPoint(constrainedOrigin), IntPoint(), 0, 0);
    constrainedOrigin.moveBy(-visibleContentRect.location());
    constrainedOrigin = -constrainedOrigin;

    if (m_transientZoomScale == scale && roundedIntPoint(m_transientZoomOrigin) == roundedIntPoint(constrainedOrigin)) {
        // We're already at the right scale and position, so we don't need to animate.
        applyTransientZoomToPage(scale, origin);
        return;
    }

    TransformationMatrix transform;
    transform.translate(constrainedOrigin.x(), constrainedOrigin.y());
    transform.scale(scale);

    RetainPtr<CABasicAnimation> renderViewAnimationCA = transientZoomSnapAnimationForKeyPath("transform");
    RefPtr<PlatformCAAnimation> renderViewAnimation = PlatformCAAnimation::create(renderViewAnimationCA.get());
    renderViewAnimation->setToValue(transform);

    RetainPtr<CALayer> shadowLayer;
    if (GraphicsLayerCA* shadowGraphicsLayer = toGraphicsLayerCA(renderView->compositor().layerForContentShadow()))
        shadowLayer = shadowGraphicsLayer->platformCALayer()->platformLayer();

    [CATransaction begin];
    [CATransaction setCompletionBlock:^(void) {
        renderViewLayer->removeAnimationForKey("transientZoomCommit");
        if (shadowLayer)
            [shadowLayer removeAllAnimations];
        applyTransientZoomToPage(scale, origin);
    }];

    renderViewLayer->addAnimationForKey("transientZoomCommit", renderViewAnimation.get());

    if (shadowLayer) {
        FloatRect shadowBounds = FloatRect(FloatPoint(), toFloatSize(renderView->layoutOverflowRect().maxXMaxYCorner()));
        shadowBounds.scale(scale);
        RetainPtr<CGPathRef> shadowPath = adoptCF(CGPathCreateWithRect(shadowBounds, NULL)).get();

        RetainPtr<CABasicAnimation> shadowBoundsAnimation = transientZoomSnapAnimationForKeyPath("bounds");
        [shadowBoundsAnimation setToValue:[NSValue valueWithRect:shadowBounds]];
        RetainPtr<CABasicAnimation> shadowPositionAnimation = transientZoomSnapAnimationForKeyPath("position");
        [shadowPositionAnimation setToValue:[NSValue valueWithPoint:constrainedOrigin + shadowBounds.center()]];
        RetainPtr<CABasicAnimation> shadowPathAnimation = transientZoomSnapAnimationForKeyPath("shadowPath");
        [shadowPathAnimation setToValue:(id)shadowPath.get()];

        [shadowLayer addAnimation:shadowBoundsAnimation.get() forKey:@"transientZoomCommitShadowBounds"];
        [shadowLayer addAnimation:shadowPositionAnimation.get() forKey:@"transientZoomCommitShadowPosition"];
        [shadowLayer addAnimation:shadowPathAnimation.get() forKey:@"transientZoomCommitShadowPath"];
    }

    [CATransaction commit];
}

void TiledCoreAnimationDrawingArea::applyTransientZoomToPage(double scale, FloatPoint origin)
{
    RenderView* renderView = m_webPage->mainFrameView()->renderView();
    PlatformCALayer* renderViewLayer = toGraphicsLayerCA(renderView->layer()->backing()->graphicsLayer())->platformCALayer();

    TransformationMatrix finalTransform;
    finalTransform.scale(scale);

    // If the page scale is already the target scale, setPageScaleFactor() will short-circuit
    // and not apply the transform, so we can't depend on it to do so.
    renderViewLayer->setTransform(finalTransform);

    GraphicsLayerCA* shadowGraphicsLayer = toGraphicsLayerCA(renderView->compositor().layerForContentShadow());
    if (shadowGraphicsLayer) {
        PlatformCALayer* shadowLayer = shadowGraphicsLayer->platformCALayer();
        IntRect overflowRect = renderView->pixelSnappedLayoutOverflowRect();
        shadowLayer->setBounds(IntRect(IntPoint(), toIntSize(overflowRect.maxXMaxYCorner())));
        shadowLayer->setPosition(shadowLayer->bounds().center());
    }

    FloatPoint unscrolledOrigin(origin);
    FloatRect unobscuredContentRect = m_webPage->mainFrameView()->unobscuredContentRectIncludingScrollbars();
    unscrolledOrigin.moveBy(-unobscuredContentRect.location());
    m_webPage->scalePage(scale, roundedIntPoint(-unscrolledOrigin));
    flushLayers();

    m_transientZoomScale = 1;
}

void TiledCoreAnimationDrawingArea::setRootLayerTransform(const TransformationMatrix& transform)
{
    m_transform = transform;
    [m_rootLayer setSublayerTransform:transform];
}

} // namespace WebKit

#endif // !PLATFORM(IOS)
