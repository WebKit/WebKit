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

#ifndef TiledCoreAnimationDrawingArea_h
#define TiledCoreAnimationDrawingArea_h

#if ENABLE(THREADED_SCROLLING)

#include "DrawingArea.h"
#include "LayerTreeContext.h"
#include <WebCore/GraphicsLayerClient.h>
#include <WebCore/LayerFlushScheduler.h>
#include <WebCore/LayerFlushSchedulerClient.h>
#include <wtf/RetainPtr.h>

OBJC_CLASS CALayer;
OBJC_CLASS WKContentLayer;

namespace WebCore {
class TiledBacking;
}

namespace WebKit {

class LayerHostingContext;

class TiledCoreAnimationDrawingArea : public DrawingArea, WebCore::GraphicsLayerClient, WebCore::LayerFlushSchedulerClient {
public:
    static PassOwnPtr<TiledCoreAnimationDrawingArea> create(WebPage*, const WebPageCreationParameters&);
    virtual ~TiledCoreAnimationDrawingArea();

private:
    TiledCoreAnimationDrawingArea(WebPage*, const WebPageCreationParameters&);

    // DrawingArea
    virtual void setNeedsDisplay(const WebCore::IntRect&) OVERRIDE;
    virtual void scroll(const WebCore::IntRect& scrollRect, const WebCore::IntSize& scrollOffset) OVERRIDE;

    virtual void forceRepaint() OVERRIDE;
    virtual bool forceRepaintAsync(uint64_t callbackID) OVERRIDE;
    virtual void setLayerTreeStateIsFrozen(bool) OVERRIDE;
    virtual bool layerTreeStateIsFrozen() const OVERRIDE;
    virtual void setRootCompositingLayer(WebCore::GraphicsLayer*) OVERRIDE;
    virtual void scheduleCompositingLayerFlush() OVERRIDE;

    virtual void didInstallPageOverlay() OVERRIDE;
    virtual void didUninstallPageOverlay() OVERRIDE;
    virtual void setPageOverlayNeedsDisplay(const WebCore::IntRect&) OVERRIDE;
    virtual void updatePreferences(const WebPreferencesStore&) OVERRIDE;
    virtual void mainFrameContentSizeChanged(const WebCore::IntSize&) OVERRIDE;

    virtual void setExposedRect(const WebCore::IntRect&) OVERRIDE;
    virtual void mainFrameScrollabilityChanged(bool) OVERRIDE;

    virtual void dispatchAfterEnsuringUpdatedScrollPosition(const Function<void ()>&) OVERRIDE;

    // WebCore::GraphicsLayerClient
    virtual void notifyAnimationStarted(const WebCore::GraphicsLayer*, double time) OVERRIDE;
    virtual void notifyFlushRequired(const WebCore::GraphicsLayer*) OVERRIDE;
    virtual void paintContents(const WebCore::GraphicsLayer*, WebCore::GraphicsContext&, WebCore::GraphicsLayerPaintingPhase, const WebCore::IntRect& clipRect) OVERRIDE;
    virtual float deviceScaleFactor() const OVERRIDE;

    // WebCore::LayerFlushSchedulerClient
    virtual bool flushLayers() OVERRIDE;

    // Message handlers.
    virtual void suspendPainting() OVERRIDE;
    virtual void resumePainting() OVERRIDE;
    virtual void updateGeometry(const WebCore::IntSize& viewSize) OVERRIDE;
    virtual void setDeviceScaleFactor(float) OVERRIDE;
    virtual void setLayerHostingMode(uint32_t) OVERRIDE;
    virtual void setColorSpace(const ColorSpaceData&) OVERRIDE;

    void updateLayerHostingContext();

    void setRootCompositingLayer(CALayer *);

    void createPageOverlayLayer();
    void destroyPageOverlayLayer();
    WebCore::TiledBacking* mainFrameTiledBacking() const;
    void updateDebugInfoLayer(bool showLayer);

    bool m_layerTreeStateIsFrozen;
    WebCore::LayerFlushScheduler m_layerFlushScheduler;

    OwnPtr<LayerHostingContext> m_layerHostingContext;

    RetainPtr<CALayer> m_rootLayer;
    RetainPtr<CALayer> m_pendingRootCompositingLayer;

    RetainPtr<CALayer> m_debugInfoLayer;

    OwnPtr<WebCore::GraphicsLayer> m_pageOverlayLayer;

    bool m_isPaintingSuspended;

    WebCore::IntRect m_exposedRect;

    WebCore::IntSize m_lastSentIntrinsicContentSize;
    bool m_inUpdateGeometry;
};

} // namespace WebKit

#endif // ENABLE(THREADED_SCROLLING)

#endif // TiledCoreAnimationDrawingArea_h
