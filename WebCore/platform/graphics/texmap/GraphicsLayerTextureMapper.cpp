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

#include "config.h"
#include "GraphicsLayerTextureMapper.h"

#include "CurrentTime.h"
#include "FloatRect.h"
#include "GraphicsContext.h"
#include "HashMap.h"
#include "Image.h"
#include "RefCounted.h"
#include "TextureMapper.h"
#include "TextureMapperPlatformLayer.h"
#include "Timer.h"
#include "TransformOperations.h"
#include "TranslateTransformOperation.h"
#include "UnitBezier.h"

#define DEBUG_TEXMAP_FPS 0

namespace WebCore {

struct TexmapPaintOptions {
    BitmapTexture* surface;
    TextureMapper* textureMapper;
    GraphicsContext* context;
    TextureMapperNode* rootLayer;
    float opacity;
    IntRect scissorRect;
    IntRect visibleRect;
    bool isSurface;
};
class TextureMapperCache {
public:
    void mark(BitmapTexture* texture);

    class Entry {
    public:
        RefPtr<BitmapTexture> texture;
        Entry() : previousCost(0) { }
        inline int calculateCost() const
        {
            if (!texture || !texture->isValid())
                return 0;
            const IntSize textureSize = texture->size();
            // an image's cost in bytes is width * height * bytes per pixel (4).
            return textureSize.width() * textureSize.height() * 4;
        }
        Entry(BitmapTexture* newTexture)
            : texture(newTexture)
        {
        }
        bool operator==(const Entry& other) const { return texture == other.texture; }
        int previousCost;
    };

    TextureMapperCache() : m_totalCost(0) {}

    void purge();
    Vector<Entry> m_data;
    int m_totalCost;
#ifndef TEXMAP_TEXTURE_CACHE_KBS
#define TEXMAP_TEXTURE_CACHE_KBS 24 * 1024
#endif
    static const int MaxCost = TEXMAP_TEXTURE_CACHE_KBS * 1024;
    static const int PurgeAmount = MaxCost / 4;
};


void TextureMapperCache::purge()
{
    // If this is in the GL implementation, we need an active GL context, because we might call glDeleteTextures.
    const int size = m_data.size();

    if (m_totalCost <= TextureMapperCache::MaxCost)
        return;

    // Ensure that we have the right count. It might be inaccurate if content changed size.
    // We only do this when we're actually ready to purge.
    m_totalCost = 0;
    for (int i = 0; i < size; ++i)
        m_totalCost += m_data[i].calculateCost();

    for (int i = size-1; i >= 0 && m_totalCost > TextureMapperCache::MaxCost - TextureMapperCache::PurgeAmount; --i) {
        Entry& entry = m_data[i];
        if (entry.texture->isLocked() || !entry.texture->isValid())
            continue;
        m_totalCost -= entry.previousCost;
        entry.texture->destroy();
        m_data.remove(i);
    }
}

void TextureMapperCache::mark(BitmapTexture* texture)
{
    if (!texture || !texture->isValid())
        return;

    Entry entry(texture);
    size_t index = m_data.find(entry);
    if (!index)
        return;

    if (index < m_data.size()) 
        m_data.remove(index);
    const int cost = entry.calculateCost();
    m_totalCost -= entry.previousCost;
    m_totalCost += (entry.previousCost = cost);
    m_data.prepend(entry);
}

TextureMapperCache gTextureMapperCache;

class TextureMapperCacheLock {
public:
    TextureMapperCacheLock(BitmapTexture* texture) : m_texture(texture)
    {
        if (m_texture)
            m_texture->lock();
    }
    ~TextureMapperCacheLock()
    {
        if (m_texture)
            m_texture->unlock();
    }

private:
    RefPtr<BitmapTexture> m_texture;
};

class TextureMapperNode : public TextureMapperContentLayer {

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

        ReplicaLayerChange =        (1L << 20)
    };

    // The compositor lets us special-case images and colors, so we try to do so.
    enum StaticContentType { HTMLContentType, DirectImageContentType, ColorContentType, MediaContentType, Canvas3DContentType};

    TextureMapperNode* rootLayer();

    TextureMapperNode(GraphicsLayerTextureMapper* newLayer);
    virtual ~TextureMapperNode();

    void clearDirectImage();
    void computeTransformations();
    IntSize nearestSurfaceSize() const;
    void computeReplicaTransform();
    void computeLayerType();
    void computeLocalTransform();
    void flattenTo2DSpaceIfNecessary();
    void initializeTextureMapper(TextureMapper*);
    void invalidateTransform();
    void notifyChange(ChangeMask);
    void syncCompositingState(bool recurse);
    void performPostSyncOperations();
    void setNeedsDisplay();
    void setNeedsDisplayInRect(IntRect);
    virtual void cleanupTextureMapper();

    void paintRecursive(TexmapPaintOptions options);
    void paintSelf(const TexmapPaintOptions& options);
    void uploadTextureFromContent(TextureMapper* textureMapper, const IntRect& visibleRect);

    int countDescendantsWithContent() const;
    bool hasSurfaceDescendants() const;

    IntSize size() const { return m_size; }

    virtual void setPlatformLayerClient(TextureMapperLayerClient*);
    virtual void paint(GraphicsContext*, const IntSize&, const IntRect& targetRect, const IntRect& exposedRect, const TransformationMatrix& transform, float opacity);

    static TextureMapperNode* toTextureMapperNode(GraphicsLayer*);
public:
    GraphicsLayerTextureMapper* m_layer;
    const char* m_lastTextureMapperType;
    RefPtr<TextureMapper> m_lastTextureMapper;
    struct TransformData {
        TransformationMatrix base, target, replica, forDescendants, perspective, local;
        IntRect targetBoundingRect;
        float centerZ;
        bool dirty, localDirty, perspectiveDirty;
        IntRect boundingRectFromRoot;
        TransformData() : dirty(true), localDirty(true), perspectiveDirty(true) { }
    };

