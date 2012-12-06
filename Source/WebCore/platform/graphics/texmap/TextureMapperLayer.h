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

#if USE(ACCELERATED_COMPOSITING)

#include "FilterOperations.h"
#include "FloatRect.h"
#include "GraphicsLayer.h"
#include "GraphicsLayerAnimation.h"
#include "GraphicsLayerTransform.h"
#include "TextureMapper.h"
#include "TextureMapperBackingStore.h"

namespace WebCore {

class TextureMapperPlatformLayer;
class GraphicsLayerTextureMapper;

class TextureMapperPaintOptions {
public:
    RefPtr<BitmapTexture> surface;
    RefPtr<BitmapTexture> mask;
    float opacity;
    TransformationMatrix transform;
    IntSize offset;
    TextureMapper* textureMapper;
    TextureMapperPaintOptions()
        : opacity(1)
        , textureMapper(0)
    { }
};

class TextureMapperLayer : public GraphicsLayerAnimation::Client {
    WTF_MAKE_FAST_ALLOCATED;
public:
    // This set of flags help us defer which properties of the layer have been
    // modified by the compositor, so we can know what to look for in the next flush.
    enum ChangeMask {
        NoChanges =                 0,

        ParentChange =              (1L << 0),
        ChildrenChange =            (1L << 1),
        MaskLayerChange =           (1L << 2),
        PositionChange =            (1L << 3),

        AnchorPointChange =         (1L << 4),
        SizeChange  =               (1L << 5),
        TransformChange =           (1L << 6),
        ContentChange =             (1L << 7),

        ContentsOrientationChange = (1L << 9),
        OpacityChange =             (1L << 10),
        ContentsRectChange =        (1L << 11),

        Preserves3DChange =         (1L << 12),
        MasksToBoundsChange =       (1L << 13),
        DrawsContentChange =        (1L << 14),
        ContentsVisibleChange =     (1L << 15),
        ContentsOpaqueChange =      (1L << 16),

        BackfaceVisibilityChange =  (1L << 17),
        ChildrenTransformChange =   (1L << 18),
        DisplayChange =             (1L << 19),
        BackgroundColorChange =     (1L << 20),

        ReplicaLayerChange =        (1L << 21),
        AnimationChange =           (1L << 22),
        FilterChange =              (1L << 23)
    };

    enum SyncOptions {
        TraverseDescendants = 1,
        ComputationsOnly = 2
    };

    TextureMapperLayer()
        : m_parent(0)
        , m_effectTarget(0)
        , m_contentsLayer(0)
        , m_opacity(1)
        , m_centerZ(0)
        , m_textureMapper(0)
    { }

    virtual ~TextureMapperLayer();

    TextureMapper* textureMapper() const;
    void flushCompositingState(GraphicsLayerTextureMapper*, int syncOptions = 0);
    void flushCompositingState(GraphicsLayerTextureMapper*, TextureMapper*, int syncOptions = 0);
    IntSize size() const { return IntSize(m_size.width(), m_size.height()); }
    void setTransform(const TransformationMatrix&);
    void setOpacity(float value) { m_opacity = value; }
#if ENABLE(CSS_FILTERS)
    void setFilters(const FilterOperations& filters) { m_filters = filters; }
#endif
    void setTextureMapper(TextureMapper* texmap) { m_textureMapper = texmap; }
    bool descendantsOrSelfHaveRunningAnimations() const;

    void paint();

    void setBackingStore(PassRefPtr<TextureMapperBackingStore> backingStore) { m_backingStore = backingStore; }
    PassRefPtr<TextureMapperBackingStore> backingStore() { return m_backingStore; }
    void clearBackingStoresRecursive();

    void setScrollPositionDeltaIfNeeded(const FloatSize&);

