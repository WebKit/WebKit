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

#if !PLATFORM(IOS_FAMILY)

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

class TiledCoreAnimationDrawingArea : public DrawingArea {
public:
    TiledCoreAnimationDrawingArea(WebPage&, const WebPageCreationParameters&);
    virtual ~TiledCoreAnimationDrawingArea();

private:
    // DrawingArea
    void setNeedsDisplay() override;
    void setNeedsDisplayInRect(const WebCore::IntRect&) override;
    void scroll(const WebCore::IntRect& scrollRect, const WebCore::IntSize& scrollDelta) override;

    void forceRepaint() override;
    bool forceRepaintAsync(CallbackID) override;
    void setLayerTreeStateIsFrozen(bool) override;
    bool layerTreeStateIsFrozen() const override;
    void setRootCompositingLayer(WebCore::GraphicsLayer*) override;
    void scheduleInitialDeferredPaint() override;
    void scheduleCompositingLayerFlush() override;
    void scheduleCompositingLayerFlushImmediately() override;

    void updatePreferences(const WebPreferencesStore&) override;
    void mainFrameContentSizeChanged(const WebCore::IntSize&) override;

    void setViewExposedRect(std::optional<WebCore::FloatRect>) override;
    std::optional<WebCore::FloatRect> viewExposedRect() const override { return m_scrolledViewExposedRect; }

    bool supportsAsyncScrolling() override { return true; }

    void dispatchAfterEnsuringUpdatedScrollPosition(WTF::Function<void ()>&&) override;

    bool shouldUseTiledBackingForFrameView(const WebCore::FrameView&) override;

    void activityStateDidChange(OptionSet<WebCore::ActivityState::Flag> changed, ActivityStateChangeID, const Vector<CallbackID>&) override;
    void didUpdateActivityStateTimerFired();

    void attachViewOverlayGraphicsLayer(WebCore::Frame*, WebCore::GraphicsLayer*) override;

    bool dispatchDidReachLayoutMilestone(OptionSet<WebCore::LayoutMilestone>) override;

    void flushLayers();

    // Message handlers.
    void updateGeometry(const WebCore::IntSize& viewSize, bool flushSynchronously, const WTF::MachSendRight& fencePort) override;
    void setDeviceScaleFactor(float) override;
    void suspendPainting();
    void resumePainting();
    void setLayerHostingMode(LayerHostingMode) override;
    void setColorSpace(const ColorSpaceData&) override;
    void addFence(const WTF::MachSendRight&) override;

    void addTransactionCallbackID(CallbackID) override;
    void setShouldScaleViewToFitDocument(bool) override;

    void attach() override;

    void adjustTransientZoom(double scale, WebCore::FloatPoint origin) override;
    void commitTransientZoom(double scale, WebCore::FloatPoint origin) override;
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
    void scaleViewToFitDocumentIfNeeded();

    void sendPendingNewlyReachedLayoutMilestones();

    void layerFlushRunLoopCallback();
    void invalidateLayerFlushRunLoopObserver();
    void scheduleLayerFlushRunLoopObserver();

    bool adjustLayerFlushThrottling(WebCore::LayerFlushThrottleState::Flags) override;
    bool layerFlushThrottlingIsActive() const override;

    void startLayerFlushThrottlingTimer();
    void layerFlushThrottlingTimerFired();

    bool m_layerTreeStateIsFrozen;

    std::unique_ptr<LayerHostingContext> m_layerHostingContext;

    RetainPtr<CALayer> m_hostingLayer;
    RetainPtr<CALayer> m_rootLayer;
    RetainPtr<CALayer> m_debugInfoLayer;

    RetainPtr<CALayer> m_pendingRootLayer;

    bool m_isPaintingSuspended;

    std::optional<WebCore::FloatRect> m_viewExposedRect;
    std::optional<WebCore::FloatRect> m_scrolledViewExposedRect;

    WebCore::IntSize m_lastSentIntrinsicContentSize;
    bool m_inUpdateGeometry;

    double m_transientZoomScale;
    WebCore::FloatPoint m_transientZoomOrigin;

    WebCore::TransformationMatrix m_transform;

    RunLoop::Timer<TiledCoreAnimationDrawingArea> m_sendDidUpdateActivityStateTimer;
    Vector<CallbackID> m_nextActivityStateChangeCallbackIDs;
    ActivityStateChangeID m_activityStateChangeID { ActivityStateChangeAsynchronous };

    WebCore::GraphicsLayer* m_viewOverlayRootLayer;

    bool m_shouldScaleViewToFitDocument { false };
    bool m_isScalingViewToFitDocument { false };
    WebCore::IntSize m_lastViewSizeForScaleToFit;
    WebCore::IntSize m_lastDocumentSizeForScaleToFit;

    OptionSet<WebCore::LayoutMilestone> m_pendingNewlyReachedLayoutMilestones;
    Vector<CallbackID> m_pendingCallbackIDs;

    std::unique_ptr<WebCore::RunLoopObserver> m_layerFlushRunLoopObserver;

    bool m_isThrottlingLayerFlushes { false };
    bool m_isLayerFlushThrottlingTemporarilyDisabledForInteraction { false };

    WebCore::Timer m_layerFlushThrottlingTimer;
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_DRAWING_AREA(TiledCoreAnimationDrawingArea, DrawingAreaTypeTiledCoreAnimation)

#endif // !PLATFORM(IOS_FAMILY)

