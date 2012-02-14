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

#include "config.h"
#include "TextureMapperNode.h"

#include "stdio.h"

#if USE(ACCELERATED_COMPOSITING)

#include "GraphicsLayerTextureMapper.h"
#include "ImageBuffer.h"
#include "MathExtras.h"

namespace WebCore {

TextureMapperNode* toTextureMapperNode(GraphicsLayer* layer)
{
    return layer ? toGraphicsLayerTextureMapper(layer)->node() : 0;
}

TextureMapperNode* TextureMapperNode::rootLayer()
{
    if (m_effectTarget)
        return m_effectTarget->rootLayer();
    if (m_parent)
        return m_parent->rootLayer();
    return this;
}

void TextureMapperNode::setTransform(const TransformationMatrix& matrix)
{
    m_transform.setLocalTransform(matrix);
}

void TextureMapperNode::clearBackingStoresRecursive()
{
    m_backingStore.clear();
    m_contentsLayer = 0;
    for (size_t i = 0; i < m_children.size(); ++i)
        m_children[i]->clearBackingStoresRecursive();
    if (m_state.maskLayer)
        m_state.maskLayer->clearBackingStoresRecursive();
}

void TextureMapperNode::computeTransformsRecursive()
{
    if (m_size.isEmpty() && m_state.masksToBounds)
        return;

    // Compute transforms recursively on the way down to leafs.
    TransformationMatrix parentTransform;
    if (m_parent)
        parentTransform = m_parent->m_transform.combinedForChildren();
    else if (m_effectTarget)
        parentTransform = m_effectTarget->m_transform.combined();
    m_transform.combineTransforms(parentTransform);

    m_state.visible = m_state.backfaceVisibility || m_transform.combined().inverse().m33() >= 0;

    if (m_parent && m_parent->m_state.preserves3D)
        m_centerZ = m_transform.combined().mapPoint(FloatPoint3D(m_size.width() / 2, m_size.height() / 2, 0)).z();

    if (m_state.maskLayer)
        m_state.maskLayer->computeTransformsRecursive();
    if (m_state.replicaLayer)
        m_state.replicaLayer->computeTransformsRecursive();
    for (int i = 0; i < m_children.size(); ++i)
        m_children[i]->computeTransformsRecursive();

    // Reorder children if needed on the way back up.
    if (m_state.preserves3D)
        sortByZOrder(m_children, 0, m_children.size());
}

void TextureMapperNode::updateBackingStore(TextureMapper* textureMapper, GraphicsLayer* layer)
{
    if (!layer || !textureMapper)
        return;

    if (!m_shouldUpdateBackingStoreFromLayer)
        return;

    if (!m_state.drawsContent || m_size.isEmpty()) {
        m_backingStore.clear();
        return;
    }

    IntRect dirtyRect = enclosingIntRect(m_state.needsDisplay ? layerRect() : m_state.needsDisplayRect);
    if (dirtyRect.isEmpty())
        return;

    if (!m_backingStore)
        m_backingStore = TextureMapperTiledBackingStore::create();

    ASSERT(dynamic_cast<TextureMapperTiledBackingStore*>(m_backingStore.get()));

    // Paint the entire dirty rect into an image buffer. This ensures we only paint once.
    OwnPtr<ImageBuffer> imageBuffer = ImageBuffer::create(dirtyRect.size());
    GraphicsContext* context = imageBuffer->context();
    context->setImageInterpolationQuality(textureMapper->imageInterpolationQuality());
    context->setTextDrawingMode(textureMapper->textDrawingMode());
    context->translate(-dirtyRect.x(), -dirtyRect.y());
    layer->paintGraphicsLayerContents(*context, dirtyRect);

    RefPtr<Image> image;

#if PLATFORM(QT)
    image = imageBuffer->copyImage(DontCopyBackingStore);
#else
    // FIXME: support DontCopyBackingStore in non-Qt ports that use TextureMapper.
    image = imageBuffer->copyImage(CopyBackingStore);
#endif

    static_cast<TextureMapperTiledBackingStore*>(m_backingStore.get())->updateContents(textureMapper, image.get(), m_size, dirtyRect);
}

void TextureMapperNode::paint()
{
    computeTransformsRecursive();

    TextureMapperPaintOptions options;
    options.textureMapper = m_textureMapper;
    options.textureMapper->bindSurface(0);
    paintRecursive(options);
}

void TextureMapperNode::paintSelf(const TextureMapperPaintOptions& options)
{
    // We apply the following transform to compensate for painting into a surface, and then apply the offset so that the painting fits in the target rect.
    TransformationMatrix transform =
            TransformationMatrix(options.transform)
            .multiply(m_transform.combined())
            .translate(options.offset.width(), options.offset.height());

    float opacity = options.opacity;
    RefPtr<BitmapTexture> mask = options.mask;

    if (m_backingStore)
        m_backingStore->paintToTextureMapper(options.textureMapper, layerRect(), transform, opacity, mask.get());

    if (m_contentsLayer)
        m_contentsLayer->paintToTextureMapper(options.textureMapper, m_state.contentsRect, transform, opacity, mask.get());
}

int TextureMapperNode::compareGraphicsLayersZValue(const void* a, const void* b)
{
    TextureMapperNode* const* nodeA = static_cast<TextureMapperNode* const*>(a);
    TextureMapperNode* const* nodeB = static_cast<TextureMapperNode* const*>(b);
    return int(((*nodeA)->m_centerZ - (*nodeB)->m_centerZ) * 1000);
}

void TextureMapperNode::sortByZOrder(Vector<TextureMapperNode* >& array, int first, int last)
{
    qsort(array.data(), array.size(), sizeof(TextureMapperNode*), compareGraphicsLayersZValue);
}

void TextureMapperNode::paintSelfAndChildren(const TextureMapperPaintOptions& options)
{
    bool hasClip = m_state.masksToBounds && !m_children.isEmpty();
    if (hasClip)
        options.textureMapper->beginClip(TransformationMatrix(options.transform).multiply(m_transform.combined()), FloatRect(0, 0, m_size.width(), m_size.height()));

    paintSelf(options);

    for (int i = 0; i < m_children.size(); ++i)
        m_children[i]->paintRecursive(options);

    if (hasClip)
        options.textureMapper->endClip();
}

IntRect TextureMapperNode::intermediateSurfaceRect()
{
    // FIXME: Add an inverse transform to LayerTransform.
    return intermediateSurfaceRect(m_transform.combined().inverse());
}

IntRect TextureMapperNode::intermediateSurfaceRect(const TransformationMatrix& matrix)
{
    IntRect rect;
    TransformationMatrix localTransform = TransformationMatrix(matrix).multiply(m_transform.combined());
    rect = enclosingIntRect(localTransform.mapRect(layerRect()));
    if (!m_state.masksToBounds && !m_state.maskLayer) {
        for (int i = 0; i < m_children.size(); ++i)
            rect.unite(m_children[i]->intermediateSurfaceRect(matrix));
    }

    if (m_state.replicaLayer)
        rect.unite(m_state.replicaLayer->intermediateSurfaceRect(matrix));

    return rect;
}

bool TextureMapperNode::shouldPaintToIntermediateSurface() const
{
    bool hasOpacity = m_opacity < 0.99;
    bool hasChildren = !m_children.isEmpty();
    bool hasReplica = !!m_state.replicaLayer;
    bool hasMask = !!m_state.maskLayer;

    // We don't use two-pass blending for preserves-3d, that's in sync with Safari.
    if (m_state.preserves3D)
        return false;

    // We should use an intermediate surface when blending several items with an ancestor opacity.
    // Tested by compositing/reflections/reflection-opacity.html
    if (hasOpacity && (hasChildren || hasReplica))
        return true;

    // We should use an intermediate surface with a masked ancestor.
    // In the case of replicas the mask is applied before replicating.
    // Tested by compositing/masks/masked-ancestor.html
    if (hasMask && hasChildren && !hasReplica)
        return true;

    return false;
}

bool TextureMapperNode::isVisible() const
{
    if (m_size.isEmpty() && (m_state.masksToBounds || m_state.maskLayer || m_children.isEmpty()))
        return false;
    if (!m_state.visible || m_opacity < 0.01)
        return false;
    return true;
}

void TextureMapperNode::paintSelfAndChildrenWithReplica(const TextureMapperPaintOptions& options)
{
    if (m_state.replicaLayer) {
        TextureMapperPaintOptions replicaOptions(options);
        // We choose either the content's mask or the replica's mask.
        // FIXME: blend the two if both exist.
        if (m_state.replicaLayer->m_state.maskLayer)
            replicaOptions.mask = m_state.replicaLayer->m_state.maskLayer->texture();

        replicaOptions.transform
                  .multiply(m_state.replicaLayer->m_transform.combined())
                  .multiply(m_transform.combined().inverse());
        paintSelfAndChildren(replicaOptions);
    }

    paintSelfAndChildren(options);
}

void TextureMapperNode::paintRecursive(const TextureMapperPaintOptions& options)
{
    if (!isVisible())
        return;

    float opacity = options.opacity * m_opacity;
    RefPtr<BitmapTexture> maskTexture = m_state.maskLayer ? m_state.maskLayer->texture() : 0;

    TextureMapperPaintOptions paintOptions(options);
    paintOptions.mask = maskTexture.get();
    IntRect surfaceRect;

    RefPtr<BitmapTexture> surface;

    if (!shouldPaintToIntermediateSurface()) {
        paintOptions.opacity = opacity;
        paintSelfAndChildrenWithReplica(paintOptions);
        return;
    }

    // Prepare a surface to paint into.
    // We paint into the surface ignoring the opacity/transform of the current layer.
    surfaceRect = intermediateSurfaceRect();
    surface = options.textureMapper->acquireTextureFromPool(surfaceRect.size());
    options.textureMapper->bindSurface(surface.get());
    paintOptions.opacity = 1;

    // We have to use combinedForChildren() and not combined(), otherwise preserve-3D doesn't work.
    paintOptions.transform = m_transform.combinedForChildren().inverse();
    paintOptions.offset = -IntSize(surfaceRect.x(), surfaceRect.y());

    paintSelfAndChildrenWithReplica(paintOptions);

    // If we painted the replica, the mask is already applied so we don't need to paint it again.
    if (m_state.replicaLayer)
        maskTexture = 0;

    options.textureMapper->bindSurface(options.surface.get());
    TransformationMatrix targetTransform =
            TransformationMatrix(options.transform)
                .multiply(m_transform.combined())
                .translate(options.offset.width(), options.offset.height());
    options.textureMapper->drawTexture(*surface.get(), surfaceRect, targetTransform, opacity, maskTexture.get());
}

TextureMapperNode::~TextureMapperNode()
{
    for (int i = m_children.size() - 1; i >= 0; --i)
        m_children[i]->m_parent = 0;

    if (m_parent)
        m_parent->m_children.remove(m_parent->m_children.find(this));
}

void TextureMapperNode::syncCompositingState(GraphicsLayerTextureMapper* graphicsLayer, int options)
{
    syncCompositingState(graphicsLayer, rootLayer()->m_textureMapper, options);
}

void TextureMapperNode::syncCompositingStateSelf(GraphicsLayerTextureMapper* graphicsLayer, TextureMapper* textureMapper)
{
    int changeMask = graphicsLayer->changeMask();

    if (changeMask == NoChanges && graphicsLayer->m_animations.isEmpty())
        return;

    if (changeMask & ParentChange) {
        TextureMapperNode* newParent = toTextureMapperNode(graphicsLayer->parent());
        if (newParent != m_parent) {
            // Remove node from current from child list first.
            if (m_parent) {
                size_t index = m_parent->m_children.find(this);
                m_parent->m_children.remove(index);
                m_parent = 0;
            }
            // Set new node parent and add node to the parents child list.
            if (newParent) {
                m_parent = newParent;
                m_parent->m_children.append(this);
            }
        }
    }

    if (changeMask & ChildrenChange) {
        // Clear children parent pointer to avoid unsync and crash on node delete.
        for (size_t i = 0; i < m_children.size(); i++)
            m_children[i]->m_parent = 0;

        m_children.clear();
        for (size_t i = 0; i < graphicsLayer->children().size(); ++i) {
            TextureMapperNode* child = toTextureMapperNode(graphicsLayer->children()[i]);
            if (!child)
                continue;
            m_children.append(child);
            child->m_parent = this;
        }
    }

    m_size = graphicsLayer->size();

    if (changeMask & MaskLayerChange) {
       if (TextureMapperNode* layer = toTextureMapperNode(graphicsLayer->maskLayer()))
           layer->m_effectTarget = this;
    }

    if (changeMask & ReplicaLayerChange) {
       if (TextureMapperNode* layer = toTextureMapperNode(graphicsLayer->replicaLayer()))
           layer->m_effectTarget = this;
    }

    if (changeMask & AnimationChange)
        m_animations = graphicsLayer->m_animations;

    m_state.maskLayer = toTextureMapperNode(graphicsLayer->maskLayer());
    m_state.replicaLayer = toTextureMapperNode(graphicsLayer->replicaLayer());
    m_state.pos = graphicsLayer->position();
    m_state.anchorPoint = graphicsLayer->anchorPoint();
    m_state.size = graphicsLayer->size();
    m_state.contentsRect = graphicsLayer->contentsRect();
    m_state.transform = graphicsLayer->transform();
    m_state.contentsRect = graphicsLayer->contentsRect();
    m_state.preserves3D = graphicsLayer->preserves3D();
    m_state.masksToBounds = graphicsLayer->masksToBounds();
    m_state.drawsContent = graphicsLayer->drawsContent();
    m_state.contentsOpaque = graphicsLayer->contentsOpaque();
    m_state.backfaceVisibility = graphicsLayer->backfaceVisibility();
    m_state.childrenTransform = graphicsLayer->childrenTransform();
    m_state.opacity = graphicsLayer->opacity();
    m_state.needsDisplay = m_state.needsDisplay || graphicsLayer->needsDisplay();
    if (!m_state.needsDisplay)
        m_state.needsDisplayRect.unite(graphicsLayer->needsDisplayRect());
    m_contentsLayer = graphicsLayer->contentsLayer();

    m_transform.setPosition(m_state.pos);
    m_transform.setAnchorPoint(m_state.anchorPoint);
    m_transform.setSize(m_state.size);
    m_transform.setFlattening(!m_state.preserves3D);
    m_transform.setChildrenTransform(m_state.childrenTransform);
}

bool TextureMapperNode::descendantsOrSelfHaveRunningAnimations() const
{
    if (m_animations.hasRunningAnimations())
        return true;

    for (size_t i = 0; i < m_children.size(); ++i) {
        if (m_children[i]->descendantsOrSelfHaveRunningAnimations())
            return true;
    }

    return false;
}

void TextureMapperNode::syncAnimations()
{
    m_animations.apply(this);
    if (!m_animations.hasActiveAnimationsOfType(AnimatedPropertyWebkitTransform))
        setTransform(m_state.transform);
    if (!m_animations.hasActiveAnimationsOfType(AnimatedPropertyOpacity))
        setOpacity(m_state.opacity);
}

void TextureMapperNode::syncAnimationsRecursively()
{
    syncAnimations();

    for (int i = m_children.size() - 1; i >= 0; --i)
        m_children[i]->syncAnimationsRecursively();
}

void TextureMapperNode::syncCompositingState(GraphicsLayerTextureMapper* graphicsLayer, TextureMapper* textureMapper, int options)
{
    if (!textureMapper)
        return;

    if (graphicsLayer && !(options & ComputationsOnly)) {
        syncCompositingStateSelf(graphicsLayer, textureMapper);
        graphicsLayer->didSynchronize();
    }

    if (graphicsLayer && m_state.maskLayer) {
        m_state.maskLayer->syncCompositingState(toGraphicsLayerTextureMapper(graphicsLayer->maskLayer()), textureMapper);

        // A mask layer has its parent's size by default, in case it's not set specifically.
        if (m_state.maskLayer->m_size.isEmpty())
            m_state.maskLayer->m_size = m_size;
    }

    if (m_state.replicaLayer)
        m_state.replicaLayer->syncCompositingState(toGraphicsLayerTextureMapper(graphicsLayer->replicaLayer()), textureMapper);

    syncAnimations();
    updateBackingStore(textureMapper, graphicsLayer);

    if (!(options & TraverseDescendants))
        options = ComputationsOnly;

    if (graphicsLayer) {
        Vector<GraphicsLayer*> children = graphicsLayer->children();
        for (int i = children.size() - 1; i >= 0; --i) {
            TextureMapperNode* node = toTextureMapperNode(children[i]);
            if (!node)
                continue;
            node->syncCompositingState(toGraphicsLayerTextureMapper(children[i]), textureMapper, options);
        }
    } else {
        for (int i = m_children.size() - 1; i >= 0; --i)
            m_children[i]->syncCompositingState(0, textureMapper, options);
    }
}

}
#endif
