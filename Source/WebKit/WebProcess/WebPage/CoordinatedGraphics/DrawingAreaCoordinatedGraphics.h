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

class ShareableBitmap;
class UpdateInfo;

class DrawingAreaCoordinatedGraphics final : public DrawingArea {
public:
    DrawingAreaCoordinatedGraphics(WebPage&, const WebPageCreationParameters&);
    virtual ~DrawingAreaCoordinatedGraphics();

private:
    // DrawingArea
    void setNeedsDisplay() override;
    void setNeedsDisplayInRect(const WebCore::IntRect&) override;
    void scroll(const WebCore::IntRect& scrollRect, const WebCore::IntSize& scrollDelta) override;
    void forceRepaint() override;
    bool forceRepaintAsync(CallbackID) override;

    void setLayerTreeStateIsFrozen(bool) override;
    bool layerTreeStateIsFrozen() const override { return m_layerTreeStateIsFrozen; }

    void updatePreferences(const WebPreferencesStore&) override;
    void enablePainting() override;
    void mainFrameContentSizeChanged(const WebCore::IntSize&) override;

#if USE(COORDINATED_GRAPHICS) || USE(TEXTURE_MAPPER)
    void deviceOrPageScaleFactorChanged() override;
    void didChangeViewportAttributes(WebCore::ViewportAttributes&&) override;
#endif

    bool supportsAsyncScrolling() const override;

    WebCore::GraphicsLayerFactory* graphicsLayerFactory() override;
    void setRootCompositingLayer(WebCore::GraphicsLayer*) override;
    void scheduleInitialDeferredPaint() override { };
    void scheduleCompositingLayerFlush() override;
    void scheduleCompositingLayerFlushImmediately() override { scheduleCompositingLayerFlush(); };
    void layerHostDidFlushLayers() override;

#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
    RefPtr<WebCore::DisplayRefreshMonitor> createDisplayRefreshMonitor(WebCore::PlatformDisplayID) override;
#endif

    void activityStateDidChange(OptionSet<WebCore::ActivityState::Flag>, ActivityStateChangeID, const Vector<CallbackID>& /* callbackIDs */) override;
    void attachViewOverlayGraphicsLayer(WebCore::GraphicsLayer*) override;

    // IPC message handlers.
    void updateBackingStoreState(uint64_t backingStoreStateID, bool respondImmediately, float deviceScaleFactor, const WebCore::IntSize&, const WebCore::IntSize& scrollOffset) override;
    void didUpdate() override;

    void sendDidUpdateBackingStoreState();

    void exitAcceleratedCompositingModeSoon();
    bool exitAcceleratedCompositingModePending() const { return m_exitCompositingTimer.isActive(); }
    void discardPreviousLayerTreeHost();

    void suspendPainting();
    void resumePainting();

    void enterAcceleratedCompositingMode(WebCore::GraphicsLayer*);
    void exitAcceleratedCompositingMode();

    void scheduleDisplay();
    void displayTimerFired();
    void display();
    void display(UpdateInfo&);

    uint64_t m_backingStoreStateID { 0 };

    // Whether painting is enabled. If painting is disabled, any calls to setNeedsDisplay and scroll are ignored.
    bool m_isPaintingEnabled { false };

    // Whether we're currently processing an UpdateBackingStoreState message.
    bool m_inUpdateBackingStoreState { false };

    // When true, we should send an UpdateBackingStoreState message instead of any other messages
    // we normally send to the UI process.
    bool m_shouldSendDidUpdateBackingStoreState { false };

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

    RunLoop::Timer<DrawingAreaCoordinatedGraphics> m_exitCompositingTimer;

    // The layer tree host that handles accelerated compositing.
    std::unique_ptr<LayerTreeHost> m_layerTreeHost;

    std::unique_ptr<LayerTreeHost> m_previousLayerTreeHost;
    RunLoop::Timer<DrawingAreaCoordinatedGraphics> m_discardPreviousLayerTreeHostTimer;

    WebCore::Region m_dirtyRegion;
    WebCore::IntRect m_scrollRect;
    WebCore::IntSize m_scrollOffset;

    // Whether we're waiting for a DidUpdate message. Used for throttling paints so that the 
    // web process won't paint more frequent than the UI process can handle.
    bool m_isWaitingForDidUpdate { false };

    bool m_alwaysUseCompositing { false };
    bool m_supportsAsyncScrolling { true };
    bool m_forceRepaintAfterBackingStoreStateUpdate { false };

    RunLoop::Timer<DrawingAreaCoordinatedGraphics> m_displayTimer;
};

} // namespace WebKit
