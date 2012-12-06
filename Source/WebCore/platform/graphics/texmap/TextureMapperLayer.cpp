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
#include "TextureMapperLayer.h"

#if USE(ACCELERATED_COMPOSITING)

#include "GraphicsLayerTextureMapper.h"

namespace WebCore {

TextureMapperLayer* toTextureMapperLayer(GraphicsLayer* layer)
{
    return layer ? toGraphicsLayerTextureMapper(layer)->layer() : 0;
}

const TextureMapperLayer* TextureMapperLayer::rootLayer() const
{
    if (m_effectTarget)
        return m_effectTarget->rootLayer();
    if (m_parent)
        return m_parent->rootLayer();
    return this;
}

void TextureMapperLayer::setTransform(const TransformationMatrix& matrix)
{
    m_transform.setLocalTransform(matrix);
}

void TextureMapperLayer::clearBackingStoresRecursive()
{
    m_backingStore.clear();
    m_contentsLayer = 0;
    for (size_t i = 0; i < m_children.size(); ++i)
        m_children[i]->clearBackingStoresRecursive();
    if (m_state.maskLayer)
        m_state.maskLayer->clearBackingStoresRecursive();
}

void TextureMapperLayer::computeTransformsRecursive()
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

    m_state.visible = m_state.backfaceVisibility || !m_transform.combined().isBackFaceVisible();

    if (m_parent && m_parent->m_state.preserves3D)
        m_centerZ = m_transform.combined().mapPoint(FloatPoint3D(m_size.width() / 2, m_size.height() / 2, 0)).z();

    if (m_state.maskLayer)
        m_state.maskLayer->computeTransformsRecursive();
    if (m_state.replicaLayer)
        m_state.replicaLayer->computeTransformsRecursive();
    for (size_t i = 0; i < m_children.size(); ++i)
        m_children[i]->computeTransformsRecursive();

    // Reorder children if needed on the way back up.
    if (m_state.preserves3D)
        sortByZOrder(m_children, 0, m_children.size());
}

void TextureMapperLayer::paint()
{
    computeTransformsRecursive();

    TextureMapperPaintOptions options;
    options.textureMapper = m_textureMapper;
    options.textureMapper->bindSurface(0);
    paintRecursive(options);
}

void TextureMapperLayer::paintSelf(const TextureMapperPaintOptions& options)
{
    if (!m_state.visible || !m_state.contentsVisible)
        return;

    // We apply the following transform to compensate for painting into a surface, and then apply the offset so that the painting fits in the target rect.
    TransformationMatrix transform;
    transform.translate(options.offset.width(), options.offset.height());
    transform.multiply(options.transform);
    transform.multiply(m_transform.combined());

    float opacity = options.opacity;
    RefPtr<BitmapTexture> mask = options.mask;

    if (m_state.backgroundColor.alpha() && !m_state.contentsRect.isEmpty()) {
        Color color = m_state.backgroundColor;
        float r, g, b, a;
        color.getRGBA(r, g, b, a);
        color = Color(r * opacity, g * opacity, b * opacity, a * opacity);
        options.textureMapper->drawSolidColor(m_state.contentsRect, transform, color);
    }

    if (m_backingStore) {
        ASSERT(m_state.drawsContent && m_state.contentsVisible && !m_size.isEmpty());
        ASSERT(!layerRect().isEmpty());
        m_backingStore->paintToTextureMapper(options.textureMapper, layerRect(), transform, opacity, mask.get());
    }

    if (m_contentsLayer) {
        ASSERT(!layerRect().isEmpty());
        m_contentsLayer->paintToTextureMapper(options.textureMapper, m_state.contentsRect, transform, opacity, mask.get());
    }
}

int TextureMapperLayer::compareGraphicsLayersZValue(const void* a, const void* b)
{
    TextureMapperLayer* const* layerA = static_cast<TextureMapperLayer* const*>(a);
    TextureMapperLayer* const* layerB = static_cast<TextureMapperLayer* const*>(b);
    return int(((*layerA)->m_centerZ - (*layerB)->m_centerZ) * 1000);
}

void TextureMapperLayer::sortByZOrder(Vector<TextureMapperLayer* >& array, int /* first */, int /* last */)
{
    qsort(array.data(), array.size(), sizeof(TextureMapperLayer*), compareGraphicsLayersZValue);
}

void TextureMapperLayer::paintSelfAndChildren(const TextureMapperPaintOptions& options)
{
    paintSelf(options);

    if (m_children.isEmpty())
        return;

    bool shouldClip = m_state.masksToBounds && !m_state.preserves3D;
    if (shouldClip)
        options.textureMapper->beginClip(TransformationMatrix(options.transform).multiply(m_transform.combined()), layerRect());

    for (size_t i = 0; i < m_children.size(); ++i)
        m_children[i]->paintRecursive(options);

    if (shouldClip)
        options.textureMapper->endClip();
}

