/*
    Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)

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

#ifndef GraphicsLayerTextureMapper_h
#define GraphicsLayerTextureMapper_h

#include "GraphicsContext.h"
#include "GraphicsLayer.h"
#include "GraphicsLayerClient.h"
#include "Image.h"
#include "TextureMapperLayer.h"
#include "Timer.h"

namespace WebCore {

class GraphicsLayerTextureMapper : public GraphicsLayer {
    friend class TextureMapperLayer;

public:
    explicit GraphicsLayerTextureMapper(GraphicsLayerClient*);
    virtual ~GraphicsLayerTextureMapper();

    // reimps from GraphicsLayer.h
    virtual void setNeedsDisplay();
    virtual void setContentsNeedsDisplay();
    virtual void setNeedsDisplayInRect(const FloatRect&);
    virtual void setParent(GraphicsLayer* layer);
    virtual bool setChildren(const Vector<GraphicsLayer*>&);
    virtual void addChild(GraphicsLayer*);
    virtual void addChildAtIndex(GraphicsLayer*, int index);
    virtual void addChildAbove(GraphicsLayer* layer, GraphicsLayer* sibling);
    virtual void addChildBelow(GraphicsLayer* layer, GraphicsLayer* sibling);
    virtual bool replaceChild(GraphicsLayer* oldChild, GraphicsLayer* newChild);
    virtual void removeFromParent();
    virtual void setMaskLayer(GraphicsLayer* layer);
    virtual void setPosition(const FloatPoint& p);
    virtual void setAnchorPoint(const FloatPoint3D& p);
    virtual void setSize(const FloatSize& size);
    virtual void setTransform(const TransformationMatrix& t);
    virtual void setChildrenTransform(const TransformationMatrix& t);
    virtual void setPreserves3D(bool b);
    virtual void setMasksToBounds(bool b);
    virtual void setDrawsContent(bool b);
    virtual void setContentsVisible(bool);
    virtual void setContentsOpaque(bool b);
    virtual void setBackfaceVisibility(bool b);
    virtual void setOpacity(float opacity);
    virtual void setContentsRect(const IntRect& r);
    virtual void setReplicatedByLayer(GraphicsLayer*);
    virtual void setContentsToImage(Image*);
    virtual void setContentsToSolidColor(const Color&);
    Color solidColor() const { return m_solidColor; }
    virtual void setContentsToMedia(PlatformLayer*);
    virtual void setContentsToCanvas(PlatformLayer* canvas) { setContentsToMedia(canvas); }
    virtual void flushCompositingState(const FloatRect&);
    virtual void flushCompositingStateForThisLayerOnly();
    virtual void setName(const String& name);
    virtual PlatformLayer* platformLayer() const { return m_contentsLayer; }

    void notifyChange(TextureMapperLayer::ChangeMask);
    inline int changeMask() const { return m_changeMask; }

    virtual bool addAnimation(const KeyframeValueList&, const IntSize&, const Animation*, const String&, double);
    virtual void pauseAnimation(const String&, double);
    virtual void removeAnimation(const String&);
    void setAnimations(const GraphicsLayerAnimations&);

    TextureMapperLayer* layer() const { return m_layer.get(); }

    virtual void setDebugBorder(const Color&, float width);

#if ENABLE(CSS_FILTERS)
    virtual bool setFilters(const FilterOperations&);
#endif

    // FIXME: It will be removed after removing dependency of LayerTreeRenderer on GraphicsLayerTextureMapper.
    void setHasOwnBackingStore(bool b) { m_hasOwnBackingStore = b; }

    void setFixedToViewport(bool fixed) { m_fixedToViewport = fixed; }
    bool fixedToViewport() const { return m_fixedToViewport; }

    void drawRepaintCounter(GraphicsContext*);
private:
    virtual void willBeDestroyed();
    void didFlushCompositingState();
    void updateBackingStore();
    void prepareBackingStore();
    bool shouldHaveBackingStore() const;
    void animationStartedTimerFired(Timer<GraphicsLayerTextureMapper>*);

    OwnPtr<TextureMapperLayer> m_layer;
    RefPtr<TextureMapperTiledBackingStore> m_compositedImage;
    NativeImagePtr m_compositedNativeImagePtr;
    RefPtr<TextureMapperBackingStore> m_backingStore;

    int m_changeMask;
    bool m_needsDisplay;
    bool m_hasOwnBackingStore;
    bool m_fixedToViewport;
    Color m_solidColor;

    Color m_debugBorderColor;
    float m_debugBorderWidth;

    TextureMapperPlatformLayer* m_contentsLayer;
    FloatRect m_needsDisplayRect;
    GraphicsLayerAnimations m_animations;
    Timer<GraphicsLayerTextureMapper> m_animationStartedTimer;
};

inline static GraphicsLayerTextureMapper* toGraphicsLayerTextureMapper(GraphicsLayer* layer)
{
    return static_cast<GraphicsLayerTextureMapper*>(layer);
}

}
#endif // GraphicsLayerTextureMapper_h
