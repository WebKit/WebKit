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

#include "FloatQuad.h"
#include "GraphicsLayerTextureMapper.h"
#include "Region.h"
#include <wtf/MathExtras.h>

namespace WebCore {

class TextureMapperPaintOptions {
public:
    TextureMapperPaintOptions(TextureMapper& textureMapper)
        : textureMapper(textureMapper)
    { }

    TextureMapper& textureMapper;
    TransformationMatrix transform;
    RefPtr<BitmapTexture> surface;
    float opacity { 1 };
    IntSize offset;
};

TextureMapperLayer::TextureMapperLayer() = default;

TextureMapperLayer::~TextureMapperLayer()
{
    for (auto* child : m_children)
        child->m_parent = nullptr;

    removeFromParent();
}

void TextureMapperLayer::computeTransformsRecursive()
{
    if (m_state.size.isEmpty() && m_state.masksToBounds)
        return;

    // Compute transforms recursively on the way down to leafs.
    {
        TransformationMatrix parentTransform;
        if (m_parent)
            parentTransform = m_parent->m_layerTransforms.combinedForChildren;
        else if (m_effectTarget)
            parentTransform = m_effectTarget->m_layerTransforms.combined;

        const float originX = m_state.anchorPoint.x() * m_state.size.width();
        const float originY = m_state.anchorPoint.y() * m_state.size.height();

        m_layerTransforms.combined = parentTransform;
        m_layerTransforms.combined
            .translate3d(originX + m_state.pos.x(), originY + m_state.pos.y(), m_state.anchorPoint.z())
            .multiply(m_layerTransforms.localTransform);

        m_layerTransforms.combinedForChildren = m_layerTransforms.combined;
        m_layerTransforms.combined.translate3d(-originX, -originY, -m_state.anchorPoint.z());

        if (!m_state.preserves3D)
            m_layerTransforms.combinedForChildren = m_layerTransforms.combinedForChildren.to2dTransform();
        m_layerTransforms.combinedForChildren.multiply(m_state.childrenTransform);
        m_layerTransforms.combinedForChildren.translate3d(-originX, -originY, -m_state.anchorPoint.z());
    }

    m_state.visible = m_state.backfaceVisibility || !m_layerTransforms.combined.isBackFaceVisible();

    if (m_parent && m_parent->m_state.preserves3D)
        m_centerZ = m_layerTransforms.combined.mapPoint(FloatPoint3D(m_state.size.width() / 2, m_state.size.height() / 2, 0)).z();

    if (m_state.maskLayer)
        m_state.maskLayer->computeTransformsRecursive();
    if (m_state.replicaLayer)
        m_state.replicaLayer->computeTransformsRecursive();
    for (auto* child : m_children) {
        ASSERT(child->m_parent == this);
        child->computeTransformsRecursive();
    }

    // Reorder children if needed on the way back up.
    if (m_state.preserves3D)
        sortByZOrder(m_children);
}

void TextureMapperLayer::paint()
{
    computeTransformsRecursive();

    ASSERT(m_textureMapper);
    TextureMapperPaintOptions options(*m_textureMapper);
    options.textureMapper.bindSurface(0);

    paintRecursive(options);
}

static Color blendWithOpacity(const Color& color, float opacity)
{
    if (color.isOpaque() && opacity == 1.)
        return color;

    return color.colorWithAlphaMultipliedBy(opacity);
}

void TextureMapperLayer::paintSelf(const TextureMapperPaintOptions& options)
{
    if (!m_state.visible || !m_state.contentsVisible)
        return;

    // We apply the following transform to compensate for painting into a surface, and then apply the offset so that the painting fits in the target rect.
    TransformationMatrix transform;
    transform.translate(options.offset.width(), options.offset.height());
    transform.multiply(options.transform);
    transform.multiply(m_layerTransforms.combined);

    if (m_state.solidColor.isValid() && !m_state.contentsRect.isEmpty() && m_state.solidColor.isVisible()) {
        options.textureMapper.drawSolidColor(m_state.contentsRect, transform, blendWithOpacity(m_state.solidColor, options.opacity), true);
        if (m_state.showDebugBorders)
            options.textureMapper.drawBorder(m_state.debugBorderColor, m_state.debugBorderWidth, layerRect(), transform);
        return;
    }

    options.textureMapper.setWrapMode(TextureMapper::StretchWrap);
    options.textureMapper.setPatternTransform(TransformationMatrix());

    if (m_backingStore) {
        FloatRect targetRect = layerRect();
        ASSERT(!targetRect.isEmpty());
        m_backingStore->paintToTextureMapper(options.textureMapper, targetRect, transform, options.opacity);
        if (m_state.showDebugBorders)
            m_backingStore->drawBorder(options.textureMapper, m_state.debugBorderColor, m_state.debugBorderWidth, targetRect, transform);
        // Only draw repaint count for the main backing store.
        if (m_state.showRepaintCounter)
            m_backingStore->drawRepaintCounter(options.textureMapper, m_state.repaintCount, m_state.debugBorderColor, targetRect, transform);
    }

    if (!m_contentsLayer)
        return;

    if (!m_state.contentsTileSize.isEmpty()) {
        options.textureMapper.setWrapMode(TextureMapper::RepeatWrap);

        auto patternTransform = TransformationMatrix::rectToRect({ { }, m_state.contentsTileSize }, { { }, m_state.contentsRect.size() })
            .translate(m_state.contentsTilePhase.width() / m_state.contentsRect.width(), m_state.contentsTilePhase.height() / m_state.contentsRect.height());
        options.textureMapper.setPatternTransform(patternTransform);
    }

    ASSERT(!layerRect().isEmpty());
    m_contentsLayer->paintToTextureMapper(options.textureMapper, m_state.contentsRect, transform, options.opacity);
    if (m_state.showDebugBorders)
        m_contentsLayer->drawBorder(options.textureMapper, m_state.debugBorderColor, m_state.debugBorderWidth, m_state.contentsRect, transform);
}

void TextureMapperLayer::sortByZOrder(Vector<TextureMapperLayer* >& array)
{
    std::sort(array.begin(), array.end(),
        [](TextureMapperLayer* a, TextureMapperLayer* b) {
            return a->m_centerZ < b->m_centerZ;
        });
}

void TextureMapperLayer::paintSelfAndChildren(const TextureMapperPaintOptions& options)
{
    paintSelf(options);

    if (m_children.isEmpty())
        return;

    bool shouldClip = m_state.masksToBounds && !m_state.preserves3D;
    if (shouldClip) {
        TransformationMatrix clipTransform;
        clipTransform.translate(options.offset.width(), options.offset.height());
        clipTransform.multiply(options.transform);
        clipTransform.multiply(m_layerTransforms.combined);
        options.textureMapper.beginClip(clipTransform, layerRect());

        // If as a result of beginClip(), the clipping area is empty, it means that the intersection of the previous
        // clipping area and the current one don't have any pixels in common. In this case we can skip painting the
        // children as they will be clipped out (see https://bugs.webkit.org/show_bug.cgi?id=181080).
        if (options.textureMapper.clipBounds().isEmpty()) {
            options.textureMapper.endClip();
            return;
        }
    }

    for (auto* child : m_children)
        child->paintRecursive(options);

    if (shouldClip)
        options.textureMapper.endClip();
}

bool TextureMapperLayer::shouldBlend() const
{
    if (m_state.preserves3D)
        return false;

    return m_currentOpacity < 1
        || hasFilters()
        || m_state.maskLayer
        || (m_state.replicaLayer && m_state.replicaLayer->m_state.maskLayer);
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
            .multiply(m_state.replicaLayer->m_layerTransforms.combined)
            .multiply(m_layerTransforms.combined.inverse().valueOr(TransformationMatrix()));
        paintSelfAndChildren(replicaOptions);
    }

