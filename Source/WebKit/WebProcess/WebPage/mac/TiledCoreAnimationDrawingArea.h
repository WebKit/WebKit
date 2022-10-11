/*
 * Copyright (C) 2011-2018 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC)

#include "CallbackID.h"
#include "DrawingArea.h"
#include "LayerTreeContext.h"
#include <WebCore/FloatRect.h>
#include <WebCore/TransformationMatrix.h>
#include <wtf/HashMap.h>
#include <wtf/RetainPtr.h>
#include <wtf/RunLoop.h>

OBJC_CLASS CALayer;

namespace WebCore {
class FrameView;
class PlatformCALayer;
class RunLoopObserver;
class TiledBacking;
}

namespace WebKit {

class LayerHostingContext;

class TiledCoreAnimationDrawingArea final : public DrawingArea {
public:
    TiledCoreAnimationDrawingArea(WebPage&, const WebPageCreationParameters&);
    virtual ~TiledCoreAnimationDrawingArea();

private:
    // DrawingArea
    void setNeedsDisplay() override;
    void setNeedsDisplayInRect(const WebCore::IntRect&) override;
    void scroll(const WebCore::IntRect& scrollRect, const WebCore::IntSize& scrollDelta) override { }

    void forceRepaint() override;
    void forceRepaintAsync(WebPage&, CompletionHandler<void()>&&) override;
    void setLayerTreeStateIsFrozen(bool) override;
    bool layerTreeStateIsFrozen() const override;
    void setRootCompositingLayer(WebCore::GraphicsLayer*) override;
    void triggerRenderingUpdate() override;

    void updatePreferences(const WebPreferencesStore&) override;
    void mainFrameContentSizeChanged(const WebCore::IntSize&) override;

    void setViewExposedRect(std::optional<WebCore::FloatRect>) override;
    std::optional<WebCore::FloatRect> viewExposedRect() const override { return m_viewExposedRect; }

    WebCore::FloatRect exposedContentRect() const override;
    void setExposedContentRect(const WebCore::FloatRect&) override;

    bool supportsAsyncScrolling() const override { return true; }

    void dispatchAfterEnsuringUpdatedScrollPosition(WTF::Function<void ()>&&) override;

    bool shouldUseTiledBackingForFrameView(const WebCore::FrameView&) const override;

    void activityStateDidChange(OptionSet<WebCore::ActivityState::Flag> changed, ActivityStateChangeID, CompletionHandler<void()>&&) override;

    void attachViewOverlayGraphicsLayer(WebCore::GraphicsLayer*) override;

    bool addMilestonesToDispatch(OptionSet<WebCore::LayoutMilestone> paintMilestones) override;

    void addCommitHandlers();
    enum class UpdateRenderingType { Normal, TransientZoom };
    void updateRendering(UpdateRenderingType = UpdateRenderingType::Normal);

    // Message handlers.
    void updateGeometry(const WebCore::IntSize& viewSize, bool flushSynchronously, const WTF::MachSendRight& fencePort) override;
    void setDeviceScaleFactor(float) override;
    void suspendPainting();
    void resumePainting();
    void setLayerHostingMode(LayerHostingMode) override;
    void setColorSpace(std::optional<WebCore::DestinationColorSpace>) override;
    void addFence(const WTF::MachSendRight&) override;

    void addTransactionCallbackID(CallbackID) override;
    void setShouldScaleViewToFitDocument(bool) override;

    void sendEnterAcceleratedCompositingModeIfNeeded() override;
    void sendDidFirstLayerFlushIfNeeded();
    void handleActivityStateChangeCallbacksIfNeeded();
    void handleActivityStateChangeCallbacks();

    void adjustTransientZoom(double scale, WebCore::FloatPoint origin) override;
    void commitTransientZoom(double scale, WebCore::FloatPoint origin) override;
    void applyTransientZoomToPage(double scale, WebCore::FloatPoint origin);
    WebCore::PlatformCALayer* layerForTransientZoom() const;
    WebCore::PlatformCALayer* shadowLayerForTransientZoom() const;

    void applyTransientZoomToLayers(double scale, WebCore::FloatPoint origin);

    RefPtr<WebCore::DisplayRefreshMonitor> createDisplayRefreshMonitor(WebCore::PlatformDisplayID) final;

    void updateLayerHostingContext();

    void setRootCompositingLayer(CALayer *);
    void updateRootLayers();

    WebCore::TiledBacking* mainFrameTiledBacking() const;
    void updateDebugInfoLayer(bool showLayer);

    void scaleViewToFitDocumentIfNeeded();

    void sendPendingNewlyReachedPaintingMilestones();

    void updateRenderingRunLoopCallback();
    void invalidateRenderingUpdateRunLoopObserver();
    void scheduleRenderingUpdateRunLoopObserver();

    void startRenderThrottlingTimer();
    void renderThrottlingTimerFired();

    std::unique_ptr<LayerHostingContext> m_layerHostingContext;

    RetainPtr<CALayer> m_hostingLayer;
    RetainPtr<CALayer> m_rootLayer;
    RetainPtr<CALayer> m_debugInfoLayer;
    RetainPtr<CALayer> m_pendingRootLayer;

    std::optional<WebCore::FloatRect> m_viewExposedRect;

    WebCore::IntSize m_lastViewSizeForScaleToFit;
    WebCore::IntSize m_lastDocumentSizeForScaleToFit;

    double m_transientZoomScale { 1 };
    WebCore::FloatPoint m_transientZoomOrigin;

    Vector<CompletionHandler<void()>> m_nextActivityStateChangeCallbacks;
    ActivityStateChangeID m_activityStateChangeID { ActivityStateChangeAsynchronous };

    RefPtr<WebCore::GraphicsLayer> m_viewOverlayRootLayer;

    OptionSet<WebCore::LayoutMilestone> m_pendingNewlyReachedPaintingMilestones;
    Vector<CallbackID> m_pendingCallbackIDs;

    std::unique_ptr<WebCore::RunLoopObserver> m_renderUpdateRunLoopObserver;

    bool m_isPaintingSuspended { false };
    bool m_inUpdateGeometry { false };
    bool m_layerTreeStateIsFrozen { false };
    bool m_shouldScaleViewToFitDocument { false };
    bool m_isScalingViewToFitDocument { false };
    bool m_needsSendEnterAcceleratedCompositingMode { true };
    bool m_needsSendDidFirstLayerFlush { true };
    bool m_shouldHandleActivityStateChangeCallbacks { false };
};

inline bool TiledCoreAnimationDrawingArea::addMilestonesToDispatch(OptionSet<WebCore::LayoutMilestone> paintMilestones)
{
    m_pendingNewlyReachedPaintingMilestones.add(paintMilestones);
    return true;
}

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_DRAWING_AREA(TiledCoreAnimationDrawingArea, DrawingAreaType::TiledCoreAnimation)

#endif // PLATFORM(MAC)
