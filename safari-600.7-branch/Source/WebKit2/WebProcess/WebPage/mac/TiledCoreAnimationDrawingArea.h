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

#if !PLATFORM(IOS)

#include "DrawingArea.h"
#include "LayerTreeContext.h"
#include <WebCore/FloatRect.h>
#include <WebCore/LayerFlushScheduler.h>
#include <WebCore/LayerFlushSchedulerClient.h>
#include <WebCore/TransformationMatrix.h>
#include <wtf/HashMap.h>
#include <wtf/RetainPtr.h>
#include <wtf/RunLoop.h>

OBJC_CLASS CALayer;

namespace WebCore {
class FrameView;
class PlatformCALayer;
class TiledBacking;
}

namespace WebKit {

class LayerHostingContext;

class TiledCoreAnimationDrawingArea : public DrawingArea, WebCore::LayerFlushSchedulerClient {
public:
    TiledCoreAnimationDrawingArea(WebPage&, const WebPageCreationParameters&);
    virtual ~TiledCoreAnimationDrawingArea();

private:
    // DrawingArea
    virtual void setNeedsDisplay() override;
    virtual void setNeedsDisplayInRect(const WebCore::IntRect&) override;
    virtual void scroll(const WebCore::IntRect& scrollRect, const WebCore::IntSize& scrollDelta) override;

    virtual void forceRepaint() override;
    virtual bool forceRepaintAsync(uint64_t callbackID) override;
    virtual void setLayerTreeStateIsFrozen(bool) override;
    virtual bool layerTreeStateIsFrozen() const override;
    virtual void setRootCompositingLayer(WebCore::GraphicsLayer*) override;
    virtual void scheduleCompositingLayerFlush() override;
    virtual void scheduleCompositingLayerFlushImmediately() override;

    virtual void updatePreferences(const WebPreferencesStore&) override;
    virtual void mainFrameContentSizeChanged(const WebCore::IntSize&) override;

    virtual void setExposedRect(const WebCore::FloatRect&) override;
    virtual WebCore::FloatRect exposedRect() const override { return m_scrolledExposedRect; }

    virtual bool supportsAsyncScrolling() override { return true; }

    virtual void dispatchAfterEnsuringUpdatedScrollPosition(std::function<void ()>) override;

    virtual bool shouldUseTiledBackingForFrameView(const WebCore::FrameView*) override;

    virtual void viewStateDidChange(WebCore::ViewState::Flags changed, bool wantsDidUpdateViewState, const Vector<uint64_t>&) override;
    void didUpdateViewStateTimerFired();

    virtual void attachViewOverlayGraphicsLayer(WebCore::Frame*, WebCore::GraphicsLayer*) override;

    // WebCore::LayerFlushSchedulerClient
    virtual bool flushLayers() override;

    // Message handlers.
    virtual void updateGeometry(const WebCore::IntSize& viewSize, const WebCore::IntSize& layerPosition) override;
    virtual void setDeviceScaleFactor(float) override;
    void suspendPainting();
    void resumePainting();
    void setLayerHostingMode(LayerHostingMode) override;
    virtual void setColorSpace(const ColorSpaceData&) override;

    virtual void adjustTransientZoom(double scale, WebCore::FloatPoint origin) override;
    virtual void commitTransientZoom(double scale, WebCore::FloatPoint origin) override;
    void applyTransientZoomToPage(double scale, WebCore::FloatPoint origin);
    WebCore::PlatformCALayer* layerForTransientZoom() const;
    WebCore::PlatformCALayer* shadowLayerForTransientZoom() const;

    void applyTransientZoomToLayers(double scale, WebCore::FloatPoint origin);

    void updateLayerHostingContext();

    void setRootCompositingLayer(CALayer *);
    void updateRootLayers();

    WebCore::TiledBacking* mainFrameTiledBacking() const;
    void updateDebugInfoLayer(bool showLayer);

    void updateIntrinsicContentSizeIfNeeded();
    void updateScrolledExposedRect();

    bool m_layerTreeStateIsFrozen;
    WebCore::LayerFlushScheduler m_layerFlushScheduler;

    std::unique_ptr<LayerHostingContext> m_layerHostingContext;

    RetainPtr<CALayer> m_hostingLayer;
    RetainPtr<CALayer> m_rootLayer;
    RetainPtr<CALayer> m_debugInfoLayer;

    RetainPtr<CALayer> m_pendingRootLayer;

    bool m_isPaintingSuspended;

    WebCore::FloatRect m_exposedRect;
    WebCore::FloatRect m_scrolledExposedRect;

    WebCore::IntSize m_lastSentIntrinsicContentSize;
    bool m_inUpdateGeometry;

    double m_transientZoomScale;
    WebCore::FloatPoint m_transientZoomOrigin;

    WebCore::TransformationMatrix m_transform;

    RunLoop::Timer<TiledCoreAnimationDrawingArea> m_sendDidUpdateViewStateTimer;
    Vector<uint64_t> m_nextViewStateChangeCallbackIDs;
    bool m_wantsDidUpdateViewState;

    WebCore::GraphicsLayer* m_viewOverlayRootLayer;
};

DRAWING_AREA_TYPE_CASTS(TiledCoreAnimationDrawingArea, type() == DrawingAreaTypeTiledCoreAnimation);

} // namespace WebKit

#endif // !PLATFORM(IOS)

#endif // TiledCoreAnimationDrawingArea_h
