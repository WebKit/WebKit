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

#ifndef TextureMapperLayer_h
#define TextureMapperLayer_h

#include "FilterOperations.h"
#include "FloatRect.h"
#include "NicosiaAnimation.h"
#include "TextureMapper.h"
#include "TextureMapperBackingStore.h"
#include <wtf/WeakPtr.h>

#if USE(COORDINATED_GRAPHICS)
#include "NicosiaAnimatedBackingStoreClient.h"
#endif

namespace WebCore {

class GraphicsLayer;
class Region;
class TextureMapperPaintOptions;
class TextureMapperPlatformLayer;

class WEBCORE_EXPORT TextureMapperLayer : public CanMakeWeakPtr<TextureMapperLayer> {
    WTF_MAKE_NONCOPYABLE(TextureMapperLayer);
    WTF_MAKE_FAST_ALLOCATED;
public:
    TextureMapperLayer();
    virtual ~TextureMapperLayer();

    void setID(uint32_t id) { m_id = id; }
    uint32_t id() { return m_id; }

    const Vector<TextureMapperLayer*>& children() const { return m_children; }

    TextureMapper* textureMapper() const { return rootLayer().m_textureMapper; }
    void setTextureMapper(TextureMapper* texmap) { m_textureMapper = texmap; }

#if !USE(COORDINATED_GRAPHICS)
    void setChildren(const Vector<GraphicsLayer*>&);
#endif
    void setChildren(const Vector<TextureMapperLayer*>&);
    void setMaskLayer(TextureMapperLayer*);
    void setReplicaLayer(TextureMapperLayer*);
    void setPosition(const FloatPoint&);
    void setSize(const FloatSize&);
    void setAnchorPoint(const FloatPoint3D&);
    void setPreserves3D(bool);
    void setTransform(const TransformationMatrix&);
    void setChildrenTransform(const TransformationMatrix&);
    void setContentsRect(const FloatRect&);
    void setMasksToBounds(bool);
    void setDrawsContent(bool);
    bool drawsContent() const { return m_state.drawsContent; }
    bool contentsAreVisible() const { return m_state.contentsVisible; }
    FloatSize size() const { return m_state.size; }
    float opacity() const { return m_state.opacity; }
    TransformationMatrix transform() const { return m_state.transform; }
    void setContentsVisible(bool);
    void setContentsOpaque(bool);
    void setBackfaceVisibility(bool);
    void setOpacity(float);
    void setSolidColor(const Color&);
    void setContentsTileSize(const FloatSize&);
    void setContentsTilePhase(const FloatSize&);
    void setFilters(const FilterOperations&);

    bool hasFilters() const
    {
        return !m_currentFilters.isEmpty();
    }

    void setDebugVisuals(bool showDebugBorders, const Color& debugBorderColor, float debugBorderWidth);
    void setRepaintCounter(bool showRepaintCounter, int repaintCount);
    void setContentsLayer(TextureMapperPlatformLayer*);
    void setAnimations(const Nicosia::Animations&);
    void setBackingStore(TextureMapperBackingStore*);
#if USE(COORDINATED_GRAPHICS)
    void setAnimatedBackingStoreClient(Nicosia::AnimatedBackingStoreClient*);
#endif

    bool applyAnimationsRecursively(MonotonicTime);
    bool syncAnimations(MonotonicTime);
    bool descendantsOrSelfHaveRunningAnimations() const;

    void paint();

    void addChild(TextureMapperLayer*);

private:
    const TextureMapperLayer& rootLayer() const
    {
        if (m_effectTarget)
            return m_effectTarget->rootLayer();
        if (m_parent)
            return m_parent->rootLayer();
        return *this;
    }
    void computeTransformsRecursive();

    static void sortByZOrder(Vector<TextureMapperLayer* >& array);

    TransformationMatrix replicaTransform();
    void removeFromParent();
    void removeAllChildren();

    enum ResolveSelfOverlapMode {
        ResolveSelfOverlapAlways = 0,
        ResolveSelfOverlapIfNeeded
    };
    void computeOverlapRegions(Region& overlapRegion, Region& nonOverlapRegion, ResolveSelfOverlapMode);

    void paintRecursive(const TextureMapperPaintOptions&);
    void paintUsingOverlapRegions(const TextureMapperPaintOptions&);
    RefPtr<BitmapTexture> paintIntoSurface(const TextureMapperPaintOptions&, const IntSize&);
    void paintWithIntermediateSurface(const TextureMapperPaintOptions&, const IntRect&);
    void paintSelf(const TextureMapperPaintOptions&);
    void paintSelfAndChildren(const TextureMapperPaintOptions&);
    void paintSelfAndChildrenWithReplica(const TextureMapperPaintOptions&);
    void applyMask(const TextureMapperPaintOptions&);

    bool isVisible() const;

    bool shouldBlend() const;

    inline FloatRect layerRect() const
    {
        return FloatRect(FloatPoint::zero(), m_state.size);
    }

    Vector<TextureMapperLayer*> m_children;
    TextureMapperLayer* m_parent { nullptr };
    WeakPtr<TextureMapperLayer> m_effectTarget;
    TextureMapperBackingStore* m_backingStore { nullptr };
    TextureMapperPlatformLayer* m_contentsLayer { nullptr };
    float m_currentOpacity { 1.0 };
    FilterOperations m_currentFilters;
    float m_centerZ { 0 };

    struct State {
        FloatPoint pos;
        FloatPoint3D anchorPoint;
        FloatSize size;
        TransformationMatrix transform;
        TransformationMatrix childrenTransform;
        float opacity;
        FloatRect contentsRect;
        FloatSize contentsTileSize;
        FloatSize contentsTilePhase;
        WeakPtr<TextureMapperLayer> maskLayer;
        WeakPtr<TextureMapperLayer> replicaLayer;
        Color solidColor;
        FilterOperations filters;
        Color debugBorderColor;
        float debugBorderWidth;
        int repaintCount;

        bool preserves3D : 1;
        bool masksToBounds : 1;
        bool drawsContent : 1;
        bool contentsVisible : 1;
        bool contentsOpaque : 1;
        bool backfaceVisibility : 1;
        bool visible : 1;
        bool showDebugBorders : 1;
        bool showRepaintCounter : 1;

        State()
            : anchorPoint(0.5, 0.5, 0)
            , opacity(1)
            , debugBorderWidth(0)
            , repaintCount(0)
            , preserves3D(false)
            , masksToBounds(false)
            , drawsContent(false)
            , contentsVisible(true)
            , contentsOpaque(false)
            , backfaceVisibility(true)
            , visible(true)
            , showDebugBorders(false)
            , showRepaintCounter(false)
        {
        }
    };

    State m_state;
    TextureMapper* m_textureMapper { nullptr };
    Nicosia::Animations m_animations;
    uint32_t m_id { 0 };
#if USE(COORDINATED_GRAPHICS)
    RefPtr<Nicosia::AnimatedBackingStoreClient> m_animatedBackingStoreClient;
#endif

    struct {
        TransformationMatrix localTransform;
        TransformationMatrix combined;
        TransformationMatrix combinedForChildren;
#if USE(COORDINATED_GRAPHICS)
        TransformationMatrix futureLocalTransform;
        TransformationMatrix futureCombined;
        TransformationMatrix futureCombinedForChildren;
#endif
    } m_layerTransforms;
};

}

#endif // TextureMapperLayer_h
