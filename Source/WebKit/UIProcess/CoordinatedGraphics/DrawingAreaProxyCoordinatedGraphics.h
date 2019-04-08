/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2016-2019 Igalia S.L.
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

#include "BackingStore.h"
#include "DrawingAreaProxy.h"
#include "LayerTreeContext.h"
#include <wtf/RunLoop.h>

namespace WebCore {
class Region;
}

namespace WebKit {

class DrawingAreaProxyCoordinatedGraphics final : public DrawingAreaProxy {
public:
    DrawingAreaProxyCoordinatedGraphics(WebPageProxy&, WebProcessProxy&);
    virtual ~DrawingAreaProxyCoordinatedGraphics();

#if !PLATFORM(WPE)
    void paint(BackingStore::PlatformGraphicsContext, const WebCore::IntRect&, WebCore::Region& unpaintedRegion);
#endif

    bool isInAcceleratedCompositingMode() const { return alwaysUseCompositing() || !m_layerTreeContext.isEmpty(); }

#if USE(TEXTURE_MAPPER_GL) && PLATFORM(GTK) && PLATFORM(X11) && !USE(REDIRECTED_XCOMPOSITE_WINDOW)
    void setNativeSurfaceHandleForCompositing(uint64_t);
    void destroyNativeSurfaceHandleForCompositing();
#endif

private:
    // DrawingAreaProxy
    void sizeDidChange() override;
    void deviceScaleFactorDidChange() override;
    void waitForBackingStoreUpdateOnNextPaint() override;
    void setBackingStoreIsDiscardable(bool) override;

    // IPC message handlers
    void update(uint64_t backingStoreStateID, const UpdateInfo&) override;
    void didUpdateBackingStoreState(uint64_t backingStoreStateID, const UpdateInfo&, const LayerTreeContext&) override;
    void enterAcceleratedCompositingMode(uint64_t backingStoreStateID, const LayerTreeContext&) override;
    void exitAcceleratedCompositingMode(uint64_t backingStoreStateID, const UpdateInfo&) override;
    void updateAcceleratedCompositingMode(uint64_t backingStoreStateID, const LayerTreeContext&) override;

#if !PLATFORM(WPE)
    void incorporateUpdate(const UpdateInfo&);
#endif

    bool alwaysUseCompositing() const;
    void enterAcceleratedCompositingMode(const LayerTreeContext&);
    void exitAcceleratedCompositingMode();
    void updateAcceleratedCompositingMode(const LayerTreeContext&);

    enum RespondImmediatelyOrNot { DoNotRespondImmediately, RespondImmediately };
    void backingStoreStateDidChange(RespondImmediatelyOrNot);
    void sendUpdateBackingStoreState(RespondImmediatelyOrNot);
    void waitForAndDispatchDidUpdateBackingStoreState();

#if !PLATFORM(WPE)
    void discardBackingStoreSoon();
    void discardBackingStore();
#endif

    void dispatchAfterEnsuringDrawing(WTF::Function<void(CallbackBase::Error)>&&) override;

    class DrawingMonitor {
        WTF_MAKE_NONCOPYABLE(DrawingMonitor); WTF_MAKE_FAST_ALLOCATED;
    public:
        DrawingMonitor(WebPageProxy&);
        ~DrawingMonitor();

        void start(WTF::Function<void(CallbackBase::Error)>&&);

    private:
        static int webViewDrawCallback(DrawingMonitor*);

        void stop();
        void didDraw();

        MonotonicTime m_startTime;
        WTF::Function<void(CallbackBase::Error)> m_callback;
        RunLoop::Timer<DrawingMonitor> m_timer;
#if PLATFORM(GTK)
        WebPageProxy& m_webPage;
#endif
    };

    // The state ID corresponding to our current backing store. Updated whenever we allocate
    // a new backing store. Any messages received that correspond to an earlier state are ignored,
    // as they don't apply to our current backing store.
    uint64_t m_currentBackingStoreStateID { 0 };

    // The next backing store state ID we will request the web process update to. Incremented
    // whenever our state changes in a way that will require a new backing store to be allocated.
    uint64_t m_nextBackingStoreStateID { 0 };

    // The current layer tree context.
    LayerTreeContext m_layerTreeContext;

    // Whether we've sent a UpdateBackingStoreState message and are now waiting for a DidUpdateBackingStoreState message.
    // Used to throttle UpdateBackingStoreState messages so we don't send them faster than the Web process can handle.
    bool m_isWaitingForDidUpdateBackingStoreState { false };

    // For a new Drawing Area don't draw anything until the WebProcess has sent over the first content.
    bool m_hasReceivedFirstUpdate { false };

#if USE(TEXTURE_MAPPER_GL) && PLATFORM(GTK) && PLATFORM(X11) && !USE(REDIRECTED_XCOMPOSITE_WINDOW)
    uint64_t m_pendingNativeSurfaceHandleForCompositing { 0 };
#endif

#if !PLATFORM(WPE)
    bool m_isBackingStoreDiscardable { true };
    std::unique_ptr<BackingStore> m_backingStore;
    RunLoop::Timer<DrawingAreaProxyCoordinatedGraphics> m_discardBackingStoreTimer;
#endif
    std::unique_ptr<DrawingMonitor> m_drawingMonitor;
};

} // namespace WebKit
