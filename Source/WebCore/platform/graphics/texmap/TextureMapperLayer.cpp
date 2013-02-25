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

namespace WebCore {

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

const TextureMapperLayer* TextureMapperLayer::rootLayer() const
{
    if (m_effectTarget)
        return m_effectTarget->rootLayer();
    if (m_parent)
        return m_parent->rootLayer();
    return this;
}

void TextureMapperLayer::computeTransformsRecursive()
{
    if (m_state.size.isEmpty() && m_state.masksToBounds)
        return;

    // Compute transforms recursively on the way down to leafs.
    TransformationMatrix parentTransform;
    if (m_parent)
        parentTransform = m_parent->m_currentTransform.combinedForChildren();
    else if (m_effectTarget)
        parentTransform = m_effectTarget->m_currentTransform.combined();
    m_currentTransform.combineTransforms(parentTransform);

    m_state.visible = m_state.backfaceVisibility || !m_currentTransform.combined().isBackFaceVisible();

    if (m_parent && m_parent->m_state.preserves3D)
        m_centerZ = m_currentTransform.combined().mapPoint(FloatPoint3D(m_state.size.width() / 2, m_state.size.height() / 2, 0)).z();

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

static Color blendWithOpacity(const Color& color, float opacity)
{
    RGBA32 rgba = color.rgb();
    // See Color::getRGBA() to know how to extract alpha from color.
    float alpha = alphaChannel(rgba) / 255.;
    float effectiveAlpha = alpha * opacity;
    return Color(colorWithOverrideAlpha(rgba, effectiveAlpha));
}

void TextureMapperLayer::paintSelf(const TextureMapperPaintOptions& options)
{
    if (!m_state.visible || !m_state.contentsVisible)
        return;

    // We apply the following transform to compensate for painting into a surface, and then apply the offset so that the painting fits in the target rect.
    TransformationMatrix transform;
    transform.translate(options.offset.width(), options.offset.height());
    transform.multiply(options.transform);
    transform.multiply(m_currentTransform.combined());

    float opacity = options.opacity;
    RefPtr<BitmapTexture> mask = options.mask;

    if (m_state.solidColor.isValid() && !m_state.contentsRect.isEmpty()) {
        options.textureMapper->drawSolidColor(m_state.contentsRect, transform, blendWithOpacity(m_state.solidColor, opacity));
        if (m_state.showDebugBorders)
            options.textureMapper->drawBorder(m_state.debugBorderColor, m_state.debugBorderWidth, layerRect(), transform);
        return;
    }

    if (m_backingStore) {
        ASSERT(m_state.drawsContent && m_state.contentsVisible && !m_state.size.isEmpty());
        ASSERT(!layerRect().isEmpty());
        m_backingStore->paintToTextureMapper(options.textureMapper, layerRect(), transform, opacity, mask.get());
        if (m_state.showDebugBorders)
            m_backingStore->drawBorder(options.textureMapper, m_state.debugBorderColor, m_state.debugBorderWidth, layerRect(), transform);
        // Only draw repaint count for the main backing store.
        if (m_state.showRepaintCounter)
            m_backingStore->drawRepaintCounter(options.textureMapper, m_state.repaintCount, m_state.debugBorderColor, layerRect(), transform);
    }

    if (m_contentsLayer) {
        ASSERT(!layerRect().isEmpty());
        m_contentsLayer->paintToTextureMapper(options.textureMapper, m_state.contentsRect, transform, opacity, mask.get());
        if (m_state.showDebugBorders)
            m_contentsLayer->drawBorder(options.textureMapper, m_state.debugBorderColor, m_state.debugBorderWidth, m_state.contentsRect, transform);
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
        options.textureMapper->beginClip(TransformationMatrix(options.transform).multiply(m_currentTransform.combined()), layerRect());

    for (size_t i = 0; i < m_children.size(); ++i)
        m_children[i]->paintRecursive(options);

    if (shouldClip)
        options.textureMapper->endClip();
}

IntRect TextureMapperLayer::intermediateSurfaceRect()
{
    // FIXME: Add an inverse transform to LayerTransform.
    return intermediateSurfaceRect(m_currentTransform.combined().inverse());
}

IntRect TextureMapperLayer::intermediateSurfaceRect(const TransformationMatrix& matrix)
{
    IntRect rect;
    TransformationMatrix localTransform = TransformationMatrix(matrix).multiply(m_currentTransform.combined());
    rect = enclosingIntRect(localTransform.mapRect(layerRect()));
    if (!m_state.masksToBounds && !m_state.maskLayer) {
        for (size_t i = 0; i < m_children.size(); ++i)
            rect.unite(m_children[i]->intermediateSurfaceRect(matrix));
    }

#if ENABLE(CSS_FILTERS)
    if (m_currentFilters.hasOutsets()) {
        FilterOutsets outsets = m_currentFilters.outsets();
        IntRect unfilteredTargetRect(rect);
        rect.move(std::max(0, -outsets.left()), std::max(0, -outsets.top()));
        rect.expand(outsets.left() + outsets.right(), outsets.top() + outsets.bottom());
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
    if (m_currentFilters.size())
        return true;
#endif
    bool hasOpacity = m_currentOpacity < 0.99;
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
    if (m_state.size.isEmpty() && (m_state.masksToBounds || m_state.maskLayer || m_children.isEmpty()))
        return false;
    if (!m_state.visible && m_children.isEmpty())
        return false;
    if (!m_state.contentsVisible && m_children.isEmpty())
        return false;
    if (m_currentOpacity < 0.01)
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
            .multiply(m_state.replicaLayer->m_currentTransform.combined())
            .multiply(m_currentTransform.combined().inverse());
        paintSelfAndChildren(replicaOptions);
    }

    paintSelfAndChildren(options);
}

void TextureMapperLayer::setAnimatedTransform(const TransformationMatrix& matrix)
{
    m_currentTransform.setLocalTransform(matrix);
}

void TextureMapperLayer::setAnimatedOpacity(float opacity)
{
    m_currentOpacity = opacity;
}

#if ENABLE(CSS_FILTERS)
void TextureMapperLayer::setAnimatedFilters(const FilterOperations& filters)
{
    m_currentFilters = filters;
}

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

    float opacity = options.opacity * m_currentOpacity;
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

    paintOptions.transform = m_currentTransform.combined().inverse();
    paintOptions.offset = -IntSize(surfaceRect.x(), surfaceRect.y());

    paintSelfAndChildrenWithReplica(paintOptions);

    // If we painted the replica, the mask is already applied so we don't need to paint it again.
    if (m_state.replicaLayer)
        maskTexture = 0;

#if ENABLE(CSS_FILTERS)
    surface = applyFilters(m_currentFilters, options.textureMapper, surface.get(), surfaceRect);
#endif

    options.textureMapper->bindSurface(options.surface.get());
    TransformationMatrix targetTransform;
    targetTransform.translate(options.offset.width(), options.offset.height());
    targetTransform.multiply(options.transform);
    targetTransform.multiply(m_currentTransform.combined());

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

void TextureMapperLayer::setChildren(const Vector<TextureMapperLayer*>& newChildren)
{
    removeAllChildren();
    for (size_t i = 0; i < newChildren.size(); ++i)
        addChild(newChildren[i]);
}

void TextureMapperLayer::addChild(TextureMapperLayer* childLayer)
{
    ASSERT(childLayer != this);

    if (childLayer->m_parent)
        childLayer->removeFromParent();

    childLayer->m_parent = this;
    m_children.append(childLayer);
}

void TextureMapperLayer::removeFromParent()
{
    if (m_parent) {
        unsigned i;
        for (i = 0; i < m_parent->m_children.size(); i++) {
            if (this == m_parent->m_children[i]) {
                m_parent->m_children.remove(i);
                break;
            }
        }

        m_parent = 0;
    }
}

void TextureMapperLayer::removeAllChildren()
{
    while (m_children.size()) {
        TextureMapperLayer* curLayer = m_children[0];
        ASSERT(curLayer->m_parent);
        curLayer->removeFromParent();
    }
}

void TextureMapperLayer::setMaskLayer(TextureMapperLayer* maskLayer)
{
    if (maskLayer)
        maskLayer->m_effectTarget = this;
    m_state.maskLayer = maskLayer;
}

void TextureMapperLayer::setReplicaLayer(TextureMapperLayer* replicaLayer)
{
    if (replicaLayer)
        replicaLayer->m_effectTarget = this;
    m_state.replicaLayer = replicaLayer;
}

void TextureMapperLayer::setPosition(const FloatPoint& position)
{
    m_state.pos = position;
    m_currentTransform.setPosition(adjustedPosition());
}

void TextureMapperLayer::setSize(const FloatSize& size)
{
    m_state.size = size;
    m_currentTransform.setSize(size);
}

void TextureMapperLayer::setAnchorPoint(const FloatPoint3D& anchorPoint)
{
    m_state.anchorPoint = anchorPoint;
    m_currentTransform.setAnchorPoint(anchorPoint);
}

void TextureMapperLayer::setPreserves3D(bool preserves3D)
{
    m_state.preserves3D = preserves3D;
    m_currentTransform.setFlattening(!preserves3D);
}

void TextureMapperLayer::setTransform(const TransformationMatrix& transform)
{
    m_state.transform = transform;
    m_currentTransform.setLocalTransform(transform);
}

void TextureMapperLayer::setChildrenTransform(const TransformationMatrix& childrenTransform)
{
    m_state.childrenTransform = childrenTransform;
    m_currentTransform.setChildrenTransform(childrenTransform);
}

void TextureMapperLayer::setContentsRect(const IntRect& contentsRect)
{
    m_state.contentsRect = contentsRect;
}

void TextureMapperLayer::setMasksToBounds(bool masksToBounds)
{
    m_state.masksToBounds = masksToBounds;
}

void TextureMapperLayer::setDrawsContent(bool drawsContent)
{
    m_state.drawsContent = drawsContent;
}

void TextureMapperLayer::setContentsVisible(bool contentsVisible)
{
    m_state.contentsVisible = contentsVisible;
}

void TextureMapperLayer::setContentsOpaque(bool contentsOpaque)
{
    m_state.contentsOpaque = contentsOpaque;
}

void TextureMapperLayer::setBackfaceVisibility(bool backfaceVisibility)
{
    m_state.backfaceVisibility = backfaceVisibility;
}

void TextureMapperLayer::setOpacity(float opacity)
{
    m_state.opacity = opacity;
}

void TextureMapperLayer::setSolidColor(const Color& color)
{
    m_state.solidColor = color;
}

#if ENABLE(CSS_FILTERS)
void TextureMapperLayer::setFilters(const FilterOperations& filters)
{
    m_state.filters = filters;
}
#endif

void TextureMapperLayer::setDebugVisuals(bool showDebugBorders, const Color& debugBorderColor, float debugBorderWidth, bool showRepaintCounter)
{
    m_state.showDebugBorders = showDebugBorders;
    m_state.debugBorderColor = debugBorderColor;
    m_state.debugBorderWidth = debugBorderWidth;
    m_state.showRepaintCounter = showRepaintCounter;
}

void TextureMapperLayer::setRepaintCount(int repaintCount)
{
    m_state.repaintCount = repaintCount;
}

void TextureMapperLayer::setContentsLayer(TextureMapperPlatformLayer* platformLayer)
{
    m_contentsLayer = platformLayer;
}

void TextureMapperLayer::setAnimations(const GraphicsLayerAnimations& animations)
{
    m_animations = animations;
}

void TextureMapperLayer::setFixedToViewport(bool fixedToViewport)
{
    m_fixedToViewport = fixedToViewport;
}

void TextureMapperLayer::setBackingStore(PassRefPtr<TextureMapperBackingStore> backingStore)
{
    m_backingStore = backingStore;
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
        m_currentTransform.setLocalTransform(m_state.transform);
    if (!m_animations.hasActiveAnimationsOfType(AnimatedPropertyOpacity))
        m_currentOpacity = m_state.opacity;

#if ENABLE(CSS_FILTERS)
    if (!m_animations.hasActiveAnimationsOfType(AnimatedPropertyWebkitFilter))
        m_currentFilters = m_state.filters;
#endif
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
    m_currentTransform.setPosition(adjustedPosition());
}

}
#endif