    paintSelfAndChildren(options);
}

TransformationMatrix TextureMapperLayer::replicaTransform()
{
    return TransformationMatrix(m_state.replicaLayer->m_layerTransforms.combined)
        .multiply(m_layerTransforms.combined.inverse().valueOr(TransformationMatrix()));
}

static void resolveOverlaps(Region& newRegion, Region& overlapRegion, Region& nonOverlapRegion)
{
    Region newOverlapRegion(newRegion);
    newOverlapRegion.intersect(nonOverlapRegion);
    nonOverlapRegion.subtract(newOverlapRegion);
    overlapRegion.unite(newOverlapRegion);
    newRegion.subtract(overlapRegion);
    nonOverlapRegion.unite(newRegion);
}

void TextureMapperLayer::computeOverlapRegions(Region& overlapRegion, Region& nonOverlapRegion, ResolveSelfOverlapMode mode)
{
    if (!m_state.visible || !m_state.contentsVisible)
        return;

    FloatRect boundingRect;
    if (m_backingStore || m_state.masksToBounds || m_state.maskLayer || hasFilters())
        boundingRect = layerRect();
    else if (m_contentsLayer || m_state.solidColor.isVisible())
        boundingRect = m_state.contentsRect;

    if (m_currentFilters.hasOutsets()) {
        FilterOutsets outsets = m_currentFilters.outsets();
        IntRect unfilteredTargetRect(boundingRect);
        boundingRect.move(std::max(0, -outsets.left()), std::max(0, -outsets.top()));
        boundingRect.expand(outsets.left() + outsets.right(), outsets.top() + outsets.bottom());
        boundingRect.unite(unfilteredTargetRect);
    }

    TransformationMatrix replicaMatrix;
    if (m_state.replicaLayer) {
        replicaMatrix = replicaTransform();
        boundingRect.unite(replicaMatrix.mapRect(boundingRect));
    }

    boundingRect = m_layerTransforms.combined.mapRect(boundingRect);

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
        for (auto* child : m_children)
            child->computeOverlapRegions(newOverlapRegion, newNonOverlapRegion, ResolveSelfOverlapIfNeeded);
    }

    if (m_state.replicaLayer) {
        newOverlapRegion.unite(replicaMatrix.mapRect(newOverlapRegion.bounds()));
        Region replicaRegion(replicaMatrix.mapRect(newNonOverlapRegion.bounds()));
        resolveOverlaps(replicaRegion, newOverlapRegion, newNonOverlapRegion);
    }

    if ((mode != ResolveSelfOverlapAlways) && shouldBlend()) {
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
    computeOverlapRegions(overlapRegion, nonOverlapRegion, ResolveSelfOverlapAlways);
    if (overlapRegion.isEmpty()) {
        paintSelfAndChildrenWithReplica(options);
        return;
    }

    // Having both overlap and non-overlap regions carries some overhead. Avoid it if the overlap area
    // is big anyway.
    if (overlapRegion.bounds().size().area() > nonOverlapRegion.bounds().size().area()) {
        overlapRegion.unite(nonOverlapRegion);
        nonOverlapRegion = Region();
    }

    nonOverlapRegion.translate(options.offset);
    Vector<IntRect> rects = nonOverlapRegion.rects();

    for (auto& rect : rects) {
        if (!rect.intersects(options.textureMapper.clipBounds()))
            continue;

        options.textureMapper.beginClip(TransformationMatrix(), rect);
        paintSelfAndChildrenWithReplica(options);
        options.textureMapper.endClip();
    }

    rects = overlapRegion.rects();
    static const size_t OverlapRegionConsolidationThreshold = 4;
    if (nonOverlapRegion.isEmpty() && rects.size() > OverlapRegionConsolidationThreshold) {
        rects.clear();
        rects.append(overlapRegion.bounds());
    }

    IntSize maxTextureSize = options.textureMapper.maxTextureSize();
    IntRect adjustedClipBounds(options.textureMapper.clipBounds());
    adjustedClipBounds.move(-options.offset);
    for (auto& rect : rects) {
        for (int x = rect.x(); x < rect.maxX(); x += maxTextureSize.width()) {
            for (int y = rect.y(); y < rect.maxY(); y += maxTextureSize.height()) {
                IntRect tileRect(IntPoint(x, y), maxTextureSize);
                tileRect.intersect(rect);
                if (!tileRect.intersects(adjustedClipBounds))
                    continue;

                paintWithIntermediateSurface(options, tileRect);
            }
        }
    }
}

