/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#if USE(ACCELERATED_COMPOSITING)

#include "LayerBackedDrawingArea.h"

#include "DrawingAreaProxyMessageKinds.h"
#include "WebKitSystemInterface.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/Frame.h>
#include <WebCore/FrameView.h>
#include <WebCore/GraphicsLayer.h>
#include <WebCore/Page.h>

using namespace WebCore;

namespace WebKit {

void LayerBackedDrawingArea::platformInit()
{
    setUpUpdateLayoutRunLoopObserver();

    [m_backingLayer->platformLayer() setGeometryFlipped:YES];
#if HAVE(HOSTED_CORE_ANIMATION)
    attachCompositingContext();
#endif

    scheduleCompositingLayerSync();
}

void LayerBackedDrawingArea::platformClear()
{
    if (m_updateLayoutRunLoopObserver) {
        CFRunLoopObserverInvalidate(m_updateLayoutRunLoopObserver.get());
        m_updateLayoutRunLoopObserver = 0;
    }

#if HAVE(HOSTED_CORE_ANIMATION)
    WKCARemoteLayerClientInvalidate(m_remoteLayerRef.get());
    m_remoteLayerRef = 0;
#endif

    m_attached = false;
}

void LayerBackedDrawingArea::attachCompositingContext()
{
    if (m_attached)
        return;

    m_attached = true;

#if HAVE(HOSTED_CORE_ANIMATION)
    mach_port_t serverPort = WebProcess::shared().compositingRenderServerPort();
    m_remoteLayerRef = WKCARemoteLayerClientMakeWithServerPort(serverPort);
    WKCARemoteLayerClientSetLayer(m_remoteLayerRef.get(), m_backingLayer->platformLayer());
    
    uint32_t contextID = WKCARemoteLayerClientGetClientId(m_remoteLayerRef.get());
    WebProcess::shared().connection()->sendSync(DrawingAreaProxyMessage::AttachCompositingContext, m_webPage->pageID(), CoreIPC::In(contextID), CoreIPC::Out(), CoreIPC::Connection::NoTimeout);
#endif
}

void LayerBackedDrawingArea::detachCompositingContext()
{
    m_backingLayer->removeAllChildren();

    scheduleCompositingLayerSync();
}

void LayerBackedDrawingArea::setRootCompositingLayer(WebCore::GraphicsLayer* layer)
{
    m_backingLayer->removeAllChildren();
    if (layer)
        m_backingLayer->addChild(layer);

    scheduleCompositingLayerSync();
}

void LayerBackedDrawingArea::scheduleCompositingLayerSync()
{
//    if (m_syncTimer.isActive())
//        return;
//
//    m_syncTimer.startOneShot(0);

    scheduleUpdateLayoutRunLoopObserver();
}

void LayerBackedDrawingArea::syncCompositingLayers()
{
    m_backingLayer->syncCompositingStateForThisLayerOnly();

    bool didSync = m_webPage->corePage()->mainFrame()->view()->syncCompositingStateRecursive();
    if (!didSync) {
    
    }
}

void LayerBackedDrawingArea::setUpUpdateLayoutRunLoopObserver()
{
    if (m_updateLayoutRunLoopObserver)
        return;

    // Run before Core Animations commit observer, which has order 2000000.
    const CFIndex runLoopOrder = 2000000 - 1;
    CFRunLoopObserverContext context = { 0, this, 0, 0, 0 };
    m_updateLayoutRunLoopObserver.adoptCF(CFRunLoopObserverCreate(0,
        kCFRunLoopBeforeWaiting | kCFRunLoopExit, true /* repeats */,
        runLoopOrder, updateLayoutRunLoopObserverCallback, &context));
}

void LayerBackedDrawingArea::scheduleUpdateLayoutRunLoopObserver()
{
    CFRunLoopRef currentRunLoop = CFRunLoopGetCurrent();
    CFRunLoopWakeUp(currentRunLoop);

    if (CFRunLoopContainsObserver(currentRunLoop, m_updateLayoutRunLoopObserver.get(), kCFRunLoopCommonModes))
        return;

    CFRunLoopAddObserver(currentRunLoop, m_updateLayoutRunLoopObserver.get(), kCFRunLoopCommonModes);
}

void LayerBackedDrawingArea::removeUpdateLayoutRunLoopObserver()
{
    // FIXME: cache the run loop ref?
    CFRunLoopRemoveObserver(CFRunLoopGetCurrent(), m_updateLayoutRunLoopObserver.get(), kCFRunLoopCommonModes);
}

void LayerBackedDrawingArea::updateLayoutRunLoopObserverCallback(CFRunLoopObserverRef, CFRunLoopActivity, void* info)
{
    // Keep the drawing area alive while running the callback, since that does layout,
    // which might replace this drawing area with one of another type.
    RefPtr<LayerBackedDrawingArea> drawingArea = reinterpret_cast<LayerBackedDrawingArea*>(info);
    drawingArea->updateLayoutRunLoopObserverFired();
}

void LayerBackedDrawingArea::updateLayoutRunLoopObserverFired()
{
    m_webPage->layoutIfNeeded();
    
    if (m_attached)
        syncCompositingLayers();
}

} // namespace WebKit

#endif // USE(ACCELERATED_COMPOSITING)
