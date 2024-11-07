/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2019 Igalia S.L.
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

#include "DrawingArea.h"
#include <WebCore/Region.h>
#include <wtf/RunLoop.h>

namespace WebCore {
class GraphicsContext;
}

namespace WebKit {

struct UpdateInfo;

class DrawingAreaCoordinatedGraphics final : public DrawingArea {
public:
    static Ref<DrawingAreaCoordinatedGraphics> create(WebPage& webPage, const WebPageCreationParameters& parameters)
    {
        return adoptRef(*new DrawingAreaCoordinatedGraphics(webPage, parameters));
    }

    virtual ~DrawingAreaCoordinatedGraphics();

private:
    DrawingAreaCoordinatedGraphics(WebPage&, const WebPageCreationParameters&);

    // DrawingArea
    void setNeedsDisplay() override;
    void setNeedsDisplayInRect(const WebCore::IntRect&) override;
    void scroll(const WebCore::IntRect& scrollRect, const WebCore::IntSize& scrollDelta) override;
    void updateRenderingWithForcedRepaint() override;
    void updateRenderingWithForcedRepaintAsync(WebPage&, CompletionHandler<void()>&&) override;

    void setLayerTreeStateIsFrozen(bool) override;
    bool layerTreeStateIsFrozen() const override { return m_layerTreeStateIsFrozen; }

    void updatePreferences(const WebPreferencesStore&) override;
    void sendEnterAcceleratedCompositingModeIfNeeded() override;

#if USE(COORDINATED_GRAPHICS) || USE(TEXTURE_MAPPER)
    void deviceOrPageScaleFactorChanged() override;
    bool enterAcceleratedCompositingModeIfNeeded() override;
    void backgroundColorDidChange() override;
#endif

#if PLATFORM(WPE) && USE(GBM) && ENABLE(WPE_PLATFORM)
    void preferredBufferFormatsDidChange() override;
#endif

    bool supportsAsyncScrolling() const override;
    void registerScrollingTree() override;
    void unregisterScrollingTree() override;

    WebCore::GraphicsLayerFactory* graphicsLayerFactory() override;
    void setRootCompositingLayer(WebCore::Frame&, WebCore::GraphicsLayer*) override;
    void triggerRenderingUpdate() override;

    RefPtr<WebCore::DisplayRefreshMonitor> createDisplayRefreshMonitor(WebCore::PlatformDisplayID) override;

    void activityStateDidChange(OptionSet<WebCore::ActivityState>, ActivityStateChangeID, CompletionHandler<void()>&&) override;
    void attachViewOverlayGraphicsLayer(WebCore::FrameIdentifier, WebCore::GraphicsLayer*) override;

    // IPC message handlers.
    void updateGeometry(const WebCore::IntSize&, CompletionHandler<void()>&&) override;
    void displayDidRefresh() override;
    void setDeviceScaleFactor(float) override;
    void forceUpdate() override;
    void didDiscardBackingStore() override;

#if PLATFORM(GTK)
    void adjustTransientZoom(double scale, WebCore::FloatPoint origin) override;
    void commitTransientZoom(double scale, WebCore::FloatPoint origin, CompletionHandler<void()>&&) override;
#endif

    void exitAcceleratedCompositingModeSoon();
    bool exitAcceleratedCompositingModePending() const { return m_exitCompositingTimer.isActive(); }

    void suspendPainting();
    void resumePainting();

    void enterAcceleratedCompositingMode(WebCore::GraphicsLayer*);
    void exitAcceleratedCompositingMode();

    void scheduleDisplay();
    void displayTimerFired();
    void display();
    void display(UpdateInfo&);

    // Whether we're currently processing an UpdateGeometry message.
    bool m_inUpdateGeometry { false };

    // True between sending the 'enter compositing' messages, and the 'exit compositing' message.
    bool m_compositingAccordingToProxyMessages { false };

    // When true, we maintain the layer tree in its current state by not leaving accelerated compositing mode
    // and not scheduling layer flushes.
    bool m_layerTreeStateIsFrozen { false };

    // True when we were asked to exit accelerated compositing mode but couldn't because layer tree
    // state was frozen.
    bool m_wantsToExitAcceleratedCompositingMode { false };

    // Whether painting is suspended. We'll still keep track of the dirty region but we
    // won't paint until painting has resumed again.
    bool m_isPaintingSuspended { false };

    RunLoop::Timer m_exitCompositingTimer;

    // The layer tree host that handles accelerated compositing.
    std::unique_ptr<LayerTreeHost> m_layerTreeHost;

    WebCore::Region m_dirtyRegion;
    WebCore::IntRect m_scrollRect;
    WebCore::IntSize m_scrollOffset;

    // Whether we're waiting for a DidUpdate message. Used for throttling paints so that the
    // web process won't paint more frequent than the UI process can handle.
    bool m_isWaitingForDidUpdate { false };
    bool m_scheduledWhileWaitingForDidUpdate { false };

    bool m_alwaysUseCompositing { false };
    bool m_supportsAsyncScrolling { true };
    bool m_shouldSendEnterAcceleratedCompositingMode { false };

    RunLoop::Timer m_displayTimer;

#if PLATFORM(GTK)
    bool m_transientZoom { false };
    WebCore::FloatPoint m_transientZoomInitialOrigin;
#endif
};

} // namespace WebKit
