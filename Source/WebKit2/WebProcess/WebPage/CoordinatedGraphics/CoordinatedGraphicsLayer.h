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
#include <WebCore/RunLoop.h>
#include <wtf/text/StringHash.h>

#if USE(COORDINATED_GRAPHICS)
namespace WebCore {
class CoordinatedGraphicsLayer;
}

namespace WebKit {

class CoordinatedGraphicsLayerClient {
public:
    // CoordinatedTileClient
    virtual void createTile(WebLayerID, int tileID, const SurfaceUpdateInfo&, const WebCore::IntRect&) = 0;
    virtual void updateTile(WebLayerID, int tileID, const SurfaceUpdateInfo&, const WebCore::IntRect&) = 0;
    virtual void removeTile(WebLayerID, int tileID) = 0;

    virtual WebCore::IntRect visibleContentsRect() const = 0;
    virtual bool layerTreeTileUpdatesAllowed() const = 0;
    virtual int64_t adoptImageBackingStore(WebCore::Image*) = 0;
    virtual void releaseImageBackingStore(int64_t) = 0;
    virtual void syncLayerState(WebLayerID, const WebLayerInfo&) = 0;
    virtual void syncLayerChildren(WebLayerID, const Vector<WebLayerID>&) = 0;
#if ENABLE(CSS_FILTERS)
    virtual void syncLayerFilters(WebLayerID, const WebCore::FilterOperations&) = 0;
#endif
#if PLATFORM(QT)
    virtual void syncCanvas(WebLayerID, const WebCore::IntSize& canvasSize, uint64_t graphicsSurfaceToken, uint32_t frontBuffer) = 0;
#endif

    virtual void setLayerAnimatedOpacity(WebLayerID, float) = 0;
    virtual void setLayerAnimatedTransform(WebLayerID, const WebCore::TransformationMatrix&) = 0;

    virtual void attachLayer(WebCore::CoordinatedGraphicsLayer*) = 0;
    virtual void detachLayer(WebCore::CoordinatedGraphicsLayer*) = 0;
    virtual void syncFixedLayers() = 0;
    virtual PassOwnPtr<WebCore::GraphicsContext> beginContentUpdate(const WebCore::IntSize&, ShareableBitmap::Flags, ShareableSurface::Handle&, WebCore::IntPoint&) = 0;
};
}

