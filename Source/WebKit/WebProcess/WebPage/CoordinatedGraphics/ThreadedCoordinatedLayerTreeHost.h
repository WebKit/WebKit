/*
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2013 Company 100, Inc.
 * Copyright (C) 2014 Igalia S.L.
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

#if USE(COORDINATED_GRAPHICS_THREADED)

#include "AcceleratedSurface.h"
#include "CoordinatedLayerTreeHost.h"
#include "SimpleViewportController.h"
#include "ThreadedCompositor.h"
#include "ThreadedDisplayRefreshMonitor.h"
#include <wtf/OptionSet.h>

namespace WebCore {
class GraphicsContext;
class GraphicsLayer;
struct CoordinatedGraphicsState;
}

namespace WebKit {

class WebPage;

class ThreadedCoordinatedLayerTreeHost final : public CoordinatedLayerTreeHost, public AcceleratedSurface::Client {
public:
    static Ref<ThreadedCoordinatedLayerTreeHost> create(WebPage&);
    virtual ~ThreadedCoordinatedLayerTreeHost();

private:
    explicit ThreadedCoordinatedLayerTreeHost(WebPage&);

    void scrollNonCompositedContents(const WebCore::IntRect& scrollRect) override;
    void sizeDidChange(const WebCore::IntSize&) override;
    void deviceOrPageScaleFactorChanged() override;
    void pageBackgroundTransparencyChanged() override;

    void contentsSizeChanged(const WebCore::IntSize&) override;
    void didChangeViewportAttributes(WebCore::ViewportAttributes&&) override;

    void invalidate() override;

    void forceRepaint() override;

    void setIsDiscardable(bool) override;

#if PLATFORM(GTK) && PLATFORM(X11) &&  !USE(REDIRECTED_XCOMPOSITE_WINDOW)
    void setNativeSurfaceHandleForCompositing(uint64_t) override;
#endif

    class CompositorClient final : public ThreadedCompositor::Client, public ThreadedDisplayRefreshMonitor::Client  {
        WTF_MAKE_NONCOPYABLE(CompositorClient);
    public:
        CompositorClient(ThreadedCoordinatedLayerTreeHost& layerTreeHost)
            : m_layerTreeHost(layerTreeHost)
        {
        }

    private:
        uint64_t nativeSurfaceHandleForCompositing() override
        {
            return m_layerTreeHost.nativeSurfaceHandleForCompositing();
        }

        void didDestroyGLContext() override
        {
            m_layerTreeHost.didDestroyGLContext();
        }

        void resize(const IntSize& size)
        {
            if (m_layerTreeHost.m_surface)
                m_layerTreeHost.m_surface->clientResize(size);
        }

        void willRenderFrame() override
        {
            m_layerTreeHost.willRenderFrame();
        }

        void didRenderFrame() override
        {
            m_layerTreeHost.didRenderFrame();
        }

        void requestDisplayRefreshMonitorUpdate() override
        {
            m_layerTreeHost.requestDisplayRefreshMonitorUpdate();
        }

        void handleDisplayRefreshMonitorUpdate(bool hasBeenRescheduled)
        {
            m_layerTreeHost.handleDisplayRefreshMonitorUpdate(hasBeenRescheduled);
        }

        ThreadedCoordinatedLayerTreeHost& m_layerTreeHost;
    };

    void didChangeViewport();

    // CompositingCoordinator::Client
    void didFlushRootLayer(const WebCore::FloatRect&) override { }
    void commitSceneState(const WebCore::CoordinatedGraphicsState&) override;

#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
    RefPtr<WebCore::DisplayRefreshMonitor> createDisplayRefreshMonitor(WebCore::PlatformDisplayID) override;
#endif

    // AcceleratedSurface::Client
    void frameComplete() override;

    uint64_t nativeSurfaceHandleForCompositing();
    void didDestroyGLContext();
    void willRenderFrame();
    void didRenderFrame();
    void requestDisplayRefreshMonitorUpdate();
    void handleDisplayRefreshMonitorUpdate(bool);

    enum class DiscardableSyncActions {
        UpdateSize = 1 << 1,
        UpdateViewport = 1 << 2,
        UpdateScale = 1 << 3,
        UpdateBackground = 1 << 4
    };

    CompositorClient m_compositorClient;
    std::unique_ptr<AcceleratedSurface> m_surface;
    RefPtr<ThreadedCompositor> m_compositor;
    SimpleViewportController m_viewportController;
    float m_lastPageScaleFactor { 1 };
    WebCore::IntPoint m_lastScrollPosition;
    bool m_isDiscardable { false };
    OptionSet<DiscardableSyncActions> m_discardableSyncActions;
};

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS_THREADED)