IntRect TextureMapperLayer::intermediateSurfaceRect()
{
    // FIXME: Add an inverse transform to LayerTransform.
    return intermediateSurfaceRect(m_transform.combined().inverse());
}

IntRect TextureMapperLayer::intermediateSurfaceRect(const TransformationMatrix& matrix)
{
    IntRect rect;
    TransformationMatrix localTransform = TransformationMatrix(matrix).multiply(m_transform.combined());
    rect = enclosingIntRect(localTransform.mapRect(layerRect()));
    if (!m_state.masksToBounds && !m_state.maskLayer) {
        for (size_t i = 0; i < m_children.size(); ++i)
            rect.unite(m_children[i]->intermediateSurfaceRect(matrix));
    }

#if ENABLE(CSS_FILTERS)
    if (m_filters.hasOutsets()) {
        int leftOutset;
        int topOutset;
        int bottomOutset;
        int rightOutset;
        m_filters.getOutsets(topOutset, rightOutset, bottomOutset, leftOutset);
        IntRect unfilteredTargetRect(rect);
        rect.move(std::max(0, -leftOutset), std::max(0, -topOutset));
        rect.expand(leftOutset + rightOutset, topOutset + bottomOutset);
        rect.unite(unfilteredTargetRect);
    }
#endif

    if (m_state.replicaLayer)
        rect.unite(m_state.replicaLayer->intermediateSurfaceRect(matrix));

    return rect;
}

TextureMapperLayer::ContentsLayerCount TextureMapperLayer::countPotentialLayersWithContents() const
{
    int selfLayersWithContents = (m_state.drawsContent ? 1 : 0) + (m_contentsLayer ? 1 : 0);
    int potentialLayersWithContents = selfLayersWithContents + m_children.size();

    if (!potentialLayersWithContents)
        return NoLayersWithContent;

    if (potentialLayersWithContents > 1)
        return MultipleLayersWithContents;

    if (m_children.isEmpty())
        return SingleLayerWithContents;

    return m_children.first()->countPotentialLayersWithContents();
}

bool TextureMapperLayer::shouldPaintToIntermediateSurface() const
{
#if ENABLE(CSS_FILTERS)
    if (m_filters.size())
        return true;
#endif
    bool hasOpacity = m_opacity < 0.99;
    bool canHaveMultipleLayersWithContent = countPotentialLayersWithContents() == MultipleLayersWithContents;
    bool hasReplica = !!m_state.replicaLayer;
    bool hasMask = !!m_state.maskLayer;

    // We don't use two-pass blending for preserves-3d, that's in sync with Safari.
    if (m_state.preserves3D)
        return false;

    // We should use an intermediate surface when blending several items with an ancestor opacity.
    // Tested by compositing/reflections/reflection-opacity.html
    if (hasOpacity && (canHaveMultipleLayersWithContent || hasReplica))
        return true;

    // We should use an intermediate surface with a masked ancestor.
    // In the case of replicas the mask is applied before replicating.
    // Tested by compositing/masks/masked-ancestor.html
    if (hasMask && canHaveMultipleLayersWithContent && !hasReplica)
        return true;

    return false;
}

bool TextureMapperLayer::isVisible() const
{
    if (m_size.isEmpty() && (m_state.masksToBounds || m_state.maskLayer || m_children.isEmpty()))
        return false;
    if (!m_state.visible && m_children.isEmpty())
        return false;
    if (!m_state.contentsVisible && m_children.isEmpty())
        return false;
    if (m_opacity < 0.01)
        return false;
    return true;
}

void TextureMapperLayer::paintSelfAndChildrenWithReplica(const TextureMapperPaintOptions& options)
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

#if ENABLE(CSS_FILTERS)
static bool shouldKeepContentTexture(const FilterOperations& filters)
{
    for (size_t i = 0; i < filters.size(); ++i) {
        switch (filters.operations().at(i)->getOperationType()) {
        // The drop-shadow filter requires the content texture, because it needs to composite it
        // on top of the blurred shadow color.
        case FilterOperation::DROP_SHADOW:
            return true;
        default:
            break;
        }
    }

    return false;
}

static PassRefPtr<BitmapTexture> applyFilters(const FilterOperations& filters, TextureMapper* textureMapper, BitmapTexture* source, IntRect& targetRect)
{
    if (!filters.size())
        return source;

    RefPtr<BitmapTexture> filterSurface = shouldKeepContentTexture(filters) ? textureMapper->acquireTextureFromPool(source->size()) : source;
    return filterSurface->applyFilters(textureMapper, *source, filters);
}
#endif

