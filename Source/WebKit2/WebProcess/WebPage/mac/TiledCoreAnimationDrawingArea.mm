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

#if ENABLE(THREADED_SCROLLING)

#import "ColorSpaceData.h"
#import "DrawingAreaProxyMessages.h"
#import "EventDispatcher.h"
#import "LayerHostingContext.h"
#import "LayerTreeContext.h"
#import "WebFrame.h"
#import "WebPage.h"
#import "WebPageCreationParameters.h"
#import "WebPageProxyMessages.h"
#import "WebProcess.h"
#import <QuartzCore/QuartzCore.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameView.h>
#import <WebCore/GraphicsContext.h>
#import <WebCore/GraphicsLayerCA.h>
#import <WebCore/Page.h>
#import <WebCore/RenderView.h>
#import <WebCore/ScrollingCoordinator.h>
#import <WebCore/ScrollingThread.h>
#import <WebCore/ScrollingTree.h>
#import <WebCore/Settings.h>
#import <WebCore/TiledBacking.h>
#import <wtf/MainThread.h>

@interface CATransaction (Details)
+ (void)synchronize;
@end

using namespace WebCore;

namespace WebKit {

PassOwnPtr<TiledCoreAnimationDrawingArea> TiledCoreAnimationDrawingArea::create(WebPage* webPage, const WebPageCreationParameters& parameters)
{
    return adoptPtr(new TiledCoreAnimationDrawingArea(webPage, parameters));
}

TiledCoreAnimationDrawingArea::TiledCoreAnimationDrawingArea(WebPage* webPage, const WebPageCreationParameters& parameters)
    : DrawingArea(DrawingAreaTypeTiledCoreAnimation, webPage)
    , m_layerTreeStateIsFrozen(false)
    , m_layerFlushScheduler(this)
    , m_isPaintingSuspended(!parameters.isVisible)
    , m_minimumLayoutWidth(0)
{
    Page* page = m_webPage->corePage();

    page->settings()->setScrollingCoordinatorEnabled(true);
    page->settings()->setForceCompositingMode(true);

    WebProcess::shared().eventDispatcher().addScrollingTreeForPage(webPage);

    m_rootLayer = [CALayer layer];

    CGRect rootLayerFrame = m_webPage->bounds();
    m_rootLayer.get().frame = rootLayerFrame;
    m_rootLayer.get().opaque = YES;
    m_rootLayer.get().geometryFlipped = YES;

    updateLayerHostingContext();
    setColorSpace(parameters.colorSpace);

    LayerTreeContext layerTreeContext;
    layerTreeContext.contextID = m_layerHostingContext->contextID();
    m_webPage->send(Messages::DrawingAreaProxy::EnterAcceleratedCompositingMode(0, layerTreeContext));
}

TiledCoreAnimationDrawingArea::~TiledCoreAnimationDrawingArea()
{
    WebProcess::shared().eventDispatcher().removeScrollingTreeForPage(m_webPage);

    m_layerFlushScheduler.invalidate();
}

void TiledCoreAnimationDrawingArea::setNeedsDisplay(const IntRect& rect)
{
}

void TiledCoreAnimationDrawingArea::scroll(const IntRect& scrollRect, const IntSize& scrollOffset)
{
}

void TiledCoreAnimationDrawingArea::setRootCompositingLayer(GraphicsLayer* graphicsLayer)
{
    CALayer *rootCompositingLayer = graphicsLayer ? graphicsLayer->platformLayer() : nil;

    // Since we'll always be in accelerated compositing mode, the only time that layer will be nil
    // is when the WKView is removed from its containing window. In that case, the layer will already be
    // removed from the layer tree hierarchy over in the UI process, so there's no reason to remove it locally.
    // In addition, removing the layer here will cause flashes when switching between tabs.
    if (!rootCompositingLayer)
        return;

    if (m_layerTreeStateIsFrozen) {
        m_pendingRootCompositingLayer = rootCompositingLayer;
        return;
    }

    setRootCompositingLayer(rootCompositingLayer);
}

void TiledCoreAnimationDrawingArea::forceRepaint()
{
    if (m_layerTreeStateIsFrozen)
        return;

    for (Frame* frame = m_webPage->corePage()->mainFrame(); frame; frame = frame->tree()->traverseNext()) {
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

void TiledCoreAnimationDrawingArea::didInstallPageOverlay()
{
    m_webPage->corePage()->scrollingCoordinator()->setForceMainThreadScrollLayerPositionUpdates(true);

    createPageOverlayLayer();
    scheduleCompositingLayerFlush();
}

void TiledCoreAnimationDrawingArea::didUninstallPageOverlay()
{
    if (Page* page = m_webPage->corePage())
        page->scrollingCoordinator()->setForceMainThreadScrollLayerPositionUpdates(false);

    destroyPageOverlayLayer();
    scheduleCompositingLayerFlush();
}

void TiledCoreAnimationDrawingArea::setPageOverlayNeedsDisplay(const IntRect& rect)
{
    ASSERT(m_pageOverlayLayer);
    m_pageOverlayLayer->setNeedsDisplayInRect(rect);
    scheduleCompositingLayerFlush();
}

void TiledCoreAnimationDrawingArea::updatePreferences(const WebPreferencesStore&)
{
    bool scrollingPerformanceLoggingEnabled = m_webPage->scrollingPerformanceLoggingEnabled();
    ScrollingThread::dispatch(bind(&ScrollingTree::setScrollingPerformanceLoggingEnabled, m_webPage->corePage()->scrollingCoordinator()->scrollingTree(), scrollingPerformanceLoggingEnabled));

    if (TiledBacking* tiledBacking = mainFrameTiledBacking())
        tiledBacking->setAggressivelyRetainsTiles(m_webPage->corePage()->settings()->aggressiveTileRetentionEnabled());

    // Soon we want pages with fixed positioned elements to be able to be scrolled by the ScrollingCoordinator.
    // As a part of that work, we have to composite fixed position elements, and we have to allow those
    // elements to create a stacking context.
    m_webPage->corePage()->settings()->setAcceleratedCompositingForFixedPositionEnabled(true);
    m_webPage->corePage()->settings()->setFixedPositionCreatesStackingContext(true);

    bool showTiledScrollingIndicator = m_webPage->corePage()->settings()->showTiledScrollingIndicator();
    if (showTiledScrollingIndicator == !!m_debugInfoLayer)
        return;

    updateDebugInfoLayer(showTiledScrollingIndicator);
}

void TiledCoreAnimationDrawingArea::mainFrameContentSizeChanged(const IntSize& contentSize)
{
    if (!m_minimumLayoutWidth)
        return;

    if (m_inUpdateGeometry)
        return;

    if (m_lastSentIntrinsicContentSize == contentSize)
        return;

    m_lastSentIntrinsicContentSize = contentSize;
    m_webPage->send(Messages::DrawingAreaProxy::IntrinsicContentSizeDidChange(contentSize));
}

void TiledCoreAnimationDrawingArea::dispatchAfterEnsuringUpdatedScrollPosition(const Function<void ()>& functionRef)
{
    m_webPage->ref();
    m_webPage->corePage()->scrollingCoordinator()->commitTreeStateIfNeeded();

    if (!m_layerTreeStateIsFrozen)
        m_layerFlushScheduler.suspend();

    Function<void ()> function = functionRef;

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
}

void TiledCoreAnimationDrawingArea::notifyAnimationStarted(const GraphicsLayer*, double)
{
}

void TiledCoreAnimationDrawingArea::notifyFlushRequired(const GraphicsLayer*)
{
}

void TiledCoreAnimationDrawingArea::paintContents(const GraphicsLayer* graphicsLayer, GraphicsContext& graphicsContext, GraphicsLayerPaintingPhase, const IntRect& clipRect)
{
    ASSERT_UNUSED(graphicsLayer, graphicsLayer == m_pageOverlayLayer);

    m_webPage->drawPageOverlay(graphicsContext, clipRect);
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

    if (m_pendingRootCompositingLayer) {
        setRootCompositingLayer(m_pendingRootCompositingLayer.get());
        m_pendingRootCompositingLayer = nullptr;
    }

    if (m_pageOverlayLayer) {
        m_pageOverlayLayer->setNeedsDisplay();
        m_pageOverlayLayer->flushCompositingStateForThisLayerOnly();
    }

    bool returnValue = m_webPage->corePage()->mainFrame()->view()->flushCompositingStateIncludingSubframes();

    [pool drain];
    return returnValue;
}

void TiledCoreAnimationDrawingArea::suspendPainting()
{
    ASSERT(!m_isPaintingSuspended);
    m_isPaintingSuspended = true;

    [m_rootLayer.get() setValue:(id)kCFBooleanTrue forKey:@"NSCAViewRenderPaused"];
    [[NSNotificationCenter defaultCenter] postNotificationName:@"NSCAViewRenderDidPauseNotification" object:nil userInfo:[NSDictionary dictionaryWithObject:m_rootLayer.get() forKey:@"layer"]];

    m_webPage->corePage()->suspendScriptedAnimations();
}

void TiledCoreAnimationDrawingArea::resumePainting()
{
    if (!m_isPaintingSuspended) {
        // FIXME: We can get a call to resumePainting when painting is not suspended.
        // This happens when sending a synchronous message to create a new page. See <rdar://problem/8976531>.
        return;
    }
    m_isPaintingSuspended = false;

    [m_rootLayer.get() setValue:(id)kCFBooleanFalse forKey:@"NSCAViewRenderPaused"];
    [[NSNotificationCenter defaultCenter] postNotificationName:@"NSCAViewRenderDidResumeNotification" object:nil userInfo:[NSDictionary dictionaryWithObject:m_rootLayer.get() forKey:@"layer"]];

    if (m_webPage->windowIsVisible())
        m_webPage->corePage()->resumeScriptedAnimations();
}

void TiledCoreAnimationDrawingArea::setExposedRect(const IntRect& exposedRect)
{
    // FIXME: This should be mapped through the scroll offset, but we need to keep it up to date.
    m_exposedRect = exposedRect;

    mainFrameTiledBacking()->setExposedRect(exposedRect);
}

void TiledCoreAnimationDrawingArea::mainFrameScrollabilityChanged(bool isScrollable)
{
    mainFrameTiledBacking()->setClipsToExposedRect(!isScrollable);
}

void TiledCoreAnimationDrawingArea::updateGeometry(const IntSize& viewSize, double minimumLayoutWidth)
{
    m_inUpdateGeometry = true;

    m_minimumLayoutWidth = minimumLayoutWidth;

    IntSize size = viewSize;
    IntSize contentSize = IntSize(-1, -1);

    if (m_minimumLayoutWidth > 0) {
        m_webPage->setSize(IntSize(m_minimumLayoutWidth, 0));
        m_webPage->layoutIfNeeded();

        contentSize = m_webPage->mainWebFrame()->contentBounds().size();
        size = contentSize;
    }

    m_webPage->setSize(size);
    m_webPage->layoutIfNeeded();

    if (m_pageOverlayLayer)
        m_pageOverlayLayer->setSize(size);

    if (!m_layerTreeStateIsFrozen)
        flushLayers();

    [CATransaction begin];
    [CATransaction setDisableActions:YES];

    m_rootLayer.get().frame = CGRectMake(0, 0, size.width(), size.height());

    [CATransaction commit];
    
    [CATransaction flush];
    [CATransaction synchronize];

    m_lastSentIntrinsicContentSize = contentSize;
    m_webPage->send(Messages::DrawingAreaProxy::DidUpdateGeometry(contentSize));

    m_inUpdateGeometry = false;
}

void TiledCoreAnimationDrawingArea::setDeviceScaleFactor(float deviceScaleFactor)
{
    m_webPage->setDeviceScaleFactor(deviceScaleFactor);
}

void TiledCoreAnimationDrawingArea::setLayerHostingMode(uint32_t opaqueLayerHostingMode)
{
    LayerHostingMode layerHostingMode = static_cast<LayerHostingMode>(opaqueLayerHostingMode);
    if (layerHostingMode == m_webPage->layerHostingMode())
        return;

    m_webPage->setLayerHostingMode(layerHostingMode);

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
    case LayerHostingModeDefault:
        m_layerHostingContext = LayerHostingContext::createForPort(WebProcess::shared().compositingRenderServerPort());
        break;
#if HAVE(LAYER_HOSTING_IN_WINDOW_SERVER)
    case LayerHostingModeInWindowServer:
        m_layerHostingContext = LayerHostingContext::createForWindowServer();
        break;
#endif
    }

    m_layerHostingContext->setRootLayer(m_rootLayer.get());
    if (colorSpace)
        m_layerHostingContext->setColorSpace(colorSpace.get());
}

void TiledCoreAnimationDrawingArea::setRootCompositingLayer(CALayer *layer)
{
    ASSERT(layer);
    ASSERT(!m_layerTreeStateIsFrozen);

    [CATransaction begin];
    [CATransaction setDisableActions:YES];

    m_rootLayer.get().sublayers = [NSArray arrayWithObject:layer];

    if (m_pageOverlayLayer)
        [m_rootLayer.get() addSublayer:m_pageOverlayLayer->platformLayer()];

    if (TiledBacking* tiledBacking = mainFrameTiledBacking()) {
        tiledBacking->setAggressivelyRetainsTiles(m_webPage->corePage()->settings()->aggressiveTileRetentionEnabled());
        tiledBacking->setExposedRect(m_exposedRect);
        tiledBacking->setClipsToExposedRect(!m_webPage->mainFrameIsScrollable());
    }

    updateDebugInfoLayer(m_webPage->corePage()->settings()->showTiledScrollingIndicator());

    [CATransaction commit];
}

void TiledCoreAnimationDrawingArea::createPageOverlayLayer()
{
    ASSERT(!m_pageOverlayLayer);

    m_pageOverlayLayer = GraphicsLayer::create(graphicsLayerFactory(), this);
#ifndef NDEBUG
    m_pageOverlayLayer->setName("page overlay content");
#endif

    // We don't ever want the overlay layer to become tiled because that will look bad, and
    // we also never expect the underlying CALayer to change.
    static_cast<GraphicsLayerCA*>(m_pageOverlayLayer.get())->setAllowTiledLayer(false);
    m_pageOverlayLayer->setAcceleratesDrawing(true);
    m_pageOverlayLayer->setDrawsContent(true);
    m_pageOverlayLayer->setSize(m_webPage->size());

    [CATransaction begin];
    [CATransaction setDisableActions:YES];

    [m_rootLayer.get() addSublayer:m_pageOverlayLayer->platformLayer()];

    [CATransaction commit];
}

void TiledCoreAnimationDrawingArea::destroyPageOverlayLayer()
{
    ASSERT(m_pageOverlayLayer);

    [m_pageOverlayLayer->platformLayer() removeFromSuperlayer];
    m_pageOverlayLayer = nullptr;
}

TiledBacking* TiledCoreAnimationDrawingArea::mainFrameTiledBacking() const
{
    Frame* frame = m_webPage->corePage()->mainFrame();
    if (!frame)
        return 0;
    
    FrameView* frameView = frame->view();
    return frameView ? frameView->tiledBacking() : 0;
}

void TiledCoreAnimationDrawingArea::updateDebugInfoLayer(bool showLayer)
{
    if (showLayer) {
        if (TiledBacking* tiledBacking = mainFrameTiledBacking())
            m_debugInfoLayer = tiledBacking->tiledScrollingIndicatorLayer();
        
        if (m_debugInfoLayer) {
#ifndef NDEBUG
            [m_debugInfoLayer.get() setName:@"Debug Info"];
#endif
            [m_rootLayer.get() addSublayer:m_debugInfoLayer.get()];
        }
    } else if (m_debugInfoLayer) {
        [m_debugInfoLayer.get() removeFromSuperlayer];
        m_debugInfoLayer = nullptr;
    }
}

} // namespace WebKit

#endif // ENABLE(THREADED_SCROLLING)
