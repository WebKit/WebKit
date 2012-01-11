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

#ifndef TextureMapperNode_h
#define TextureMapperNode_h

#include "FloatRect.h"
#include "GraphicsContext.h"
#include "GraphicsLayer.h"
#include "Image.h"
#include "IntPointHash.h"
#include "TextureMapper.h"
#include "Timer.h"
#include "TransformOperations.h"
#include "TranslateTransformOperation.h"
#include "UnitBezier.h"
#include <wtf/CurrentTime.h>
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class TextureMapperPlatformLayer;
class TextureMapperNode;
class GraphicsLayerTextureMapper;

class TextureMapperPaintOptions {
public:
    BitmapTexture* surface;
    TextureMapper* textureMapper;

    float opacity;
    bool isSurface;
    TextureMapperPaintOptions()
        : surface(0)
        , textureMapper(0)
        , opacity(1)
        , isSurface(false)
    { }
};

class TextureMapperAnimation : public RefCounted<TextureMapperAnimation> {
public:
    String name;
    KeyframeValueList keyframes;
    IntSize boxSize;
    RefPtr<Animation> animation;
    bool paused;
    Vector<TransformOperation::OperationType> functionList;
    bool listsMatch;
    bool hasBigRotation;
    double startTime;
    TextureMapperAnimation(const KeyframeValueList&);
    static PassRefPtr<TextureMapperAnimation> create(const KeyframeValueList& values) { return adoptRef(new TextureMapperAnimation(values)); }
};

class TextureMapperNode {

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
        ContentsOpaqueChange =      (1L << 15),

        BackfaceVisibilityChange =  (1L << 16),
        ChildrenTransformChange =   (1L << 17),
        DisplayChange =             (1L << 18),
        BackgroundColorChange =     (1L << 19),

        ReplicaLayerChange =        (1L << 20),
        AnimationChange =           (1L << 21)
    };

    enum SyncOptions {
        TraverseDescendants = 1,
        ComputationsOnly = 2
    };

    enum TileOwnership {
        OwnedTiles,
        ExternallyManagedTiles
    };

    typedef HashMap<TextureMapperNode*, FloatRect> NodeRectMap;

    // The compositor lets us special-case images and colors, so we try to do so.
    enum ContentType { HTMLContentType, DirectImageContentType, ColorContentType, MediaContentType, Canvas3DContentType};
    struct ContentData {
        FloatRect needsDisplayRect;
        bool needsDisplay;
        Color backgroundColor;

        ContentType contentType;
        RefPtr<Image> image;
        const TextureMapperPlatformLayer* media;
        ContentData()
            : needsDisplay(false)
            , contentType(HTMLContentType)
            , image(0)
            , media(0)
        {
        }
    };

    TextureMapperNode()
        : m_parent(0)
        , m_effectTarget(0)
        , m_opacity(1)
        , m_textureMapper(0)
    { }

    virtual ~TextureMapperNode();

    void syncCompositingState(GraphicsLayerTextureMapper*, int syncOptions = 0);
    void syncCompositingState(GraphicsLayerTextureMapper*, TextureMapper*, int syncOptions = 0);
    void syncAnimationsRecursively();
    IntSize size() const { return IntSize(m_size.width(), m_size.height()); }
    void setTransform(const TransformationMatrix&);
    void setOpacity(float value) { m_opacity = value; }
    void setTextureMapper(TextureMapper* texmap) { m_textureMapper = texmap; }
    bool descendantsOrSelfHaveRunningAnimations() const;

    void paint();

#if USE(TILED_BACKING_STORE)
    void setTileOwnership(TileOwnership ownership) { m_state.tileOwnership = ownership; }
    int createContentsTile(float scale);
    void removeContentsTile(int id);
    void setContentsTileBackBuffer(int id, const IntRect& sourceRect, const IntRect& targetRect, void* bits, BitmapTexture::PixelFormat);
    void setTileBackBufferTextureForDirectlyCompositedImage(int id, const IntRect& sourceRect, const FloatRect& targetRect, BitmapTexture*);
    void clearAllDirectlyCompositedImageTiles();
    bool collectVisibleContentsRects(NodeRectMap&, const FloatRect&);
    void purgeNodeTexturesRecursive();
#endif
    void setID(int id) { m_id = id; }
    int id() const { return m_id; }

    const TextureMapperPlatformLayer* media() const { return m_currentContent.media; }

