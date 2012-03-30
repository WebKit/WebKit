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

#import "DrawingAreaProxyMessages.h"
#import "EventDispatcher.h"
#import "LayerHostingContext.h"
#import "LayerTreeContext.h"
#import "WebPage.h"
#import "WebPageProxyMessages.h"
#import "WebProcess.h"
#import <QuartzCore/QuartzCore.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameView.h>
#import <WebCore/GraphicsContext.h>
#import <WebCore/Page.h>
#import <WebCore/RenderLayerCompositor.h>
#import <WebCore/RenderView.h>
#import <WebCore/ScrollingCoordinator.h>
#import <WebCore/ScrollingThread.h>
#import <WebCore/ScrollingTree.h>
#import <WebCore/Settings.h>
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
{
    Page* page = webPage->corePage();

    // FIXME: It's weird that we're mucking around with the settings here.
    page->settings()->setForceCompositingMode(true);

#if ENABLE(THREADED_SCROLLING)
    page->settings()->setScrollingCoordinatorEnabled(true);

    WebProcess::shared().eventDispatcher().addScrollingTreeForPage(webPage);
#endif

    m_rootLayer = [CALayer layer];

    CGRect rootLayerFrame = m_webPage->bounds();
    m_rootLayer.get().frame = rootLayerFrame;
    m_rootLayer.get().opaque = YES;
    m_rootLayer.get().geometryFlipped = YES;

    m_layerHostingContext = LayerHostingContext::createForPort(WebProcess::shared().compositingRenderServerPort());
    m_layerHostingContext->setRootLayer(m_rootLayer.get());
    
    LayerTreeContext layerTreeContext;
    layerTreeContext.contextID = m_layerHostingContext->contextID();
    m_webPage->send(Messages::DrawingAreaProxy::EnterAcceleratedCompositingMode(0, layerTreeContext));

    updatePreferences();
}

TiledCoreAnimationDrawingArea::~TiledCoreAnimationDrawingArea()
{
#if ENABLE(THREADED_SCROLLING)
    WebProcess::shared().eventDispatcher().removeScrollingTreeForPage(m_webPage);
#endif

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

    flushLayers();
    [CATransaction flush];
    [CATransaction synchronize];
}

#if ENABLE(THREADED_SCROLLING)
static void forceRepaintAndSendMessage(uint64_t webPageID, uint64_t callbackID)
{
    WebPage* webPage = WebProcess::shared().webPage(webPageID);
    if (!webPage)
        return;

    webPage->drawingArea()->forceRepaint();
    webPage->send(Messages::WebPageProxy::VoidCallback(callbackID));
}

static void dispatchBackToMainThread(uint64_t webPageID, uint64_t callbackID)
{
    callOnMainThread(bind(forceRepaintAndSendMessage, webPageID, callbackID));
}
#endif

