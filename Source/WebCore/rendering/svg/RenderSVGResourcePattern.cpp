/*
 * Copyright (C) 2021, 2022, 2023, 2024 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "RenderSVGResourcePattern.h"

#include "ContainerNodeInlines.h"
#include "ElementChildIteratorInlines.h"
#include "ImageBuffer.h"
#include "NativeImage.h"
#include "RenderLayer.h"
#include "RenderObjectInlines.h"
#include "RenderSVGModelObjectInlines.h"
#include "RenderSVGResourcePatternInlines.h"
#include "RenderSVGShape.h"
#include "SVGElementTypeHelpers.h"
#include "SVGFitToViewBox.h"
#include "SVGVisitedRendererTracking.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderSVGResourcePattern);

static constexpr size_t maximumCachedTilesPerClient = 4;

RenderSVGResourcePattern::RenderSVGResourcePattern(SVGElement& element, Style::ComputedStyle&& style)
    : RenderSVGResourcePaintServer(Type::SVGResourcePattern, element, WTF::move(style))
{
}

RenderSVGResourcePattern::~RenderSVGResourcePattern() = default;

void RenderSVGResourcePattern::collectPatternAttributesIfNeeded()
{
    if (m_attributes.has_value())
        return;

    Ref patternElement = this->patternElement();

    auto attributes = PatternAttributes { };
    if (patternElement->collectPatternAttributes(attributes))
        m_attributes = WTF::move(attributes);
}

RefPtr<Pattern> RenderSVGResourcePattern::buildPattern(GraphicsContext& context, const RenderLayerModelObject& renderer, const std::optional<AffineTransform>& nonScalingStrokeTransform)
{
    auto objectBoundingBox = renderer.objectBoundingBox();

    // Ignore 2D rotation, as it doesn't affect the size of the tile.
    auto absoluteTransformIgnoringRotation = context.getCTM(GraphicsContext::DefinitelyIncludeDeviceScale);
    // The tile is cached for all paint operations of the renderer, so render it at the scale of the renderer's
    // user space. A non-scaling stroke paints in a context without the transform to the SVG root.
    if (nonScalingStrokeTransform)
        absoluteTransformIgnoringRotation.multiply(*nonScalingStrokeTransform);

    FloatSize absoluteScale(absoluteTransformIgnoringRotation.xScale(), absoluteTransformIgnoringRotation.yScale());

    if (auto it = m_tiles.find(renderer); it != m_tiles.end()) {
        const auto& cachedTiles = it->value;
        auto matchesScale = [&](const auto& tile) {
            return tile.absoluteScale == absoluteScale;
        };
        if (cachedTiles.objectBoundingBox != objectBoundingBox)
            m_tiles.remove(it);
        else if (auto index = cachedTiles.tiles.findIf(matchesScale); index != notFound) {
            auto tile = cachedTiles.tiles[index];
            return Pattern::create({ WTF::move(tile.image) }, { true, true, tile.patternSpaceTransform });
        }
    }

    collectPatternAttributesIfNeeded();

    if (!m_attributes)
        return nullptr;

    // Spec: When the geometry of the applicable element has no width or height and objectBoundingBox is specified,
    // then the given effect (e.g. a gradient or a filter) will be ignored.
    if (m_attributes->patternUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX && objectBoundingBox.isEmpty())
        return nullptr;

    // Compute all necessary transformations to build the tile image & the pattern.
    FloatRect tileBoundaries;
    AffineTransform tileImageTransform;
    if (!buildTileImageTransform(renderer, *m_attributes, protect(patternElement()), tileBoundaries, tileImageTransform))
        return nullptr;

    auto tileScale = absoluteScale;

    // Scale the tile size to match the scale level of the patternTransform.
    tileScale.scale(static_cast<float>(m_attributes->patternTransform().xScale()), static_cast<float>(m_attributes->patternTransform().yScale()));

    // Build tile image.
    RefPtr tileImage = createTileImage(context, *m_attributes, tileBoundaries.size(), tileScale, tileImageTransform);
    if (!tileImage)
        return nullptr;

    auto tileImageSize = tileImage->logicalSize();

    // Compute pattern space transformation.
    AffineTransform transform;
    transform.translate(tileBoundaries.location());
    transform.scale(tileBoundaries.size() / tileImageSize);

    AffineTransform patternTransform = m_attributes->patternTransform();
    if (!patternTransform.isIdentity())
        transform = patternTransform * transform;

    Ref image = tileImage.releaseNonNull();
    auto pattern = Pattern::create({ image.copyRef() }, { true, true, transform });

    auto& cachedTiles = m_tiles.ensure(renderer, [&] {
        return CachedTiles { objectBoundingBox, { } };
    }).iterator->value;
    if (cachedTiles.tiles.size() == maximumCachedTilesPerClient)
        cachedTiles.tiles.removeAt(0);
    cachedTiles.tiles.append({ absoluteScale, WTF::move(image), transform });
    return pattern;
}

bool RenderSVGResourcePattern::prepareFillOperation(GraphicsContext& context, const RenderLayerModelObject& targetRenderer, const Style::ComputedStyle& style)
{
    auto pattern = buildPattern(context, targetRenderer);
    if (!pattern)
        return false;

    context.setAlpha(Style::evaluate<float>(style.fillOpacity()));
    context.setFillRule(style.fillRule());
    context.setFillPattern(*pattern);
    return true;
}

bool RenderSVGResourcePattern::prepareStrokeOperation(GraphicsContext& context, const RenderLayerModelObject& targetRenderer, const Style::ComputedStyle& style)
{
    std::optional<AffineTransform> nonScalingStrokeTransform;
    if (style.vectorEffect() == VectorEffect::NonScalingStroke) {
        if (CheckedPtr shape = dynamicDowncast<RenderSVGShape>(targetRenderer))
            nonScalingStrokeTransform = shape->computeNonScalingStrokeTransform();
    }

    auto pattern = buildPattern(context, targetRenderer, nonScalingStrokeTransform);
    if (!pattern)
        return false;

    context.setAlpha(Style::evaluate<float>(style.strokeOpacity()));
    SVGRenderSupport::applyStrokeStyleToContext(context, style, targetRenderer);
    if (nonScalingStrokeTransform)
        pattern = pattern->withPatternSpaceTransform(*nonScalingStrokeTransform * pattern->patternSpaceTransform());
    context.setStrokePattern(*pattern);
    return true;
}

bool RenderSVGResourcePattern::buildTileImageTransform(const RenderElement& renderer, const PatternAttributes& attributes, const SVGPatternElement& patternElement, FloatRect& patternBoundaries, AffineTransform& tileImageTransform) const
{
    auto objectBoundingBox = renderer.objectBoundingBox();
    patternBoundaries = calculatePatternBoundaries(attributes, objectBoundingBox, patternElement);
    if (patternBoundaries.width() <= 0 || patternBoundaries.height() <= 0)
        return false;

    auto viewBoxCTM = SVGFitToViewBox::viewBoxToViewTransform(attributes.viewBox(), attributes.preserveAspectRatio(), patternBoundaries.width(), patternBoundaries.height());

    // Apply viewBox/objectBoundingBox transformations.
    if (!viewBoxCTM.isIdentity())
        tileImageTransform = viewBoxCTM;
    else if (attributes.patternContentUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX)
        tileImageTransform.scale(objectBoundingBox.width(), objectBoundingBox.height());

    return true;
}

RefPtr<ImageBuffer> RenderSVGResourcePattern::createTileImage(GraphicsContext& context, const PatternAttributes& attributes, const FloatSize& size, const FloatSize& scale, const AffineTransform& tileImageTransform) const
{
    CheckedPtr patternRenderer = downcast<RenderSVGResourcePattern>(attributes.patternContentElement()->renderer());
    ASSERT(patternRenderer);
    ASSERT(patternRenderer->hasLayer());

    static NeverDestroyed<SVGVisitedRendererTracking::VisitedSet> s_visitedSet;

    SVGVisitedRendererTracking recursionTracking(s_visitedSet);
    if (recursionTracking.isVisiting(*patternRenderer))
        return nullptr;

    SVGVisitedRendererTracking::Scope recursionScope(recursionTracking, *patternRenderer);

    // This is equivalent to making createImageBuffer() use roundedIntSize().
    auto roundedUnscaledImageBufferSize = [](const FloatSize& size, const FloatSize& scale) -> FloatSize {
        auto scaledSize = size * scale;
        return size - (expandedIntSize(scaledSize) - roundedIntSize(scaledSize)) * (scaledSize - flooredIntSize(scaledSize)) / scale;
    };
    auto tileSize = roundedUnscaledImageBufferSize(size, scale);

    // FIXME: consider color space handling/'color-interpolation'.
    auto tileImage = context.createScaledImageBuffer(tileSize, scale, ColorSpace::SRGB());
    if (!tileImage)
        return nullptr;

    auto& tileImageContext = tileImage->context();
    GraphicsContextStateSaver stateSaver(tileImageContext);

    // Draw the content into the ImageBuffer.
    protect(patternRenderer->layer())->paintResourceLayerForSVG(tileImageContext, tileImageTransform);
    return tileImage;
}

void RenderSVGResourcePattern::removeReferencingCSSClient(const RenderElement& client)
{
    if (auto renderer = dynamicDowncast<RenderLayerModelObject>(client))
        m_tiles.remove(renderer);
}

}