void TextureMapperLayer::applyMask(const TextureMapperPaintOptions& options)
{
    options.textureMapper.setMaskMode(true);
    paintSelf(options);
    options.textureMapper.setMaskMode(false);
}

RefPtr<BitmapTexture> TextureMapperLayer::paintIntoSurface(const TextureMapperPaintOptions& options, const IntSize& size)
{
    RefPtr<BitmapTexture> surface = options.textureMapper.acquireTextureFromPool(size, BitmapTexture::SupportsAlpha);
    TextureMapperPaintOptions paintOptions(options);
    paintOptions.surface = surface;
    options.textureMapper.bindSurface(surface.get());
    paintSelfAndChildren(paintOptions);
    if (m_state.maskLayer)
        m_state.maskLayer->applyMask(options);
    surface = surface->applyFilters(options.textureMapper, m_currentFilters);
    options.textureMapper.bindSurface(surface.get());
    return surface;
}

static void commitSurface(const TextureMapperPaintOptions& options, BitmapTexture& surface, const IntRect& rect, float opacity)
{
    options.textureMapper.bindSurface(options.surface.get());
    TransformationMatrix targetTransform;
    targetTransform.translate(options.offset.width(), options.offset.height());
    targetTransform.multiply(options.transform);
    options.textureMapper.drawTexture(surface, rect, targetTransform, opacity);
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

    if (replicaSurface && options.opacity == 1) {
        commitSurface(options, *replicaSurface, rect, 1);
        replicaSurface = nullptr;
    }

    mainSurface = paintIntoSurface(paintOptions, rect.size());
    if (replicaSurface) {
        options.textureMapper.bindSurface(replicaSurface.get());
        options.textureMapper.drawTexture(*mainSurface.get(), FloatRect(FloatPoint::zero(), rect.size()));
        mainSurface = replicaSurface;
    }

    commitSurface(options, *mainSurface, rect, options.opacity);
}

