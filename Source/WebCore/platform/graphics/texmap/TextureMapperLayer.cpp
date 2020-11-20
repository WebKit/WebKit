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
    TextureMapperLayer* backdropLayer { nullptr };
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
            .translate3d(originX + (m_state.pos.x() - m_state.boundsOrigin.x()), originY + (m_state.pos.y() - m_state.boundsOrigin.y()), m_state.anchorPoint.z())
            .multiply(m_layerTransforms.localTransform);

        m_layerTransforms.combinedForChildren = m_layerTransforms.combined;
        m_layerTransforms.combined.translate3d(-originX, -originY, -m_state.anchorPoint.z());

        if (m_isReplica)
            m_layerTransforms.combined.translate(-m_state.pos.x(), -m_state.pos.y());

        if (!m_state.preserves3D)
            m_layerTransforms.combinedForChildren = m_layerTransforms.combinedForChildren.to2dTransform();
        m_layerTransforms.combinedForChildren.multiply(m_state.childrenTransform);
        m_layerTransforms.combinedForChildren.translate3d(-originX, -originY, -m_state.anchorPoint.z());

#if USE(COORDINATED_GRAPHICS)
        // Compute transforms for the future as well.
        TransformationMatrix futureParentTransform;
        if (m_parent)
            futureParentTransform = m_parent->m_layerTransforms.futureCombinedForChildren;
        else if (m_effectTarget)
            futureParentTransform = m_effectTarget->m_layerTransforms.futureCombined;

        m_layerTransforms.futureCombined = futureParentTransform;
        m_layerTransforms.futureCombined
            .translate3d(originX + (m_state.pos.x() - m_state.boundsOrigin.x()), originY + (m_state.pos.y() - m_state.boundsOrigin.y()), m_state.anchorPoint.z())
            .multiply(m_layerTransforms.futureLocalTransform);

        m_layerTransforms.futureCombinedForChildren = m_layerTransforms.futureCombined;
        m_layerTransforms.futureCombined.translate3d(-originX, -originY, -m_state.anchorPoint.z());

        if (!m_state.preserves3D)
            m_layerTransforms.futureCombinedForChildren = m_layerTransforms.futureCombinedForChildren.to2dTransform();
        m_layerTransforms.futureCombinedForChildren.multiply(m_state.childrenTransform);
        m_layerTransforms.futureCombinedForChildren.translate3d(-originX, -originY, -m_state.anchorPoint.z());
#endif
    }

    m_state.visible = m_state.backfaceVisibility || !m_layerTransforms.combined.isBackFaceVisible();

    if (m_parent && m_parent->m_state.preserves3D)
        m_centerZ = m_layerTransforms.combined.mapPoint(FloatPoint3D(m_state.size.width() / 2, m_state.size.height() / 2, 0)).z();

    if (m_state.maskLayer)
        m_state.maskLayer->computeTransformsRecursive();
    if (m_state.replicaLayer)
        m_state.replicaLayer->computeTransformsRecursive();
    if (m_state.backdropLayer)
        m_state.backdropLayer->computeTransformsRecursive();
    for (auto* child : m_children) {
        ASSERT(child->m_parent == this);
        child->computeTransformsRecursive();
    }

    // Reorder children if needed on the way back up.
    if (m_state.preserves3D)
        sortByZOrder(m_children);

#if USE(COORDINATED_GRAPHICS)
    if (m_backingStore && m_animatedBackingStoreClient)
        m_animatedBackingStoreClient->requestBackingStoreUpdateIfNeeded(m_layerTransforms.futureCombined);
#endif
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

    bool shouldClip = m_state.contentsClippingRect.isRounded() || !m_state.contentsClippingRect.rect().contains(m_state.contentsRect);
    if (shouldClip) {
        options.textureMapper.beginClip(transform, m_state.contentsClippingRect);
    }

    m_contentsLayer->paintToTextureMapper(options.textureMapper, m_state.contentsRect, transform, options.opacity);

    if (shouldClip)
        options.textureMapper.endClip();

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
    if (m_state.backdropLayer && m_state.backdropLayer == options.backdropLayer)
        return;

    if (m_state.backdropLayer && !options.backdropLayer) {
        TransformationMatrix clipTransform;
        clipTransform.translate(options.offset.width(), options.offset.height());
        clipTransform.multiply(options.transform);
        clipTransform.multiply(m_layerTransforms.combined);
        options.textureMapper.beginClip(clipTransform, m_state.backdropFiltersRect);
        m_state.backdropLayer->paintRecursive(options);
        options.textureMapper.endClip();
    }

    paintSelf(options);

    if (m_children.isEmpty())
        return;

    bool shouldClip = m_state.masksToBounds && !m_state.preserves3D;
    if (shouldClip) {
        TransformationMatrix clipTransform;
        clipTransform.translate(options.offset.width(), options.offset.height());
        clipTransform.multiply(options.transform);
        clipTransform.multiply(m_layerTransforms.combined);
        options.textureMapper.beginClip(clipTransform, FloatRoundedRect(layerRect()));

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
        replicaOptions.transform.multiply(replicaTransform());
        paintSelfAndChildren(replicaOptions);
    }

    paintSelfAndChildren(options);
}

