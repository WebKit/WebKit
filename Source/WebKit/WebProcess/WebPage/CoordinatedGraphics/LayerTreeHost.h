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
#include "FrameRenderer.h"
#include "ThreadedCompositor.h"
#include <WebCore/CoordinatedImageBackingStore.h>
#include <WebCore/CoordinatedPlatformLayer.h>
#include <WebCore/FloatPoint.h>
#include <WebCore/GraphicsLayerClient.h>
#include <WebCore/GraphicsLayerFactory.h>
#include <WebCore/PlatformScreen.h>
#include <wtf/CheckedRef.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Forward.h>
#include <wtf/Lock.h>
#include <wtf/OptionSet.h>
#include <wtf/RunLoop.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakRef.h>

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
}

namespace WebKit {
class CoordinatedSceneState;
class WebPage;
struct RenderProcessInfo;

class LayerTreeHost final : public FrameRenderer, public CanMakeCheckedPtr<LayerTreeHost>, public WebCore::GraphicsLayerFactory, public WebCore::CoordinatedPlatformLayer::Client {
    WTF_MAKE_TZONE_ALLOCATED(LayerTreeHost);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(LayerTreeHost);
public:
    static std::unique_ptr<LayerTreeHost> create(WebPage&);

    explicit LayerTreeHost(WebPage&);
    ~LayerTreeHost();

    void willRenderFrame();
    void didRenderFrame();

#if ENABLE(DAMAGE_TRACKING)
    void notifyFrameDamageForTesting(WebCore::Region&&);
#endif

private:
    void updateRootLayer();
    WebCore::FloatRect visibleContentsRect() const;

    void requestCompositionForRenderingUpdate();

    // CoordinatedPlatformLayer::Client
    WebCore::SkiaPaintingEngine& paintingEngine() const override { return *m_skiaPaintingEngine.get(); }
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

    // FrameRenderer
    uint64_t surfaceID() const override;
    void updateRenderingWithForcedRepaint() override;
    void scheduleRenderingUpdate() override;
    bool canUpdateRendering() const override;
    void updateRendering() override;
    void suspend() override;
    void resume() override;
    void sizeDidChange() override;
    void backgroundColorDidChange() override;
    bool ensureDrawing() override;
    void fillGLInformation(RenderProcessInfo&&, CompletionHandler<void(RenderProcessInfo&&)>&&) override;
    WebCore::GraphicsLayerFactory* graphicsLayerFactory() override;
    void setRootCompositingLayer(WebCore::GraphicsLayer*) override;
    void setViewOverlayRootLayer(WebCore::GraphicsLayer*) override;
#if PLATFORM(WPE) && ENABLE(WPE_PLATFORM) && (USE(GBM) || OS(ANDROID))
    void preferredBufferFormatsDidChange() override;
#endif
#if ENABLE(DAMAGE_TRACKING)
    void resetDamageHistoryForTesting() override;
    void foreachRegionInDamageHistoryForTesting(Function<void(const WebCore::Region&)>&&) const override;
#endif
#if PLATFORM(GTK)
    void adjustTransientZoom(double, WebCore::FloatPoint, WebCore::FloatPoint) override;
    void commitTransientZoom(double, WebCore::FloatPoint, WebCore::FloatPoint) override;

    WebCore::FloatPoint constrainTransientZoomOrigin(double, WebCore::FloatPoint) const;
    WebCore::CoordinatedPlatformLayer* layerForTransientZoom() const;
    void applyTransientZoomToLayers(double, WebCore::FloatPoint);
#endif

    const WeakRef<WebPage> m_webPage;
    const Ref<CoordinatedSceneState> m_sceneState;
    WebCore::GraphicsLayer* m_rootCompositingLayer { nullptr };
    WebCore::GraphicsLayer* m_overlayCompositingLayer { nullptr };
    bool m_pendingResize { false };
    bool m_pendingForceRepaint { false };
    bool m_waitUntilPaintingComplete { false };
    bool m_isWaitingForRenderer { false };
    bool m_scheduledWhileWaitingForRenderer { false };
    bool m_forceFrameSync { false };
    bool m_compositionRequired { false };
#if ENABLE(SCROLLING_THREAD)
    bool m_compositionRequiredInScrollingThread { false };
#endif
    RefPtr<ThreadedCompositor> m_compositor;
    std::unique_ptr<WebCore::SkiaPaintingEngine> m_skiaPaintingEngine;
    HashMap<uint64_t, Ref<WebCore::CoordinatedImageBackingStore>> m_imageBackingStores;

#if PLATFORM(GTK)
    bool m_transientZoom { false };
    double m_transientZoomScale { 1 };
    WebCore::FloatPoint m_transientZoomOrigin;
#endif

#if ENABLE(DAMAGE_TRACKING)
    mutable Lock m_frameDamageHistoryForTestingLock;
    Vector<WebCore::Region> m_frameDamageHistoryForTesting WTF_GUARDED_BY_LOCK(m_frameDamageHistoryForTestingLock);
    std::shared_ptr<WebCore::Damage> m_damageInGlobalCoordinateSpace;
#endif
};

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)
