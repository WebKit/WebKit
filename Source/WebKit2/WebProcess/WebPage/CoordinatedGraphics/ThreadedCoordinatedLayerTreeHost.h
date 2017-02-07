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

#ifndef ThreadedCoordinatedLayerTreeHost_h
#define ThreadedCoordinatedLayerTreeHost_h

#if USE(COORDINATED_GRAPHICS_THREADED)

#include "CoordinatedLayerTreeHost.h"
#include "SimpleViewportController.h"
#include "ThreadedCompositor.h"
#include <wtf/OptionSet.h>

namespace WebCore {
class GraphicsContext;
class GraphicsLayer;
struct CoordinatedGraphicsState;
}

namespace WebKit {

class AcceleratedSurface;
class WebPage;

class ThreadedCoordinatedLayerTreeHost final : public CoordinatedLayerTreeHost {
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
    bool forceRepaintAsync(uint64_t callbackID) override { return false; }

    void setIsDiscardable(bool) override;

#if PLATFORM(GTK) && PLATFORM(X11) &&  !USE(REDIRECTED_XCOMPOSITE_WINDOW)
    void setNativeSurfaceHandleForCompositing(uint64_t) override;
#endif

    class CompositorClient final : public ThreadedCompositor::Client {
        WTF_MAKE_NONCOPYABLE(CompositorClient);
    public:
        CompositorClient(ThreadedCoordinatedLayerTreeHost& layerTreeHost)
            : m_layerTreeHost(layerTreeHost)
        {
        }

    private:
        void renderNextFrame() override
        {
            m_layerTreeHost.renderNextFrame();
        }

        void commitScrollOffset(uint32_t layerID, const WebCore::IntSize& offset) override
        {
            m_layerTreeHost.commitScrollOffset(layerID, offset);
        }

        ThreadedCoordinatedLayerTreeHost& m_layerTreeHost;
    };

    void didChangeViewport();

    // CompositingCoordinator::Client
    void didFlushRootLayer(const WebCore::FloatRect&) override { }
    void commitSceneState(const WebCore::CoordinatedGraphicsState&) override;

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

#endif // ThreadedCoordinatedLayerTreeHost_h