namespace WebCore {

class CoordinatedGraphicsLayer : public WebCore::GraphicsLayer
                       , public TiledBackingStoreClient
                       , public WebKit::CoordinatedTileClient
                       , public GraphicsLayerAnimation::Client {
public:
    CoordinatedGraphicsLayer(GraphicsLayerClient*);
    virtual ~CoordinatedGraphicsLayer();

    // Reimplementations from GraphicsLayer.h.
    bool setChildren(const Vector<GraphicsLayer*>&);
    void addChild(GraphicsLayer*);
    void addChildAtIndex(GraphicsLayer*, int);
    void addChildAbove(GraphicsLayer*, GraphicsLayer*);
    void addChildBelow(GraphicsLayer*, GraphicsLayer*);
    bool replaceChild(GraphicsLayer*, GraphicsLayer*);
    void removeFromParent();
    void setPosition(const FloatPoint&);
    void setAnchorPoint(const FloatPoint3D&);
    void setSize(const FloatSize&);
    void setTransform(const TransformationMatrix&);
    void setChildrenTransform(const TransformationMatrix&);
    void setPreserves3D(bool);
    void setMasksToBounds(bool);
    void setDrawsContent(bool);
    void setContentsVisible(bool);
    void setContentsOpaque(bool);
    void setBackfaceVisibility(bool);
    void setOpacity(float);
    void setContentsRect(const IntRect&);
    void setContentsToImage(Image*);
    void setContentsToCanvas(PlatformLayer*);
    void setMaskLayer(GraphicsLayer*);
    void setReplicatedByLayer(GraphicsLayer*);
    void setNeedsDisplay();
    void setNeedsDisplayInRect(const FloatRect&);
    void setContentsNeedsDisplay();
    void setContentsScale(float);
    void setVisibleContentRectTrajectoryVector(const FloatPoint&);
    virtual void syncCompositingState(const FloatRect&);
    virtual void syncCompositingStateForThisLayerOnly();
#if ENABLE(CSS_FILTERS)
    bool setFilters(const FilterOperations&);
#endif

    void setRootLayer(bool);

    WebKit::WebLayerID id() const;
    static CoordinatedGraphicsLayer* layerByID(WebKit::WebLayerID);
    void didSynchronize();
    Image* image() { return m_image.get(); }

    bool fixedToViewport() const { return m_fixedToViewport; }
    void setFixedToViewport(bool isFixed) { m_fixedToViewport = isFixed; }

    GraphicsLayer* maskTarget() const { return m_maskTarget; }
    void setMaskTarget(GraphicsLayer* layer) { m_maskTarget = layer; }

    static void initFactory();

    // TiledBackingStoreClient
    virtual void tiledBackingStorePaintBegin();
    virtual void tiledBackingStorePaint(GraphicsContext*, const IntRect&);
    virtual void tiledBackingStorePaintEnd(const Vector<IntRect>& paintedArea);
    virtual bool tiledBackingStoreUpdatesAllowed() const;
    virtual IntRect tiledBackingStoreContentsRect();
    virtual IntRect tiledBackingStoreVisibleRect();
    virtual Color tiledBackingStoreBackgroundColor() const;

    // CoordinatedTileClient
    virtual void createTile(int tileID, const WebKit::SurfaceUpdateInfo&, const WebCore::IntRect&);
    virtual void updateTile(int tileID, const WebKit::SurfaceUpdateInfo&, const WebCore::IntRect&);
    virtual void removeTile(int tileID);
    virtual PassOwnPtr<WebCore::GraphicsContext> beginContentUpdate(const WebCore::IntSize&, WebKit::ShareableSurface::Handle&, WebCore::IntPoint&);

    void setCoordinatedGraphicsLayerClient(WebKit::CoordinatedGraphicsLayerClient*);
    void syncChildren();
    void syncLayerState();
#if ENABLE(CSS_FILTERS)
    void syncFilters();
#endif
    void syncCanvas();
    void ensureImageBackingStore();

    void adjustVisibleRect();
    bool isReadyForTileBufferSwap() const;
    void updateContentBuffers();
    void purgeBackingStores();

    // GraphicsLayerAnimation::Client
    virtual void setAnimatedTransform(const TransformationMatrix&);
    virtual void setAnimatedOpacity(float);

    virtual bool addAnimation(const KeyframeValueList&, const IntSize&, const Animation*, const String&, double);
    virtual void pauseAnimation(const String&, double);
    virtual void removeAnimation(const String&);

private:
    virtual void willBeDestroyed();
    WebKit::WebLayerID m_id;
    WebKit::WebLayerInfo m_layerInfo;
    RefPtr<Image> m_image;
    GraphicsLayer* m_maskTarget;
    FloatRect m_needsDisplayRect;
    GraphicsLayerTransform m_layerTransform;
    bool m_inUpdateMode : 1;
    bool m_shouldUpdateVisibleRect: 1;
    bool m_shouldSyncLayerState: 1;
    bool m_shouldSyncChildren: 1;
    bool m_shouldSyncFilters: 1;
    bool m_shouldSyncAnimatedProperties: 1;
    bool m_fixedToViewport : 1;
    bool m_canvasNeedsDisplay : 1;

    void notifyChange();
    void didChangeAnimatedProperties();
    void didChangeGeometry();
    void didChangeLayerState();
    void didChangeChildren();
#if ENABLE(CSS_FILTERS)
    void didChangeFilters();
#endif

    float m_effectiveOpacity;
    TransformationMatrix m_effectiveTransform;

    void createBackingStore();

    bool selfOrAncestorHaveNonAffineTransforms();
    bool shouldUseTiledBackingStore();
    void adjustContentsScale();
    void computeTransformedVisibleRect();
    void syncLayerParameters();
    void syncAnimatedProperties();
    void setShouldUpdateVisibleRect();
    float effectiveContentsScale();

    void animationStartedTimerFired(WebCore::Timer<CoordinatedGraphicsLayer>*);

    WebKit::CoordinatedGraphicsLayerClient* m_CoordinatedGraphicsLayerClient;
    OwnPtr<WebCore::TiledBackingStore> m_mainBackingStore;
    OwnPtr<WebCore::TiledBackingStore> m_previousBackingStore;
    float m_contentsScale;
    PlatformLayer* m_canvasPlatformLayer;
    Timer<CoordinatedGraphicsLayer> m_animationStartedTimer;
    GraphicsLayerAnimations m_animations;
};

CoordinatedGraphicsLayer* toCoordinatedGraphicsLayer(GraphicsLayer*);

}
#endif

#endif // CoordinatedGraphicsLayer_H
