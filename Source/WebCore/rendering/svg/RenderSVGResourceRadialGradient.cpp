/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2021, 2022, 2023 Igalia S.L.
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
#include "RenderSVGResourceRadialGradient.h"

#if ENABLE(LAYER_BASED_SVG_ENGINE)
#include "RenderSVGModelObjectInlines.h"
#include "RenderSVGResourceRadialGradientInlines.h"
#include "RenderSVGShape.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderSVGResourceRadialGradient);

RenderSVGResourceRadialGradient::RenderSVGResourceRadialGradient(SVGRadialGradientElement& element, RenderStyle&& style)
    : RenderSVGResourceGradient(Type::SVGResourceRadialGradient, element, WTFMove(style))
{
}

RenderSVGResourceRadialGradient::~RenderSVGResourceRadialGradient() = default;

void RenderSVGResourceRadialGradient::collectGradientAttributesIfNeeded()
{
    if (m_attributes.has_value())
        return;

    radialGradientElement().synchronizeAllAttributes();

    auto attributes = RadialGradientAttributes { };
    if (radialGradientElement().collectGradientAttributes(attributes))
        m_attributes = WTFMove(attributes);
}

RefPtr<Gradient> RenderSVGResourceRadialGradient::createGradient(const RenderStyle& style)
{
    if (!m_attributes)
        return nullptr;

    auto centerPoint = SVGLengthContext::resolvePoint(&radialGradientElement(), m_attributes->gradientUnits(), m_attributes->cx(), m_attributes->cy());
    auto radius = SVGLengthContext::resolveLength(&radialGradientElement(), m_attributes->gradientUnits(), m_attributes->r());

    auto focalPoint = SVGLengthContext::resolvePoint(&radialGradientElement(), m_attributes->gradientUnits(), m_attributes->fx(), m_attributes->fy());
    auto focalRadius = SVGLengthContext::resolveLength(&radialGradientElement(), m_attributes->gradientUnits(), m_attributes->fr());

    return Gradient::create(
        Gradient::RadialData { focalPoint, centerPoint, focalRadius, radius, 1 },
        { ColorInterpolationMethod::SRGB { }, AlphaPremultiplication::Unpremultiplied },
        platformSpreadMethodFromSVGType(m_attributes->spreadMethod()),
        stopsByApplyingColorFilter(m_attributes->stops(), style),
        RenderingResourceIdentifier::generate()
    );
}

}

#endif // ENABLE(LAYER_BASED_SVG_ENGINE)
