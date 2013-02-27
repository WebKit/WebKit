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

#include "Region.h"

#if USE(ACCELERATED_COMPOSITING)

namespace WebCore {

class TextureMapperPaintOptions {
public:
    RefPtr<BitmapTexture> surface;
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
    if (m_state.solidColor.isValid() && !m_state.contentsRect.isEmpty()) {
        options.textureMapper->drawSolidColor(m_state.contentsRect, transform, blendWithOpacity(m_state.solidColor, opacity));
        if (m_state.showDebugBorders)
            options.textureMapper->drawBorder(m_state.debugBorderColor, m_state.debugBorderWidth, layerRect(), transform);
        return;
    }

    if (m_backingStore) {
        ASSERT(m_state.drawsContent && m_state.contentsVisible && !m_state.size.isEmpty());
        ASSERT(!layerRect().isEmpty());
        m_backingStore->paintToTextureMapper(options.textureMapper, layerRect(), transform, opacity);
        if (m_state.showDebugBorders)
            m_backingStore->drawBorder(options.textureMapper, m_state.debugBorderColor, m_state.debugBorderWidth, layerRect(), transform);
        // Only draw repaint count for the main backing store.
        if (m_state.showRepaintCounter)
            m_backingStore->drawRepaintCounter(options.textureMapper, m_state.repaintCount, m_state.debugBorderColor, layerRect(), transform);
    }