private:
    TextureMapperNode* rootLayer();
    void computeAllTransforms();
    void computeVisibleRect(const FloatRect& rootVisibleRect);
    void computePerspectiveTransformIfNeeded();
    void computeReplicaTransformIfNeeded();
    void computeOverlapsIfNeeded();
    void computeLocalTransformIfNeeded();
    void computeTiles();
    void swapContentsBuffers();
    int countDescendantsWithContent() const;
    FloatRect targetRectForTileRect(const FloatRect& totalTargetRect, const FloatRect& tileRect) const;
    void invalidateViewport(const FloatRect&);
    void notifyChange(ChangeMask);
    void syncCompositingStateSelf(GraphicsLayerTextureMapper* graphicsLayer, TextureMapper* textureMapper);

    static int compareGraphicsLayersZValue(const void* a, const void* b);
    static void sortByZOrder(Vector<TextureMapperNode* >& array, int first, int last);

    BitmapTexture* texture() { return m_ownedTiles.isEmpty() ? 0 : m_ownedTiles[0].texture.get(); }

    void paintRecursive(TextureMapperPaintOptions);
    bool paintReflection(const TextureMapperPaintOptions&, BitmapTexture* surface);
    void paintSelf(const TextureMapperPaintOptions&);
    void paintSelfAndChildren(const TextureMapperPaintOptions&, TextureMapperPaintOptions& optionsForDescendants);
    void renderContent(TextureMapper*, GraphicsLayer*);

    void syncAnimations(GraphicsLayerTextureMapper*);
    void applyAnimation(const TextureMapperAnimation&, double runningTime);
    void applyAnimationFrame(const TextureMapperAnimation&, const AnimationValue* from, const AnimationValue* to, float progress);
    void applyOpacityAnimation(float fromOpacity, float toOpacity, double);
    void applyTransformAnimation(const TextureMapperAnimation&, const TransformOperations* start, const TransformOperations* end, double);
    bool hasOpacityAnimation() const;
    bool hasTransformAnimation() const;
    bool hasMoreThanOneTile() const;
    struct TransformData {
        TransformationMatrix target;
        TransformationMatrix replica;
        TransformationMatrix forDescendants;
        TransformationMatrix local;
        TransformationMatrix base;
        TransformationMatrix perspective;
        float centerZ;
        TransformData() { }
    };

    TransformData m_transforms;

    inline FloatRect targetRect() const
    {
        return m_currentContent.contentType == HTMLContentType ? entireRect() : m_state.contentsRect;
    }

    inline FloatRect entireRect() const
    {
        return FloatRect(0, 0, m_size.width(), m_size.height());
    }

    FloatSize contentSize() const
    {
        return m_currentContent.contentType == DirectImageContentType && m_currentContent.image ? m_currentContent.image->size() : m_size;
    }
    struct OwnedTile {
        FloatRect rect;
        RefPtr<BitmapTexture> texture;
        bool needsReset;
    };

    Vector<OwnedTile> m_ownedTiles;

#if USE(TILED_BACKING_STORE)
    struct ExternallyManagedTileBuffer {
        FloatRect sourceRect;
        FloatRect targetRect;
        RefPtr<BitmapTexture> texture;
    };

    struct ExternallyManagedTile {
        bool isBackBufferUpdated;
        bool isDirectlyCompositedImage;
        float scale;
        ExternallyManagedTileBuffer frontBuffer;
        ExternallyManagedTileBuffer backBuffer;

        ExternallyManagedTile(float scale = 1.0)
            : isBackBufferUpdated(false)
            , isDirectlyCompositedImage(false)
            , scale(scale)
        {
        }
    };

    HashMap<int, ExternallyManagedTile> m_externallyManagedTiles;
#endif

    ContentData m_currentContent;

    Vector<TextureMapperNode*> m_children;
    TextureMapperNode* m_parent;
    TextureMapperNode* m_effectTarget;
    FloatSize m_size;
    float m_opacity;
    String m_name;
    int m_id;

    struct State {
        FloatPoint pos;
        FloatPoint3D anchorPoint;
        FloatSize size;
        TransformationMatrix transform;
        TransformationMatrix childrenTransform;
        float opacity;
        FloatRect contentsRect;
        int descendantsWithContent;
        TextureMapperNode* maskLayer;
        TextureMapperNode* replicaLayer;
        bool preserves3D : 1;
        bool masksToBounds : 1;
        bool drawsContent : 1;
        bool contentsOpaque : 1;
        bool backfaceVisibility : 1;
        bool visible : 1;
        bool needsReset: 1;
        bool mightHaveOverlaps : 1;
        bool needsRepaint;
        IntRect visibleRect;
        float contentScale;
#if USE(TILED_BACKING_STORE)
        TileOwnership tileOwnership;
#endif
        State()
            : opacity(1.f)
            , maskLayer(0)
            , replicaLayer(0)
            , preserves3D(false)
            , masksToBounds(false)
            , drawsContent(false)
            , contentsOpaque(false)
            , backfaceVisibility(false)
            , visible(true)
            , needsReset(false)
            , mightHaveOverlaps(false)
            , needsRepaint(false)
            , contentScale(1.0f)
#if USE(TILED_BACKING_STORE)
            , tileOwnership(OwnedTiles)
#endif
        {
        }
    };

    State m_state;
    TextureMapper* m_textureMapper;

    Vector<RefPtr<TextureMapperAnimation> > m_animations;
};


TextureMapperNode* toTextureMapperNode(GraphicsLayer*);

}
#endif // TextureMapperNode_h
