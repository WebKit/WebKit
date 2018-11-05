/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
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
#include <wtf/RunLoop.h>

namespace WebKit {

class LayerTreeHost;

class AcceleratedDrawingArea : public DrawingArea {
public:
    AcceleratedDrawingArea(WebPage&, const WebPageCreationParameters&);
    virtual ~AcceleratedDrawingArea();

protected:
    // DrawingArea
    void setNeedsDisplay() override;
    void setNeedsDisplayInRect(const WebCore::IntRect&) override;
    void scroll(const WebCore::IntRect& scrollRect, const WebCore::IntSize& scrollDelta) override;
    void pageBackgroundTransparencyChanged() override;
    void setLayerTreeStateIsFrozen(bool) override;
    bool layerTreeStateIsFrozen() const override { return m_layerTreeStateIsFrozen; }
    LayerTreeHost* layerTreeHost() const override { return m_layerTreeHost.get(); }
    void forceRepaint() override;
    bool forceRepaintAsync(CallbackID) override;

    void setPaintingEnabled(bool) override;
    void updatePreferences(const WebPreferencesStore&) override;
    void mainFrameContentSizeChanged(const WebCore::IntSize&) override;

    WebCore::GraphicsLayerFactory* graphicsLayerFactory() override;
    void setRootCompositingLayer(WebCore::GraphicsLayer*) override;
    void scheduleCompositingLayerFlush() override;
    void scheduleCompositingLayerFlushImmediately() override;

#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
    virtual RefPtr<WebCore::DisplayRefreshMonitor> createDisplayRefreshMonitor(WebCore::PlatformDisplayID);
#endif

#if USE(TEXTURE_MAPPER_GL) && PLATFORM(GTK) && PLATFORM(X11) && !USE(REDIRECTED_XCOMPOSITE_WINDOW)
    void setNativeSurfaceHandleForCompositing(uint64_t) override;
    void destroyNativeSurfaceHandleForCompositing(bool&) override;
#endif

    void activityStateDidChange(OptionSet<WebCore::ActivityState::Flag>, ActivityStateChangeID, const Vector<CallbackID>& /* callbackIDs */) override;
    void attachViewOverlayGraphicsLayer(WebCore::Frame*, WebCore::GraphicsLayer*) override;

    void layerHostDidFlushLayers() override;

#if USE(COORDINATED_GRAPHICS)
    void didChangeViewportAttributes(WebCore::ViewportAttributes&&) override;
#endif

#if USE(COORDINATED_GRAPHICS) || USE(TEXTURE_MAPPER)
    void deviceOrPageScaleFactorChanged() override;
#endif

    // IPC message handlers.
    void updateBackingStoreState(uint64_t backingStoreStateID, bool respondImmediately, float deviceScaleFactor, const WebCore::IntSize&, const WebCore::IntSize& scrollOffset) override;

    void exitAcceleratedCompositingModeSoon();
    bool exitAcceleratedCompositingModePending() const { return m_exitCompositingTimer.isActive(); }
    void exitAcceleratedCompositingModeNow();
    void discardPreviousLayerTreeHost();

    virtual void suspendPainting();
    virtual void resumePainting();

    virtual void sendDidUpdateBackingStoreState();
    virtual void didUpdateBackingStoreState() { }

    virtual void enterAcceleratedCompositingMode(WebCore::GraphicsLayer*);
    virtual void exitAcceleratedCompositingMode() { }

    uint64_t m_backingStoreStateID { 0 };

    // Whether painting is enabled. If painting is disabled, any calls to setNeedsDisplay and scroll are ignored.
    bool m_isPaintingEnabled { true };

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

    RunLoop::Timer<AcceleratedDrawingArea> m_exitCompositingTimer;

    // The layer tree host that handles accelerated compositing.
    RefPtr<LayerTreeHost> m_layerTreeHost;

    RefPtr<LayerTreeHost> m_previousLayerTreeHost;
    RunLoop::Timer<AcceleratedDrawingArea> m_discardPreviousLayerTreeHostTimer;
};

} // namespace WebKit
