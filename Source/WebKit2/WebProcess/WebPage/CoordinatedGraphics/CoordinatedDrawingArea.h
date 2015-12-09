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

#ifndef CoordinatedDrawingArea_h
#define CoordinatedDrawingArea_h

#if USE(COORDINATED_GRAPHICS)

#include "DrawingArea.h"
#include "LayerTreeHost.h"
#include <WebCore/Region.h>
#include <wtf/RunLoop.h>

namespace WebKit {

class CoordinatedDrawingArea : public DrawingArea {
public:
    CoordinatedDrawingArea(WebPage&, const WebPageCreationParameters&);
    virtual ~CoordinatedDrawingArea();

    void layerHostDidFlushLayers();

private:
    // DrawingArea
    virtual void setNeedsDisplay() override;
    virtual void setNeedsDisplayInRect(const WebCore::IntRect&) override;
    virtual void scroll(const WebCore::IntRect& scrollRect, const WebCore::IntSize& scrollDelta) override;
    virtual void pageBackgroundTransparencyChanged() override;
    virtual void setLayerTreeStateIsFrozen(bool) override;
    virtual bool layerTreeStateIsFrozen() const override { return m_layerTreeStateIsFrozen; }
    virtual LayerTreeHost* layerTreeHost() const override { return m_layerTreeHost.get(); }
    virtual void forceRepaint() override;
    virtual bool forceRepaintAsync(uint64_t callbackID) override;

    virtual void setPaintingEnabled(bool) override;
    virtual void updatePreferences(const WebPreferencesStore&) override;
    virtual void mainFrameContentSizeChanged(const WebCore::IntSize&) override;

    virtual WebCore::GraphicsLayerFactory* graphicsLayerFactory() override;
    virtual void setRootCompositingLayer(WebCore::GraphicsLayer*) override;
    virtual void scheduleCompositingLayerFlush() override;
    virtual void scheduleCompositingLayerFlushImmediately() override;

#if USE(COORDINATED_GRAPHICS_MULTIPROCESS)
    virtual void didReceiveCoordinatedLayerTreeHostMessage(IPC::Connection&, IPC::MessageDecoder&) override;
#endif

#if USE(TEXTURE_MAPPER_GL) && PLATFORM(GTK)
    virtual void setNativeSurfaceHandleForCompositing(uint64_t) override { };
#endif

    virtual void viewStateDidChange(WebCore::ViewState::Flags, bool /* wantsDidUpdateViewState */, const Vector<uint64_t>& /* callbackIDs */) override;
    virtual void attachViewOverlayGraphicsLayer(WebCore::Frame*, WebCore::GraphicsLayer*) override;

    // IPC message handlers.
    virtual void updateBackingStoreState(uint64_t backingStoreStateID, bool respondImmediately, float deviceScaleFactor, const WebCore::IntSize&, const WebCore::IntSize& scrollOffset) override;
    virtual void suspendPainting();
    virtual void resumePainting();

    void sendDidUpdateBackingStoreState();

    void enterAcceleratedCompositingMode(WebCore::GraphicsLayer*);
    void exitAcceleratedCompositingModeSoon();
    bool exitAcceleratedCompositingModePending() const { return m_exitCompositingTimer.isActive(); }
    void exitAcceleratedCompositingMode() { }

    uint64_t m_backingStoreStateID;

    // Whether painting is enabled. If painting is disabled, any calls to setNeedsDisplay and scroll are ignored.
    bool m_isPaintingEnabled;

    // Whether we're currently processing an UpdateBackingStoreState message.
    bool m_inUpdateBackingStoreState;

    // When true, we should send an UpdateBackingStoreState message instead of any other messages
    // we normally send to the UI process.
    bool m_shouldSendDidUpdateBackingStoreState;

    // True between sending the 'enter compositing' messages, and the 'exit compositing' message.
    bool m_compositingAccordingToProxyMessages;

    // When true, we maintain the layer tree in its current state by not leaving accelerated compositing mode
    // and not scheduling layer flushes.
    bool m_layerTreeStateIsFrozen;

    // True when we were asked to exit accelerated compositing mode but couldn't because layer tree
    // state was frozen.
    bool m_wantsToExitAcceleratedCompositingMode;

    // Whether painting is suspended. We'll still keep track of the dirty region but we
    // won't paint until painting has resumed again.
    bool m_isPaintingSuspended;

    RunLoop::Timer<CoordinatedDrawingArea> m_exitCompositingTimer;

    // The layer tree host that handles accelerated compositing.
    RefPtr<LayerTreeHost> m_layerTreeHost;
};

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)

#endif // CoordinatedDrawingArea_h
