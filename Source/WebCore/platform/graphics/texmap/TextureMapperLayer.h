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

#pragma once

#include "FilterOperations.h"
#include "FloatRect.h"
#include "NicosiaAnimation.h"
#include "TextureMapper.h"
#include "TextureMapperSolidColorLayer.h"
#include <wtf/WeakPtr.h>

#if USE(COORDINATED_GRAPHICS)
#include "NicosiaAnimatedBackingStoreClient.h"
#endif

namespace WebCore {

class Region;
class TextureMapperPaintOptions;
class TextureMapperPlatformLayer;

class WEBCORE_EXPORT TextureMapperLayer : public CanMakeWeakPtr<TextureMapperLayer> {
    WTF_MAKE_NONCOPYABLE(TextureMapperLayer);
    WTF_MAKE_FAST_ALLOCATED;
public:
    TextureMapperLayer();
    virtual ~TextureMapperLayer();

#if USE(COORDINATED_GRAPHICS)
    void setID(uint32_t id) { m_id = id; }
    uint32_t id() { return m_id; }
#endif

    const Vector<TextureMapperLayer*>& children() const { return m_children; }

    void setChildren(const Vector<TextureMapperLayer*>&);
    void setMaskLayer(TextureMapperLayer*);
    void setReplicaLayer(TextureMapperLayer*);
    void setBackdropLayer(TextureMapperLayer*);
    void setBackdropFiltersRect(const FloatRoundedRect&);
    void setPosition(const FloatPoint&);
    void setBoundsOrigin(const FloatPoint&);
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
    void setBackgroundColor(const Color&);
    void setContentsTileSize(const FloatSize&);
    void setContentsTilePhase(const FloatSize&);
    void setContentsClippingRect(const FloatRoundedRect&);
    void setContentsRectClipsDescendants(bool);
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

    void paint(TextureMapper&);

    void addChild(TextureMapperLayer*);

private:
    TextureMapperLayer& rootLayer() const
    {
        if (m_effectTarget)
            return m_effectTarget->rootLayer();
        if (m_parent)
            return m_parent->rootLayer();
        return const_cast<TextureMapperLayer&>(*this);
    }

    struct ComputeTransformData;
    void computeTransformsRecursive(ComputeTransformData&);

    static void sortByZOrder(Vector<TextureMapperLayer* >& array);

    TransformationMatrix replicaTransform();
    void removeFromParent();
    void removeAllChildren();

    enum class ComputeOverlapRegionMode : uint8_t {
        Intersection,
        Union,
        Mask
    };
    struct ComputeOverlapRegionData {
        ComputeOverlapRegionMode mode;
        IntRect clipBounds;
        Region& overlapRegion;
        Region& nonOverlapRegion;
    };
    void computeOverlapRegions(ComputeOverlapRegionData&, const TransformationMatrix&, bool includesReplica = true);

    void paintRecursive(TextureMapperPaintOptions&);
    void paintWith3DRenderingContext(TextureMapperPaintOptions&);
    void paintSelfChildrenReplicaFilterAndMask(TextureMapperPaintOptions&);
    void paintUsingOverlapRegions(TextureMapperPaintOptions&);
    void paintIntoSurface(TextureMapperPaintOptions&);
    void paintWithIntermediateSurface(TextureMapperPaintOptions&, const IntRect&);
    void paintSelfAndChildrenWithIntermediateSurface(TextureMapperPaintOptions&, const IntRect&);
    void paintSelfChildrenFilterAndMask(TextureMapperPaintOptions&);
    void paintSelf(TextureMapperPaintOptions&);
    void paintSelfAndChildren(TextureMapperPaintOptions&);
    void paintSelfAndChildrenWithReplica(TextureMapperPaintOptions&);
    void applyMask(TextureMapperPaintOptions&);

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
        FloatPoint boundsOrigin;
        FloatSize size;
        TransformationMatrix transform;
        TransformationMatrix childrenTransform;
        float opacity;
        FloatRect contentsRect;
        FloatSize contentsTileSize;
        FloatSize contentsTilePhase;
        FloatRoundedRect contentsClippingRect;
        WeakPtr<TextureMapperLayer> maskLayer;
        WeakPtr<TextureMapperLayer> replicaLayer;
        WeakPtr<TextureMapperLayer> backdropLayer;
        FloatRoundedRect backdropFiltersRect;
        Color solidColor;
        Color backgroundColor;
        FilterOperations filters;
        Color debugBorderColor;
        float debugBorderWidth;
        int repaintCount;

        bool preserves3D : 1;
        bool masksToBounds : 1;
        bool drawsContent : 1;
        bool contentsVisible : 1;
        bool contentsOpaque : 1;
        bool contentsRectClipsDescendants : 1;
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
            , contentsRectClipsDescendants(false)
            , backfaceVisibility(true)
            , visible(true)
            , showDebugBorders(false)
            , showRepaintCounter(false)
        {
        }
    };

    State m_state;
    Nicosia::Animations m_animations;
#if USE(COORDINATED_GRAPHICS)
    uint32_t m_id { 0 };
    RefPtr<Nicosia::AnimatedBackingStoreClient> m_animatedBackingStoreClient;
#endif
    bool m_isBackdrop { false };
    bool m_isReplica { false };

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

} // namespace WebCore
