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
#include "RunLoop.h"
#include "ShareableBitmap.h"
#include "TiledBackingStore.h"
#include "TiledBackingStoreClient.h"
#include "TiledBackingStoreRemoteTile.h"
#include "TransformationMatrix.h"
#include "UpdateInfo.h"
#include "WebLayerTreeInfo.h"
#include "WebProcess.h"

#if USE(ACCELERATED_COMPOSITING)

namespace WebKit {

class WebLayerTreeTileClient {
public:
    // TiledBackingStoreRemoteTileClient
    virtual void createTile(WebLayerID, int tileID, const UpdateInfo&) = 0;
    virtual void updateTile(WebLayerID, int tileID, const UpdateInfo&) = 0;
    virtual void removeTile(WebLayerID, int tileID) = 0;
    virtual bool layerTreeTileUpdatesAllowed() const = 0;
    virtual int64_t adoptImageBackingStore(WebCore::Image*) = 0;
    virtual void releaseImageBackingStore(int64_t) = 0;
    virtual void didSyncCompositingStateForLayer(const WebLayerInfo&) = 0;
    virtual void didDeleteLayer(WebLayerID) = 0;
};
}

namespace WebCore {

class WebGraphicsLayer : public WebCore::GraphicsLayer
#if USE(TILED_BACKING_STORE)
                       , public TiledBackingStoreClient
                       , public WebKit::TiledBackingStoreRemoteTileClient
#endif
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
    bool addAnimation(const KeyframeValueList&, const IntSize&, const Animation*, const String&, double);
    void pauseAnimation(const String&, double);
    void removeAnimation(const String&);
    void setContentsToImage(Image*);
    void setMaskLayer(GraphicsLayer*);
    void setReplicatedByLayer(GraphicsLayer*);
    void setNeedsDisplay();
    void setNeedsDisplayInRect(const FloatRect&);
    void setContentsNeedsDisplay();
    void setVisibleContentRectAndScale(const IntRect&, float scale);
    void setVisibleContentRectTrajectoryVector(const FloatPoint&);
    virtual void syncCompositingState(const FloatRect&);
    virtual void syncCompositingStateForThisLayerOnly();
    void setRootLayer(bool);

    WebKit::WebLayerID id() const;
    static WebGraphicsLayer* layerByID(WebKit::WebLayerID);
    bool isModified() const { return m_modified; }
    void didSynchronize();
    Image* image() { return m_image.get(); }
    void notifyAnimationStarted(double);

    GraphicsLayer* maskTarget() const { return m_maskTarget; }
    void setMaskTarget(GraphicsLayer* layer) { m_maskTarget = layer; }

    static void initFactory();

#if USE(TILED_BACKING_STORE)
    // TiledBackingStoreClient
    virtual void tiledBackingStorePaintBegin();
    virtual void tiledBackingStorePaint(GraphicsContext*, const IntRect&);
    virtual void tiledBackingStorePaintEnd(const Vector<IntRect>& paintedArea);
    virtual bool tiledBackingStoreUpdatesAllowed() const;
    virtual IntRect tiledBackingStoreContentsRect();
    virtual IntRect tiledBackingStoreVisibleRect();
    virtual Color tiledBackingStoreBackgroundColor() const;

    // TiledBackingStoreRemoteTileClient
    virtual void createTile(int tileID, const WebKit::UpdateInfo&);
    virtual void updateTile(int tileID, const WebKit::UpdateInfo&);
    virtual void removeTile(int tileID);

    void setLayerTreeTileClient(WebKit::WebLayerTreeTileClient*);
    WebKit::WebLayerTreeTileClient* layerTreeTileClient() const;

    bool isReadyForTileBufferSwap() const;
    void updateTileBuffersRecursively();
    void updateContentBuffers();
    void purgeBackingStores();
    void recreateBackingStoreIfNeeded();
#endif

private:
    WebKit::WebLayerInfo m_layerInfo;
    RefPtr<Image> m_image;
    GraphicsLayer* m_maskTarget;
    FloatRect m_needsDisplayRect;
    IntRect m_pageVisibleRect;
    LayerTransform m_layerTransform;
    bool m_needsDisplay : 1;
    bool m_modified : 1;
    bool m_contentNeedsDisplay : 1;
    bool m_hasPendingAnimations : 1;
    bool m_inUpdateMode : 2;

    void notifyChange();

#if USE(TILED_BACKING_STORE)
    void computeTransformedVisibleRect();

    WebKit::WebLayerTreeTileClient* m_layerTreeTileClient;
    OwnPtr<WebCore::TiledBackingStore> m_mainBackingStore;
    OwnPtr<WebCore::TiledBackingStore> m_previousBackingStore;
    float m_contentsScale;
#endif
};

WebGraphicsLayer* toWebGraphicsLayer(GraphicsLayer*);

}
#endif

#endif // WebGraphicsLayer_H