    TransformData m_transforms;

    enum LayerType {
        DefaultLayer,
        RootLayer,
        ScissorLayer,
        ClipLayer,
        TransparencyLayer
    };

    LayerType m_layerType;

    struct ContentData {
        IntRect needsDisplayRect;
        bool needsDisplay;
        Color backgroundColor;

        StaticContentType contentType;
        RefPtr<Image> image;
        TextureMapperVideoLayer* media;

        ContentData()
            : needsDisplay(false)
            , contentType(HTMLContentType)
            , image(0)
            , media(0)
        {
        }

    };

    inline IntRect targetRect() const
    {
        return m_currentContent.contentType == HTMLContentType ? entireRect() : m_state.contentsRect;
    }

    inline IntRect entireRect() const
    {
        return IntRect(0, 0, m_size.width(), m_size.height());
    }

    inline IntRect replicaRect() const
    {
        return m_layerType == TransparencyLayer ? IntRect(0, 0, m_nearestSurfaceSize.width(), m_nearestSurfaceSize.height()) : entireRect();
    }

    RefPtr<BitmapTexture> m_texture;
    RefPtr<BitmapTexture> m_surface, m_replicaSurface;

    ContentData m_pendingContent;
    ContentData m_currentContent;

    Vector<TextureMapperNode*> m_children;
    TextureMapperNode* m_parent;
    TextureMapperNode* m_effectTarget;
    int m_changeMask;
    IntSize m_size, m_nearestSurfaceSize;
    String m_name;
    TextureMapperLayerClient* m_platformClient;

    struct State {
        FloatPoint pos;
        FloatPoint3D anchorPoint;
        FloatSize size;
        TransformationMatrix transform;
        TransformationMatrix childrenTransform;
        Color backgroundColor;
        Color currentColor;
        GraphicsLayer::CompositingCoordinatesOrientation geoOrientation;
        GraphicsLayer::CompositingCoordinatesOrientation contentsOrientation;
        float opacity;
        IntRect contentsRect;
        int descendantsWithContent;
        TextureMapperNode* maskLayer;
        TextureMapperNode* replicaLayer;
        bool preserves3D;
        bool masksToBounds;
        bool drawsContent;
        bool contentsOpaque;
        bool backfaceVisibility;
        bool visible;
        bool dirty;
        bool tiled;
        bool hasSurfaceDescendants;

        State()
            : opacity(1.f)
            , descendantsWithContent(0)
            , maskLayer(0)
            , replicaLayer(0)
            , preserves3D(false)
            , masksToBounds(false)
            , drawsContent(false)
            , contentsOpaque(false)
            , backfaceVisibility(false)
            , visible(true)
            , dirty(true)
            , tiled(false)
            , hasSurfaceDescendants(false)
        {
        }
    };
    State m_state;
};

void TextureMapperNode::setNeedsDisplayInRect(IntRect rect)
{
    if (m_platformClient) {
        if (m_state.hasSurfaceDescendants) {
            m_platformClient->setNeedsDisplay();
            return;
        }
        rect.intersect(IntRect(0, 0, m_size.width(), m_size.height()));
        if (rect.isEmpty())
            return;
        m_platformClient->setNeedsDisplayInRect(rect);
        return;
    }

    if (!m_parent)
        return;

    m_parent->setNeedsDisplayInRect(rect);
}

void TextureMapperNode::setNeedsDisplay()
{
    if (m_effectTarget)
        m_effectTarget->setNeedsDisplay();
    if (m_transforms.targetBoundingRect.isEmpty())
        return;
    if (m_state.drawsContent || m_currentContent.contentType != HTMLContentType)
        setNeedsDisplayInRect(m_transforms.targetBoundingRect);
}


void TextureMapperNode::setPlatformLayerClient(TextureMapperLayerClient* client)
{
    m_platformClient = client;
}

static int compareGraphicsLayersZValue(const void* a, const void* b)
{
    typedef const TextureMapperNode* NodePtr;
    const NodePtr* nodeA = static_cast<const NodePtr*>(a);
    const NodePtr* nodeB = static_cast<const NodePtr*>(b);
    return int(((*nodeA)->m_transforms.centerZ - (*nodeB)->m_transforms.centerZ) * 1000);
}
inline static void sortByZOrder(Vector<TextureMapperNode* >& array, int first, int last)
{
    qsort(array.data(), array.size(), sizeof(TextureMapperNode*), compareGraphicsLayersZValue);
}

bool TextureMapperNode::hasSurfaceDescendants() const
{
    if (m_layerType == ClipLayer || m_layerType == TransparencyLayer || m_state.replicaLayer)
        return true;
    const int size = m_children.size();
    for (int i = 0; i < size; ++i) {
        if (TextureMapperNode* child = m_children[i]) {
            if (child->hasSurfaceDescendants())
                return true;
        }
    }
    return false;

}

void TextureMapperNode::paint(GraphicsContext* context, const IntSize& size, const IntRect& targetRect, const IntRect& exposedRect, const TransformationMatrix& transform, float opacity)
{
    ASSERT(m_layerType == RootLayer);
    if (m_size.isEmpty())
        return;

#if 0
    WTF::StopWatch stopWatch;
    ("[TextureMapper] RootPaint!!\n");
#endif

    RefPtr<TextureMapper> textureMapper = TextureMapper::create(context);

    if (textureMapper->type() != m_lastTextureMapperType)
        gTextureMapperCache.m_data.clear();

    m_lastTextureMapper = textureMapper;
    TexmapPaintOptions opt;
    opt.opacity = 1;
    opt.rootLayer = this;
    opt.scissorRect = targetRect;
    opt.visibleRect = exposedRect;
    opt.textureMapper = textureMapper.get();
    opt.context = textureMapper->graphicsContext();
    opt.surface = 0;
    paintRecursive(opt);

    if (textureMapper->allowSurfaceForRoot() || m_state.hasSurfaceDescendants) {
        textureMapper->bindSurface(0);
        textureMapper->paintToTarget(*m_surface.get(), size, transform, opacity * m_state.opacity, targetRect);
    }
    gTextureMapperCache.purge();
}

