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

#include "DrawingAreaProxy.h"
#include "LayerTreeContext.h"
#include <wtf/RunLoop.h>
#include <wtf/TZoneMalloc.h>

#if !PLATFORM(WPE)
#include "BackingStore.h"
#endif

namespace WebCore {
class Region;
}

namespace WebKit {

class DrawingAreaProxyCoordinatedGraphics final : public DrawingAreaProxy {
    WTF_MAKE_TZONE_ALLOCATED(DrawingAreaProxyCoordinatedGraphics);
    WTF_MAKE_NONCOPYABLE(DrawingAreaProxyCoordinatedGraphics);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(DrawingAreaProxyCoordinatedGraphics);
public:
    DrawingAreaProxyCoordinatedGraphics(WebPageProxy&, WebProcessProxy&);
    virtual ~DrawingAreaProxyCoordinatedGraphics();

#if !PLATFORM(WPE)
    void paint(PlatformPaintContextPtr, const WebCore::IntRect&, WebCore::Region& unpaintedRegion);
#endif

    bool isInAcceleratedCompositingMode() const { return !m_layerTreeContext.isEmpty(); }
    const LayerTreeContext& layerTreeContext() const { return m_layerTreeContext; }

    void dispatchAfterEnsuringDrawing(CompletionHandler<void()>&&);

private:
    // DrawingAreaProxy
    void sizeDidChange() override;
    void deviceScaleFactorDidChange(CompletionHandler<void()>&&) override;
    void setBackingStoreIsDiscardable(bool) override;

#if HAVE(DISPLAY_LINK)
    std::optional<WebCore::FramesPerSecond> displayNominalFramesPerSecond() override;
#endif

#if PLATFORM(GTK)
    void adjustTransientZoom(double scale, WebCore::FloatPoint origin) override;
    void commitTransientZoom(double scale, WebCore::FloatPoint origin) override;
#endif

    // IPC message handlers
    void update(uint64_t backingStoreStateID, UpdateInfo&&) override;
    void enterAcceleratedCompositingMode(uint64_t backingStoreStateID, const LayerTreeContext&) override;
    void exitAcceleratedCompositingMode(uint64_t backingStoreStateID, UpdateInfo&&) override;
    void updateAcceleratedCompositingMode(uint64_t backingStoreStateID, const LayerTreeContext&) override;

    bool shouldSendWheelEventsToEventDispatcher() const override { return true; }

    bool alwaysUseCompositing() const;
    void enterAcceleratedCompositingMode(const LayerTreeContext&);
    void exitAcceleratedCompositingMode();
    void updateAcceleratedCompositingMode(const LayerTreeContext&);

    void sendUpdateGeometry();
    void didUpdateGeometry();

#if !PLATFORM(WPE)
    bool forceUpdateIfNeeded();
    void incorporateUpdate(UpdateInfo&&);
    void discardBackingStoreSoon();
    void discardBackingStore();
#endif

    class DrawingMonitor {
        WTF_MAKE_TZONE_ALLOCATED(DrawingMonitor);
        WTF_MAKE_NONCOPYABLE(DrawingMonitor);
    public:
        DrawingMonitor(WebPageProxy&);
        ~DrawingMonitor();

        void start(CompletionHandler<void()>&&);

    private:
        void stop();

        CompletionHandler<void()> m_callback;
        RunLoop::Timer m_timer;
    };

    // The current layer tree context.
    LayerTreeContext m_layerTreeContext;

    // Whether we're waiting for a DidUpdateGeometry message from the web process.
    bool m_isWaitingForDidUpdateGeometry { false };

    // The last size we sent to the web process.
    WebCore::IntSize m_lastSentSize;


#if !PLATFORM(WPE)
    bool m_isBackingStoreDiscardable { true };
    bool m_inForceUpdate { false };
    std::unique_ptr<BackingStore> m_backingStore;
    RunLoop::Timer m_discardBackingStoreTimer;
#endif
    std::unique_ptr<DrawingMonitor> m_drawingMonitor;
};

} // namespace WebKit

namespace WTF {
template<typename T> struct IsDeprecatedTimerSmartPointerException;
template<> struct IsDeprecatedTimerSmartPointerException<WebKit::DrawingAreaProxyCoordinatedGraphics::DrawingMonitor> : std::true_type { };
}

SPECIALIZE_TYPE_TRAITS_DRAWING_AREA_PROXY(DrawingAreaProxyCoordinatedGraphics, DrawingAreaType::CoordinatedGraphics)
