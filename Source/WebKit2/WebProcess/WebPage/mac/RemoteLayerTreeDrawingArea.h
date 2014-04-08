/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef RemoteLayerTreeDrawingArea_h
#define RemoteLayerTreeDrawingArea_h

#include "DrawingArea.h"
#include "GraphicsLayerCARemote.h"
#include <WebCore/Timer.h>
#include <wtf/HashMap.h>

namespace WebCore {
class PlatformCALayer;
}

namespace WebKit {

class RemoteLayerTreeContext;

class RemoteLayerTreeDrawingArea : public DrawingArea {
public:
    RemoteLayerTreeDrawingArea(WebPage*, const WebPageCreationParameters&);
    virtual ~RemoteLayerTreeDrawingArea();

private:
    // DrawingArea
    virtual void setNeedsDisplay() override;
    virtual void setNeedsDisplayInRect(const WebCore::IntRect&) override;
    virtual void scroll(const WebCore::IntRect& scrollRect, const WebCore::IntSize& scrollDelta) override;
    virtual void updateGeometry(const WebCore::IntSize& viewSize, const WebCore::IntSize& layerPosition) override;

    virtual WebCore::GraphicsLayerFactory* graphicsLayerFactory() override;
    virtual void setRootCompositingLayer(WebCore::GraphicsLayer*) override;
    virtual void scheduleCompositingLayerFlush() override;

    virtual bool shouldUseTiledBackingForFrameView(const WebCore::FrameView*) override;

    virtual void updatePreferences(const WebPreferencesStore&) override;

    virtual bool supportsAsyncScrolling() override { return true; }

    virtual void setLayerTreeStateIsFrozen(bool) override;

    virtual void forceRepaint() override;
    virtual bool forceRepaintAsync(uint64_t) override { return false; }

    virtual void setExposedRect(const WebCore::FloatRect&) override;
    virtual WebCore::FloatRect exposedRect() const override { return m_scrolledExposedRect; }

    virtual void acceleratedAnimationDidStart(uint64_t layerID, double startTime) override;

#if PLATFORM(IOS)
    virtual void setExposedContentRect(const WebCore::FloatRect&) override;
#endif

    virtual void didUpdate() override;

#if PLATFORM(IOS)
    virtual void setDeviceScaleFactor(float) override;
#endif

    void updateScrolledExposedRect();

    void layerFlushTimerFired(WebCore::Timer<RemoteLayerTreeDrawingArea>*);
    void flushLayers();

    WebCore::TiledBacking* mainFrameTiledBacking() const;

    std::unique_ptr<RemoteLayerTreeContext> m_remoteLayerTreeContext;
    std::unique_ptr<WebCore::GraphicsLayer> m_rootLayer;

    WebCore::IntSize m_viewSize;

    WebCore::FloatRect m_exposedRect;
    WebCore::FloatRect m_scrolledExposedRect;

    WebCore::Timer<RemoteLayerTreeDrawingArea> m_layerFlushTimer;
    bool m_isFlushingSuspended;
    bool m_hasDeferredFlush;

    bool m_waitingForBackingStoreSwap;
    bool m_hadFlushDeferredWhileWaitingForBackingStoreSwap;
};

DRAWING_AREA_TYPE_CASTS(RemoteLayerTreeDrawingArea, type() == DrawingAreaTypeRemoteLayerTree);

} // namespace WebKit

#endif // RemoteLayerTreeDrawingArea_h