    void applyAnimationsRecursively();

private:
    const TextureMapperLayer* rootLayer() const;
    void computeTransformsRecursive();
    void computeOverlapsIfNeeded();
    void computeTiles();
    IntRect intermediateSurfaceRect(const TransformationMatrix&);
    IntRect intermediateSurfaceRect();
    void swapContentsBuffers();
    FloatRect targetRectForTileRect(const FloatRect& totalTargetRect, const FloatRect& tileRect) const;
    void invalidateViewport(const FloatRect&);
    void notifyChange(ChangeMask);
    void flushCompositingStateSelf(GraphicsLayerTextureMapper*, TextureMapper*);

    static int compareGraphicsLayersZValue(const void* a, const void* b);
    static void sortByZOrder(Vector<TextureMapperLayer* >& array, int first, int last);

    PassRefPtr<BitmapTexture> texture() { return m_backingStore ? m_backingStore->texture() : 0; }
    FloatPoint adjustedPosition() const { return m_state.pos + m_scrollPositionDelta; }
    bool isAncestorFixedToViewport() const;

    void paintRecursive(const TextureMapperPaintOptions&);
    void paintSelf(const TextureMapperPaintOptions&);
    void paintSelfAndChildren(const TextureMapperPaintOptions&);
    void paintSelfAndChildrenWithReplica(const TextureMapperPaintOptions&);

    // GraphicsLayerAnimation::Client
    void setAnimatedTransform(const TransformationMatrix& matrix) { setTransform(matrix); }
    void setAnimatedOpacity(float opacity) { setOpacity(opacity); }
#if ENABLE(CSS_FILTERS)
    virtual void setAnimatedFilters(const FilterOperations& filters) { setFilters(filters); }
#endif

    void syncAnimations();
    bool isVisible() const;
    enum ContentsLayerCount {
        NoLayersWithContent,
        SingleLayerWithContents,
        MultipleLayersWithContents
    };

    ContentsLayerCount countPotentialLayersWithContents() const;
    bool shouldPaintToIntermediateSurface() const;

    GraphicsLayerTransform m_transform;

    inline FloatRect layerRect() const
    {
        return FloatRect(FloatPoint::zero(), m_size);
    }

    Vector<TextureMapperLayer*> m_children;
    TextureMapperLayer* m_parent;
    TextureMapperLayer* m_effectTarget;
    RefPtr<TextureMapperBackingStore> m_backingStore;
    TextureMapperPlatformLayer* m_contentsLayer;
    FloatSize m_size;
    float m_opacity;
#if ENABLE(CSS_FILTERS)
    FilterOperations m_filters;
#endif
    float m_centerZ;
    String m_name;

    struct State {
        FloatPoint pos;
        FloatPoint3D anchorPoint;
        FloatSize size;
        TransformationMatrix transform;
        TransformationMatrix childrenTransform;
        float opacity;
        FloatRect contentsRect;
        int descendantsWithContent;
        TextureMapperLayer* maskLayer;
        TextureMapperLayer* replicaLayer;
        Color backgroundColor;
#if ENABLE(CSS_FILTERS)
         FilterOperations filters;
#endif

        bool preserves3D : 1;
        bool masksToBounds : 1;
        bool drawsContent : 1;
        bool contentsVisible : 1;
        bool contentsOpaque : 1;
        bool backfaceVisibility : 1;
        bool visible : 1;
        bool mightHaveOverlaps : 1;
        bool needsRepaint;
        State()
            : opacity(1.f)
            , maskLayer(0)
            , replicaLayer(0)
            , preserves3D(false)
            , masksToBounds(false)
            , drawsContent(false)
            , contentsVisible(true)
            , contentsOpaque(false)
            , backfaceVisibility(false)
            , visible(true)
            , mightHaveOverlaps(false)
            , needsRepaint(false)
        {
        }
    };

    State m_state;
    TextureMapper* m_textureMapper;
    GraphicsLayerAnimations m_animations;
    FloatSize m_scrollPositionDelta;
    bool m_fixedToViewport;
};


TextureMapperLayer* toTextureMapperLayer(GraphicsLayer*);

}
#endif

#endif // TextureMapperLayer_h