void TextureMapperLayer::paintRecursive(const TextureMapperPaintOptions& options)
{
    if (!isVisible())
        return;

    TextureMapperPaintOptions paintOptions(options);
    paintOptions.opacity *= m_currentOpacity;

    if (!shouldBlend()) {
        paintSelfAndChildrenWithReplica(paintOptions);
        return;
    }

    paintUsingOverlapRegions(paintOptions);
}

#if !USE(COORDINATED_GRAPHICS)
void TextureMapperLayer::setChildren(const Vector<GraphicsLayer*>& newChildren)
{
    removeAllChildren();
    for (auto* child : newChildren)
        addChild(&downcast<GraphicsLayerTextureMapper>(child)->layer());
}
#endif

void TextureMapperLayer::setChildren(const Vector<TextureMapperLayer*>& newChildren)
{
    removeAllChildren();
    for (auto* child : newChildren)
        addChild(child);
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
        size_t index = m_parent->m_children.find(this);
        ASSERT(index != notFound);
        m_parent->m_children.remove(index);
    }

    m_parent = nullptr;
}

void TextureMapperLayer::removeAllChildren()
{
    auto oldChildren = WTFMove(m_children);
    for (auto* child : oldChildren)
        child->m_parent = nullptr;
}

void TextureMapperLayer::setMaskLayer(TextureMapperLayer* maskLayer)
{
    if (maskLayer) {
        maskLayer->m_effectTarget = makeWeakPtr(*this);
        m_state.maskLayer = makeWeakPtr(*maskLayer);
    } else
        m_state.maskLayer = nullptr;
}