int TextureMapperNode::countDescendantsWithContent() const
{
    if (!m_state.visible || m_state.opacity < 0.001)
        return 0;
    int descendantsWithContent = (m_state.drawsContent || m_currentContent.contentType != HTMLContentType) ? 1 : 0;

    const int size = m_children.size();
    for (int i = 0; i < size; ++i) {
        if (TextureMapperNode* child = m_children[i])
            descendantsWithContent += child->countDescendantsWithContent();
    }

    return descendantsWithContent;
}

inline TextureMapperNode* TextureMapperNode::toTextureMapperNode(GraphicsLayer* layer)
{
    return layer ? static_cast<GraphicsLayerTextureMapper*>(layer)->m_node.get() : 0;
}

void TextureMapperNode::computeLayerType()
{
    // calculate layer type. A layer can be one of the following:
    //  RootLayer: the top level. Draws to a framebuffer, and the target texture draws into the viewport.
    //            only one layer is the root layer.
    //  ScissorLayer: draws to the current framebuffer, and applies an extra scissor before drawing its children.
    //                A scissor layer is a layer with children that masks to bounds, is not a transparency layer, and has a rectangular clip.
    //  ClipLayer: creates a new framebuffer, the size of the layer, and then paints it to the enclosing BitmapTexture with the layer's transform/opacity.
    //              A clip layer is a layer that masks to bounds, doesn't preserve 3D, has children, and has a transparency/mask or a non-rectangular transform.
    //  TransparencyLayer: creates a new framebuffer idetical in size to the current framebuffer. Then draws the fb's texture to the current framebuffer with identity transform.
    //                     Used for layers with children and transparency/mask that preserve 3D or don't mask to bounds.
    //  DefaultLayer: draws itself and its children directly to the current framebuffer.
    //                any layer that doesn't conform to the other rules is a DefaultLayer.

    const bool selfHasContent = m_state.drawsContent || (m_currentContent.contentType != HTMLContentType);
    const bool hasDescendantsWithContent = m_state.descendantsWithContent - (selfHasContent ? 1 : 0);
    const bool hasTransparency = m_state.opacity < 0.99 || m_state.maskLayer;
    const bool hasReplica = m_state.replicaLayer;
    m_layerType = DefaultLayer;

    // Layer has no parent, it must be a root layer.
    if (!m_parent && !m_effectTarget) {
        m_layerType = RootLayer;
        return;
    }

    // A layer with no contents is always a default layer.
    if (!m_state.descendantsWithContent)
        return;

    // A layer with content-descendants and a mask is always a clip layer.
    if (hasDescendantsWithContent && m_state.maskLayer) {
        m_layerType = ClipLayer;
        return;
    }

    // A masks-to bounds layer can be a clip or a scissor layer. It's a scissor layer only if it has a trivial clip (identity or translation), or if it has transparency.
    // That's because a ClipLayer would create an intermediate drawing surface (FB) - we want to limit it to when it's actually necessary, i.e. transparency or non-trivial clip.
    if (m_state.masksToBounds && hasDescendantsWithContent) {
        if (hasTransparency || !m_state.transform.isIdentityOrTranslation() || m_parent->m_state.preserves3D)
            m_layerType = ClipLayer;
        else
            m_layerType = ScissorLayer;
        return;
    }

    // We use a transparency layer when we have two of the following 3: replica, transparency, descendants with contents.
    if ((hasReplica && hasDescendantsWithContent) || (hasReplica && hasTransparency) || (hasTransparency && m_state.descendantsWithContent > 1))
        m_layerType = TransparencyLayer;
}
void TextureMapperNode::initializeTextureMapper(TextureMapper* textureMapper)
{
    if (textureMapper->type() == m_lastTextureMapperType)
        return;
    m_surface = textureMapper->createTexture();
    m_replicaSurface = textureMapper->createTexture();
    m_texture = textureMapper->createTexture();
    gTextureMapperCache.mark(m_texture.get());
    m_lastTextureMapperType = textureMapper->type();
}

TextureMapperNode::TextureMapperNode(GraphicsLayerTextureMapper* newLayer)
    : m_layer(newLayer)
    , m_lastTextureMapperType(0)
    , m_lastTextureMapper(0)
    , m_layerType(DefaultLayer)
    , m_surface(0)
    , m_parent(0)
    , m_effectTarget(0)
    , m_changeMask(NoChanges)
    , m_platformClient(0)
{

}

TextureMapperNode* TextureMapperNode::rootLayer()
{
    if (m_effectTarget)
        return m_effectTarget->rootLayer();
    if (m_parent)
        return m_parent->rootLayer();
    return this;
}

void TextureMapperNode::invalidateTransform()
{
    m_transforms.dirty = true;
    if (m_layerType != ClipLayer)
        m_state.dirty = true;
    if (m_state.replicaLayer)
        m_state.replicaLayer->invalidateTransform();
    const int size = m_children.size();
    for (int i = 0; i < size; ++i) {
        if (TextureMapperNode* layer = m_children[i])
            layer->invalidateTransform();
    }
}

void TextureMapperNode::computeLocalTransform()
{
    if (!m_transforms.localDirty)
        return;
    const float originX = m_state.anchorPoint.x() * m_size.width();
    const float originY = m_state.anchorPoint.y() * m_size.height();
    m_transforms.local =
        TransformationMatrix()
        .translate3d(originX + m_state.pos.x(), originY + m_state.pos.y(), m_state.anchorPoint.z())
        .multLeft(m_state.transform)
        .translate3d(-originX, -originY, -m_state.anchorPoint.z());
    m_transforms.localDirty = false;
}

