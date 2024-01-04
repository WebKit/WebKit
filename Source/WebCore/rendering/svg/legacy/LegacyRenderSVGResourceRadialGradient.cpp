/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
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
#include "LegacyRenderSVGResourceRadialGradient.h"

#include "LegacyRenderSVGResourceRadialGradientInlines.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(LegacyRenderSVGResourceRadialGradient);

LegacyRenderSVGResourceRadialGradient::LegacyRenderSVGResourceRadialGradient(SVGRadialGradientElement& element, RenderStyle&& style)
    : LegacyRenderSVGResourceGradient(Type::LegacySVGResourceRadialGradient, element, WTFMove(style))
{
}

LegacyRenderSVGResourceRadialGradient::~LegacyRenderSVGResourceRadialGradient() = default;

bool LegacyRenderSVGResourceRadialGradient::collectGradientAttributes()
{
    m_attributes = RadialGradientAttributes();
    return radialGradientElement().collectGradientAttributes(m_attributes);
}

FloatPoint LegacyRenderSVGResourceRadialGradient::centerPoint(const RadialGradientAttributes& attributes) const
{
    return SVGLengthContext::resolvePoint(&radialGradientElement(), attributes.gradientUnits(), attributes.cx(), attributes.cy());
}

FloatPoint LegacyRenderSVGResourceRadialGradient::focalPoint(const RadialGradientAttributes& attributes) const
{
    return SVGLengthContext::resolvePoint(&radialGradientElement(), attributes.gradientUnits(), attributes.fx(), attributes.fy());
}

float LegacyRenderSVGResourceRadialGradient::radius(const RadialGradientAttributes& attributes) const
{
    return SVGLengthContext::resolveLength(&radialGradientElement(), attributes.gradientUnits(), attributes.r());
}

float LegacyRenderSVGResourceRadialGradient::focalRadius(const RadialGradientAttributes& attributes) const
{
    return SVGLengthContext::resolveLength(&radialGradientElement(), attributes.gradientUnits(), attributes.fr());
}

Ref<Gradient> LegacyRenderSVGResourceRadialGradient::buildGradient(const RenderStyle& style) const
{
    return Gradient::create(
        Gradient::RadialData { focalPoint(m_attributes), centerPoint(m_attributes), focalRadius(m_attributes), radius(m_attributes), 1 },
        { ColorInterpolationMethod::SRGB { }, AlphaPremultiplication::Unpremultiplied },
        platformSpreadMethodFromSVGType(m_attributes.spreadMethod()),
        stopsByApplyingColorFilter(m_attributes.stops(), style),
        RenderingResourceIdentifier::generate()
    );
}

}
