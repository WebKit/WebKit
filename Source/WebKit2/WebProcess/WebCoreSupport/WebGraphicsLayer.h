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


#ifndef WebGraphicsLayer_h
#define WebGraphicsLayer_h

#include "FloatPoint3D.h"
#include "GraphicsLayer.h"
#include "Image.h"
#include "IntSize.h"
#include "LayerTransform.h"
#include "ShareableBitmap.h"
#include "TiledBackingStore.h"
#include "TiledBackingStoreClient.h"
#include "TiledBackingStoreRemoteTile.h"
#include "TransformationMatrix.h"
#include "UpdateInfo.h"
#include "WebLayerTreeInfo.h"
#include "WebProcess.h"
#include <WebCore/RunLoop.h>
#include <wtf/text/StringHash.h>

#if USE(UI_SIDE_COMPOSITING)
namespace WebCore {
class WebGraphicsLayer;
}

namespace WebKit {

class WebGraphicsLayerClient {
public:
    // TiledBackingStoreRemoteTileClient
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
    virtual void attachLayer(WebCore::WebGraphicsLayer*) = 0;
    virtual void detachLayer(WebCore::WebGraphicsLayer*) = 0;
    virtual void syncFixedLayers() = 0;
    virtual PassOwnPtr<WebCore::GraphicsContext> beginContentUpdate(const WebCore::IntSize&, ShareableBitmap::Flags, ShareableSurface::Handle&, WebCore::IntPoint&) = 0;
};
}

namespace WebCore {

class WebGraphicsLayer : public WebCore::GraphicsLayer
                       , public TiledBackingStoreClient
                       , public WebKit::TiledBackingStoreRemoteTileClient
{
public:
    WebGraphicsLayer(GraphicsLayerClient*);
    virtual ~WebGraphicsLayer();

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
    void setContentsOpaque(bool);
    void setBackfaceVisibility(bool);
    void setOpacity(float);
    void setContentsRect(const IntRect&);
    void setContentsToImage(Image*);
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
    static WebGraphicsLayer* layerByID(WebKit::WebLayerID);
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

    // TiledBackingStoreRemoteTileClient
    virtual void createTile(int tileID, const WebKit::SurfaceUpdateInfo&, const WebCore::IntRect&);
    virtual void updateTile(int tileID, const WebKit::SurfaceUpdateInfo&, const WebCore::IntRect&);
    virtual void removeTile(int tileID);
    virtual PassOwnPtr<WebCore::GraphicsContext> beginContentUpdate(const WebCore::IntSize&, WebKit::ShareableSurface::Handle&, WebCore::IntPoint&);

    void setWebGraphicsLayerClient(WebKit::WebGraphicsLayerClient*);
    void syncChildren();
    void syncLayerState();
#if ENABLE(CSS_FILTERS)
    void syncFilters();
#endif
    void ensureImageBackingStore();

    void adjustVisibleRect();
    bool isReadyForTileBufferSwap() const;
    void updateContentBuffers();
    void purgeBackingStores();

private:
    virtual void willBeDestroyed();
    WebKit::WebLayerID m_id;
    WebKit::WebLayerInfo m_layerInfo;
    RefPtr<Image> m_image;
    GraphicsLayer* m_maskTarget;
    FloatRect m_needsDisplayRect;
    LayerTransform m_layerTransform;
    bool m_inUpdateMode : 1;
    bool m_shouldUpdateVisibleRect: 1;
    bool m_shouldSyncLayerState: 1;
    bool m_shouldSyncChildren: 1;
    bool m_shouldSyncFilters: 1;
    bool m_fixedToViewport : 1;

    void notifyChange();
    void didChangeGeometry();
    void didChangeLayerState();
    void didChangeChildren();
#if ENABLE(CSS_FILTERS)
    void didChangeFilters();
#endif

    void createBackingStore();

    bool selfOrAncestorHaveNonAffineTransforms();
    bool shouldUseTiledBackingStore();
    void adjustContentsScale();
    void computeTransformedVisibleRect();
    void syncLayerParameters();
    void setShouldUpdateVisibleRect();
    float effectiveContentsScale();

    WebKit::WebGraphicsLayerClient* m_webGraphicsLayerClient;
    OwnPtr<WebCore::TiledBackingStore> m_mainBackingStore;
    OwnPtr<WebCore::TiledBackingStore> m_previousBackingStore;
    float m_contentsScale;
};

WebGraphicsLayer* toWebGraphicsLayer(GraphicsLayer*);

}
#endif

#endif // WebGraphicsLayer_H
