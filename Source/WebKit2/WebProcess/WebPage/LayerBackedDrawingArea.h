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

#ifndef LayerBackedDrawingArea_h
#define LayerBackedDrawingArea_h

#if USE(ACCELERATED_COMPOSITING)

#include "DrawingArea.h"
#include "RunLoop.h"
#include <WebCore/IntPoint.h>
#include <WebCore/GraphicsLayerClient.h>

#if PLATFORM(MAC)
#include <wtf/RetainPtr.h>
#ifdef __OBJC__
@class CALayer;
#else
class CALayer;
#endif
typedef struct __WKCARemoteLayerClientRef *WKCARemoteLayerClientRef;
#endif

namespace WebCore {
    class GraphicsContext;
    class GraphicsLayer;
}

namespace WebKit {

class LayerBackedDrawingArea : public DrawingArea, private WebCore::GraphicsLayerClient {
public:
    LayerBackedDrawingArea(DrawingAreaInfo::Identifier identifier, WebPage*);
    virtual ~LayerBackedDrawingArea();

    virtual void setNeedsDisplay(const WebCore::IntRect&);
    virtual void scroll(const WebCore::IntRect& scrollRect, const WebCore::IntSize& scrollOffset);
    virtual void display();

    virtual void pageBackgroundTransparencyChanged();

    virtual void attachCompositingContext();
    virtual void detachCompositingContext();
    virtual void setRootCompositingLayer(WebCore::GraphicsLayer*);
    virtual void scheduleCompositingLayerSync();
    virtual void syncCompositingLayers();

    virtual void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);

private:

    // GraphicsLayerClient
    virtual void notifyAnimationStarted(const WebCore::GraphicsLayer*, double /*time*/) { }
    virtual void notifySyncRequired(const WebCore::GraphicsLayer*) { }
public:
    virtual void paintContents(const WebCore::GraphicsLayer*, WebCore::GraphicsContext&, WebCore::GraphicsLayerPaintingPhase, const WebCore::IntRect& inClip);
private:
    virtual bool showDebugBorders() const;
    virtual bool showRepaintCounter() const;

#if PLATFORM(MAC)
    virtual void onPageClose();
#endif

    void scheduleDisplay();
    
    // CoreIPC message handlers.
    void setSize(const WebCore::IntSize& viewSize);
    void suspendPainting();
    void resumePainting();
    void didUpdate();
    
    void platformInit();
    void platformClear();

#if PLATFORM(MAC)
    void setUpUpdateLayoutRunLoopObserver();
    void scheduleUpdateLayoutRunLoopObserver();
    void removeUpdateLayoutRunLoopObserver();

    static void updateLayoutRunLoopObserverCallback(CFRunLoopObserverRef, CFRunLoopActivity, void*);
    void updateLayoutRunLoopObserverFired();
#endif

    RunLoop::Timer<LayerBackedDrawingArea> m_syncTimer;

    OwnPtr<WebCore::GraphicsLayer> m_hostingLayer;
    OwnPtr<WebCore::GraphicsLayer> m_backingLayer;
#if PLATFORM(MAC)
#if HAVE(HOSTED_CORE_ANIMATION)
    RetainPtr<WKCARemoteLayerClientRef> m_remoteLayerRef;
#endif
    RetainPtr<CFRunLoopObserverRef> m_updateLayoutRunLoopObserver;
#endif

    bool m_attached;
    bool m_shouldPaint;
};

} // namespace WebKit

#endif // USE(ACCELERATED_COMPOSITING)

#endif // LayerBackedDrawingArea_h