void TextureMapperLayer::paintRecursive(const TextureMapperPaintOptions& options)
{
    if (!isVisible())
        return;

    float opacity = options.opacity * m_opacity;
    RefPtr<BitmapTexture> maskTexture = m_state.maskLayer ? m_state.maskLayer->texture() : 0;

    TextureMapperPaintOptions paintOptions(options);
    paintOptions.mask = maskTexture.get();

    if (!shouldPaintToIntermediateSurface()) {
        paintOptions.opacity = opacity;
        paintSelfAndChildrenWithReplica(paintOptions);
        return;
    }

    // Prepare a surface to paint into.
    // We paint into the surface ignoring the opacity/transform of the current layer.
    IntRect surfaceRect = intermediateSurfaceRect();
    RefPtr<BitmapTexture> surface = options.textureMapper->acquireTextureFromPool(surfaceRect.size());
    paintOptions.surface = surface;
    options.textureMapper->bindSurface(surface.get());
    paintOptions.opacity = 1;

    paintOptions.transform = m_transform.combined().inverse();
    paintOptions.offset = -IntSize(surfaceRect.x(), surfaceRect.y());

    paintSelfAndChildrenWithReplica(paintOptions);

    // If we painted the replica, the mask is already applied so we don't need to paint it again.
    if (m_state.replicaLayer)
        maskTexture = 0;

#if ENABLE(CSS_FILTERS)
    surface = applyFilters(m_filters, options.textureMapper, surface.get(), surfaceRect);
#endif

    options.textureMapper->bindSurface(options.surface.get());
    TransformationMatrix targetTransform;
    targetTransform.translate(options.offset.width(), options.offset.height());
    targetTransform.multiply(options.transform);
    targetTransform.multiply(m_transform.combined());

    options.textureMapper->drawTexture(*surface.get(), surfaceRect, targetTransform, opacity, maskTexture.get());
}

TextureMapperLayer::~TextureMapperLayer()
{
    for (int i = m_children.size() - 1; i >= 0; --i)
        m_children[i]->m_parent = 0;

    if (m_parent)
        m_parent->m_children.remove(m_parent->m_children.find(this));
}

TextureMapper* TextureMapperLayer::textureMapper() const
{
    return rootLayer()->m_textureMapper;
}

void TextureMapperLayer::flushCompositingState(GraphicsLayerTextureMapper* graphicsLayer, int options)
{
    flushCompositingState(graphicsLayer, textureMapper(), options);
}

void TextureMapperLayer::flushCompositingStateSelf(GraphicsLayerTextureMapper* graphicsLayer, TextureMapper*)
{
    int changeMask = graphicsLayer->changeMask();

    if (changeMask == NoChanges && graphicsLayer->m_animations.isEmpty())
        return;

    graphicsLayer->updateDebugIndicators();

    if (changeMask & ParentChange) {
        TextureMapperLayer* newParent = toTextureMapperLayer(graphicsLayer->parent());
        if (newParent != m_parent) {
            // Remove layer from current from child list first.
            if (m_parent) {
                size_t index = m_parent->m_children.find(this);
                m_parent->m_children.remove(index);
                m_parent = 0;
            }
            // Set new layer parent and add layer to the parents child list.
            if (newParent) {
                m_parent = newParent;
                m_parent->m_children.append(this);
            }
        }
    }

    if (changeMask & ChildrenChange) {
        // Clear children parent pointer to avoid unsync and crash on layer delete.
        for (size_t i = 0; i < m_children.size(); i++)
            m_children[i]->m_parent = 0;

        m_children.clear();
        for (size_t i = 0; i < graphicsLayer->children().size(); ++i) {
            TextureMapperLayer* child = toTextureMapperLayer(graphicsLayer->children()[i]);
            if (!child)
                continue;
            m_children.append(child);
            child->m_parent = this;
        }
    }

    m_size = graphicsLayer->size();

    if (changeMask & MaskLayerChange) {
       if (TextureMapperLayer* layer = toTextureMapperLayer(graphicsLayer->maskLayer()))
           layer->m_effectTarget = this;
    }

    if (changeMask & ReplicaLayerChange) {
       if (TextureMapperLayer* layer = toTextureMapperLayer(graphicsLayer->replicaLayer()))
           layer->m_effectTarget = this;
    }

    if (changeMask & AnimationChange)
        m_animations = graphicsLayer->m_animations;

    m_state.maskLayer = toTextureMapperLayer(graphicsLayer->maskLayer());
    m_state.replicaLayer = toTextureMapperLayer(graphicsLayer->replicaLayer());
    m_state.pos = graphicsLayer->position();
    m_state.anchorPoint = graphicsLayer->anchorPoint();
    m_state.size = graphicsLayer->size();
    m_state.transform = graphicsLayer->transform();
    m_state.contentsRect = graphicsLayer->contentsRect();
    m_state.preserves3D = graphicsLayer->preserves3D();
    m_state.masksToBounds = graphicsLayer->masksToBounds();
    m_state.drawsContent = graphicsLayer->drawsContent();
    m_state.contentsVisible = graphicsLayer->contentsAreVisible();
    m_state.contentsOpaque = graphicsLayer->contentsOpaque();
    m_state.backfaceVisibility = graphicsLayer->backfaceVisibility();
    m_state.childrenTransform = graphicsLayer->childrenTransform();
    m_state.opacity = graphicsLayer->opacity();
    m_state.backgroundColor = graphicsLayer->backgroundColor();
#if ENABLE(CSS_FILTERS)
    if (changeMask & FilterChange)
        m_state.filters = graphicsLayer->filters();
#endif
    m_fixedToViewport = graphicsLayer->fixedToViewport();

    m_contentsLayer = graphicsLayer->platformLayer();

    m_transform.setPosition(adjustedPosition());
    m_transform.setAnchorPoint(m_state.anchorPoint);
    m_transform.setSize(m_state.size);
    m_transform.setFlattening(!m_state.preserves3D);
    m_transform.setChildrenTransform(m_state.childrenTransform);
}