void TextureMapperNode::flattenTo2DSpaceIfNecessary()
{
    if (m_state.preserves3D)
        return;
    m_transforms.forDescendants.setM13(0);
    m_transforms.forDescendants.setM23(0);
    m_transforms.forDescendants.setM31(0);
    m_transforms.forDescendants.setM32(0);
    m_transforms.forDescendants.setM33(1);
    m_transforms.forDescendants.setM34(0);
    m_transforms.forDescendants.setM43(0);
}

IntSize TextureMapperNode::nearestSurfaceSize() const
{
    if (m_layerType == ClipLayer || m_layerType == RootLayer)
        return m_surface && !m_surface->size().isEmpty() ? m_surface->size() : m_size;
    return m_parent->nearestSurfaceSize();
}

void TextureMapperNode::computeReplicaTransform()
{
    if (!m_state.replicaLayer)
        return;

    m_nearestSurfaceSize = nearestSurfaceSize();

    if (m_layerType != TransparencyLayer) {
        m_transforms.replica = TransformationMatrix(m_transforms.target).multLeft(m_state.replicaLayer->m_transforms.local);
        return;
    }

    const float originX = m_transforms.target.m41();
    const float originY = m_transforms.target.m42();
    m_transforms.replica =
            TransformationMatrix()
                .translate(originX, originY)
                .multLeft(m_state.replicaLayer->m_transforms.local)
                .translate(-originX, -originY);
}

void TextureMapperNode::computeTransformations()
{
    if (!m_transforms.dirty)
        return;

    m_transforms.dirty = false;
    if ((m_size.isEmpty() && m_state.masksToBounds))
        return;

    TextureMapperNode* parent = m_parent;
    computeLocalTransform();

    m_transforms.target = TransformationMatrix(parent ? parent->m_transforms.forDescendants : TransformationMatrix()).multLeft(m_transforms.local);
    m_transforms.forDescendants = (m_layerType == ClipLayer ? TransformationMatrix() : m_transforms.target);

    if (m_effectTarget)
        return;

    m_transforms.targetBoundingRect = IntRect(m_transforms.target.mapRect(entireRect()));
    if (m_state.replicaLayer)
        m_state.replicaLayer->computeTransformations();

    flattenTo2DSpaceIfNecessary();

    if (!m_state.backfaceVisibility && m_transforms.target.inverse().m33() < 0) {
        m_state.visible = false;
        return;
    }
    m_state.visible = true;

    if (parent && parent->m_state.preserves3D)
        m_transforms.centerZ = m_transforms.target.mapPoint(FloatPoint3D(m_size.width() / 2, m_size.height() / 2, 0)).z();

    if (!m_children.size())
        return;

    if (m_state.childrenTransform.isIdentity())
        return;

    const FloatPoint centerPoint = FloatPoint(m_size.width() / 2, m_size.height() / 2);
    if (m_transforms.perspectiveDirty)
        m_transforms.perspective = TransformationMatrix()
            .translate(centerPoint.x(), centerPoint.y())
            .multLeft(m_state.childrenTransform)
            .translate(-centerPoint.x(), -centerPoint.y());
    m_transforms.perspectiveDirty = false;
    m_transforms.forDescendants.multLeft(m_transforms.perspective);
}

void TextureMapperNode::uploadTextureFromContent(TextureMapper* textureMapper, const IntRect& visibleRect)
{
    if (m_size.isEmpty() || !m_layer) {
        m_texture->destroy();
        return;
    }

    if (m_currentContent.contentType == DirectImageContentType) {
        if (m_currentContent.image)
            m_texture->setContentsToImage(m_currentContent.image.get());
        return;
    }

    if (m_currentContent.contentType == MediaContentType) {
        if (!m_currentContent.media)
            return;
        m_texture->reset(m_size, true);
        PlatformGraphicsContext* platformContext = m_texture->beginPaintMedia();
        GraphicsContext context(platformContext);
        m_currentContent.media->paint(&context);
        m_texture->endPaint();
        return;
    }

    const bool needsReset = (m_texture->contentSize() != m_size) || !m_texture->isValid();
    if ((m_currentContent.contentType != HTMLContentType)
        || (!m_currentContent.needsDisplay && m_currentContent.needsDisplayRect.isEmpty() && !needsReset))
        return;

    IntRect dirtyRect = IntRect(0, 0, m_size.width(), m_size.height());
    if (!needsReset && !m_currentContent.needsDisplay)
        dirtyRect.intersect(m_currentContent.needsDisplayRect);
    if (needsReset)
        m_texture->reset(m_size, m_state.contentsOpaque);
    m_pendingContent.needsDisplayRect = IntRect();

    {
        GraphicsContext context(m_texture->beginPaint(dirtyRect));
        if (textureMapper && textureMapper->graphicsContext()) {
            GraphicsContext* originalContext = textureMapper->graphicsContext();
            context.setImageInterpolationQuality(originalContext->imageInterpolationQuality());
            context.setTextDrawingMode(originalContext->textDrawingMode());
        }
        m_layer->paintGraphicsLayerContents(context, dirtyRect);
    }
    m_texture->endPaint();
    m_currentContent.needsDisplay = false;

}

void TextureMapperNode::paintSelf(const TexmapPaintOptions& options)
{
    if (!m_layer || m_size.isEmpty() || (!m_state.drawsContent && m_currentContent.contentType == HTMLContentType))
        return;

    RefPtr<BitmapTexture> maskTexture = m_state.maskLayer ? m_state.maskLayer->m_texture : 0;
    RefPtr<BitmapTexture> replicaMaskTexture = 0;
    if (m_state.replicaLayer && m_state.replicaLayer->m_state.maskLayer)
        replicaMaskTexture = m_state.replicaLayer->m_state.maskLayer->m_texture;

    const float opacity = options.isSurface ? 1 : options.opacity;

    uploadTextureFromContent(options.textureMapper, options.visibleRect);
    if (m_state.replicaLayer && !options.isSurface)
        options.textureMapper->drawTexture(*m_texture.get(), replicaRect(), m_transforms.replica,
                         opacity * m_state.replicaLayer->m_state.opacity,
                         replicaMaskTexture ? replicaMaskTexture.get() : maskTexture.get());

    const IntRect rect = m_layerType == ClipLayer ? entireRect() : targetRect();
    const TransformationMatrix transform = m_layerType == ClipLayer ? TransformationMatrix() : m_transforms.target;
    options.textureMapper->drawTexture(*m_texture.get(), rect, transform, opacity, options.isSurface ? 0 : maskTexture.get());
}