    if (m_contentsLayer) {
        ASSERT(!layerRect().isEmpty());
        m_contentsLayer->paintToTextureMapper(options.textureMapper, m_state.contentsRect, transform, opacity);
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

bool TextureMapperLayer::shouldBlend() const
{
    if (m_state.preserves3D)
        return false;

#if ENABLE(CSS_FILTERS)
    if (m_currentFilters.size())
        return true;
#endif
    return m_currentOpacity < 1 || m_state.maskLayer || (m_state.replicaLayer && m_state.replicaLayer->m_state.maskLayer);
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

TransformationMatrix TextureMapperLayer::replicaTransform()
{
    return TransformationMatrix(m_state.replicaLayer->m_currentTransform.combined()).multiply(m_currentTransform.combined().inverse());
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

static PassRefPtr<BitmapTexture> applyFilters(const FilterOperations& filters, TextureMapper* textureMapper, BitmapTexture* source)
{
    if (!filters.size())
        return source;

    RefPtr<BitmapTexture> filterSurface = shouldKeepContentTexture(filters) ? textureMapper->acquireTextureFromPool(source->size()) : source;
    return filterSurface->applyFilters(textureMapper, *source, filters);
}
#endif

static void resolveOverlaps(Region newRegion, Region& overlapRegion, Region& nonOverlapRegion)
{
    Region newOverlapRegion(newRegion);
    newOverlapRegion.intersect(nonOverlapRegion);
    nonOverlapRegion.subtract(newOverlapRegion);
    overlapRegion.unite(newOverlapRegion);
    newRegion.subtract(overlapRegion);
    nonOverlapRegion.unite(newRegion);
}

void TextureMapperLayer::computeOverlapRegions(Region& overlapRegion, Region& nonOverlapRegion, bool alwaysResolveSelfOverlap)
{
    if (!m_state.visible || !m_state.contentsVisible)
        return;

    FloatRect boundingRect;
    if (m_backingStore || m_state.masksToBounds || m_state.maskLayer || hasFilters())
        boundingRect = layerRect();
    else if (m_contentsLayer || m_state.solidColor.alpha())
        boundingRect = m_state.contentsRect;

#if ENABLE(CSS_FILTERS)
    if (m_currentFilters.hasOutsets()) {
        FilterOutsets outsets = m_currentFilters.outsets();
        IntRect unfilteredTargetRect(boundingRect);
        boundingRect.move(std::max(0, -outsets.left()), std::max(0, -outsets.top()));
        boundingRect.expand(outsets.left() + outsets.right(), outsets.top() + outsets.bottom());
        boundingRect.unite(unfilteredTargetRect);
    }
#endif

    TransformationMatrix replicaMatrix;
    if (m_state.replicaLayer) {
        replicaMatrix = replicaTransform();
        boundingRect.unite(replicaMatrix.mapRect(boundingRect));
    }

    boundingRect = m_currentTransform.combined().mapRect(boundingRect);

    // Count all masks and filters as overlap layers.
    if (hasFilters() || m_state.maskLayer || (m_state.replicaLayer && m_state.replicaLayer->m_state.maskLayer)) {
        Region newOverlapRegion(enclosingIntRect(boundingRect));
        nonOverlapRegion.subtract(newOverlapRegion);
        overlapRegion.unite(newOverlapRegion);
        return;
    }

    Region newOverlapRegion;
    Region newNonOverlapRegion(enclosingIntRect(boundingRect));

    if (!m_state.masksToBounds) {
        for (size_t i = 0; i < m_children.size(); ++i) {
            TextureMapperLayer* child = m_children[i];
            bool alwaysResolveSelfOverlapForChildren = false;
            child->computeOverlapRegions(newOverlapRegion, newNonOverlapRegion, alwaysResolveSelfOverlapForChildren);
        }
    }

    if (m_state.replicaLayer) {
        newOverlapRegion.unite(replicaMatrix.mapRect(newOverlapRegion.bounds()));
        Region replicaRegion(replicaMatrix.mapRect(newNonOverlapRegion.bounds()));
        resolveOverlaps(replicaRegion, newOverlapRegion, newNonOverlapRegion);
    }

    if (!alwaysResolveSelfOverlap && shouldBlend()) {
        newNonOverlapRegion.unite(newOverlapRegion);
        newOverlapRegion = Region();
    }

    overlapRegion.unite(newOverlapRegion);
    resolveOverlaps(newNonOverlapRegion, overlapRegion, nonOverlapRegion);
}

void TextureMapperLayer::paintUsingOverlapRegions(const TextureMapperPaintOptions& options)
{
    Region overlapRegion;
    Region nonOverlapRegion;
    computeOverlapRegions(overlapRegion, nonOverlapRegion);
    if (overlapRegion.isEmpty()) {
        paintSelfAndChildrenWithReplica(options);
        return;
    }

    Vector<IntRect> rects = nonOverlapRegion.rects();
    for (size_t i = 0; i < rects.size(); ++i) {
        options.textureMapper->beginClip(TransformationMatrix(), rects[i]);
        paintSelfAndChildrenWithReplica(options);
        options.textureMapper->endClip();
    }

    rects = overlapRegion.rects();
    IntSize maxTextureSize = options.textureMapper->maxTextureSize();
    for (size_t i = 0; i < rects.size(); ++i) {
        IntRect rect = rects[i];
        for (int x = rect.x(); x < rect.maxX(); x += maxTextureSize.width()) {
            for (int y = rect.y(); y < rect.maxY(); y += maxTextureSize.height()) {
                IntRect tileRect(IntPoint(x, y), maxTextureSize);
                tileRect.intersect(rect);
                paintWithIntermediateSurface(options, tileRect);
            }
        }
    }
}

void TextureMapperLayer::applyMask(const TextureMapperPaintOptions& options)
{
    options.textureMapper->setMaskMode(true);
    paintSelf(options);
    options.textureMapper->setMaskMode(false);
}

PassRefPtr<BitmapTexture> TextureMapperLayer::paintIntoSurface(const TextureMapperPaintOptions& options, const IntSize& size)
{
    RefPtr<BitmapTexture> surface = options.textureMapper->acquireTextureFromPool(size);
    options.textureMapper->bindSurface(surface.get());
    paintSelfAndChildren(options);
    if (m_state.maskLayer)
        m_state.maskLayer->applyMask(options);
#if ENABLE(CSS_FILTERS)
    if (!m_currentFilters.isEmpty())
        surface = applyFilters(m_currentFilters, options.textureMapper, surface.get());
#endif
    options.textureMapper->bindSurface(surface.get());
    return surface;
}

static PassRefPtr<BitmapTexture> commitSurface(const TextureMapperPaintOptions& options, PassRefPtr<BitmapTexture> surface, const IntRect& rect, float opacity)
{
    options.textureMapper->bindSurface(options.surface.get());
    TransformationMatrix targetTransform;
    targetTransform.translate(options.offset.width(), options.offset.height());
    targetTransform.multiply(options.transform);
    options.textureMapper->drawTexture(*surface.get(), rect, targetTransform, opacity);
    return 0;
}

void TextureMapperLayer::paintWithIntermediateSurface(const TextureMapperPaintOptions& options, const IntRect& rect)
{
    RefPtr<BitmapTexture> replicaSurface;
    RefPtr<BitmapTexture> mainSurface;
    TextureMapperPaintOptions paintOptions(options);
    paintOptions.offset = -IntSize(rect.x(), rect.y());
    paintOptions.opacity = 1;
    paintOptions.transform = TransformationMatrix();
    if (m_state.replicaLayer) {
        paintOptions.transform = replicaTransform();
        replicaSurface = paintIntoSurface(paintOptions, rect.size());
        paintOptions.transform = TransformationMatrix();
        if (m_state.replicaLayer->m_state.maskLayer)
            m_state.replicaLayer->m_state.maskLayer->applyMask(paintOptions);
    }

    if (replicaSurface && options.opacity == 1)
        replicaSurface = commitSurface(options, replicaSurface, rect, 1);

    mainSurface = paintIntoSurface(paintOptions, rect.size());
    if (replicaSurface) {
        options.textureMapper->bindSurface(replicaSurface.get());
        options.textureMapper->drawTexture(*mainSurface.get(), FloatRect(FloatPoint::zero(), rect.size()));
        mainSurface = replicaSurface;
    }

    commitSurface(options, mainSurface, rect, options.opacity);
}

void TextureMapperLayer::paintRecursive(const TextureMapperPaintOptions& options)
{
    if (!isVisible())
        return;

    if (!shouldBlend()) {
        paintSelfAndChildrenWithReplica(options);
        return;
    }

    TextureMapperPaintOptions paintOptions(options);
    paintOptions.opacity = options.opacity * m_currentOpacity;
    paintUsingOverlapRegions(paintOptions);
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
