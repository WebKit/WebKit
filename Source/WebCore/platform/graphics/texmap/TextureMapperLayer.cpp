/*
 Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 Copyright (C) 2022 Sony Interactive Entertainment Inc.

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

#include "BitmapTexture.h"
#include "FloatQuad.h"
#include "Region.h"
#include "TextureMapper.h"
#include "TextureMapperLayer3DRenderingContext.h"
#include <wtf/MathExtras.h>
#include <wtf/SetForScope.h>
#include <wtf/TZoneMallocInlines.h>

#if USE(COORDINATED_GRAPHICS)
#include "CoordinatedAnimatedBackingStoreClient.h"
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(TextureMapperLayer);

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
    TextureMapperLayer* replicaLayer { nullptr };
};

struct TextureMapperLayer::ComputeTransformData {
    double zNear { 0 };
    double zFar { 0 };

    void updateDepthRange(double z)
    {
        if (zNear < z)
            zNear = z;
        else if (zFar > z)
            zFar = z;
    }
};

class TextureMapperFlattenedLayer final {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit TextureMapperFlattenedLayer(const IntRect& rect, double zNear, double zFar)
        : m_rect(rect)
        , m_zNear(zNear)
        , m_zFar(zFar)
    { }

    IntRect layerRect() const { return m_rect; }

    bool needsUpdate() const { return m_textures.isEmpty(); }

    void update(TextureMapperPaintOptions& options, const std::function<void(TextureMapperPaintOptions&)>& paintFunction)
    {
        auto [prevZNear, prevZFar] =  options.textureMapper.depthRange();
        options.textureMapper.setDepthRange(m_zNear, m_zFar);

        m_textures.clear();

        auto maxTextureSize = options.textureMapper.maxTextureSize();
        forEachTile(maxTextureSize, [&](const IntRect& tileRect) {
            RefPtr<BitmapTexture> surface = options.textureMapper.acquireTextureFromPool(tileRect.size(), { BitmapTexture::Flags::SupportsAlpha });
            {
                SetForScope scopedSurface(options.surface, surface);
                SetForScope scopedOffset(options.offset, -toIntSize(tileRect.location()));
                SetForScope scopedOpacity(options.opacity, 1);

                options.textureMapper.bindSurface(options.surface.get());
                paintFunction(options);

                // If paintFunction applies filters to flattened surface then surface object might have
                // changed to new intermediate filter surface and it needs to be stored for painting.
                ASSERT(options.surface);
                surface = options.surface;
            }
            m_textures.append(WTFMove(surface));
        });

        options.textureMapper.bindSurface(options.surface.get());
        options.textureMapper.setDepthRange(prevZNear, prevZFar);
    }

    void paintToTextureMapper(TextureMapper& textureMapper, const FloatRect& targetRect, TransformationMatrix& modelViewMatrix, float opacity)
    {
        auto maxTextureSize = textureMapper.maxTextureSize();
        auto allEdgesExposed = m_rect.width() <= maxTextureSize.width() && m_rect.height() <= maxTextureSize.height()
            ? TextureMapper::AllEdgesExposed::Yes
            : TextureMapper::AllEdgesExposed::No;
        TransformationMatrix adjustedTransform = modelViewMatrix * TransformationMatrix::rectToRect(m_rect, targetRect);

        size_t textureIndex = 0;
        forEachTile(maxTextureSize, [&](const IntRect& tileRect) {
            ASSERT(textureIndex < m_textures.size());
            textureMapper.drawTexture(*m_textures[textureIndex++], tileRect, adjustedTransform, opacity, allEdgesExposed);
        });
    }

private:
    void forEachTile(const IntSize& maxTextureSize, const std::function<void(const IntRect&)>& tileAction)
    {
        for (int x = m_rect.x(); x < m_rect.maxX(); x += maxTextureSize.width()) {
            for (int y = m_rect.y(); y < m_rect.maxY(); y += maxTextureSize.height()) {
                IntRect tileRect({ x, y }, maxTextureSize);
                tileRect.intersect(m_rect);
                tileAction(tileRect);
            }
        }
    }

    IntRect m_rect;
    double m_zNear;
    double m_zFar;
    Vector<RefPtr<BitmapTexture>> m_textures;
};

TextureMapperLayer::TextureMapperLayer(Damage::ShouldPropagate propagateDamage)
    : m_propagateDamage(propagateDamage)
{
}

TextureMapperLayer::~TextureMapperLayer()
{
    for (auto* child : m_children)
        child->m_parent = nullptr;

    removeFromParent();
}

void TextureMapperLayer::processDescendantLayersFlatteningRequirements()
{
    // Traverse all children in depth first, post-order
    for (auto* child : m_children) {
        child->processDescendantLayersFlatteningRequirements();

        if (child->flattensAsLeafOf3DSceneOr3DPerspective())
            child->processFlatteningRequirements();
    }
}

void TextureMapperLayer::processFlatteningRequirements()
{
    m_flattenedLayer = nullptr;

    // Reset transformations as this layer is root 2D
    SetForScope scopedLocalTransform(m_layerTransforms.localTransform, TransformationMatrix::identity);
    m_layerTransforms.combined = { };
    m_layerTransforms.combinedForChildren = { };
#if USE(COORDINATED_GRAPHICS)
    SetForScope scopedFutureLocalTransform(m_layerTransforms.futureLocalTransform, TransformationMatrix::identity);
    m_layerTransforms.futureLocalTransform = { };
    m_layerTransforms.futureCombined = { };
    m_layerTransforms.futureCombinedForChildren = { };
#endif
    ComputeTransformData data;
    if (m_state.maskLayer)
        m_state.maskLayer->computeTransformsRecursive(data);
    if (m_state.replicaLayer)
        m_state.replicaLayer->computeTransformsRecursive(data);
    if (m_state.backdropLayer)
        m_state.backdropLayer->computeTransformsRecursive(data);
    for (auto* child : m_children)
        child->computeTransformsRecursive(data);

    Region layerRegion;
    computeFlattenedRegion(layerRegion, true);

    if (layerRegion.isEmpty())
        return;

    m_flattenedLayer = makeUnique<TextureMapperFlattenedLayer>(layerRegion.bounds(), data.zNear, data.zFar);
}

void TextureMapperLayer::computeFlattenedRegion(Region& region, bool layerIsFlatteningRoot)
{
    auto rect = isFlattened() ? m_flattenedLayer->layerRect() : layerRect();

    bool shouldExpand = m_currentFilters.hasOutsets() && !m_state.masksToBounds && !m_state.maskLayer;
    if (shouldExpand && !layerIsFlatteningRoot) {
        auto outsets = m_currentFilters.outsets();
        rect.move(-outsets.left(), -outsets.top());
        rect.expand(outsets.left() + outsets.right(), outsets.top() + outsets.bottom());
    }

    region.unite(enclosingIntRect(m_layerTransforms.combined.mapRect(rect)));

    if (!m_state.masksToBounds && !m_state.maskLayer) {
        if (m_state.replicaLayer)
            region.unite(enclosingIntRect(m_state.replicaLayer->m_layerTransforms.combined.mapRect(m_state.replicaLayer->layerRect())));

        for (auto* child : m_children)
            child->computeFlattenedRegion(region, false);
    }

    if (shouldExpand && layerIsFlatteningRoot) {
        auto bounds = region.bounds();
        auto outsets = m_currentFilters.outsets();
        bounds.move(-outsets.left(), -outsets.top());
        bounds.expand(outsets.left() + outsets.right(), outsets.top() + outsets.bottom());
        region = bounds;
    }
}

void TextureMapperLayer::destroyFlattenedDescendantLayers()
{
    if (m_flattenedLayer)
        m_flattenedLayer = nullptr;

    for (auto* child : m_children)
        child->destroyFlattenedDescendantLayers();
}

void TextureMapperLayer::computeTransformsRecursive(ComputeTransformData& data)
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
            m_layerTransforms.combinedForChildren.flatten();
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
            m_layerTransforms.futureCombinedForChildren.flatten();
        m_layerTransforms.futureCombinedForChildren.multiply(m_state.childrenTransform);
        m_layerTransforms.futureCombinedForChildren.translate3d(-originX, -originY, -m_state.anchorPoint.z());
#endif
    }

    m_state.visible = m_state.backfaceVisibility || !m_layerTransforms.combined.isBackFaceVisible();

    auto calculateZ = [&](double x, double y) -> double {
        double z = 0;
        double w = 1;
        m_layerTransforms.combined.map4ComponentPoint(x, y, z, w);
        if (w <= 0) {
            if (!z)
                return 0;
            if (z < 0)
                return -std::numeric_limits<double>::infinity();
            return std::numeric_limits<double>::infinity();
        }
        return z / w;
    };

    auto rect = isFlattened() ? m_flattenedLayer->layerRect() : layerRect();

    data.updateDepthRange(calculateZ(rect.x(), rect.y()));
    data.updateDepthRange(calculateZ(rect.x() + rect.width(), rect.y()));
    data.updateDepthRange(calculateZ(rect.x(), rect.y() + rect.height()));
    data.updateDepthRange(calculateZ(rect.x() + rect.width(), rect.y() + rect.height()));

    if (m_state.backdropLayer)
        m_state.backdropLayer->computeTransformsRecursive(data);

    if (!isFlattened()) {
        if (m_state.maskLayer)
            m_state.maskLayer->computeTransformsRecursive(data);
        if (m_state.replicaLayer)
            m_state.replicaLayer->computeTransformsRecursive(data);
        for (auto* child : m_children) {
            ASSERT(child->m_parent == this);
            child->computeTransformsRecursive(data);
        }
    }

#if USE(COORDINATED_GRAPHICS)
    if (m_backingStore && m_animatedBackingStoreClient)
        m_animatedBackingStoreClient->requestBackingStoreUpdateIfNeeded(m_layerTransforms.futureCombined);
#endif
}

void TextureMapperLayer::paint(TextureMapper& textureMapper)
{
    processDescendantLayersFlatteningRequirements();

    ComputeTransformData data;
    computeTransformsRecursive(data);
    textureMapper.setDepthRange(data.zNear, data.zFar);

    TextureMapperPaintOptions options(textureMapper);
    options.surface = textureMapper.currentSurface();
    paintRecursive(options);

    destroyFlattenedDescendantLayers();
}

void TextureMapperLayer::collectDamage(TextureMapper& textureMapper)
{
    TextureMapperPaintOptions options(textureMapper);
    options.surface = textureMapper.currentSurface();
    collectDamageRecursive(options);
}

void TextureMapperLayer::paintSelf(TextureMapperPaintOptions& options)
{
    if (!m_state.visible || !m_state.contentsVisible)
        return;
    auto targetRect = layerRect();
    if (targetRect.isEmpty())
        return;

    // We apply the following transform to compensate for painting into a surface, and then apply the offset so that the painting fits in the target rect.
    TransformationMatrix transform;
    transform.translate(options.offset.width(), options.offset.height());
    transform.multiply(options.transform);
    transform.multiply(m_layerTransforms.combined);

    if (isFlattened() && !m_flattenedLayer->needsUpdate()) {
        m_flattenedLayer->paintToTextureMapper(options.textureMapper, m_flattenedLayer->layerRect(), transform, options.opacity);
        return;
    }

    TextureMapperSolidColorLayer solidColorLayer;
    TextureMapperBackingStore* backingStore = m_backingStore;
    if (m_state.backgroundColor.isValid()) {
        solidColorLayer.setColor(m_state.backgroundColor);
        backingStore = &solidColorLayer;
    }

    options.textureMapper.setWrapMode(TextureMapper::WrapMode::Stretch);
    options.textureMapper.setPatternTransform(TransformationMatrix());

    if (backingStore) {
        backingStore->paintToTextureMapper(options.textureMapper, targetRect, transform, options.opacity);
        if (m_state.showDebugBorders)
            backingStore->drawBorder(options.textureMapper, m_state.debugBorderColor, m_state.debugBorderWidth, targetRect, transform);
        // Only draw repaint count for the main backing store.
        if (m_state.showRepaintCounter)
            backingStore->drawRepaintCounter(options.textureMapper, m_state.repaintCount, m_state.debugBorderColor, targetRect, transform);
    }

    TextureMapperPlatformLayer* contentsLayer = m_contentsLayer;
    if (m_state.solidColor.isValid() && m_state.solidColor.isVisible()) {
        solidColorLayer.setColor(m_state.solidColor);
        contentsLayer = &solidColorLayer;
    }

    if (!contentsLayer)
        return;

    if (!m_state.contentsTileSize.isEmpty()) {
        options.textureMapper.setWrapMode(TextureMapper::WrapMode::Repeat);

        auto patternTransform = TransformationMatrix::rectToRect({ { }, m_state.contentsTileSize }, { { }, m_state.contentsRect.size() })
            .translate(m_state.contentsTilePhase.width() / m_state.contentsRect.width(), m_state.contentsTilePhase.height() / m_state.contentsRect.height());
        options.textureMapper.setPatternTransform(patternTransform);
    }

    bool shouldClip = m_state.contentsClippingRect.isRounded() || !m_state.contentsClippingRect.rect().contains(m_state.contentsRect);
    if (shouldClip) {
        options.textureMapper.beginClip(transform, m_state.contentsClippingRect);
    }

    contentsLayer->paintToTextureMapper(options.textureMapper, m_state.contentsRect, transform, options.opacity);

    if (shouldClip)
        options.textureMapper.endClip();

    if (m_state.showDebugBorders)
        contentsLayer->drawBorder(options.textureMapper, m_state.debugBorderColor, m_state.debugBorderWidth, m_state.contentsRect, transform);
}

void TextureMapperLayer::collectDamageSelf(TextureMapperPaintOptions& options)
{
    if (!m_state.visible || !m_state.contentsVisible)
        return;
    auto targetRect = layerRect();
    if (targetRect.isEmpty())
        return;

    TransformationMatrix transform;
    transform.translate(options.offset.width(), options.offset.height());
    transform.multiply(options.transform);
    transform.multiply(m_layerTransforms.combined);

    TextureMapperPlatformLayer* contentsLayer = m_contentsLayer;
    if (!contentsLayer) {
        // Use the damage information we received from the CoordinatedGraphicsLayer
        // Here we ignore the targetRect parameter as it should already have
        // been covered by the damage tracking in setNeedsDisplay/setNeedsDisplayInRect
        // calls from CoordinatedGraphicsLayer.
        if (m_propagateDamage == Damage::ShouldPropagate::Yes) {
            if (m_damage.isInvalid())
                recordDamage(targetRect, transform, options);
            else {
                for (const auto& rect : m_damage.rects()) {
                    ASSERT(!rect.isEmpty());
                    recordDamage(rect, transform, options);
                }
            }
            clearDamage();
        }
        return;
    }

    if (m_propagateDamage == Damage::ShouldPropagate::Yes) {
        // Layers with content layer are always fully damaged for now...
        recordDamage(targetRect, transform, options);
        clearDamage();
    }
}

void TextureMapperLayer::paintBackdrop(TextureMapperPaintOptions& options)
{
    TransformationMatrix clipTransform;
    clipTransform.translate(options.offset.width(), options.offset.height());
    clipTransform.multiply(options.transform);
    clipTransform.multiply(m_layerTransforms.combined);
    options.textureMapper.beginClip(clipTransform, m_state.backdropFiltersRect);
    m_state.backdropLayer->paintRecursive(options);
    options.textureMapper.endClip();
}

void TextureMapperLayer::paintSelfAndChildren(TextureMapperPaintOptions& options)
{
    if (m_state.backdropLayer && m_state.backdropLayer == options.backdropLayer)
        return;

    if (m_state.backdropLayer && !options.backdropLayer)
        paintBackdrop(options);

    paintSelf(options);

    if (m_children.isEmpty())
        return;

    bool shouldClip = (m_state.masksToBounds || m_state.contentsRectClipsDescendants) && !m_state.preserves3D;
    if (shouldClip) {
        TransformationMatrix clipTransform;
        clipTransform.translate(options.offset.width(), options.offset.height());
        clipTransform.multiply(options.transform);
        clipTransform.multiply(m_layerTransforms.combined);
        if (m_state.contentsRectClipsDescendants)
            options.textureMapper.beginClip(clipTransform, m_state.contentsClippingRect);
        else {
            clipTransform.translate(m_state.boundsOrigin.x(), m_state.boundsOrigin.y());
            options.textureMapper.beginClip(clipTransform, FloatRoundedRect(layerRect()));
        }

        // If as a result of beginClip(), the clipping area is empty, it means that the intersection of the previous
        // clipping area and the current one don't have any pixels in common. In this case we can skip painting the
        // children as they will be clipped out (see https://bugs.webkit.org/show_bug.cgi?id=181080).
        if (options.textureMapper.clipBounds().isEmpty()) {
            options.textureMapper.endClip();
            return;
        }
    }

    if (!isFlattened() || m_flattenedLayer->needsUpdate()) {
        for (auto* child : m_children)
            child->paintRecursive(options);
    }

    if (shouldClip)
        options.textureMapper.endClip();
}

bool TextureMapperLayer::shouldBlend() const
{
    if (m_state.preserves3D)
        return false;

    return m_currentOpacity < 1;
}

bool TextureMapperLayer::flattensAsLeafOf3DSceneOr3DPerspective() const
{
    bool isLeafOf3DScene = !m_state.preserves3D && (m_parent && m_parent->preserves3D());
    bool hasPerspective = m_layerTransforms.localTransform.hasPerspective() && m_layerTransforms.localTransform.m34() != -1.f;
    if ((isLeafOf3DScene || hasPerspective) && !m_children.isEmpty() && !m_layerTransforms.localTransform.isAffine())
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

void TextureMapperLayer::paintSelfAndChildrenWithReplica(TextureMapperPaintOptions& options)
{
    if (m_state.replicaLayer) {
        SetForScope scopedReplicaLayer(options.replicaLayer, this);
        SetForScope scopedTransform(options.transform, options.transform);
        options.transform.multiply(replicaTransform());
        paintSelfAndChildren(options);
    }

    paintSelfAndChildren(options);
}

TransformationMatrix TextureMapperLayer::replicaTransform()
{
    return TransformationMatrix(m_state.replicaLayer->m_layerTransforms.combined)
        .multiply(m_layerTransforms.combined.inverse().value_or(TransformationMatrix()));
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

template<typename T>
struct Point4D {
    T x;
    T y;
    T z;
    T w;
};

template<typename T>
Point4D<T> operator+(Point4D<T> s, Point4D<T> t)
{
    return { s.x + t.x, s.y + t.y, s.z + t.z, s.w + t.w };
}

template<typename T>
Point4D<T> operator-(Point4D<T> s, Point4D<T> t)
{
    return { s.x - t.x, s.y - t.y, s.z - t.z, s.w - t.w };
}

template<typename T>
Point4D<T> operator*(T s, Point4D<T> t)
{
    return { s * t.x, s * t.y, s * t.z, s * t.w };
}

template<typename T>
struct MinMax {
    T min;
    T max;
};

IntRect transformedBoundingBox(const TransformationMatrix& transform, FloatRect rect, const IntRect& clip)
{
    using Point = Point4D<double>;
    auto mapPoint = [&](FloatPoint p) -> Point {
        double x = p.x();
        double y = p.y();
        double z = 0;
        double w = 1;
        transform.map4ComponentPoint(x, y, z, w);
        return { x, y, z, w };
    };

    WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN // GLib/Win port
    Point vertex[] = {
        mapPoint(rect.minXMinYCorner()),
        mapPoint(rect.maxXMinYCorner()),
        mapPoint(rect.maxXMaxYCorner()),
        mapPoint(rect.minXMaxYCorner())
    };
    bool isPositive[] = {
        vertex[0].w >= 0,
        vertex[1].w >= 0,
        vertex[2].w >= 0,
        vertex[3].w >= 0
    };
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

    auto findFirstPositiveVertex = [&]() {
        int i = 0;
        for (; i < 4; i++) {
            if (isPositive[i])
                return i;
        }
        return i;
    };
    auto findFirstNegativeVertex = [&]() {
        int i = 0;
        for (; i < 4; i++) {
            if (!isPositive[i])
                return i;
        }
        return i;
    };

    auto leftVertexIndex = [](int i) {
        return (i + 1) % 4;
    };
    auto diagonalVertexIndex = [](int i) {
        return (i + 2) % 4;
    };
    auto rightVertexIndex = [](int i) {
        return (i + 3) % 4;
    };

    auto minMax2 = [](double d1, double d2) -> MinMax<double> {
        if (d1 < d2)
            return { d1, d2 };
        return { d2, d1 };
    };
    auto minMax3 = [&](double d1, double d2, double d3) -> MinMax<double> {
        auto minmax = minMax2(d1, d2);
        return { std::min(minmax.min, d3), std::max(minmax.max, d3) };
    };
    auto minMax4 = [&](double d1, double d2, double d3, double d4) -> MinMax<double> {
        auto minmax1 = minMax2(d1, d2);
        auto minmax2 = minMax2(d3, d4);
        return { std::min(minmax1.min, minmax2.min), std::max(minmax1.max, minmax2.max) };
    };

    auto clipped = [&](const MinMax<double>& xMinMax, const MinMax<double>& yMinMax) -> IntRect {
        int minX = std::max<double>(xMinMax.min, clip.x());
        int minY = std::max<double>(yMinMax.min, clip.y());
        int maxX = std::min<double>(xMinMax.max, clip.maxX());
        int maxY = std::min<double>(yMinMax.max, clip.maxY());
        return { minX, minY, maxX - minX, maxY - minY };
    };

    auto toPositive = [&](Point positive, Point negative) -> Point {
        ASSERT(positive.w > 0);
        ASSERT(negative.w <= 0);
        auto v = positive.w * negative - negative.w * positive;
        v.w = 0;
        return v;
    };
    auto boundingBoxPPP = [&](Point p1, Point p2, Point p3) -> IntRect {
        ASSERT(p1.w >= 0);
        ASSERT(p2.w >= 0);
        ASSERT(p3.w >= 0);
        auto xMinMax = minMax3(p1.x / p1.w, p2.x / p2.w, p3.x / p3.w);
        auto yMinMax = minMax3(p1.y / p1.w, p2.y / p2.w, p3.y / p3.w);
        return clipped(xMinMax, yMinMax);
    };
    auto boundingBoxPPN = [&](Point p1, Point p2, Point n3) -> IntRect {
        return boundingBoxPPP(p1, p2, toPositive(p1, n3));
    };
    auto boundingBoxPNN = [&](Point p1, Point n2, Point n3) -> IntRect {
        return boundingBoxPPP(p1, toPositive(p1, n2), toPositive(p1, n3));
    };

    auto boundingBoxPPPByIndex = [&](int i1, int i2, int i3) {
        return boundingBoxPPP(vertex[i1], vertex[i2], vertex[i3]);
    };
    auto boundingBoxPPNByIndex = [&](int i1, int i2, int i3) {
        return boundingBoxPPN(vertex[i1], vertex[i2], vertex[i3]);
    };
    auto boundingBoxPNNByIndex = [&](int i1, int i2, int i3) {
        return boundingBoxPNN(vertex[i1], vertex[i2], vertex[i3]);
    };

    int count = isPositive[0] + isPositive[1] + isPositive[2] + isPositive[3];
    switch (count) {
    case 0:
        return { };
    case 1: {
        int i = findFirstPositiveVertex();
        ASSERT(i < 4);
        return boundingBoxPNNByIndex(i, rightVertexIndex(i), leftVertexIndex(i));
    }
    case 2: {
        int i = findFirstPositiveVertex();
        ASSERT(i < 3);
        if (!i && isPositive[3])
            i = 3;
        int positiveRightIndex = i;
        int positiveLeftIndex = leftVertexIndex(i);
        ASSERT(isPositive[positiveLeftIndex]);
        return unionRect(boundingBoxPPNByIndex(positiveRightIndex, positiveLeftIndex, leftVertexIndex(positiveLeftIndex)), boundingBoxPPNByIndex(positiveRightIndex, positiveLeftIndex, rightVertexIndex(positiveRightIndex)));
    }
    case 3: {
        int i = findFirstNegativeVertex();
        ASSERT(i < 4);
        return unionRect(boundingBoxPPNByIndex(leftVertexIndex(i), rightVertexIndex(i), i), boundingBoxPPPByIndex(leftVertexIndex(i), rightVertexIndex(i), diagonalVertexIndex(i)));
    }
    case 4: {
        auto xMinMax = minMax4(vertex[0].x / vertex[0].w, vertex[1].x / vertex[1].w, vertex[2].x / vertex[2].w, vertex[3].x / vertex[3].w);
        auto yMinMax = minMax4(vertex[0].y / vertex[0].w, vertex[1].y / vertex[1].w, vertex[2].y / vertex[2].w, vertex[3].y / vertex[3].w);
        return clipped(xMinMax, yMinMax);
    }
    }
    ASSERT_NOT_REACHED();
    return { };
}

void TextureMapperLayer::computeOverlapRegions(ComputeOverlapRegionData& data, const TransformationMatrix& accumulatedReplicaTransform, bool includesReplica)
{
    if (!m_state.visible || !m_state.contentsVisible)
        return;

    FloatRect localBoundingRect;
    if (isFlattened() && !m_isBackdrop)
        localBoundingRect = m_flattenedLayer->layerRect();
    else if (m_backingStore || m_state.masksToBounds || m_state.maskLayer || hasFilters())
        localBoundingRect = layerRect();
    else if (m_contentsLayer || m_state.solidColor.isVisible())
        localBoundingRect = m_state.contentsRect;

    if (m_currentFilters.hasOutsets() && !m_state.backdropLayer && !m_state.masksToBounds && !m_state.maskLayer) {
        auto outsets = m_currentFilters.outsets();
        localBoundingRect.move(-outsets.left(), -outsets.top());
        localBoundingRect.expand(outsets.left() + outsets.right(), outsets.top() + outsets.bottom());
    }

    TransformationMatrix transform(accumulatedReplicaTransform);
    transform.multiply(m_layerTransforms.combined);

    IntRect viewportBoundingRect = transformedBoundingBox(transform, localBoundingRect, data.clipBounds);

    switch (data.mode) {
    case ComputeOverlapRegionMode::Intersection:
        resolveOverlaps(viewportBoundingRect, data.overlapRegion, data.nonOverlapRegion);
        break;
    case ComputeOverlapRegionMode::Union:
    case ComputeOverlapRegionMode::Mask:
        data.overlapRegion.unite(viewportBoundingRect);
        break;
    }

    if (m_state.replicaLayer && includesReplica) {
        TransformationMatrix newReplicaTransform(accumulatedReplicaTransform);
        newReplicaTransform.multiply(replicaTransform());
        computeOverlapRegions(data, newReplicaTransform, false);
    }

    if (!m_state.masksToBounds && data.mode != ComputeOverlapRegionMode::Mask && !isFlattened()) {
        for (auto* child : m_children)
            child->computeOverlapRegions(data, accumulatedReplicaTransform);
    }
}

void TextureMapperLayer::paintUsingOverlapRegions(TextureMapperPaintOptions& options)
{
    Region overlapRegion;
    Region nonOverlapRegion;
    auto mode = ComputeOverlapRegionMode::Intersection;
    ComputeOverlapRegionData data {
        mode,
        options.textureMapper.clipBounds(),
        overlapRegion,
        nonOverlapRegion
    };
    data.clipBounds.move(-options.offset);
    computeOverlapRegions(data, options.transform);
    if (overlapRegion.isEmpty()) {
        paintSelfChildrenReplicaFilterAndMask(options);
        return;
    }

    // Having both overlap and non-overlap regions carries some overhead. Avoid it if the overlap area
    // is big anyway.
    if (overlapRegion.totalArea() > nonOverlapRegion.totalArea()) {
        overlapRegion.unite(nonOverlapRegion);
        nonOverlapRegion = Region();
    }

    nonOverlapRegion.translate(options.offset);
    auto rects = nonOverlapRegion.rects();

    for (auto& rect : rects) {
        options.textureMapper.beginClip(TransformationMatrix(), FloatRoundedRect(rect));
        paintSelfChildrenReplicaFilterAndMask(options);
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

void TextureMapperLayer::paintSelfChildrenFilterAndMask(TextureMapperPaintOptions& options)
{
    Region overlapRegion;
    Region nonOverlapRegion;
    auto mode = ComputeOverlapRegionMode::Union;
    if (m_state.maskLayer || (options.replicaLayer == this && m_state.replicaLayer->m_state.maskLayer))
        mode = ComputeOverlapRegionMode::Mask;
    ComputeOverlapRegionData data {
        mode,
        options.textureMapper.clipBounds(),
        overlapRegion,
        nonOverlapRegion
    };
    data.clipBounds.move(-options.offset);
    computeOverlapRegions(data, options.transform, false);
    ASSERT(nonOverlapRegion.isEmpty());

    auto rects = overlapRegion.rects();
    static const size_t OverlapRegionConsolidationThreshold = 4;
    if (rects.size() > OverlapRegionConsolidationThreshold) {
        rects.clear();
        rects.append(overlapRegion.bounds());
    }

    IntSize maxTextureSize = options.textureMapper.maxTextureSize();
    for (auto& rect : rects) {
        for (int x = rect.x(); x < rect.maxX(); x += maxTextureSize.width()) {
            for (int y = rect.y(); y < rect.maxY(); y += maxTextureSize.height()) {
                IntRect tileRect(IntPoint(x, y), maxTextureSize);
                tileRect.intersect(rect);

                paintSelfAndChildrenWithIntermediateSurface(options, tileRect);
            }
        }
    }
}

void TextureMapperLayer::applyMask(TextureMapperPaintOptions& options)
{
    options.textureMapper.setMaskMode(true);
    paintSelf(options);
    options.textureMapper.setMaskMode(false);
}

void TextureMapperLayer::paintIntoSurface(TextureMapperPaintOptions& options)
{
    options.textureMapper.bindSurface(options.surface.get());
    if (m_isBackdrop) {
        SetForScope scopedTransform(options.transform, TransformationMatrix());
        SetForScope scopedReplicaLayer(options.replicaLayer, nullptr);
        SetForScope scopedBackdropLayer(options.backdropLayer, this);

        rootLayer().paintSelfAndChildren(options);
    } else
        paintSelfAndChildren(options);

    bool hasMask = !!m_state.maskLayer;
    bool hasReplicaMask = options.replicaLayer == this && m_state.replicaLayer->m_state.maskLayer;
    bool defersLastFilterPass = !hasMask && !hasReplicaMask;
    options.surface = options.textureMapper.applyFilters(options.surface, m_currentFilters, defersLastFilterPass);
    options.textureMapper.bindSurface(options.surface.get());
    if (hasMask)
        m_state.maskLayer->applyMask(options);
    if (hasReplicaMask)
        m_state.replicaLayer->m_state.maskLayer->applyMask(options);
}

static void commitSurface(TextureMapperPaintOptions& options, BitmapTexture& surface, const IntRect& rect, float opacity)
{
    IntRect targetRect(rect);
    targetRect.move(options.offset);
    options.textureMapper.bindSurface(options.surface.get());
    options.textureMapper.drawTexture(surface, targetRect, { }, opacity);
}

void TextureMapperLayer::paintWithIntermediateSurface(TextureMapperPaintOptions& options, const IntRect& rect)
{
    auto surface = options.textureMapper.acquireTextureFromPool(rect.size(), { BitmapTexture::Flags::SupportsAlpha });
    {
        SetForScope scopedSurface(options.surface, surface.ptr());
        SetForScope scopedOffset(options.offset, -toIntSize(rect.location()));
        SetForScope scopedOpacity(options.opacity, 1);

        options.textureMapper.bindSurface(options.surface.get());
        paintSelfChildrenReplicaFilterAndMask(options);
    }

    commitSurface(options, surface.get(), rect, options.opacity);
}

void TextureMapperLayer::paintSelfAndChildrenWithIntermediateSurface(TextureMapperPaintOptions& options, const IntRect& rect)
{
    auto surface = options.textureMapper.acquireTextureFromPool(rect.size(), { BitmapTexture::Flags::SupportsAlpha });
    {
        SetForScope scopedSurface(options.surface, surface.ptr());
        SetForScope scopedOffset(options.offset, -toIntSize(rect.location()));
        SetForScope scopedOpacity(options.opacity, 1);

        paintIntoSurface(options);
        surface = Ref { *options.surface };
    }

    commitSurface(options, surface.get(), rect, options.opacity);
}

void TextureMapperLayer::paintSelfChildrenReplicaFilterAndMask(TextureMapperPaintOptions& options)
{
    bool hasFilterOrMask = [&] {
        if (hasFilters())
            return true;
        if (m_state.maskLayer)
            return true;
        if (m_state.replicaLayer && m_state.replicaLayer->m_state.maskLayer)
            return true;
        return false;
    }();
    if (hasFilterOrMask) {
        if (m_state.replicaLayer) {
            SetForScope scopedReplicaLayer(options.replicaLayer, this);
            SetForScope scopedTransform(options.transform, options.transform);
            options.transform.multiply(replicaTransform());
            paintSelfChildrenFilterAndMask(options);
        }
        paintSelfChildrenFilterAndMask(options);
    } else
        paintSelfAndChildrenWithReplica(options);
}

void TextureMapperLayer::paintRecursive(TextureMapperPaintOptions& options)
{
    if (!isVisible())
        return;

    SetForScope scopedOpacity(options.opacity, options.opacity * m_currentOpacity);

    if (preserves3D())
        paintWith3DRenderingContext(options);
    else if (isFlattened())
        paintFlattened(options);
    else if (shouldBlend())
        paintUsingOverlapRegions(options);
    else
        paintSelfChildrenReplicaFilterAndMask(options);
}

void TextureMapperLayer::paintFlattened(TextureMapperPaintOptions& options)
{
    // If painting a backdrop for a flattened layer, processing should stop here.
    // Only the elements up to the flattened layer need to be painted into the backdrop buffer.
    if (m_state.backdropLayer && m_state.backdropLayer == options.backdropLayer)
        return;

    if (m_state.backdropLayer && !options.backdropLayer)
        paintBackdrop(options);

    if (m_flattenedLayer->needsUpdate()) {
        SetForScope scopedOffset(options.offset, IntSize());
        SetForScope scopedTransform(options.transform, TransformationMatrix());
        SetForScope scopedCombinedTransform(m_layerTransforms.combined, TransformationMatrix());
        SetForScope scopedBackdropLayer(m_state.backdropLayer, nullptr);

        m_flattenedLayer->update(options, [&](TextureMapperPaintOptions& options) {
            paintIntoSurface(options);
        });
    }

    paintSelf(options);
}

void TextureMapperLayer::collectDamageRecursive(TextureMapperPaintOptions& options)
{
    if (!isVisible())
        return;

    SetForScope scopedOpacity(options.opacity, options.opacity * m_currentOpacity);

    collectDamageSelf(options);

    for (auto* child : m_children)
        child->collectDamageRecursive(options);
}

void TextureMapperLayer::paintWith3DRenderingContext(TextureMapperPaintOptions& options)
{
    Vector<TextureMapperLayer*> layers;
    collect3DSceneLayers(layers);

    TextureMapperLayer3DRenderingContext context;
    context.paint(layers, [&](TextureMapperLayer* layer, const FloatPolygon& clipArea) {
        if (!clipArea.isEmpty())
            options.textureMapper.beginClip(layer->toSurfaceTransform(), clipArea);

        if (layer->preserves3D())
            layer->paintSelf(options);
        else
            layer->paintRecursive(options);

        if (!clipArea.isEmpty())
            options.textureMapper.endClip();
    });
}

void TextureMapperLayer::collect3DSceneLayers(Vector<TextureMapperLayer*>& layers)
{
    bool isLeafOf3DScene = !m_state.preserves3D && (m_parent && m_parent->preserves3D());
    if (preserves3D() || isLeafOf3DScene) {
        if (m_state.visible)
            layers.append(this);

        // Stop recursion on scene leaf
        if (isLeafOf3DScene)
            return;
    }

    for (auto* child : m_children)
        child->collect3DSceneLayers(layers);
}

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

    if (m_visitor)
        childLayer->acceptDamageVisitor(*m_visitor);
}

void TextureMapperLayer::removeFromParent()
{
    dismissDamageVisitor();

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
    for (auto* child : oldChildren) {
        child->dismissDamageVisitor();
        child->m_parent = nullptr;
    }
}

void TextureMapperLayer::setMaskLayer(TextureMapperLayer* maskLayer)
{
    if (maskLayer) {
        maskLayer->m_effectTarget = m_isReplica ? m_effectTarget : *this;
        m_state.maskLayer = *maskLayer;
    } else
        m_state.maskLayer = nullptr;
}

void TextureMapperLayer::setReplicaLayer(TextureMapperLayer* replicaLayer)
{
    if (replicaLayer) {
        replicaLayer->m_isReplica = true;
        replicaLayer->m_effectTarget = *this;
        m_state.replicaLayer = *replicaLayer;
    } else
        m_state.replicaLayer = nullptr;
}

void TextureMapperLayer::setBackdropLayer(TextureMapperLayer* backdropLayer)
{
    if (backdropLayer) {
        backdropLayer->m_isBackdrop = true;
        backdropLayer->m_effectTarget = *this;
        m_state.backdropLayer = *backdropLayer;
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

void TextureMapperLayer::setContentsRectClipsDescendants(bool clips)
{
    m_state.contentsRectClipsDescendants = clips;
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

void TextureMapperLayer::setBackgroundColor(const Color& color)
{
    m_state.backgroundColor = color;
}

void TextureMapperLayer::setFilters(const FilterOperations& filters)
{
    m_state.filters = filters;
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

#if USE(COORDINATED_GRAPHICS)
void TextureMapperLayer::setAnimatedBackingStoreClient(CoordinatedAnimatedBackingStoreClient* client)
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
    if (hasRunningAnimations) // FIXME Too broad?
        addDamage(layerRect());
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
    TextureMapperAnimation::ApplicationResult applicationResults;
    m_animations.apply(applicationResults, time);

    m_layerTransforms.localTransform = applicationResults.transform.value_or(m_state.transform);
    m_currentOpacity = applicationResults.opacity.value_or(m_state.opacity);
    m_currentFilters = applicationResults.filters.value_or(m_state.filters);

#if USE(COORDINATED_GRAPHICS)
    // Calculate localTransform 50ms in the future.
    TextureMapperAnimation::ApplicationResult futureApplicationResults;
    m_animations.apply(futureApplicationResults, time + 50_ms, TextureMapperAnimation::KeepInternalState::Yes);
    m_layerTransforms.futureLocalTransform = futureApplicationResults.transform.value_or(m_layerTransforms.localTransform);
#endif

    return applicationResults.hasRunningAnimations;
}

void TextureMapperLayer::acceptDamageVisitor(TextureMapperLayerDamageVisitor& visitor)
{
    if (&visitor == m_visitor)
        return;

    m_visitor = &visitor;

    for (auto* child : m_children)
        child->acceptDamageVisitor(visitor);
}

void TextureMapperLayer::dismissDamageVisitor()
{
    for (auto* child : m_children)
        child->dismissDamageVisitor();
    m_visitor = nullptr;
}

void TextureMapperLayer::recordDamage(const FloatRect& rect, const TransformationMatrix& transform, const TextureMapperPaintOptions& options)
{
    if (!m_visitor)
        return;

    FloatQuad quad(rect);
    quad = transform.mapQuad(quad);
    FloatRect transformedRect = quad.boundingBox();
    // Some layers are drawn on an intermediate surface and have this offset applied to convert to the
    // intermediate surface coordinates. In order to translate back to actual coordinates,
    // we have to undo it.
    transformedRect.move(-options.offset);
    auto clipBounds = options.textureMapper.clipBounds();
    clipBounds.move(-options.offset);
    transformedRect.intersect(clipBounds);

    m_visitor->recordDamage(transformedRect);
}

FloatRect TextureMapperLayer::effectiveLayerRect() const
{
    if (isFlattened())
        return m_flattenedLayer->layerRect();
    return layerRect();
}

}
