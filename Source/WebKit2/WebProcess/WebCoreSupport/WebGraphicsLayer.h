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
#include "RunLoop.h"
#include "ShareableBitmap.h"
#include "TransformationMatrix.h"
#include "WebLayerTreeInfo.h"
#include "WebProcess.h"

#if USE(ACCELERATED_COMPOSITING)

namespace WebKit {
class WebPage;
}

namespace WebCore {

class WebGraphicsLayer : public WebCore::GraphicsLayer {
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
    virtual void syncCompositingState();
    virtual void syncCompositingStateForThisLayerOnly();

    WebKit::WebLayerID id() const;
    const WebKit::WebLayerInfo& layerInfo() const;
    FloatRect needsDisplayRect() const;
    static Vector<WebKit::WebLayerID> takeLayersToDelete();
    static WebGraphicsLayer* layerByID(WebKit::WebLayerID);
    bool isModified() const { return m_modified; }
    void didSynchronize();
    Image* image() { return m_image.get(); }
    void notifyAnimationStarted(double);

    static void initFactory();
    static void sendLayersToUIProcess(WebCore::GraphicsLayer*, WebKit::WebPage*);

private:
    WebKit::WebLayerInfo m_layerInfo;
    RefPtr<WebKit::ShareableBitmap> m_backingStore;
    RefPtr<Image> m_image;
    FloatRect m_needsDisplayRect;
    bool m_needsDisplay;
    bool m_modified;
    bool m_contentNeedsDisplay;
    bool m_hasPendingAnimations;

    float m_contentScale;

    void notifyChange();
};

WebGraphicsLayer* toWebGraphicsLayer(GraphicsLayer*);

}
#endif

#endif // WebGraphicsLayer_H
