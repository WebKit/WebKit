/*
 * Copyright (C) 2023, 2024 Igalia S.L.
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
#include "RenderSVGResourceGradient.h"

#include "RenderSVGModelObjectInlines.h"
#include "RenderSVGResourceGradientInlines.h"
#include "RenderSVGShape.h"
#include "SVGRenderStyle.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(RenderSVGResourceGradient);

RenderSVGResourceGradient::RenderSVGResourceGradient(Type type, SVGElement& element, RenderStyle&& style)
    : RenderSVGResourcePaintServer(type, element, WTFMove(style))
{
}

RenderSVGResourceGradient::~RenderSVGResourceGradient() = default;

GradientColorStops RenderSVGResourceGradient::stopsByApplyingColorFilter(const GradientColorStops& stops, const RenderStyle& style) const
{
    if (!style.hasAppleColorFilter())
        return stops;

    return stops.mapColors([&] (auto& color) {
        return style.colorByApplyingColorFilter(color);
    });
}

GradientSpreadMethod RenderSVGResourceGradient::platformSpreadMethodFromSVGType(SVGSpreadMethodType method) const
{
    switch (method) {
    case SVGSpreadMethodUnknown:
    case SVGSpreadMethodPad:
        return GradientSpreadMethod::Pad;
    case SVGSpreadMethodReflect:
        return GradientSpreadMethod::Reflect;
    case SVGSpreadMethodRepeat:
        return GradientSpreadMethod::Repeat;
    }

    ASSERT_NOT_REACHED();
    return GradientSpreadMethod::Pad;
}

bool RenderSVGResourceGradient::buildGradientIfNeeded(const RenderLayerModelObject& targetRenderer, const RenderStyle& style, AffineTransform& userspaceTransform)
{
    if (!m_gradient) {
        collectGradientAttributesIfNeeded();
        m_gradient = createGradient(style);

        if (!m_gradient)
            return false;
    }

    auto objectBoundingBox = targetRenderer.objectBoundingBox();
    if (gradientUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        // Gradient is not applicable on 1d objects (empty objectBoundingBox), unless 'gradientUnits' is equal to 'userSpaceOnUse'.
        if (objectBoundingBox.isEmpty())
            return false;

        userspaceTransform.translate(objectBoundingBox.location());
        userspaceTransform.scale(objectBoundingBox.size());
    }

    if (auto gradientTransform = this->gradientTransform(); !gradientTransform.isIdentity())
        userspaceTransform.multiply(gradientTransform);

    return true;
}

bool RenderSVGResourceGradient::prepareFillOperation(GraphicsContext& context, const RenderLayerModelObject& targetRenderer, const RenderStyle& style)
{
    AffineTransform userspaceTransform;
    if (!buildGradientIfNeeded(targetRenderer, style, userspaceTransform))
        return false;

    Ref svgStyle = style.svgStyle();
    context.setAlpha(svgStyle->fillOpacity());
    context.setFillRule(svgStyle->fillRule());
    context.setFillGradient(m_gradient.copyRef().releaseNonNull(), userspaceTransform);
    return true;
}

bool RenderSVGResourceGradient::prepareStrokeOperation(GraphicsContext& context, const RenderLayerModelObject& targetRenderer, const RenderStyle& style)
{
    AffineTransform userspaceTransform;
    if (!buildGradientIfNeeded(targetRenderer, style, userspaceTransform))
        return false;

    Ref svgStyle = style.svgStyle();
    if (svgStyle->vectorEffect() == VectorEffect::NonScalingStroke) {
        if (CheckedPtr shape = dynamicDowncast<RenderSVGShape>(targetRenderer))
            userspaceTransform = shape->nonScalingStrokeTransform() * userspaceTransform;
    }

    context.setAlpha(svgStyle->strokeOpacity());
    SVGRenderSupport::applyStrokeStyleToContext(context, style, targetRenderer);
    context.setStrokeGradient(m_gradient.copyRef().releaseNonNull(), userspaceTransform);
    return true;
}

}