void TextureMapperNode::paintRecursive(TexmapPaintOptions options)
{
    bool isDirty = m_state.dirty;
    m_state.dirty = false;

    if ((m_size.isEmpty() && (m_state.masksToBounds
        || m_children.isEmpty())) || !m_state.visible || options.opacity < 0.01 || m_state.opacity < 0.01)
        return;

    initializeTextureMapper(options.textureMapper);
    computeReplicaTransform();

    if (m_state.maskLayer) {
        m_state.maskLayer->initializeTextureMapper(options.textureMapper);
        m_state.maskLayer->m_state.dirty = false;
    }

    if (m_state.replicaLayer) {
        m_state.replicaLayer->initializeTextureMapper(options.textureMapper);
        m_state.replicaLayer->m_state.dirty = false;
        if (m_state.replicaLayer->m_state.maskLayer) {
            m_state.replicaLayer->m_state.maskLayer->initializeTextureMapper(options.textureMapper);
            m_state.replicaLayer->m_state.maskLayer->m_state.dirty = false;
        }
    }

    TextureMapperNode* replica = m_state.replicaLayer;
    const bool isSurface = (m_layerType == ClipLayer
                            || m_layerType == TransparencyLayer
                            || (m_layerType == RootLayer
                                && (options.textureMapper->allowSurfaceForRoot() || m_state.hasSurfaceDescendants)
                                ));
    if (isSurface)
        uploadTextureFromContent(options.textureMapper, options.visibleRect);
    const IntRect boundingRectfromNearestSurface = m_transforms.targetBoundingRect;

    options.opacity *= m_state.opacity;

    TexmapPaintOptions optionsForDescendants(options);
    optionsForDescendants.opacity = isSurface ? 1 : options.opacity;
    options.isSurface = isSurface;

    if (m_layerType == ClipLayer) {
        optionsForDescendants.visibleRect = TransformationMatrix().translate(-boundingRectfromNearestSurface.x(), -boundingRectfromNearestSurface.y()).mapRect(options.visibleRect);
        optionsForDescendants.scissorRect = IntRect(0, 0, m_size.width(), m_size.height());
    }

    if (m_layerType == ScissorLayer)
        optionsForDescendants.scissorRect.intersect(m_transforms.targetBoundingRect);
    options.textureMapper->setClip(optionsForDescendants.scissorRect);

    TextureMapperCacheLock(m_texture.get());
    TextureMapperCacheLock(m_surface.get());
    TextureMapperCacheLock(m_replicaSurface.get());

    gTextureMapperCache.purge();

    if (isSurface) {
        ASSERT(m_surface);
        if (!m_surface->isValid())
            isDirty = true;
        if (m_state.tiled) {
            m_surface->reset(options.visibleRect.size());
            m_surface->setOffset(options.visibleRect.location());
        } else if (isDirty)
            m_surface->reset(m_layerType == TransparencyLayer ? options.surface->size() : m_size);
        gTextureMapperCache.mark(m_surface.get());
        options.textureMapper->bindSurface(m_surface.get());

        optionsForDescendants.surface = m_surface.get();
    } else if (m_surface)
        m_surface->destroy();

    RefPtr<BitmapTexture> maskTexture;
    RefPtr<BitmapTexture> replicaMaskTexture;
    if (TextureMapperNode* mask = m_state.maskLayer) {
        mask->uploadTextureFromContent(options.textureMapper, options.visibleRect);
        maskTexture = mask->m_texture;
    }

    if (replica && replica->m_state.maskLayer) {
        replica->m_state.maskLayer->uploadTextureFromContent(options.textureMapper, options.visibleRect);
        replicaMaskTexture = replica->m_state.maskLayer->m_texture;
    }

    int childrenSize = m_children.size();
    if (isDirty || !isSurface || m_state.tiled || !m_surface->isValid()) {
        bool didPaintSelf = false;
        if (!m_state.preserves3D || m_children.isEmpty()) {
            paintSelf(options);
            didPaintSelf = true;
        }

        if (m_children.isEmpty() && !isSurface)
            return;

        if (m_layerType == ScissorLayer)
            optionsForDescendants.scissorRect.intersect(m_transforms.target.mapRect(IntRect(0, 0, m_size.width(), m_size.height())));

        for (int i = 0; i < childrenSize; ++i) {
            TextureMapperNode* layer = m_children[i];
            if (!layer)
                continue;

            if (!didPaintSelf && layer->m_transforms.centerZ >= 0) {
                paintSelf(options);
                didPaintSelf = true;
            }
            layer->paintRecursive(optionsForDescendants);
            if (isSurface) {
                ASSERT(m_surface);
                gTextureMapperCache.mark(m_surface.get());
                options.textureMapper->bindSurface(m_surface.get());
            }
        }
        if (!didPaintSelf) {
            paintSelf(options);
            didPaintSelf = true;
        }
    }

    if (m_layerType == RootLayer || m_layerType == DefaultLayer || m_layerType == ScissorLayer)
        return;

    ASSERT(m_surface);
    BitmapTexture& texture = *m_surface.get();
    if (replica) {
        ASSERT(m_replicaSurface);
        m_replicaSurface->reset(options.surface->size());
        m_replicaSurface->setOffset(options.surface->offset());
        gTextureMapperCache.mark(m_replicaSurface.get());
        options.textureMapper->bindSurface(m_replicaSurface.get());
        options.textureMapper->drawTexture(texture, replicaRect(), m_transforms.replica, replica->m_state.opacity, replicaMaskTexture ? replicaMaskTexture.get() : maskTexture.get());
        options.textureMapper->drawTexture(texture, IntRect(IntPoint(0, 0), options.surface->size()), TransformationMatrix(), 1.0f, maskTexture.get());
        options.textureMapper->bindSurface(options.surface);
        gTextureMapperCache.mark(options.surface);
        options.textureMapper->drawTexture(*m_replicaSurface.get(), IntRect(IntPoint(0, 0), options.surface->size()), TransformationMatrix(), options.opacity, 0);
        return;
    }

    options.textureMapper->bindSurface(options.surface);
    options.textureMapper->drawTexture(texture,
                             m_layerType == TransparencyLayer ? IntRect(IntPoint(0, 0), options.surface->size()) :
                             targetRect(),
                             m_layerType == TransparencyLayer ? TransformationMatrix() : m_transforms.target,
                             options.opacity, maskTexture.get());
    gTextureMapperCache.mark(&texture);
}