TransformationMatrix TextureMapperLayer::replicaTransform()
{
    return TransformationMatrix(m_state.replicaLayer->m_layerTransforms.combined)
        .multiply(m_layerTransforms.combined.inverse().valueOr(TransformationMatrix()));
}

static void resolveOverlaps(const IntRect& newRegion, Region& overlapRegion, Region& nonOverlapRegion)
{
    Region newOverlapRegion(newRegion);
    newOverlapRegion.intersect(nonOverlapRegion);
    nonOverlapRegion.subtract(newOverlapRegion);
    overlapRegion.unite(newOverlapRegion);

    Region newNonOverlapRegion(newRegion);
    newNonOverlapRegion.subtract(overlapRegion);
    nonOverlapRegion.unite(newNonOverlapRegion);
}

void TextureMapperLayer::computeOverlapRegions(ComputeOverlapRegionData& data, const TransformationMatrix& accumulatedReplicaTransform, bool includesReplica)
{
    if (!m_state.visible || !m_state.contentsVisible)
        return;

    FloatRect localBoundingRect;
    if (m_backingStore || m_state.masksToBounds || m_state.maskLayer || hasFilters())
        localBoundingRect = layerRect();
    else if (m_contentsLayer || m_state.solidColor.isVisible())
        localBoundingRect = m_state.contentsRect;

    if (m_currentFilters.hasOutsets() && !m_state.backdropLayer) {
        auto outsets = m_currentFilters.outsets();
        localBoundingRect.move(-outsets.left(), -outsets.top());
        localBoundingRect.expand(outsets.left() + outsets.right(), outsets.top() + outsets.bottom());
    }

    TransformationMatrix transform(accumulatedReplicaTransform);
    transform.multiply(m_layerTransforms.combined);

    IntRect viewportBoundingRect = enclosingIntRect(transform.mapRect(localBoundingRect));
    viewportBoundingRect.intersect(data.clipBounds);

    switch (data.mode) {
    case ComputeOverlapRegionMode::Intersection:
        resolveOverlaps(viewportBoundingRect, data.overlapRegion, data.nonOverlapRegion);
        break;
    case ComputeOverlapRegionMode::Union:
        data.overlapRegion.unite(viewportBoundingRect);
        break;
    }

    if (m_state.replicaLayer && includesReplica) {
        TransformationMatrix newReplicaTransform(accumulatedReplicaTransform);
        newReplicaTransform.multiply(replicaTransform());
        computeOverlapRegions(data, newReplicaTransform, false);
    }

    if (!m_state.masksToBounds) {
        for (auto* child : m_children)
            child->computeOverlapRegions(data, accumulatedReplicaTransform);
    }
}