bool TextureMapperLayer::descendantsOrSelfHaveRunningAnimations() const
{
    if (m_animations.hasRunningAnimations())
        return true;

    for (size_t i = 0; i < m_children.size(); ++i) {
        if (m_children[i]->descendantsOrSelfHaveRunningAnimations())
            return true;
    }

    return false;
}

void TextureMapperLayer::applyAnimationsRecursively()
{
    syncAnimations();
    for (size_t i = 0; i < m_children.size(); ++i)
        m_children[i]->applyAnimationsRecursively();
}

void TextureMapperLayer::syncAnimations()
{
    m_animations.apply(this);
    if (!m_animations.hasActiveAnimationsOfType(AnimatedPropertyWebkitTransform))
        setTransform(m_state.transform);
    if (!m_animations.hasActiveAnimationsOfType(AnimatedPropertyOpacity))
        setOpacity(m_state.opacity);
#if ENABLE(CSS_FILTERS)
    if (!m_animations.hasActiveAnimationsOfType(AnimatedPropertyWebkitFilter))
        setFilters(m_state.filters);
#endif
}

void TextureMapperLayer::flushCompositingState(GraphicsLayerTextureMapper* graphicsLayer, TextureMapper* textureMapper, int options)
{
    if (!textureMapper)
        return;

    if (graphicsLayer && !(options & ComputationsOnly))
        flushCompositingStateSelf(graphicsLayer, textureMapper);

    if (graphicsLayer && m_state.maskLayer)
        m_state.maskLayer->flushCompositingState(toGraphicsLayerTextureMapper(graphicsLayer->maskLayer()), textureMapper);

    if (m_state.replicaLayer)
        m_state.replicaLayer->flushCompositingState(toGraphicsLayerTextureMapper(graphicsLayer->replicaLayer()), textureMapper);

    syncAnimations();

    if (!(options & TraverseDescendants))
        options = ComputationsOnly;

    if (graphicsLayer) {
        Vector<GraphicsLayer*> children = graphicsLayer->children();
        for (int i = children.size() - 1; i >= 0; --i) {
            TextureMapperLayer* layer = toTextureMapperLayer(children[i]);
            if (!layer)
                continue;
            layer->flushCompositingState(toGraphicsLayerTextureMapper(children[i]), textureMapper, options);
        }
    } else {
        for (int i = m_children.size() - 1; i >= 0; --i)
            m_children[i]->flushCompositingState(0, textureMapper, options);
    }
}

bool TextureMapperLayer::isAncestorFixedToViewport() const
{
    for (TextureMapperLayer* parent = m_parent; parent; parent = parent->m_parent) {
        if (parent->m_fixedToViewport)
            return true;
    }

    return false;
}

void TextureMapperLayer::setScrollPositionDeltaIfNeeded(const FloatSize& delta)
{
    // delta is the difference between the scroll offset in the ui process and the scroll offset
    // in the web process. We add this delta to the position of fixed layers, to make
    // sure that they do not move while scrolling. We need to reset this delta to fixed layers
    // that have an ancestor which is also a fixed layer, because the delta will be added to the ancestor.
    if (isAncestorFixedToViewport())
        m_scrollPositionDelta = FloatSize();
    else
        m_scrollPositionDelta = delta;
    m_transform.setPosition(adjustedPosition());
}

}
#endif
