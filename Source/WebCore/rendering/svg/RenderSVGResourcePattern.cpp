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

#include "ElementChildIteratorInlines.h"
#include "RenderLayer.h"
#include "RenderSVGModelObjectInlines.h"
#include "RenderSVGResourcePatternInlines.h"
#include "RenderSVGShape.h"
#include "SVGElementTypeHelpers.h"
#include "SVGFitToViewBox.h"
#include "SVGRenderStyle.h"
#include "SVGVisitedRendererTracking.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(RenderSVGResourcePattern);

RenderSVGResourcePattern::RenderSVGResourcePattern(SVGElement& element, RenderStyle&& style)
    : RenderSVGResourcePaintServer(Type::SVGResourcePattern, element, WTFMove(style))
{
}

RenderSVGResourcePattern::~RenderSVGResourcePattern() = default;

void RenderSVGResourcePattern::collectPatternAttributesIfNeeded()
{
    if (m_attributes.has_value())
        return;

    auto attributes = PatternAttributes { };

    RefPtr current = &patternElement();

    current->synchronizeAllAttributes();

    while (current) {
        if (!current->renderer())
            break;
        current->collectPatternAttributes(attributes);

        auto target = SVGURIReference::targetElementFromIRIString(current->href(), current->treeScopeForSVGReferences());
        current = dynamicDowncast<SVGPatternElement>(target.element.get());
    }

    // If we couldn't determine the pattern content element root, stop here.
    if (!attributes.patternContentElement())
        return;

    // An empty viewBox disables rendering.
    if (attributes.hasViewBox() && attributes.viewBox().isEmpty())
        return;

    m_attributes = WTFMove(attributes);
}

RefPtr<Pattern> RenderSVGResourcePattern::buildPattern(GraphicsContext& context, const RenderLayerModelObject& renderer)
{
    RefPtr<ImageBuffer> tileImage = m_imageMap.get(renderer);
    if (!tileImage) {
        collectPatternAttributesIfNeeded();

        if (!m_attributes)
            return nullptr;

        // Spec: When the geometry of the applicable element has no width or height and objectBoundingBox is specified,
        // then the given effect (e.g. a gradient or a filter) will be ignored.
        FloatRect objectBoundingBox = renderer.objectBoundingBox();
        if (m_attributes->patternUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX && objectBoundingBox.isEmpty())
            return nullptr;

        // Compute all necessary transformations to build the tile image & the pattern.
        FloatRect tileBoundaries;
        AffineTransform tileImageTransform;
        if (!buildTileImageTransform(renderer, *m_attributes, protectedPatternElement(), tileBoundaries, tileImageTransform))
            return nullptr;

        // Ignore 2D rotation, as it doesn't affect the size of the tile.
        auto absoluteTransformIgnoringRotation = context.getCTM(GraphicsContext::DefinitelyIncludeDeviceScale);

        FloatSize tileScale(absoluteTransformIgnoringRotation.xScale(), absoluteTransformIgnoringRotation.yScale());

        // Scale the tile size to match the scale level of the patternTransform.
        tileScale.scale(static_cast<float>(m_attributes->patternTransform().xScale()), static_cast<float>(m_attributes->patternTransform().yScale()));

        // Build tile image.
        tileImage = createTileImage(context, *m_attributes, tileBoundaries.size(), tileScale, tileImageTransform);
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

        m_imageMap.set(renderer, tileImage);
        m_transformMap.set(renderer, transform);
    }

    // Build pattern.
    return Pattern::create({ *tileImage }, { true, true, m_transformMap.get(renderer) } );
}

bool RenderSVGResourcePattern::prepareFillOperation(GraphicsContext& context, const RenderLayerModelObject& targetRenderer, const RenderStyle& style)
{
    auto pattern = buildPattern(context, targetRenderer);
    if (!pattern)
        return false;

    const auto& svgStyle = style.svgStyle();
    context.setAlpha(svgStyle.fillOpacity());
    context.setFillRule(svgStyle.fillRule());
    context.setFillPattern(*pattern);
    return true;
}

bool RenderSVGResourcePattern::prepareStrokeOperation(GraphicsContext& context, const RenderLayerModelObject& targetRenderer, const RenderStyle& style)
{
    auto pattern = buildPattern(context, targetRenderer);
    if (!pattern)
        return false;

    const auto& svgStyle = style.svgStyle();

    context.setAlpha(svgStyle.strokeOpacity());
    SVGRenderSupport::applyStrokeStyleToContext(context, style, targetRenderer);
    if (svgStyle.vectorEffect() == VectorEffect::NonScalingStroke) {
        if (CheckedPtr shape = dynamicDowncast<RenderSVGShape>(targetRenderer))
            pattern->setPatternSpaceTransform(shape->nonScalingStrokeTransform().multiply(m_transformMap.get(targetRenderer)));
    }
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
    CheckedPtr patternRenderer = static_cast<RenderSVGResourcePattern*>(attributes.patternContentElement()->renderer());
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
    auto tileImage = context.createScaledImageBuffer(tileSize, scale, DestinationColorSpace::SRGB());
    if (!tileImage)
        return nullptr;

    auto& tileImageContext = tileImage->context();
    GraphicsContextStateSaver stateSaver(tileImageContext);

    // Draw the content into the ImageBuffer.
    patternRenderer->checkedLayer()->paintSVGResourceLayer(tileImageContext, tileImageTransform);
    return tileImage;
}

void RenderSVGResourcePattern::removeReferencingCSSClient(const RenderElement& client)
{
    if (auto renderer = dynamicDowncast<RenderLayerModelObject>(client)) {
        m_imageMap.remove(renderer);
        m_transformMap.remove(renderer);
    }
}

}