void TextureMapperLayer::paintUsingOverlapRegions(const TextureMapperPaintOptions& options)
{
    Region overlapRegion;
    Region nonOverlapRegion;
    bool needsUnion = hasFilters() || m_state.maskLayer || (m_state.replicaLayer && m_state.replicaLayer->m_state.maskLayer);
    ComputeOverlapRegionData data {
        needsUnion ? ComputeOverlapRegionMode::Union : ComputeOverlapRegionMode::Intersection,
        options.textureMapper.clipBounds(),
        overlapRegion,
        nonOverlapRegion
    };
    data.clipBounds.move(-options.offset);
    computeOverlapRegions(data, options.transform);
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
    auto rects = nonOverlapRegion.rects();

    for (auto& rect : rects) {
        options.textureMapper.beginClip(TransformationMatrix(), FloatRoundedRect(rect));
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
    for (auto& rect : rects) {
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
    options.textureMapper.setMaskMode(true);
    paintSelf(options);
    options.textureMapper.setMaskMode(false);
}

void TextureMapperLayer::paintIntoSurface(TextureMapperPaintOptions& options)
{
    options.textureMapper.bindSurface(options.surface.get());
    if (m_isBackdrop) {
        TextureMapperPaintOptions paintOptions(options);
        paintOptions.transform = TransformationMatrix();
        paintOptions.backdropLayer = this;
        rootLayer().paintSelfAndChildren(paintOptions);
    } else
        paintSelfAndChildren(options);
    if (m_state.maskLayer)
        m_state.maskLayer->applyMask(options);
    options.surface = options.surface->applyFilters(options.textureMapper, m_currentFilters);
    options.textureMapper.bindSurface(options.surface.get());
}

static void commitSurface(const TextureMapperPaintOptions& options, BitmapTexture& surface, const IntRect& rect, float opacity)
{
    IntRect targetRect(rect);
    targetRect.move(options.offset);
    options.textureMapper.bindSurface(options.surface.get());
    options.textureMapper.drawTexture(surface, targetRect, { }, opacity);
}

void TextureMapperLayer::paintWithIntermediateSurface(const TextureMapperPaintOptions& options, const IntRect& rect)
{
    TextureMapperPaintOptions paintOptions(options);
    paintOptions.surface = options.textureMapper.acquireTextureFromPool(rect.size(), BitmapTexture::SupportsAlpha);
    paintOptions.offset = -toIntSize(rect.location());
    paintOptions.opacity = 1;
    if (m_state.replicaLayer) {
        paintOptions.transform.multiply(replicaTransform());
        paintIntoSurface(paintOptions);
        paintOptions.transform = options.transform;
        if (m_state.replicaLayer->m_state.maskLayer)
            m_state.replicaLayer->m_state.maskLayer->applyMask(paintOptions);
    }

    paintIntoSurface(paintOptions);

    commitSurface(options, *paintOptions.surface, rect, options.opacity);
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
        replicaLayer->m_isReplica = true;
        replicaLayer->m_effectTarget = makeWeakPtr(*this);
        m_state.replicaLayer = makeWeakPtr(*replicaLayer);
    } else
        m_state.replicaLayer = nullptr;
}

void TextureMapperLayer::setBackdropLayer(TextureMapperLayer* backdropLayer)
{
    if (backdropLayer) {
        backdropLayer->m_isBackdrop = true;
        backdropLayer->m_effectTarget = makeWeakPtr(*this);
        m_state.backdropLayer = makeWeakPtr(*backdropLayer);
    } else
        m_state.backdropLayer = nullptr;
}

void TextureMapperLayer::setBackdropFiltersRect(const FloatRoundedRect& backdropFiltersRect)
{
    m_state.backdropFiltersRect = backdropFiltersRect;
}

void TextureMapperLayer::setPosition(const FloatPoint& position)
{
    m_state.pos = position;
}

void TextureMapperLayer::setBoundsOrigin(const FloatPoint& boundsOrigin)
{
    m_state.boundsOrigin = boundsOrigin;
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

void TextureMapperLayer::setContentsClippingRect(const FloatRoundedRect& contentsClippingRect)
{
    m_state.contentsClippingRect = contentsClippingRect;
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

void TextureMapperLayer::setAnimations(const Nicosia::Animations& animations)
{
    m_animations = animations;
}

void TextureMapperLayer::setBackingStore(TextureMapperBackingStore* backingStore)
{
    m_backingStore = backingStore;
}

#if USE(COORDINATED_GRAPHICS)
void TextureMapperLayer::setAnimatedBackingStoreClient(Nicosia::AnimatedBackingStoreClient* client)
{
    m_animatedBackingStoreClient = client;
}
#endif

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
    if (m_state.replicaLayer)
        hasRunningAnimations |= m_state.replicaLayer->applyAnimationsRecursively(time);
    if (m_state.backdropLayer)
        hasRunningAnimations |= m_state.backdropLayer->syncAnimations(time);
    for (auto* child : m_children)
        hasRunningAnimations |= child->applyAnimationsRecursively(time);
    return hasRunningAnimations;
}

bool TextureMapperLayer::syncAnimations(MonotonicTime time)
{
    Nicosia::Animation::ApplicationResult applicationResults;
    m_animations.apply(applicationResults, time);

    m_layerTransforms.localTransform = applicationResults.transform.valueOr(m_state.transform);
    m_currentOpacity = applicationResults.opacity.valueOr(m_state.opacity);
    m_currentFilters = applicationResults.filters.valueOr(m_state.filters);

#if USE(COORDINATED_GRAPHICS)
    // Calculate localTransform 50ms in the future.
    Nicosia::Animation::ApplicationResult futureApplicationResults;
    m_animations.applyKeepingInternalState(futureApplicationResults, time + 50_ms);
    m_layerTransforms.futureLocalTransform = futureApplicationResults.transform.valueOr(m_layerTransforms.localTransform);
#endif

    return applicationResults.hasRunningAnimations;
}

}
