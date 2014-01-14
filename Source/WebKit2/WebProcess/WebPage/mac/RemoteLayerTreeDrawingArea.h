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
#include <WebCore/GraphicsLayerClient.h>
#include <WebCore/Timer.h>
#include <wtf/HashMap.h>

namespace WebCore {
class PlatformCALayer;
}

namespace WebKit {

class RemoteLayerTreeContext;

class RemoteLayerTreeDrawingArea : public DrawingArea, public WebCore::GraphicsLayerClient {
public:
    RemoteLayerTreeDrawingArea(WebPage*, const WebPageCreationParameters&);
    virtual ~RemoteLayerTreeDrawingArea();

private:
    // DrawingArea
    virtual void setNeedsDisplay() OVERRIDE;
    virtual void setNeedsDisplayInRect(const WebCore::IntRect&) OVERRIDE;
    virtual void scroll(const WebCore::IntRect& scrollRect, const WebCore::IntSize& scrollDelta) OVERRIDE;
    virtual void updateGeometry(const WebCore::IntSize& viewSize, const WebCore::IntSize& layerPosition) OVERRIDE;

    virtual WebCore::GraphicsLayerFactory* graphicsLayerFactory() OVERRIDE;
    virtual void setRootCompositingLayer(WebCore::GraphicsLayer*) OVERRIDE;
    virtual void scheduleCompositingLayerFlush() OVERRIDE;

    virtual bool shouldUseTiledBackingForFrameView(const WebCore::FrameView*) OVERRIDE;

    virtual void updatePreferences(const WebPreferencesStore&) OVERRIDE;

    virtual void didInstallPageOverlay(PageOverlay*) OVERRIDE;
    virtual void didUninstallPageOverlay(PageOverlay*) OVERRIDE;
    virtual void setPageOverlayNeedsDisplay(PageOverlay*, const WebCore::IntRect&) OVERRIDE;
    virtual void setPageOverlayOpacity(PageOverlay*, float) OVERRIDE;
    virtual bool supportsAsyncScrolling() OVERRIDE { return true; }

    virtual void setLayerTreeStateIsFrozen(bool) OVERRIDE;

    virtual void forceRepaint() OVERRIDE;
    virtual bool forceRepaintAsync(uint64_t) OVERRIDE { return false; }

    virtual void setExposedRect(const WebCore::FloatRect&) OVERRIDE;
    virtual WebCore::FloatRect exposedRect() const OVERRIDE { return m_scrolledExposedRect; }

    // WebCore::GraphicsLayerClient
    virtual void notifyAnimationStarted(const WebCore::GraphicsLayer*, double time) OVERRIDE { }
    virtual void notifyFlushRequired(const WebCore::GraphicsLayer*) OVERRIDE { }
    virtual void paintContents(const WebCore::GraphicsLayer*, WebCore::GraphicsContext&, WebCore::GraphicsLayerPaintingPhase, const WebCore::IntRect& clipRect) OVERRIDE;
    virtual float deviceScaleFactor() const OVERRIDE;
    virtual void didCommitChangesForLayer(const WebCore::GraphicsLayer*) const OVERRIDE { }
#if PLATFORM(IOS)
    virtual void setDeviceScaleFactor(float) OVERRIDE;
    virtual bool allowCompositingLayerVisualDegradation() const OVERRIDE { return false; }
#endif

    void updateScrolledExposedRect();

    void layerFlushTimerFired(WebCore::Timer<RemoteLayerTreeDrawingArea>*);
    void flushLayers();

    WebCore::TiledBacking* mainFrameTiledBacking() const;

    std::unique_ptr<RemoteLayerTreeContext> m_remoteLayerTreeContext;
    RefPtr<WebCore::PlatformCALayer> m_rootLayer;

    HashMap<PageOverlay*, std::unique_ptr<GraphicsLayerCARemote>> m_pageOverlayLayers;

    WebCore::IntSize m_viewSize;

    WebCore::FloatRect m_exposedRect;
    WebCore::FloatRect m_scrolledExposedRect;

    WebCore::Timer<RemoteLayerTreeDrawingArea> m_layerFlushTimer;
    bool m_isFlushingSuspended;
    bool m_hasDeferredFlush;
};

DRAWING_AREA_TYPE_CASTS(RemoteLayerTreeDrawingArea, type() == DrawingAreaTypeRemoteLayerTree);

} // namespace WebKit

#endif // RemoteLayerTreeDrawingArea_h
