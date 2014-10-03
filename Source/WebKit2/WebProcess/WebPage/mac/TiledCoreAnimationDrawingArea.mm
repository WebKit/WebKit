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
#import <WebCore/PlatformCAAnimationMac.h>
#import <WebCore/RenderLayerBacking.h>
#import <WebCore/RenderLayerCompositor.h>
#import <WebCore/RenderView.h>
#import <WebCore/ScrollbarTheme.h>
#import <WebCore/Settings.h>
#import <WebCore/TiledBacking.h>
#import <WebCore/WebActionDisablingCALayerDelegate.h>
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

TiledCoreAnimationDrawingArea::TiledCoreAnimationDrawingArea(WebPage& webPage, const WebPageCreationParameters& parameters)
    : DrawingArea(DrawingAreaTypeTiledCoreAnimation, webPage)
    , m_layerTreeStateIsFrozen(false)
    , m_layerFlushScheduler(this)
    , m_isPaintingSuspended(!(parameters.viewState & ViewState::IsVisible))
    , m_exposedRect(FloatRect::infiniteRect())
    , m_scrolledExposedRect(FloatRect::infiniteRect())
    , m_transientZoomScale(1)
    , m_sendDidUpdateViewStateTimer(RunLoop::main(), this, &TiledCoreAnimationDrawingArea::didUpdateViewStateTimerFired)
    , m_viewOverlayRootLayer(nullptr)
{
    m_webPage.corePage()->settings().setForceCompositingMode(true);

    m_hostingLayer = [CALayer layer];
    [m_hostingLayer setDelegate:[WebActionDisablingCALayerDelegate shared]];
    [m_hostingLayer setFrame:m_webPage.bounds()];
    [m_hostingLayer setOpaque:YES];
    [m_hostingLayer setGeometryFlipped:YES];

    updateLayerHostingContext();
    setColorSpace(parameters.colorSpace);

    LayerTreeContext layerTreeContext;
    layerTreeContext.contextID = m_layerHostingContext->contextID();
    m_webPage.send(Messages::DrawingAreaProxy::EnterAcceleratedCompositingMode(0, layerTreeContext));
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

    for (Frame* frame = &m_webPage.corePage()->mainFrame(); frame; frame = frame->tree().traverseNext()) {
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

    dispatchAfterEnsuringUpdatedScrollPosition([this, callbackID] {
        m_webPage.drawingArea()->forceRepaint();
        m_webPage.send(Messages::WebPageProxy::VoidCallback(callbackID));
    });
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

void TiledCoreAnimationDrawingArea::scheduleCompositingLayerFlushImmediately()
{
    scheduleCompositingLayerFlush();
}

void TiledCoreAnimationDrawingArea::updatePreferences(const WebPreferencesStore&)
{
    Settings& settings = m_webPage.corePage()->settings();

#if ENABLE(ASYNC_SCROLLING)
    if (AsyncScrollingCoordinator* scrollingCoordinator = toAsyncScrollingCoordinator(m_webPage.corePage()->scrollingCoordinator())) {
        bool scrollingPerformanceLoggingEnabled = m_webPage.scrollingPerformanceLoggingEnabled();
        ScrollingThread::dispatch(bind(&ScrollingTree::setScrollingPerformanceLoggingEnabled, scrollingCoordinator->scrollingTree(), scrollingPerformanceLoggingEnabled));
    }
#endif

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

void TiledCoreAnimationDrawingArea::updateRootLayers()
{
    if (!m_rootLayer) {
        [m_hostingLayer setSublayers:@[ ]];
        return;
    }

    [m_hostingLayer setSublayers:m_viewOverlayRootLayer ? @[ m_rootLayer.get(), m_viewOverlayRootLayer->platformLayer() ] : @[ m_rootLayer.get() ]];
}

void TiledCoreAnimationDrawingArea::attachViewOverlayGraphicsLayer(Frame* frame, GraphicsLayer* viewOverlayRootLayer)
{
    if (!frame->isMainFrame())
        return;

    m_viewOverlayRootLayer = viewOverlayRootLayer;
    updateRootLayers();
}

void TiledCoreAnimationDrawingArea::mainFrameContentSizeChanged(const IntSize& size)
{

}

void TiledCoreAnimationDrawingArea::updateIntrinsicContentSizeIfNeeded()
{
    if (!m_webPage.minimumLayoutSize().width())
        return;

    FrameView* frameView = m_webPage.mainFrameView();
    if (!frameView)
        return;

    if (frameView->needsLayout())
        return;

    IntSize contentSize = frameView->autoSizingIntrinsicContentSize();
    if (m_lastSentIntrinsicContentSize == contentSize)
        return;

    m_lastSentIntrinsicContentSize = contentSize;
    m_webPage.send(Messages::DrawingAreaProxy::IntrinsicContentSizeDidChange(contentSize));
}

void TiledCoreAnimationDrawingArea::dispatchAfterEnsuringUpdatedScrollPosition(std::function<void ()> function)
{
#if ENABLE(ASYNC_SCROLLING)
    if (!m_webPage.corePage()->scrollingCoordinator()) {
        function();
        return;
    }

    m_webPage.ref();
    m_webPage.corePage()->scrollingCoordinator()->commitTreeStateIfNeeded();

    if (!m_layerTreeStateIsFrozen)
        m_layerFlushScheduler.suspend();

    // It is possible for the drawing area to be destroyed before the bound block
    // is invoked, so grab a reference to the web page here so we can access the drawing area through it.
    // (The web page is already kept alive by dispatchAfterEnsuringUpdatedScrollPosition).
    WebPage* webPage = &m_webPage;

    ScrollingThread::dispatchBarrier([this, webPage, function] {
        DrawingArea* drawingArea = webPage->drawingArea();
        if (!drawingArea)
            return;

        function();

        if (!m_layerTreeStateIsFrozen)
            m_layerFlushScheduler.resume();

        webPage->deref();
    });
#else
    function();
#endif
}

bool TiledCoreAnimationDrawingArea::flushLayers()
{
    ASSERT(!m_layerTreeStateIsFrozen);

    // This gets called outside of the normal event loop so wrap in an autorelease pool
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    m_webPage.layoutIfNeeded();

    updateIntrinsicContentSizeIfNeeded();

    if (m_pendingRootLayer) {
        setRootCompositingLayer(m_pendingRootLayer.get());
        m_pendingRootLayer = nullptr;
    }

    FloatRect visibleRect = [m_hostingLayer frame];
    visibleRect.intersect(m_scrolledExposedRect);

    // Because our view-relative overlay root layer is not attached to the main GraphicsLayer tree, we need to flush it manually.
    if (m_viewOverlayRootLayer)
        m_viewOverlayRootLayer->flushCompositingState(visibleRect);

    bool returnValue = m_webPage.mainFrameView()->flushCompositingStateIncludingSubframes();
#if ENABLE(ASYNC_SCROLLING)
    if (ScrollingCoordinator* scrollingCoordinator = m_webPage.corePage()->scrollingCoordinator())
        scrollingCoordinator->commitTreeStateIfNeeded();
#endif

    // If we have an active transient zoom, we want the zoom to win over any changes
    // that WebCore makes to the relevant layers, so re-apply our changes after flushing.
    if (m_transientZoomScale != 1)
        applyTransientZoomToLayers(m_transientZoomScale, m_transientZoomOrigin);

    [pool drain];
    return returnValue;
}

void TiledCoreAnimationDrawingArea::viewStateDidChange(ViewState::Flags changed, bool wantsDidUpdateViewState)
{
    if (changed & ViewState::IsVisible) {
        if (m_webPage.isVisible())
            resumePainting();
        else
            suspendPainting();
    }

    if (wantsDidUpdateViewState)
        m_sendDidUpdateViewStateTimer.startOneShot(0);
}

void TiledCoreAnimationDrawingArea::didUpdateViewStateTimerFired()
{
    [CATransaction flush];
    m_webPage.send(Messages::WebPageProxy::DidUpdateViewState());
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
    FrameView* frameView = m_webPage.mainFrameView();
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
}

void TiledCoreAnimationDrawingArea::updateGeometry(const IntSize& viewSize, const IntSize& layerPosition)
{
    m_inUpdateGeometry = true;

    IntSize size = viewSize;
    IntSize contentSize = IntSize(-1, -1);

    if (!m_webPage.minimumLayoutSize().width() || m_webPage.autoSizingShouldExpandToViewHeight())
        m_webPage.setSize(size);

    FrameView* frameView = m_webPage.mainFrameView();

    if (m_webPage.autoSizingShouldExpandToViewHeight() && frameView)
        frameView->setAutoSizeFixedMinimumHeight(viewSize.height());

    m_webPage.layoutIfNeeded();

    if (m_webPage.minimumLayoutSize().width() && frameView) {
        contentSize = frameView->autoSizingIntrinsicContentSize();
        size = contentSize;
    }

    if (!m_layerTreeStateIsFrozen)
        flushLayers();

    [CATransaction begin];
    [CATransaction setDisableActions:YES];

    [m_hostingLayer setFrame:CGRectMake(layerPosition.width(), layerPosition.height(), viewSize.width(), viewSize.height())];

    [CATransaction commit];
    
    [CATransaction flush];
    [CATransaction synchronize];

    m_webPage.send(Messages::DrawingAreaProxy::DidUpdateGeometry());

    m_inUpdateGeometry = false;
}

void TiledCoreAnimationDrawingArea::setDeviceScaleFactor(float deviceScaleFactor)
{
    m_webPage.setDeviceScaleFactor(deviceScaleFactor);
}

void TiledCoreAnimationDrawingArea::setLayerHostingMode(LayerHostingMode)
{
    updateLayerHostingContext();

    // Finally, inform the UIProcess that the context has changed.
    LayerTreeContext layerTreeContext;
    layerTreeContext.contextID = m_layerHostingContext->contextID();
    m_webPage.send(Messages::DrawingAreaProxy::UpdateAcceleratedCompositingMode(0, layerTreeContext));
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
    switch (m_webPage.layerHostingMode()) {
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

    bool hadRootLayer = !!m_rootLayer;
    m_rootLayer = layer;
    [m_rootLayer setSublayerTransform:m_transform];

    updateRootLayers();

    if (hadRootLayer != !!layer)
        m_layerHostingContext->setRootLayer(layer ? m_hostingLayer.get() : 0);

    updateDebugInfoLayer(m_webPage.corePage()->settings().showTiledScrollingIndicator());

    [CATransaction commit];
}

TiledBacking* TiledCoreAnimationDrawingArea::mainFrameTiledBacking() const
{
    FrameView* frameView = m_webPage.mainFrameView();
    return frameView ? frameView->tiledBacking() : nullptr;
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

PlatformCALayer* TiledCoreAnimationDrawingArea::layerForTransientZoom() const
{
    RenderLayerBacking* renderViewBacking = m_webPage.mainFrameView()->renderView()->layer()->backing();

    if (GraphicsLayer* contentsContainmentLayer = renderViewBacking->contentsContainmentLayer())
        return toGraphicsLayerCA(contentsContainmentLayer)->platformCALayer();

    return toGraphicsLayerCA(renderViewBacking->graphicsLayer())->platformCALayer();
}

PlatformCALayer* TiledCoreAnimationDrawingArea::shadowLayerForTransientZoom() const
{
    RenderLayerCompositor& renderLayerCompositor = m_webPage.mainFrameView()->renderView()->compositor();

    if (GraphicsLayer* shadowGraphicsLayer = renderLayerCompositor.layerForContentShadow())
        return toGraphicsLayerCA(shadowGraphicsLayer)->platformCALayer();

    return nullptr;
}
    
static FloatPoint shadowLayerPositionForFrame(FrameView& frameView, FloatPoint origin)
{
    FloatPoint position = frameView.renderView()->documentRect().location() + FloatPoint(0, FrameView::yPositionForRootContentLayer(frameView.scrollPosition(), frameView.topContentInset(), frameView.headerHeight()));

    return position + origin.expandedTo(FloatPoint());
}

static FloatRect shadowLayerBoundsForFrame(FrameView& frameView, float transientScale)
{
    FloatRect clipLayerFrame(frameView.renderView()->documentRect());
    FloatRect shadowLayerFrame = clipLayerFrame;
    
    shadowLayerFrame.scale(transientScale / frameView.frame().page()->pageScaleFactor());
    shadowLayerFrame.intersect(clipLayerFrame);
    
    return shadowLayerFrame;
}

void TiledCoreAnimationDrawingArea::applyTransientZoomToLayers(double scale, FloatPoint origin)
{
    // FIXME: Scrollbars should stay in-place and change height while zooming.

    if (!m_hostingLayer)
        return;

    TransformationMatrix transform;
    transform.translate(origin.x(), origin.y());
    transform.scale(scale);

    PlatformCALayer* zoomLayer = layerForTransientZoom();
    zoomLayer->setTransform(transform);
    zoomLayer->setAnchorPoint(FloatPoint3D());
    zoomLayer->setPosition(FloatPoint3D());
    
    if (PlatformCALayer* shadowLayer = shadowLayerForTransientZoom()) {
        FrameView& frameView = *m_webPage.mainFrameView();
        shadowLayer->setBounds(shadowLayerBoundsForFrame(frameView, scale));
        shadowLayer->setPosition(shadowLayerPositionForFrame(frameView, origin));
    }

    m_transientZoomScale = scale;
    m_transientZoomOrigin = origin;
}

void TiledCoreAnimationDrawingArea::adjustTransientZoom(double scale, FloatPoint origin)
{
    applyTransientZoomToLayers(scale, origin);

    double currentPageScale = m_webPage.pageScaleFactor();
    if (scale > currentPageScale)
        return;

    FrameView* frameView = m_webPage.mainFrameView();
    FloatRect tileCoverageRect = frameView->visibleContentRectIncludingScrollbars();
    tileCoverageRect.moveBy(-origin);
    tileCoverageRect.scale(currentPageScale / scale);
    frameView->renderView()->layer()->backing()->tiledBacking()->prepopulateRect(tileCoverageRect);
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
    FrameView& frameView = *m_webPage.mainFrameView();
    FloatRect visibleContentRect = frameView.visibleContentRectIncludingScrollbars();

    FloatPoint constrainedOrigin = visibleContentRect.location();
    constrainedOrigin.moveBy(-origin);

    IntSize scaledTotalContentsSize = frameView.totalContentsSize();
    scaledTotalContentsSize.scale(scale / m_webPage.pageScaleFactor());

    // Scaling may have exposed the overhang area, so we need to constrain the final
    // layer position exactly like scrolling will once it's committed, to ensure that
    // scrolling doesn't make the view jump.
    constrainedOrigin = ScrollableArea::constrainScrollPositionForOverhang(roundedIntRect(visibleContentRect), scaledTotalContentsSize, roundedIntPoint(constrainedOrigin), frameView.scrollOrigin(), frameView.headerHeight(), frameView.footerHeight());
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
    RefPtr<PlatformCAAnimation> renderViewAnimation = PlatformCAAnimationMac::create(renderViewAnimationCA.get());
    renderViewAnimation->setToValue(transform);

    RetainPtr<CALayer> shadowCALayer;
    if (PlatformCALayer* shadowLayer = shadowLayerForTransientZoom())
        shadowCALayer = shadowLayer->platformLayer();

    RefPtr<PlatformCALayer> zoomLayer = layerForTransientZoom();
    RefPtr<WebPage> page = &m_webPage;

    [CATransaction begin];
    [CATransaction setCompletionBlock:[zoomLayer, shadowCALayer, page, scale, origin] () {
        zoomLayer->removeAnimationForKey("transientZoomCommit");
        if (shadowCALayer)
            [shadowCALayer removeAllAnimations];

        if (TiledCoreAnimationDrawingArea* drawingArea = static_cast<TiledCoreAnimationDrawingArea*>(page->drawingArea()))
            drawingArea->applyTransientZoomToPage(scale, origin);
    }];

    zoomLayer->addAnimationForKey("transientZoomCommit", renderViewAnimation.get());

    if (shadowCALayer) {
        FloatRect shadowBounds = shadowLayerBoundsForFrame(frameView, scale);
        RetainPtr<CGPathRef> shadowPath = adoptCF(CGPathCreateWithRect(shadowBounds, NULL)).get();

        RetainPtr<CABasicAnimation> shadowBoundsAnimation = transientZoomSnapAnimationForKeyPath("bounds");
        [shadowBoundsAnimation setToValue:[NSValue valueWithRect:shadowBounds]];
        RetainPtr<CABasicAnimation> shadowPositionAnimation = transientZoomSnapAnimationForKeyPath("position");
        [shadowPositionAnimation setToValue:[NSValue valueWithPoint:shadowLayerPositionForFrame(frameView, constrainedOrigin)]];
        RetainPtr<CABasicAnimation> shadowPathAnimation = transientZoomSnapAnimationForKeyPath("shadowPath");
        [shadowPathAnimation setToValue:(id)shadowPath.get()];

        [shadowCALayer addAnimation:shadowBoundsAnimation.get() forKey:@"transientZoomCommitShadowBounds"];
        [shadowCALayer addAnimation:shadowPositionAnimation.get() forKey:@"transientZoomCommitShadowPosition"];
        [shadowCALayer addAnimation:shadowPathAnimation.get() forKey:@"transientZoomCommitShadowPath"];
    }

    [CATransaction commit];
}

void TiledCoreAnimationDrawingArea::applyTransientZoomToPage(double scale, FloatPoint origin)
{
    // If the page scale is already the target scale, setPageScaleFactor() will short-circuit
    // and not apply the transform, so we can't depend on it to do so.
    TransformationMatrix finalTransform;
    finalTransform.scale(scale);
    layerForTransientZoom()->setTransform(finalTransform);
    
    FrameView& frameView = *m_webPage.mainFrameView();

    if (PlatformCALayer* shadowLayer = shadowLayerForTransientZoom()) {
        shadowLayer->setBounds(shadowLayerBoundsForFrame(frameView, 1));
        shadowLayer->setPosition(shadowLayerPositionForFrame(frameView, FloatPoint()));
    }

    FloatPoint unscrolledOrigin(origin);
    FloatRect unobscuredContentRect = frameView.unobscuredContentRectIncludingScrollbars();
    unscrolledOrigin.moveBy(-unobscuredContentRect.location());
    m_webPage.scalePage(scale, roundedIntPoint(-unscrolledOrigin));
    m_transientZoomScale = 1;
    flushLayers();
}

} // namespace WebKit

#endif // !PLATFORM(IOS)
