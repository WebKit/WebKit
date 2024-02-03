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

#if ENABLE(LAYER_BASED_SVG_ENGINE)
#include "ElementChildIteratorInlines.h"
#include "RenderLayer.h"
#include "RenderSVGModelObjectInlines.h"
#include "RenderSVGResourcePatternInlines.h"
#include "RenderSVGShape.h"
#include "SVGElementTypeHelpers.h"
#include "SVGFitToViewBox.h"
#include "SVGRenderStyle.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderSVGResourcePattern);

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

    SVGPatternElement* current = &patternElement();

    patternElement().synchronizeAllAttributes();

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

static void clear2DRotation(AffineTransform& transform)
{
    AffineTransform::DecomposedType decomposition;
    transform.decompose(decomposition);
    decomposition.angle = 0;
    transform.recompose(decomposition);
}

RefPtr<Pattern> RenderSVGResourcePattern::buildPattern(GraphicsContext& context, const RenderLayerModelObject& renderer)
{
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
    if (!buildTileImageTransform(renderer, *m_attributes, patternElement(), tileBoundaries, tileImageTransform))
        return nullptr;

    // Ignore 2D rotation, as it doesn't affect the size of the tile.
    auto absoluteTransformIgnoringRotation = context.getCTM(GraphicsContext::DefinitelyIncludeDeviceScale);
    clear2DRotation(absoluteTransformIgnoringRotation);
    FloatRect absoluteTileBoundaries = absoluteTransformIgnoringRotation.mapRect(tileBoundaries);

    // Scale the tile size to match the scale level of the patternTransform.
    absoluteTileBoundaries.scale(static_cast<float>(m_attributes->patternTransform().xScale()), static_cast<float>(m_attributes->patternTransform().yScale()));

    // Build tile image.
    auto tileImage = createTileImage(*m_attributes, tileBoundaries, absoluteTileBoundaries, tileImageTransform);
    if (!tileImage)
        return nullptr;

    auto tileImageSize = tileImage->logicalSize();

    auto copiedImage = ImageBuffer::sinkIntoNativeImage(WTFMove(tileImage));
    if (!copiedImage)
        return nullptr;

    // Compute pattern space transformation.
    AffineTransform patternSpaceTransform;
    patternSpaceTransform.translate(tileBoundaries.location());
    patternSpaceTransform.scale(tileBoundaries.size() / tileImageSize);

    auto patternTransform = m_attributes->patternTransform();
    if (!patternTransform.isIdentity())
        patternSpaceTransform = patternTransform * patternSpaceTransform;

    // Build pattern.
    return Pattern::create({ copiedImage.releaseNonNull() }, { true, true, patternSpaceTransform });
}

bool RenderSVGResourcePattern::prepareFillOperation(GraphicsContext& context, const RenderLayerModelObject& targetRenderer, const RenderStyle& style)
{
    auto pattern = buildPattern(context, targetRenderer);
    if (!pattern)
        return false;

    const auto& svgStyle = style.svgStyle();
    context.setAlpha(svgStyle.fillOpacity());
    context.setFillRule(svgStyle.fillRule());
    context.setFillPattern(pattern.copyRef().releaseNonNull());
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
        if (auto* shape = dynamicDowncast<RenderSVGShape>(targetRenderer))
            pattern->setPatternSpaceTransform(shape->nonScalingStrokeTransform().multiply(pattern->patternSpaceTransform()));
    }
    context.setStrokePattern(pattern.releaseNonNull());
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

RefPtr<ImageBuffer> RenderSVGResourcePattern::createTileImage(const PatternAttributes& attributes, const FloatRect& tileBoundaries, const FloatRect& absoluteTileBoundaries, const AffineTransform& tileImageTransform) const
{
    auto* patternRenderer = static_cast<RenderSVGResourcePattern*>(attributes.patternContentElement()->renderer());
    ASSERT(patternRenderer);
    ASSERT(patternRenderer->hasLayer());

    if (SVGHitTestCycleDetectionScope::isVisiting(*patternRenderer))
        return nullptr;

    // FIXME: consider color space handling/'color-interpolation'.
    auto clampedAbsoluteTileBoundaries = ImageBuffer::clampedRect(absoluteTileBoundaries);
    auto tileImage = ImageBuffer::create(roundedIntSize(clampedAbsoluteTileBoundaries.size()), RenderingPurpose::Unspecified, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8);
    if (!tileImage)
        return nullptr;

    auto& tileImageContext = tileImage->context();

    GraphicsContextStateSaver stateSaver(tileImageContext);

    FloatSize unclampedSize = roundedIntSize(tileBoundaries.size());

    // Compensate rounding effects, as the absolute target rect is using floating-point numbers and the image buffer size is integer.
    tileImageContext.scale(unclampedSize / tileBoundaries.size());

    // The image buffer represents the final rendered size, so the content has to be scaled (to avoid pixelation).
    tileImageContext.scale(clampedAbsoluteTileBoundaries.size() / tileBoundaries.size());

    // Draw the content into the ImageBuffer.
    patternRenderer->layer()->paintSVGResourceLayer(tileImageContext, tileImageTransform);
    return tileImage;
}

}

#endif // ENABLE(LAYER_BASED_SVG_ENGINE)
