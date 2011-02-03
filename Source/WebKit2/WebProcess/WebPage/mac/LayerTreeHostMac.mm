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
#import "LayerTreeHostMac.h"

#import "WebPage.h"
#import "WebProcess.h"
#import <WebCore/Frame.h>
#import <WebCore/FrameView.h>
#import <WebCore/Page.h>
#import <WebCore/Settings.h>
#import <WebKitSystemInterface.h>

using namespace WebCore;

namespace WebKit {

PassRefPtr<LayerTreeHostMac> LayerTreeHostMac::create(WebPage* webPage, GraphicsLayer* graphicsLayer)
{
    return adoptRef(new LayerTreeHostMac(webPage, graphicsLayer));
}

LayerTreeHostMac::LayerTreeHostMac(WebPage* webPage, GraphicsLayer* graphicsLayer)
    : LayerTreeHost(webPage)
    , m_isValid(true)
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

    // Add the accelerated layer tree hierarchy.
    m_rootLayer->addChild(graphicsLayer);
    
    WKCARemoteLayerClientSetLayer(m_remoteLayerClient.get(), m_rootLayer->platformLayer());

    scheduleLayerFlush();

    m_layerTreeContext.contextID = WKCARemoteLayerClientGetClientId(m_remoteLayerClient.get());
}

LayerTreeHostMac::~LayerTreeHostMac()
{
    ASSERT(!m_isValid);
    ASSERT(!m_flushPendingLayerChangesRunLoopObserver);
    ASSERT(!m_remoteLayerClient);
    ASSERT(!m_rootLayer);
}

const LayerTreeContext& LayerTreeHostMac::layerTreeContext()
{
    return m_layerTreeContext;
}

void LayerTreeHostMac::scheduleLayerFlush()
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

void LayerTreeHostMac::invalidate()
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

void LayerTreeHostMac::notifyAnimationStarted(const WebCore::GraphicsLayer*, double time)
{
}

void LayerTreeHostMac::notifySyncRequired(const WebCore::GraphicsLayer*)
{
}

void LayerTreeHostMac::paintContents(const GraphicsLayer* graphicsLayer, GraphicsContext& graphicsContext, GraphicsLayerPaintingPhase, const IntRect& clipRect)
{
}

bool LayerTreeHostMac::showDebugBorders() const
{
    return m_webPage->corePage()->settings()->showDebugBorders();
}

bool LayerTreeHostMac::showRepaintCounter() const
{
    return m_webPage->corePage()->settings()->showRepaintCounter();
}

void LayerTreeHostMac::flushPendingLayerChangesRunLoopObserverCallback(CFRunLoopObserverRef, CFRunLoopActivity, void* context)
{
    static_cast<LayerTreeHostMac*>(context)->flushPendingLayerChangesRunLoopObserverCallback();
}

void LayerTreeHostMac::flushPendingLayerChangesRunLoopObserverCallback()
{
    if (!flushPendingLayerChanges())
        return;

    // We successfully flushed the pending layer changes, remove the run loop observer.
    ASSERT(m_flushPendingLayerChangesRunLoopObserver);
    CFRunLoopObserverInvalidate(m_flushPendingLayerChangesRunLoopObserver.get());
    m_flushPendingLayerChangesRunLoopObserver = 0;
}

bool LayerTreeHostMac::flushPendingLayerChanges()
{
    m_rootLayer->syncCompositingStateForThisLayerOnly();

    return m_webPage->corePage()->mainFrame()->view()->syncCompositingStateIncludingSubframes();
}

    
} // namespace WebKit