void TextureMapperNode::cleanupTextureMapper()
{
    if (m_texture)
        m_texture->destroy();
    if (m_surface)
        m_surface->destroy();
    if (m_replicaSurface)
        m_replicaSurface->destroy();
    for (int i = 0; i < m_children.size(); ++i) {
        if (m_children[i])
            m_children[i]->cleanupTextureMapper();
    }
    if (m_lastTextureMapper)
        m_lastTextureMapper->cleanup();
}

TextureMapperNode::~TextureMapperNode()
{
    setNeedsDisplay();
    {
        const int childrenSize = m_children.size();
        for (int i = childrenSize-1; i >= 0; --i) {
            ASSERT(m_children[i]->m_parent == this);
            m_children[i]->m_parent = 0;
        }
    }
    if (m_parent)
        m_parent->m_children.remove(m_parent->m_children.find(this));
}

void TextureMapperNode::notifyChange(ChangeMask changeMask)
{
    m_changeMask |= changeMask;
    if (!m_layer->client())
        return;
    m_layer->client()->notifySyncRequired(m_layer);
}

void TextureMapperNode::performPostSyncOperations()
{
    const LayerType prevLayerType = m_layerType;
    computeLayerType();
    if (prevLayerType != m_layerType)
        m_state.dirty = true;
    if (m_transforms.dirty)
        setNeedsDisplay();

    computeTransformations();
    if (m_state.maskLayer && !m_state.dirty)
        m_state.dirty = m_state.maskLayer->m_state.dirty;
    if (m_state.replicaLayer && !m_state.dirty)
        m_state.dirty = m_state.replicaLayer->m_state.dirty;

    const int size = m_children.size();

    for (int i = size - 1; i >= 0; --i) {
        TextureMapperNode* layer = m_children[i];

        layer->performPostSyncOperations();
        if (!m_state.dirty)
            m_state.dirty = layer->m_state.dirty;
    }
    m_state.hasSurfaceDescendants = hasSurfaceDescendants();
    if (m_state.dirty)
        m_state.descendantsWithContent = countDescendantsWithContent();

    if (m_state.preserves3D)
        sortByZOrder(m_children, 0, size);
    if (m_state.dirty) 
        setNeedsDisplay();
}

