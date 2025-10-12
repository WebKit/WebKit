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

#include "Damage.h"
#include "FilterOperations.h"
#include "TextureMapperAnimation.h"
#include "TextureMapperSolidColorLayer.h"
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
class CoordinatedAnimatedBackingStoreClient;
class TextureMapperLayer;
}

namespace WTF {
template<typename T> struct IsDeprecatedWeakRefSmartPointerException;
template<> struct IsDeprecatedWeakRefSmartPointerException<WebCore::TextureMapperLayer> : std::true_type { };
}

namespace WebCore {

class TextureMapper;
class TextureMapperFlattenedLayer;
class TextureMapperPaintOptions;
class TextureMapperPlatformLayer;

class TextureMapperLayer : public CanMakeWeakPtr<TextureMapperLayer> {
    WTF_MAKE_TZONE_ALLOCATED(TextureMapperLayer);
    WTF_MAKE_NONCOPYABLE(TextureMapperLayer);
public:
    WEBCORE_EXPORT TextureMapperLayer();
    WEBCORE_EXPORT virtual ~TextureMapperLayer();

#if USE(COORDINATED_GRAPHICS)
    void setID(uint32_t id) { m_id = id; }
    uint32_t id() { return m_id; }
#endif

    const Vector<TextureMapperLayer*>& children() const { return m_children; }

    WEBCORE_EXPORT void setChildren(const Vector<TextureMapperLayer*>&);
    WEBCORE_EXPORT void setMaskLayer(TextureMapperLayer*);
    WEBCORE_EXPORT void setReplicaLayer(TextureMapperLayer*);
    WEBCORE_EXPORT void setBackdropLayer(TextureMapperLayer*);
    WEBCORE_EXPORT void setBackdropFiltersRect(const FloatRoundedRect&);
    WEBCORE_EXPORT void setPosition(const FloatPoint&);
    WEBCORE_EXPORT void setBoundsOrigin(const FloatPoint&);
    WEBCORE_EXPORT void setSize(const FloatSize&);
    WEBCORE_EXPORT void setAnchorPoint(const FloatPoint3D&);
    WEBCORE_EXPORT void setPreserves3D(bool);
    WEBCORE_EXPORT void setTransform(const TransformationMatrix&);
    WEBCORE_EXPORT void setChildrenTransform(const TransformationMatrix&);
    WEBCORE_EXPORT void setContentsRect(const FloatRect&);
    WEBCORE_EXPORT void setMasksToBounds(bool);
    WEBCORE_EXPORT void setDrawsContent(bool);
    bool drawsContent() const { return m_state.drawsContent; }
    bool contentsAreVisible() const { return m_state.contentsVisible; }
    FloatSize size() const { return m_state.size; }
    float opacity() const { return m_state.opacity; }
    TransformationMatrix transform() const { return m_state.transform; }
    const TransformationMatrix& toSurfaceTransform() const { return m_layerTransforms.combined; }
    WEBCORE_EXPORT void setContentsVisible(bool);
    WEBCORE_EXPORT void setContentsOpaque(bool);
    WEBCORE_EXPORT void setBackfaceVisibility(bool);
    WEBCORE_EXPORT void setOpacity(float);
    WEBCORE_EXPORT void setSolidColor(const Color&);
    WEBCORE_EXPORT void setBackgroundColor(const Color&);
    WEBCORE_EXPORT void setContentsTileSize(const FloatSize&);
    WEBCORE_EXPORT void setContentsTilePhase(const FloatSize&);
    WEBCORE_EXPORT void setContentsClippingRect(const FloatRoundedRect&);
    WEBCORE_EXPORT void setContentsRectClipsDescendants(bool);
    WEBCORE_EXPORT void setFilters(const FilterOperations&);

    bool hasFilters() const
    {
        return !m_currentFilters.isEmpty();
    }

    void setShowDebugBorder(bool showDebugBorder) { m_state.showDebugBorders = showDebugBorder; }
    void setDebugBorderColor(Color debugBorderColor) { m_state.debugBorderColor = debugBorderColor; }
    void setDebugBorderWidth(float debugBorderWidth) { m_state.debugBorderWidth = debugBorderWidth; }

    void setShowRepaintCounter(bool showRepaintCounter) { m_state.showRepaintCounter = showRepaintCounter; }
    void setRepaintCount(int repaintCount) { m_state.repaintCount = repaintCount; }

    WEBCORE_EXPORT void setContentsLayer(TextureMapperPlatformLayer*);
    void setAnimations(const TextureMapperAnimations&);
    WEBCORE_EXPORT void setBackingStore(TextureMapperBackingStore*);
#if USE(COORDINATED_GRAPHICS)
    void setAnimatedBackingStoreClient(CoordinatedAnimatedBackingStoreClient*);
#endif

