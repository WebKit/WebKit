/*
 Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License as published by the Free Software Foundation; either
 version 2 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
 */


#ifndef CoordinatedGraphicsLayer_h
#define CoordinatedGraphicsLayer_h

#include "CoordinatedImageBacking.h"
#include "CoordinatedTile.h"
#include "FloatPoint3D.h"
#include "GraphicsLayer.h"
#include "GraphicsLayerAnimation.h"
#include "GraphicsLayerTransform.h"
#include "Image.h"
#include "IntSize.h"
#include "ShareableBitmap.h"
#include "TiledBackingStore.h"
#include "TiledBackingStoreClient.h"
#include "TransformationMatrix.h"
#include "UpdateInfo.h"
#include "WebLayerTreeInfo.h"
#include "WebProcess.h"
#if USE(GRAPHICS_SURFACE)
#include <WebCore/GraphicsSurfaceToken.h>
#endif
#include <WebCore/RunLoop.h>
#include <wtf/text/StringHash.h>

#if USE(COORDINATED_GRAPHICS)
namespace WebCore {
class CoordinatedGraphicsLayer;
class GraphicsLayerAnimations;
}

namespace WebKit {

class CoordinatedGraphicsLayerClient {
public:
    // CoordinatedTileClient
    virtual void createTile(WebLayerID, uint32_t tileID, const SurfaceUpdateInfo&, const WebCore::IntRect&) = 0;
    virtual void updateTile(WebLayerID, uint32_t tileID, const SurfaceUpdateInfo&, const WebCore::IntRect&) = 0;
    virtual void removeTile(WebLayerID, uint32_t tileID) = 0;

    virtual WebCore::IntRect visibleContentsRect() const = 0;
    virtual bool layerTreeTileUpdatesAllowed() const = 0;
    virtual PassRefPtr<CoordinatedImageBacking> createImageBackingIfNeeded(WebCore::Image*) = 0;
    virtual void syncLayerState(WebLayerID, const WebLayerInfo&) = 0;
    virtual void syncLayerChildren(WebLayerID, const Vector<WebLayerID>&) = 0;
#if ENABLE(CSS_FILTERS)
    virtual void syncLayerFilters(WebLayerID, const WebCore::FilterOperations&) = 0;
#endif
#if USE(GRAPHICS_SURFACE)
    virtual void createCanvas(WebLayerID, WebCore::PlatformLayer*) = 0;
    virtual void syncCanvas(WebLayerID, WebCore::PlatformLayer*) = 0;
    virtual void destroyCanvas(WebLayerID) = 0;
#endif

    virtual void setLayerAnimations(WebLayerID, const WebCore::GraphicsLayerAnimations&) = 0;

    virtual void detachLayer(WebCore::CoordinatedGraphicsLayer*) = 0;
    virtual void syncFixedLayers() = 0;
    virtual PassOwnPtr<WebCore::GraphicsContext> beginContentUpdate(const WebCore::IntSize&, ShareableBitmap::Flags, int& atlasID, WebCore::IntPoint&) = 0;
};
}