void TextureMapperNode::syncCompositingState(bool recurse)
{
    bool needsToInvalidateTransform = false;

    if (!m_layer)
        return;

    if (m_changeMask == NoChanges)
        goto afterCurrentLayerSync;

    setNeedsDisplay();
    if (m_parent)
        m_parent->m_state.dirty = true;

    if (m_currentContent.contentType == HTMLContentType && (m_changeMask & ParentChange)) {
        // The WebCore compositor manages item ownership. We have to make sure graphicsview doesn't
        // try to snatch that ownership.

        if (!m_layer->parent())
            m_parent = 0;
        else
            m_parent = toTextureMapperNode(m_layer->parent());

        if (!m_layer->parent() && m_parent) {
            size_t index = m_parent->m_children.find(this);
            m_parent->m_children.remove(index);
        }

    }

    if (m_changeMask & ChildrenChange) {
        m_children.clear();
        for (size_t i = 0; i < m_layer->children().size(); ++i) {
            if (TextureMapperNode* child = toTextureMapperNode(m_layer->children()[i])) {
                if (!child)
                    continue;
                m_children.append(child);
                child->m_parent = this;
            }
        }
        m_state.dirty = true;
    }

    if (m_changeMask & (SizeChange | ContentsRectChange)) {
        IntSize wantedSize = IntSize(m_layer->size().width(), m_layer->size().height());
        if (wantedSize.isEmpty() && m_pendingContent.contentType == HTMLContentType)
            wantedSize = IntSize(m_layer->contentsRect().width(), m_layer->contentsRect().height());

        if (wantedSize != m_size) {
            m_size = IntSize(wantedSize.width(), wantedSize.height());
            if (m_platformClient)
                m_platformClient->setSizeChanged(m_size);
            const bool needsTiling = m_size.width() > 2000 || m_size.height() > 2000;
            if (m_state.tiled != needsTiling)
                m_state.tiled = needsTiling;
            m_state.dirty = true;
        }
    }

    if (m_changeMask & MaskLayerChange) {
       if (TextureMapperNode* layer = toTextureMapperNode(m_layer->maskLayer()))
           layer->m_effectTarget = this;
    }

    if (m_changeMask & ReplicaLayerChange) {
       if (TextureMapperNode* layer = toTextureMapperNode(m_layer->replicaLayer()))
           layer->m_effectTarget = this;
    }

    if (m_changeMask & (TransformChange | SizeChange | AnchorPointChange | PositionChange))
        m_transforms.localDirty = true;

    if (m_changeMask & (ChildrenTransformChange | SizeChange))
        m_transforms.perspectiveDirty = true;

    if (m_changeMask & (ChildrenTransformChange | Preserves3DChange | TransformChange | AnchorPointChange | SizeChange | ContentsRectChange | BackfaceVisibilityChange | PositionChange | MaskLayerChange | DrawsContentChange | ContentChange | ReplicaLayerChange))    {
        // Due to the differences between the way WebCore handles transforms and the way Qt handles transforms,
        // all these elements affect the transforms of all the descendants.
        needsToInvalidateTransform = true;
    }

    if (m_changeMask & DisplayChange) 
        m_state.dirty = true;

    m_state.maskLayer = toTextureMapperNode(m_layer->maskLayer());
    m_state.replicaLayer = toTextureMapperNode(m_layer->replicaLayer());
    m_state.pos = m_layer->position();
    m_state.anchorPoint = m_layer->anchorPoint();
    m_state.size = m_layer->size();
    m_state.transform = m_layer->transform();
    m_state.contentsRect = m_layer->contentsRect();
    m_state.opacity = m_layer->opacity();
    m_state.contentsRect = m_layer->contentsRect();
    m_state.preserves3D = m_layer->preserves3D();
    m_state.masksToBounds = m_layer->masksToBounds();
    m_state.drawsContent = m_layer->drawsContent();
    m_state.contentsOpaque = m_layer->contentsOpaque();
    m_state.backfaceVisibility = m_layer->backfaceVisibility();
    m_state.childrenTransform = m_layer->childrenTransform();
    m_currentContent.contentType = m_pendingContent.contentType;
    m_currentContent.image = m_pendingContent.image;
    m_currentContent.media = m_pendingContent.media;
    m_currentContent.backgroundColor = m_pendingContent.backgroundColor;
    m_currentContent.needsDisplay = m_currentContent.needsDisplay || m_pendingContent.needsDisplay;
    m_currentContent.needsDisplayRect.unite(m_pendingContent.needsDisplayRect);
    m_pendingContent.needsDisplay = false;
    m_pendingContent.needsDisplayRect = IntRect();
    m_changeMask = NoChanges;
    afterCurrentLayerSync:
    if (needsToInvalidateTransform)
        invalidateTransform();

    if (m_state.maskLayer) {
        m_state.maskLayer->syncCompositingState(false);
        if (m_state.maskLayer->m_size.isEmpty())
            m_state.maskLayer->m_size = m_size;
    }

    if (m_state.replicaLayer)
        m_state.replicaLayer->syncCompositingState(false);

#if 0
    if (m_state.dirty && m_texture && m_texture->allowOfflineTextureUpload())
        uploadTextureFromContent(0);
#endif

    if (!recurse)
        return;

    const int childrenSize = m_children.size();
    for (int i = childrenSize-1; i >= 0; --i)
        m_children[i]->syncCompositingState(true);
}

GraphicsLayerTextureMapper::GraphicsLayerTextureMapper(GraphicsLayerClient* client)
    : GraphicsLayer(client)
    , m_node(new TextureMapperNode(this))
{
}

void GraphicsLayerTextureMapper::setName(const String& name)
{
    m_node->m_name = name;
}

GraphicsLayerTextureMapper::~GraphicsLayerTextureMapper()
{
}