    WEBCORE_EXPORT bool applyAnimationsRecursively(MonotonicTime);
    bool syncAnimations(MonotonicTime);
    WEBCORE_EXPORT bool descendantsOrSelfHaveRunningAnimations() const;

    WEBCORE_EXPORT void prepareForPainting(TextureMapper&);
    WEBCORE_EXPORT void paint(TextureMapper&);

    void addChild(TextureMapperLayer*);

#if ENABLE(DAMAGE_TRACKING)
    void setDamagePropagationEnabled(bool enabled) { m_damagePropagationEnabled = enabled; }
    void setDamageInGlobalCoordinateSpace(std::shared_ptr<Damage> damage) { m_damageInGlobalCoordinateSpace = WTFMove(damage); }
    void setDamage(Damage&&);
    void collectDamage(TextureMapper&, Damage&);
#endif

    FloatRect effectiveLayerRect() const;

private:
    TextureMapperLayer& backdropRootLayer() const
    {
        if (m_effectTarget)
            return m_effectTarget->backdropRootLayer();
        if (m_parent) {
            if (m_parent->flattensAsLeafOf3DSceneOr3DPerspective()
                || m_parent->m_state.opacity < 1
                || m_parent->hasMask()
                || m_parent->hasFilters()) {
                return *m_parent;
            }

            return m_parent->backdropRootLayer();
        }
        return const_cast<TextureMapperLayer&>(*this);
    }

    void processDescendantLayersFlatteningRequirements();
    void processFlatteningRequirements();
    void computeFlattenedRegion(Region&, bool) const;
    void destroyFlattenedDescendantLayers();

    struct ComputeTransformData;
    void computeTransformsRecursive(ComputeTransformData&);

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
    Vector<IntRect, 1> computeConsolidatedOverlapRegionRects(TextureMapperPaintOptions&);

    void paintRecursive(TextureMapperPaintOptions&);
    void paintFlattened(TextureMapperPaintOptions&);
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
    void paintBackdrop(TextureMapperPaintOptions&);
    void applyMask(TextureMapperPaintOptions&);
    void collect3DRenderingContextLayers(Vector<TextureMapperLayer*>&);

#if ENABLE(DAMAGE_TRACKING)
    void collectDamageRecursive(TextureMapperPaintOptions&, Damage&);
    void collectDamageSelfAndChildren(TextureMapperPaintOptions&, Damage&);
    void collectDamageSelf(TextureMapperPaintOptions&, Damage&);
    void collectDamageSelfChildrenReplicaFilterAndMask(TextureMapperPaintOptions&, Damage&);
    void collectDamageSelfChildrenFilterAndMask(TextureMapperPaintOptions&, Damage&);
    void collectDamageFromLayerAboutToBeRemoved(TextureMapperLayer&);
    ALWAYS_INLINE Damage& ensureDamageInLayerCoordinateSpace();
    inline void damageWholeLayer();
    void damageWholeLayerIncludingItsRectFromPreviousFrame();
#endif

    bool isVisible() const;

    bool shouldBlend() const;

    bool flattensAsLeafOf3DSceneOr3DPerspective() const;

    bool preserves3D() const { return m_state.preserves3D; }
    bool isLeafOf3DRenderingContext() const { return !m_state.preserves3D && (m_parent && m_parent->preserves3D()); }
    bool isFlattened() const { return !!m_flattenedLayer; }
    bool hasMask() const { return !!m_state.maskLayer; }
    bool hasBackdrop() const  { return !!m_state.backdropLayer; }

    inline FloatRect layerRect() const
    {
        return FloatRect(FloatPoint::zero(), m_state.size);
    }

    Vector<TextureMapperLayer*> m_children;
    TextureMapperLayer* m_parent { nullptr };
    WeakPtr<TextureMapperLayer> m_effectTarget;
    TextureMapperBackingStore* m_backingStore { nullptr };
    TextureMapperPlatformLayer* m_contentsLayer { nullptr };
    std::unique_ptr<TextureMapperFlattenedLayer> m_flattenedLayer;
    float m_currentOpacity { 1.0 };
    FilterOperations m_currentFilters;

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
    TextureMapperAnimations m_animations;
#if USE(COORDINATED_GRAPHICS)
    uint32_t m_id { 0 };
    RefPtr<CoordinatedAnimatedBackingStoreClient> m_animatedBackingStoreClient;
#endif
    bool m_isBackdrop { false };
    bool m_isReplica { false };

#if ENABLE(DAMAGE_TRACKING)
    bool m_damagePropagationEnabled { false };
    bool m_collectDamageDespiteBeingInvisible { false };
    std::optional<Damage> m_damageInLayerCoordinateSpace;
    std::shared_ptr<Damage> m_damageInGlobalCoordinateSpace;
    FloatRect m_accumulatedOverlapRegionDamage;
    std::optional<FloatRect> m_previousLayerRectInGlobalCoordinateSpace;
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

} // namespace WebCore