bool TiledCoreAnimationDrawingArea::forceRepaintAsync(uint64_t callbackID)
{
#if ENABLE(THREADED_SCROLLING)
    if (m_layerTreeStateIsFrozen)
        return false;

    ScrollingThread::dispatch(bind(dispatchBackToMainThread, m_webPage->pageID(), callbackID));
    return true;
#else
    return false;
#endif
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

void TiledCoreAnimationDrawingArea::scheduleCompositingLayerSync()
{
    m_layerFlushScheduler.schedule();
}

void TiledCoreAnimationDrawingArea::didInstallPageOverlay()
{
#if ENABLE(THREADED_SCROLLING)
    m_webPage->corePage()->scrollingCoordinator()->setForceMainThreadScrollLayerPositionUpdates(true);
#endif

    createPageOverlayLayer();
    scheduleCompositingLayerSync();
}

void TiledCoreAnimationDrawingArea::didUninstallPageOverlay()
{
#if ENABLE(THREADED_SCROLLING)
    if (Page* page = m_webPage->corePage())
        page->scrollingCoordinator()->setForceMainThreadScrollLayerPositionUpdates(false);
#endif

    destroyPageOverlayLayer();
    scheduleCompositingLayerSync();
}

void TiledCoreAnimationDrawingArea::setPageOverlayNeedsDisplay(const IntRect& rect)
{
    ASSERT(m_pageOverlayLayer);
    m_pageOverlayLayer->setNeedsDisplayInRect(rect);
    scheduleCompositingLayerSync();
}

void TiledCoreAnimationDrawingArea::updatePreferences()
{
    bool showDebugBorders = m_webPage->corePage()->settings()->showDebugBorders();

    if (showDebugBorders == !!m_debugInfoLayer)
        return;

    if (showDebugBorders) {
        m_debugInfoLayer = [CALayer layer];
        [m_rootLayer.get() addSublayer:m_debugInfoLayer.get()];
    } else {
        [m_debugInfoLayer.get() removeFromSuperlayer];
        m_debugInfoLayer = nullptr;
    }

#if ENABLE(THREADED_SCROLLING)
    ScrollingThread::dispatch(bind(&ScrollingTree::setDebugRootLayer, m_webPage->corePage()->scrollingCoordinator()->scrollingTree(), m_debugInfoLayer));
#endif
}

void TiledCoreAnimationDrawingArea::notifyAnimationStarted(const GraphicsLayer*, double)
{
}

void TiledCoreAnimationDrawingArea::notifySyncRequired(const GraphicsLayer*)
{
}

void TiledCoreAnimationDrawingArea::paintContents(const GraphicsLayer* graphicsLayer, GraphicsContext& graphicsContext, GraphicsLayerPaintingPhase, const IntRect& clipRect)
{
    ASSERT_UNUSED(graphicsLayer, graphicsLayer == m_pageOverlayLayer);

    m_webPage->drawPageOverlay(graphicsContext, clipRect);
}

bool TiledCoreAnimationDrawingArea::showDebugBorders(const GraphicsLayer*) const
{
    return m_webPage->corePage()->settings()->showDebugBorders();
}

bool TiledCoreAnimationDrawingArea::showRepaintCounter(const GraphicsLayer*) const
{
    return m_webPage->corePage()->settings()->showRepaintCounter();
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
        m_pageOverlayLayer->syncCompositingStateForThisLayerOnly();
    }

    bool returnValue = m_webPage->corePage()->mainFrame()->view()->syncCompositingStateIncludingSubframes();

    [pool drain];
    return returnValue;
}

void TiledCoreAnimationDrawingArea::updateGeometry(const IntSize& viewSize)
{
    m_webPage->setSize(viewSize);
    m_webPage->layoutIfNeeded();

    if (m_pageOverlayLayer)
        m_pageOverlayLayer->setSize(viewSize);

    if (!m_layerTreeStateIsFrozen)
        flushLayers();

    [CATransaction begin];
    [CATransaction setDisableActions:YES];

    m_rootLayer.get().frame = CGRectMake(0, 0, viewSize.width(), viewSize.height());

    [CATransaction commit];
    
    [CATransaction flush];
    [CATransaction synchronize];

    m_webPage->send(Messages::DrawingAreaProxy::DidUpdateGeometry());
}

void TiledCoreAnimationDrawingArea::setDeviceScaleFactor(float deviceScaleFactor)
{
    m_webPage->setDeviceScaleFactor(deviceScaleFactor);
}

void TiledCoreAnimationDrawingArea::setLayerHostingMode(uint32_t opaqueLayerHostingMode)
{
    LayerHostingMode layerHostingMode = static_cast<LayerHostingMode>(opaqueLayerHostingMode);
    if (layerHostingMode != m_layerHostingContext->layerHostingMode()) {
        // The mode has changed.
        
        // First, invalidate the old hosting context.
        m_layerHostingContext->invalidate();
        m_layerHostingContext = nullptr;

        // Create a new context and set it up.
        switch (layerHostingMode) {
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

        m_webPage->setLayerHostingMode(layerHostingMode);

        // Finally, inform the UIProcess that the context has changed.
        LayerTreeContext layerTreeContext;
        layerTreeContext.contextID = m_layerHostingContext->contextID();
        m_webPage->send(Messages::DrawingAreaProxy::UpdateAcceleratedCompositingMode(0, layerTreeContext));
    }
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

    if (m_debugInfoLayer)
        [m_rootLayer.get() addSublayer:m_debugInfoLayer.get()];

    [CATransaction commit];
}

void TiledCoreAnimationDrawingArea::createPageOverlayLayer()
{
    ASSERT(!m_pageOverlayLayer);

    m_pageOverlayLayer = GraphicsLayer::create(this);
#ifndef NDEBUG
    m_pageOverlayLayer->setName("page overlay content");
#endif

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

} // namespace WebKit
