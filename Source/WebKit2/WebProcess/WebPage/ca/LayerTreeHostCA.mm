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
#import "LayerTreeHostCA.h"

#import "DrawingAreaImpl.h"
#import "WebPage.h"
#import "WebProcess.h"
#import <QuartzCore/CATransaction.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameView.h>
#import <WebCore/Page.h>
#import <WebCore/Settings.h>
#import <WebKitSystemInterface.h>

@interface CATransaction (Details)
+ (void)synchronize;
@end

using namespace WebCore;

namespace WebKit {

PassRefPtr<LayerTreeHostCA> LayerTreeHostCA::create(WebPage* webPage)
{
    return adoptRef(new LayerTreeHostCA(webPage));
}

LayerTreeHostCA::LayerTreeHostCA(WebPage* webPage)
    : LayerTreeHost(webPage)
    , m_isValid(true)
    , m_notifyAfterScheduledLayerFlush(false)
{
    mach_port_t serverPort = WebProcess::shared().compositingRenderServerPort();
    m_remoteLayerClient = WKCARemoteLayerClientMakeWithServerPort(serverPort);

    // Create a root layer.
    m_rootLayer = GraphicsLayer::create(this);
#ifndef NDEBUG
    m_rootLayer->setName("LayerTreeHost root layer");
#endif
    m_rootLayer->setDrawsContent(false);
    m_rootLayer->setSize(webPage->size());

    [m_rootLayer->platformLayer() setGeometryFlipped:YES];

    m_nonCompositedContentLayer = GraphicsLayer::create(this);
#ifndef NDEBUG
    m_nonCompositedContentLayer->setName("LayerTreeHost non-composited content");
#endif
    m_nonCompositedContentLayer->setDrawsContent(true);
    m_nonCompositedContentLayer->setContentsOpaque(m_webPage->drawsBackground() && !m_webPage->drawsTransparentBackground());
    m_nonCompositedContentLayer->setSize(webPage->size());
    if (m_webPage->corePage()->settings()->acceleratedDrawingEnabled())
        m_nonCompositedContentLayer->setAcceleratesDrawing(true);

    m_rootLayer->addChild(m_nonCompositedContentLayer.get());

    WKCARemoteLayerClientSetLayer(m_remoteLayerClient.get(), m_rootLayer->platformLayer());

    if (m_webPage->hasPageOverlay())
        createPageOverlayLayer();

    scheduleLayerFlush();

    m_layerTreeContext.contextID = WKCARemoteLayerClientGetClientId(m_remoteLayerClient.get());
}

LayerTreeHostCA::~LayerTreeHostCA()
{
    ASSERT(!m_isValid);
    ASSERT(!m_flushPendingLayerChangesRunLoopObserver);
    ASSERT(!m_remoteLayerClient);
    ASSERT(!m_rootLayer);
}

const LayerTreeContext& LayerTreeHostCA::layerTreeContext()
{
    return m_layerTreeContext;
}

void LayerTreeHostCA::scheduleLayerFlush()
{
    CFRunLoopRef currentRunLoop = CFRunLoopGetCurrent();
    
    // Make sure we wake up the loop or the observer could be delayed until some other source fires.
    CFRunLoopWakeUp(currentRunLoop);

    if (m_flushPendingLayerChangesRunLoopObserver)
        return;

    // Run before the Core Animation commit observer, which has order 2000000.
    const CFIndex runLoopOrder = 2000000 - 1;
    CFRunLoopObserverContext context = { 0, this, 0, 0, 0 };
    m_flushPendingLayerChangesRunLoopObserver.adoptCF(CFRunLoopObserverCreate(0, kCFRunLoopBeforeWaiting | kCFRunLoopExit, true, runLoopOrder, flushPendingLayerChangesRunLoopObserverCallback, &context));

    CFRunLoopAddObserver(currentRunLoop, m_flushPendingLayerChangesRunLoopObserver.get(), kCFRunLoopCommonModes);
}

void LayerTreeHostCA::setShouldNotifyAfterNextScheduledLayerFlush(bool notifyAfterScheduledLayerFlush)
{
    m_notifyAfterScheduledLayerFlush = notifyAfterScheduledLayerFlush;
}

void LayerTreeHostCA::setRootCompositingLayer(GraphicsLayer* graphicsLayer)
{
    m_nonCompositedContentLayer->removeAllChildren();

    // Add the accelerated layer tree hierarchy.
    if (graphicsLayer)
        m_nonCompositedContentLayer->addChild(graphicsLayer);
}

void LayerTreeHostCA::invalidate()
{
    ASSERT(m_isValid);

    if (m_flushPendingLayerChangesRunLoopObserver) {
        CFRunLoopObserverInvalidate(m_flushPendingLayerChangesRunLoopObserver.get());
        m_flushPendingLayerChangesRunLoopObserver = nullptr;
    }

    WKCARemoteLayerClientInvalidate(m_remoteLayerClient.get());
    m_remoteLayerClient = nullptr;
    m_rootLayer = nullptr;
    m_isValid = false;
}

void LayerTreeHostCA::setNonCompositedContentsNeedDisplay(const IntRect& rect)
{
    m_nonCompositedContentLayer->setNeedsDisplayInRect(rect);
    if (m_pageOverlayLayer)
        m_pageOverlayLayer->setNeedsDisplayInRect(rect);

    scheduleLayerFlush();
}

void LayerTreeHostCA::scrollNonCompositedContents(const IntRect& scrollRect, const IntSize& scrollOffset)
{
    setNonCompositedContentsNeedDisplay(scrollRect);
}

void LayerTreeHostCA::sizeDidChange(const IntSize& newSize)
{
    m_rootLayer->setSize(newSize);
    m_nonCompositedContentLayer->setSize(newSize);

    if (m_pageOverlayLayer)
        m_pageOverlayLayer->setSize(newSize);

    scheduleLayerFlush();
    flushPendingLayerChanges();

    [CATransaction flush];
    [CATransaction synchronize];
}

void LayerTreeHostCA::forceRepaint()
{
    scheduleLayerFlush();
    flushPendingLayerChanges();

    [CATransaction flush];
    [CATransaction synchronize];
}    

void LayerTreeHostCA::didInstallPageOverlay()
{
    createPageOverlayLayer();
    scheduleLayerFlush();
}

void LayerTreeHostCA::didUninstallPageOverlay()
{
    destroyPageOverlayLayer();
    scheduleLayerFlush();
}

void LayerTreeHostCA::setPageOverlayNeedsDisplay(const IntRect& rect)
{
    ASSERT(m_pageOverlayLayer);
    m_pageOverlayLayer->setNeedsDisplayInRect(rect);
    scheduleLayerFlush();
}

void LayerTreeHostCA::notifyAnimationStarted(const WebCore::GraphicsLayer*, double time)
{
}

void LayerTreeHostCA::notifySyncRequired(const WebCore::GraphicsLayer*)
{
}

void LayerTreeHostCA::paintContents(const GraphicsLayer* graphicsLayer, GraphicsContext& graphicsContext, GraphicsLayerPaintingPhase, const IntRect& clipRect)
{
    if (graphicsLayer == m_nonCompositedContentLayer) {
        m_webPage->drawRect(graphicsContext, clipRect);
        return;
    }

    if (graphicsLayer == m_pageOverlayLayer) {
        m_webPage->drawPageOverlay(graphicsContext, clipRect);
        return;
    }
}

bool LayerTreeHostCA::showDebugBorders() const
{
    return m_webPage->corePage()->settings()->showDebugBorders();
}

bool LayerTreeHostCA::showRepaintCounter() const
{
    return m_webPage->corePage()->settings()->showRepaintCounter();
}

void LayerTreeHostCA::flushPendingLayerChangesRunLoopObserverCallback(CFRunLoopObserverRef, CFRunLoopActivity, void* context)
{
    // This gets called outside of the normal event loop so wrap in an autorelease pool
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    static_cast<LayerTreeHostCA*>(context)->flushPendingLayerChangesRunLoopObserverCallback();
    [pool drain];
}

void LayerTreeHostCA::flushPendingLayerChangesRunLoopObserverCallback()
{
    {
        RefPtr<LayerTreeHostCA> protect(this);
        m_webPage->layoutIfNeeded();

        if (!m_isValid)
            return;
    }

    if (!flushPendingLayerChanges())
        return;

    // We successfully flushed the pending layer changes, remove the run loop observer.
    ASSERT(m_flushPendingLayerChangesRunLoopObserver);
    CFRunLoopObserverInvalidate(m_flushPendingLayerChangesRunLoopObserver.get());
    m_flushPendingLayerChangesRunLoopObserver = 0;

    if (m_notifyAfterScheduledLayerFlush) {
        // Let the drawing area know that we've done a flush of the layer changes.
        static_cast<DrawingAreaImpl*>(m_webPage->drawingArea())->layerHostDidFlushLayers();
        m_notifyAfterScheduledLayerFlush = false;
    }
}

bool LayerTreeHostCA::flushPendingLayerChanges()
{
    m_rootLayer->syncCompositingStateForThisLayerOnly();
    m_nonCompositedContentLayer->syncCompositingStateForThisLayerOnly();
    if (m_pageOverlayLayer)
        m_pageOverlayLayer->syncCompositingStateForThisLayerOnly();

    return m_webPage->corePage()->mainFrame()->view()->syncCompositingStateIncludingSubframes();
}

void LayerTreeHostCA::createPageOverlayLayer()
{
    ASSERT(!m_pageOverlayLayer);

    m_pageOverlayLayer = GraphicsLayer::create(this);
#ifndef NDEBUG
    m_pageOverlayLayer->setName("LayerTreeHost page overlay content");
#endif

    m_pageOverlayLayer->setDrawsContent(true);
    m_pageOverlayLayer->setSize(m_webPage->size());

    m_rootLayer->addChild(m_pageOverlayLayer.get());
}

void LayerTreeHostCA::destroyPageOverlayLayer()
{
    ASSERT(m_pageOverlayLayer);
    m_pageOverlayLayer->removeFromParent();
    m_pageOverlayLayer = nullptr;
}

} // namespace WebKit
