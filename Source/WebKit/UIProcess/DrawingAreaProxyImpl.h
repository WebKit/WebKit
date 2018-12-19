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

#ifndef DrawingAreaProxyImpl_h
#define DrawingAreaProxyImpl_h

#include "AcceleratedDrawingAreaProxy.h"
#include "BackingStore.h"
#include "DrawingAreaProxy.h"
#include <wtf/RunLoop.h>

namespace WebCore {
class Region;
}

namespace WebKit {

class DrawingAreaProxyImpl final : public AcceleratedDrawingAreaProxy {
public:
    explicit DrawingAreaProxyImpl(WebPageProxy&);
    virtual ~DrawingAreaProxyImpl();

    void paint(BackingStore::PlatformGraphicsContext, const WebCore::IntRect&, WebCore::Region& unpaintedRegion);

private:
    // DrawingAreaProxy
    void setBackingStoreIsDiscardable(bool) override;

    // IPC message handlers
    void update(uint64_t backingStoreStateID, const UpdateInfo&) override;
    void didUpdateBackingStoreState(uint64_t backingStoreStateID, const UpdateInfo&, const LayerTreeContext&) override;
    void exitAcceleratedCompositingMode(uint64_t backingStoreStateID, const UpdateInfo&) override;

    void incorporateUpdate(const UpdateInfo&);

    void enterAcceleratedCompositingMode(const LayerTreeContext&) override;

    void discardBackingStoreSoon();
    void discardBackingStore();

    void dispatchAfterEnsuringDrawing(WTF::Function<void(CallbackBase::Error)>&&) override;

    class DrawingMonitor {
        WTF_MAKE_NONCOPYABLE(DrawingMonitor); WTF_MAKE_FAST_ALLOCATED;
    public:
        DrawingMonitor(WebPageProxy&);
        ~DrawingMonitor();

        void start(WTF::Function<void (CallbackBase::Error)>&&);

    private:
        static int webViewDrawCallback(DrawingMonitor*);

        void stop();
        void didDraw();

        MonotonicTime m_startTime;
        WTF::Function<void (CallbackBase::Error)> m_callback;
        RunLoop::Timer<DrawingMonitor> m_timer;
#if PLATFORM(GTK)
        WebPageProxy& m_webPage;
#endif
    };

    bool m_isBackingStoreDiscardable { true };
    std::unique_ptr<BackingStore> m_backingStore;
    RunLoop::Timer<DrawingAreaProxyImpl> m_discardBackingStoreTimer;
    std::unique_ptr<DrawingMonitor> m_drawingMonitor;
};

} // namespace WebKit

#endif // DrawingAreaProxyImpl_h
