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

#include "CoordinatedGraphicsLayer.h"
#include "CoordinatedGraphicsState.h"
#include "CoordinatedImageBacking.h"
#include "FloatPoint.h"
#include "GraphicsLayerClient.h"
#include "GraphicsLayerFactory.h"
#include "IntRect.h"
#include "Timer.h"
#include "UpdateAtlas.h"

namespace WebCore {

class Page;
class GraphicsContext;
class GraphicsLayer;
class CoordinatedSurface;

class CompositingCoordinator : public GraphicsLayerClient
    , public CoordinatedGraphicsLayerClient
    , public CoordinatedImageBacking::Client
    , public UpdateAtlas::Client
    , public GraphicsLayerFactory {
    WTF_MAKE_NONCOPYABLE(CompositingCoordinator); WTF_MAKE_FAST_ALLOCATED;
public:
    class Client {
    public:
        virtual void didFlushRootLayer() = 0;
        virtual void notifyFlushRequired() = 0;
        virtual void commitSceneState(const CoordinatedGraphicsState&) = 0;
        virtual void paintLayerContents(const GraphicsLayer*, GraphicsContext&, const IntRect& clipRect) = 0;
    };

    CompositingCoordinator(Page*, CompositingCoordinator::Client*);
    virtual ~CompositingCoordinator();

    void setRootCompositingLayer(GraphicsLayer*);
    void sizeDidChange(const IntSize& newSize);
    void deviceOrPageScaleFactorChanged();

    void setVisibleContentsRect(const FloatRect&, const FloatPoint&);
    void renderNextFrame();
    void purgeBackingStores();
    void commitScrollOffset(uint32_t layerID, const IntSize& offset);

    void createRootLayer(const IntSize&);
    void clearRootLayer() { m_rootLayer = nullptr; }
    GraphicsLayer* rootLayer() const { return m_rootLayer.get(); }
    CoordinatedGraphicsLayer* mainContentsLayer();

    bool flushPendingLayerChanges();
    CoordinatedGraphicsState& state() { return m_state; }

    void syncDisplayState();

#if ENABLE(REQUEST_ANIMATION_FRAME)
    double nextAnimationServiceTime() const;
#endif

private:
    // GraphicsLayerClient
    virtual void notifyAnimationStarted(const GraphicsLayer*, double time) override;
    virtual void notifyFlushRequired(const GraphicsLayer*) override;
    virtual void paintContents(const GraphicsLayer*, GraphicsContext&, GraphicsLayerPaintingPhase, const FloatRect& clipRect) override;
    virtual float deviceScaleFactor() const override;
    virtual float pageScaleFactor() const override;

    // CoordinatedImageBacking::Client
    virtual void createImageBacking(CoordinatedImageBackingID) override;
    virtual void updateImageBacking(CoordinatedImageBackingID, PassRefPtr<CoordinatedSurface>) override;
    virtual void clearImageBackingContents(CoordinatedImageBackingID) override;
    virtual void removeImageBacking(CoordinatedImageBackingID) override;

    // CoordinatedGraphicsLayerClient
    virtual bool isFlushingLayerChanges() const override { return m_isFlushingLayerChanges; }
    virtual FloatRect visibleContentsRect() const override;
    virtual PassRefPtr<CoordinatedImageBacking> createImageBackingIfNeeded(Image*) override;
    virtual void detachLayer(CoordinatedGraphicsLayer*) override;
    virtual bool paintToSurface(const WebCore::IntSize&, WebCore::CoordinatedSurface::Flags, uint32_t& /* atlasID */, WebCore::IntPoint&, WebCore::CoordinatedSurface::Client*) override;
    virtual void syncLayerState(CoordinatedLayerID, CoordinatedGraphicsLayerState&) override;

    // UpdateAtlas::Client
    virtual void createUpdateAtlas(uint32_t atlasID, PassRefPtr<CoordinatedSurface>) override;
    virtual void removeUpdateAtlas(uint32_t atlasID) override;

    // GraphicsLayerFactory
    virtual std::unique_ptr<GraphicsLayer> createGraphicsLayer(GraphicsLayerClient*) override;

    void initializeRootCompositingLayerIfNeeded();
    void flushPendingImageBackingChanges();
    void clearPendingStateChanges();

    void scheduleReleaseInactiveAtlases();

    void releaseInactiveAtlasesTimerFired(Timer<CompositingCoordinator>*);

    Page* m_page;
    CompositingCoordinator::Client* m_client;

    std::unique_ptr<GraphicsLayer> m_rootLayer;
    GraphicsLayer* m_rootCompositingLayer;

    CoordinatedGraphicsState m_state;

    typedef HashMap<CoordinatedLayerID, CoordinatedGraphicsLayer*> LayerMap;
    LayerMap m_registeredLayers;
    typedef HashMap<CoordinatedImageBackingID, RefPtr<CoordinatedImageBacking> > ImageBackingMap;
    ImageBackingMap m_imageBackings;
    Vector<std::unique_ptr<UpdateAtlas>> m_updateAtlases;

    // We don't send the messages related to releasing resources to renderer during purging, because renderer already had removed all resources.
    bool m_isPurging;
    bool m_isFlushingLayerChanges;

    FloatRect m_visibleContentsRect;

    bool m_shouldSyncFrame;
    bool m_didInitializeRootCompositingLayer;
    Timer<CompositingCoordinator> m_releaseInactiveAtlasesTimer;

#if ENABLE(REQUEST_ANIMATION_FRAME)
    double m_lastAnimationServiceTime;
#endif
};

}

#endif

#endif // CompositingCoordinator_h