void TextureMapperLayer::setReplicaLayer(TextureMapperLayer* replicaLayer)
{
    if (replicaLayer) {
        replicaLayer->m_effectTarget = makeWeakPtr(*this);
        m_state.replicaLayer = makeWeakPtr(*replicaLayer);
    } else
        m_state.replicaLayer = nullptr;
}

void TextureMapperLayer::setPosition(const FloatPoint& position)
{
    m_state.pos = position;
}

void TextureMapperLayer::setSize(const FloatSize& size)
{
    m_state.size = size;
}

void TextureMapperLayer::setAnchorPoint(const FloatPoint3D& anchorPoint)
{
    m_state.anchorPoint = anchorPoint;
}

void TextureMapperLayer::setPreserves3D(bool preserves3D)
{
    m_state.preserves3D = preserves3D;
}

void TextureMapperLayer::setTransform(const TransformationMatrix& transform)
{
    m_state.transform = transform;
}

void TextureMapperLayer::setChildrenTransform(const TransformationMatrix& childrenTransform)
{
    m_state.childrenTransform = childrenTransform;
}

void TextureMapperLayer::setContentsRect(const FloatRect& contentsRect)
{
    m_state.contentsRect = contentsRect;
}

void TextureMapperLayer::setContentsTileSize(const FloatSize& size)
{
    m_state.contentsTileSize = size;
}

void TextureMapperLayer::setContentsTilePhase(const FloatSize& phase)
{
    m_state.contentsTilePhase = phase;
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

void TextureMapperLayer::setFilters(const FilterOperations& filters)
{
    m_state.filters = filters;
}

void TextureMapperLayer::setDebugVisuals(bool showDebugBorders, const Color& debugBorderColor, float debugBorderWidth)
{
    m_state.showDebugBorders = showDebugBorders;
    m_state.debugBorderColor = debugBorderColor;
    m_state.debugBorderWidth = debugBorderWidth;
}

void TextureMapperLayer::setRepaintCounter(bool showRepaintCounter, int repaintCount)
{
    m_state.showRepaintCounter = showRepaintCounter;
    m_state.repaintCount = repaintCount;
}

void TextureMapperLayer::setContentsLayer(TextureMapperPlatformLayer* platformLayer)
{
    m_contentsLayer = platformLayer;
}

void TextureMapperLayer::setAnimations(const TextureMapperAnimations& animations)
{
    m_animations = animations;
}

void TextureMapperLayer::setBackingStore(TextureMapperBackingStore* backingStore)
{
    m_backingStore = backingStore;
}

bool TextureMapperLayer::descendantsOrSelfHaveRunningAnimations() const
{
    if (m_animations.hasRunningAnimations())
        return true;

    return std::any_of(m_children.begin(), m_children.end(),
        [](TextureMapperLayer* child) {
            return child->descendantsOrSelfHaveRunningAnimations();
        });
}

bool TextureMapperLayer::applyAnimationsRecursively(MonotonicTime time)
{
    bool hasRunningAnimations = syncAnimations(time);
    for (auto* child : m_children)
        hasRunningAnimations |= child->applyAnimationsRecursively(time);
    return hasRunningAnimations;
}

bool TextureMapperLayer::syncAnimations(MonotonicTime time)
{
    TextureMapperAnimation::ApplicationResult applicationResults;
    m_animations.apply(applicationResults, time);

    m_layerTransforms.localTransform = applicationResults.transform.valueOr(m_state.transform);
    m_currentOpacity = applicationResults.opacity.valueOr(m_state.opacity);
    m_currentFilters = applicationResults.filters.valueOr(m_state.filters);

    return applicationResults.hasRunningAnimations;
}

}
