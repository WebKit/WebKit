/*
 * Copyright (C) 2012-2018 Apple Inc. All rights reserved.
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

#pragma once

#include "CallbackID.h"
#include "DrawingArea.h"
#include "GraphicsLayerCARemote.h"
#include "RemoteLayerTreeTransaction.h"
#include <WebCore/AnimationFrameRate.h>
#include <WebCore/GraphicsLayerClient.h>
#include <WebCore/Timer.h>
#include <atomic>
#include <dispatch/dispatch.h>
#include <wtf/HashMap.h>
#include <wtf/OSObjectPtr.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
class PlatformCALayer;
class ThreadSafeImageBufferFlusher;
}

namespace WebKit {

class RemoteLayerTreeContext;
class RemoteLayerTreeDisplayRefreshMonitor;

class RemoteLayerTreeDrawingArea : public DrawingArea, public WebCore::GraphicsLayerClient {
    friend class RemoteLayerTreeDisplayRefreshMonitor;
public:
    RemoteLayerTreeDrawingArea(WebPage&, const WebPageCreationParameters&);
    virtual ~RemoteLayerTreeDrawingArea();

    TransactionID nextTransactionID() const { return m_currentTransactionID.next(); }
    TransactionID lastCommittedTransactionID() const { return m_currentTransactionID; }

protected:
    void updateRendering();

private:
    // DrawingArea
    void setNeedsDisplay() override;
    void setNeedsDisplayInRect(const WebCore::IntRect&) override;
    void scroll(const WebCore::IntRect& scrollRect, const WebCore::IntSize& scrollDelta) override;
    void updateGeometry(const WebCore::IntSize& viewSize, bool flushSynchronously, const WTF::MachSendRight& fencePort) override;
    void adoptDisplayRefreshMonitorsFromDrawingArea(DrawingArea&) override;

    WebCore::GraphicsLayerFactory* graphicsLayerFactory() override;
    void setRootCompositingLayer(WebCore::GraphicsLayer*) override;
    void triggerRenderingUpdate() override;
    void attachViewOverlayGraphicsLayer(WebCore::GraphicsLayer*) override;

    void addTransactionCallbackID(CallbackID) override;
    virtual void willCommitLayerTree(RemoteLayerTreeTransaction&) { };

    RefPtr<WebCore::DisplayRefreshMonitor> createDisplayRefreshMonitor(WebCore::PlatformDisplayID) final;
    void willDestroyDisplayRefreshMonitor(WebCore::DisplayRefreshMonitor*);
    void setPreferredFramesPerSecond(WebCore::FramesPerSecond);

    bool shouldUseTiledBackingForFrameView(const WebCore::FrameView&) const override;

    void updatePreferences(const WebPreferencesStore&) override;

    bool supportsAsyncScrolling() const override { return true; }
    bool usesDelegatedPageScaling() const override { return true; }
    WebCore::DelegatedScrollingMode delegatedScrollingMode() const override;

    void setLayerTreeStateIsFrozen(bool) override;
    bool layerTreeStateIsFrozen() const override { return m_isRenderingSuspended; }

    void forceRepaint() override;
    void forceRepaintAsync(WebPage&, CompletionHandler<void()>&&) override;

    void setViewExposedRect(std::optional<WebCore::FloatRect>) override;
    std::optional<WebCore::FloatRect> viewExposedRect() const override { return m_viewExposedRect; }

    void acceleratedAnimationDidStart(uint64_t layerID, const String& key, MonotonicTime startTime) override;
    void acceleratedAnimationDidEnd(uint64_t layerID, const String& key) override;

    WebCore::FloatRect exposedContentRect() const override;
    void setExposedContentRect(const WebCore::FloatRect&) override;

    void displayDidRefresh() override;

    void setDeviceScaleFactor(float) override;

    void mainFrameContentSizeChanged(const WebCore::IntSize&) override;

    void activityStateDidChange(OptionSet<WebCore::ActivityState::Flag> changed, ActivityStateChangeID, CompletionHandler<void()>&&) override;

    bool addMilestonesToDispatch(OptionSet<WebCore::LayoutMilestone>) override;

    void updateRootLayers();

    void addCommitHandlers();
    void startRenderingUpdateTimer();
    void didCompleteRenderingUpdateDisplay() override;

    WebCore::TiledBacking* mainFrameTiledBacking() const;

    TransactionID takeNextTransactionID() { return m_currentTransactionID.increment(); }

    void tryMarkLayersVolatile(CompletionHandler<void(bool succeeded)>&&) final;

    void adoptLayersFromDrawingArea(DrawingArea&) final;

    class BackingStoreFlusher : public ThreadSafeRefCounted<BackingStoreFlusher> {
    public:
        static Ref<BackingStoreFlusher> create(IPC::Connection*, UniqueRef<IPC::Encoder>&&, Vector<std::unique_ptr<WebCore::ThreadSafeImageBufferFlusher>>);

        void flush();
        bool hasFlushed() const { return m_hasFlushed; }

    private:
        BackingStoreFlusher(IPC::Connection*, UniqueRef<IPC::Encoder>&&, Vector<std::unique_ptr<WebCore::ThreadSafeImageBufferFlusher>>);

        RefPtr<IPC::Connection> m_connection;
        std::unique_ptr<IPC::Encoder> m_commitEncoder;
        Vector<std::unique_ptr<WebCore::ThreadSafeImageBufferFlusher>> m_flushers;

        std::atomic<bool> m_hasFlushed;
    };

    std::unique_ptr<RemoteLayerTreeContext> m_remoteLayerTreeContext;
    Ref<WebCore::GraphicsLayer> m_rootLayer;

    std::optional<WebCore::FloatRect> m_viewExposedRect;

    WebCore::Timer m_updateRenderingTimer;
    bool m_isRenderingSuspended { false };
    bool m_hasDeferredRenderingUpdate { false };
    bool m_nextRenderingUpdateRequiresSynchronousImageDecoding { false };
    bool m_inUpdateRendering { false };

    bool m_waitingForBackingStoreSwap { false };
    bool m_deferredRenderingUpdateWhileWaitingForBackingStoreSwap { false };

    OSObjectPtr<dispatch_queue_t> m_commitQueue;
    RefPtr<BackingStoreFlusher> m_pendingBackingStoreFlusher;

    HashSet<RemoteLayerTreeDisplayRefreshMonitor*> m_displayRefreshMonitors;
    HashSet<RemoteLayerTreeDisplayRefreshMonitor*>* m_displayRefreshMonitorsToNotify { nullptr };

    TransactionID m_currentTransactionID;
    Vector<RemoteLayerTreeTransaction::TransactionCallbackID> m_pendingCallbackIDs;
    ActivityStateChangeID m_activityStateChangeID { ActivityStateChangeAsynchronous };

    OptionSet<WebCore::LayoutMilestone> m_pendingNewlyReachedPaintingMilestones;

    RefPtr<WebCore::GraphicsLayer> m_contentLayer;
    RefPtr<WebCore::GraphicsLayer> m_viewOverlayRootLayer;
};

inline bool RemoteLayerTreeDrawingArea::addMilestonesToDispatch(OptionSet<WebCore::LayoutMilestone> paintMilestones)
{
    m_pendingNewlyReachedPaintingMilestones.add(paintMilestones);
    return true;
}

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_DRAWING_AREA(RemoteLayerTreeDrawingArea, DrawingAreaType::RemoteLayerTree)
