/*
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2013 Company 100, Inc. All rights reserved.
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

#ifndef CompositingCoordinator_h
#define CompositingCoordinator_h

#if USE(COORDINATED_GRAPHICS)

#include "UpdateAtlas.h"
#include <WebCore/CoordinatedGraphicsLayer.h>
#include <WebCore/CoordinatedGraphicsState.h>
#include <WebCore/CoordinatedImageBacking.h>
#include <WebCore/FloatPoint.h>
#include <WebCore/GraphicsLayerClient.h>
#include <WebCore/GraphicsLayerFactory.h>
#include <WebCore/IntRect.h>
#include <WebCore/Timer.h>

namespace WebCore {
class Page;
class GraphicsContext;
class GraphicsLayer;
class CoordinatedSurface;
}

namespace WebKit {

class CompositingCoordinator final : public WebCore::GraphicsLayerClient
    , public WebCore::CoordinatedGraphicsLayerClient
    , public WebCore::CoordinatedImageBacking::Client
    , public UpdateAtlas::Client
    , public WebCore::GraphicsLayerFactory {
    WTF_MAKE_NONCOPYABLE(CompositingCoordinator);
public:
    class Client {
    public:
        virtual void didFlushRootLayer(const WebCore::FloatRect& visibleContentRect) = 0;
        virtual void notifyFlushRequired() = 0;
        virtual void commitSceneState(const WebCore::CoordinatedGraphicsState&) = 0;
        virtual void paintLayerContents(const WebCore::GraphicsLayer*, WebCore::GraphicsContext&, const WebCore::IntRect& clipRect) = 0;
    };

    CompositingCoordinator(WebCore::Page*, CompositingCoordinator::Client&);
    virtual ~CompositingCoordinator();

    void invalidate();

    void setRootCompositingLayer(WebCore::GraphicsLayer*);
    void setViewOverlayRootLayer(WebCore::GraphicsLayer*);
    void sizeDidChange(const WebCore::IntSize&);
    void deviceOrPageScaleFactorChanged();

    void setVisibleContentsRect(const WebCore::FloatRect&, const WebCore::FloatPoint&);
    void renderNextFrame();
    void commitScrollOffset(uint32_t layerID, const WebCore::IntSize& offset);

    void createRootLayer(const WebCore::IntSize&);
    WebCore::GraphicsLayer* rootLayer() const { return m_rootLayer.get(); }
    WebCore::CoordinatedGraphicsLayer* mainContentsLayer();

    bool flushPendingLayerChanges();
    WebCore::CoordinatedGraphicsState& state() { return m_state; }

    void syncDisplayState();

#if ENABLE(REQUEST_ANIMATION_FRAME)
    double nextAnimationServiceTime() const;
#endif

private:
    // GraphicsLayerClient
    void notifyAnimationStarted(const WebCore::GraphicsLayer*, const String&, double time) override;
    void notifyFlushRequired(const WebCore::GraphicsLayer*) override;
    void paintContents(const WebCore::GraphicsLayer*, WebCore::GraphicsContext&, WebCore::GraphicsLayerPaintingPhase, const WebCore::FloatRect& clipRect) override;
    float deviceScaleFactor() const override;
    float pageScaleFactor() const override;

    // CoordinatedImageBacking::Client
    void createImageBacking(WebCore::CoordinatedImageBackingID) override;
    void updateImageBacking(WebCore::CoordinatedImageBackingID, RefPtr<WebCore::CoordinatedSurface>&&) override;
    void clearImageBackingContents(WebCore::CoordinatedImageBackingID) override;
    void removeImageBacking(WebCore::CoordinatedImageBackingID) override;

    // CoordinatedGraphicsLayerClient
    bool isFlushingLayerChanges() const override { return m_isFlushingLayerChanges; }
    WebCore::FloatRect visibleContentsRect() const override;
    Ref<WebCore::CoordinatedImageBacking> createImageBackingIfNeeded(WebCore::Image*) override;
    void detachLayer(WebCore::CoordinatedGraphicsLayer*) override;
    bool paintToSurface(const WebCore::IntSize&, WebCore::CoordinatedSurface::Flags, uint32_t& /* atlasID */, WebCore::IntPoint&, WebCore::CoordinatedSurface::Client&) override;
    void syncLayerState(WebCore::CoordinatedLayerID, WebCore::CoordinatedGraphicsLayerState&) override;

    // UpdateAtlas::Client
    void createUpdateAtlas(uint32_t atlasID, RefPtr<WebCore::CoordinatedSurface>&&) override;
    void removeUpdateAtlas(uint32_t atlasID) override;

    // GraphicsLayerFactory
    std::unique_ptr<WebCore::GraphicsLayer> createGraphicsLayer(WebCore::GraphicsLayer::Type, WebCore::GraphicsLayerClient&) override;

    void initializeRootCompositingLayerIfNeeded();
    void flushPendingImageBackingChanges();
    void clearPendingStateChanges();

    void purgeBackingStores();

    void scheduleReleaseInactiveAtlases();

    void releaseInactiveAtlasesTimerFired();

    double timestamp() const;

    WebCore::Page* m_page;
    CompositingCoordinator::Client& m_client;

    std::unique_ptr<WebCore::GraphicsLayer> m_rootLayer;
    WebCore::GraphicsLayer* m_rootCompositingLayer { nullptr };
    WebCore::GraphicsLayer* m_overlayCompositingLayer { nullptr };

    WebCore::CoordinatedGraphicsState m_state;

    typedef HashMap<WebCore::CoordinatedLayerID, WebCore::CoordinatedGraphicsLayer*> LayerMap;
    LayerMap m_registeredLayers;
    typedef HashMap<WebCore::CoordinatedImageBackingID, RefPtr<WebCore::CoordinatedImageBacking> > ImageBackingMap;
    ImageBackingMap m_imageBackings;
    Vector<std::unique_ptr<UpdateAtlas>> m_updateAtlases;

    // We don't send the messages related to releasing resources to renderer during purging, because renderer already had removed all resources.
    bool m_isDestructing { false };
    bool m_isPurging { false };
    bool m_isFlushingLayerChanges { false };
    bool m_shouldSyncFrame { false };
    bool m_didInitializeRootCompositingLayer { false };

    WebCore::FloatRect m_visibleContentsRect;
    WebCore::Timer m_releaseInactiveAtlasesTimer;

#if ENABLE(REQUEST_ANIMATION_FRAME)
    double m_lastAnimationServiceTime { 0 };
#endif
};

}

#endif // namespace WebKit

#endif // CompositingCoordinator_h
