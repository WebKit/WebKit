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
#include "RenderSVGResourceRadialGradient.h"

#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderSVGResourceRadialGradient);

RenderSVGResourceRadialGradient::RenderSVGResourceRadialGradient(SVGRadialGradientElement& element, RenderStyle&& style)
    : RenderSVGResourceGradient(element, WTFMove(style))
{
}

RenderSVGResourceRadialGradient::~RenderSVGResourceRadialGradient() = default;

bool RenderSVGResourceRadialGradient::collectGradientAttributes()
{
    m_attributes = RadialGradientAttributes();
    return radialGradientElement().collectGradientAttributes(m_attributes);
}

FloatPoint RenderSVGResourceRadialGradient::centerPoint(const RadialGradientAttributes& attributes) const
{
    return SVGLengthContext::resolvePoint(&radialGradientElement(), attributes.gradientUnits(), attributes.cx(), attributes.cy());
}

FloatPoint RenderSVGResourceRadialGradient::focalPoint(const RadialGradientAttributes& attributes) const
{
    return SVGLengthContext::resolvePoint(&radialGradientElement(), attributes.gradientUnits(), attributes.fx(), attributes.fy());
}

float RenderSVGResourceRadialGradient::radius(const RadialGradientAttributes& attributes) const
{
    return SVGLengthContext::resolveLength(&radialGradientElement(), attributes.gradientUnits(), attributes.r());
}

float RenderSVGResourceRadialGradient::focalRadius(const RadialGradientAttributes& attributes) const
{
    return SVGLengthContext::resolveLength(&radialGradientElement(), attributes.gradientUnits(), attributes.fr());
}

Ref<Gradient> RenderSVGResourceRadialGradient::buildGradient(const RenderStyle& style) const
{
    auto gradient = Gradient::create(Gradient::RadialData { focalPoint(m_attributes), centerPoint(m_attributes), focalRadius(m_attributes), radius(m_attributes), 1 });
    gradient->setSpreadMethod(platformSpreadMethodFromSVGType(m_attributes.spreadMethod()));
    addStops(gradient, m_attributes.stops(), style);
    return gradient;
}

}
