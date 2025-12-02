/*
 * Copyright (C) 2011-2025 Apple Inc. All rights reserved.
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

#if USE(COORDINATED_GRAPHICS)
#include "CallbackID.h"
#include "LayerTreeContext.h"
#include "ThreadedCompositor.h"
#include <WebCore/CoordinatedImageBackingStore.h>
#include <WebCore/CoordinatedPlatformLayer.h>
#include <WebCore/FloatPoint.h>
#include <WebCore/GraphicsLayerClient.h>
#include <WebCore/GraphicsLayerFactory.h>
#include <WebCore/PlatformScreen.h>
#include <WebCore/RunLoopObserver.h>
#include <wtf/CheckedRef.h>
#include <wtf/Forward.h>
#include <wtf/Lock.h>
#include <wtf/OptionSet.h>
#include <wtf/RunLoop.h>
#include <wtf/TZoneMalloc.h>

#if ENABLE(DAMAGE_TRACKING)
#include <WebCore/Region.h>
#endif

namespace WebCore {
class Damage;
class IntRect;
class IntSize;
class GraphicsLayer;
class GraphicsLayerFactory;
class NativeImage;
class SkiaPaintingEngine;
#if USE(CAIRO)
namespace Cairo {
class PaintingEngine;
}
#endif
}

namespace WebKit {
class CoordinatedSceneState;
class WebPage;
struct RenderProcessInfo;

class LayerTreeHost final : public CanMakeCheckedPtr<LayerTreeHost>, public WebCore::GraphicsLayerFactory, public WebCore::CoordinatedPlatformLayer::Client
{
    WTF_MAKE_TZONE_ALLOCATED(LayerTreeHost);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(LayerTreeHost);
public:
    explicit LayerTreeHost(WebPage&);
    ~LayerTreeHost();

    WebPage& webPage() const { return m_webPage; }
    CoordinatedSceneState& sceneState() const { return m_sceneState.get(); }

    const LayerTreeContext& layerTreeContext() const { return m_layerTreeContext; }
    void setLayerTreeStateIsFrozen(bool);

    void setRootCompositingLayer(WebCore::GraphicsLayer*);
    void setViewOverlayRootLayer(WebCore::GraphicsLayer*);

    void scheduleRenderingUpdate();
    void updateRenderingWithForcedRepaint();
    void updateRenderingWithForcedRepaintAsync(CompletionHandler<void()>&&);
    void sizeDidChange();

    void pauseRendering();
    void resumeRendering();

    WebCore::GraphicsLayerFactory* graphicsLayerFactory();

    void backgroundColorDidChange();

    void willRenderFrame();
    void didRenderFrame();

#if PLATFORM(GTK)
    void adjustTransientZoom(double, WebCore::FloatPoint);
    void commitTransientZoom(double, WebCore::FloatPoint);
#endif

    void ensureDrawing();

#if ENABLE(DAMAGE_TRACKING)
    void notifyFrameDamageForTesting(WebCore::Region&&);
    void resetDamageHistoryForTesting();
    void foreachRegionInDamageHistoryForTesting(Function<void(const WebCore::Region&)>&&);
#endif

#if PLATFORM(WPE) && USE(GBM) && ENABLE(WPE_PLATFORM)
    void preferredBufferFormatsDidChange();
#endif

    void fillGLInformation(RenderProcessInfo&&, CompletionHandler<void(RenderProcessInfo&&)>&&);
private:
    void updateRootLayer();
    WebCore::FloatRect visibleContentsRect() const;

    void scheduleRenderingUpdateRunLoopObserver();
    void invalidateRenderingUpdateRunLoopObserver();
    void renderingUpdateRunLoopObserverFired();
    void updateRendering();
    void requestCompositionForRenderingUpdate();

    // CoordinatedPlatformLayer::Client
#if USE(CAIRO)
    WebCore::Cairo::PaintingEngine& paintingEngine() override;
#elif USE(SKIA)
    WebCore::SkiaPaintingEngine& paintingEngine() const override { return *m_skiaPaintingEngine.get(); }
#endif
    Ref<WebCore::CoordinatedImageBackingStore> imageBackingStore(Ref<WebCore::NativeImage>&&) override;

    void attachLayer(WebCore::CoordinatedPlatformLayer&) override;
    void detachLayer(WebCore::CoordinatedPlatformLayer&) override;
    void notifyCompositionRequired() override;
    bool isCompositionRequiredOrOngoing() const override;
    void requestComposition(WebCore::CompositionReason) override;
    RunLoop* compositingRunLoop() const override;
    int maxTextureSize() const override;
    void willPaintTile() override;
    void didPaintTile() override;

    // GraphicsLayerFactory
    Ref<WebCore::GraphicsLayer> createGraphicsLayer(WebCore::GraphicsLayer::Type, WebCore::GraphicsLayerClient&) override;

#if PLATFORM(GTK)
    WebCore::FloatPoint constrainTransientZoomOrigin(double, WebCore::FloatPoint) const;
    WebCore::CoordinatedPlatformLayer* layerForTransientZoom() const;
    void applyTransientZoomToLayers(double, WebCore::FloatPoint);
#endif

    WebPage& m_webPage;
    LayerTreeContext m_layerTreeContext;
    const Ref<CoordinatedSceneState> m_sceneState;
    WebCore::GraphicsLayer* m_rootCompositingLayer { nullptr };
    WebCore::GraphicsLayer* m_overlayCompositingLayer { nullptr };
    bool m_layerTreeStateIsFrozen { false };
    bool m_pendingResize { false };
    bool m_pendingForceRepaint { false };
    bool m_isUpdatingRendering { false };
    bool m_waitUntilPaintingComplete { false };
    bool m_isSuspended { false };
    bool m_isWaitingForRenderer { false };
    bool m_scheduledWhileWaitingForRenderer { false };
    bool m_forceFrameSync { false };
    bool m_compositionRequired { false };
#if ENABLE(SCROLLING_THREAD)
    bool m_compositionRequiredInScrollingThread { false };
#endif
    RefPtr<ThreadedCompositor> m_compositor;
    struct {
        CompletionHandler<void()> callback;
        std::optional<uint32_t> compositionRequestID;
    } m_forceRepaintAsync;
    std::unique_ptr<WebCore::RunLoopObserver> m_renderingUpdateRunLoopObserver;
#if USE(CAIRO)
    std::unique_ptr<WebCore::Cairo::PaintingEngine> m_paintingEngine;
#elif USE(SKIA)
    std::unique_ptr<WebCore::SkiaPaintingEngine> m_skiaPaintingEngine;
#endif
    HashMap<uint64_t, Ref<WebCore::CoordinatedImageBackingStore>> m_imageBackingStores;

#if PLATFORM(GTK)
    bool m_transientZoom { false };
    double m_transientZoomScale { 1 };
    WebCore::FloatPoint m_transientZoomOrigin;
#endif

#if ENABLE(DAMAGE_TRACKING)
    Lock m_frameDamageHistoryForTestingLock;
    Vector<WebCore::Region> m_frameDamageHistoryForTesting WTF_GUARDED_BY_LOCK(m_frameDamageHistoryForTestingLock);
    std::shared_ptr<WebCore::Damage> m_damageInGlobalCoordinateSpace;
#endif
};

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)