/* \reimp (GraphicsLayer.h): The current size might change, thus we need to update the whole display.
*/
void GraphicsLayerTextureMapper::setNeedsDisplay()
{
    m_node->m_pendingContent.needsDisplay = true;
    m_node->notifyChange(TextureMapperNode::DisplayChange);
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::setNeedsDisplayInRect(const FloatRect& rect)
{
    if (m_node->m_pendingContent.needsDisplay)
        return;
    m_node->m_pendingContent.needsDisplayRect.unite(IntRect(rect));
    m_node->notifyChange(TextureMapperNode::DisplayChange);
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::setParent(GraphicsLayer* layer)
{
    m_node->notifyChange(TextureMapperNode::ParentChange);
    GraphicsLayer::setParent(layer);
}

/* \reimp (GraphicsLayer.h)
*/
bool GraphicsLayerTextureMapper::setChildren(const Vector<GraphicsLayer*>& children)
{
    m_node->notifyChange(TextureMapperNode::ChildrenChange);
    return GraphicsLayer::setChildren(children);
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::addChild(GraphicsLayer* layer)
{
    m_node->notifyChange(TextureMapperNode::ChildrenChange);
    GraphicsLayer::addChild(layer);
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::addChildAtIndex(GraphicsLayer* layer, int index)
{
    GraphicsLayer::addChildAtIndex(layer, index);
    m_node->notifyChange(TextureMapperNode::ChildrenChange);
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::addChildAbove(GraphicsLayer* layer, GraphicsLayer* sibling)
{
     GraphicsLayer::addChildAbove(layer, sibling);
     m_node->notifyChange(TextureMapperNode::ChildrenChange);
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::addChildBelow(GraphicsLayer* layer, GraphicsLayer* sibling)
{

    GraphicsLayer::addChildBelow(layer, sibling);
    m_node->notifyChange(TextureMapperNode::ChildrenChange);
}

/* \reimp (GraphicsLayer.h)
*/
bool GraphicsLayerTextureMapper::replaceChild(GraphicsLayer* oldChild, GraphicsLayer* newChild)
{
    if (GraphicsLayer::replaceChild(oldChild, newChild)) {
        m_node->notifyChange(TextureMapperNode::ChildrenChange);
        return true;
    }

    return false;
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::removeFromParent()
{
    if (!parent())
        return;
    m_node->notifyChange(TextureMapperNode::ParentChange);
    GraphicsLayer::removeFromParent();
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::setMaskLayer(GraphicsLayer* value)
{
    if (value == maskLayer())
        return;
    GraphicsLayer::setMaskLayer(value);
    m_node->notifyChange(TextureMapperNode::MaskLayerChange);
}


/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::setReplicatedByLayer(GraphicsLayer* value)
{
    if (value == replicaLayer())
        return;
    GraphicsLayer::setReplicatedByLayer(value);
    m_node->notifyChange(TextureMapperNode::ReplicaLayerChange);
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::setPosition(const FloatPoint& value)
{
    if (value == position())
        return;
    GraphicsLayer::setPosition(value);
    m_node->notifyChange(TextureMapperNode::PositionChange);
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::setAnchorPoint(const FloatPoint3D& value)
{
    if (value == anchorPoint())
        return;
    GraphicsLayer::setAnchorPoint(value);
    m_node->notifyChange(TextureMapperNode::AnchorPointChange);
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::setSize(const FloatSize& value)
{
    if (value == size())
        return;

    GraphicsLayer::setSize(value);
    m_node->notifyChange(TextureMapperNode::SizeChange);
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::setTransform(const TransformationMatrix& value)
{
    if (value == transform())
        return;

    GraphicsLayer::setTransform(value);
    m_node->notifyChange(TextureMapperNode::TransformChange);
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::setChildrenTransform(const TransformationMatrix& value)
{
    if (value == childrenTransform())
        return;
    GraphicsLayer::setChildrenTransform(value);
    m_node->notifyChange(TextureMapperNode::ChildrenTransformChange);
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::setPreserves3D(bool value)
{
    if (value == preserves3D())
        return;
    GraphicsLayer::setPreserves3D(value);
    m_node->notifyChange(TextureMapperNode::Preserves3DChange);
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::setMasksToBounds(bool value)
{
    if (value == masksToBounds())
        return;
    GraphicsLayer::setMasksToBounds(value);
    m_node->notifyChange(TextureMapperNode::MasksToBoundsChange);
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::setDrawsContent(bool value)
{
    if (value == drawsContent())
        return;
    m_node->notifyChange(TextureMapperNode::DrawsContentChange);
    GraphicsLayer::setDrawsContent(value);
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::setBackgroundColor(const Color& value)
{
    if (value == m_node->m_pendingContent.backgroundColor)
        return;
    m_node->m_pendingContent.backgroundColor = value;
    GraphicsLayer::setBackgroundColor(value);
    m_node->notifyChange(TextureMapperNode::BackgroundColorChange);
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::clearBackgroundColor()
{
    if (!m_node->m_pendingContent.backgroundColor.isValid())
        return;
    m_node->m_pendingContent.backgroundColor = Color();
    GraphicsLayer::clearBackgroundColor();
    m_node->notifyChange(TextureMapperNode::BackgroundColorChange);
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::setContentsOpaque(bool value)
{
    if (value == contentsOpaque())
        return;
    m_node->notifyChange(TextureMapperNode::ContentsOpaqueChange);
    GraphicsLayer::setContentsOpaque(value);
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::setBackfaceVisibility(bool value)
{
    if (value == backfaceVisibility())
        return;
    GraphicsLayer::setBackfaceVisibility(value);
    m_node->notifyChange(TextureMapperNode::BackfaceVisibilityChange);
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::setOpacity(float value)
{
    if (value == opacity())
        return;
    GraphicsLayer::setOpacity(value);
    m_node->notifyChange(TextureMapperNode::OpacityChange);
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::setContentsRect(const IntRect& value)
{
    if (value == contentsRect())
        return;
    GraphicsLayer::setContentsRect(value);
    m_node->notifyChange(TextureMapperNode::ContentsRectChange);
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::setContentsToImage(Image* image)
{
    m_node->notifyChange(TextureMapperNode::ContentChange);
    m_node->m_pendingContent.contentType = image ? TextureMapperNode::DirectImageContentType : TextureMapperNode::HTMLContentType;
    m_node->m_pendingContent.image = image;
    GraphicsLayer::setContentsToImage(image);
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::setContentsBackgroundColor(const Color& color)
{
    m_node->notifyChange(TextureMapperNode::ContentChange);
    m_node->m_pendingContent.contentType = TextureMapperNode::ColorContentType;
    m_node->m_pendingContent.backgroundColor = color;
    GraphicsLayer::setContentsBackgroundColor(color);
}


void GraphicsLayerTextureMapper::setContentsToMedia(PlatformLayer* media)
{
    GraphicsLayer::setContentsToMedia(media);
    m_node->notifyChange(TextureMapperNode::ContentChange);
    m_node->m_pendingContent.contentType = media ? TextureMapperNode::MediaContentType : TextureMapperNode::HTMLContentType;
    if (media)
        m_node->m_pendingContent.media = static_cast<TextureMapperVideoLayer*>(media);
    else
        m_node->m_pendingContent.media = 0;
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::setContentsOrientation(CompositingCoordinatesOrientation orientation)
{
    if (contentsOrientation() == orientation)
        return;
    m_node->notifyChange(TextureMapperNode::ContentsOrientationChange);
    GraphicsLayer::setContentsOrientation(orientation);
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::syncCompositingStateForThisLayerOnly()
{
    m_node->syncCompositingState(false);
    m_node->performPostSyncOperations();
}

/* \reimp (GraphicsLayer.h)
*/
void GraphicsLayerTextureMapper::syncCompositingState()
{
    GraphicsLayer::syncCompositingState();
    m_node->syncCompositingState(true);
    m_node->performPostSyncOperations();
}

/* \reimp (GraphicsLayer.h)
 */
NativeLayer GraphicsLayerTextureMapper::nativeLayer() const
{
    return m_node.get();
}

/* \reimp (GraphicsLayer.h)
*/
PlatformLayer* GraphicsLayerTextureMapper::platformLayer() const
{
    return m_node.get();
}

PassOwnPtr<GraphicsLayer> GraphicsLayer::create(GraphicsLayerClient* client)
{
    return new GraphicsLayerTextureMapper(client);
}

}