namespace WebCore {

class CoordinatedGraphicsLayer : public GraphicsLayer
    , public TiledBackingStoreClient
    , public WebKit::CoordinatedImageBacking::Host
    , public WebKit::CoordinatedTileClient {
public:
    explicit CoordinatedGraphicsLayer(GraphicsLayerClient*);
    virtual ~CoordinatedGraphicsLayer();

    // Reimplementations from GraphicsLayer.h.
    virtual bool setChildren(const Vector<GraphicsLayer*>&) OVERRIDE;
    virtual void addChild(GraphicsLayer*) OVERRIDE;
    virtual void addChildAtIndex(GraphicsLayer*, int) OVERRIDE;
    virtual void addChildAbove(GraphicsLayer*, GraphicsLayer*) OVERRIDE;
    virtual void addChildBelow(GraphicsLayer*, GraphicsLayer*) OVERRIDE;
    virtual bool replaceChild(GraphicsLayer*, GraphicsLayer*) OVERRIDE;
    virtual void removeFromParent() OVERRIDE;
    virtual void setPosition(const FloatPoint&) OVERRIDE;
    virtual void setAnchorPoint(const FloatPoint3D&) OVERRIDE;
    virtual void setSize(const FloatSize&) OVERRIDE;
    virtual void setTransform(const TransformationMatrix&) OVERRIDE;
    virtual void setChildrenTransform(const TransformationMatrix&) OVERRIDE;
    virtual void setPreserves3D(bool) OVERRIDE;
    virtual void setMasksToBounds(bool) OVERRIDE;
    virtual void setDrawsContent(bool) OVERRIDE;
    virtual void setContentsVisible(bool) OVERRIDE;
    virtual void setContentsOpaque(bool) OVERRIDE;
    virtual void setBackfaceVisibility(bool) OVERRIDE;
    virtual void setOpacity(float) OVERRIDE;
    virtual void setContentsRect(const IntRect&) OVERRIDE;
    virtual void setContentsToImage(Image*) OVERRIDE;
    virtual void setContentsToBackgroundColor(const Color&) OVERRIDE;
    virtual bool shouldDirectlyCompositeImage(Image*) const OVERRIDE;
    virtual void setContentsToCanvas(PlatformLayer*) OVERRIDE;
    virtual void setMaskLayer(GraphicsLayer*) OVERRIDE;
    virtual void setReplicatedByLayer(GraphicsLayer*) OVERRIDE;
    virtual void setNeedsDisplay() OVERRIDE;
    virtual void setNeedsDisplayInRect(const FloatRect&) OVERRIDE;
    virtual void setContentsNeedsDisplay() OVERRIDE;
    virtual void flushCompositingState(const FloatRect&) OVERRIDE;
    virtual void flushCompositingStateForThisLayerOnly() OVERRIDE;
#if ENABLE(CSS_FILTERS)
    virtual bool setFilters(const FilterOperations&) OVERRIDE;
#endif
    virtual bool addAnimation(const KeyframeValueList&, const IntSize&, const Animation*, const String&, double) OVERRIDE;
    virtual void pauseAnimation(const String&, double) OVERRIDE;
    virtual void removeAnimation(const String&) OVERRIDE;
    virtual void suspendAnimations(double time) OVERRIDE;
    virtual void resumeAnimations() OVERRIDE;
    
    void setContentsScale(float);
    void setVisibleContentRectTrajectoryVector(const FloatPoint&);

    void setRootLayer(bool);

    WebKit::WebLayerID id() const;

    void setFixedToViewport(bool isFixed) { m_fixedToViewport = isFixed; }

    IntRect coverRect() const { return m_mainBackingStore ? m_mainBackingStore->mapToContents(m_mainBackingStore->coverRect()) : IntRect(); }

    static void initFactory();

    // TiledBackingStoreClient
    virtual void tiledBackingStorePaintBegin() OVERRIDE;
    virtual void tiledBackingStorePaint(GraphicsContext*, const IntRect&) OVERRIDE;
    virtual void tiledBackingStorePaintEnd(const Vector<IntRect>& paintedArea) OVERRIDE;
    virtual bool tiledBackingStoreUpdatesAllowed() const OVERRIDE;
    virtual IntRect tiledBackingStoreContentsRect() OVERRIDE;
    virtual IntRect tiledBackingStoreVisibleRect() OVERRIDE;
    virtual Color tiledBackingStoreBackgroundColor() const OVERRIDE;

    // CoordinatedTileClient
    virtual void createTile(uint32_t tileID, const WebKit::SurfaceUpdateInfo&, const IntRect&) OVERRIDE;
    virtual void updateTile(uint32_t tileID, const WebKit::SurfaceUpdateInfo&, const IntRect&) OVERRIDE;
    virtual void removeTile(uint32_t tileID) OVERRIDE;
    virtual PassOwnPtr<GraphicsContext> beginContentUpdate(const IntSize&, int& atlasID, IntPoint&) OVERRIDE;

    void setCoordinator(WebKit::CoordinatedGraphicsLayerClient*);

    void adjustVisibleRect();
    void purgeBackingStores();
    bool hasPendingVisibleChanges();

private:
    bool fixedToViewport() const { return m_fixedToViewport; }

    void didChangeLayerState();
    void didChangeAnimations();
    void didChangeGeometry();
    void didChangeChildren();
#if ENABLE(CSS_FILTERS)
    void didChangeFilters();
#endif
    void didChangeImageBacking();

    void syncLayerState();
    void syncAnimations();
    void syncChildren();
#if ENABLE(CSS_FILTERS)
    void syncFilters();
#endif
    void syncImageBacking();
    void syncCanvas();
    void computeTransformedVisibleRect();
    void updateContentBuffers();

    void createBackingStore();
    void releaseImageBackingIfNeeded();

    // CoordinatedImageBacking::Host
    virtual bool imageBackingVisible() OVERRIDE;

    void destroyCanvasIfNeeded();
    void createCanvasIfNeeded();

    bool shouldHaveBackingStore() const;
    bool selfOrAncestorHasActiveTransformAnimation() const;
    bool selfOrAncestorHaveNonAffineTransforms();
    void adjustContentsScale();

    void setShouldUpdateVisibleRect();
    float effectiveContentsScale();

    void animationStartedTimerFired(Timer<CoordinatedGraphicsLayer>*);

    WebKit::WebLayerID m_id;
    WebKit::WebLayerInfo m_layerInfo;
    GraphicsLayerTransform m_layerTransform;
    TransformationMatrix m_cachedInverseTransform;
    bool m_inUpdateMode : 1;
    bool m_shouldUpdateVisibleRect: 1;
    bool m_shouldSyncLayerState: 1;
    bool m_shouldSyncChildren: 1;
    bool m_shouldSyncFilters: 1;
    bool m_shouldSyncImageBacking: 1;
    bool m_shouldSyncAnimations: 1;
    bool m_fixedToViewport : 1;
    bool m_canvasNeedsDisplay : 1;
    bool m_canvasNeedsCreate : 1;
    bool m_canvasNeedsDestroy : 1;

    WebKit::CoordinatedGraphicsLayerClient* m_coordinator;
    OwnPtr<TiledBackingStore> m_mainBackingStore;
    OwnPtr<TiledBackingStore> m_previousBackingStore;
    float m_contentsScale;

    RefPtr<Image> m_compositedImage;
    NativeImagePtr m_compositedNativeImagePtr;
    RefPtr<WebKit::CoordinatedImageBacking> m_coordinatedImageBacking;

    PlatformLayer* m_canvasPlatformLayer;
#if USE(GRAPHICS_SURFACE)
    GraphicsSurfaceToken m_canvasToken;
#endif
    Timer<CoordinatedGraphicsLayer> m_animationStartedTimer;
    GraphicsLayerAnimations m_animations;
    double m_lastAnimationStartTime;
};

CoordinatedGraphicsLayer* toCoordinatedGraphicsLayer(GraphicsLayer*);

}
#endif

#endif // CoordinatedGraphicsLayer_H
