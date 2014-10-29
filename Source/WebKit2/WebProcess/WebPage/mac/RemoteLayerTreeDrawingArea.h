/*
 * Copyright (C) 2012-2014 Apple Inc. All rights reserved.
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

#ifndef RemoteLayerTreeDrawingArea_h
#define RemoteLayerTreeDrawingArea_h

#include "DrawingArea.h"
#include "GraphicsLayerCARemote.h"
#include "RemoteLayerTreeTransaction.h"
#include <WebCore/GraphicsLayerClient.h>
#include <WebCore/Timer.h>
#include <atomic>
#include <dispatch/dispatch.h>
#include <wtf/HashMap.h>

namespace WebCore {
class PlatformCALayer;
}

namespace IPC {
class MessageEncoder;
}

namespace WebKit {

class RemoteLayerTreeContext;
class RemoteLayerTreeDisplayRefreshMonitor;

class RemoteLayerTreeDrawingArea : public DrawingArea, public WebCore::GraphicsLayerClient {
    friend class RemoteLayerTreeDisplayRefreshMonitor;
public:
    RemoteLayerTreeDrawingArea(WebPage&, const WebPageCreationParameters&);
    virtual ~RemoteLayerTreeDrawingArea();

    uint64_t nextTransactionID() const { return m_currentTransactionID + 1; }

private:
    // DrawingArea
    virtual void setNeedsDisplay() override;
    virtual void setNeedsDisplayInRect(const WebCore::IntRect&) override;
    virtual void scroll(const WebCore::IntRect& scrollRect, const WebCore::IntSize& scrollDelta) override;
    virtual void updateGeometry(const WebCore::IntSize& viewSize, const WebCore::IntSize& layerPosition) override;

    virtual WebCore::GraphicsLayerFactory* graphicsLayerFactory() override;
    virtual void setRootCompositingLayer(WebCore::GraphicsLayer*) override;
    virtual void scheduleCompositingLayerFlush() override;
    virtual void scheduleCompositingLayerFlushImmediately() override;
    virtual void attachViewOverlayGraphicsLayer(WebCore::Frame*, WebCore::GraphicsLayer*) override;

    virtual void addTransactionCallbackID(uint64_t callbackID) override;

    virtual PassRefPtr<WebCore::DisplayRefreshMonitor> createDisplayRefreshMonitor(PlatformDisplayID) override;
    void willDestroyDisplayRefreshMonitor(WebCore::DisplayRefreshMonitor*);

    virtual bool shouldUseTiledBackingForFrameView(const WebCore::FrameView*) override;

    virtual void updatePreferences(const WebPreferencesStore&) override;

    virtual bool supportsAsyncScrolling() override { return true; }

    virtual void setLayerTreeStateIsFrozen(bool) override;

    virtual void forceRepaint() override;
    virtual bool forceRepaintAsync(uint64_t) override { return false; }

    virtual void setExposedRect(const WebCore::FloatRect&) override;
    virtual WebCore::FloatRect exposedRect() const override { return m_scrolledExposedRect; }

    virtual void acceleratedAnimationDidStart(uint64_t layerID, const String& key, double startTime) override;

#if PLATFORM(IOS)
    virtual void setExposedContentRect(const WebCore::FloatRect&) override;
#endif

    virtual void didUpdate() override;

#if PLATFORM(IOS)
    virtual void setDeviceScaleFactor(float) override;
#endif

    virtual void mainFrameContentSizeChanged(const WebCore::IntSize&) override;

    virtual void viewStateDidChange(WebCore::ViewState::Flags changed, bool wantsDidUpdateViewState) override;

    virtual bool adjustLayerFlushThrottling(WebCore::LayerFlushThrottleState::Flags) override;

    // GraphicsLayerClient
    virtual void notifyAnimationStarted(const WebCore::GraphicsLayer*, double time) override { }
    virtual void notifyFlushRequired(const WebCore::GraphicsLayer*) override { }
    virtual void paintContents(const WebCore::GraphicsLayer*, WebCore::GraphicsContext&, WebCore::GraphicsLayerPaintingPhase, const WebCore::FloatRect& inClip) override { }

    void updateScrolledExposedRect();
    void updateRootLayers();

    void layerFlushTimerFired(WebCore::Timer<RemoteLayerTreeDrawingArea>*);
    void flushLayers();

    WebCore::TiledBacking* mainFrameTiledBacking() const;

    uint64_t takeNextTransactionID() { return ++m_currentTransactionID; }

    virtual bool markLayersVolatileImmediatelyIfPossible() override;

    class BackingStoreFlusher : public ThreadSafeRefCounted<BackingStoreFlusher> {
    public:
        static PassRefPtr<BackingStoreFlusher> create(IPC::Connection*, std::unique_ptr<IPC::MessageEncoder>, Vector<RetainPtr<CGContextRef>>);

        void flush();
        bool hasFlushed() const { return m_hasFlushed; }

    private:
        BackingStoreFlusher(IPC::Connection*, std::unique_ptr<IPC::MessageEncoder>, Vector<RetainPtr<CGContextRef>>);

        RefPtr<IPC::Connection> m_connection;
        std::unique_ptr<IPC::MessageEncoder> m_commitEncoder;
        Vector<RetainPtr<CGContextRef>> m_contextsToFlush;

        std::atomic<bool> m_hasFlushed;
    };

    std::unique_ptr<RemoteLayerTreeContext> m_remoteLayerTreeContext;
    std::unique_ptr<WebCore::GraphicsLayer> m_rootLayer;

    WebCore::IntSize m_viewSize;

    WebCore::FloatRect m_exposedRect;
    WebCore::FloatRect m_scrolledExposedRect;

    WebCore::Timer<RemoteLayerTreeDrawingArea> m_layerFlushTimer;
    bool m_isFlushingSuspended;
    bool m_hasDeferredFlush;
    bool m_isThrottlingLayerFlushes;
    bool m_isLayerFlushThrottlingTemporarilyDisabledForInteraction;
    bool m_isInitialThrottledLayerFlush;

    bool m_waitingForBackingStoreSwap;
    bool m_hadFlushDeferredWhileWaitingForBackingStoreSwap;

    dispatch_queue_t m_commitQueue;
    RefPtr<BackingStoreFlusher> m_pendingBackingStoreFlusher;

    HashSet<RemoteLayerTreeDisplayRefreshMonitor*> m_displayRefreshMonitors;
    HashSet<RemoteLayerTreeDisplayRefreshMonitor*>* m_displayRefreshMonitorsToNotify;

    uint64_t m_currentTransactionID;
    Vector<RemoteLayerTreeTransaction::TransactionCallbackID> m_pendingCallbackIDs;

    WebCore::GraphicsLayer* m_contentLayer;
    WebCore::GraphicsLayer* m_viewOverlayRootLayer;
};

DRAWING_AREA_TYPE_CASTS(RemoteLayerTreeDrawingArea, type() == DrawingAreaTypeRemoteLayerTree);

} // namespace WebKit

#endif // RemoteLayerTreeDrawingArea_h
